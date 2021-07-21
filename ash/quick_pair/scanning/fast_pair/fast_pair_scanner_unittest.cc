// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/scanning/fast_pair/fast_pair_scanner_impl.h"

#include <memory>

#include "ash/quick_pair/common/constants.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_low_energy_scan_filter.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "device/bluetooth/test/mock_bluetooth_low_energy_scan_session.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Below constants are used to construct MockBluetoothDevice for testing.
constexpr char kTestBleDeviceAddress[] = "11:12:13:14:15:16";

constexpr char kTestBleDeviceName[] = "Test Device Name";

std::unique_ptr<device::MockBluetoothDevice> CreateTestBluetoothDevice() {
  auto mock_device =
      std::make_unique<testing::NiceMock<device::MockBluetoothDevice>>(
          /*adapter=*/nullptr, /*bluetooth_class=*/0, kTestBleDeviceName,
          kTestBleDeviceAddress, /*paired=*/true, /*connected=*/true);
  mock_device->AddUUID(ash::quick_pair::kFastPairBluetoothUuid);
  mock_device->SetServiceDataForUUID(ash::quick_pair::kFastPairBluetoothUuid,
                                     {1, 2, 3});
  return mock_device;
}

}  // namespace

namespace ash {
namespace quick_pair {

class FakeBluetoothAdapter
    : public testing::NiceMock<device::MockBluetoothAdapter> {
 public:
  void AddDevice() {
    auto mock_device = CreateTestBluetoothDevice();
    auto* mock_device_ptr = mock_device.get();
    AddMockDevice(std::move(mock_device));

    for (auto& observer : GetObservers())
      observer.DeviceAdded(this, mock_device_ptr);
  }

  void RemoveDevice() {
    auto mock_device = RemoveMockDevice(kTestBleDeviceAddress);
    for (auto& observer : GetObservers())
      observer.DeviceRemoved(this, mock_device.get());
  }

  void ChangeDevice() {
    auto mock_device = CreateTestBluetoothDevice();
    auto* mock_device_ptr = mock_device.get();
    mock_device->SetServiceDataForUUID(ash::quick_pair::kFastPairBluetoothUuid,
                                       {4, 5, 6});

    for (auto& observer : GetObservers())
      observer.DeviceChanged(this, mock_device_ptr);
  }

 private:
  ~FakeBluetoothAdapter() override = default;
};

class FastPairScannerObserver : public FastPairScanner::Observer {
 public:
  // FastPairScanner::Observer overrides
  void OnDeviceFound(device::BluetoothDevice* device) override {
    device_addreses_.push_back(device->GetAddress());
    on_device_found_count_++;
  }

  void OnDeviceLost(device::BluetoothDevice* device) override {
    device_addreses_.erase(std::find(device_addreses_.begin(),
                                     device_addreses_.end(),
                                     device->GetAddress()));
  }

  bool DoesDeviceListContainTestDevice() {
    return std::find(device_addreses_.begin(), device_addreses_.end(),
                     kTestBleDeviceAddress) != device_addreses_.end();
  }

  int on_device_found_count() { return on_device_found_count_; }

 private:
  std::vector<std::string> device_addreses_;
  int on_device_found_count_ = 0;
};

class FastPairScannerTest : public testing::Test {
 public:
  void SetUp() override {
    adapter_ = base::MakeRefCounted<FakeBluetoothAdapter>();
    ON_CALL(*adapter_, StartLowEnergyScanSession(testing::_, testing::_))
        .WillByDefault(
            Invoke(this, &FastPairScannerTest::StartLowEnergyScanSession));
    device::BluetoothAdapterFactory::SetAdapterForTesting(adapter_);
    EXPECT_CALL(adapter(), AddObserver);
    scanner_ = base::MakeRefCounted<FastPairScannerImpl>();
    scanner_observer_ = std::make_unique<FastPairScannerObserver>();
    scanner().AddObserver(scanner_observer_.get());
  }

  void TearDown() override {
    EXPECT_CALL(adapter(), RemoveObserver(scanner_.get()));
    scanner().RemoveObserver(scanner_observer_.get());
  }

  std::unique_ptr<device::BluetoothLowEnergyScanSession>
  StartLowEnergyScanSession(
      std::unique_ptr<device::BluetoothLowEnergyScanFilter> filter,
      base::WeakPtr<device::BluetoothLowEnergyScanSession::Delegate> delegate) {
    auto mock_scan_session =
        std::make_unique<device::MockBluetoothLowEnergyScanSession>(
            base::BindOnce(&FastPairScannerTest::OnScanSessionDestroyed,
                           weak_ptr_factory_.GetWeakPtr()));
    mock_scan_session_ = mock_scan_session.get();
    delegate_ = delegate;
    return mock_scan_session;
  }

  void OnScanSessionDestroyed() { mock_scan_session_ = nullptr; }

  FakeBluetoothAdapter& adapter() { return *(adapter_.get()); }

  device::MockBluetoothLowEnergyScanSession* scan_session() {
    return mock_scan_session_;
  }

  FastPairScannerObserver& scanner_observer() {
    return *(scanner_observer_.get());
  }

  FastPairScanner& scanner() { return *(scanner_.get()); }

  void TriggerOnDeviceFound() {
    delegate_->OnDeviceFound(mock_scan_session_,
                             CreateTestBluetoothDevice().get());
  }

  void TriggerOnDeviceLost() {
    delegate_->OnDeviceLost(mock_scan_session_,
                            CreateTestBluetoothDevice().get());
  }

 protected:
  scoped_refptr<FakeBluetoothAdapter> adapter_;
  scoped_refptr<FastPairScannerImpl> scanner_;
  device::MockBluetoothLowEnergyScanSession* mock_scan_session_ = nullptr;
  std::unique_ptr<FastPairScannerObserver> scanner_observer_;
  base::WeakPtr<device::BluetoothLowEnergyScanSession::Delegate> delegate_;
  base::WeakPtrFactory<FastPairScannerTest> weak_ptr_factory_{this};
};

TEST_F(FastPairScannerTest, DeviceAddedNotifiesObservers) {
  TriggerOnDeviceFound();
  EXPECT_TRUE(scanner_observer().DoesDeviceListContainTestDevice());
}

TEST_F(FastPairScannerTest, DeviceLostNotifiesObservers) {
  TriggerOnDeviceFound();
  EXPECT_TRUE(scanner_observer().DoesDeviceListContainTestDevice());
  TriggerOnDeviceLost();
  EXPECT_FALSE(scanner_observer().DoesDeviceListContainTestDevice());
}

TEST_F(FastPairScannerTest, DeviceChangedNotifiesObservors) {
  TriggerOnDeviceFound();
  EXPECT_EQ(scanner_observer().on_device_found_count(), 1);
  adapter().ChangeDevice();
  EXPECT_EQ(scanner_observer().on_device_found_count(), 2);
}

}  // namespace quick_pair
}  // namespace ash
