// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/ash_color_provider.h"

#include <ostream>

#include "ash/public/cpp/style/dark_light_mode_controller.h"
#include "ash/test/ash_test_helper.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/constants/chromeos_features.h"
#include "testing/gtest/include/gtest/gtest-param-test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/color/color_provider_key.h"

namespace ash {

namespace {

using ColorMode = ui::ColorProviderKey::ColorMode;

template <class LayerType>
struct ColorsTestCase {
  ColorMode color_mode;
  LayerType type;
  SkColor expected_color;
};

std::string ColorToString(SkColor color) {
  uint8_t alpha = SkColorGetA(color);
  if (alpha == 0xFF)
    return base::StringPrintf("RGB(0x%X,0x%X,0x%X)", SkColorGetR(color),
                              SkColorGetG(color), SkColorGetB(color));

  return base::StringPrintf("ARGB(0x%X,0x%X,0x%X,0x%X)", alpha,
                            SkColorGetR(color), SkColorGetG(color),
                            SkColorGetB(color));
}

template <class T>
std::ostream& operator<<(std::ostream& os, const ColorsTestCase<T>& test_case) {
  os << "{";
  os << (test_case.color_mode == ColorMode::kDark ? "ColorMode::kDark"
                                                  : "ColorMode::kLight");
  os << ", ";
  os << (int)test_case.type;
  os << ", ";
  os << ColorToString(test_case.expected_color);
  os << "}";
  return os;
}

template <class LayerType>
class AshColorProviderBase
    : public testing::TestWithParam<ColorsTestCase<LayerType>> {
 public:
  AshColorProviderBase()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI) {
    // Disable when Jelly is enabled since it changes all the colors (and
    // this test verifies the old colors).
    features_.InitAndDisableFeature(chromeos::features::kJelly);
  }

  void SetUp() override {
    ash_test_helper_.SetUp();
    color_provider_ = AshColorProvider::Get();
  }

  void TearDown() override {
    ash_test_helper_.TearDown();
    color_provider_ = nullptr;
  }

 protected:
  base::test::ScopedFeatureList features_;
  base::test::TaskEnvironment task_environment_;
  AshTestHelper ash_test_helper_;
  raw_ptr<AshColorProvider, DanglingUntriaged> color_provider_;
};

using AshColorProviderControlsLayerTest =
    AshColorProviderBase<ColorProvider::ControlsLayerType>;

TEST_P(AshColorProviderControlsLayerTest, TestControlsColors) {
  const auto& test_case = GetParam();
  bool dark = test_case.color_mode == ColorMode::kDark;
  DarkLightModeController::Get()->SetDarkModeEnabledForTest(dark);
  EXPECT_EQ(test_case.expected_color,
            color_provider_->GetControlsLayerColor(test_case.type))
      << "Colors do not match. Expected " << test_case << " Actual: "
      << ColorToString(color_provider_->GetControlsLayerColor(test_case.type));
}

INSTANTIATE_TEST_SUITE_P(
    AshColorProviderTests,
    AshColorProviderControlsLayerTest,
    testing::ValuesIn<ColorsTestCase<ColorProvider::ControlsLayerType>>(
        {// Dark mode
         {ColorMode::kDark,
          ColorProvider::ControlsLayerType::kHairlineBorderColor,
          SkColorSetARGB(0x24, 0xFF, 0xFF, 0xFF)},
         {ColorMode::kDark,
          ColorProvider::ControlsLayerType::kControlBackgroundColorActive,
          SkColorSetRGB(0x8A, 0xB4, 0xF8)},
         {ColorMode::kDark,
          ColorProvider::ControlsLayerType::kControlBackgroundColorInactive,
          SkColorSetARGB(0x1A, 0xFF, 0xFF, 0xFF)},
         {ColorMode::kDark,
          ColorProvider::ControlsLayerType::kControlBackgroundColorAlert,
          SkColorSetRGB(0xF2, 0x8B, 0x82)},
         {ColorMode::kDark,
          ColorProvider::ControlsLayerType::kControlBackgroundColorWarning,
          SkColorSetRGB(0xFD, 0xD6, 0x63)},
         {ColorMode::kDark,
          ColorProvider::ControlsLayerType::kControlBackgroundColorPositive,
          SkColorSetRGB(0x81, 0xC9, 0x95)},
         {ColorMode::kDark, ColorProvider::ControlsLayerType::kFocusAuraColor,
          SkColorSetARGB(0x3D, 0x8A, 0xB4, 0xF8)},
         {ColorMode::kDark, ColorProvider::ControlsLayerType::kFocusRingColor,
          SkColorSetRGB(0x8A, 0xB4, 0xF8)},

         // Light mode
         {ColorMode::kLight,
          ColorProvider::ControlsLayerType::kHairlineBorderColor,
          SkColorSetARGB(0x24, 0x0, 0x0, 0x0)},
         {ColorMode::kLight,
          ColorProvider::ControlsLayerType::kControlBackgroundColorActive,
          SkColorSetRGB(0x1A, 0x73, 0xE8)},
         {ColorMode::kLight,
          ColorProvider::ControlsLayerType::kControlBackgroundColorInactive,
          SkColorSetARGB(0x0D, 0x0, 0x0, 0x0)},
         {ColorMode::kLight,
          ColorProvider::ControlsLayerType::kControlBackgroundColorAlert,
          SkColorSetRGB(0xD9, 0x30, 0x25)},
         {ColorMode::kLight,
          ColorProvider::ControlsLayerType::kControlBackgroundColorWarning,
          SkColorSetRGB(0xE3, 0x74, 0x0)},
         {ColorMode::kLight,
          ColorProvider::ControlsLayerType::kControlBackgroundColorPositive,
          SkColorSetRGB(0x18, 0x80, 0x38)},
         {ColorMode::kLight, ColorProvider::ControlsLayerType::kFocusAuraColor,
          SkColorSetARGB(0x3D, 0x1A, 0x73, 0xE8)},
         {ColorMode::kLight, ColorProvider::ControlsLayerType::kFocusRingColor,
          SkColorSetRGB(0x1A, 0x73, 0xE8)}}));

class AshColorProviderContentTest
    : public AshColorProviderBase<ColorProvider::ContentLayerType> {};

TEST_P(AshColorProviderContentTest, Colors) {
  const auto& test_case = GetParam();
  bool dark = test_case.color_mode == ColorMode::kDark;
  DarkLightModeController::Get()->SetDarkModeEnabledForTest(dark);
  SkColor actual_color = color_provider_->GetContentLayerColor(test_case.type);
  EXPECT_EQ(test_case.expected_color, actual_color)
      << "Colors do not match. Expected " << test_case
      << " Actual: " << ColorToString(actual_color);
}

INSTANTIATE_TEST_SUITE_P(
    AshColorProviderTests,
    AshColorProviderContentTest,
    testing::ValuesIn<ColorsTestCase<ColorProvider::ContentLayerType>>(
        {// Light colors
         {ColorMode::kLight, ColorProvider::ContentLayerType::kScrollBarColor,
          SkColorSetRGB(0x5F, 0x63, 0x68)},
         {ColorMode::kLight, ColorProvider::ContentLayerType::kSeparatorColor,
          SkColorSetARGB(0x24, 0x0, 0x0, 0x0)},

         {ColorMode::kLight, ColorProvider::ContentLayerType::kTextColorPrimary,
          SkColorSetRGB(0x20, 0x21, 0x24)},
         {ColorMode::kLight,
          ColorProvider::ContentLayerType::kInvertedTextColorPrimary,
          SkColorSetRGB(0xE8, 0xEA, 0xED)},
         {ColorMode::kLight,
          ColorProvider::ContentLayerType::kTextColorSecondary,
          SkColorSetRGB(0x5F, 0x63, 0x68)},
         {ColorMode::kLight, ColorProvider::ContentLayerType::kTextColorAlert,
          SkColorSetRGB(0xD9, 0x30, 0x25)},
         {ColorMode::kLight, ColorProvider::ContentLayerType::kTextColorWarning,
          SkColorSetRGB(0xE3, 0x74, 0x0)},
         {ColorMode::kLight,
          ColorProvider::ContentLayerType::kTextColorPositive,
          SkColorSetRGB(0x18, 0x80, 0x38)},
         {ColorMode::kLight, ColorProvider::ContentLayerType::kTextColorURL,
          SkColorSetRGB(0x1A, 0x73, 0xE8)},

         {ColorMode::kLight, ColorProvider::ContentLayerType::kIconColorPrimary,
          SkColorSetRGB(0x20, 0x21, 0x24)},
         {ColorMode::kLight,
          ColorProvider::ContentLayerType::kIconColorSecondary,
          SkColorSetRGB(0x5F, 0x63, 0x68)},
         {ColorMode::kLight, ColorProvider::ContentLayerType::kIconColorAlert,
          SkColorSetRGB(0xD9, 0x30, 0x25)},
         {ColorMode::kLight, ColorProvider::ContentLayerType::kIconColorWarning,
          SkColorSetRGB(0xE3, 0x74, 0x0)},
         {ColorMode::kLight,
          ColorProvider::ContentLayerType::kIconColorPositive,
          SkColorSetRGB(0x18, 0x80, 0x38)},
         {ColorMode::kLight,
          ColorProvider::ContentLayerType::kIconColorProminent,
          SkColorSetRGB(0x1A, 0x73, 0xE8)},

         {ColorMode::kLight,
          ColorProvider::ContentLayerType::kIconColorSecondaryBackground,
          SkColorSetRGB(0x3C, 0x40, 0x43)},

         {ColorMode::kLight, ColorProvider::ContentLayerType::kButtonLabelColor,
          SkColorSetRGB(0x20, 0x21, 0x24)},
         {ColorMode::kLight,
          ColorProvider::ContentLayerType::kInvertedButtonLabelColor,
          SkColorSetRGB(0xE8, 0xEA, 0xED)},
         {ColorMode::kLight,
          ColorProvider::ContentLayerType::kButtonLabelColorPrimary,
          SkColorSetRGB(0xE8, 0xEA, 0xED)},

         {ColorMode::kLight,
          ColorProvider::ContentLayerType::kButtonLabelColorBlue,
          SkColorSetRGB(0x1A, 0x73, 0xE8)},

         {ColorMode::kLight, ColorProvider::ContentLayerType::kButtonIconColor,
          SkColorSetRGB(0x20, 0x21, 0x24)},
         {ColorMode::kLight,
          ColorProvider::ContentLayerType::kButtonIconColorPrimary,
          SkColorSetRGB(0xE8, 0xEA, 0xED)},

         {ColorMode::kLight,
          ColorProvider::ContentLayerType::kAppStateIndicatorColor,
          SkColorSetRGB(0x20, 0x21, 0x24)},
         {ColorMode::kLight,
          ColorProvider::ContentLayerType::kAppStateIndicatorColorInactive,
          SkColorSetARGB(0x60, 0x20, 0x21, 0x24)},

         {ColorMode::kLight,
          ColorProvider::ContentLayerType::kSliderColorActive,
          SkColorSetRGB(0x1A, 0x73, 0xE8)},
         {ColorMode::kLight,
          ColorProvider::ContentLayerType::kSliderColorInactive,
          SkColorSetRGB(0x5F, 0x63, 0x68)},

         {ColorMode::kLight, ColorProvider::ContentLayerType::kRadioColorActive,
          SkColorSetRGB(0x1A, 0x73, 0xE8)},
         {ColorMode::kLight,
          ColorProvider::ContentLayerType::kRadioColorInactive,
          SkColorSetRGB(0x5F, 0x63, 0x68)},

         {ColorMode::kLight,
          ColorProvider::ContentLayerType::kSwitchKnobColorActive,
          SkColorSetRGB(0x1A, 0x73, 0xE8)},
         {ColorMode::kLight,
          ColorProvider::ContentLayerType::kSwitchKnobColorInactive,
          SkColorSetRGB(0xFF, 0xFF, 0xFF)},
         {ColorMode::kLight,
          ColorProvider::ContentLayerType::kSwitchTrackColorActive,
          SkColorSetARGB(0x4C, 0x1A, 0x73, 0xE8)},
         {ColorMode::kLight,
          ColorProvider::ContentLayerType::kSwitchTrackColorInactive,
          SkColorSetARGB(0x4C, 0x5F, 0x63, 0x68)},

         {ColorMode::kLight, ColorProvider::ContentLayerType::kCurrentDeskColor,
          SkColorSetRGB(0x0, 0x0, 0x0)},

         {ColorMode::kLight,
          ColorProvider::ContentLayerType::kBatteryBadgeColor,
          SkColorSetRGB(0xE8, 0xEA, 0xED)},

         {ColorMode::kLight,
          ColorProvider::ContentLayerType::kSwitchAccessInnerStrokeColor,
          SkColorSetRGB(0x8A, 0xB4, 0xF8)},
         {ColorMode::kLight,
          ColorProvider::ContentLayerType::kSwitchAccessOuterStrokeColor,
          SkColorSetRGB(0x17, 0x4E, 0xA6)},

         {ColorMode::kLight,
          ColorProvider::ContentLayerType::kProgressBarColorForeground,
          SkColorSetRGB(0x1A, 0x73, 0xE8)},
         {ColorMode::kLight,
          ColorProvider::ContentLayerType::kProgressBarColorBackground,
          SkColorSetARGB(0x4C, 0x1A, 0x73, 0xE8)},

         {ColorMode::kLight,
          ColorProvider::ContentLayerType::kHighlightColorHover,
          SkColorSetARGB(0x14, 0x0, 0x0, 0x0)},

         {ColorMode::kLight,
          ColorProvider::ContentLayerType::kBatterySystemInfoBackgroundColor,
          SkColorSetRGB(0x18, 0x80, 0x38)},

         {ColorMode::kLight,
          ColorProvider::ContentLayerType::kBatterySystemInfoIconColor,
          SkColorSetRGB(0xE8, 0xEA, 0xED)},

         {ColorMode::kLight,
          ColorProvider::ContentLayerType::kCaptureRegionColor,
          SkColorSetARGB(0x4C, 0x1A, 0x73, 0xE8)},

         // Dark colors
         {ColorMode::kDark, ColorProvider::ContentLayerType::kScrollBarColor,
          SkColorSetRGB(0xE8, 0xEA, 0xED)},
         {ColorMode::kDark, ColorProvider::ContentLayerType::kSeparatorColor,
          SkColorSetARGB(0x24, 0xFF, 0xFF, 0xFF)},

         {ColorMode::kDark, ColorProvider::ContentLayerType::kTextColorPrimary,
          SkColorSetRGB(0xE8, 0xEA, 0xED)},
         {ColorMode::kDark,
          ColorProvider::ContentLayerType::kInvertedTextColorPrimary,
          SkColorSetRGB(0x20, 0x21, 0x24)},
         {ColorMode::kDark,
          ColorProvider::ContentLayerType::kTextColorSecondary,
          SkColorSetRGB(0xBD, 0xC1, 0xC6)},
         {ColorMode::kDark, ColorProvider::ContentLayerType::kTextColorAlert,
          SkColorSetRGB(0xF2, 0x8B, 0x82)},
         {ColorMode::kDark, ColorProvider::ContentLayerType::kTextColorWarning,
          SkColorSetRGB(0xFD, 0xD6, 0x63)},
         {ColorMode::kDark, ColorProvider::ContentLayerType::kTextColorPositive,
          SkColorSetRGB(0x81, 0xC9, 0x95)},
         {ColorMode::kDark, ColorProvider::ContentLayerType::kTextColorURL,
          SkColorSetRGB(0x8A, 0xB4, 0xF8)},

         {ColorMode::kDark, ColorProvider::ContentLayerType::kIconColorPrimary,
          SkColorSetRGB(0xE8, 0xEA, 0xED)},
         {ColorMode::kDark,
          ColorProvider::ContentLayerType::kIconColorSecondary,
          SkColorSetRGB(0xBD, 0xC1, 0xC6)},
         {ColorMode::kDark, ColorProvider::ContentLayerType::kIconColorAlert,
          SkColorSetRGB(0xF2, 0x8B, 0x82)},
         {ColorMode::kDark, ColorProvider::ContentLayerType::kIconColorWarning,
          SkColorSetRGB(0xFD, 0xD6, 0x63)},
         {ColorMode::kDark, ColorProvider::ContentLayerType::kIconColorPositive,
          SkColorSetRGB(0x81, 0xC9, 0x95)},
         {ColorMode::kDark,
          ColorProvider::ContentLayerType::kIconColorProminent,
          SkColorSetRGB(0x8A, 0xB4, 0xF8)},

         {ColorMode::kDark,
          ColorProvider::ContentLayerType::kIconColorSecondaryBackground,
          SkColorSetRGB(0xF1, 0xF3, 0xF4)},

         {ColorMode::kDark, ColorProvider::ContentLayerType::kButtonLabelColor,
          SkColorSetRGB(0xE8, 0xEA, 0xED)},
         {ColorMode::kDark,
          ColorProvider::ContentLayerType::kInvertedButtonLabelColor,
          SkColorSetRGB(0x20, 0x21, 0x24)},
         {ColorMode::kDark,
          ColorProvider::ContentLayerType::kButtonLabelColorPrimary,
          SkColorSetRGB(0x20, 0x21, 0x24)},

         {ColorMode::kDark,
          ColorProvider::ContentLayerType::kButtonLabelColorBlue,
          SkColorSetRGB(0x8A, 0xB4, 0xF8)},

         {ColorMode::kDark, ColorProvider::ContentLayerType::kButtonIconColor,
          SkColorSetRGB(0xE8, 0xEA, 0xED)},
         {ColorMode::kDark,
          ColorProvider::ContentLayerType::kButtonIconColorPrimary,
          SkColorSetRGB(0x20, 0x21, 0x24)},

         {ColorMode::kDark,
          ColorProvider::ContentLayerType::kAppStateIndicatorColor,
          SkColorSetRGB(0xE8, 0xEA, 0xED)},
         {ColorMode::kDark,
          ColorProvider::ContentLayerType::kAppStateIndicatorColorInactive,
          SkColorSetARGB(0x60, 0xE8, 0xEA, 0xED)},

         {ColorMode::kDark, ColorProvider::ContentLayerType::kSliderColorActive,
          SkColorSetRGB(0x8A, 0xB4, 0xF8)},
         {ColorMode::kDark,
          ColorProvider::ContentLayerType::kSliderColorInactive,
          SkColorSetRGB(0xE8, 0xEA, 0xED)},

         {ColorMode::kDark, ColorProvider::ContentLayerType::kRadioColorActive,
          SkColorSetRGB(0x8A, 0xB4, 0xF8)},
         {ColorMode::kDark,
          ColorProvider::ContentLayerType::kRadioColorInactive,
          SkColorSetRGB(0xE8, 0xEA, 0xED)},

         {ColorMode::kDark,
          ColorProvider::ContentLayerType::kSwitchKnobColorActive,
          SkColorSetRGB(0x8A, 0xB4, 0xF8)},
         {ColorMode::kDark,
          ColorProvider::ContentLayerType::kSwitchKnobColorInactive,
          SkColorSetRGB(0xBD, 0xC1, 0xC6)},
         {ColorMode::kDark,
          ColorProvider::ContentLayerType::kSwitchTrackColorActive,
          SkColorSetARGB(0x4C, 0x8A, 0xB4, 0xF8)},
         {ColorMode::kDark,
          ColorProvider::ContentLayerType::kSwitchTrackColorInactive,
          SkColorSetARGB(0x4C, 0xE8, 0xEA, 0xED)},

         {ColorMode::kDark, ColorProvider::ContentLayerType::kCurrentDeskColor,
          SkColorSetRGB(0xFF, 0xFF, 0xFF)},

         {ColorMode::kDark, ColorProvider::ContentLayerType::kBatteryBadgeColor,
          SkColorSetRGB(0x20, 0x21, 0x24)},

         {ColorMode::kDark,
          ColorProvider::ContentLayerType::kSwitchAccessInnerStrokeColor,
          SkColorSetRGB(0x8A, 0xB4, 0xF8)},
         {ColorMode::kDark,
          ColorProvider::ContentLayerType::kSwitchAccessOuterStrokeColor,
          SkColorSetRGB(0x17, 0x4E, 0xA6)},

         {ColorMode::kDark,
          ColorProvider::ContentLayerType::kProgressBarColorForeground,
          SkColorSetRGB(0x8A, 0xB4, 0xF8)},
         {ColorMode::kDark,
          ColorProvider::ContentLayerType::kProgressBarColorBackground,
          SkColorSetARGB(0x4C, 0x8A, 0xB4, 0xF8)},

         {ColorMode::kDark,
          ColorProvider::ContentLayerType::kHighlightColorHover,
          SkColorSetARGB(0xD, 0xFF, 0xFF, 0xFF)},

         {ColorMode::kDark,
          ColorProvider::ContentLayerType::kBatterySystemInfoBackgroundColor,
          SkColorSetRGB(0x81, 0xC9, 0x95)},

         {ColorMode::kDark,
          ColorProvider::ContentLayerType::kBatterySystemInfoIconColor,
          SkColorSetRGB(0x20, 0x21, 0x24)},

         {ColorMode::kDark,
          ColorProvider::ContentLayerType::kCaptureRegionColor,
          SkColorSetARGB(0x4C, 0x8A, 0xB4, 0xF8)}}));

}  // namespace

}  // namespace ash
