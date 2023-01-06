// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_MESSAGE_STREAM_FAKE_BLUETOOTH_SOCKET_H_
#define ASH_QUICK_PAIR_MESSAGE_STREAM_FAKE_BLUETOOTH_SOCKET_H_

#include <memory>

#include "base/functional/callback.h"
#include "device/bluetooth/test/mock_bluetooth_socket.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace quick_pair {

class FakeBluetoothSocket
    : public testing::NiceMock<device::MockBluetoothSocket> {
 public:
  FakeBluetoothSocket();

  // Move-only class
  FakeBluetoothSocket(const FakeBluetoothSocket&) = delete;
  FakeBluetoothSocket& operator=(const FakeBluetoothSocket&) = delete;

  void Receive(int buffer_size,
               ReceiveCompletionCallback success_callback,
               ReceiveErrorCompletionCallback error_callback) override;

  void Disconnect(base::OnceClosure success_callback) override;

  void SetIOBufferFromBytes(std::vector<uint8_t> bytes);

  void SetErrorReason(device::BluetoothSocket::ErrorReason error);

  void TriggerReceiveCallback();

  void SetEmptyBuffer();

 protected:
  ~FakeBluetoothSocket() override;

 private:
  device::BluetoothSocket::ErrorReason error_ =
      device::BluetoothSocket::ErrorReason::kIOPending;
  std::vector<uint8_t> bytes_;
  ReceiveCompletionCallback success_callback_;
  ReceiveErrorCompletionCallback error_callback_;
  bool empty_buffer_ = false;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_MESSAGE_STREAM_FAKE_BLUETOOTH_SOCKET_H_
