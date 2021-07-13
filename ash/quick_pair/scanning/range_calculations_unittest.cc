// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/scanning/range_calculations.h"

#include <cmath>

#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace quick_pair {
namespace range_calculations {

class RangeCalculationsTest : public testing::Test {};

TEST_F(RangeCalculationsTest,
       RssiFromTargetDistance_ReturnsTxPowerForZeroDistance) {
  int8_t tx_power = 1;
  EXPECT_EQ(RssiFromTargetDistance(0, tx_power), tx_power);
}

TEST_F(RangeCalculationsTest,
       RssiFromTargetDistance_ReturnsTxPowerForNegativeDistance) {
  int8_t tx_power = 1;
  EXPECT_EQ(RssiFromTargetDistance(-10, tx_power), tx_power);
}

TEST_F(RangeCalculationsTest, RssiFromTargetDistance) {
  EXPECT_EQ(RssiFromTargetDistance(2, -40), -88);
}

TEST_F(RangeCalculationsTest, DistanceFromRssiAndTxPower) {
  EXPECT_EQ(floor(DistanceFromRssiAndTxPower(-100, -24)), 56);
}

}  // namespace range_calculations
}  // namespace quick_pair
}  // namespace ash
