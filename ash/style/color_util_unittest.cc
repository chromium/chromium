// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/color_util.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/wallpaper/wallpaper_controller_test_api.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_calculated_colors.h"
#include "base/test/scoped_feature_list.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/test/sk_color_eq.h"

namespace ash {

namespace {

constexpr SkColor kTestDefaultColor = SK_ColorYELLOW;

}  // namespace

class ColorUtilTest : public AshTestBase {
 public:
  ColorUtilTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kDarkLightModeKMeansColor);
  }

  ColorUtilTest(const ColorUtilTest&) = delete;
  ColorUtilTest& operator=(const ColorUtilTest&) = delete;

  void SetUp() override {
    AshTestBase::SetUp();
    wallpaper_controller_test_api_ =
        std::make_unique<WallpaperControllerTestApi>(
            Shell::Get()->wallpaper_controller());
  }

  WallpaperControllerTestApi* test_api() {
    return wallpaper_controller_test_api_.get();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<WallpaperControllerTestApi> wallpaper_controller_test_api_;
};

TEST_F(ColorUtilTest, DefaultsToDefaultColor) {
  test_api()->SetCalculatedColors(
      {/*prominent_colors=*/{}, /*k_mean_color=*/kInvalidWallpaperColor});
  for (const bool use_dark_color : {true, false}) {
    EXPECT_SKCOLOR_EQ(
        kTestDefaultColor,
        ColorUtil::GetBackgroundThemedColor(kTestDefaultColor, use_dark_color));
  }
}

TEST_F(ColorUtilTest, MixesWithWhiteInLightMode) {
  // Tuple of k_mean_color, expected output color after masking with white.
  std::vector<std::tuple<SkColor, SkColor>> cases = {
      {SK_ColorRED, SkColorSetARGB(0xFF, 0xFF, 0xE6, 0xE6)},
      {SK_ColorGREEN, SkColorSetARGB(0xFF, 0xE6, 0xFF, 0xE6)},
      {SK_ColorMAGENTA, SkColorSetARGB(0xFF, 0xFF, 0xE6, 0xFF)},
  };
  for (const auto& [k_mean_color, expected_color] : cases) {
    test_api()->SetCalculatedColors({{}, k_mean_color});
    SkColor result_color =
        ColorUtil::GetBackgroundThemedColor(kTestDefaultColor, false);
    EXPECT_SKCOLOR_EQ(expected_color, result_color);
  }
}

TEST_F(ColorUtilTest, MixesWithBlackInDarkMode) {
  // Tuple of k_mean_color, expected output color after masking with black.
  std::vector<std::tuple<SkColor, SkColor>> cases = {
      {SK_ColorRED, SkColorSetARGB(0xFF, 0x33, 0x00, 0x00)},
      {SK_ColorGREEN, SkColorSetARGB(0xFF, 0x00, 0x33, 0x00)},
      {SK_ColorMAGENTA, SkColorSetARGB(0xFF, 0x33, 0x00, 0x33)},
  };
  for (const auto& [k_mean_color, expected_color] : cases) {
    test_api()->SetCalculatedColors({{}, k_mean_color});
    SkColor result_color =
        ColorUtil::GetBackgroundThemedColor(kTestDefaultColor, true);
    EXPECT_SKCOLOR_EQ(expected_color, result_color);
  }
}

}  // namespace ash
