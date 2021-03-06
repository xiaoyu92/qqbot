#include "irc.h"

IrcClient::IrcClient(boost::asio::io_service &io_service,const privmsg_cb &cb,const std::string& server, const std::string& port):cb_(cb),resolver_(io_service),socket_(io_service)
{
  
    boost::asio::ip::tcp::resolver::query query(server,port);
    boost::system::error_code ec;
    boost::asio::ip::tcp::resolver::iterator endpoint_iterator = resolver_.resolve(query,ec);

    if (!ec)
    {
        boost::asio::async_connect(socket_, endpoint_iterator,
            boost::bind(&IrcClient::handle_connect_request, this,
            boost::asio::placeholders::error));
    }
    else
    {
#ifdef DEBUG
        std::cout << "Error: " << ec.message() << "\n";
#endif
    }
}

IrcClient::~IrcClient()
{

}

void IrcClient::oper(const std::string& user,const std::string& pwd)
{
    send_request("OPER "+user+" "+pwd);
}
void IrcClient::chat(const std::string& whom,const std::string& msg)
{
    return send_request("PRIVMSG "+whom+" :"+msg);
}

void IrcClient::login(const std::string& user,const std::string& ch,const std::string& user_pwd,const std::string& ch_pwd)
{
    if (!user_pwd.empty())
        send_request("PASS "+user_pwd);

    send_request("NICK "+user);
    send_request("USER "+user+ " 0 * "+user);

    if (ch_pwd.empty())
        send_request("JOIN "+ch);
    else
        send_request("JOIN "+ch+" "+ch_pwd);
}

void IrcClient::process_request(std::size_t readed)
{
    std::istream is(&response_);

    is.unsetf(std::ios_base::skipws);

    std::string req;
    req.append(std::istream_iterator<char>(is), std::istream_iterator<char>());

#ifdef DEBUG
    std::cout << req;
#endif

    if (req.substr(0,4)=="PING")
        send_request("PONG "+req.substr(6,req.length()-8));
    
    size_t pos=req.find(" PRIVMSG ")+1;

    if (pos)
    {
        std::string msg=req;
        IrcMsg m;

        pos=msg.find("!")+1;
        if (!pos)
            return;

        m.whom=msg.substr(1,pos-2);

        msg=msg.substr(pos);

        pos=msg.find(" PRIVMSG ")+1;
        if (!pos)
            return;

        m.locate=msg.substr(0,pos-1);

        msg=msg.substr(pos);

        pos=msg.find("PRIVMSG ")+1;

        if (!pos)
            return;

        msg=msg.substr(strlen("PRIVMSG "));

        pos=msg.find(" ")+1;

        if (!pos)
            return;
        
        m.from=msg.substr(0,pos-1);

        msg=msg.substr(pos);

        pos=msg.find(":")+1;

        if (!pos)
            return;

        m.msg=msg.substr(pos,msg.length()-2-pos);

        cb_(m);
    }

}

void IrcClient::handle_read_request(const boost::system::error_code& err, std::size_t readed)
{
    if (!err)
    {
        process_request(readed);
        boost::asio::async_read_until(socket_, response_, "\r\n",
            boost::bind(&IrcClient::handle_read_request, this,
            boost::asio::placeholders::error,boost::asio::placeholders::bytes_transferred));
    }
    else if (err != boost::asio::error::eof)
    {
#ifdef DEBUG
        std::cout << "Error: " << err.message() << "\n";
#endif
    }
}

void IrcClient::handle_write_request(const boost::system::error_code& err, std::size_t bytewrited)
{    
    if (!err)    
    {		
        request_.consume(bytewrited);		
        if (request_.size())			
            boost::asio::async_write(socket_,
            request_,
            boost::bind(&IrcClient::handle_write_request, this,boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)); 
    }    
    else    
    {
#ifdef DEBUG        
        std::cout << "Error: " << err.message() << "\n";
#endif 
    }
}

void IrcClient::handle_connect_request(const boost::system::error_code& err)
{
    if (!err)
    {
        boost::asio::async_read_until(socket_, response_, "\r\n",
            boost::bind(&IrcClient::handle_read_request, this,
            boost::asio::placeholders::error,boost::asio::placeholders::bytes_transferred));
    }
    else if (err != boost::asio::error::eof)
    {
#ifdef DEBUG
        std::cout << "Error: " << err.message() << "\n";
#endif
    }
}

void IrcClient::send_command(const std::string& cmd)
{
    send_request(cmd);
}

void IrcClient::send_request(const std::string& msg)
{
    boost::system::error_code ec;
    std::ostream request_stream(&request_);
    request_stream << msg+"\r\n";

    boost::asio::async_write(socket_, request_,
        boost::bind(&IrcClient::handle_write_request, this,
        boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
}