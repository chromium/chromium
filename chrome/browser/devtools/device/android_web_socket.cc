// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/rand_util.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/devtools/device/android_device_manager.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/server/web_socket_encoder.h"
#include "net/socket/stream_socket.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

using content::BrowserThread;
using net::WebSocket;

namespace {

const int kBufferSize = 16 * 1024;
const char kCloseResponse[] = "\x88\x80\x2D\x0E\x1E\xFA";

net::NetworkTrafficAnnotationTag kAndroidWebSocketTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("android_web_socket", R"(
        semantics {
          sender: "Android Web Socket"
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
}  // namespace

class AndroidDeviceManager::AndroidWebSocket::WebSocketImpl {
 public:
  WebSocketImpl(
      scoped_refptr<base::SingleThreadTaskRunner> response_task_runner,
      base::WeakPtr<AndroidWebSocket> weak_socket,
      const std::string& extensions,
      const std::string& body_head,
      std::unique_ptr<net::StreamSocket> socket)
      : response_task_runner_(response_task_runner),
        weak_socket_(weak_socket),
        socket_(std::move(socket)),
        encoder_(net::WebSocketEncoder::CreateClient(extensions)),
        response_buffer_(body_head) {
    thread_checker_.DetachFromThread();
  }

  void StartListening() {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK(socket_);

    scoped_refptr<net::IOBuffer> buffer =
        base::MakeRefCounted<net::IOBuffer>(kBufferSize);

    if (!response_buffer_.empty())
      ProcessResponseBuffer(buffer);
    else
      Read(buffer);
  }

  void SendFrame(const std::string& message) {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (!socket_)
      return;
    int mask = base::RandInt(0, 0x7FFFFFFF);
    std::string encoded_frame;
    encoder_->EncodeFrame(message, mask, &encoded_frame);
    SendData(encoded_frame);
  }

  base::WeakPtr<WebSocketImpl> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  void Read(scoped_refptr<net::IOBuffer> io_buffer) {
    int result =
        socket_->Read(io_buffer.get(), kBufferSize,
                      base::Bind(&WebSocketImpl::OnBytesRead,
                                 weak_factory_.GetWeakPtr(), io_buffer));
    if (result != net::ERR_IO_PENDING)
      OnBytesRead(io_buffer, result);
  }

  void OnBytesRead(scoped_refptr<net::IOBuffer> io_buffer, int result) {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (result <= 0) {
      Disconnect();
      return;
    }
    response_buffer_.append(io_buffer->data(), result);

    ProcessResponseBuffer(io_buffer);
  }

  void ProcessResponseBuffer(scoped_refptr<net::IOBuffer> io_buffer) {
    int bytes_consumed;
    std::string output;
    WebSocket::ParseResult parse_result = encoder_->DecodeFrame(
        response_buffer_, &bytes_consumed, &output);

    while (parse_result == WebSocket::FRAME_OK) {
      response_buffer_ = response_buffer_.substr(bytes_consumed);
      response_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&AndroidWebSocket::OnFrameRead, weak_socket_, output));
      parse_result = encoder_->DecodeFrame(
          response_buffer_, &bytes_consumed, &output);
    }
    if (parse_result == WebSocket::FRAME_CLOSE)
      SendData(kCloseResponse);

    if (parse_result == WebSocket::FRAME_ERROR) {
      Disconnect();
      return;
    }
    Read(io_buffer);
  }

  void SendData(const std::string& data) {
    request_buffer_ += data;
    if (request_buffer_.length() == data.length())
      SendPendingRequests(0);
  }

  void SendPendingRequests(int result) {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (result < 0) {
      Disconnect();
      return;
    }
    request_buffer_ = request_buffer_.substr(result);
    if (request_buffer_.empty())
      return;

    scoped_refptr<net::StringIOBuffer> buffer =
        base::MakeRefCounted<net::StringIOBuffer>(request_buffer_);
    result = socket_->Write(buffer.get(), buffer->size(),
                            base::Bind(&WebSocketImpl::SendPendingRequests,
                                       weak_factory_.GetWeakPtr()),
                            kAndroidWebSocketTrafficAnnotation);
    if (result != net::ERR_IO_PENDING)
      SendPendingRequests(result);
  }

  void Disconnect() {
    DCHECK(thread_checker_.CalledOnValidThread());
    socket_.reset();
    response_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&AndroidWebSocket::OnSocketClosed, weak_socket_));
  }

  scoped_refptr<base::SingleThreadTaskRunner> response_task_runner_;
  base::WeakPtr<AndroidWebSocket> weak_socket_;
  std::unique_ptr<net::StreamSocket> socket_;
  std::unique_ptr<net::WebSocketEncoder> encoder_;
  std::string response_buffer_;
  std::string request_buffer_;
  base::ThreadChecker thread_checker_;
  DISALLOW_COPY_AND_ASSIGN(WebSocketImpl);

  base::WeakPtrFactory<WebSocketImpl> weak_factory_{this};
};

AndroidDeviceManager::AndroidWebSocket::AndroidWebSocket(
    scoped_refptr<Device> device,
    const std::string& socket_name,
    const std::string& path,
    Delegate* delegate)
    : device_(device),
      socket_impl_(nullptr, base::OnTaskRunnerDeleter(device->task_runner_)),
      delegate_(delegate) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(delegate_);
  DCHECK(device_);
  device_->HttpUpgrade(
      socket_name, path, net::WebSocketEncoder::kClientExtensions,
      base::Bind(&AndroidWebSocket::Connected, weak_factory_.GetWeakPtr()));
}

AndroidDeviceManager::AndroidWebSocket::~AndroidWebSocket() = default;

void AndroidDeviceManager::AndroidWebSocket::SendFrame(
    const std::string& message) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(socket_impl_);
  DCHECK(device_);
  device_->task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&WebSocketImpl::SendFrame,
                                socket_impl_->GetWeakPtr(), message));
}

void AndroidDeviceManager::AndroidWebSocket::Connected(
    int result,
    const std::string& extensions,
    const std::string& body_head,
    std::unique_ptr<net::StreamSocket> socket) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (result != net::OK || !socket.get()) {
    OnSocketClosed();
    return;
  }
  socket_impl_.reset(new WebSocketImpl(base::ThreadTaskRunnerHandle::Get(),
                                       weak_factory_.GetWeakPtr(), extensions,
                                       body_head, std::move(socket)));
  device_->task_runner_->PostTask(FROM_HERE,
                                  base::BindOnce(&WebSocketImpl::StartListening,
                                                 socket_impl_->GetWeakPtr()));
  delegate_->OnSocketOpened();
}

void AndroidDeviceManager::AndroidWebSocket::OnFrameRead(
    const std::string& message) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  delegate_->OnFrameRead(message);
}

void AndroidDeviceManager::AndroidWebSocket::OnSocketClosed() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  delegate_->OnSocketClosed();
}

AndroidDeviceManager::AndroidWebSocket*
AndroidDeviceManager::Device::CreateWebSocket(
    const std::string& socket_name,
    const std::string& path,
    AndroidWebSocket::Delegate* delegate) {
  return new AndroidWebSocket(this, socket_name, path, delegate);
}
