// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/color_util.h"

#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/wallpaper/wallpaper_controller_test_api.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_calculated_colors.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/test/sk_color_eq.h"

namespace ash {

class ColorUtilTest : public AshTestBase {
 public:
  ColorUtilTest() = default;

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
  std::unique_ptr<WallpaperControllerTestApi> wallpaper_controller_test_api_;
};

TEST_F(ColorUtilTest, MixesWithWhiteInLightMode) {
  // Tuple of k_mean_color, expected output color after masking with white.
  std::vector<std::tuple<SkColor, SkColor>> cases = {
      {SK_ColorRED, SkColorSetARGB(0xFF, 0xFF, 0xE6, 0xE6)},
      {SK_ColorGREEN, SkColorSetARGB(0xFF, 0xE6, 0xFF, 0xE6)},
      {SK_ColorMAGENTA, SkColorSetARGB(0xFF, 0xFF, 0xE6, 0xFF)},
  };
  for (const auto& [k_mean_color, expected_color] : cases) {
    SkColor result_color = ColorUtil::AdjustKMeansColor(k_mean_color, false);
    EXPECT_SKCOLOR_EQ(expected_color, result_color);
  }
}

TEST_F(ColorUtilTest, ClampsMaxLightnessInLightMode) {
  // Tuple of k_mean_color, expected output color after darkening and masking
  // with white.
  std::vector<std::tuple<SkColor, SkColor>> cases = {
      // Pure white is shifted to gray.
      {SK_ColorWHITE, SkColorSetARGB(0xFF, 0xF8, 0xF8, 0xF8)},
      // #B3B3B3 should result in the same output color as pure white.
      {
          SkColorSetARGB(0xFF, 0xB3, 0xB3, 0xB3),
          SkColorSetARGB(0xFF, 0xF8, 0xF8, 0xF8),
      },
      // Slightly darker than last case results in a different output color than
      // previous two.
      {
          SkColorSetARGB(0xFF, 0xB2, 0xB2, 0xB2),
          SkColorSetARGB(0xFF, 0xF7, 0xF7, 0xF7),
      },
      // Light pink retains red hue.
      {
          SkColorSetARGB(0xFF, 0xFF, 0xEE, 0xEE),
          SkColorSetARGB(0xFF, 0xFF, 0xF0, 0xF0),
      },
  };
  for (const auto& [k_mean_color, expected_color] : cases) {
    SkColor result_color = ColorUtil::AdjustKMeansColor(k_mean_color, false);
    EXPECT_SKCOLOR_EQ(expected_color, result_color);
  }
}

TEST_F(ColorUtilTest, MixesWithBlackInDarkMode) {
  // Tuple of k_mean_color, expected output color after masking with black.
  std::vector<std::tuple<SkColor, SkColor>> cases = {
      {SK_ColorRED, SkColorSetARGB(0xFF, 0x5A, 0x00, 0x00)},
      {SK_ColorGREEN, SkColorSetARGB(0xFF, 0x00, 0x5A, 0x00)},
      {SK_ColorMAGENTA, SkColorSetARGB(0xFF, 0x5A, 0x00, 0x5A)},
  };
  for (const auto& [k_mean_color, expected_color] : cases) {
    SkColor result_color = ColorUtil::AdjustKMeansColor(k_mean_color, true);
    EXPECT_SKCOLOR_EQ(expected_color, result_color);
  }
}

TEST_F(ColorUtilTest, ClampsMaxDarknessInDarkMode) {
  // Tuple of k_mean_color, expected output color after lightening and masking
  // with black.
  std::vector<std::tuple<SkColor, SkColor>> cases = {
      // Pure black is shifted to dark gray.
      {SK_ColorBLACK, SkColorSetARGB(0xFF, 0x1B, 0x1B, 0x1B)},
      // #4D4D4D should result in the same output color as black.
      {
          SkColorSetARGB(0xFF, 0x4D, 0x4D, 0x4D),
          SkColorSetARGB(0xFF, 0x1B, 0x1B, 0x1B),
      },
      // Slightly lighter than last case results in a different output color
      // from previous two, as it is light enough to skip lightness shift.
      {
          SkColorSetARGB(0xFF, 0x4F, 0x4F, 0x4F),
          SkColorSetARGB(0xFF, 0x1C, 0x1C, 0x1C),
      },
      // Dark red retains red hue.
      {
          SkColorSetARGB(0xFF, 0x44, 0x00, 0x00),
          SkColorSetARGB(0xFF, 0x36, 0x00, 0x00),
      },
  };
  for (const auto& [k_mean_color, expected_color] : cases) {
    SkColor result_color = ColorUtil::AdjustKMeansColor(k_mean_color, true);
    EXPECT_SKCOLOR_EQ(expected_color, result_color);
  }
}

}  // namespace ash
