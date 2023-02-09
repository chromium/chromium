// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/pairing/fast_pair/fast_pair_unpair_handler.h"

#include <memory>

#include "ash/quick_pair/common/fake_bluetooth_adapter.h"
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
  void SetPaired(bool paired) { device_->SetPaired(paired); }

  scoped_refptr<FakeBluetoothAdapter> adapter_;
  std::unique_ptr<device::MockBluetoothDevice> device_;
  std::unique_ptr<FastPairUnpairHandler> unpair_handler_;
  std::unique_ptr<MockFastPairRepository> mock_repository_;
};

}  // namespace quick_pair
}  // namespace ash
