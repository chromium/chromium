// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_util.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "base/notreached.h"
#include "base/test/scoped_feature_list.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/display/display.h"
#include "ui/display/display_features.h"
#include "ui/display/display_switches.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {
namespace {

constexpr auto kRoundedDisplayRadii = gfx::RoundedCornersF(16);

std::string ToDisplaySpecRadiiString(const gfx::RoundedCornersF& radii) {
  return base::StringPrintf("~%1.f|%1.f|%1.f|%1.f", radii.upper_left(),
                            radii.upper_right(), radii.lower_right(),
                            radii.lower_left());
}

std::string ToDisplaySpecRotationString(display::Display::Rotation rotation) {
  switch (rotation) {
    case display::Display::ROTATE_0:
      return "";
    case display::Display::ROTATE_90:
      return "/r";
    case display::Display::ROTATE_180:
      return "/u";
    case display::Display::ROTATE_270:
      return "/l";
  }

  NOTREACHED();
}

std::string ToDisplaySpecDeviceScaleFactorString(int scale_factor) {
  return base::StringPrintf("*%d", scale_factor);
}

class RoundedDisplayPixelTest
    : public AshTestBase,
      public testing::WithParamInterface<
          std::tuple</*device_scale_factor=*/int, display::Display::Rotation>> {
 public:
  RoundedDisplayPixelTest() = default;

  RoundedDisplayPixelTest(const RoundedDisplayPixelTest&) = delete;
  RoundedDisplayPixelTest& operator=(const RoundedDisplayPixelTest&) = delete;

  ~RoundedDisplayPixelTest() override = default;

  // AshTestBase:
  void SetUp() override {
    // TODO(b/331996916): Use `UpdateDisplay()` method to specify display
    // radius.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kHostWindowBounds,
        "1280x720" +
            ToDisplaySpecDeviceScaleFactorString(device_scale_factor()) +
            ToDisplaySpecRotationString(rotation()) +
            ToDisplaySpecRadiiString(kRoundedDisplayRadii));
    scoped_features_.InitAndEnableFeature(display::features::kRoundedDisplay);
    AshTestBase::SetUp();
  }
  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

 private:
  int device_scale_factor() const { return std::get<0>(GetParam()); }
  display::Display::Rotation rotation() const {
    return std::get<1>(GetParam());
  }

  base::test::ScopedFeatureList scoped_features_;
};

// Verifies that mask textures are correctly drawn on the display.
TEST_P(RoundedDisplayPixelTest, AlwaysOnTopMaskTextures) {
  auto window = CreateTestWindow(gfx::Rect(500, 500));
  DecorateWindow(window.get(), u"Window", SK_ColorGREEN);

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "always_on_top_rounded_display_mask_textures",
      /*revision_number=*/1, Shell::GetPrimaryRootWindow()));
}

INSTANTIATE_TEST_SUITE_P(
    /*no prefix*/,
    RoundedDisplayPixelTest,
    testing::Values(std::make_tuple(2, display::Display::ROTATE_0),
                    std::make_tuple(1, display::Display::ROTATE_90)));

}  // namespace
}  // namespace ash
