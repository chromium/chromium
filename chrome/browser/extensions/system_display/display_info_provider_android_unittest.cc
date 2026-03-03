// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/system_display/display_info_provider_android.h"

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/rect.h"

namespace extensions {
namespace {

class DisplayInfoProviderAndroidForTesting : public DisplayInfoProviderAndroid {
 public:
  using DisplayInfoProvider::GetAllDisplaysInfoList;
};

class DisplayInfoProviderAndroidTest : public testing::Test {
 protected:
  DisplayInfoProviderAndroidForTesting provider_;
};

TEST_F(DisplayInfoProviderAndroidTest, PopulatePlatformFields) {
  display::Display display(1234, gfx::Rect(0, 0, 1920, 1080));
  display.set_label("Test Display");
  display.set_pixels_per_inch(260.0f, 265.0f);

  std::vector<display::Display> displays;
  displays.push_back(display);
  const auto info = provider_.GetAllDisplaysInfoList(displays, display.id());

  ASSERT_EQ(1u, info.size());
  EXPECT_EQ("Test Display", info[0].name);
  EXPECT_FLOAT_EQ(260.0f, info[0].dpi_x);
  EXPECT_FLOAT_EQ(265.0f, info[0].dpi_y);
}

TEST_F(DisplayInfoProviderAndroidTest, UsesScaleFactorFallbackWhenDpiMissing) {
  display::Display display(42, gfx::Rect(0, 0, 1920, 1080));
  display.set_label("");
  display.set_device_scale_factor(2.0f);
  display.set_pixels_per_inch(0.0f, 0.0f);

  std::vector<display::Display> displays;
  displays.push_back(display);
  const auto info = provider_.GetAllDisplaysInfoList(displays, display.id());

  ASSERT_EQ(1u, info.size());
  EXPECT_EQ("42", info[0].name);
  EXPECT_DOUBLE_EQ(192.0, info[0].dpi_x);
  EXPECT_DOUBLE_EQ(192.0, info[0].dpi_y);
}

TEST_F(DisplayInfoProviderAndroidTest,
       UsesBaseFallbackWhenDpiAndScaleFactorMissing) {
  display::Display display(24, gfx::Rect(0, 0, 1920, 1080));
  display.set_label("");
  display.set_device_scale_factor(0.0f);
  display.set_pixels_per_inch(0.0f, 0.0f);

  std::vector<display::Display> displays;
  displays.push_back(display);
  const auto info = provider_.GetAllDisplaysInfoList(displays, display.id());

  ASSERT_EQ(1u, info.size());
  EXPECT_EQ("24", info[0].name);
  EXPECT_DOUBLE_EQ(96.0, info[0].dpi_x);
  EXPECT_DOUBLE_EQ(96.0, info[0].dpi_y);
}

TEST_F(DisplayInfoProviderAndroidTest, MultiDisplayListMapping) {
  display::Display display1(1, gfx::Rect(0, 0, 1920, 1080));
  display1.set_label("Display One");

  display::Display display2(2, gfx::Rect(1920, 0, 800, 600));
  display2.set_label("");

  display::Display display3(3, gfx::Rect(0, 1080, 1024, 768));
  display3.set_label("Display Three");

  std::vector<display::Display> displays = {display1, display2, display3};
  const DisplayInfoProvider::DisplayUnitInfoList info =
      provider_.GetAllDisplaysInfoList(displays, /*primary_id=*/display1.id());

  ASSERT_EQ(3u, info.size());

  EXPECT_EQ("1", info[0].id);
  EXPECT_EQ("Display One", info[0].name);

  EXPECT_EQ("2", info[1].id);
  EXPECT_EQ("2", info[1].name);

  EXPECT_EQ("3", info[2].id);
  EXPECT_EQ("Display Three", info[2].name);
}

TEST_F(DisplayInfoProviderAndroidTest, MixedDpiFallbackMultiDisplay) {
  display::Display display1(1, gfx::Rect(0, 0, 1920, 1080));
  display1.set_pixels_per_inch(100.0f, 110.0f);
  display1.set_device_scale_factor(2.0f);

  display::Display display2(2, gfx::Rect(1920, 0, 800, 600));
  display2.set_pixels_per_inch(0.0f, 0.0f);
  display2.set_device_scale_factor(2.5f);

  display::Display display3(3, gfx::Rect(0, 1080, 1024, 768));
  display3.set_pixels_per_inch(0.0f, 0.0f);
  display3.set_device_scale_factor(0.0f);

  std::vector<display::Display> displays = {display1, display2, display3};
  const DisplayInfoProvider::DisplayUnitInfoList info =
      provider_.GetAllDisplaysInfoList(displays, /*primary_id=*/display1.id());

  ASSERT_EQ(3u, info.size());
  EXPECT_DOUBLE_EQ(100.0, info[0].dpi_x);
  EXPECT_DOUBLE_EQ(110.0, info[0].dpi_y);
  EXPECT_DOUBLE_EQ(240.0, info[1].dpi_x);
  EXPECT_DOUBLE_EQ(240.0, info[1].dpi_y);
  EXPECT_DOUBLE_EQ(96.0, info[2].dpi_x);
  EXPECT_DOUBLE_EQ(96.0, info[2].dpi_y);
}

}  // namespace
}  // namespace extensions
