#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <iostream>
#include <cstring>

const char SOCKET_PATH[] = "/tmp/shared-sockets/echo_socket";

int main() {
    // Create UNIX domain socket
    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    // Setup server address struct
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    // Connect to server socket
    if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("connect");
        close(sockfd);
        return 1;
    }

    // Send command
    const char* cmd = "GET /data";
    ssize_t sent = send(sockfd, cmd, strlen(cmd), 0);
    if (sent == -1) {
        perror("send");
        close(sockfd);
        return 1;
    }

    // Receive response
    char buffer[1024];
    ssize_t received = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
    if (received == -1) {
        perror("recv");
        close(sockfd);
        return 1;
    }

    buffer[received] = '\0'; // Null terminate string

    std::cout << "Response from server: " << buffer << std::endl;

    close(sockfd);
    return 0;
}