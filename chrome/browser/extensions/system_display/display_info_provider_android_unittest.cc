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

}  // namespace
}  // namespace extensions
