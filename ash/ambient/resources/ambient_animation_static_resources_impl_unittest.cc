// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/resources/ambient_animation_static_resources.h"

#include "ash/public/cpp/ambient/ambient_animation_theme.h"
#include "base/json/json_reader.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::Not;
using ::testing::NotNull;

// AmbientAnimationStaticResources actually has very little application logic
// and is more a class to house static data. Thus, an animation theme is picked
// as an example and the basics are tested with it. A test case does not need to
// exist for every possible animation theme.

TEST(AmbientAnimationStaticResourcesTest, LoadsLottieData) {
  auto resources = AmbientAnimationStaticResources::Create(
      AmbientAnimationTheme::kFeelTheBreeze);
  ASSERT_THAT(resources, NotNull());
  ASSERT_THAT(resources->GetLottieData(), Not(IsEmpty()));
  EXPECT_TRUE(base::JSONReader::Read(resources->GetLottieData()));
}

TEST(AmbientAnimationStaticResourcesTest, LoadsStaticAssets) {
  auto resources = AmbientAnimationStaticResources::Create(
      AmbientAnimationTheme::kFeelTheBreeze);
  ASSERT_THAT(resources, NotNull());
  gfx::ImageSkia clips_bottom_original =
      resources->GetStaticImageAsset("clips_bottom.png");
  ASSERT_FALSE(clips_bottom_original.isNull());
  gfx::ImageSkia clips_bottom_reloaded =
      resources->GetStaticImageAsset("clips_bottom.png");
  ASSERT_FALSE(clips_bottom_reloaded.isNull());
  EXPECT_TRUE(
      clips_bottom_reloaded.BackedBySameObjectAs(clips_bottom_original));

  gfx::ImageSkia clips_top = resources->GetStaticImageAsset("clips_top.png");
  EXPECT_FALSE(clips_top.isNull());
}

TEST(AmbientAnimationStaticResourcesTest, FailsForSlideshowTheme) {
  EXPECT_THAT(AmbientAnimationStaticResources::Create(
                  AmbientAnimationTheme::kSlideshow),
              IsNull());
}

TEST(AmbientAnimationStaticResourcesTest, FailsForUnknownAssetId) {
  auto resources = AmbientAnimationStaticResources::Create(
      AmbientAnimationTheme::kFeelTheBreeze);
  ASSERT_THAT(resources, NotNull());
  gfx::ImageSkia image = resources->GetStaticImageAsset("unknown_asset_id");
  EXPECT_TRUE(image.isNull());
}

}  // namespace ash
