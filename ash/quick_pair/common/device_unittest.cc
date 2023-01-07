// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/common/device.h"

#include <cstdint>

#include "ash/quick_pair/common/protocol.h"
#include "base/memory/scoped_refptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace quick_pair {

using AdditionalDataType = Device::AdditionalDataType;

class DeviceTest : public testing::Test {
 protected:
  scoped_refptr<Device> device_ =
      base::MakeRefCounted<Device>("metadata_id",
                                   "ble_address",
                                   Protocol::kFastPairInitial);
};

TEST_F(DeviceTest, GetAndSetAdditionalData) {
  // Test that it returns null before any sets.
  absl::optional<std::vector<uint8_t>> additional_data =
      device_->GetAdditionalData(AdditionalDataType::kAccountKey);
  EXPECT_FALSE(additional_data.has_value());

  // Test that it returns the set value.
  std::vector<uint8_t> data = {0};
  device_->SetAdditionalData(AdditionalDataType::kAccountKey, data);
  additional_data = device_->GetAdditionalData(AdditionalDataType::kAccountKey);
  EXPECT_TRUE(additional_data.has_value());
  EXPECT_EQ(additional_data.value(), data);

  // Test that overriding works.
  std::vector<uint8_t> more_data = {1};
  device_->SetAdditionalData(AdditionalDataType::kAccountKey, more_data);
  additional_data = device_->GetAdditionalData(AdditionalDataType::kAccountKey);
  EXPECT_TRUE(additional_data.has_value());
  EXPECT_EQ(additional_data.value(), more_data);
}

TEST_F(DeviceTest, SetClassicAddressForV1Devices) {
  // Test that overriding works.
  std::vector<uint8_t> more_data = {1};
  device_->SetAdditionalData(AdditionalDataType::kFastPairVersion, more_data);

  EXPECT_EQ(device_->classic_address(), device_->ble_address);
}
}  // namespace quick_pair
}  // namespace ash
