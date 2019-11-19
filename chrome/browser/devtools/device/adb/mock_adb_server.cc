// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/device/adb/mock_adb_server.h"

#include <stddef.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_source.h"
#include "net/socket/stream_socket.h"
#include "net/socket/tcp_server_socket.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"

using content::BrowserThread;

namespace {

const char kHostTransportPrefix[] = "host:transport:";
const char kLocalAbstractPrefix[] = "localabstract:";

const char kShellPrefix[] = "shell:";
const char kOpenedUnixSocketsCommand[] = "cat /proc/net/unix";
const char kDeviceModelCommand[] = "getprop ro.product.model";
const char kDumpsysCommand[] = "dumpsys window policy";
const char kListProcessesCommand[] = "ps";
const char kListUsersCommand[] = "dumpsys user";
const char kEchoCommandPrefix[] = "echo ";

const char kSerialOnline[] = "01498B321301A00A";
const char kSerialOffline[] = "01498B2B0D01300E";
const char kDeviceModel[] = "Nexus 6";

const char kJsonVersionPath[] = "/json/version";
const char kJsonPath[] = "/json";
const char kJsonListPath[] = "/json/list";

const char kHttpRequestTerminator[] = "\r\n\r\n";

const char kHttpResponse[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Length:%d\r\n"
    "Content-Type:application/json; charset=UTF-8\r\n\r\n%s";

const char kSampleOpenedUnixSockets[] =
    "Num       RefCount Protocol Flags    Type St Inode Path\n"
    "00000000: 00000004 00000000"
    " 00000000 0002 01  3328 /dev/socket/wpa_wlan0\n"
    "00000000: 00000002 00000000"
    " 00010000 0001 01  5394 /dev/socket/vold\n"
    "00000000: 00000002 00000000"
    " 00010000 0001 01 11810 @webview_devtools_remote_2425\n"
    "00000000: 00000002 00000000"
    " 00010000 0001 01 20893 @chrome_devtools_remote\n"
    "00000000: 00000002 00000000"
    " 00010000 0001 01 20894 @chrome_devtools_remote_1002\n"
    "00000000: 00000002 00000000"
    " 00010000 0001 01 20895 @noprocess_devtools_remote\n";

const char kSampleListProcesses[] =
    "USER    PID  PPID VSIZE  RSS    WCHAN    PC         NAME\n"
    "root    1    0    688    508    ffffffff 00000000 S /init\r\n"
    "u0_a75  2425 123  933736 193024 ffffffff 00000000 S com.sample.feed\r\n"
    "nfc     741  123  706448 26316  ffffffff 00000000 S com.android.nfc\r\n"
    "u0_a76  1001 124  111111 222222 ffffffff 00000000 S com.android.chrome\r\n"
    "u10_a77 1002 125  111111 222222 ffffffff 00000000 S com.chrome.beta\r\n"
    "u0_a78  1003 126  111111 222222 ffffffff 00000000 S com.noprocess.app\r\n";

const char kSampleDumpsys[] =
    "WINDOW MANAGER POLICY STATE (dumpsys window policy)\r\n"
    "    mSafeMode=false mSystemReady=true mSystemBooted=true\r\n"
    "    mStable=(0,50)-(720,1184)\r\n" // Only mStable parameter is parsed
    "    mForceStatusBar=false mForceStatusBarFromKeyguard=false\r\n";

const char kSampleListUsers[] =
    "Users:\r\n"
    "  UserInfo{0:Test User:13} serialNo=0\r\n"
    "    Created: <unknown>\r\n"
    "    Last logged in: +17m18s871ms ago\r\n"
    "  UserInfo{10:Test User : 2:10} serialNo=10\r\n"
    "    Created: +3d4h35m1s139ms ago\r\n"
    "    Last logged in: +17m26s287ms ago\r\n";

char kSampleChromeVersion[] = "{\n"
    "   \"Browser\": \"Chrome/32.0.1679.0\",\n"
    "   \"Protocol-Version\": \"1.0\",\n"
    "   \"User-Agent\": \"Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/32.0.1679.0 Safari/537.36\",\n"
    "   \"WebKit-Version\": \"537.36 (@160162)\"\n"
    "}";

char kSampleChromeBetaVersion[] = "{\n"
    "   \"Browser\": \"Chrome/31.0.1599.0\",\n"
    "   \"Protocol-Version\": \"1.0\",\n"
    "   \"User-Agent\": \"Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/32.0.1679.0 Safari/537.36\",\n"
    "   \"WebKit-Version\": \"537.36 (@160162)\"\n"
    "}";

char kSampleWebViewVersion[] = "{\n"
    "   \"Browser\": \"Version/4.0\",\n"
    "   \"Protocol-Version\": \"1.0\",\n"
    "   \"User-Agent\": \"Mozilla/5.0 (Linux; Android 4.3; Build/KRS74B) "
    "AppleWebKit/537.36 (KHTML, like Gecko) Version/4.0 Safari/537.36\",\n"
    "   \"WebKit-Version\": \"537.36 (@157588)\"\n"
    "}";

char kSampleChromePages[] = "[ {\n"
    "   \"description\": \"\",\n"
    "   \"devtoolsFrontendUrl\": \"/devtools/devtools.html?"
    "ws=/devtools/page/0\",\n"
    "   \"id\": \"0\",\n"
    "   \"title\": \"The Chromium Projects\",\n"
    "   \"type\": \"page\",\n"
    "   \"url\": \"http://www.chromium.org/\",\n"
    "   \"webSocketDebuggerUrl\": \""
    "ws:///devtools/page/0\"\n"
    "} ]";

char kSampleChromeBetaPages[] = "[ {\n"
    "   \"description\": \"\",\n"
    "   \"devtoolsFrontendUrl\": \"/devtools/devtools.html?"
    "ws=/devtools/page/0\",\n"
    "   \"id\": \"0\",\n"
    "   \"title\": \"The Chromium Projects\",\n"
    "   \"type\": \"page\",\n"
    "   \"url\": \"http://www.chromium.org/\",\n"
    "   \"webSocketDebuggerUrl\": \""
    "ws:///devtools/page/0\"\n"
    "} ]";

char kSampleWebViewPages[] = "[ {\n"
    "   \"description\": \"{\\\"attached\\\":false,\\\"empty\\\":false,"
    "\\\"height\\\":1173,\\\"screenX\\\":0,\\\"screenY\\\":0,"
    "\\\"visible\\\":true,\\\"width\\\":800}\",\n"
    "   \"devtoolsFrontendUrl\": \"http://chrome-devtools-frontend.appspot.com/"
    "serve_rev/@157588/devtools.html?ws="
    "/devtools/page/3E962D4D-B676-182D-3BE8-FAE7CE224DE7\",\n"
    "   \"faviconUrl\": \"http://chromium.org/favicon.ico\",\n"
    "   \"id\": \"3E962D4D-B676-182D-3BE8-FAE7CE224DE7\",\n"
    "   \"title\": \"Blink - The Chromium Projects\",\n"
    "   \"type\": \"page\",\n"
    "   \"url\": \"http://www.chromium.org/blink\",\n"
    "   \"webSocketDebuggerUrl\": \"ws:///devtools/"
    "page/3E962D4D-B676-182D-3BE8-FAE7CE224DE7\"\n"
    "}, {\n"
    "   \"description\": \"{\\\"attached\\\":true,\\\"empty\\\":true,"
    "\\\"screenX\\\":0,\\\"screenY\\\":33,\\\"visible\\\":false}\",\n"
    "   \"devtoolsFrontendUrl\": \"http://chrome-devtools-frontend.appspot.com/"
    "serve_rev/@157588/devtools.html?ws="
    "/devtools/page/44681551-ADFD-2411-076B-3AB14C1C60E2\",\n"
    "   \"faviconUrl\": \"\",\n"
    "   \"id\": \"44681551-ADFD-2411-076B-3AB14C1C60E2\",\n"
    "   \"title\": \"More Activity\",\n"
    "   \"type\": \"page\",\n"
    "   \"url\": \"about:blank\",\n"
    "   \"webSocketDebuggerUrl\": \"ws:///devtools/page/"
    "44681551-ADFD-2411-076B-3AB14C1C60E2\"\n"
    "}]";

static const int kBufferSize = 16*1024;
static const uint16_t kAdbPort = 5037;

static const int kAdbMessageHeaderSize = 4;

class SimpleHttpServer {
 public:
  class Parser {
   public:
    virtual int Consume(const char* data, int size) = 0;
    virtual ~Parser() {}
  };

  using SendCallback = base::Callback<void(const std::string&)>;
  using ParserFactory = base::Callback<Parser*(const SendCallback&)>;

  SimpleHttpServer(const ParserFactory& factory, net::IPEndPoint endpoint);
  virtual ~SimpleHttpServer();

 private:
  class Connection {
   public:
    Connection(net::StreamSocket* socket, const ParserFactory& factory);
    virtual ~Connection();

   private:
    void Send(const std::string& message);
    void ReadData();
    void OnDataRead(int count);
    void WriteData();
    void OnDataWritten(int count);

    std::unique_ptr<net::StreamSocket> socket_;
    std::unique_ptr<Parser> parser_;
    scoped_refptr<net::GrowableIOBuffer> input_buffer_;
    scoped_refptr<net::GrowableIOBuffer> output_buffer_;
    int bytes_to_write_;
    bool read_closed_;

    SEQUENCE_CHECKER(sequence_checker_);

    base::WeakPtrFactory<Connection> weak_factory_{this};

    DISALLOW_COPY_AND_ASSIGN(Connection);
  };

  void OnConnect();
  void OnAccepted(int result);

  ParserFactory factory_;
  std::unique_ptr<net::TCPServerSocket> socket_;
  std::unique_ptr<net::StreamSocket> client_socket_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<SimpleHttpServer> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SimpleHttpServer);
};

SimpleHttpServer::SimpleHttpServer(const ParserFactory& factory,
                                   net::IPEndPoint endpoint)
    : factory_(factory),
      socket_(new net::TCPServerSocket(nullptr, net::NetLogSource())) {
  socket_->Listen(endpoint, 5);
  OnConnect();
}

SimpleHttpServer::~SimpleHttpServer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

SimpleHttpServer::Connection::Connection(net::StreamSocket* socket,
                                         const ParserFactory& factory)
    : socket_(socket),
      parser_(
          factory.Run(base::Bind(&Connection::Send, base::Unretained(this)))),
      input_buffer_(base::MakeRefCounted<net::GrowableIOBuffer>()),
      output_buffer_(base::MakeRefCounted<net::GrowableIOBuffer>()),
      bytes_to_write_(0),
      read_closed_(false) {
  input_buffer_->SetCapacity(kBufferSize);
  ReadData();
}

SimpleHttpServer::Connection::~Connection() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void SimpleHttpServer::Connection::Send(const std::string& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const char* data = message.c_str();
  int size = message.size();

  if ((output_buffer_->offset() + bytes_to_write_ + size) >
      output_buffer_->capacity()) {
    // If not enough space without relocation
    if (output_buffer_->capacity() < (bytes_to_write_ + size)) {
      // If even buffer is not enough
      int new_size = std::max(output_buffer_->capacity() * 2, size * 2);
      output_buffer_->SetCapacity(new_size);
    }
    memmove(output_buffer_->StartOfBuffer(),
            output_buffer_->data(),
            bytes_to_write_);
    output_buffer_->set_offset(0);
  }

  memcpy(output_buffer_->data() + bytes_to_write_, data, size);
  bytes_to_write_ += size;

  if (bytes_to_write_ == size)
    // If write loop wasn't yet started, then start it
    WriteData();
}

void SimpleHttpServer::Connection::ReadData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (input_buffer_->RemainingCapacity() == 0)
    input_buffer_->SetCapacity(input_buffer_->capacity() * 2);

  int read_result = socket_->Read(
      input_buffer_.get(),
      input_buffer_->RemainingCapacity(),
      base::Bind(&Connection::OnDataRead, base::Unretained(this)));

  if (read_result != net::ERR_IO_PENDING)
    OnDataRead(read_result);
}

void SimpleHttpServer::Connection::OnDataRead(int count) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (count <= 0) {
    if (bytes_to_write_ == 0)
      delete this;
    else
      read_closed_ = true;
    return;
  }
  input_buffer_->set_offset(input_buffer_->offset() + count);
  int bytes_processed;

  do {
    char* data = input_buffer_->StartOfBuffer();
    int data_size = input_buffer_->offset();
    bytes_processed = parser_->Consume(data, data_size);

    if (bytes_processed) {
      memmove(data, data + bytes_processed, data_size - bytes_processed);
      input_buffer_->set_offset(data_size - bytes_processed);
    }
  } while (bytes_processed);
  // Posting to avoid deep recursion in case of synchronous IO
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&Connection::ReadData, weak_factory_.GetWeakPtr()));
}

void SimpleHttpServer::Connection::WriteData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_GE(output_buffer_->capacity(),
           output_buffer_->offset() + bytes_to_write_) << "Overflow";

  int write_result = socket_->Write(
      output_buffer_.get(), bytes_to_write_,
      base::Bind(&Connection::OnDataWritten, base::Unretained(this)),
      TRAFFIC_ANNOTATION_FOR_TESTS);

  if (write_result != net::ERR_IO_PENDING)
    OnDataWritten(write_result);
}

void SimpleHttpServer::Connection::OnDataWritten(int count) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (count < 0) {
    delete this;
    return;
  }
  CHECK_GT(count, 0);
  CHECK_GE(output_buffer_->capacity(),
           output_buffer_->offset() + bytes_to_write_) << "Overflow";

  bytes_to_write_ -= count;
  output_buffer_->set_offset(output_buffer_->offset() + count);

  if (bytes_to_write_ != 0)
    // Posting to avoid deep recursion in case of synchronous IO
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&Connection::WriteData, weak_factory_.GetWeakPtr()));
  else if (read_closed_)
    delete this;
}

void SimpleHttpServer::OnConnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int accept_result = socket_->Accept(&client_socket_,
      base::Bind(&SimpleHttpServer::OnAccepted, base::Unretained(this)));

  if (accept_result != net::ERR_IO_PENDING)
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&SimpleHttpServer::OnAccepted,
                                  weak_factory_.GetWeakPtr(), accept_result));
}

void SimpleHttpServer::OnAccepted(int result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ASSERT_EQ(result, 0);  // Fails if the socket is already in use.
  new Connection(client_socket_.release(), factory_);
  OnConnect();
}

class AdbParser : public SimpleHttpServer::Parser,
                  public MockAndroidConnection::Delegate {
 public:
  static Parser* Create(FlushMode flush_mode,
                        const SimpleHttpServer::SendCallback& callback) {
    return new AdbParser(flush_mode, callback);
  }

  ~AdbParser() override { DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_); }

 private:
  explicit AdbParser(FlushMode flush_mode,
                     const SimpleHttpServer::SendCallback& callback)
      : flush_mode_(flush_mode),
        callback_(callback) {
  }

  int Consume(const char* data, int size) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (mock_connection_) {
      mock_connection_->Receive(std::string(data, size));
      return size;
    }
    if (size >= kAdbMessageHeaderSize) {
      std::string message_header(data, kAdbMessageHeaderSize);
      int message_size;

      EXPECT_TRUE(base::HexStringToInt(message_header, &message_size));

      if (size >= message_size + kAdbMessageHeaderSize) {
        std::string message_body(data + kAdbMessageHeaderSize, message_size);
        ProcessCommand(message_body);
        return kAdbMessageHeaderSize + message_size;
      }
    }
    return 0;
  }

  void ProcessCommand(const std::string& command) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (command == "host:devices") {
      SendSuccess(base::StringPrintf("%s\tdevice\n%s\toffline",
                                     kSerialOnline,
                                     kSerialOffline));
    } else if (base::StartsWith(command, kHostTransportPrefix,
                                base::CompareCase::SENSITIVE)) {
      serial_ = command.substr(sizeof(kHostTransportPrefix) - 1);
      SendSuccess(std::string());
    } else if (serial_ != kSerialOnline) {
      Send("FAIL", "device offline (x)");
    } else {
      mock_connection_ =
          std::make_unique<MockAndroidConnection>(this, serial_, command);
    }
  }

  void SendSuccess(const std::string& response) override {
    Send("OKAY", response);
  }

  void SendRaw(const std::string& data) override {
    callback_.Run(data);
  }

  void Send(const std::string& status, const std::string& response) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK_EQ(4U, status.size());
    std::string buffer = status;
    if (flush_mode_ == FlushWithoutSize) {
        callback_.Run(buffer);
        buffer = std::string();
    }

    int size = response.size();
    if (size > 0) {
      static const char kHexChars[] = "0123456789ABCDEF";
      for (int i = 3; i >= 0; i--)
        buffer += kHexChars[ (size >> 4*i) & 0x0f ];
      if (flush_mode_ == FlushWithSize) {
          callback_.Run(buffer);
          buffer = std::string();
      }
      buffer += response;
      callback_.Run(buffer);
    } else if (flush_mode_ != FlushWithoutSize) {
      callback_.Run(buffer);
    }
  }

  FlushMode flush_mode_;
  SimpleHttpServer::SendCallback callback_;
  std::string serial_;
  std::unique_ptr<MockAndroidConnection> mock_connection_;

  SEQUENCE_CHECKER(sequence_checker_);
};

static SimpleHttpServer* mock_adb_server_ = NULL;

void StartMockAdbServerOnIOThread(FlushMode flush_mode) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  CHECK(mock_adb_server_ == NULL);
  net::IPEndPoint endpoint(net::IPAddress(127, 0, 0, 1), kAdbPort);
  mock_adb_server_ = new SimpleHttpServer(
      base::Bind(&AdbParser::Create, flush_mode), endpoint);
}

void StopMockAdbServerOnIOThread() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  CHECK(mock_adb_server_ != NULL);
  delete mock_adb_server_;
  mock_adb_server_ = NULL;
}

} // namespace

MockAndroidConnection::MockAndroidConnection(
    Delegate* delegate,
    const std::string& serial,
    const std::string& command)
    : delegate_(delegate),
      serial_(serial) {
  ProcessCommand(command);
}

MockAndroidConnection::~MockAndroidConnection() {
}

void MockAndroidConnection::Receive(const std::string& data) {
  request_ += data;
  size_t request_end_pos = data.find(kHttpRequestTerminator);
  if (request_end_pos == std::string::npos)
    return;

  std::string request(request_.substr(0, request_end_pos));
  std::vector<base::StringPiece> lines = base::SplitStringPieceUsingSubstr(
      request, "\r\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  CHECK_GE(2U, lines.size());
  std::vector<std::string> tokens = base::SplitString(
      lines[0], " ", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  CHECK_EQ(3U, tokens.size());
  CHECK_EQ("GET", tokens[0]);
  CHECK_EQ("HTTP/1.1", tokens[2]);
  CHECK_EQ("Host: 0.0.0.0:0", lines[1]);

  std::string path(tokens[1]);
  if (path == kJsonPath)
    path = kJsonListPath;

  if (socket_name_ == "chrome_devtools_remote") {
    if (path == kJsonVersionPath)
      SendHTTPResponse(kSampleChromeVersion);
    else if (path == kJsonListPath)
      SendHTTPResponse(kSampleChromePages);
    else
      NOTREACHED() << "Unknown command " << request;
  } else if (socket_name_ == "chrome_devtools_remote_1002") {
    if (path == kJsonVersionPath)
      SendHTTPResponse(kSampleChromeBetaVersion);
    else if (path == kJsonListPath)
      SendHTTPResponse(kSampleChromeBetaPages);
    else
      NOTREACHED() << "Unknown command " << request;
  } else if (base::StartsWith(socket_name_, "noprocess_devtools_remote",
                              base::CompareCase::SENSITIVE)) {
    if (path == kJsonVersionPath)
      SendHTTPResponse("{}");
    else if (path == kJsonListPath)
      SendHTTPResponse("[]");
    else
      NOTREACHED() << "Unknown command " << request;
  } else if (socket_name_ == "webview_devtools_remote_2425") {
    if (path == kJsonVersionPath)
      SendHTTPResponse(kSampleWebViewVersion);
    else if (path == kJsonListPath)
      SendHTTPResponse(kSampleWebViewPages);
    else
      NOTREACHED() << "Unknown command " << request;
  } else {
    NOTREACHED() << "Unknown socket " << socket_name_;
  }
}

void MockAndroidConnection::ProcessCommand(const std::string& command) {
  if (base::StartsWith(command, kLocalAbstractPrefix,
                       base::CompareCase::SENSITIVE)) {
    socket_name_ = command.substr(sizeof(kLocalAbstractPrefix) - 1);
    delegate_->SendSuccess(std::string());
    return;
  }

  if (base::StartsWith(command, kShellPrefix, base::CompareCase::SENSITIVE)) {
    std::string result;
    for (const auto& line :
         base::SplitString(command.substr(sizeof(kShellPrefix) - 1), "\n",
                           base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
      if (line == kDeviceModelCommand) {
        result += kDeviceModel;
        result += "\r\n";
      } else if (line == kOpenedUnixSocketsCommand) {
        result += kSampleOpenedUnixSockets;
      } else if (line == kDumpsysCommand) {
        result += kSampleDumpsys;
      } else if (line == kListProcessesCommand) {
        result += kSampleListProcesses;
      } else if (line == kListUsersCommand) {
        result += kSampleListUsers;
      } else if (base::StartsWith(line, kEchoCommandPrefix,
                                  base::CompareCase::SENSITIVE)) {
        result += line.substr(sizeof(kEchoCommandPrefix) - 1);
        result += "\r\n";
      } else {
        NOTREACHED() << "Unknown shell command - " << command;
      }
    }
    delegate_->SendSuccess(result);
  } else {
    NOTREACHED() << "Unknown command - " << command;
  }
  delegate_->Close();
}

void MockAndroidConnection::SendHTTPResponse(const std::string& body) {
  std::string response_data(base::StringPrintf(kHttpResponse,
                                               static_cast<int>(body.size()),
                                               body.c_str()));
  delegate_->SendRaw(response_data);
}

void StartMockAdbServer(FlushMode flush_mode) {
  base::RunLoop run_loop;
  base::PostTaskAndReply(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&StartMockAdbServerOnIOThread, flush_mode),
      run_loop.QuitClosure());
  run_loop.Run();
}

void StopMockAdbServer() {
  base::RunLoop run_loop;
  base::PostTaskAndReply(FROM_HERE, {BrowserThread::IO},
                         base::BindOnce(&StopMockAdbServerOnIOThread),
                         run_loop.QuitClosure());
  run_loop.Run();
}
