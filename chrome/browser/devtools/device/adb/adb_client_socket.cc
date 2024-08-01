// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/devtools/device/adb/adb_client_socket.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_source.h"
#include "net/socket/tcp_client_socket.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace {

const int kBufferSize = 16 * 1024;
const char kOkayResponse[] = "OKAY";
const char kHostTransportCommand[] = "host:transport:%s";
const char kLocalhost[] = "127.0.0.1";

std::string EncodeMessage(const std::string& message) {
  size_t length = message.length();
  CHECK_LE(length, 0xffffu);
  std::string result;
  result.reserve(4);
  base::AppendHexEncodedByte(reinterpret_cast<const uint8_t*>(&length)[1],
                             result);
  base::AppendHexEncodedByte(reinterpret_cast<const uint8_t*>(&length)[0],
                             result);
  return result + message;
}

class AdbTransportSocket : public AdbClientSocket {
 public:
  AdbTransportSocket(int port,
                     const std::string& serial,
                     const std::string& socket_name,
                     SocketCallback callback)
      : AdbClientSocket(port),
        serial_(serial),
        socket_name_(socket_name),
        callback_(std::move(callback)) {
    Connect(base::BindOnce(&AdbTransportSocket::OnConnected,
                           base::Unretained(this)));
  }

 private:
  ~AdbTransportSocket() { DCHECK(callback_.is_null()); }

  void OnConnected(int result) {
    if (!CheckNetResultOrDie(result))
      return;
    SendCommand(base::StringPrintf(kHostTransportCommand, serial_.c_str()),
                true,
                base::BindOnce(&AdbTransportSocket::SendLocalAbstract,
                               base::Unretained(this)));
  }

  void SendLocalAbstract(int result, const std::string& response) {
    if (!CheckNetResultOrDie(result))
      return;
    SendCommand(socket_name_, true,
                base::BindOnce(&AdbTransportSocket::OnSocketAvailable,
                               base::Unretained(this)));
  }

  void OnSocketAvailable(int result, const std::string& response) {
    if (!CheckNetResultOrDie(result))
      return;
    std::move(callback_).Run(net::OK, std::move(socket_));
    delete this;
  }

  bool CheckNetResultOrDie(int result) {
    if (result >= 0)
      return true;
    std::move(callback_).Run(result,
                             base::WrapUnique<net::StreamSocket>(nullptr));
    delete this;
    return false;
  }

  std::string serial_;
  std::string socket_name_;
  SocketCallback callback_;
};

class AdbQuerySocket : AdbClientSocket {
 public:
  AdbQuerySocket(int port, const std::string& query, CommandCallback callback)
      : AdbClientSocket(port),
        current_query_(0),
        callback_(std::move(callback)) {
    queries_ = base::SplitString(query, "|", base::KEEP_WHITESPACE,
                                 base::SPLIT_WANT_NONEMPTY);
    if (queries_.empty()) {
      CheckNetResultOrDie(net::ERR_INVALID_ARGUMENT);
      return;
    }
    Connect(
        base::BindOnce(&AdbQuerySocket::SendNextQuery, base::Unretained(this)));
  }

 private:
  ~AdbQuerySocket() { DCHECK(callback_.is_null()); }

  void SendNextQuery(int result) {
    if (!CheckNetResultOrDie(result))
      return;
    std::string query = queries_[current_query_];
    if (query.length() > 0xFFFF) {
      CheckNetResultOrDie(net::ERR_MSG_TOO_BIG);
      return;
    }
    bool is_void = current_query_ < queries_.size() - 1;
    SendCommand(
        query, is_void,
        base::BindOnce(&AdbQuerySocket::OnResponse, base::Unretained(this)));
  }

  void OnResponse(int result, const std::string& response) {
    if (++current_query_ < queries_.size()) {
      SendNextQuery(net::OK);
    } else {
      std::move(callback_).Run(result, response);
      delete this;
    }
  }

  bool CheckNetResultOrDie(int result) {
    if (result >= 0)
      return true;
    std::move(callback_).Run(result, std::string());
    delete this;
    return false;
  }

  std::vector<std::string> queries_;
  size_t current_query_;
  CommandCallback callback_;
};

}  // namespace

// static
void AdbClientSocket::AdbQuery(int port,
                               const std::string& query,
                               CommandCallback callback) {
  new AdbQuerySocket(port, query, std::move(callback));
}

// static
void AdbClientSocket::TransportQuery(int port,
                                     const std::string& serial,
                                     const std::string& socket_name,
                                     SocketCallback callback) {
  new AdbTransportSocket(port, serial, socket_name, std::move(callback));
}

AdbClientSocket::AdbClientSocket(int port)
    : host_(kLocalhost), port_(port) {
}

AdbClientSocket::~AdbClientSocket() {
}

void AdbClientSocket::RunConnectCallback(int result) {
  std::move(connect_callback_).Run(result);
}

void AdbClientSocket::Connect(net::CompletionOnceCallback callback) {
  net::IPAddress ip_address;
  if (!ip_address.AssignFromIPLiteral(host_)) {
    std::move(callback).Run(net::ERR_FAILED);
    return;
  }

  net::AddressList address_list =
      net::AddressList::CreateFromIPAddress(ip_address, port_);
  socket_ = std::make_unique<net::TCPClientSocket>(
      address_list, nullptr, nullptr, nullptr, net::NetLogSource());
  connect_callback_ = std::move(callback);
  int result = socket_->Connect(base::BindOnce(
      &AdbClientSocket::RunConnectCallback, base::Unretained(this)));
  if (result != net::ERR_IO_PENDING)
    AdbClientSocket::RunConnectCallback(result);
}

void AdbClientSocket::SendCommand(const std::string& command,
                                  bool is_void,
                                  CommandCallback callback) {
  scoped_refptr<net::StringIOBuffer> request_buffer =
      base::MakeRefCounted<net::StringIOBuffer>(EncodeMessage(command));
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("adb_client_socket", R"(
        semantics {
          sender: "ADB Client Socket"
          description:
            "Remote debugging is supported over existing ADB (Android Debug "
            "Bridge) connection, in addition to raw USB connection. This "
            "socket talks to the local ADB daemon which routes debugging "
            "traffic to a remote device."
          trigger:
            "A user connects to an Android device using remote debugging."
          data: "Any data required for remote debugging."
          destination: LOCAL
        }
        policy {
          cookies_allowed: NO
          setting:
            "To use adb with a device connected over USB, you must enable USB "
            "debugging in the device system settings, under Developer options."
          policy_exception_justification:
            "This is not a network request and is only used for remote "
            "debugging."
        })");

  auto split_callback = base::SplitOnceCallback(
      base::BindOnce(&AdbClientSocket::ReadResponse, base::Unretained(this),
                     std::move(callback), is_void));
  int result =
      socket_->Write(request_buffer.get(), request_buffer->size(),
                     std::move(split_callback.first), traffic_annotation);
  if (result != net::ERR_IO_PENDING) {
    std::move(split_callback.second).Run(result);
  }
}

void AdbClientSocket::ReadResponse(CommandCallback callback,
                                   bool is_void,
                                   int result) {
  if (result < 0) {
    std::move(callback).Run(result, "IO error");
    return;
  }
  auto response_buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(kBufferSize);
  auto split_callback = base::SplitOnceCallback(
      base::BindOnce(&AdbClientSocket::OnResponseHeader, base::Unretained(this),
                     std::move(callback), is_void, response_buffer));
  result = socket_->Read(response_buffer.get(), kBufferSize,
                         std::move(split_callback.first));
  if (result != net::ERR_IO_PENDING) {
    std::move(split_callback.second).Run(result);
  }
}

void AdbClientSocket::OnResponseHeader(
    CommandCallback callback,
    bool is_void,
    scoped_refptr<net::IOBuffer> response_buffer,
    int result) {
  if (result <= 0) {
    std::move(callback).Run(result == 0 ? net::ERR_CONNECTION_CLOSED : result,
                            "IO error");
    return;
  }

  std::string data = std::string(response_buffer->data(), result);
  if (result < 4) {
    std::move(callback).Run(net::ERR_FAILED, "Response is too short: " + data);
    return;
  }

  std::string status = data.substr(0, 4);
  if (status != kOkayResponse) {
    std::move(callback).Run(net::ERR_FAILED, data);
    return;
  }

  // Trim OKAY.
  data = data.substr(4);
  if (!is_void)
    OnResponseData(std::move(callback), data, response_buffer, -1, 0);
  else
    std::move(callback).Run(net::OK, data);
}

void AdbClientSocket::OnResponseData(
    CommandCallback callback,
    const std::string& response,
    scoped_refptr<net::IOBuffer> response_buffer,
    int bytes_left,
    int result) {
  if (result < 0) {
    std::move(callback).Run(result, "IO error");
    return;
  }

  std::string new_response = response +
      std::string(response_buffer->data(), result);

  if (bytes_left == -1) {
    // First read the response header.
    int payload_length = 0;
    if (new_response.length() >= 4 &&
        base::HexStringToInt(new_response.substr(0, 4), &payload_length)) {
      new_response = new_response.substr(4);
      bytes_left = payload_length - new_response.size();
    }
  } else {
    bytes_left -= result;
  }

  if (bytes_left == 0) {
    std::move(callback).Run(net::OK, new_response);
    return;
  }

  // Read tail
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  result = socket_->Read(
      response_buffer.get(), kBufferSize,
      base::BindOnce(&AdbClientSocket::OnResponseData, base::Unretained(this),
                     std::move(split_callback.first), new_response,
                     response_buffer, bytes_left));
  if (result > 0) {
    OnResponseData(std::move(split_callback.second), new_response,
                   response_buffer, bytes_left, result);
  } else if (result != net::ERR_IO_PENDING) {
    std::move(split_callback.second).Run(net::OK, new_response);
  }
}
