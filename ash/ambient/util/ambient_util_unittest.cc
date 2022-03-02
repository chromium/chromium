// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/util/ambient_util.h"

#include <string>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace ambient {
namespace util {
namespace {
using ::testing::Eq;
}  // namespace

TEST(AmbientUtilTest, ParseDynamicLottieAssetId) {
  std::string position;
  int idx = 0;
  EXPECT_FALSE(ParseDynamicLottieAssetId("StaticAsset", position, idx));
  EXPECT_FALSE(ParseDynamicLottieAssetId("_CrOS_UnknownAsset", position, idx));
  ASSERT_TRUE(
      ParseDynamicLottieAssetId("_CrOS_Photo_PositionA_1", position, idx));
  EXPECT_THAT(position, Eq("A"));
  EXPECT_THAT(idx, Eq(1));
  ASSERT_TRUE(
      ParseDynamicLottieAssetId("_CrOS_Photo_PositionB_1", position, idx));
  EXPECT_THAT(position, Eq("B"));
  EXPECT_THAT(idx, Eq(1));
  ASSERT_TRUE(ParseDynamicLottieAssetId("_CrOS_Photo_PositionTopLeft_1",
                                        position, idx));
  EXPECT_THAT(position, Eq("TopLeft"));
  EXPECT_THAT(idx, Eq(1));
  ASSERT_TRUE(ParseDynamicLottieAssetId("_CrOS_Photo_PositionTopRight_2",
                                        position, idx));
  EXPECT_THAT(position, Eq("TopRight"));
  EXPECT_THAT(idx, Eq(2));
  ASSERT_TRUE(
      ParseDynamicLottieAssetId("_CrOS_Photo_PositionA_1.png", position, idx));
  EXPECT_THAT(position, Eq("A"));
  EXPECT_THAT(idx, Eq(1));
}

}  // namespace util
}  // namespace ambient
}  // namespace ash
