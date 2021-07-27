// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/pairing/fast_pair/fast_pair_gatt_service_client.h"

#include <stddef.h>

#include "ash/quick_pair/common/constants.h"
#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/common/pair_failure.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/test/mock_callback.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_connection.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

// Below constants are used to construct MockBluetoothDevice for testing.
constexpr char kTestBleDeviceAddress[] = "11:12:13:14:15:16";
const char kTestServiceId[] = "service_id1";
const device::BluetoothUUID kNonFastPairUuid("0xFE2B");

class FakeBluetoothAdapter
    : public testing::NiceMock<device::MockBluetoothAdapter> {
 public:
  FakeBluetoothAdapter() = default;

  // Move-only class
  FakeBluetoothAdapter(const FakeBluetoothAdapter&) = delete;
  FakeBluetoothAdapter& operator=(const FakeBluetoothAdapter&) = delete;

  device::BluetoothDevice* GetDevice(const std::string& address) override {
    for (const auto& it : mock_devices_) {
      if (it->GetAddress() == address)
        return it.get();
    }
    return nullptr;
  }

  void NotifyGattDiscoveryCompleteForService(
      device::BluetoothRemoteGattService* service) {
    device::BluetoothAdapter::NotifyGattDiscoveryComplete(service);
  }

 protected:
  ~FakeBluetoothAdapter() override = default;
};

class FakeBluetoothDevice
    : public testing::NiceMock<device::MockBluetoothDevice> {
 public:
  FakeBluetoothDevice(FakeBluetoothAdapter* adapter, const std::string& address)
      : testing::NiceMock<device::MockBluetoothDevice>(adapter,
                                                       /*bluetooth_class=*/0u,
                                                       /*name=*/"Test Device",
                                                       address,
                                                       /*paired=*/true,
                                                       /*connected=*/true),
        fake_adapter_(adapter) {}

  void CreateGattConnection(
      device::BluetoothDevice::GattConnectionCallback callback,
      absl::optional<device::BluetoothUUID> service_uuid =
          absl::nullopt) override {
    if (has_gatt_connection_error_) {
      std::move(callback).Run(
          std::make_unique<
              testing::NiceMock<device::MockBluetoothGattConnection>>(
              fake_adapter_, kTestBleDeviceAddress),
          /*error_code=*/device::BluetoothDevice::ConnectErrorCode::
              ERROR_FAILED);
    } else {
      std::move(callback).Run(
          std::make_unique<
              testing::NiceMock<device::MockBluetoothGattConnection>>(
              fake_adapter_, kTestBleDeviceAddress),
          /*error_code=*/absl::nullopt);
    }
  }

  void SetError(bool has_gatt_connection_error) {
    has_gatt_connection_error_ = has_gatt_connection_error;
  }

  // Move-only class
  FakeBluetoothDevice(const FakeBluetoothDevice&) = delete;
  FakeBluetoothDevice& operator=(const FakeBluetoothDevice&) = delete;

 protected:
  bool has_gatt_connection_error_ = false;
  FakeBluetoothAdapter* fake_adapter_;
};

std::unique_ptr<FakeBluetoothDevice> CreateTestBluetoothDevice(
    FakeBluetoothAdapter* adapter,
    device::BluetoothUUID uuid) {
  auto mock_device = std::make_unique<FakeBluetoothDevice>(
      /*adapter=*/adapter, kTestBleDeviceAddress);
  mock_device->AddUUID(uuid);
  mock_device->SetServiceDataForUUID(uuid, {1, 2, 3});
  return mock_device;
}

}  // namespace

namespace ash {
namespace quick_pair {

class FastPairGattServiceClientTest : public testing::Test {
 public:
  void SuccessfulGattConnectionSetUp() {
    adapter_ = base::MakeRefCounted<FakeBluetoothAdapter>();
    device_ = CreateTestBluetoothDevice(
        adapter_.get(), ash::quick_pair::kFastPairBluetoothUuid);
    adapter_->AddMockDevice(std::move(device_));
    EXPECT_CALL(callback_, Run).Times(0);
    gatt_service_client_ = std::make_unique<FastPairGattServiceClient>(
        adapter_->GetDevice(kTestBleDeviceAddress), adapter_.get(),
        callback_.Get());
  }

  void FailedGattConnectionSetUp() {
    adapter_ = base::MakeRefCounted<FakeBluetoothAdapter>();
    device_ = CreateTestBluetoothDevice(
        adapter_.get(), ash::quick_pair::kFastPairBluetoothUuid);
    device_->SetError(true);
    adapter_->AddMockDevice(std::move(device_));
    EXPECT_CALL(callback_, Run).Times(1);
    gatt_service_client_ = std::make_unique<FastPairGattServiceClient>(
        adapter_->GetDevice(kTestBleDeviceAddress), adapter_.get(),
        callback_.Get());
  }

  void NonFastPairServiceDataSetUp() {
    adapter_ = base::MakeRefCounted<FakeBluetoothAdapter>();
    device_ = CreateTestBluetoothDevice(adapter_.get(), kNonFastPairUuid);
    adapter_->AddMockDevice(std::move(device_));
    EXPECT_CALL(callback_, Run).Times(0);
    gatt_service_client_ = std::make_unique<FastPairGattServiceClient>(
        adapter_->GetDevice(kTestBleDeviceAddress), adapter_.get(),
        callback_.Get());
  }

  void NotifyGattDiscoveryCompleteForService() {
    auto gatt_service =
        std::make_unique<testing::NiceMock<device::MockBluetoothGattService>>(
            CreateTestBluetoothDevice(adapter_.get(),
                                      ash::quick_pair::kFastPairBluetoothUuid)
                .get(),
            kTestServiceId, ash::quick_pair::kFastPairBluetoothUuid,
            /*is_primary=*/true);
    gatt_service_ = std::move(gatt_service);
    ON_CALL(*(gatt_service_.get()), GetDevice)
        .WillByDefault(
            testing::Return(adapter_->GetDevice(kTestBleDeviceAddress)));
    adapter_->NotifyGattDiscoveryCompleteForService(gatt_service_.get());
  }

  bool ServiceIsSet() {
    if (!gatt_service_client_->gatt_service())
      return false;
    return gatt_service_client_->gatt_service() == gatt_service_.get();
  }

  base::MockCallback<base::OnceCallback<void(absl::optional<PairFailure>)>>
      callback_;

 private:
  scoped_refptr<FakeBluetoothAdapter> adapter_;
  std::unique_ptr<FakeBluetoothDevice> device_;
  std::unique_ptr<testing::NiceMock<device::MockBluetoothGattService>>
      gatt_service_;
  std::unique_ptr<FastPairGattServiceClient> gatt_service_client_;
  base::WeakPtrFactory<FastPairGattServiceClientTest> weak_ptr_factory_{this};
};

TEST_F(FastPairGattServiceClientTest, SuccessfulGattConnection) {
  SuccessfulGattConnectionSetUp();
  NotifyGattDiscoveryCompleteForService();
  EXPECT_TRUE(ServiceIsSet());
}

TEST_F(FastPairGattServiceClientTest, FailedGattConnection) {
  FailedGattConnectionSetUp();
  EXPECT_FALSE(ServiceIsSet());
}

TEST_F(FastPairGattServiceClientTest, IgnoreNonFastPairServices) {
  NonFastPairServiceDataSetUp();
  EXPECT_FALSE(ServiceIsSet());
}

}  // namespace quick_pair
}  // namespace ash
