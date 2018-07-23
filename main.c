// ==============================================================================================
// Title: Winsock 5 in 1 (PortScanner, TCP Server, TCP Client, UPD Server and UDP Client)
// Goal: To understand the Winsock library and Threads in C/C++
// Author  Paulo Ramos  Date : July 2018
// Compiled using CodeBlocks (GCC)
// Project based on Codes availabe at: https://www.binarytides.com
// ================================ IMPORTANT ==================================================
// Note: it is necessary to add the c:\windows\system32\ws2_32.dll library to the project.
// Go to menu: Project -> Build Options -> Debug -> Linker Options -> Add -> ..\ws2_32.dll (find it on windows)
// ==============================================================================================

#include<io.h>
#include<stdio.h>
#include<winsock2.h>
#include<pthread.h>
#pragma comment(lib,"c:\windows\system32\ws2_32.dll") //Winsock Library - Add it to the project
//===============================================================================================
//=================== Prototyping, Global Variables and Constants ===============================
//===============================================================================================
/* Prototyping */
void start_winsock();
struct sockaddr_in set_sock_addr(struct sockaddr_in sock, int family, int addr, int port, int opt);
void port_scanner();
void tcp_server();
void tcp_client();
int udp_server();
void udp_client(void);
void send_msg_sock_addr(SOCKET sk, struct sockaddr_in saddr_x, char * message);
void * send_data(void *value);
void start_send_thread();
void check_commands(char * text);
void help_summary();
void help_full();
void settings();
int main(int argc, char **argv);


#define BUFLEN 512  //Max length of buffer
char SERVER[100] =  "127.0.0.1";  //ip address of udp server
int PORT = 8888;   //The port on which to listen for incoming data

SOCKET s, new_socket;
WSADATA wsa;
struct sockaddr_in server, client, si_other, sa;
int slen , recv_len;
char buf[BUFLEN], message[BUFLEN];
slen = sizeof(struct sockaddr_in) ;
pthread_t recv_thread, send_thread;
int thread_cancel_flag = 0, ck_box = 1, app_option = -1;

//===============================================================================================
//========================== Common Routines ====================================================
//===============================================================================================
void start_winsock() {
    //Initialise winsock
    printf("\nInitializing Winsock...");
    if (WSAStartup(MAKEWORD(2,2),&wsa) != 0) {
        printf("Failed. Error Code : %d",WSAGetLastError());
        exit(EXIT_FAILURE);
    }
    printf("Initialized.\n");
}
SOCKET create_socket(int domain, int type, int protocol) {
    //Create a socket
    SOCKET ls;
    if((ls = socket(domain, type, protocol)) == INVALID_SOCKET) {
        printf("Could not create socket : %d" , WSAGetLastError());
        exit(EXIT_FAILURE);
    }
    return ls;
}
void check_bind_socket(int bd) {
    check_socket_error(bd, "Bind failed with error code :");
    puts("Bind done\n");
}
void check_socket_error(int sk, char* err_msg) {
    if (sk == SOCKET_ERROR) {
        printf("%s %d" , err_msg, WSAGetLastError());
        exit(EXIT_FAILURE);
    }
}
struct sockaddr_in set_sock_addr(struct sockaddr_in sock, int family, int addr, int port, int opt) {
    //Prepare the sockaddr_in structure
    sock.sin_family = family;
    if (opt==0) {sock.sin_addr.s_addr = addr;} else {sock.sin_addr.S_un.S_addr = addr;}
    sock.sin_port = htons(port);

    return sock;
}
//===============================================================================================
// Simple Port Scanner Adapted from BinaryTides
// https://www.binarytides.com/udp-socket-programming-in-winsock/
// Date: 06 July 2018
//===============================================================================================

void port_scanner(){
    printf("*** TCP Port Scanner *** \n");
    struct hostent *host;
    int err,i, startport , endport;
    char hostname[100];

    strncpy((char *)&sa,"",sizeof sa);
    sa.sin_family = AF_INET; //this line must be like this coz internet
    start_winsock();//Initialise winsock

    printf("Enter hostname or ip to scan : ");
    scanf(" %s", hostname);
    printf("Enter starting port : ");
    scanf(" %d" , &startport);
    printf("Enter ending port : ");
    scanf(" %d" , &endport);

    if(isdigit(hostname[0])) { // translating the hostname to ip when necessary
        sa.sin_addr.s_addr = inet_addr(hostname); //get ip into s_addr
    } else if((host=gethostbyname(hostname)) != 0){
        strncpy((char *)&sa.sin_addr , (char *)host->h_addr_list[0] , sizeof sa.sin_addr);
    } else {
        printf("Error resolving hostname");
        exit(EXIT_FAILURE);
    }
    //Start the portscan loop
    printf("Starting the scan loop...\n");
    for(i = startport ; i<= endport ; i++) {
        s = create_socket(AF_INET , SOCK_STREAM , 0);
        printf("%d\r", i);
        sa.sin_port = htons(i);
        //connect to the server with that socket
        err = connect(s , (struct sockaddr *)&sa , sizeof sa);
        if(err != SOCKET_ERROR) {//connection not accepted
        printf("%s %-5d accepted            \n" , hostname , i);
            if( shutdown( s ,SD_BOTH ) == SOCKET_ERROR )  {
                perror("\nshutdown");// perror function prints an error message to stderr
                exit(EXIT_FAILURE);
            }
        }
        closesocket(s);   //closes the net socket
    }
    fflush(stdout); //clears the contents of a buffer or flushes a stream
    printf("\n\nPress Enter to Continue...");
    char c = getc(stdin);
    c = getc(stdin);

    return(0);
    }
//===============================================================================================
// Simple TCP Server Adapted from BinaryTides
// https://www.binarytides.com/code-tcp-socket-server-winsock/
// Date: 06 July 2018
//===============================================================================================
void tcp_server() {
    printf("*** TCP Server listening on port %d *** \n", PORT);
    start_winsock();// Initialize socket
    s = create_socket(AF_INET , SOCK_STREAM , 0);//create socket
    server = set_sock_addr(server, AF_INET, INADDR_ANY, PORT, 0);//set sock_addr_in
    check_bind_socket(bind(s ,(struct sockaddr *)&server , sizeof(server)));//Bind
    listen(s , 3);//Listen to incoming connections

    //Accept and incoming connection
    puts("Waiting for incoming connections...");
    check_socket_error((new_socket = accept(s , (struct sockaddr *)&client, &slen)), "accept failed with error code :");
    puts("Connection accepted");

    //Reply to client
    strcpy(message, "*** Welcome To the TCP Chat/Echo Server ***");
    send(new_socket , message , strlen(message) , 0);
    start_send_thread(); //Send Data in Other Thread

    while(1) {
        memset(buf,'\0', BUFLEN);
        check_socket_error((recv_len = recvfrom(new_socket, buf, BUFLEN, 0, (struct sockaddr *) &client, &slen)), "recvfrom() failed with error code :");
        if (strlen(buf)>0) {
            if (ck_box==1) check_socket_error(sendto(new_socket, buf, recv_len, 0, (struct sockaddr*) &client, slen), "sendto() failed with error code :");
            printf("\r[client %s:%d]: %s\nEnter a message: ", inet_ntoa(client.sin_addr), ntohs(client.sin_port), buf);
            check_commands(buf);
        }
    }
    closesocket(s);
    WSACleanup();

    return 0;
}
//===============================================================================================
// Simple TCP Server Adapted from BinaryTides
// https://www.binarytides.com/code-tcp-socket-server-winsock/
// Date: 06 July 2018
//===============================================================================================
void tcp_client()
{
    printf("*** TCP Client *** \n");
    start_winsock();// Initialize socket
    s = create_socket(AF_INET , SOCK_STREAM , 0);//create socket
    server = set_sock_addr(server, AF_INET, inet_addr(SERVER), PORT, 0);// set sokaddr_in

    //Connect to remote server
    if (connect(s , (struct sockaddr *)&server , sizeof(server)) < 0) {
        puts("connect error\n");
        return 1;
    }
    printf("Connected to TCP Server %s : %d\n", SERVER, PORT);
    //Send some data
    start_send_thread(); //Send Data in Other Thread
    while(1) {
        if (s==0) break;
        memset(buf,'\0', BUFLEN);
        check_socket_error((recv_len = recv(s , buf , BUFLEN , 0)), "Receive failed");
        if (strlen(buf)>0) {
                printf("\r[TCP Server]: %s\nEnter a message: ", buf);
                check_commands(buf);
        }
    }
    return 0;
}
//===============================================================================================
// Simple UPD Server Adapted from BinaryTides
// https://www.binarytides.com/udp-socket-programming-in-winsock/
// Date: 06 July 2018
//===============================================================================================
int udp_server()
{
    printf("*** UDP Server listening on port %d *** \n", PORT);
    start_winsock();//Initialise winsock
    s = create_socket(AF_INET , SOCK_DGRAM , 0);//Create a socket
    server = set_sock_addr(server, AF_INET, INADDR_ANY, PORT, 0);//set sock_addr_in
    check_bind_socket(bind(s ,(struct sockaddr *)&server , sizeof(server)));//Bind
    //keep listening for data
    printf("Waiting for data...\n");

    strcpy(message, "*** Welcome To the UDP Chat/Echo Server ***");
    // check_socket_error(sendto(s, message, strlen(message) , 0 , (struct sockaddr *) &si_other, slen),"sendto() failed :");
    start_send_thread(); //Send Data in Other Thread

    while(1)
    {
        fflush(stdout); //clear the buffer by filling null, it might have previously received data
        memset(buf,'\0', BUFLEN);  //try to receive some data, this is a blocking call
        check_socket_error((recv_len = recvfrom(s, buf, BUFLEN, 0, (struct sockaddr *) &si_other, &slen)), "recvfrom() failed with error code :");
        //print details of the client/peer and the data received
        if (strlen(buf)>0) {
            printf("\r[client: %s:%d]: %s\nEnter a message: ", inet_ntoa(si_other.sin_addr), ntohs(si_other.sin_port), buf);
            if (ck_box==1) check_socket_error(sendto(s, buf, recv_len, 0, (struct sockaddr*) &si_other, slen), "sendto() failed with error code :");
            check_commands(buf);
        }
    }
    closesocket(s);
    WSACleanup();
    return 0;
}

//===============================================================================================
// Simple UDP Client Adapted from BinaryTides
// https://www.binarytides.com/udp-socket-programming-in-winsock/
// Date: 06 July 2018
//===============================================================================================
void udp_client(void)
{
    printf("*** UDP Client *** \n");
    fd_set readfds;
    start_winsock();//Initialise winsock
    s = create_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);//Create a socket

    //setup address structure
    memset((char *) &si_other, 0, sizeof(si_other));
    si_other = set_sock_addr(si_other, AF_INET, inet_addr(SERVER), PORT, 1);

    printf("Connected to UDP Server %s : %d\n", SERVER, PORT);
    start_send_thread(); //Send Data in Other Thread
    FD_ZERO(&readfds);
    FD_SET(s, &readfds);
    while(1)
    {
        if (select( 0 , &readfds , NULL , NULL , NULL) == SOCKET_ERROR ){
            printf("select call failed with error code : %d" , WSAGetLastError());
            exit(EXIT_FAILURE);
        }
        if (FD_ISSET(s , &readfds)){
            memset(buf,'\0', BUFLEN);
            check_socket_error((recv_len = recv(s , buf , BUFLEN , 0)), "Receive failed");
            //check_socket_error(recvfrom(s, buf, BUFLEN, 0, (struct sockaddr *) &si_other, &slen), "recvfrom() failed with error code :");
            if (strlen(buf) >0) {
                printf("\r[UDP Server]: %s\nEnter a message: ", buf);
                check_commands(buf);
            }
        }

    }
    closesocket(s);
    WSACleanup();
    return 0;
}

//===============================================================================================
// Send Data using a secondary Thread. The receive data is handled by the primary thread.
// Date: 06 July 2018
//===============================================================================================
void * send_data(void *value){
    fflush(stdin);
    while(1) {
        if (s==0) break;
        printf("\rEnter message : ");
        gets(message);
        if (strlen(message)>0) {
            //TCP Server
            if (app_option==2) check_socket_error(sendto(new_socket, message , strlen(message), 0, (struct sockaddr*) &client, slen), "sendto() failed :");
            //TCP Client
            if (app_option==3) check_socket_error(send(s , message , strlen(message) , 0), "Send failed");
            //UDP Server
            if (app_option==4) check_socket_error(sendto(s, message, strlen(message) , 0 , (struct sockaddr *) &si_other, slen),"sendto() failed :");
            //UDP Client
            if (app_option==5) check_socket_error(sendto(s, message, strlen(message) , 0 , (struct sockaddr *) &si_other, slen),"sendto() failed :");

            check_commands(message);
            if (strcmp(message, "/help")==0)  help_summary();
            if (strcmp(message, "/cls")==0)  system("cls");

        }
    }
}
void start_send_thread() {
    thread_cancel_flag = 0;
    pthread_create(&send_thread, NULL, send_data, NULL);

}
void check_commands(char * text) {
    if (strcmp(text, "/echo_off")==0) {
        ck_box = 0;
        printf("\n*** Echo OFF ***\n\n");
    }
    if (strcmp(text, "/echo_on")==0) {
        ck_box = 1;
        printf("\n*** Echo ON ***\n\n");
    }
    if (strcmp(text, "/quit")==0) {
        closesocket(s);
        closesocket(new_socket);
        WSACleanup();
        exit(0);
        printf("\n*** Quiting Applications ***\n");
    }
}


//===============================================================================================
// Interface to access the code adapted from BinaryTides
// Date: 06 July 2018
//===============================================================================================
void help_summary() {
    printf("===================================================================\n");
    printf("* Winsock 5 in 1 - PortScanner, TCP/UDP Servers, TCP/UDP Clients  *\n");
    printf("===================================================================\n");
    printf("* Code Based on www.BinaryTides.com code examples                 *\n");
    printf("*                                                                 *\n");
    printf("* *** Other Commands ***                                          *\n");
    printf("* /quit - Quit the server and client                              *\n");
    printf("* /echo_off - Disables the echo on the servers                    *\n");
    printf("* /echo_on  - Enables the echo on the servers                     *\n");
    printf("* /cls  - Clear the screen                                        *\n");
    printf("* /help - Lists this help                                         *\n");
    printf("*                                                                 *\n");
    printf("* Made by Paulo Ramos @ July 2018 - C/C++ Winsock Projects        *\n");
    printf("==================================================================\n");
}


void help_full() {
    char c;
    system("cls");
    printf("===================================================================\n");
    printf("* Winsock 5 in 1 - PortScanner, TCP/UDP Servers, TCP/UDP Clients  *\n");
    printf("===================================================================\n");
    printf("* Code Based on www.BinaryTides.com code examples                 *\n");
    printf("*                                                                 *\n");
    printf("* How to use the Servers and Clients:                             *\n");
    printf("* 1. Build and compile the source code using CodeBlocks / GCC     *\n");
    printf("* *** Remember to add the 'ws2_32.dll' file to the project ***    *\n");
    printf("* 2. Run the Server in one Window (option 2 or 4)                 *\n");
    printf("* 3. In a different window run the Client (option 3 or 5)         *\n");
    printf("* 4. Test it by typing something in the client and in the server  *\n");
    printf("*     To disable the echo message type '/echo_off'                *\n");
    printf("*  The clients and servers works as a p2p chat                    *\n");
    printf("*                                                                 *\n");
    printf("* *** Other Commands ***                                          *\n");
    printf("* /quit - Quit the server and client                              *\n");
    printf("* /echo_off - Disables the echo on the servers                    *\n");
    printf("* /echo_on  - Enables the echo on the servers                     *\n");
    printf("* /help - Lists this help                                         *\n");
    printf("*                                                                 *\n");
    printf("* Made by Paulo Ramos @ July 2018 - C/C++ Winsock Projects        *\n");
    printf("==================================================================\n");
    printf("\n\nPress Enter to Continue...");
    c = getc(stdin);
    c = getc(stdin);
    //scanf("%s", c);
    system("cls");
}

void settings() {
    int opt = -1;

    printf("\n**** Change Settings ****");
    while (opt!=0) {
        printf("\nDefault SERVER: %s Default PORT: %d\n", SERVER, PORT);
        printf("\nOptions:\n 0. Do not Change and Exit\n 1. Change SERVER\n 2. Change PORT \n 3. Change BOTH (SERVER and PORT)\n");

        printf("\nEnter Option:");
        scanf(" %d", & opt);
        if (opt==0) break;
        if (opt==1 || opt==3) {
            printf("Enter a New IP/Host: ");
            scanf(" %s", SERVER);
        }
        if (opt==2 || opt==3) {
            printf("Enter a New PORT: ");
            scanf(" %d", &PORT);
        }

    }
}
int main(int argc, char **argv) {
    while (app_option!=0) {
        system("cls");
        printf("Winsock Projects Adapted from BinaryTides (www.binarytides.com)\n\n");
        printf("Options:\n");
        printf("1 - TCP Port Scanner\n");
        printf("2 - TCP Server\n");
        printf("3 - TCP Client\n");
        printf("4 - UDP Server\n");
        printf("5 - UDP Client\n\n");
        printf("6 - Change SERVER [%s] / PORT[%d]\n\n", SERVER, PORT);

        printf("7 - Help\n");
        printf("\n0- To Exit\n\n");

        printf("\nEnter an option: ");
        scanf(" %d", &app_option);
        system("cls");
        if (app_option==1) port_scanner();
        if (app_option==2) tcp_server();
        if (app_option==3) tcp_client();
        if (app_option==4) udp_server();
        if (app_option==5) udp_client();
        if (app_option==6) settings();
        if (app_option==7) help_full();
    }
}
//===============================================================================================

