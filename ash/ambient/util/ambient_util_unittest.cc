// Copyright 2022 The Chromium Authors
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
using ::testing::Lt;
using ::testing::Not;
}  // namespace

TEST(AmbientUtilTest, ParseDynamicLottieAssetId) {
  ParsedDynamicAssetId output;
  EXPECT_FALSE(ParseDynamicLottieAssetId("StaticAsset", output));
  EXPECT_FALSE(ParseDynamicLottieAssetId("_CrOS_UnknownAsset", output));
  ASSERT_TRUE(ParseDynamicLottieAssetId("_CrOS_Photo_PositionA_1", output));
  EXPECT_THAT(output.position_id, Eq("A"));
  EXPECT_THAT(output.idx, Eq(1));
  ASSERT_TRUE(ParseDynamicLottieAssetId("_CrOS_Photo_PositionB_1", output));
  EXPECT_THAT(output.position_id, Eq("B"));
  EXPECT_THAT(output.idx, Eq(1));
  ASSERT_TRUE(
      ParseDynamicLottieAssetId("_CrOS_Photo_PositionTopLeft_1", output));
  EXPECT_THAT(output.position_id, Eq("TopLeft"));
  EXPECT_THAT(output.idx, Eq(1));
  ASSERT_TRUE(
      ParseDynamicLottieAssetId("_CrOS_Photo_PositionTopRight_2", output));
  EXPECT_THAT(output.position_id, Eq("TopRight"));
  EXPECT_THAT(output.idx, Eq(2));
  ASSERT_TRUE(ParseDynamicLottieAssetId("_CrOS_Photo_PositionA_1.png", output));
  EXPECT_THAT(output.position_id, Eq("A"));
  EXPECT_THAT(output.idx, Eq(1));
}

TEST(AmbientUtilTest, CompareDynamicLottieAssetId) {
  EXPECT_THAT(ParsedDynamicAssetId({"A", 1}),
              Lt(ParsedDynamicAssetId({"A", 2})));
  EXPECT_THAT(ParsedDynamicAssetId({"A", 2}),
              Not(Lt(ParsedDynamicAssetId({"A", 1}))));
  EXPECT_THAT(ParsedDynamicAssetId({"A", 1}),
              Lt(ParsedDynamicAssetId({"B", 1})));
  EXPECT_THAT(ParsedDynamicAssetId({"B", 1}),
              Not(Lt(ParsedDynamicAssetId({"A", 1}))));
  EXPECT_THAT(ParsedDynamicAssetId({"A", 1}),
              Not(Lt(ParsedDynamicAssetId({"A", 1}))));
}

}  // namespace util
}  // namespace ambient
}  // namespace ash
