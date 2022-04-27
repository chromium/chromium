// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_query_view.h"

#include "ash/assistant/test/assistant_ash_test_base.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ash/assistant/ui/colors/assistant_colors.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/chromeos/styles/cros_styles.h"
#include "ui/compositor/layer.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"

namespace ash {
namespace {

using AssistantQueryViewUnittest = AssistantAshTestBase;

TEST_F(AssistantQueryViewUnittest, ThemeDarkLightMode) {
  base::test::ScopedFeatureList scoped_feature_list(
      chromeos::features::kDarkLightMode);
  auto* color_provider = AshColorProvider::Get();

  color_provider->OnActiveUserPrefServiceChanged(
      Shell::Get()->session_controller()->GetActivePrefService());

  ShowAssistantUi();
  const bool initial_dark_mode_status = color_provider->IsDarkModeEnabled();

  const views::View* query_view =
      page_view()->GetViewByID(AssistantViewID::kQueryView);
  const views::Label* high_confidence_label = static_cast<views::Label*>(
      page_view()->GetViewByID(AssistantViewID::kHighConfidenceLabel));
  const views::Label* low_confidence_label = static_cast<views::Label*>(
      page_view()->GetViewByID(AssistantViewID::kLowConfidenceLabel));

  EXPECT_FALSE(query_view->background());
  ASSERT_TRUE(query_view->layer());
  EXPECT_FALSE(query_view->layer()->fills_bounds_opaquely());
  EXPECT_EQ(high_confidence_label->GetEnabledColor(),
            cros_styles::ResolveColor(cros_styles::ColorName::kTextColorPrimary,
                                      /*is_dark_mode=*/initial_dark_mode_status,
                                      /*use_debug_colors=*/false));
  EXPECT_EQ(
      low_confidence_label->GetEnabledColor(),
      cros_styles::ResolveColor(cros_styles::ColorName::kTextColorSecondary,
                                /*is_dark_mode=*/initial_dark_mode_status,
                                /*use_debug_colors=*/false));

  // Switch the color mode.
  color_provider->ToggleColorMode();
  ASSERT_NE(initial_dark_mode_status, color_provider->IsDarkModeEnabled());

  EXPECT_EQ(
      high_confidence_label->GetEnabledColor(),
      cros_styles::ResolveColor(cros_styles::ColorName::kTextColorPrimary,
                                /*is_dark_mode=*/!initial_dark_mode_status,
                                /*use_debug_colors=*/false));
  EXPECT_EQ(
      low_confidence_label->GetEnabledColor(),
      cros_styles::ResolveColor(cros_styles::ColorName::kTextColorSecondary,
                                /*is_dark_mode=*/!initial_dark_mode_status,
                                /*use_debug_colors=*/false));
}

TEST_F(AssistantQueryViewUnittest, Theme) {
  ASSERT_FALSE(chromeos::features::IsDarkLightModeEnabled());

  // ProductivityLauncher uses DarkLightMode colors.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kProductivityLauncher);

  ShowAssistantUi();

  const views::View* query_view =
      page_view()->GetViewByID(AssistantViewID::kQueryView);
  const views::Label* high_confidence_label = static_cast<views::Label*>(
      page_view()->GetViewByID(AssistantViewID::kHighConfidenceLabel));
  const views::Label* low_confidence_label = static_cast<views::Label*>(
      page_view()->GetViewByID(AssistantViewID::kLowConfidenceLabel));

  EXPECT_FALSE(query_view->background());
  ASSERT_TRUE(query_view->layer());
  EXPECT_FALSE(query_view->layer()->fills_bounds_opaquely());
  EXPECT_EQ(high_confidence_label->GetEnabledColor(), kTextColorPrimary);
  EXPECT_EQ(low_confidence_label->GetEnabledColor(), kTextColorSecondary);

  // Avoid some cleanup during test teardown by explicitly closing the launcher.
  CloseAssistantUi();
}

}  // namespace
}  // namespace ash
