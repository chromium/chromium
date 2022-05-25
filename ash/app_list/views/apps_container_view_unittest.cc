// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/apps_container_view.h"

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/continue_section_view.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"

namespace ash {

class AppsContainerViewTest : public AshTestBase {
 public:
  AppsContainerViewTest() {
    // These tests primarily exercise the "hide continue section" behavior.
    features_.InitWithFeatures({features::kProductivityLauncher,
                                features::kLauncherHideContinueSection},
                               {});
  }
  ~AppsContainerViewTest() override = default;

 private:
  base::test::ScopedFeatureList features_;
};

TEST_F(AppsContainerViewTest, ContinueSectionVisibleByDefault) {
  // Show the app list with enough items to make the continue section and
  // recent apps visible.
  auto* helper = GetAppListTestHelper();
  helper->AddContinueSuggestionResults(4);
  helper->AddRecentApps(5);
  helper->AddAppItems(5);
  TabletMode::Get()->SetEnabledForTest(true);

  // The show continue section button is hidden.
  auto* apps_container_view = helper->GetAppsContainerView();
  EXPECT_FALSE(
      apps_container_view->GetShowContinueSectionButtonForTest()->GetVisible());

  // The continue section and recent apps are visible.
  EXPECT_TRUE(helper->GetFullscreenContinueSectionView()->GetVisible());
  EXPECT_TRUE(helper->GetFullscreenRecentAppsView()->GetVisible());
  EXPECT_TRUE(apps_container_view->separator()->GetVisible());
}

TEST_F(AppsContainerViewTest, CanHideContinueSection) {
  // Show the app list with enough items to make the continue section and
  // recent apps visible.
  auto* helper = GetAppListTestHelper();
  helper->AddContinueSuggestionResults(4);
  helper->AddRecentApps(5);
  helper->AddAppItems(5);
  TabletMode::Get()->SetEnabledForTest(true);

  // Hide the continue section.
  Shell::Get()->app_list_controller()->SetHideContinueSection(true);

  // The show continue section button appears.
  auto* apps_container_view = helper->GetAppsContainerView();
  auto* show_continue_section_button =
      apps_container_view->GetShowContinueSectionButtonForTest();
  EXPECT_TRUE(show_continue_section_button->GetVisible());

  // Continue section and recent apps are hidden.
  EXPECT_FALSE(helper->GetFullscreenContinueSectionView()->GetVisible());
  EXPECT_FALSE(helper->GetFullscreenRecentAppsView()->GetVisible());
  EXPECT_FALSE(apps_container_view->separator()->GetVisible());
}

TEST_F(AppsContainerViewTest, CanShowContinueSectionByClickingButton) {
  // Simulate a user with the continue section hidden on startup.
  Shell::Get()->app_list_controller()->SetHideContinueSection(true);

  // Show the app list with enough items to make the continue section and
  // recent apps visible.
  auto* helper = GetAppListTestHelper();
  helper->AddContinueSuggestionResults(4);
  helper->AddRecentApps(5);
  helper->AddAppItems(5);
  TabletMode::Get()->SetEnabledForTest(true);

  // The show continue section button appears.
  auto* apps_container_view = helper->GetAppsContainerView();
  auto* show_continue_section_button =
      apps_container_view->GetShowContinueSectionButtonForTest();
  EXPECT_TRUE(show_continue_section_button->GetVisible());

  // Continue section and recent apps are hidden.
  EXPECT_FALSE(helper->GetFullscreenContinueSectionView()->GetVisible());
  EXPECT_FALSE(helper->GetFullscreenRecentAppsView()->GetVisible());
  EXPECT_FALSE(apps_container_view->separator()->GetVisible());

  // Click the show continue section button.
  LeftClickOn(show_continue_section_button);

  // The button hides.
  EXPECT_FALSE(show_continue_section_button->GetVisible());

  // The continue section and recent apps are visible.
  EXPECT_TRUE(helper->GetFullscreenContinueSectionView()->GetVisible());
  EXPECT_TRUE(helper->GetFullscreenRecentAppsView()->GetVisible());
  EXPECT_TRUE(apps_container_view->separator()->GetVisible());
}

}  // namespace ash
