// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_zero_state_view.h"

#include "ash/app_list/views/app_list_toast_view.h"
#include "ash/assistant/test/assistant_ash_test_base.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ash/assistant/ui/colors/assistant_colors.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/services/assistant/public/cpp/features.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/chromeos/styles/cros_styles.h"
#include "ui/views/controls/label.h"

namespace ash {
namespace {

using AssistantZeroStateViewUnittest = AssistantAshTestBase;

TEST_F(AssistantZeroStateViewUnittest, ThemeDarkLightMode) {
  auto* dark_light_mode_controller = DarkLightModeControllerImpl::Get();
  dark_light_mode_controller->OnActiveUserPrefServiceChanged(
      Shell::Get()->session_controller()->GetActivePrefService());
  const bool initial_dark_mode_status =
      dark_light_mode_controller->IsDarkModeEnabled();

  ShowAssistantUi();

  const views::Label* greeting_label = static_cast<views::Label*>(
      page_view()->GetViewByID(AssistantViewID::kGreetingLabel));

  EXPECT_EQ(greeting_label->GetBackgroundColor(),
            assistant_colors::ResolveColor(
                assistant_colors::ColorName::kBgAssistantPlate,
                /*is_dark_mode=*/initial_dark_mode_status,
                /*use_debug_colors=*/false));
  EXPECT_EQ(greeting_label->GetEnabledColor(),
            cros_styles::ResolveColor(cros_styles::ColorName::kTextColorPrimary,
                                      /*is_dark_mode=*/initial_dark_mode_status,
                                      /*use_debug_colors=*/false));
  // Switch the color mode.
  dark_light_mode_controller->ToggleColorMode();
  ASSERT_NE(initial_dark_mode_status,
            dark_light_mode_controller->IsDarkModeEnabled());

  EXPECT_EQ(greeting_label->GetBackgroundColor(),
            assistant_colors::ResolveColor(
                assistant_colors::ColorName::kBgAssistantPlate,
                /*is_dark_mode=*/!initial_dark_mode_status,
                /*use_debug_colors=*/false));
  EXPECT_EQ(
      greeting_label->GetEnabledColor(),
      cros_styles::ResolveColor(cros_styles::ColorName::kTextColorPrimary,
                                /*is_dark_mode=*/!initial_dark_mode_status,
                                /*use_debug_colors=*/false));
}

TEST_F(AssistantZeroStateViewUnittest, ZeroStateViewIsVisible) {
  ShowAssistantUi();

  AssistantZeroStateView* zero_state_view =
      static_cast<AssistantZeroStateView*>(
          page_view()->GetViewByID(AssistantViewID::kZeroStateView));
  ASSERT_TRUE(zero_state_view->GetVisible());
}

TEST_F(AssistantZeroStateViewUnittest, ZeroStateViewIsNotVisibleAfterResponse) {
  ShowAssistantUi();

  AssistantZeroStateView* zero_state_view =
      static_cast<AssistantZeroStateView*>(
          page_view()->GetViewByID(AssistantViewID::kZeroStateView));
  ASSERT_TRUE(zero_state_view->GetVisible());

  MockTextInteraction().WithTextResponse("The response");
  ASSERT_FALSE(zero_state_view->GetVisible());
}

TEST_F(AssistantZeroStateViewUnittest, LearnMoreToastViewIsNotVisible) {
  base::test::ScopedFeatureList feature_list_;
  feature_list_.InitAndDisableFeature(
      assistant::features::kEnableAssistantLearnMore);

  ShowAssistantUi();

  AppListToastView* learn_more_toast = static_cast<AppListToastView*>(
      page_view()->GetViewByID(AssistantViewID::kLearnMoreToast));
  ASSERT_TRUE(learn_more_toast);
  ASSERT_FALSE(learn_more_toast->GetVisible());
  ASSERT_FALSE(learn_more_toast->IsDrawn());
}

TEST_F(AssistantZeroStateViewUnittest, LearnMoreToastViewIsVisible) {
  base::test::ScopedFeatureList scoped_feature_list(
      assistant::features::kEnableAssistantLearnMore);

  ShowAssistantUi();

  AppListToastView* learn_more_toast = static_cast<AppListToastView*>(
      page_view()->GetViewByID(AssistantViewID::kLearnMoreToast));
  ASSERT_TRUE(learn_more_toast);
  ASSERT_TRUE(learn_more_toast->GetVisible());
  ASSERT_TRUE(learn_more_toast->IsDrawn());
}

TEST_F(AssistantZeroStateViewUnittest,
       LearnMoreToastViewIsNotVisibleAfterResponse) {
  base::test::ScopedFeatureList scoped_feature_list(
      assistant::features::kEnableAssistantLearnMore);

  ShowAssistantUi();

  AppListToastView* learn_more_toast = static_cast<AppListToastView*>(
      page_view()->GetViewByID(AssistantViewID::kLearnMoreToast));
  ASSERT_TRUE(learn_more_toast);
  ASSERT_TRUE(learn_more_toast->GetVisible());
  ASSERT_TRUE(learn_more_toast->IsDrawn());

  MockTextInteraction().WithTextResponse("The response");
  ASSERT_TRUE(learn_more_toast->GetVisible());
  ASSERT_FALSE(learn_more_toast->IsDrawn());
}

TEST_F(AssistantZeroStateViewUnittest,
       LearnMoreToastViewIsNotVisible_TabletMode) {
  base::test::ScopedFeatureList feature_list_;
  feature_list_.InitAndDisableFeature(
      assistant::features::kEnableAssistantLearnMore);

  SetNumberOfSessionsWhereOnboardingShown(
      assistant::ui::kOnboardingMaxSessionsShown);
  SetTabletMode(true);
  ShowAssistantUi();

  const views::Label* greeting_label = static_cast<views::Label*>(
      page_view()->GetViewByID(AssistantViewID::kGreetingLabel));
  ASSERT_TRUE(greeting_label);
  ASSERT_TRUE(greeting_label->GetVisible());
  ASSERT_TRUE(greeting_label->IsDrawn());

  AppListToastView* learn_more_toast = static_cast<AppListToastView*>(
      page_view()->GetViewByID(AssistantViewID::kLearnMoreToast));
  ASSERT_TRUE(learn_more_toast);
  ASSERT_FALSE(learn_more_toast->GetVisible());
  ASSERT_FALSE(learn_more_toast->IsDrawn());
}

TEST_F(AssistantZeroStateViewUnittest, LearnMoreToastViewIsVisible_TabletMode) {
  base::test::ScopedFeatureList scoped_feature_list(
      assistant::features::kEnableAssistantLearnMore);

  SetNumberOfSessionsWhereOnboardingShown(
      assistant::ui::kOnboardingMaxSessionsShown);
  SetTabletMode(true);
  ShowAssistantUi();

  const views::Label* greeting_label = static_cast<views::Label*>(
      page_view()->GetViewByID(AssistantViewID::kGreetingLabel));
  ASSERT_TRUE(greeting_label);
  ASSERT_FALSE(greeting_label->GetVisible());
  ASSERT_FALSE(greeting_label->IsDrawn());

  AppListToastView* learn_more_toast = static_cast<AppListToastView*>(
      page_view()->GetViewByID(AssistantViewID::kLearnMoreToast));
  ASSERT_TRUE(learn_more_toast);
  ASSERT_TRUE(learn_more_toast->GetVisible());
  ASSERT_TRUE(learn_more_toast->IsDrawn());
}

TEST_F(AssistantZeroStateViewUnittest,
       LearnMoreToastViewIsNotVisibleAfterResponse_TabletMode) {
  base::test::ScopedFeatureList scoped_feature_list(
      assistant::features::kEnableAssistantLearnMore);

  SetNumberOfSessionsWhereOnboardingShown(
      assistant::ui::kOnboardingMaxSessionsShown);
  SetTabletMode(true);
  ShowAssistantUi();
  // Show Assistant UI in text mode, which is required to set text query.
  TapOnAndWait(keyboard_input_toggle());

  const views::Label* greeting_label = static_cast<views::Label*>(
      page_view()->GetViewByID(AssistantViewID::kGreetingLabel));
  ASSERT_TRUE(greeting_label);
  ASSERT_FALSE(greeting_label->GetVisible());
  ASSERT_FALSE(greeting_label->IsDrawn());

  AppListToastView* learn_more_toast = static_cast<AppListToastView*>(
      page_view()->GetViewByID(AssistantViewID::kLearnMoreToast));
  ASSERT_TRUE(learn_more_toast);
  ASSERT_TRUE(learn_more_toast->GetVisible());
  ASSERT_TRUE(learn_more_toast->IsDrawn());

  MockTextInteraction().WithTextResponse("The response");
  ASSERT_TRUE(learn_more_toast->GetVisible());
  ASSERT_FALSE(learn_more_toast->IsDrawn());
}

TEST_F(AssistantZeroStateViewUnittest, LearnMoreToastTitleLabelMaxWidth) {
  base::test::ScopedFeatureList scoped_feature_list(
      assistant::features::kEnableAssistantLearnMore);

  ShowAssistantUi();

  AppListToastView* learn_more_toast = static_cast<AppListToastView*>(
      page_view()->GetViewByID(AssistantViewID::kLearnMoreToast));
  ASSERT_TRUE(learn_more_toast);
  ASSERT_TRUE(learn_more_toast->GetVisible());

  learn_more_toast->SetTitle(u"Short title single line");
  learn_more_toast->toast_button()->SetText(u"Button with long text");
  views::Label* title_label = learn_more_toast->GetTitleLabelForTesting();
  int max_width_with_long_button_text = title_label->GetMaximumWidth();

  learn_more_toast->toast_button()->SetText(u"text");
  learn_more_toast->SetTitleLabelMaximumWidth();
  int max_width_with_short_button_text = title_label->GetMaximumWidth();
  EXPECT_GT(max_width_with_short_button_text, max_width_with_long_button_text);
}

TEST_F(AssistantZeroStateViewUnittest, ThemeDarkLightModeForToast) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {/*enabled_features=*/assistant::features::kEnableAssistantLearnMore},
      /*disabled_features=*/{});

  auto* dark_light_mode_controller = DarkLightModeControllerImpl::Get();
  dark_light_mode_controller->OnActiveUserPrefServiceChanged(
      Shell::Get()->session_controller()->GetActivePrefService());
  const bool initial_dark_mode_status =
      dark_light_mode_controller->IsDarkModeEnabled();

  ShowAssistantUi();

  AppListToastView* learn_more_toast = static_cast<AppListToastView*>(
      page_view()->GetViewByID(AssistantViewID::kLearnMoreToast));
  ASSERT_TRUE(learn_more_toast);
  ASSERT_TRUE(learn_more_toast->GetVisible());

  views::Label* title_label = learn_more_toast->GetTitleLabelForTesting();

  EXPECT_FALSE(title_label->background());
  EXPECT_EQ(title_label->GetEnabledColor(),
            cros_styles::ResolveColor(cros_styles::ColorName::kTextColorPrimary,
                                      /*is_dark_mode=*/initial_dark_mode_status,
                                      /*use_debug_colors=*/false));

  // Switch the color mode.
  dark_light_mode_controller->ToggleColorMode();
  ASSERT_NE(initial_dark_mode_status,
            dark_light_mode_controller->IsDarkModeEnabled());

  EXPECT_FALSE(title_label->background());
  EXPECT_EQ(
      title_label->GetEnabledColor(),
      cros_styles::ResolveColor(cros_styles::ColorName::kTextColorPrimary,
                                /*is_dark_mode=*/!initial_dark_mode_status,
                                /*use_debug_colors=*/false));
}

}  // namespace
}  // namespace ash
