#include <http/HttpServer.h>

#include <http/HttpResponse.h>
#include <http/HttpRequest.h>
#include <http/HttpContext.h>
#include <http/HttpHandler.h>

#include <net/Logging.h>
#include <net/TcpConnection.h>


HttpServer::HttpServer(EventLoop *loop,
                       const InetAddress &listenAddr,
                       const std::string &name,
                       const std::string &root)
        : server_(loop, listenAddr),
          httpHandler_(&defaultHttpHandler),
          name_(name),
          root_(root) {
    server_.setConnectionCallback(
            std::bind(&HttpServer::onConnection, this, std::placeholders::_1));
    server_.setMessageCallback(
            std::bind(&HttpServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

HttpServer::~HttpServer() {
}

void HttpServer::start() {
    log_info("HttpServer[%s] starts listenning on %s", name_.c_str(), server_.ipPort().c_str());
    server_.start();
}

void HttpServer::onConnection(const TcpConnectionPtr &conn) {
    if (conn->connected()) {
        conn->setContext(HttpContext());
    }
}

void HttpServer::onMessage(const TcpConnectionPtr &conn,
                           Buffer *buf,
                           Clock::time_point receiveTime) {
    HttpContext *context =  boost::any_cast<HttpContext>(conn->getMutableContext());

    if (!context->parseRequest(buf, receiveTime)) {
        conn->send("HTTP/1.1 400 Bad Request\r\n\r\n");
        conn->shutdown();
    }

    if (context->gotAll()) {
        onRequest(conn, context->request());
        context->reset();
    }
}

void HttpServer::onRequest(const TcpConnectionPtr &conn, const HttpRequest &req) {
    const std::string &connection = req.header("Connection");
    bool         close = connection == "close" ||
                         (req.version() == HttpRequest::kHttp10 && connection != "Keep-Alive");
    HttpResponse response(close);
    httpHandler_(req, &response, this);
    Buffer buf;
    response.appendToBuffer(&buf);

    conn->send(&buf);
    if (response.closeConnection()) {
        conn->shutdown();
    }
}
