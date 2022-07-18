// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/ash_color_provider.h"

#include <ostream>

#include "ash/public/cpp/style/dark_light_mode_controller.h"
#include "ash/test/ash_test_helper.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/constants/chromeos_features.h"
#include "testing/gtest/include/gtest/gtest-param-test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/color/color_provider_manager.h"

namespace ash {

namespace {

using ColorMode = ui::ColorProviderManager::ColorMode;

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
      : scoped_feature_list_({chromeos::features::kDarkLightMode}),
        task_environment_(base::test::TaskEnvironment::MainThreadType::UI) {}

  void SetUp() override {
    ash_test_helper_.SetUp();
    color_provider_ = AshColorProvider::Get();
  }

  void TearDown() override {
    ash_test_helper_.TearDown();
    color_provider_ = nullptr;
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
  AshTestHelper ash_test_helper_;
  AshColorProvider* color_provider_;
};

using AshColorProviderBaseLayerTest =
    AshColorProviderBase<ColorProvider::BaseLayerType>;

TEST_P(AshColorProviderBaseLayerTest, TestBaseColors) {
  const auto& test_case = GetParam();
  bool dark = test_case.color_mode == ColorMode::kDark;
  DarkLightModeController::Get()->SetDarkModeEnabledForTest(dark);
  EXPECT_EQ(test_case.expected_color,
            color_provider_->GetBaseLayerColor(test_case.type))
      << "Colors do not match. Expected " << test_case << " Actual: "
      << ColorToString(color_provider_->GetBaseLayerColor(test_case.type));
}

INSTANTIATE_TEST_SUITE_P(
    AshColorProviderTests,
    AshColorProviderBaseLayerTest,
    testing::ValuesIn<ColorsTestCase<ColorProvider::BaseLayerType>>(
        {// Light mode values
         {ColorMode::kLight, ColorProvider::BaseLayerType::kTransparent20,
          SkColorSetARGB(0x33, 0xFF, 0xFF, 0xFF)},
         {ColorMode::kLight, ColorProvider::BaseLayerType::kTransparent40,
          SkColorSetARGB(0x66, 0xFF, 0xFF, 0xFF)},
         {ColorMode::kLight, ColorProvider::BaseLayerType::kTransparent60,
          SkColorSetARGB(0x99, 0xFF, 0xFF, 0xFF)},
         {ColorMode::kLight, ColorProvider::BaseLayerType::kTransparent80,
          SkColorSetARGB(0xCC, 0xFF, 0xFF, 0xFF)},
         {ColorMode::kLight, ColorProvider::BaseLayerType::kTransparent90,
          SkColorSetARGB(0xE6, 0xFF, 0xFF, 0xFF)},
         {ColorMode::kLight, ColorProvider::BaseLayerType::kTransparent95,
          SkColorSetARGB(0xF2, 0xFF, 0xFF, 0xFF)},
         {ColorMode::kLight, ColorProvider::BaseLayerType::kOpaque,
          SkColorSetARGB(0xFF, 0xFF, 0xFF, 0xFF)},

         // Dark mode values
         {ColorMode::kDark, ColorProvider::BaseLayerType::kTransparent20,
          SkColorSetARGB(0x33, 0x20, 0x21, 0x24)},
         {ColorMode::kDark, ColorProvider::BaseLayerType::kTransparent40,
          SkColorSetARGB(0x66, 0x20, 0x21, 0x24)},
         {ColorMode::kDark, ColorProvider::BaseLayerType::kTransparent60,
          SkColorSetARGB(0x99, 0x20, 0x21, 0x24)},
         {ColorMode::kDark, ColorProvider::BaseLayerType::kTransparent80,
          SkColorSetARGB(0xCC, 0x20, 0x21, 0x24)},
         {ColorMode::kDark, ColorProvider::BaseLayerType::kTransparent90,
          SkColorSetARGB(0xE6, 0x20, 0x21, 0x24)},
         {ColorMode::kDark, ColorProvider::BaseLayerType::kTransparent95,
          SkColorSetARGB(0xF2, 0x20, 0x21, 0x24)},
         {ColorMode::kDark, ColorProvider::BaseLayerType::kOpaque,
          SkColorSetARGB(0xFF, 0x20, 0x21, 0x24)}}));

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
         {ColorMode::kDark, ColorProvider::ControlsLayerType::kHighlightColor1,
          SkColorSetARGB(0x14, 0xFF, 0xFF, 0xFF)},
         {ColorMode::kDark, ColorProvider::ControlsLayerType::kHighlightColor2,
          SkColorSetARGB(0x0F, 0xFF, 0xFF, 0xFF)},
         {ColorMode::kDark, ColorProvider::ControlsLayerType::kHighlightColor3,
          SkColorSetARGB(0x14, 0xFF, 0xFF, 0xFF)},
         {ColorMode::kDark, ColorProvider::ControlsLayerType::kBorderColor1,
          SkColorSetARGB(0xCC, 0x20, 0x21, 0x24)},
         {ColorMode::kDark, ColorProvider::ControlsLayerType::kBorderColor2,
          SkColorSetARGB(0x99, 0x20, 0x21, 0x24)},
         {ColorMode::kDark, ColorProvider::ControlsLayerType::kBorderColor3,
          SkColorSetARGB(0x0F, 0x0, 0x0, 0x0)},

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
          SkColorSetRGB(0xF9, 0xAB, 0x0)},
         {ColorMode::kLight,
          ColorProvider::ControlsLayerType::kControlBackgroundColorPositive,
          SkColorSetRGB(0x1E, 0x8E, 0x3E)},
         {ColorMode::kLight, ColorProvider::ControlsLayerType::kFocusAuraColor,
          SkColorSetARGB(0x3D, 0x1A, 0x73, 0xE8)},
         {ColorMode::kLight, ColorProvider::ControlsLayerType::kFocusRingColor,
          SkColorSetRGB(0x1A, 0x73, 0xE8)},
         {ColorMode::kLight, ColorProvider::ControlsLayerType::kHighlightColor1,
          SkColorSetARGB(0x4C, 0xFF, 0xFF, 0xFF)},
         {ColorMode::kLight, ColorProvider::ControlsLayerType::kHighlightColor2,
          SkColorSetARGB(0x33, 0xFF, 0xFF, 0xFF)},
         {ColorMode::kLight, ColorProvider::ControlsLayerType::kHighlightColor3,
          SkColorSetARGB(0x4C, 0xFF, 0xFF, 0xFF)},
         {ColorMode::kLight, ColorProvider::ControlsLayerType::kBorderColor1,
          SkColorSetARGB(0x0F, 0x0, 0x0, 0x0)},
         {ColorMode::kLight, ColorProvider::ControlsLayerType::kBorderColor2,
          SkColorSetARGB(0x0F, 0x0, 0x0, 0x0)},
         {ColorMode::kLight, ColorProvider::ControlsLayerType::kBorderColor3,
          SkColorSetARGB(0x0F, 0x0, 0x0, 0x0)}}));

}  // namespace

}  // namespace ash
