// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/util/ambient_util.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace ambient {
namespace util {

TEST(AmbientUtilTest, IsDynamicLottieAsset) {
  EXPECT_TRUE(IsDynamicLottieAsset("USER_PHOTO_1"));
  EXPECT_TRUE(IsDynamicLottieAsset("USER_PHOTO_2"));
  EXPECT_FALSE(IsDynamicLottieAsset("some_random_string"));
  EXPECT_FALSE(IsDynamicLottieAsset("random_string_with_user_photo_in_it"));
}

}  // namespace util
}  // namespace ambient
}  // namespace ash
