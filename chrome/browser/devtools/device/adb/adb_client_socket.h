// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVICE_ADB_ADB_CLIENT_SOCKET_H_
#define CHROME_BROWSER_DEVTOOLS_DEVICE_ADB_ADB_CLIENT_SOCKET_H_

#include <string>

#include "base/functional/callback.h"
#include "net/base/io_buffer.h"
#include "net/socket/stream_socket.h"

class AdbClientSocket {
 public:
  using CommandCallback = base::OnceCallback<void(int, const std::string&)>;
  using SocketCallback =
      base::OnceCallback<void(int result, std::unique_ptr<net::StreamSocket>)>;

  static void AdbQuery(int port,
                       const std::string& query,
                       CommandCallback callback);

  static void TransportQuery(int port,
                             const std::string& serial,
                             const std::string& socket_name,
                             SocketCallback callback);

  AdbClientSocket(const AdbClientSocket&) = delete;
  AdbClientSocket& operator=(const AdbClientSocket&) = delete;

 protected:
  explicit AdbClientSocket(int port);
  ~AdbClientSocket();

  void Connect(net::CompletionOnceCallback callback);

  void SendCommand(const std::string& command,
                   bool is_void,
                   CommandCallback callback);

  std::unique_ptr<net::StreamSocket> socket_;

 private:
  void ReadResponse(CommandCallback callback, bool is_void, int result);

  void OnResponseHeader(CommandCallback callback,
                        bool is_void,
                        scoped_refptr<net::IOBuffer> response_buffer,
                        int result);

  void OnResponseData(CommandCallback callback,
                      const std::string& response,
                      scoped_refptr<net::IOBuffer> response_buffer,
                      int bytes_left,
                      int result);

  void RunConnectCallback(int result);

  net::CompletionOnceCallback connect_callback_;

  std::string host_;
  int port_;
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVICE_ADB_ADB_CLIENT_SOCKET_H_
