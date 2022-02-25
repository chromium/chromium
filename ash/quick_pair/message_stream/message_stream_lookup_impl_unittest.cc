// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/message_stream/message_stream_lookup_impl.h"

#include <memory>

#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/message_stream/fake_bluetooth_socket.h"
#include "ash/quick_pair/message_stream/message_stream.h"
#include "ash/quick_pair/message_stream/message_stream_lookup.h"
#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_socket.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kAcceptFailedString[] = "Failed to accept connection.";
const char kInvalidUUIDString[] = "Invalid UUID";
const char kSocketNotListeningString[] = "Socket is not listening.";

constexpr char kTestDeviceAddress[] = "11:12:13:14:15:16";

const device::BluetoothUUID kMessageStreamUuid(
    "df21fe2c-2515-4fdb-8886-f12c4d67927c");

const char kMessageStreamConnectToServiceError[] =
    "Bluetooth.ChromeOS.FastPair.MessageStream.ConnectToService.ErrorReason";
const char kMessageStreamConnectToServiceResult[] =
    "Bluetooth.ChromeOS.FastPair.MessageStream.ConnectToService.Result";
const char kMessageStreamConnectToServiceTime[] =
    "Bluetooth.ChromeOS.FastPair.MessageStream.ConnectToService."
    "TotalConnectTime";

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

  void NotifyDeviceRemoved(device::BluetoothDevice* device) {
    for (auto& observer : observers_)
      observer.DeviceRemoved(this, device);
  }

  void NotifyDeviceAdded(device::BluetoothDevice* device) {
    for (auto& observer : observers_)
      observer.DeviceAdded(this, device);
  }

  void NotifyDevicePairedChanged(device::BluetoothDevice* device,
                                 bool new_paired_status) {
    for (auto& observer : observers_)
      observer.DevicePairedChanged(this, device, new_paired_status);
  }

  void NotifyDeviceChanged(device::BluetoothDevice* device) {
    for (auto& observer : observers_)
      observer.DeviceChanged(this, device);
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
    connect_to_service_count_++;

    if (dont_invoke_callback_) {
      return;
    }

    if (error_) {
      std::move(error_callback).Run(/*message=*/error_message_);
      return;
    }

    std::move(callback).Run(fake_socket_.get());
  }

  void SetConnectToServiceError(const std::string& error_message) {
    error_ = true;
    error_message_ = error_message;
  }

  int connect_to_service_count() { return connect_to_service_count_; }

  void DontInvokeCallback() { dont_invoke_callback_ = true; }

  // Move-only class
  MessageStreamFakeBluetoothDevice(const MessageStreamFakeBluetoothDevice&) =
      delete;
  MessageStreamFakeBluetoothDevice& operator=(
      const MessageStreamFakeBluetoothDevice&) = delete;

 protected:
  int connect_to_service_count_ = 0;
  bool dont_invoke_callback_ = false;
  bool error_ = false;
  std::string error_message_;
  MessageStreamFakeBluetoothAdapter* fake_adapter_;
  scoped_refptr<FakeBluetoothSocket> fake_socket_ =
      base::MakeRefCounted<FakeBluetoothSocket>();
};

class MessageStreamLookupImplTest : public testing::Test,
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

  void DeviceConnectedStateChanged(bool is_now_connected) {
    adapter_->NotifyDeviceConnectedStateChanged(
        /*device=*/device_,
        /*is_now_connected=*/is_now_connected);
  }

  void DeviceChanged() { adapter_->NotifyDeviceChanged(/*device=*/device_); }

  void DeviceAdded() { adapter_->NotifyDeviceAdded(/*device=*/device_); }

  void DeviceRemoved() { adapter_->NotifyDeviceRemoved(/*device=*/device_); }

  void EmptyDeviceRemoved() {
    adapter_->NotifyDeviceRemoved(/*device=*/nullptr);
  }

  void EmptyDevicePairedChanged(bool new_paired_status) {
    adapter_->NotifyDeviceConnectedStateChanged(
        /*device=*/nullptr,
        /*new_paired_status=*/new_paired_status);
  }

  void DevicePairedChanged(bool new_paired_status) {
    adapter_->NotifyDevicePairedChanged(
        /*device=*/device_,
        /*new_paired_status=*/new_paired_status);
  }

  void SetConnectToServiceError(const std::string& error_message) {
    device_->SetConnectToServiceError(error_message);
  }

  MessageStream* GetMessageStream() {
    return message_stream_lookup_->GetMessageStream(kTestDeviceAddress);
  }

  void OnMessageStreamConnected(const std::string& device_address,
                                MessageStream* message_stream) override {
    message_stream_ = message_stream;
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 protected:
  base::test::SingleThreadTaskEnvironment task_enviornment_;
  base::HistogramTester histogram_tester_;
  MessageStream* message_stream_ = nullptr;
  scoped_refptr<MessageStreamFakeBluetoothAdapter> adapter_;
  MessageStreamFakeBluetoothDevice* device_;
  std::unique_ptr<MessageStreamLookup> message_stream_lookup_;
};

TEST_F(MessageStreamLookupImplTest, ConnectDevice_NoMessageStreamUUid) {
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceError, 0);
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceTime, 0);
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceResult, 0);

  SetConnectToServiceError(kAcceptFailedString);

  EXPECT_EQ(GetMessageStream(), nullptr);
  DeviceConnectedStateChanged(/*is_now_connected=*/true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetMessageStream(), nullptr);

  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceError, 0);
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceTime, 0);
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceResult, 0);
}

TEST_F(MessageStreamLookupImplTest, DeviceAdded_NoMessageStreamUUid) {
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceError, 0);
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceTime, 0);
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceResult, 0);

  SetConnectToServiceError(kInvalidUUIDString);

  EXPECT_EQ(GetMessageStream(), nullptr);
  DeviceAdded();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetMessageStream(), nullptr);

  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceError, 0);
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceTime, 0);
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceResult, 0);
}

TEST_F(MessageStreamLookupImplTest, DeviceAdded_NotPaired) {
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceError, 0);
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceTime, 0);
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceResult, 0);

  SetConnectToServiceError(kSocketNotListeningString);
  device_->SetPaired(false);
  device_->AddUUID(kMessageStreamUuid);

  EXPECT_EQ(GetMessageStream(), nullptr);
  DeviceAdded();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetMessageStream(), nullptr);

  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceError, 0);
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceTime, 0);
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceResult, 0);
}

TEST_F(MessageStreamLookupImplTest, DeviceChanged_NoMessageStreamUUid) {
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceError, 0);
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceTime, 0);
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceResult, 0);

  SetConnectToServiceError(kSocketNotListeningString);

  EXPECT_EQ(GetMessageStream(), nullptr);
  DeviceChanged();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetMessageStream(), nullptr);

  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceError, 0);
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceTime, 0);
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceResult, 0);
}

TEST_F(MessageStreamLookupImplTest, DeviceChanged_NotPaired) {
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceError, 0);
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceTime, 0);
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceResult, 0);

  SetConnectToServiceError(kSocketNotListeningString);
  device_->SetPaired(false);
  device_->AddUUID(kMessageStreamUuid);

  EXPECT_EQ(GetMessageStream(), nullptr);
  DeviceChanged();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetMessageStream(), nullptr);

  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceError, 0);
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceTime, 0);
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceResult, 0);
}

TEST_F(MessageStreamLookupImplTest, DevicePaired_NoMessageStreamUUid) {
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceError, 0);
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceTime, 0);
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceResult, 0);

  SetConnectToServiceError(kInvalidUUIDString);

  EXPECT_EQ(GetMessageStream(), nullptr);
  DevicePairedChanged(/*new_paired_status=*/true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetMessageStream(), nullptr);

  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceError, 0);
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceTime, 0);
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceResult, 0);
}

TEST_F(MessageStreamLookupImplTest, DevicePairedChanged_NoDevice) {
  EXPECT_EQ(GetMessageStream(), nullptr);
  EmptyDevicePairedChanged(/*new_paired_status=*/true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetMessageStream(), nullptr);
}

TEST_F(MessageStreamLookupImplTest, ConnectDevice_ConnectToServiceFailure) {
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceError, 0);
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceTime, 0);
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceResult, 0);

  device_->AddUUID(kMessageStreamUuid);
  SetConnectToServiceError(kAcceptFailedString);

  EXPECT_EQ(GetMessageStream(), nullptr);
  DeviceConnectedStateChanged(/*is_now_connected=*/true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetMessageStream(), nullptr);

  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceError, 1);
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceTime, 0);
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceResult, 1);
}

TEST_F(MessageStreamLookupImplTest, DeviceAdded_ConnectToServiceFailure) {
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceError, 0);
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceTime, 0);
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceResult, 0);

  device_->AddUUID(kMessageStreamUuid);
  SetConnectToServiceError(kInvalidUUIDString);

  EXPECT_EQ(GetMessageStream(), nullptr);
  DeviceAdded();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetMessageStream(), nullptr);

  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceError, 1);
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceTime, 0);
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceResult, 1);
}

TEST_F(MessageStreamLookupImplTest,
       DevicePairedChanged_ConnectToServiceFailure) {
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceError, 0);
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceTime, 0);
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceResult, 0);

  device_->AddUUID(kMessageStreamUuid);
  SetConnectToServiceError(kSocketNotListeningString);

  EXPECT_EQ(GetMessageStream(), nullptr);
  DevicePairedChanged(/*new_paired_state=*/true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetMessageStream(), nullptr);

  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceError, 1);
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceTime, 0);
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceResult, 1);
}

TEST_F(MessageStreamLookupImplTest, DeviceChanged_ConnectToServiceFailure) {
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceError, 0);
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceTime, 0);
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceResult, 0);

  device_->AddUUID(kMessageStreamUuid);
  SetConnectToServiceError(kAcceptFailedString);

  EXPECT_EQ(GetMessageStream(), nullptr);
  DeviceChanged();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetMessageStream(), nullptr);

  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceError, 1);
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceTime, 0);
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceResult, 1);
}

TEST_F(MessageStreamLookupImplTest, ConnectDevice_ConnectToServiceSuccess) {
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceError, 0);
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceTime, 0);
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceResult, 0);

  device_->AddUUID(kMessageStreamUuid);

  EXPECT_EQ(GetMessageStream(), nullptr);
  DeviceConnectedStateChanged(/*is_now_connected=*/true);
  base::RunLoop().RunUntilIdle();
  EXPECT_NE(GetMessageStream(), nullptr);

  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceError, 0);
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceTime, 1);
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceResult, 1);
}

TEST_F(MessageStreamLookupImplTest,
       DevicePairedChanged_ConnectToServiceSuccess) {
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceError, 0);
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceTime, 0);
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceResult, 0);

  device_->AddUUID(kMessageStreamUuid);

  EXPECT_EQ(GetMessageStream(), nullptr);
  DevicePairedChanged(/*new_paired_state=*/true);
  base::RunLoop().RunUntilIdle();
  EXPECT_NE(GetMessageStream(), nullptr);

  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceError, 0);
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceTime, 1);
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceResult, 1);
}

TEST_F(MessageStreamLookupImplTest, DeviceAdded_ConnectToServiceSuccess) {
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceError, 0);
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceTime, 0);
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceResult, 0);

  device_->AddUUID(kMessageStreamUuid);

  EXPECT_EQ(GetMessageStream(), nullptr);
  DeviceAdded();
  base::RunLoop().RunUntilIdle();
  EXPECT_NE(GetMessageStream(), nullptr);

  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceError, 0);
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceTime, 1);
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceResult, 1);
}

TEST_F(MessageStreamLookupImplTest, DeviceChanged_ConnectToServiceSuccess) {
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceError, 0);
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceTime, 0);
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceResult, 0);

  device_->AddUUID(kMessageStreamUuid);

  EXPECT_EQ(GetMessageStream(), nullptr);
  DeviceChanged();
  base::RunLoop().RunUntilIdle();
  EXPECT_NE(GetMessageStream(), nullptr);

  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceError, 0);
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceTime, 1);
  histogram_tester().ExpectTotalCount(kMessageStreamConnectToServiceResult, 1);
}

TEST_F(MessageStreamLookupImplTest,
       ConnectDevice_ConnectToServiceSuccess_Observer) {
  device_->AddUUID(kMessageStreamUuid);

  EXPECT_EQ(GetMessageStream(), nullptr);
  DeviceConnectedStateChanged(/*is_now_connected=*/true);
  base::RunLoop().RunUntilIdle();
  EXPECT_NE(message_stream_, nullptr);
}

TEST_F(MessageStreamLookupImplTest,
       DevicePairedChanged_ConnectToServiceSuccess_Observer) {
  device_->AddUUID(kMessageStreamUuid);

  EXPECT_EQ(GetMessageStream(), nullptr);
  DevicePairedChanged(/*new_paired_status=*/true);
  base::RunLoop().RunUntilIdle();
  EXPECT_NE(message_stream_, nullptr);
}

TEST_F(MessageStreamLookupImplTest,
       DeviceAdded_ConnectToServiceSuccess_Observer) {
  device_->AddUUID(kMessageStreamUuid);

  EXPECT_EQ(GetMessageStream(), nullptr);
  DeviceAdded();
  base::RunLoop().RunUntilIdle();
  EXPECT_NE(message_stream_, nullptr);
}

TEST_F(MessageStreamLookupImplTest,
       DeviceChanged_ConnectToServiceSuccess_Observer) {
  device_->AddUUID(kMessageStreamUuid);

  EXPECT_EQ(GetMessageStream(), nullptr);
  DeviceChanged();
  base::RunLoop().RunUntilIdle();
  EXPECT_NE(message_stream_, nullptr);
}

TEST_F(MessageStreamLookupImplTest, ConnectDevice_DisconnectDevice) {
  device_->AddUUID(kMessageStreamUuid);

  EXPECT_EQ(GetMessageStream(), nullptr);
  DeviceConnectedStateChanged(/*is_now_connected=*/true);
  base::RunLoop().RunUntilIdle();
  EXPECT_NE(GetMessageStream(), nullptr);

  DeviceConnectedStateChanged(/*is_now_connected=*/false);
  EXPECT_EQ(GetMessageStream(), nullptr);
}

TEST_F(MessageStreamLookupImplTest, PairDevice_UnpairDevice) {
  device_->AddUUID(kMessageStreamUuid);

  EXPECT_EQ(GetMessageStream(), nullptr);
  DevicePairedChanged(/*new_paired_status=*/true);
  base::RunLoop().RunUntilIdle();
  EXPECT_NE(GetMessageStream(), nullptr);

  DevicePairedChanged(/*new_paired_status=*/false);
  EXPECT_EQ(GetMessageStream(), nullptr);
}

TEST_F(MessageStreamLookupImplTest, AddDevice_RemoveDevice) {
  device_->AddUUID(kMessageStreamUuid);

  EXPECT_EQ(GetMessageStream(), nullptr);
  DeviceAdded();
  base::RunLoop().RunUntilIdle();
  EXPECT_NE(GetMessageStream(), nullptr);

  DeviceRemoved();
  EXPECT_EQ(GetMessageStream(), nullptr);
}

TEST_F(MessageStreamLookupImplTest, RemoveDevice_NoMessageStream) {
  device_->AddUUID(kMessageStreamUuid);

  EXPECT_EQ(GetMessageStream(), nullptr);
  DeviceRemoved();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(GetMessageStream(), nullptr);
}

TEST_F(MessageStreamLookupImplTest, EmptyDeviceRemoved) {
  EXPECT_EQ(GetMessageStream(), nullptr);
  EmptyDeviceRemoved();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(GetMessageStream(), nullptr);
}

TEST_F(MessageStreamLookupImplTest, InFlightConnections) {
  device_->AddUUID(kMessageStreamUuid);
  device_->SetPaired(true);
  device_->DontInvokeCallback();

  EXPECT_EQ(GetMessageStream(), nullptr);
  DeviceConnectedStateChanged(/*is_now_connected=*/true);
  DeviceChanged();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(device_->connect_to_service_count(), 1);
}

}  // namespace quick_pair
}  // namespace ash
