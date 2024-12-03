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
#include "ash/style/ash_color_id.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/services/assistant/public/cpp/features.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/chromeos/styles/cros_styles.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
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

  EXPECT_EQ(
      greeting_label->GetBackgroundColor(),
      page_view()->GetColorProvider()->GetColor(kColorAshAssistantBgPlate));
  EXPECT_EQ(greeting_label->GetEnabledColor(),
            page_view()->GetColorProvider()->GetColor(
                kColorAshAssistantTextColorPrimary));
  // Switch the color mode.
  dark_light_mode_controller->ToggleColorMode();
  ASSERT_NE(initial_dark_mode_status,
            dark_light_mode_controller->IsDarkModeEnabled());

  EXPECT_EQ(
      greeting_label->GetBackgroundColor(),
      page_view()->GetColorProvider()->GetColor(kColorAshAssistantBgPlate));
  EXPECT_EQ(greeting_label->GetEnabledColor(),
            page_view()->GetColorProvider()->GetColor(
                kColorAshAssistantTextColorPrimary));
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

TEST_F(AssistantZeroStateViewUnittest, OnboardingViewIsVisible_TabletMode) {
  base::test::ScopedFeatureList scoped_feature_list(
      assistant::features::kEnableAssistantOnboarding);

  SetTabletMode(true);
  ShowAssistantUi();

  // When Launcher Search IPH is enabled, both onboarding and greeting views are
  // not visible in tablet mode.
  const views::View* onboarding_view =
      page_view()->GetViewByID(AssistantViewID::kOnboardingView);
  ASSERT_TRUE(onboarding_view);
  EXPECT_FALSE(onboarding_view->GetVisible());
  EXPECT_FALSE(onboarding_view->IsDrawn());

  const views::View* greeting_label =
      page_view()->GetViewByID(AssistantViewID::kGreetingLabel);
  ASSERT_TRUE(greeting_label);
  EXPECT_FALSE(greeting_label->GetVisible());
  EXPECT_FALSE(greeting_label->IsDrawn());
}

TEST_F(AssistantZeroStateViewUnittest, OnboardingViewIsNotVisible_TabletMode) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      assistant::features::kEnableAssistantOnboarding);

  SetTabletMode(true);
  ShowAssistantUi();

  // When Launcher Search IPH is enabled, both onboarding and greeting views are
  // not visible in tablet mode.
  views::View* onboarding_view =
      page_view()->GetViewByID(AssistantViewID::kOnboardingView);
  ASSERT_TRUE(onboarding_view);
  EXPECT_FALSE(onboarding_view->GetVisible());
  EXPECT_FALSE(onboarding_view->IsDrawn());

  const views::View* greeting_label =
      page_view()->GetViewByID(AssistantViewID::kGreetingLabel);
  ASSERT_TRUE(greeting_label);
  EXPECT_FALSE(greeting_label->GetVisible());
  EXPECT_FALSE(greeting_label->IsDrawn());
}

TEST_F(AssistantZeroStateViewUnittest, OnboardingViewIsVisible) {
  base::test::ScopedFeatureList scoped_feature_list(
      assistant::features::kEnableAssistantOnboarding);

  ShowAssistantUi();

  // The onboarding and greeting views are shown in a mutually exclusive way.
  // An onboarding view should be shown instead of a greeting label.
  const views::View* onboarding_view =
      page_view()->GetViewByID(AssistantViewID::kOnboardingView);
  ASSERT_TRUE(onboarding_view);
  EXPECT_TRUE(onboarding_view->GetVisible());
  EXPECT_TRUE(onboarding_view->IsDrawn());

  const views::View* greeting_label =
      page_view()->GetViewByID(AssistantViewID::kGreetingLabel);
  ASSERT_TRUE(greeting_label);
  EXPECT_FALSE(greeting_label->GetVisible());
  EXPECT_FALSE(greeting_label->IsDrawn());
}

TEST_F(AssistantZeroStateViewUnittest, OnboardingViewIsNotVisible) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      assistant::features::kEnableAssistantOnboarding);

  ShowAssistantUi();

  // The onboarding and greeting views are shown in a mutually exclusive way.
  // A greeting label should be shown instead of an onboarding view.
  views::View* onboarding_view =
      page_view()->GetViewByID(AssistantViewID::kOnboardingView);
  ASSERT_TRUE(onboarding_view);
  EXPECT_FALSE(onboarding_view->GetVisible());
  EXPECT_FALSE(onboarding_view->IsDrawn());

  const views::View* greeting_label =
      page_view()->GetViewByID(AssistantViewID::kGreetingLabel);
  ASSERT_TRUE(greeting_label);
  EXPECT_TRUE(greeting_label->GetVisible());
  EXPECT_TRUE(greeting_label->IsDrawn());
}

TEST_F(AssistantZeroStateViewUnittest, IphViewIsNotVisible) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      feature_engagement::kIPHLauncherSearchHelpUiFeature);

  ShowAssistantUi();

  LauncherSearchIphView* launcher_search_iph =
      static_cast<LauncherSearchIphView*>(
          page_view()->GetViewByID(AssistantViewID::kLauncherSearchIph));
  ASSERT_TRUE(launcher_search_iph);
  ASSERT_FALSE(launcher_search_iph->GetVisible());
  ASSERT_FALSE(launcher_search_iph->IsDrawn());
}

TEST_F(AssistantZeroStateViewUnittest, IphViewIsVisible) {
  base::test::ScopedFeatureList feature_list(
      feature_engagement::kIPHLauncherSearchHelpUiFeature);

  ShowAssistantUi();

  LauncherSearchIphView* launcher_search_iph =
      static_cast<LauncherSearchIphView*>(
          page_view()->GetViewByID(AssistantViewID::kLauncherSearchIph));
  ASSERT_TRUE(launcher_search_iph);
  ASSERT_TRUE(launcher_search_iph->GetVisible());
  ASSERT_TRUE(launcher_search_iph->IsDrawn());
}

TEST_F(AssistantZeroStateViewUnittest, IphViewIsNotVisibleAfterResponse) {
  base::test::ScopedFeatureList feature_list(
      feature_engagement::kIPHLauncherSearchHelpUiFeature);

  ShowAssistantUi();

  LauncherSearchIphView* launcher_search_iph =
      static_cast<LauncherSearchIphView*>(
          page_view()->GetViewByID(AssistantViewID::kLauncherSearchIph));
  ASSERT_TRUE(launcher_search_iph);
  ASSERT_TRUE(launcher_search_iph->GetVisible());
  ASSERT_TRUE(launcher_search_iph->IsDrawn());

  MockTextInteraction().WithTextResponse("The response");
  ASSERT_TRUE(launcher_search_iph->GetVisible());
  ASSERT_FALSE(launcher_search_iph->IsDrawn());
}

TEST_F(AssistantZeroStateViewUnittest, IphViewIsNotVisible_TabletMode) {
  base::test::ScopedFeatureList feature_list_;
  feature_list_.InitAndDisableFeature(
      feature_engagement::kIPHLauncherSearchHelpUiFeature);

  SetNumberOfSessionsWhereOnboardingShown(
      assistant::ui::kOnboardingMaxSessionsShown);
  SetTabletMode(true);
  ShowAssistantUi();

  const views::Label* greeting_label = static_cast<views::Label*>(
      page_view()->GetViewByID(AssistantViewID::kGreetingLabel));
  ASSERT_TRUE(greeting_label);
  ASSERT_TRUE(greeting_label->GetVisible());
  ASSERT_TRUE(greeting_label->IsDrawn());

  LauncherSearchIphView* launcher_search_iph =
      static_cast<LauncherSearchIphView*>(
          page_view()->GetViewByID(AssistantViewID::kLauncherSearchIph));
  ASSERT_TRUE(launcher_search_iph);
  ASSERT_FALSE(launcher_search_iph->GetVisible());
  ASSERT_FALSE(launcher_search_iph->IsDrawn());
}

TEST_F(AssistantZeroStateViewUnittest, IphViewIsVisible_TabletMode) {
  base::test::ScopedFeatureList feature_list(
      feature_engagement::kIPHLauncherSearchHelpUiFeature);

  SetNumberOfSessionsWhereOnboardingShown(
      assistant::ui::kOnboardingMaxSessionsShown);
  SetTabletMode(true);
  ShowAssistantUi();

  const views::Label* greeting_label = static_cast<views::Label*>(
      page_view()->GetViewByID(AssistantViewID::kGreetingLabel));
  ASSERT_TRUE(greeting_label);
  ASSERT_FALSE(greeting_label->GetVisible());
  ASSERT_FALSE(greeting_label->IsDrawn());

  LauncherSearchIphView* launcher_search_iph =
      static_cast<LauncherSearchIphView*>(
          page_view()->GetViewByID(AssistantViewID::kLauncherSearchIph));
  ASSERT_TRUE(launcher_search_iph);
  ASSERT_TRUE(launcher_search_iph->GetVisible());
  ASSERT_TRUE(launcher_search_iph->IsDrawn());
}

TEST_F(AssistantZeroStateViewUnittest,
       IphViewIsNotVisibleAfterResponse_TabletMode) {
  base::test::ScopedFeatureList feature_list(
      feature_engagement::kIPHLauncherSearchHelpUiFeature);

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

  LauncherSearchIphView* launcher_search_iph =
      static_cast<LauncherSearchIphView*>(
          page_view()->GetViewByID(AssistantViewID::kLauncherSearchIph));
  ASSERT_TRUE(launcher_search_iph);
  ASSERT_TRUE(launcher_search_iph->GetVisible());
  ASSERT_TRUE(launcher_search_iph->IsDrawn());

  MockTextInteraction().WithTextResponse("The response");
  ASSERT_TRUE(launcher_search_iph->GetVisible());
  ASSERT_FALSE(launcher_search_iph->IsDrawn());
}

}  // namespace
}  // namespace ash
