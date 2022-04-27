// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/ui_element_container_view.h"

#include "ash/assistant/assistant_interaction_controller_impl.h"
#include "ash/assistant/test/assistant_ash_test_base.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/assistant/controller/assistant_interaction_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "base/test/scoped_feature_list.h"
#include "cc/base/math_util.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/services/libassistant/public/cpp/assistant_interaction_metadata.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/background.h"
#include "ui/views/view.h"

namespace ash {

namespace {
constexpr char kResponseText[] = "Response";
}

// Use AssistantAshTestBase as we expect that UiElementContainer::OnThemeChanged
// gets called with dark and light mode preference change.
using UiElementContainerViewTest = AssistantAshTestBase;

TEST_F(UiElementContainerViewTest, DarkAndLightTheme) {
  base::test::ScopedFeatureList scoped_feature_list(
      chromeos::features::kDarkLightMode);
  ASSERT_TRUE(chromeos::features::IsDarkLightModeEnabled());

  auto* color_provider = AshColorProvider::Get();
  color_provider->OnActiveUserPrefServiceChanged(
      Shell::Get()->session_controller()->GetActivePrefService());
  const bool initial_dark_mode_status = color_provider->IsDarkModeEnabled();

  ShowAssistantUi();

  views::View* ui_element_container_view =
      page_view()->GetViewByID(kUiElementContainer);
  views::View* indicator =
      ui_element_container_view->GetViewByID(kOverflowIndicator);
  EXPECT_EQ(indicator->GetBackground()->get_color(),
            AshColorProvider::Get()->GetContentLayerColor(
                ColorProvider::ContentLayerType::kSeparatorColor));

  // Switch the color mode.
  color_provider->ToggleColorMode();
  const bool dark_mode_status = color_provider->IsDarkModeEnabled();
  ASSERT_NE(initial_dark_mode_status, dark_mode_status);

  EXPECT_EQ(indicator->GetBackground()->get_color(),
            AshColorProvider::Get()->GetContentLayerColor(
                ColorProvider::ContentLayerType::kSeparatorColor));
}

TEST_F(UiElementContainerViewTest, DarkAndLightModeFlagOff) {
  ASSERT_FALSE(chromeos::features::IsDarkLightModeEnabled());

  // ProductivityLauncher uses DarkLightMode colors.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kProductivityLauncher);

  ShowAssistantUi();

  views::View* ui_element_container_view =
      page_view()->GetViewByID(kUiElementContainer);
  views::View* indicator =
      ui_element_container_view->GetViewByID(kOverflowIndicator);
  EXPECT_EQ(indicator->GetBackground()->get_color(), gfx::kGoogleGrey300);

  // Avoid test teardown issues by explicitly closing the launcher.
  CloseAssistantUi();
}

TEST_F(UiElementContainerViewTest, CustomOverflowIndicator) {
  ShowAssistantUi();

  UiElementContainerView* ui_element_container_view =
      static_cast<UiElementContainerView*>(
          page_view()->GetViewByID(kUiElementContainer));
  views::View* indicator =
      ui_element_container_view->GetViewByID(kOverflowIndicator);

  AssistantInteractionControllerImpl* controller =
      static_cast<AssistantInteractionControllerImpl*>(
          AssistantInteractionController::Get());
  controller->OnInteractionStarted(
      chromeos::assistant::AssistantInteractionMetadata());

  // Add a single text response and confirm that overflow indicator is not
  // visible.
  controller->OnTextResponse(kResponseText);

  ASSERT_LT(ui_element_container_view->content_view()->height(),
            ui_element_container_view->height());
  EXPECT_TRUE(cc::MathUtil::IsWithinEpsilon(
      0.0f, indicator->layer()->GetTargetOpacity()));

  // Add 20 text responses as scroll becomes necessary.
  for (int i = 0; i < 20; ++i) {
    controller->OnTextResponse(kResponseText);
  }

  ASSERT_GT(ui_element_container_view->content_view()->height(),
            ui_element_container_view->height());
  EXPECT_TRUE(cc::MathUtil::IsWithinEpsilon(
      1.0f, indicator->layer()->GetTargetOpacity()));
}

}  // namespace ash
