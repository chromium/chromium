// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/feature_status_tracker/fast_pair_support_utils.h"

#include "ash/constants/ash_features.h"
#include "ash/quick_pair/common/fake_bluetooth_adapter.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace quick_pair {

class FastPairSupportUtilsTest : public testing::Test {
 public:
  void SetUp() override {
    adapter_ = base::MakeRefCounted<FakeBluetoothAdapter>();
    adapter_->SetBluetoothIsPowered(true);
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

TEST_F(FastPairSupportUtilsTest, DisableAllowCrossDeviceFeatureSuite) {
  EXPECT_TRUE(HasHardwareSupport(adapter_));

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kAllowCrossDeviceFeatureSuite);
  EXPECT_FALSE(HasHardwareSupport(adapter_));
}

TEST_F(FastPairSupportUtilsTest, HasHardwareSupportFalseForAdapterPresence) {
  EXPECT_TRUE(HasHardwareSupport(adapter_));

  adapter_->SetBluetoothIsPresent(false);
  EXPECT_FALSE(HasHardwareSupport(adapter_));

  scoped_refptr<FakeBluetoothAdapter> null_adapter;
  EXPECT_FALSE(HasHardwareSupport(null_adapter));
}

TEST_F(FastPairSupportUtilsTest, HasHardwareSupportFalseForAdapterPowerState) {
  EXPECT_TRUE(HasHardwareSupport(adapter_));

  adapter_->SetBluetoothIsPowered(false);
  EXPECT_FALSE(HasHardwareSupport(adapter_));
}

}  // namespace quick_pair
}  // namespace ash
