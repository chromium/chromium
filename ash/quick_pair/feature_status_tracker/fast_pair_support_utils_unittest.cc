// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/feature_status_tracker/fast_pair_support_utils.h"

#include "ash/constants/ash_features.h"
#include "ash/quick_pair/feature_status_tracker/fake_bluetooth_adapter.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace quick_pair {

class FastPairSupportUtilsTest : public testing::Test {
 public:
  void SetUp() override {
    adapter_ = base::MakeRefCounted<FakeBluetoothAdapter>();
  }

 protected:
  scoped_refptr<FakeBluetoothAdapter> adapter_;
};

TEST_F(FastPairSupportUtilsTest, HasHardwareSupportForHardwareState) {
  EXPECT_TRUE(HasHardwareSupport(adapter_));

  adapter_->SetHardwareOffloadingStatus(
      device::BluetoothAdapter::LowEnergyScanSessionHardwareOffloadingStatus::
          kNotSupported);
  EXPECT_FALSE(HasHardwareSupport(adapter_));

  adapter_->SetHardwareOffloadingStatus(
      device::BluetoothAdapter::LowEnergyScanSessionHardwareOffloadingStatus::
          kSupported);
  EXPECT_TRUE(HasHardwareSupport(adapter_));
}

TEST_F(FastPairSupportUtilsTest, HasHardwareSupportForFlagState) {
  EXPECT_TRUE(HasHardwareSupport(adapter_));

  adapter_->SetHardwareOffloadingStatus(
      device::BluetoothAdapter::LowEnergyScanSessionHardwareOffloadingStatus::
          kNotSupported);
  EXPECT_FALSE(HasHardwareSupport(adapter_));

  base::test::ScopedFeatureList feature_list{
      features::kFastPairSoftwareScanning};
  EXPECT_TRUE(HasHardwareSupport(adapter_));
}

TEST_F(FastPairSupportUtilsTest, HasHardwareSupportFalseForAdapterState) {
  EXPECT_TRUE(HasHardwareSupport(adapter_));

  adapter_->SetBluetoothIsPresent(false);
  EXPECT_FALSE(HasHardwareSupport(adapter_));

  scoped_refptr<FakeBluetoothAdapter> null_adapter;
  EXPECT_FALSE(HasHardwareSupport(null_adapter));
}

}  // namespace quick_pair
}  // namespace ash
