// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/message_stream/fake_bluetooth_socket.h"

#include <utility>

#include "base/strings/string_number_conversions.h"

namespace ash {
namespace quick_pair {

FakeBluetoothSocket::FakeBluetoothSocket() = default;

FakeBluetoothSocket::~FakeBluetoothSocket() = default;

void FakeBluetoothSocket::Receive(
    int buffer_size,
    ReceiveCompletionCallback success_callback,
    ReceiveErrorCompletionCallback error_callback) {
  success_callback_ = std::move(success_callback);
  error_callback_ = std::move(error_callback);
}

void FakeBluetoothSocket::SetIOBufferFromBytes(std::vector<uint8_t> bytes) {
  bytes_ = std::move(bytes);
}

void FakeBluetoothSocket::SetErrorReason(
    device::BluetoothSocket::ErrorReason error) {
  error_ = error;
}

void FakeBluetoothSocket::SetEmptyBuffer() {
  empty_buffer_ = true;
}

void FakeBluetoothSocket::TriggerReceiveCallback() {
  if (bytes_.empty() && !empty_buffer_) {
    std::move(error_callback_)
        .Run(error_,
             /*error_message=*/"Error message");
    return;
  }

  std::string buffer_bytes(bytes_.begin(), bytes_.end());
  const size_t buffer_bytes_size = buffer_bytes.size();
  scoped_refptr<net::IOBuffer> io_buffer =
      base::MakeRefCounted<net::StringIOBuffer>(std::move(buffer_bytes));

  if (empty_buffer_) {
    io_buffer->data()[0] = '\0';
    empty_buffer_ = false;
  }
  std::move(success_callback_)
      .Run(/*buffer_size*/ buffer_bytes_size,
           /*buffer=*/std::move(io_buffer));
}

void FakeBluetoothSocket::Disconnect(base::OnceClosure success_callback) {
  std::move(success_callback).Run();
}

}  // namespace quick_pair
}  // namespace ash
