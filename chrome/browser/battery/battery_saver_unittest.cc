// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/battery/battery_saver.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace {

class BatterySaverUnitTest : public ::testing::Test {
 public:
  BatterySaverUnitTest() = default;
  ~BatterySaverUnitTest() override = default;

  void TearDown() override { battery::ResetIsBatterySaverEnabledForTesting(); }
};

TEST_F(BatterySaverUnitTest, BatterSaverModeCheck) {
  battery::OverrideIsBatterySaverEnabledForTesting(true);
  EXPECT_TRUE(battery::IsBatterySaverEnabled());
  battery::OverrideIsBatterySaverEnabledForTesting(false);
  EXPECT_FALSE(battery::IsBatterySaverEnabled());
}

}  // namespace
