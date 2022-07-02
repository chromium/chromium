// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/apps_container_view.h"

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/continue_section_view.h"
#include "ash/app_list/views/recent_apps_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/layer_animation_stopped_waiter.h"
#include "base/test/scoped_feature_list.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/textfield/textfield.h"

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

  void PressDown() {
    ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
    generator.PressAndReleaseKey(ui::KeyboardCode::VKEY_DOWN);
  }

  int GetSelectedPage() {
    return GetAppListTestHelper()
        ->GetRootPagedAppsGridView()
        ->pagination_model()
        ->selected_page();
  }

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

TEST_F(AppsContainerViewTest, HideContinueSectionPlaysAnimation) {
  // Show the app list without animation.
  ASSERT_EQ(ui::ScopedAnimationDurationScaleMode::duration_multiplier(),
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  auto* helper = GetAppListTestHelper();
  helper->AddContinueSuggestionResults(4);
  helper->AddRecentApps(5);
  const int item_count = 5;
  helper->AddAppItems(item_count);
  TabletMode::Get()->SetEnabledForTest(true);

  // Enable animations.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Hide the continue section.
  Shell::Get()->app_list_controller()->SetHideContinueSection(true);

  // Animation status is updated.
  auto* apps_grid_view = helper->GetRootPagedAppsGridView();
  EXPECT_EQ(apps_grid_view->grid_animation_status_for_test(),
            AppListGridAnimationStatus::kHideContinueSection);

  // Individial app items are animating their transforms.
  for (int i = 0; i < item_count; ++i) {
    SCOPED_TRACE(testing::Message() << "Item " << i);
    AppListItemView* item = apps_grid_view->GetItemViewAt(i);
    ASSERT_TRUE(item->layer());
    EXPECT_TRUE(item->layer()->GetAnimator()->is_animating());
    EXPECT_TRUE(item->layer()->GetAnimator()->IsAnimatingProperty(
        ui::LayerAnimationElement::TRANSFORM));
  }

  // Wait for the last item's animation to complete.
  AppListItemView* last_item = apps_grid_view->GetItemViewAt(item_count - 1);
  LayerAnimationStoppedWaiter().Wait(last_item->layer());

  // Animation status is updated.
  EXPECT_EQ(apps_grid_view->grid_animation_status_for_test(),
            AppListGridAnimationStatus::kEmpty);

  // Layers have been removed for all items.
  for (int i = 0; i < item_count; ++i) {
    SCOPED_TRACE(testing::Message() << "Item " << i);
    AppListItemView* item = apps_grid_view->GetItemViewAt(i);
    EXPECT_FALSE(item->layer());
  }
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

TEST_F(AppsContainerViewTest, ShowContinueSectionPlaysAnimation) {
  // Simulate a user with the continue section hidden on startup.
  Shell::Get()->app_list_controller()->SetHideContinueSection(true);

  // Show the app list with enough items to make the continue section and
  // recent apps visible.
  auto* helper = GetAppListTestHelper();
  helper->AddContinueSuggestionResults(4);
  helper->AddRecentApps(5);
  helper->AddAppItems(5);
  TabletMode::Get()->SetEnabledForTest(true);

  // Enable animations.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Show the continue section.
  Shell::Get()->app_list_controller()->SetHideContinueSection(false);

  // Continue section is fading in.
  auto* continue_section = helper->GetFullscreenContinueSectionView();
  ASSERT_TRUE(continue_section->layer());
  EXPECT_TRUE(continue_section->layer()->GetAnimator()->is_animating());
  EXPECT_EQ(continue_section->layer()->opacity(), 0.0f);
  EXPECT_EQ(continue_section->layer()->GetTargetOpacity(), 1.0f);

  // Recent apps view is fading in.
  auto* recent_apps = helper->GetFullscreenRecentAppsView();
  ASSERT_TRUE(recent_apps->layer());
  EXPECT_TRUE(recent_apps->layer()->GetAnimator()->is_animating());
  EXPECT_EQ(recent_apps->layer()->opacity(), 0.0f);
  EXPECT_EQ(recent_apps->layer()->GetTargetOpacity(), 1.0f);

  // Separator view is fading in.
  auto* separator = helper->GetAppsContainerView()->separator();
  ASSERT_TRUE(separator->layer());
  EXPECT_TRUE(separator->layer()->GetAnimator()->is_animating());
  EXPECT_EQ(separator->layer()->opacity(), 0.0f);
  EXPECT_EQ(separator->layer()->GetTargetOpacity(), 1.0f);

  // Apps grid is animating its transform.
  auto* apps_grid_view = helper->GetRootPagedAppsGridView();
  ASSERT_TRUE(apps_grid_view->layer());
  EXPECT_TRUE(apps_grid_view->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(apps_grid_view->layer()->GetAnimator()->IsAnimatingProperty(
      ui::LayerAnimationElement::TRANSFORM));
}

TEST_F(AppsContainerViewTest, UpdatesSelectedPageAfterFocusTraversal) {
  auto* helper = GetAppListTestHelper();
  helper->AddRecentApps(5);
  helper->AddAppItems(16);
  TabletMode::Get()->SetEnabledForTest(true);

  auto* apps_grid_view = helper->GetRootPagedAppsGridView();
  auto* recent_apps_view = helper->GetFullscreenRecentAppsView();
  auto* search_box = helper->GetSearchBoxView()->search_box();

  // Focus moves to the search box.
  PressDown();
  EXPECT_TRUE(search_box->HasFocus());
  EXPECT_EQ(GetSelectedPage(), 0);

  // Focus moves to the first item inside `RecentAppsView`.
  PressDown();
  EXPECT_TRUE(recent_apps_view->GetItemViewAt(0)->HasFocus());
  EXPECT_EQ(GetSelectedPage(), 0);

  // Focus moves to the first item / first row inside `PagedAppsGridView`.
  PressDown();
  EXPECT_TRUE(apps_grid_view->GetItemViewAt(0)->HasFocus());
  EXPECT_EQ(GetSelectedPage(), 0);

  // Focus moves to the first item / second row inside `PagedAppsGridView`.
  PressDown();
  EXPECT_TRUE(apps_grid_view->GetItemViewAt(5)->HasFocus());
  EXPECT_EQ(GetSelectedPage(), 0);

  // Focus moves to the first item / third row inside `PagedAppsGridView`.
  PressDown();
  EXPECT_TRUE(apps_grid_view->GetItemViewAt(10)->HasFocus());
  EXPECT_EQ(GetSelectedPage(), 0);

  // Focus moves to the first item / first row on the second page of
  // `PagedAppsGridView`.
  PressDown();
  EXPECT_TRUE(apps_grid_view->GetItemViewAt(15)->HasFocus());
  EXPECT_EQ(GetSelectedPage(), 1);

  // Focus moves to the search box, but second page stays active.
  PressDown();
  EXPECT_TRUE(search_box->HasFocus());
  EXPECT_EQ(GetSelectedPage(), 1);

  // Focus moves to the first item inside `RecentAppsView` and activates first
  // page.
  PressDown();
  EXPECT_TRUE(recent_apps_view->GetItemViewAt(0)->HasFocus());
  EXPECT_EQ(GetSelectedPage(), 0);
}

}  // namespace ash
