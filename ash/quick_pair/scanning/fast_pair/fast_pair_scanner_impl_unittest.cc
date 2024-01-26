// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/scanning/fast_pair/fast_pair_scanner_impl.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/quick_pair/common/constants.h"
#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/fake_bluetooth_adapter.h"
#include "ash/quick_pair/common/protocol.h"
#include "ash/quick_pair/fast_pair_handshake/fake_fast_pair_handshake.h"
#include "ash/quick_pair/fast_pair_handshake/fake_fast_pair_handshake_lookup.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_data_encryptor.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_gatt_service_client.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_handshake.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_handshake_lookup.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_low_energy_scan_filter.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "device/bluetooth/test/mock_bluetooth_low_energy_scan_session.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Below constants are used to construct MockBluetoothDevice for testing.
constexpr char kTestBleDeviceAddress1[] = "11:12:13:14:15:16";
constexpr char kTestBleDeviceName[] = "Test Device Name";

std::unique_ptr<device::MockBluetoothDevice> CreateTestBluetoothDevice(
    const std::string& address) {
  auto mock_device =
      std::make_unique<testing::NiceMock<device::MockBluetoothDevice>>(
          /*adapter=*/nullptr, /*bluetooth_class=*/0, kTestBleDeviceName,
          address, /*paired=*/true, /*connected=*/true);
  mock_device->AddUUID(ash::quick_pair::kFastPairBluetoothUuid);
  mock_device->SetServiceDataForUUID(ash::quick_pair::kFastPairBluetoothUuid,
                                     {1, 2, 3});
  return mock_device;
}

class FastPairScannerObserver
    : public ash::quick_pair::FastPairScanner::Observer {
 public:
  // FastPairScanner::Observer overrides
  void OnDeviceFound(device::BluetoothDevice* device) override {
    device_addreses_.push_back(device->GetAddress());
    on_device_found_count_++;
  }

  void OnDeviceLost(device::BluetoothDevice* device) override {
    device_addreses_.erase(
        base::ranges::find(device_addreses_, device->GetAddress()));
  }

  bool DoesDeviceListContainTestDevice(const std::string& address) {
    return base::Contains(device_addreses_, address);
  }

  int on_device_found_count() { return on_device_found_count_; }

 private:
  std::vector<std::string> device_addreses_;
  int on_device_found_count_ = 0;
};

}  // namespace

namespace ash {
namespace quick_pair {

class FastPairScannerImplTest : public testing::Test {
 public:
  void SetUp() override {
    adapter_ = base::MakeRefCounted<FakeBluetoothAdapter>();
    ON_CALL(*adapter_, StartLowEnergyScanSession(testing::_, testing::_))
        .WillByDefault(
            Invoke(this, &FastPairScannerImplTest::StartLowEnergyScanSession));
    device::BluetoothAdapterFactory::SetAdapterForTesting(adapter_);
    FastPairHandshakeLookup::UseFakeInstance();

    scanner_ = base::MakeRefCounted<FastPairScannerImpl>();
    scanner_observer_ = std::make_unique<FastPairScannerObserver>();
    scanner().AddObserver(scanner_observer_.get());

    task_environment_.RunUntilIdle();
  }

  void TearDown() override {
    scanner().RemoveObserver(scanner_observer_.get());
    FastPairHandshakeLookup::GetInstance()->Clear();
  }

  std::unique_ptr<device::BluetoothLowEnergyScanSession>
  StartLowEnergyScanSession(
      std::unique_ptr<device::BluetoothLowEnergyScanFilter> filter,
      base::WeakPtr<device::BluetoothLowEnergyScanSession::Delegate> delegate) {
    auto mock_scan_session =
        std::make_unique<device::MockBluetoothLowEnergyScanSession>(
            base::BindOnce(&FastPairScannerImplTest::OnScanSessionDestroyed,
                           weak_ptr_factory_.GetWeakPtr()));
    mock_scan_session_ = mock_scan_session.get();
    delegate_ = delegate;
    return mock_scan_session;
  }

  void OnScanSessionDestroyed() {
    delegate_ = nullptr;
    mock_scan_session_ = nullptr;
  }

  FakeBluetoothAdapter& adapter() { return *(adapter_.get()); }

  device::MockBluetoothLowEnergyScanSession* scan_session() {
    return mock_scan_session_;
  }

  FastPairScannerObserver& scanner_observer() {
    return *(scanner_observer_.get());
  }

  FastPairScanner& scanner() { return *(scanner_.get()); }

  void TriggerOnDeviceFound(const std::string& address) {
    if (!delegate_)
      return;

    delegate_->OnDeviceFound(mock_scan_session_,
                             CreateTestBluetoothDevice(address).get());
  }

  void TriggerOnDeviceLost(const std::string& address) {
    if (!delegate_)
      return;

    delegate_->OnDeviceLost(mock_scan_session_,
                            CreateTestBluetoothDevice(address).get());
  }

  void AddConnectedHandshake(const std::string& address) {
    FastPairHandshakeLookup::GetInstance()->Create(
        adapter_,
        base::MakeRefCounted<Device>("", address, Protocol::kFastPairInitial),
        base::DoNothing());
  }

  std::unique_ptr<FastPairHandshake> CreateConnectedHandshake(
      scoped_refptr<Device> device,
      FastPairHandshakeLookup::OnCompleteCallback callback) {
    return std::make_unique<FakeFastPairHandshake>(adapter_, std::move(device),
                                                   std::move(callback));
  }

  void SetUpFactoryScanner() {
    scanner_.reset();
    scanner_ = FastPairScannerImpl::Factory::Create();
    scanner_->AddObserver(scanner_observer_.get());
    task_environment_.RunUntilIdle();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  scoped_refptr<FakeBluetoothAdapter> adapter_;
  scoped_refptr<FastPairScanner> scanner_;
  raw_ptr<device::MockBluetoothLowEnergyScanSession> mock_scan_session_ =
      nullptr;
  std::unique_ptr<FastPairScannerObserver> scanner_observer_;
  base::WeakPtr<device::BluetoothLowEnergyScanSession::Delegate> delegate_;
  base::WeakPtrFactory<FastPairScannerImplTest> weak_ptr_factory_{this};
};

TEST_F(FastPairScannerImplTest, FactoryCreate) {
  SetUpFactoryScanner();
  TriggerOnDeviceFound(kTestBleDeviceAddress1);
  EXPECT_TRUE(scanner_observer().DoesDeviceListContainTestDevice(
      kTestBleDeviceAddress1));
}

TEST_F(FastPairScannerImplTest, SessionStartedSuccessfully) {
  delegate_->OnSessionStarted(mock_scan_session_,
                              /*error_code=*/std::nullopt);
  TriggerOnDeviceFound(kTestBleDeviceAddress1);
  EXPECT_TRUE(scanner_observer().DoesDeviceListContainTestDevice(
      kTestBleDeviceAddress1));
}

TEST_F(FastPairScannerImplTest, SessionStartedFailure) {
  delegate_->OnSessionStarted(
      mock_scan_session_,
      device::BluetoothLowEnergyScanSession::ErrorCode::kFailed);
  delegate_ = nullptr;
  TriggerOnDeviceFound(kTestBleDeviceAddress1);
  EXPECT_FALSE(scanner_observer().DoesDeviceListContainTestDevice(
      kTestBleDeviceAddress1));
}

TEST_F(FastPairScannerImplTest, SessionInvalidated) {
  delegate_->OnSessionInvalidated(mock_scan_session_);
  TriggerOnDeviceFound(kTestBleDeviceAddress1);
  EXPECT_FALSE(scanner_observer().DoesDeviceListContainTestDevice(
      kTestBleDeviceAddress1));
}

TEST_F(FastPairScannerImplTest, DeviceAddedNotifiesObservers) {
  TriggerOnDeviceFound(kTestBleDeviceAddress1);
  EXPECT_TRUE(scanner_observer().DoesDeviceListContainTestDevice(
      kTestBleDeviceAddress1));
}

TEST_F(FastPairScannerImplTest, DeviceRemoved) {
  TriggerOnDeviceFound(kTestBleDeviceAddress1);
  EXPECT_EQ(scanner_observer().on_device_found_count(), 1);

  auto mock_device = CreateTestBluetoothDevice(kTestBleDeviceAddress1);
  auto* mock_device_ptr = mock_device.get();
  mock_device->SetServiceDataForUUID(ash::quick_pair::kFastPairBluetoothUuid,
                                     {4, 5, 6});

  adapter().NotifyDeviceRemoved(mock_device_ptr);
  adapter().NotifyDeviceChanged(mock_device_ptr);
  EXPECT_EQ(scanner_observer().on_device_found_count(), 1);
}

TEST_F(FastPairScannerImplTest, DevicePairedChanged) {
  TriggerOnDeviceFound(kTestBleDeviceAddress1);
  EXPECT_EQ(scanner_observer().on_device_found_count(), 1);

  auto mock_device = CreateTestBluetoothDevice(kTestBleDeviceAddress1);
  auto* mock_device_ptr = mock_device.get();
  mock_device->SetServiceDataForUUID(ash::quick_pair::kFastPairBluetoothUuid,
                                     {4, 5, 6});

  adapter().NotifyDevicePairedChanged(mock_device_ptr, false);
  adapter().NotifyDeviceChanged(mock_device_ptr);
  EXPECT_EQ(scanner_observer().on_device_found_count(), 1);
}

TEST_F(FastPairScannerImplTest, DeviceAddedAlreadyHasHandshake) {
  AddConnectedHandshake(kTestBleDeviceAddress1);
  TriggerOnDeviceFound(kTestBleDeviceAddress1);
  EXPECT_FALSE(scanner_observer().DoesDeviceListContainTestDevice(
      kTestBleDeviceAddress1));
}

TEST_F(FastPairScannerImplTest, DeviceLostNotifiesObservers) {
  TriggerOnDeviceFound(kTestBleDeviceAddress1);
  EXPECT_TRUE(scanner_observer().DoesDeviceListContainTestDevice(
      kTestBleDeviceAddress1));
  TriggerOnDeviceLost(kTestBleDeviceAddress1);
  EXPECT_FALSE(scanner_observer().DoesDeviceListContainTestDevice(
      kTestBleDeviceAddress1));
}

TEST_F(FastPairScannerImplTest, DeviceChangedNewServiceDataLength) {
  TriggerOnDeviceFound(kTestBleDeviceAddress1);
  EXPECT_EQ(scanner_observer().on_device_found_count(), 1);

  auto mock_device = CreateTestBluetoothDevice(kTestBleDeviceAddress1);
  auto* mock_device_ptr = mock_device.get();

  // The length of the service data changes between Initial/Subsequent pairing
  // which is used to detect if we should trigger OnDeviceFound or not.
  mock_device->SetServiceDataForUUID(ash::quick_pair::kFastPairBluetoothUuid,
                                     {4, 5, 6, 7});
  adapter().NotifyDeviceChanged(mock_device_ptr);
  EXPECT_EQ(scanner_observer().on_device_found_count(), 2);
}

TEST_F(FastPairScannerImplTest, DeviceChangedSameServiceDataLength) {
  TriggerOnDeviceFound(kTestBleDeviceAddress1);
  EXPECT_EQ(scanner_observer().on_device_found_count(), 1);

  auto mock_device = CreateTestBluetoothDevice(kTestBleDeviceAddress1);
  auto* mock_device_ptr = mock_device.get();
  mock_device->SetServiceDataForUUID(ash::quick_pair::kFastPairBluetoothUuid,
                                     {4, 5, 6});
  // This simulates a change of service data within one of the ongoing pairing
  // scenarios, in which case we do not notify observers.
  adapter().NotifyDeviceChanged(mock_device_ptr);
  EXPECT_EQ(scanner_observer().on_device_found_count(), 1);
}

TEST_F(FastPairScannerImplTest, DeviceAddedNoServiceData) {
  auto mock_device =
      std::make_unique<testing::NiceMock<device::MockBluetoothDevice>>(
          /*adapter=*/nullptr, /*bluetooth_class=*/0, kTestBleDeviceName,
          kTestBleDeviceAddress1, /*paired=*/true, /*connected=*/true);
  delegate_->OnDeviceFound(mock_scan_session_, mock_device.get());
  EXPECT_EQ(scanner_observer().on_device_found_count(), 0);
}

TEST_F(FastPairScannerImplTest, DeviceChangedNoServiceData) {
  auto mock_device =
      std::make_unique<testing::NiceMock<device::MockBluetoothDevice>>(
          /*adapter=*/nullptr, /*bluetooth_class=*/0, kTestBleDeviceName,
          kTestBleDeviceAddress1, /*paired=*/true, /*connected=*/true);
  delegate_->OnDeviceFound(mock_scan_session_, mock_device.get());
  EXPECT_EQ(scanner_observer().on_device_found_count(), 0);
  auto* mock_device_ptr = mock_device.get();
  mock_device->SetServiceDataForUUID(ash::quick_pair::kFastPairBluetoothUuid,
                                     {4, 5, 6});
  adapter().NotifyDeviceChanged(mock_device_ptr);
  EXPECT_EQ(scanner_observer().on_device_found_count(), 0);
}

TEST_F(FastPairScannerImplTest, IgnoresEventDuringActiveHandshake) {
  TriggerOnDeviceFound(kTestBleDeviceAddress1);
  EXPECT_TRUE(scanner_observer().DoesDeviceListContainTestDevice(
      kTestBleDeviceAddress1));
  AddConnectedHandshake(kTestBleDeviceAddress1);
  TriggerOnDeviceLost(kTestBleDeviceAddress1);
  EXPECT_FALSE(scanner_observer().DoesDeviceListContainTestDevice(
      kTestBleDeviceAddress1));
  TriggerOnDeviceFound(kTestBleDeviceAddress1);
  EXPECT_TRUE(scanner_observer().DoesDeviceListContainTestDevice(
      kTestBleDeviceAddress1));
  EXPECT_EQ(scanner_observer().on_device_found_count(), 2);
}

TEST_F(FastPairScannerImplTest, NoNotifyForPairedDevice) {
  auto paired_device = base::MakeRefCounted<Device>(
      "test_metadata_id", kTestBleDeviceAddress1, Protocol::kFastPairInitial);
  paired_device->set_classic_address(kTestBleDeviceAddress1);

  auto mock_device =
      std::make_unique<testing::NiceMock<device::MockBluetoothDevice>>(
          /*adapter=*/nullptr, /*bluetooth_class=*/0, kTestBleDeviceName,
          kTestBleDeviceAddress1, /*paired=*/true, /*connected=*/true);

  adapter_->AddMockDevice(std::move(mock_device));

  scanner_->OnDevicePaired(paired_device);
  TriggerOnDeviceFound(kTestBleDeviceAddress1);

  EXPECT_EQ(scanner_observer().on_device_found_count(), 0);
}

}  // namespace quick_pair
}  // namespace ash
