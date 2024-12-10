// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/ash_color_provider.h"

#include <ostream>

#include "ash/public/cpp/style/dark_light_mode_controller.h"
#include "ash/test/ash_test_helper.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
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
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI) {}

  void SetUp() override {
    ash_test_helper_.SetUp();
    color_provider_ = AshColorProvider::Get();
  }

  void TearDown() override {
    ash_test_helper_.TearDown();
    color_provider_ = nullptr;
  }

 protected:
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
          ColorProvider::ControlsLayerType::kControlBackgroundColorActive,
          SkColorSetRGB(0x74, 0xD5, 0xE4)},
         {ColorMode::kDark,
          ColorProvider::ControlsLayerType::kControlBackgroundColorInactive,
          SkColorSetARGB(0x1A, 0xFF, 0xFF, 0xFF)},
         {ColorMode::kDark, ColorProvider::ControlsLayerType::kFocusRingColor,
          SkColorSetRGB(0x74, 0xD5, 0xE4)},

         // Light mode
         {ColorMode::kLight,
          ColorProvider::ControlsLayerType::kControlBackgroundColorActive,
          SkColorSetRGB(0x0, 0x68, 0x74)},
         {ColorMode::kLight,
          ColorProvider::ControlsLayerType::kControlBackgroundColorInactive,
          SkColorSetARGB(0x0D, 0x0, 0x0, 0x0)},
         {ColorMode::kLight, ColorProvider::ControlsLayerType::kFocusRingColor,
          SkColorSetRGB(0x0, 0x68, 0x74)}}));

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
         {ColorMode::kLight, ColorProvider::ContentLayerType::kSeparatorColor,
          SkColorSetARGB(0x24, 0x0, 0x0, 0x0)},

         {ColorMode::kLight, ColorProvider::ContentLayerType::kTextColorPrimary,
          SkColorSetRGB(0x17, 0x1D, 0x1E)},
         {ColorMode::kLight,
          ColorProvider::ContentLayerType::kTextColorSecondary,
          SkColorSetRGB(0x3F, 0x48, 0x4A)},
         {ColorMode::kLight, ColorProvider::ContentLayerType::kTextColorURL,
          SkColorSetRGB(0x0, 0x68, 0x74)},

         {ColorMode::kLight, ColorProvider::ContentLayerType::kIconColorPrimary,
          SkColorSetRGB(0x17, 0x1D, 0x1E)},
         {ColorMode::kLight,
          ColorProvider::ContentLayerType::kIconColorSecondary,
          SkColorSetRGB(0x4A, 0x62, 0x67)},
         {ColorMode::kLight, ColorProvider::ContentLayerType::kIconColorAlert,
          SkColorSetRGB(0xD6, 0x43, 0x2F)},
         {ColorMode::kLight, ColorProvider::ContentLayerType::kIconColorWarning,
          SkColorSetRGB(0xA8, 0x69, 0x0)},

         {ColorMode::kLight,
          ColorProvider::ContentLayerType::kIconColorProminent,
          SkColorSetRGB(0x0, 0x68, 0x74)},

         {ColorMode::kLight,
          ColorProvider::ContentLayerType::kIconColorSecondaryBackground,
          SkColorSetRGB(0x3C, 0x40, 0x43)},

         {ColorMode::kLight, ColorProvider::ContentLayerType::kButtonLabelColor,
          SkColorSetRGB(0x17, 0x1D, 0x1E)},

         {ColorMode::kLight,
          ColorProvider::ContentLayerType::kButtonLabelColorBlue,
          SkColorSetRGB(0x0, 0x68, 0x74)},

         {ColorMode::kLight, ColorProvider::ContentLayerType::kButtonIconColor,
          SkColorSetRGB(0x17, 0x1D, 0x1E)},
         {ColorMode::kLight,
          ColorProvider::ContentLayerType::kButtonIconColorPrimary,
          SkColorSetRGB(0xDE, 0xE3, 0xE5)},

         {ColorMode::kLight,
          ColorProvider::ContentLayerType::kSwitchKnobColorActive,
          SkColorSetRGB(0x0, 0x68, 0x74)},
         {ColorMode::kLight,
          ColorProvider::ContentLayerType::kSwitchKnobColorInactive,
          SkColorSetRGB(0xFF, 0xFF, 0xFF)},
         {ColorMode::kLight,
          ColorProvider::ContentLayerType::kSwitchTrackColorActive,
          SkColorSetARGB(0x4C, 0x0, 0x68, 0x74)},
         {ColorMode::kLight,
          ColorProvider::ContentLayerType::kSwitchTrackColorInactive,
          SkColorSetARGB(0x4C, 0x5F, 0x63, 0x68)},

         // Dark colors
         {ColorMode::kDark, ColorProvider::ContentLayerType::kSeparatorColor,
          SkColorSetARGB(0x24, 0xFF, 0xFF, 0xFF)},

         {ColorMode::kDark, ColorProvider::ContentLayerType::kTextColorPrimary,
          SkColorSetRGB(0xDE, 0xE3, 0xE5)},
         {ColorMode::kDark,
          ColorProvider::ContentLayerType::kTextColorSecondary,
          SkColorSetRGB(0xBF, 0xC8, 0xCA)},
         {ColorMode::kDark, ColorProvider::ContentLayerType::kTextColorURL,
          SkColorSetRGB(0x74, 0xD5, 0xE4)},

         {ColorMode::kDark, ColorProvider::ContentLayerType::kIconColorPrimary,
          SkColorSetRGB(0xDE, 0xE3, 0xE5)},
         {ColorMode::kDark,
          ColorProvider::ContentLayerType::kIconColorSecondary,
          SkColorSetRGB(0xB1, 0xCB, 0xD0)},
         {ColorMode::kDark, ColorProvider::ContentLayerType::kIconColorAlert,
          SkColorSetRGB(0xFF, 0xB4, 0xA7)},
         {ColorMode::kDark, ColorProvider::ContentLayerType::kIconColorWarning,
          SkColorSetRGB(0xFF, 0xB9, 0x61)},
         {ColorMode::kDark,
          ColorProvider::ContentLayerType::kIconColorProminent,
          SkColorSetRGB(0x74, 0xD5, 0xE4)},

         {ColorMode::kDark,
          ColorProvider::ContentLayerType::kIconColorSecondaryBackground,
          SkColorSetRGB(0xF1, 0xF3, 0xF4)},

         {ColorMode::kDark, ColorProvider::ContentLayerType::kButtonLabelColor,
          SkColorSetRGB(0xDE, 0xE3, 0xE5)},

         {ColorMode::kDark,
          ColorProvider::ContentLayerType::kButtonLabelColorBlue,
          SkColorSetRGB(0x74, 0xD5, 0xE4)},

         {ColorMode::kDark, ColorProvider::ContentLayerType::kButtonIconColor,
          SkColorSetRGB(0xDE, 0xE3, 0xE5)},
         {ColorMode::kDark,
          ColorProvider::ContentLayerType::kButtonIconColorPrimary,
          SkColorSetRGB(0x17, 0x1D, 0x1E)},

         {ColorMode::kDark,
          ColorProvider::ContentLayerType::kSwitchKnobColorActive,
          SkColorSetRGB(0x74, 0xD5, 0xE4)},
         {ColorMode::kDark,
          ColorProvider::ContentLayerType::kSwitchKnobColorInactive,
          SkColorSetRGB(0xBD, 0xC1, 0xC6)},
         {ColorMode::kDark,
          ColorProvider::ContentLayerType::kSwitchTrackColorActive,
          SkColorSetARGB(0x4C, 0x74, 0xD5, 0xE4)},
         {ColorMode::kDark,
          ColorProvider::ContentLayerType::kSwitchTrackColorInactive,
          SkColorSetARGB(0x4C, 0xE8, 0xEA, 0xED)}}));

}  // namespace

}  // namespace ash
