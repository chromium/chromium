// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/assistant/assistant_dialog_plate.h"

#include "ash/app_list/views/app_list_view.h"
#include "ash/assistant/test/assistant_ash_test_base.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ash/assistant/ui/base/assistant_button.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/textfield/textfield.h"

namespace ash {
namespace {

constexpr int kIconDipSize = 24;

}  // namespace

using AssistantDialogPlateTest = AssistantAshTestBase;

TEST_F(AssistantDialogPlateTest, DarkAndLightTheme) {
  base::test::ScopedFeatureList scoped_feature_list(
      chromeos::features::kDarkLightMode);
  ASSERT_TRUE(chromeos::features::IsDarkLightModeEnabled());

  auto* dark_light_mode_controller = DarkLightModeControllerImpl::Get();
  dark_light_mode_controller->OnActiveUserPrefServiceChanged(
      Shell::Get()->session_controller()->GetActivePrefService());
  const bool initial_dark_mode_status =
      dark_light_mode_controller->IsDarkModeEnabled();

  ShowAssistantUi();

  views::View* assistant_dialog_plate =
      page_view()->GetViewByID(AssistantViewID::kDialogPlate);
  views::Textfield* assistant_text_field = static_cast<views::Textfield*>(
      assistant_dialog_plate->GetViewByID(AssistantViewID::kTextQueryField));
  AssistantButton* keyboard_input_toggle =
      static_cast<AssistantButton*>(assistant_dialog_plate->GetViewByID(
          AssistantViewID::kKeyboardInputToggle));

  const SkBitmap light_keyboard_toggle =
      *gfx::CreateVectorIcon(vector_icons::kKeyboardIcon, kIconDipSize,
                             gfx::kGoogleGrey900)
           .bitmap();
  const SkBitmap dark_keyboard_toggle =
      *gfx::CreateVectorIcon(vector_icons::kKeyboardIcon, kIconDipSize,
                             gfx::kGoogleGrey200)
           .bitmap();
  auto* color_provider = assistant_dialog_plate->GetColorProvider();
  EXPECT_EQ(assistant_text_field->GetTextColor(),
            color_provider->GetColor(kColorAshTextColorPrimary));

  EXPECT_TRUE(gfx::test::AreBitmapsEqual(
      initial_dark_mode_status ? dark_keyboard_toggle : light_keyboard_toggle,
      *keyboard_input_toggle->GetImage(views::Button::STATE_NORMAL).bitmap()));

  // Switch the color mode.
  dark_light_mode_controller->ToggleColorMode();
  color_provider = assistant_dialog_plate->GetColorProvider();
  ASSERT_NE(initial_dark_mode_status,
            dark_light_mode_controller->IsDarkModeEnabled());

  EXPECT_EQ(assistant_text_field->GetTextColor(),
            color_provider->GetColor(kColorAshTextColorPrimary));
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(
      !initial_dark_mode_status ? dark_keyboard_toggle : light_keyboard_toggle,
      *keyboard_input_toggle->GetImage(views::Button::STATE_NORMAL).bitmap()));
}

TEST_F(AssistantDialogPlateTest, DarkAndLightModeFlagOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{}, /*disabled_features=*/{
          chromeos::features::kDarkLightMode, features::kNotificationsRefresh});

  ShowAssistantUi();

  views::View* assistant_dialog_plate =
      page_view()->GetViewByID(AssistantViewID::kDialogPlate);
  views::Textfield* assistant_text_field = static_cast<views::Textfield*>(
      assistant_dialog_plate->GetViewByID(AssistantViewID::kTextQueryField));
  AssistantButton* keyboard_input_toggle =
      static_cast<AssistantButton*>(assistant_dialog_plate->GetViewByID(
          AssistantViewID::kKeyboardInputToggle));

  EXPECT_EQ(assistant_text_field->GetTextColor(),
            assistant_text_field->GetColorProvider()->GetColor(
                cros_tokens::kColorPrimaryDark));

  // When dark light mode is not on, the default color is dark.
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(
      *gfx::CreateVectorIcon(vector_icons::kKeyboardIcon, kIconDipSize,
                             gfx::kGoogleGrey200)
           .bitmap(),
      *keyboard_input_toggle->GetImage(views::Button::STATE_NORMAL).bitmap()));

  // Avoid test teardown issues by explicitly closing the launcher.
  CloseAssistantUi();
}

}  // namespace ash
