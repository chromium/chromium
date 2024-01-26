// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/resources/ambient_animation_static_resources.h"

#include <string_view>

#include "ash/ambient/ambient_ui_settings.h"
#include "ash/ambient/resources/ambient_animation_resource_constants.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom-shared.h"
#include "base/json/json_reader.h"
#include "cc/paint/skottie_wrapper.h"
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
      AmbientUiSettings(
          personalization_app::mojom::AmbientTheme::kFeelTheBreeze),
      /*serializable=*/false);
  ASSERT_THAT(resources->GetSkottieWrapper(), NotNull());
  EXPECT_TRUE(resources->GetSkottieWrapper()->is_valid());
}

TEST(AmbientAnimationStaticResourcesTest, LoadsStaticAssets) {
  auto resources = AmbientAnimationStaticResources::Create(
      AmbientUiSettings(
          personalization_app::mojom::AmbientTheme::kFeelTheBreeze),
      /*serializable=*/false);
  ASSERT_THAT(resources, NotNull());
  for (std::string_view asset_id :
       ambient::resources::kAllFeelTheBreezeStaticAssets) {
    gfx::ImageSkia image_original = resources->GetStaticImageAsset(asset_id);
    ASSERT_FALSE(image_original.isNull());
    gfx::ImageSkia image_reloaded = resources->GetStaticImageAsset(asset_id);
    ASSERT_FALSE(image_reloaded.isNull());
    EXPECT_TRUE(image_reloaded.BackedBySameObjectAs(image_original));
  }
}

TEST(AmbientAnimationStaticResourcesTest, FailsForSlideshowTheme) {
  EXPECT_THAT(AmbientAnimationStaticResources::Create(
                  AmbientUiSettings(
                      personalization_app::mojom::AmbientTheme::kSlideshow),
                  /*serializable=*/false),
              IsNull());
}

TEST(AmbientAnimationStaticResourcesTest, FailsForUnknownAssetId) {
  auto resources = AmbientAnimationStaticResources::Create(
      AmbientUiSettings(
          personalization_app::mojom::AmbientTheme::kFeelTheBreeze),
      /*serializable=*/false);
  ASSERT_THAT(resources, NotNull());
  gfx::ImageSkia image = resources->GetStaticImageAsset("unknown_asset_id");
  EXPECT_TRUE(image.isNull());
}

}  // namespace ash
