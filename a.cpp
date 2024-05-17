#include<iostream>
#include<string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include<unistd.h>
#include<thread>
#include<poll.h>
#include<sys/epoll.h>
using namespace std;

// void *client_thread(void *arg){
//     int clientfd=*(int*)arg;
//     while(1){
//         char buffer[128]={0};
//         int count=recv(clientfd,buffer,128,0);
//         if(count<=0){
//             break;
//         }
//         send(clientfd,buffer,count,0);
//         cout<<clientfd<<","<<count<<","<<buffer<<endl;   //打印的结果始终是3和4，因为stdin,stdout,stderr占据0,1,2
//     }
//     close(clientfd);   //保证tcp正确断开连接，防止出现time_wait现象
//     delete (int*)arg;
//     return NULL;
// }

//tcp
int main(){
    int sockfd=socket(AF_INET,SOCK_STREAM,0);    //套接字
    struct sockaddr_in serveraddr;         //存储地址的结构体
    memset(&serveraddr,0,sizeof(struct sockaddr_in));
    serveraddr.sin_family=AF_INET;
    serveraddr.sin_addr.s_addr=htonl(INADDR_ANY);
    serveraddr.sin_port=htons(2048);

    if(-1==bind(sockfd,(struct sockaddr*)&serveraddr,sizeof(struct sockaddr))){
        perror("bind");
        return -1;
    }
    listen(sockfd,10);   //10代表排队等待连接的最大请求数量

#if 0
//普通方式实现tcp连接，每有一个新的连接，开一个新的线程
    while(1){   
        struct sockaddr clientaddr;
        socklen_t len=sizeof(clientaddr);

        int clientfd=accept(sockfd,&clientaddr,&len);   //通信信道,第二个参数定义的时候使用sockaddr而不是sockaddr_in会有警告
        // pthread_t thid;
        // pthread_create(&thid,NULL,client_thread,&clientfd);
        // pthread_detach(thid); // detach the thread to avoid resource leak
        thread th(client_thread, new int(clientfd));
        th.detach(); // detach the thread to avoid resource leak
    }
#endif

#if 0
    //poll
    struct pollfd fds[1024]={0};
    fds[sockfd].fd=sockfd;   //可以这样理解，现在这个sockfd监听2048端口，并加入poll中管理
    fds[sockfd].events=POLLIN;
    int maxfd=sockfd;

    while(1){
        int nready=poll(fds,maxfd+1,-1);
        if(fds[sockfd].revents & POLLIN){    //如果有连接发生
            struct sockaddr clientaddr;
            socklen_t len=sizeof(clientaddr);
            int clientfd=accept(sockfd,(struct sockaddr*)&clientaddr,&len);
            cout<<"sockfd: "<<clientfd<<endl;
            fds[clientfd].fd=clientfd;
            fds[clientfd].events=POLLIN;
            maxfd=max(maxfd,clientfd);   //poll内部遍历的最大数       
        }

        for(int i=sockfd+1;i<=maxfd;i++){   //处理客户端的请求
            if(fds[i].revents & POLLIN){
                char buffer[128]={0};
                int count=recv(i,buffer,128,0);
                if(count<=0){  //close
                    cout<<"disconnect:"<<i<<endl;
                    close(i);
                    fds[i].fd=-1;
                    fds[i].events=0;
                }else{
                    send(i,buffer,count,0);
                    cout<<"clientfd: "<<i<<",count: "<<count<<",buffer: "<<buffer<<endl;
                }
                fds[i].revents = 0;
            }
        }
    }
#endif
    //epoll
    int epfd=epoll_create(1);
    struct epoll_event ev;
    ev.events=EPOLLIN;
    ev.data.fd=sockfd;

    epoll_ctl(epfd,EPOLL_CTL_ADD,sockfd,&ev);
    struct epoll_event events[1024]={0};

    while(1){
        int nready=epoll_wait(epfd,events,1024,-1);
        for(int i=0;i<nready;i++){
            int connfd=events[i].data.fd;
            if(sockfd==connfd){
                struct sockaddr clientaddr;
                socklen_t len=sizeof(clientaddr);
                int clientfd=accept(sockfd,(struct sockaddr*)&clientaddr,&len);

                ev.events=EPOLLIN;
                ev.data.fd=clientfd;
                epoll_ctl(epfd,EPOLL_CTL_ADD,clientfd,&ev);
                cout<<"clientfd: "<<clientfd<<endl;

            }else if(events[i].events & EPOLLIN){
                char buffer[128]={0};
                int count=recv(connfd,buffer,128,0);
                if(count<=0){  //close
                    cout<<"disconnect:"<<connfd<<endl;
                    epoll_ctl(epfd,EPOLL_CTL_DEL,connfd,NULL);               
                    close(i);
                }else{
                    send(connfd,buffer,count,0);
                    cout<<"clientfd: "<<connfd<<",count: "<<count<<",buffer: "<<buffer<<endl;
                }
            }
        }
    }


    //close(clientfd);   //保证tcp正确断开连接，防止出现time_wait现象
    close(sockfd);
    return 0;
} 
//poll使用的是时间轮询的方式遍历，而epoll使用的是事件通知的方式