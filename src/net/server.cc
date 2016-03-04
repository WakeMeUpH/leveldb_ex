#include <vector>
#include <boost/thread/thread.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <glog/logging.h>
#include "net/server.h"

namespace leveldb_ex {
namespace net {

Server::Server(const std::string& address, const std::string& port, std::size_t thread_pool_size)
           : thread_pool_size_(thread_pool_size),
             signals_(io_service_),
             acceptor_(io_service_),
             new_connection_(),
             request_parser_(),
             request_handler_() {
  signals_.add(SIGINT);
  signals_.add(SIGTERM);
#if defined(SIGQUIT)
  signals_.add(SIGQUIT);
#endif // defined(SIGQUIT)
  signals_.async_wait(boost::bind(&Server::HandleStop, this));

  boost::asio::ip::tcp::resolver resolver(io_service_);
  boost::asio::ip::tcp::resolver::query query(address, port);
  boost::asio::ip::tcp::endpoint endpoint = *resolver.resolve(query);
  acceptor_.open(endpoint.protocol());
  acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
  acceptor_.set_option(boost::asio::ip::tcp::acceptor::keep_alive(true));
  acceptor_.set_option(boost::asio::ip::tcp::no_delay(true));
  acceptor_.bind(endpoint);
  acceptor_.listen();
  LOG(ERROR) << "LISTEN " << address << ":" << port ;

  StartAccept();
}

void Server::Run() {
  std::vector<boost::shared_ptr<boost::thread> > threads;
  for (std::size_t i = 0; i < thread_pool_size_; ++i) {
    boost::shared_ptr<boost::thread> thread(
        new boost::thread(
              boost::bind(&boost::asio::io_service::run, &io_service_)));
    threads.push_back(thread);
  }
  for (std::size_t i = 0; i < threads.size(); ++i) {
    threads[i]->join();
  }
}

void Server::StartAccept() {
  new_connection_.reset(new Connection(io_service_, request_parser_, request_handler_));
  acceptor_.async_accept(new_connection_->Socket(),
         boost::bind(&Server::HandleAccept, this, boost::asio::placeholders::error));
}

void Server::HandleAccept(const boost::system::error_code& e) {
  if (!e) {
    new_connection_->SetPeer();
    LOG(INFO) << "ACCEPT NEW CONN " << new_connection_->GetPeer();
    new_connection_->Start();
  }
  StartAccept();
}

void Server::HandleStop()
{
  io_service_.stop();
}

} // namespace net
} // namespace leveldb_ex

