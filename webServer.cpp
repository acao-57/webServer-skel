// **************************************************************************************
// * webServer (webServer.cpp)
// * - Implements a very limited subset of HTTP/1.0, use -v to enable verbose debugging output.
// * - Port number 1701 is the default, if in use random number is selected.
// *
// * - GET requests are processed, all other metods result in 400.
// *     All header gracefully ignored
// *     Files will only be served from cwd and must have format file\d.html or image\d.jpg
// *
// * - Response to a valid get for a legal filename
// *     status line (i.e., response method)
// *     Cotent-Length:
// *     Content-Type:
// *     \r\n
// *     requested file.
// *
// * - Response to a GET that contains a filename that does not exist or is not allowed
// *     statu line w/code 404 (not found)
// *
// * - CSCI 471 - All other requests return 400
// * - CSCI 598 - HEAD and POST must also be processed.
// *
// * - Program is terminated with SIGINT (ctrl-C)
// **************************************************************************************
#include "webServer.h"


// **************************************************************************************
// * Signal Handler.
// * - Display the signal and exit (returning 0 to OS indicating normal shutdown)
// * - Optional for 471, required for 598
// **************************************************************************************
// void sig_handler(int signo) {}


// **************************************************************************************
// * processRequest,
//   - Return HTTP code to be sent back
//   - Set filename if appropriate. Filename syntax is valided but existance is not verified.
// **************************************************************************************
int readHeader(int sockFd,std::string &filename) {
  int bytesRead;
  char buffer[BUFFER_SIZE];
  std::string request;

  //reading until blank line
  while(request.find("\r\n\r\n") == std::string::npos) {
    bzero(buffer, BUFFER_SIZE);

    // if we have an error or nothing sent/client closed connection, then return 400 (no request to parse)
    if ((bytesRead = read(sockFd, buffer, BUFFER_SIZE)) < 1) {
      if (bytesRead < 0) {
        std::cout << "connection closed unexpectedly" << std::endl;
      } else {
        std::cout << "client closed connection" << std::endl;
      }
      return 400;
    }
    //accumulate 
    request.append(buffer, bytesRead);
  }

  // getting the request line
  std::string requestLine = request.substr(0, request.find("\r\n"));
  std::cout << requestLine << std::endl;

  // use a ' ' delimiter to separate out the sections of the request
  char delim = ' ';
  int firstIdx = requestLine.find(delim);
  int secondIdx = requestLine.find(delim, firstIdx + 1);

  std::string method = requestLine.substr(0, firstIdx);
  std::string path = requestLine.substr(firstIdx + 1, secondIdx - firstIdx - 1);
  std::string version = requestLine.substr(secondIdx + 1);

  // method is not GET, bad request
  if (method != "GET") {
    return 400;
  }

  // method = GET
  // html and jpeg pattern check
  std::string cleanPath = path.substr(1);
  std::regex htmlPattern("file[0-9]\\.html");
  std::regex jpgPattern("image[0-9]\\.jpg");
  // if not a legit html or jpg pattern, then return 404
  if (!std::regex_match(cleanPath, htmlPattern)) {
    if (!std::regex_match(cleanPath, jpgPattern)) {
      return 404;
    }
  }

  std::string fullPath = "data/" + cleanPath;
  // check for if file actually exists
  std::ifstream file(fullPath);
  if (!file.is_open()) {
    return 404;
  }
  file.close();

  filename = fullPath;
  return 200;

  // return 0;
}


// **************************************************************************
// * Send one line (including the line terminator <LF><CR>)
// * - Assumes the terminator is not included, so it is appended.
// **************************************************************************
void sendLine(int socketFd, std::string &stringToSend) {
  // CD = carriage return, moves cursor back to start of current line
  // LF = line feed, moves cursor down to the net line
  std::string line = stringToSend + "\r\n";
  write(socketFd, line.c_str(),line.length());
  return;
}

// **************************************************************************
// * Send the entire 404 response, header and body.
// **************************************************************************
void send404(int sockFd) {
  std::string status = "HTTP/1.0 404 Not Found";
  std::string contentType = "Content-Type: text/html";
  std::string body = "<html><body><h1>404 Not Found</h1></body></html>";
  std::string contentLength = "Content-Length: " + std::to_string(body.length());
  std::string newLine = "";

  sendLine(sockFd, status);
  sendLine(sockFd, contentLength);
  sendLine(sockFd, contentType);
  sendLine(sockFd, newLine);
  write(sockFd, body.c_str(), body.length());
  return;
}

// **************************************************************************
// * Send the entire 400 response, header and body.
// **************************************************************************
void send400(int sockFd) {
  std::string status = "HTTP/1.0 400 Bad Request";
  std::string contentType = "Content-Type: text/html";
  std::string body = "<html><body><h1>400 Bad Request</h1></body></html>";
  std::string contentLength = "Content-Length: " + std::to_string(body.length());
  std::string newLine = "";

  sendLine(sockFd, status);
  sendLine(sockFd, contentLength);
  sendLine(sockFd, contentType);
  sendLine(sockFd, newLine);
  write(sockFd, body.c_str(), body.length());
  return;
}


// **************************************************************************************
// * sendFile
// * -- Send a file back to the browser.
// **************************************************************************************
void sendFile(int sockFd,std::string filename) {
  // assuming we have a legit file callled w this function
  // go to the end of the file to get file size, in bytes in binary mode
  std::ifstream file(filename, std::ios::ate | std::ios::binary);  
  if (file.is_open()) {
    // find the content length
    std::streamsize fileSize = file.tellg();

    // reset to read content from beginning
    file.seekg(0, std::ios::beg); 

    // find the content type
    std::string type;
    if (filename.find(".html") != std::string::npos) {
      type = "text/html";
    } else if (filename.find(".jpg") != std::string::npos) {
      type = "image/jpeg";
    }
    
    // all of the required response lines
    std::string status = "HTTP/1.0 200 OK";
    std::string contentLength = "Content-Length: " + std::to_string(fileSize);
    std::string contentType = "Content-Type: " + type;
    std::string newLine = "";

    sendLine(sockFd, status);
    sendLine(sockFd, contentLength);
    sendLine(sockFd, contentType);
    sendLine(sockFd, newLine);

    char buffer[BUFFER_SIZE];
    // while read fully succeeds or bytes still left to read
    while (file.read(buffer, BUFFER_SIZE) || file.gcount() > 0) {
      // send over the raw bytes
      write(sockFd, buffer, file.gcount());
    }
    file.close();
  }

  return;
}


// **************************************************************************************
// * processConnection
// * -- process one connection/request.
// **************************************************************************************
int processConnection(int sockFd) {
 
  // Call readHeader()

  // If read header returned 400, send 400

  // If read header returned 404, call send404
  
  // 471: If read header returned 200, call sendFile

  std::string fileName;
  int status = readHeader(sockFd, fileName);

  switch (status) {
    case (400):
      send400(sockFd);
      break;
    case (404):
      send404(sockFd);
      break;
    case (200):
      sendFile(sockFd, fileName);
      break;
    default:
      break;
  }

  // 598 students
  // - If the header was valid and the method was GET, call sendFile()
  // - If the header was valid and the method was HEAD, call a function to send back the header.
  // - If the header was valid and the method was POST, call a function to save the file to dis.

  return 0;
}
    

int main (int argc, char *argv[]) {


  // ********************************************************************
  // * Process the command line arguments
  // ********************************************************************
  int opt = 0;
  while ((opt = getopt(argc,argv,"d:")) != -1) {
    
    switch (opt) {
    case 'd':
      LOG_LEVEL = std::stoi(optarg);
      break;
    case ':':
    case '?':
    default:
      std::cout << "useage: " << argv[0] << " -d LOG_LEVEL" << std::endl;
      exit(-1);
    }
  }


  // *******************************************************************
  // * Catch all possible signals
  // ********************************************************************
  DEBUG << "Setting up signal handlers" << ENDL;
  

  
  // *******************************************************************
  // * Creating the inital socket using the socket() call.
  // ********************************************************************
  int listenFd;
  DEBUG << "Calling Socket() assigned file descriptor " << listenFd << ENDL;

  // taken from in-class slides
  if ((listenFd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
    std::cout << "Failed to create listening socket" << strerror(errno) << std::endl;
    exit(-1);
  }

  
  // ********************************************************************
  // * The bind() call takes a structure used to spefiy the details of the connection. 
  // *
  // * struct sockaddr_in servaddr;
  // *
  // On a cient it contains the address of the server to connect to. 
  // On the server it specifies which IP address and port to lisen for connections.
  // If you want to listen for connections on any IP address you use the
  // address INADDR_ANY
  // ********************************************************************

  // taken from in-class slides
  // 1. define the structure
  struct sockaddr_in servaddr;

  // ********************************************************************
  // * Binding configures the socket with the parameters we have
  // * specified in the servaddr structure.  This step is implicit in
  // * the connect() call, but must be explicitly listed for servers.
  // *
  // * Don't forget to check to see if bind() fails because the port
  // * you picked is in use, and if the port is in use, pick a different one.
  // ********************************************************************
  
  // taken from in-class slides
  // 1.a picking default port
  uint16_t port = 1701;
  DEBUG << "Calling bind()" << ENDL;
  
  std::cout << "Using port: " << port << std::endl;

  // 2. zero it
  bzero(&servaddr, sizeof(servaddr));

  // 3. ipv4 protocol family
  servaddr.sin_family = AF_INET;

  // 4. let system pick the ip address
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

  // 5. use default port
  servaddr.sin_port = htons(port);

  if(bind(listenFd, (sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
    std::cout << "bind() failed: " << strerror(errno) << std::endl;
    exit(-1);
  }

  // ********************************************************************
  // * Setting the socket to the listening state is the second step
  // * needed to being accepting connections.  This creates a que for
  // * connections and starts the kernel listening for connections.
  // ********************************************************************
  DEBUG << "Calling listen()" << ENDL;

  // taken from in-class slides
  if (listen(listenFd, 1) < 0) {
    std::cout << "listen() failed: " << strerror(errno) << std::endl;
    exit(-1);
  }


  // ********************************************************************
  // * The accept call will sleep, waiting for a connection.  When 
  // * a connection request comes in the accept() call creates a NEW
  // * socket with a new fd that will be used for the communication.
  // ********************************************************************
  int quitProgram = 0;
  while (!quitProgram) {
    int connFd = 0;
    DEBUG << "Calling connFd = accept(fd,NULL,NULL)." << ENDL;

    // taken from in-class slides
    if ((connFd = accept(listenFd, (sockaddr *) NULL, NULL)) < 0) {
      std::cout << "accept() failed: " << strerror(errno) << std::endl;
      exit(-1);
    }

    DEBUG << "We have recieved a connection on " << connFd << ". Calling processConnection(" << connFd << ")" << ENDL;
    quitProgram = processConnection(connFd);
    DEBUG << "processConnection returned " << quitProgram << " (should always be 0)" << ENDL;
    DEBUG << "Closing file descriptor " << connFd << ENDL;
    close(connFd);
  }
  

  ERROR << "Program fell through to the end of main. A listening socket may have closed unexpectadly." << ENDL;
  closefrom(3);

}
