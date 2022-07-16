// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/message_stream/message_stream_lookup.h"

#include <memory>

#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/message_stream/fake_bluetooth_socket.h"
#include "ash/quick_pair/message_stream/message_stream.h"
#include "ash/quick_pair/message_stream/message_stream_lookup_impl.h"
#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_socket.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kTestDeviceAddress[] = "11:12:13:14:15:16";

const device::BluetoothUUID kMessageStreamUuid(
    "df21fe2c-2515-4fdb-8886-f12c4d67927c");

}  // namespace

namespace ash {
namespace quick_pair {

class MessageStreamFakeBluetoothAdapter
    : public testing::NiceMock<device::MockBluetoothAdapter> {
 public:
  device::BluetoothDevice* GetDevice(const std::string& address) override {
    for (const auto& it : mock_devices_) {
      if (it->GetAddress() == address)
        return it.get();
    }
    return nullptr;
  }

  void NotifyDeviceConnectedStateChanged(device::BluetoothDevice* device,
                                         bool is_now_connected) {
    for (auto& observer : observers_)
      observer.DeviceConnectedStateChanged(this, device, is_now_connected);
  }

 private:
  ~MessageStreamFakeBluetoothAdapter() override = default;
};

class MessageStreamFakeBluetoothDevice
    : public testing::NiceMock<device::MockBluetoothDevice> {
 public:
  MessageStreamFakeBluetoothDevice(MessageStreamFakeBluetoothAdapter* adapter)
      : testing::NiceMock<device::MockBluetoothDevice>(adapter,
                                                       /*bluetooth_class=*/0u,
                                                       /*name=*/"Test Device",
                                                       kTestDeviceAddress,
                                                       /*paired=*/true,
                                                       /*connected=*/true),
        fake_adapter_(adapter) {}

  void ConnectToService(const device::BluetoothUUID& uuid,
                        ConnectToServiceCallback callback,
                        ConnectToServiceErrorCallback error_callback) override {
    if (error_) {
      std::move(error_callback).Run(/*message=*/"Connect to service error.");
      return;
    }

    std::move(callback).Run(fake_socket_.get());
  }

  void SetConnectToServiceError() { error_ = true; }

  // Move-only class
  MessageStreamFakeBluetoothDevice(const MessageStreamFakeBluetoothDevice&) =
      delete;
  MessageStreamFakeBluetoothDevice& operator=(
      const MessageStreamFakeBluetoothDevice&) = delete;

 protected:
  bool error_ = false;
  MessageStreamFakeBluetoothAdapter* fake_adapter_;
  scoped_refptr<FakeBluetoothSocket> fake_socket_ =
      base::MakeRefCounted<FakeBluetoothSocket>();
};

class MessageStreamLookupTest : public testing::Test,
                                public MessageStreamLookup::Observer {
 public:
  void SetUp() override {
    adapter_ = base::MakeRefCounted<MessageStreamFakeBluetoothAdapter>();
    std::unique_ptr<MessageStreamFakeBluetoothDevice> device =
        std::make_unique<MessageStreamFakeBluetoothDevice>(adapter_.get());

    device_ = device.get();
    adapter_->AddMockDevice(std::move(device));

    device::BluetoothAdapterFactory::SetAdapterForTesting(adapter_);
    message_stream_lookup_ = std::make_unique<MessageStreamLookupImpl>();
    message_stream_lookup_->AddObserver(this);
  }

  void NotifyDeviceConnectedStateChanged(bool is_now_connected) {
    adapter_->NotifyDeviceConnectedStateChanged(
        /*device=*/device_,
        /*is_now_connected=*/is_now_connected);
  }

  void SetConnectToServiceError() { device_->SetConnectToServiceError(); }

  MessageStream* GetMessageStream() {
    return message_stream_lookup_->GetMessageStream(kTestDeviceAddress);
  }

  void OnMessageStreamConnected(const std::string& device_address,
                                MessageStream* message_stream) override {
    message_stream_ = message_stream;
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_enviornment_;
  MessageStream* message_stream_ = nullptr;
  scoped_refptr<MessageStreamFakeBluetoothAdapter> adapter_;
  MessageStreamFakeBluetoothDevice* device_;
  std::unique_ptr<MessageStreamLookup> message_stream_lookup_;
};

TEST_F(MessageStreamLookupTest, ConnectDevice_NoMessageStreamUUid) {
  SetConnectToServiceError();

  EXPECT_EQ(GetMessageStream(), nullptr);
  NotifyDeviceConnectedStateChanged(/*is_now_connected=*/true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetMessageStream(), nullptr);
}

TEST_F(MessageStreamLookupTest, ConnectDevice_ConnectToServiceFailure) {
  device_->AddUUID(kMessageStreamUuid);
  SetConnectToServiceError();

  EXPECT_EQ(GetMessageStream(), nullptr);
  NotifyDeviceConnectedStateChanged(/*is_now_connected=*/true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetMessageStream(), nullptr);
}

TEST_F(MessageStreamLookupTest, ConnectDevice_ConnectToServiceSuccess) {
  device_->AddUUID(kMessageStreamUuid);

  EXPECT_EQ(GetMessageStream(), nullptr);
  NotifyDeviceConnectedStateChanged(/*is_now_connected=*/true);
  base::RunLoop().RunUntilIdle();
  EXPECT_NE(GetMessageStream(), nullptr);
}

TEST_F(MessageStreamLookupTest,
       ConnectDevice_ConnectToServiceSuccess_Observer) {
  device_->AddUUID(kMessageStreamUuid);

  EXPECT_EQ(GetMessageStream(), nullptr);
  NotifyDeviceConnectedStateChanged(/*is_now_connected=*/true);
  base::RunLoop().RunUntilIdle();
  EXPECT_NE(message_stream_, nullptr);
}

TEST_F(MessageStreamLookupTest, ConnectDevice_DisconnectDevice) {
  device_->AddUUID(kMessageStreamUuid);

  EXPECT_EQ(GetMessageStream(), nullptr);
  NotifyDeviceConnectedStateChanged(/*is_now_connected=*/true);
  base::RunLoop().RunUntilIdle();
  EXPECT_NE(GetMessageStream(), nullptr);

  NotifyDeviceConnectedStateChanged(/*is_now_connected=*/false);
  EXPECT_EQ(GetMessageStream(), nullptr);
}

}  // namespace quick_pair
}  // namespace ash
