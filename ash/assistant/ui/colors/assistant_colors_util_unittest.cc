// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/colors/assistant_colors_util.h"

#include "ash/assistant/ui/colors/assistant_colors.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ash {
namespace assistant {

using AssistantColorsUtilUnittest = AshTestBase;

TEST_F(AssistantColorsUtilUnittest, AssistantColor) {
  base::test::ScopedFeatureList scoped_feature_list(
      chromeos::features::kDarkLightMode);
  auto* dark_light_mode_controller = DarkLightModeControllerImpl::Get();
  dark_light_mode_controller->OnActiveUserPrefServiceChanged(
      Shell::Get()->session_controller()->GetActivePrefService());
  const bool initial_dark_mode_status =
      dark_light_mode_controller->IsDarkModeEnabled();

  EXPECT_EQ(
      ResolveAssistantColor(assistant_colors::ColorName::kBgAssistantPlate),
      assistant_colors::ResolveColor(
          assistant_colors::ColorName::kBgAssistantPlate,
          /*is_dark_mode=*/initial_dark_mode_status,
          /*use_debug_colors=*/false));

  // Switch the color mode.
  dark_light_mode_controller->ToggleColorMode();
  ASSERT_NE(initial_dark_mode_status,
            dark_light_mode_controller->IsDarkModeEnabled());

  EXPECT_EQ(
      ResolveAssistantColor(assistant_colors::ColorName::kBgAssistantPlate),
      assistant_colors::ResolveColor(
          assistant_colors::ColorName::kBgAssistantPlate,
          /*is_dark_mode=*/!initial_dark_mode_status,
          /*use_debug_colors=*/false));
}

TEST_F(AssistantColorsUtilUnittest, AssistantColorFlagOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{}, /*disabled_features=*/{
          chromeos::features::kDarkLightMode, features::kNotificationsRefresh});

  // If DarkLightMode is off, the dark mode is on by default.
  EXPECT_EQ(
      ResolveAssistantColor(assistant_colors::ColorName::kBgAssistantPlate),
      assistant_colors::ResolveColor(
          assistant_colors::ColorName::kBgAssistantPlate,
          /*is_dark_mode=*/true,
          /*use_debug_colors=*/false));
}

// ResolveAssistantColor falls back to assistant_colors::ResolveColor with dark
// mode off if the color is not defined in the cc file map and the flag is off.
TEST_F(AssistantColorsUtilUnittest, AssistantColorFlagOffFallback) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{}, /*disabled_features=*/{
          chromeos::features::kDarkLightMode, features::kNotificationsRefresh});

  // If DarkLightMode is off, the dark mode is on by default.
  EXPECT_EQ(ResolveAssistantColor(assistant_colors::ColorName::kGoogleBlue100),
            assistant_colors::ResolveColor(
                assistant_colors::ColorName::kGoogleBlue100,
                /*is_dark_mode=*/true, /*use_debug_colors=*/false));
}

}  // namespace assistant
}  // namespace ash
