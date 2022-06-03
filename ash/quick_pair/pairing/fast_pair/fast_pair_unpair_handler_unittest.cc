// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/pairing/fast_pair/fast_pair_unpair_handler.h"

#include <memory>

#include "ash/quick_pair/repository/mock_fast_pair_repository.h"
#include "base/memory/scoped_refptr.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace quick_pair {

class FakeBluetoothAdapter : public device::MockBluetoothAdapter {
 public:
  void NotifyPairedChanged(device::BluetoothDevice* device,
                           bool new_pair_state) {
    device::BluetoothAdapter::NotifyDevicePairedChanged(device, new_pair_state);
  }

 private:
  ~FakeBluetoothAdapter() override = default;
};

class FastPairUnpairHandlerTest : public testing::Test {
 public:
  void SetUp() override {
    adapter_ = base::MakeRefCounted<FakeBluetoothAdapter>();
    device::BluetoothAdapterFactory::SetAdapterForTesting(adapter_);

    device_ = std::make_unique<device::MockBluetoothDevice>(
        adapter_.get(), 0, "test_name", "test_address", /*paired=*/false,
        /*connected=*/false);

    unpair_handler_ = std::make_unique<FastPairUnpairHandler>(adapter_);
    mock_repository_ = std::make_unique<MockFastPairRepository>();
  }

 protected:
  void NotifyPairChanged(bool new_pair_state) {
    device_->SetPaired(!new_pair_state);
    adapter_->NotifyPairedChanged(device_.get(), new_pair_state);
  }

  scoped_refptr<FakeBluetoothAdapter> adapter_;
  std::unique_ptr<device::MockBluetoothDevice> device_;
  std::unique_ptr<FastPairUnpairHandler> unpair_handler_;
  std::unique_ptr<MockFastPairRepository> mock_repository_;
};

TEST_F(FastPairUnpairHandlerTest, DoesntDeleteIfDevicePaired) {
  EXPECT_CALL(*(mock_repository_.get()), DeleteAssociatedDevice).Times(0);
  NotifyPairChanged(/*new_pair_state=*/true);
}

TEST_F(FastPairUnpairHandlerTest, DeletesExpectedDevice) {
  EXPECT_CALL(*(mock_repository_.get()), DeleteAssociatedDevice(device_.get()))
      .Times(1);
  NotifyPairChanged(/*new_pair_state=*/false);
}

}  // namespace quick_pair
}  // namespace ash
