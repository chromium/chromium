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

  // The continue section and recent apps are visible.
  EXPECT_TRUE(helper->GetFullscreenContinueSectionView()->GetVisible());
  EXPECT_TRUE(helper->GetFullscreenRecentAppsView()->GetVisible());
  EXPECT_TRUE(helper->GetAppsContainerView()->separator()->GetVisible());
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

  // Continue section and recent apps are hidden.
  EXPECT_FALSE(helper->GetFullscreenContinueSectionView()->GetVisible());
  EXPECT_FALSE(helper->GetFullscreenRecentAppsView()->GetVisible());
  EXPECT_FALSE(helper->GetAppsContainerView()->separator()->GetVisible());
}

TEST_F(AppsContainerViewTest, CanShowContinueSection) {
  // Simulate a user with the continue section hidden on startup.
  Shell::Get()->app_list_controller()->SetHideContinueSection(true);

  // Show the app list with enough items to make the continue section and
  // recent apps visible.
  auto* helper = GetAppListTestHelper();
  helper->AddContinueSuggestionResults(4);
  helper->AddRecentApps(5);
  helper->AddAppItems(5);
  TabletMode::Get()->SetEnabledForTest(true);

  // Continue section and recent apps are hidden.
  EXPECT_FALSE(helper->GetFullscreenContinueSectionView()->GetVisible());
  EXPECT_FALSE(helper->GetFullscreenRecentAppsView()->GetVisible());
  EXPECT_FALSE(helper->GetAppsContainerView()->separator()->GetVisible());

  // Show the continue section.
  Shell::Get()->app_list_controller()->SetHideContinueSection(false);

  // The continue section and recent apps are visible.
  EXPECT_TRUE(helper->GetFullscreenContinueSectionView()->GetVisible());
  EXPECT_TRUE(helper->GetFullscreenRecentAppsView()->GetVisible());
  EXPECT_TRUE(helper->GetAppsContainerView()->separator()->GetVisible());
}

}  // namespace ash
