// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_query_view.h"

#include "ash/assistant/test/assistant_ash_test_base.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ash/assistant/ui/colors/assistant_colors.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "base/feature_list.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"

namespace ash {
namespace {

using AssistantQueryViewUnittest = AssistantAshTestBase;

TEST_F(AssistantQueryViewUnittest, ThemeDarkLightMode) {
  auto* dark_light_mode_controller = DarkLightModeControllerImpl::Get();

  dark_light_mode_controller->OnActiveUserPrefServiceChanged(
      Shell::Get()->session_controller()->GetActivePrefService());

  ShowAssistantUi();
  const bool initial_dark_mode_status =
      dark_light_mode_controller->IsDarkModeEnabled();

  const views::View* query_view =
      page_view()->GetViewByID(AssistantViewID::kQueryView);
  const views::Label* high_confidence_label = static_cast<views::Label*>(
      page_view()->GetViewByID(AssistantViewID::kHighConfidenceLabel));
  const views::Label* low_confidence_label = static_cast<views::Label*>(
      page_view()->GetViewByID(AssistantViewID::kLowConfidenceLabel));

  EXPECT_FALSE(query_view->background());
  ASSERT_TRUE(query_view->layer());
  EXPECT_FALSE(query_view->layer()->fills_bounds_opaquely());
  EXPECT_EQ(
      high_confidence_label->GetEnabledColor(),
      query_view->GetColorProvider()->GetColor(cros_tokens::kTextColorPrimary));
  EXPECT_EQ(
      low_confidence_label->GetEnabledColor(),
      query_view->GetColorProvider()->GetColor(cros_tokens::kColorSecondary));

  // Switch the color mode.
  dark_light_mode_controller->ToggleColorMode();
  ASSERT_NE(initial_dark_mode_status,
            dark_light_mode_controller->IsDarkModeEnabled());

  EXPECT_EQ(
      high_confidence_label->GetEnabledColor(),
      query_view->GetColorProvider()->GetColor(cros_tokens::kTextColorPrimary));
  EXPECT_EQ(
      low_confidence_label->GetEnabledColor(),
      query_view->GetColorProvider()->GetColor(cros_tokens::kColorSecondary));
}

}  // namespace
}  // namespace ash
