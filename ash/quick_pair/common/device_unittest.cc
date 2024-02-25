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

class DeviceTest : public testing::Test {
 protected:
  scoped_refptr<Device> device_ =
      base::MakeRefCounted<Device>("metadata_id",
                                   "ble_address",
                                   Protocol::kFastPairInitial);
};

TEST_F(DeviceTest, GetAndSetAccountKey) {
  std::optional<std::vector<uint8_t>> accountKey;
  std::vector<uint8_t> data = {0};
  device_->set_account_key(data);
  accountKey = device_->account_key();
  EXPECT_EQ(accountKey, data);

  // Test that overriding works.
  std::vector<uint8_t> more_data = {1};
  device_->set_account_key(more_data);
  accountKey = device_->account_key();
  EXPECT_EQ(accountKey, more_data);
}

TEST_F(DeviceTest, GetAndSetName) {
  // Test that name returns null before any sets.
  std::optional<std::string> name = device_->display_name();
  EXPECT_FALSE(name.has_value());

  // Test that name returns the set value.
  std::string test_name = "test_name";
  device_->set_display_name(test_name);
  name = device_->display_name();
  EXPECT_TRUE(name.has_value());
  EXPECT_EQ(name.value(), test_name);

  // Test that overriding works.
  std::string new_test_name = "new_test_name";
  device_->set_display_name(new_test_name);
  name = device_->display_name();
  EXPECT_TRUE(name.has_value());
  EXPECT_EQ(name.value(), new_test_name);
}
}  // namespace quick_pair
}  // namespace ash
