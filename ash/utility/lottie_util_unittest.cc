// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/utility/lottie_util.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

TEST(LottieUtilTest, IsDynamicLottieAsset) {
  EXPECT_TRUE(IsCustomizableLottieId("_CrOS1"));
  EXPECT_TRUE(IsCustomizableLottieId("_CrOS2"));
  EXPECT_FALSE(IsCustomizableLottieId("some_random_string"));
  EXPECT_FALSE(IsCustomizableLottieId(""));
}

}  // namespace ash
