// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/message_stream/message_stream_lookup_impl.h"

#include <memory>

#include "ash/quick_pair/common/fake_bluetooth_adapter.h"
#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/message_stream/fake_bluetooth_socket.h"
#include "ash/quick_pair/message_stream/message_stream.h"
#include "ash/quick_pair/message_stream/message_stream_lookup.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
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

// Attempt retry `n` after cooldown period |message_retry_cooldowns[n-1]|.
const std::vector<base::TimeDelta> create_message_stream_retry_cooldowns_{
    base::Seconds(2), base::Seconds(4), base::Seconds(8), base::Seconds(16),
    base::Seconds(32)};

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

class MessageStreamFakeBluetoothDevice
    : public testing::NiceMock<device::MockBluetoothDevice> {
 public:
  MessageStreamFakeBluetoothDevice(FakeBluetoothAdapter* adapter)
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

  void SetConnectToServiceSuccess() { error_ = false; }

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
  raw_ptr<FakeBluetoothAdapter> fake_adapter_;
  scoped_refptr<FakeBluetoothSocket> fake_socket_ =
      base::MakeRefCounted<FakeBluetoothSocket>();
};

class MessageStreamLookupImplTest : public testing::Test,
                                    public MessageStreamLookup::Observer {
 public:
  void SetUp() override {
    adapter_ = base::MakeRefCounted<FakeBluetoothAdapter>();
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

  void SetConnectToServiceSuccess() { device_->SetConnectToServiceSuccess(); }

  MessageStream* GetMessageStream() {
    return message_stream_lookup_->GetMessageStream(kTestDeviceAddress);
  }

  void OnMessageStreamConnected(const std::string& device_address,
                                MessageStream* message_stream) override {
    message_stream_ = message_stream;
  }

  // Fast forwards in time between each attempt to create a message stream to
  // test that the retries did in fact fail. Assumes that |device_| has been
  // added to |adapter_| and that a service error has been set (probably via
  // SetConnectToServiceError).
  void UnsuccessfulAttemptCreateMessageStream(
      size_t num_unsuccessful_attempts) {
    if (!num_unsuccessful_attempts)
      return;

    if (num_unsuccessful_attempts > 5) {
      LOG(WARNING)
          << __func__
          << ": the maximum message stream attempts before failure is 5. "
          << num_unsuccessful_attempts
          << " were requested. 5 will be tested for failure.";
      num_unsuccessful_attempts = 5;
    }

    base::RunLoop();
    EXPECT_EQ(GetMessageStream(), nullptr);

    for (size_t curr_attempts = 1; curr_attempts < num_unsuccessful_attempts;
         curr_attempts++) {
      base::RunLoop();
      EXPECT_EQ(GetMessageStream(), nullptr);
      task_environment_.FastForwardBy(
          create_message_stream_retry_cooldowns_[curr_attempts - 1]);
    }
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
  raw_ptr<MessageStream, DanglingUntriaged> message_stream_ = nullptr;
  scoped_refptr<FakeBluetoothAdapter> adapter_;
  raw_ptr<MessageStreamFakeBluetoothDevice, DanglingUntriaged> device_;
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
  device_->SetConnected(true);
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
  device_->SetConnected(true);

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
  device_->SetConnected(true);
  EXPECT_EQ(device_->IsConnected(), true);
  EXPECT_EQ(GetMessageStream(), nullptr);

  DevicePairedChanged(/*new_paired_status=*/true);
  base::RunLoop().RunUntilIdle();
  EXPECT_NE(GetMessageStream(), nullptr);

  DevicePairedChanged(/*new_paired_status=*/false);
  EXPECT_EQ(GetMessageStream(), nullptr);
}

TEST_F(MessageStreamLookupImplTest, DevicePairedChanged_NotConnected) {
  device_->AddUUID(kMessageStreamUuid);
  device_->SetConnected(false);
  EXPECT_EQ(GetMessageStream(), nullptr);

  DevicePairedChanged(/*new_paired_status=*/true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(message_stream_, nullptr);
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

// There is a maximum of 5 retry attempts to establish a connection upon failing
// to do so. This tests all possible number of retries followed by a successful
// connection.
TEST_F(MessageStreamLookupImplTest,
       ConnectFailInitial_ConnectSuccessOnRetries) {
  device_->AddUUID(kMessageStreamUuid);
  for (size_t num_unsuccessful_attempts = 1; num_unsuccessful_attempts < 5;
       num_unsuccessful_attempts++) {
    SetConnectToServiceError(kMessageStreamConnectToServiceError);
    DeviceAdded();
    UnsuccessfulAttemptCreateMessageStream(num_unsuccessful_attempts);
    SetConnectToServiceSuccess();
    task_environment_.FastForwardBy(base::Seconds(33));
    base::RunLoop().RunUntilIdle();
    EXPECT_NE(GetMessageStream(), nullptr);
    DeviceRemoved();
  }
}

// There is a maximum of 5 retry attempts to establish a connection upon failing
// to do so. This tests a connection failure after failing all 6 attempts.
TEST_F(MessageStreamLookupImplTest,
       ConnectFailInitial_ConnectFailOnFiveRetries) {
  device_->AddUUID(kMessageStreamUuid);
  SetConnectToServiceError(kMessageStreamConnectToServiceError);
  DeviceAdded();
  UnsuccessfulAttemptCreateMessageStream(/*num_unsuccessful_attempts=*/5);
  task_environment_.FastForwardBy(base::Seconds(33));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetMessageStream(), nullptr);
}

TEST_F(MessageStreamLookupImplTest,
       ConnectFailInitial_NoCrashIfDeviceLostBetweenRetries) {
  device_->AddUUID(kMessageStreamUuid);
  SetConnectToServiceError(kMessageStreamConnectToServiceError);
  DeviceAdded();

  // Simulate the device being removed from adapter immediately following
  // pairing.
  adapter_->RemoveMockDevice(kTestDeviceAddress);

  // Expect the retries to not crash.
  UnsuccessfulAttemptCreateMessageStream(/*num_unsuccessful_attempts=*/5);
  task_environment_.FastForwardBy(base::Seconds(33));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetMessageStream(), nullptr);
}

}  // namespace quick_pair
}  // namespace ash
