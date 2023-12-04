// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/device/usb/android_usb_socket.h"

#include <stddef.h>

#include "base/check_op.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace {

const int kMaxPayload = 4096;

}  // namespace

AndroidUsbSocket::AndroidUsbSocket(scoped_refptr<AndroidUsbDevice> device,
                                   uint32_t socket_id,
                                   const std::string& command,
                                   base::OnceClosure delete_callback)
    : device_(device),
      command_(command),
      local_id_(socket_id),
      remote_id_(0),
      is_connected_(false),
      delete_callback_(std::move(delete_callback)) {}

AndroidUsbSocket::~AndroidUsbSocket() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_connected_)
    Disconnect();
  if (!delete_callback_.is_null())
    std::move(delete_callback_).Run();
}

void AndroidUsbSocket::HandleIncoming(std::unique_ptr<AdbMessage> message) {
  if (!device_.get())
    return;

  CHECK_EQ(message->arg1, local_id_);
  switch (message->command) {
    case AdbMessage::kCommandOKAY:
      if (!is_connected_) {
        remote_id_ = message->arg0;
        is_connected_ = true;
        if (!connect_callback_.is_null())
          std::move(connect_callback_).Run(net::OK);
        // "this" can be deleted.
      } else {
        RespondToWriter(write_length_);
        // "this" can be deleted.
      }
      break;
    case AdbMessage::kCommandWRTE:
      device_->Send(AdbMessage::kCommandOKAY, local_id_, message->arg0, "");
      read_buffer_ += message->body;
      // Allow WRTE over new connection even though OKAY ack was not received.
      if (!is_connected_) {
        remote_id_ = message->arg0;
        is_connected_ = true;
        if (!connect_callback_.is_null())
          std::move(connect_callback_).Run(net::OK);
        // "this" can be deleted.
      } else {
        RespondToReader(false);
        // "this" can be deleted.
      }
      break;
    case AdbMessage::kCommandCLSE:
      if (is_connected_)
        device_->Send(AdbMessage::kCommandCLSE, local_id_, 0, "");
      Terminated(true);
      // "this" can be deleted.
      break;
    default:
      break;
  }
}

void AndroidUsbSocket::Terminated(bool closed_by_device) {
  is_connected_ = false;

  // Break the socket -> device connection, release the device.
  device_ = nullptr;
  std::move(delete_callback_).Run();

  if (!closed_by_device)
    return;

  // Respond to pending callbacks.
  if (!connect_callback_.is_null()) {
    std::move(connect_callback_).Run(net::ERR_FAILED);
    // "this" can be deleted.
    return;
  }
  base::WeakPtr<AndroidUsbSocket> weak_this = weak_factory_.GetWeakPtr();
  RespondToReader(true);
  // "this" can be deleted.
  if (weak_this) {
    RespondToWriter(net::ERR_FAILED);
    // "this" can be deleted.
  }
}

int AndroidUsbSocket::Read(net::IOBuffer* buffer,
                           int length,
                           net::CompletionOnceCallback callback) {
  DCHECK(!callback.is_null());
  if (!is_connected_)
    return device_.get() ? net::ERR_SOCKET_NOT_CONNECTED : 0;

  DCHECK(read_callback_.is_null());
  if (read_buffer_.empty()) {
    read_callback_ = std::move(callback);
    read_io_buffer_ = buffer;
    read_length_ = length;
    return net::ERR_IO_PENDING;
  }

  size_t bytes_to_copy = static_cast<size_t>(length) > read_buffer_.length() ?
      read_buffer_.length() : static_cast<size_t>(length);
  memcpy(buffer->data(), read_buffer_.data(), bytes_to_copy);
  if (read_buffer_.length() > bytes_to_copy)
    read_buffer_ = read_buffer_.substr(bytes_to_copy);
  else
    read_buffer_ = std::string();
  return bytes_to_copy;
}

int AndroidUsbSocket::Write(
    net::IOBuffer* buffer,
    int length,
    net::CompletionOnceCallback callback,
    const net::NetworkTrafficAnnotationTag& /*traffic_annotation*/) {
  DCHECK(!callback.is_null());
  if (!is_connected_)
    return net::ERR_SOCKET_NOT_CONNECTED;

  if (length > kMaxPayload)
    length = kMaxPayload;

  DCHECK(write_callback_.is_null());
  write_callback_ = std::move(callback);
  write_length_ = length;
  device_->Send(AdbMessage::kCommandWRTE, local_id_, remote_id_,
                std::string(buffer->data(), length));
  return net::ERR_IO_PENDING;
}

int AndroidUsbSocket::SetReceiveBufferSize(int32_t size) {
  NOTIMPLEMENTED();
  return net::ERR_NOT_IMPLEMENTED;
}

int AndroidUsbSocket::SetSendBufferSize(int32_t size) {
  NOTIMPLEMENTED();
  return net::ERR_NOT_IMPLEMENTED;
}

int AndroidUsbSocket::Connect(net::CompletionOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback.is_null());
  if (!device_.get())
    return net::ERR_FAILED;

  DCHECK(!is_connected_);
  DCHECK(connect_callback_.is_null());
  connect_callback_ = std::move(callback);
  device_->Send(AdbMessage::kCommandOPEN, local_id_, 0, command_);
  return net::ERR_IO_PENDING;
}

void AndroidUsbSocket::Disconnect() {
  if (!device_.get())
    return;
  device_->Send(AdbMessage::kCommandCLSE, local_id_, remote_id_, "");
  Terminated(false);
}

bool AndroidUsbSocket::IsConnected() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_connected_;
}

bool AndroidUsbSocket::IsConnectedAndIdle() const {
  NOTIMPLEMENTED();
  return false;
}

int AndroidUsbSocket::GetPeerAddress(net::IPEndPoint* address) const {
  *address = net::IPEndPoint(net::IPAddress(0, 0, 0, 0), 0);
  return net::OK;
}

int AndroidUsbSocket::GetLocalAddress(net::IPEndPoint* address) const {
  NOTIMPLEMENTED();
  return net::ERR_NOT_IMPLEMENTED;
}

const net::NetLogWithSource& AndroidUsbSocket::NetLog() const {
  return net_log_;
}

bool AndroidUsbSocket::WasEverUsed() const {
  NOTIMPLEMENTED();
  return true;
}

net::NextProto AndroidUsbSocket::GetNegotiatedProtocol() const {
  NOTIMPLEMENTED();
  return net::kProtoUnknown;
}

bool AndroidUsbSocket::GetSSLInfo(net::SSLInfo* ssl_info) {
  return false;
}

int64_t AndroidUsbSocket::GetTotalReceivedBytes() const {
  NOTIMPLEMENTED();
  return 0;
}

void AndroidUsbSocket::ApplySocketTag(const net::SocketTag& tag) {
  NOTIMPLEMENTED();
}

void AndroidUsbSocket::RespondToReader(bool disconnect) {
  if (read_callback_.is_null() || (read_buffer_.empty() && !disconnect))
    return;
  size_t bytes_to_copy =
      static_cast<size_t>(read_length_) > read_buffer_.length() ?
          read_buffer_.length() : static_cast<size_t>(read_length_);
  memcpy(read_io_buffer_->data(), read_buffer_.data(), bytes_to_copy);
  if (read_buffer_.length() > bytes_to_copy)
    read_buffer_ = read_buffer_.substr(bytes_to_copy);
  else
    read_buffer_ = std::string();
  std::move(read_callback_).Run(bytes_to_copy);
}

void AndroidUsbSocket::RespondToWriter(int result) {
  if (!write_callback_.is_null())
    std::move(write_callback_).Run(result);
}
