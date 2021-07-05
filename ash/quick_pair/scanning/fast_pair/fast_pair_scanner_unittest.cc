// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/scanning/fast_pair/fast_pair_scanner_impl.h"

#include <memory>

#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Below constants are used to construct MockBluetoothDevice for testing.
constexpr char kTestBleDeviceAddress[] = "11:12:13:14:15:16";

constexpr char kTestBleDeviceName[] = "Test Device Name";

std::unique_ptr<device::MockBluetoothDevice> CreateTestBluetoothDevice() {
  return std::make_unique<testing::NiceMock<device::MockBluetoothDevice>>(
      /*adapter=*/nullptr, /*bluetooth_class=*/0, kTestBleDeviceName,
      kTestBleDeviceAddress, /*paired=*/true, /*connected=*/true);
}

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

 private:
  ~FakeBluetoothAdapter() override = default;
};

}  // namespace

namespace ash {
namespace quick_pair {

class FastPairScannerObserver : public FastPairScanner::Observer {
 public:
  // FastPairScanner::Observer overrides
  void OnDeviceFound(device::BluetoothDevice* device) override {
    device_addreses_.push_back(device->GetAddress());
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

 private:
  std::vector<std::string> device_addreses_;
};

class FastPairScannerTest : public testing::Test {
 public:
  void SetUp() override {
    adapter_ = base::MakeRefCounted<FakeBluetoothAdapter>();
    device::BluetoothAdapterFactory::SetAdapterForTesting(adapter_);
    EXPECT_CALL(adapter(), AddObserver);
    scanner_ = std::make_unique<FastPairScannerImpl>();
    scanner_observer_ = std::make_unique<FastPairScannerObserver>();
    scanner().AddObserver(scanner_observer_.get());
  }

  void TearDown() override {
    EXPECT_CALL(adapter(), RemoveObserver(scanner_.get()));
    scanner().RemoveObserver(scanner_observer_.get());
  }

  FakeBluetoothAdapter& adapter() { return *(adapter_.get()); }

  FastPairScannerObserver& scanner_observer() {
    return *(scanner_observer_.get());
  }

  FastPairScanner& scanner() { return *(scanner_.get()); }

 protected:
  scoped_refptr<FakeBluetoothAdapter> adapter_;
  std::unique_ptr<FastPairScannerImpl> scanner_;
  std::unique_ptr<FastPairScannerObserver> scanner_observer_;
};

TEST_F(FastPairScannerTest, DeviceAddedNotifiesObservers) {
  adapter().AddDevice();
  EXPECT_TRUE(scanner_observer().DoesDeviceListContainTestDevice());
}

TEST_F(FastPairScannerTest, DeviceRemovedNotifiesObservers) {
  adapter().AddDevice();
  EXPECT_TRUE(scanner_observer().DoesDeviceListContainTestDevice());
  adapter().RemoveDevice();
  EXPECT_FALSE(scanner_observer().DoesDeviceListContainTestDevice());
}

TEST_F(FastPairScannerTest, RemovedObserverReceivesNoNewEvents) {
  scanner().RemoveObserver(&scanner_observer());
  adapter().AddDevice();
  EXPECT_FALSE(scanner_observer().DoesDeviceListContainTestDevice());
}

}  // namespace quick_pair
}  // namespace ash
