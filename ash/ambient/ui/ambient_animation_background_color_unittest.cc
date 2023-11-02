// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/ambient_animation_background_color.h"

#include "base/memory/scoped_refptr.h"
#include "cc/paint/skottie_wrapper.h"
#include "cc/test/lottie_test_data.h"
#include "cc/test/skia_common.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using ::testing::Eq;

TEST(AmbientAnimationBackgroundColorTest, Basic) {
  EXPECT_THAT(
      GetAnimationBackgroundColor(*cc::CreateSkottieFromString(
          cc::CreateCustomLottieDataWith2ColorNodes(
              "background_solid", cc::kLottieDataWithoutAssets1Color2Node))),
      Eq(cc::kLottieDataWithoutAssets1Color1));
  EXPECT_THAT(
      GetAnimationBackgroundColor(*cc::CreateSkottieFromString(
          cc::CreateCustomLottieDataWith2ColorNodes(
              cc::kLottieDataWithoutAssets1Color1Node, "background_solid"))),
      Eq(cc::kLottieDataWithoutAssets1Color2));
}

}  // namespace
}  // namespace ash
