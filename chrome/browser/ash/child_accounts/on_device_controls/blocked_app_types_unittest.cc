// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/on_device_controls/blocked_app_types.h"

#include <optional>

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::on_device_controls {

using BlockedAppTypesTest = testing::Test;

TEST_F(BlockedAppTypesTest, AppDetailsWithBlockTimestamp) {
  const base::Time timestamp = base::Time::Now() + base::Hours(12);
  BlockedAppDetails details = BlockedAppDetails(timestamp);

  EXPECT_EQ(timestamp, details.block_timestamp());
  EXPECT_EQ(std::nullopt, details.uninstall_timestamp());
  EXPECT_TRUE(details.IsInstalled());
}

TEST_F(BlockedAppTypesTest, UninstalledAppDetails) {
  const base::Time block_time = base::Time::Now() + base::Hours(12);
  const base::Time uninstall_time = base::Time::Now() + base::Days(30);

  BlockedAppDetails details = BlockedAppDetails(block_time, uninstall_time);

  EXPECT_EQ(block_time, details.block_timestamp());
  EXPECT_FALSE(details.IsInstalled());
  EXPECT_EQ(uninstall_time, *details.uninstall_timestamp());
}

TEST_F(BlockedAppTypesTest, MarkUninstalled) {
  const base::Time timestamp;
  BlockedAppDetails details = BlockedAppDetails(timestamp, timestamp);
  EXPECT_FALSE(details.IsInstalled());

  details.MarkInstalled();
  EXPECT_TRUE(details.IsInstalled());
}

TEST_F(BlockedAppTypesTest, SetBlockTimestamp) {
  const base::Time timestamp = base::Time::Now();
  BlockedAppDetails details = BlockedAppDetails(timestamp);
  EXPECT_EQ(timestamp, details.block_timestamp());

  base::Time new_block_time = base::Time::Now() + base::Minutes(30);
  details.SetBlockTimestamp(new_block_time);
  EXPECT_EQ(new_block_time, details.block_timestamp());
}

TEST_F(BlockedAppTypesTest, SetUninstallTimestamp) {
  BlockedAppDetails details = BlockedAppDetails();
  EXPECT_EQ(std::nullopt, details.uninstall_timestamp());

  base::Time uninstall_time = base::Time::Now() + base::Minutes(30);
  details.SetUninstallTimestamp(uninstall_time);
  EXPECT_EQ(uninstall_time, details.uninstall_timestamp());
}

}  // namespace ash::on_device_controls
