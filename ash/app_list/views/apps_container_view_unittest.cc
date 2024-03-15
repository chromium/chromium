// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/apps_container_view.h"

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/model/app_list_test_model.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/apps_grid_view_test_api.h"
#include "ash/app_list/views/continue_section_view.h"
#include "ash/app_list/views/page_switcher.h"
#include "ash/app_list/views/pagination_model_transition_waiter.h"
#include "ash/app_list/views/recent_apps_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/overview/overview_controller.h"
#include "base/test/metrics/histogram_tester.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/layer_animation_stopped_waiter.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/textfield/textfield.h"

namespace ash {

class AppsContainerViewTest : public AshTestBase {
 public:
  AppsContainerViewTest() = default;
  ~AppsContainerViewTest() override = default;

  void AddFolderWithApps(int count) {
    GetAppListTestHelper()->model()->CreateAndPopulateFolderWithApps(count);
  }

  AppListToastContainerView* GetToastContainerView() {
    return GetAppListTestHelper()
        ->GetAppsContainerView()
        ->GetToastContainerView();
  }

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

  int GetTotalPages() {
    return GetAppListTestHelper()
        ->GetRootPagedAppsGridView()
        ->pagination_model()
        ->total_pages();
  }

  bool HasGradientMask() {
    return !GetAppListTestHelper()
                ->GetAppsContainerView()
                ->scrollable_container_for_test()
                ->layer()
                ->gradient_mask()
                .IsEmpty();
  }
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
  ui::LayerAnimationStoppedWaiter().Wait(last_item->layer());

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

TEST_F(AppsContainerViewTest, OpeningFolderRemovesOtherViewsFromAccessibility) {
  auto* helper = GetAppListTestHelper();
  helper->AddContinueSuggestionResults(4);
  helper->AddRecentApps(5);
  AddFolderWithApps(5);
  TabletMode::Get()->SetEnabledForTest(true);

  // Force the sorting toast to show.
  AppListController::Get()->UpdateAppListWithNewTemporarySortOrder(
      AppListSortOrder::kColor,
      /*animate=*/false, /*update_position_closure=*/base::OnceClosure());
  ASSERT_TRUE(GetToastContainerView()->GetToastButton());

  // Open the folder.
  AppListItemView* folder_item =
      helper->GetRootPagedAppsGridView()->GetItemViewAt(0);
  LeftClickOn(folder_item);

  // Note: For fullscreen app list, the search box is part of the focus cycle
  // when a folder is open.
  auto* continue_section = helper->GetFullscreenContinueSectionView();
  EXPECT_TRUE(continue_section->GetViewAccessibility().GetIsIgnored());
  EXPECT_TRUE(continue_section->GetViewAccessibility().IsLeaf());
  auto* recent_apps = helper->GetFullscreenRecentAppsView();
  EXPECT_TRUE(recent_apps->GetViewAccessibility().GetIsIgnored());
  EXPECT_TRUE(recent_apps->GetViewAccessibility().IsLeaf());
  auto* toast_container = GetToastContainerView();
  EXPECT_TRUE(toast_container->GetViewAccessibility().GetIsIgnored());
  EXPECT_TRUE(toast_container->GetViewAccessibility().IsLeaf());
  auto* apps_grid_view = helper->GetRootPagedAppsGridView();
  EXPECT_TRUE(apps_grid_view->GetViewAccessibility().GetIsIgnored());
  EXPECT_TRUE(apps_grid_view->GetViewAccessibility().IsLeaf());

  // Close the folder.
  PressAndReleaseKey(ui::VKEY_ESCAPE);

  EXPECT_FALSE(continue_section->GetViewAccessibility().GetIsIgnored());
  EXPECT_FALSE(continue_section->GetViewAccessibility().IsLeaf());
  EXPECT_FALSE(recent_apps->GetViewAccessibility().GetIsIgnored());
  EXPECT_FALSE(recent_apps->GetViewAccessibility().IsLeaf());
  EXPECT_FALSE(toast_container->GetViewAccessibility().GetIsIgnored());
  EXPECT_FALSE(toast_container->GetViewAccessibility().IsLeaf());
  EXPECT_FALSE(apps_grid_view->GetViewAccessibility().GetIsIgnored());
  EXPECT_FALSE(apps_grid_view->GetViewAccessibility().IsLeaf());
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

// Test that the gradient mask is created when the page drag begins, and
// destroyed once the page drag has been released and completes.
TEST_F(AppsContainerViewTest, StartPageDragThenRelease) {
  GetAppListTestHelper()->AddAppItems(23);
  TabletMode::Get()->SetEnabledForTest(true);
  auto* apps_grid_view = GetAppListTestHelper()->GetRootPagedAppsGridView();
  test::AppsGridViewTestApi test_api(apps_grid_view);

  EXPECT_FALSE(HasGradientMask());
  EXPECT_EQ(0, GetSelectedPage());
  EXPECT_EQ(2, GetTotalPages());

  PaginationModelTransitionWaiter transition_waiter(
      apps_grid_view->pagination_model());
  gfx::Point start_page_drag = test_api.GetViewAtIndex(GridIndex(0, 0))
                                   ->GetIconBoundsInScreen()
                                   .bottom_right();
  start_page_drag.Offset(10, 0);

  // Begin a touch and drag the page upward.
  auto* generator = GetEventGenerator();
  generator->set_current_screen_location(start_page_drag);
  generator->PressTouch();
  generator->MoveTouchBy(0, -20);

  // Move the touch down a bit so it does not register as a fling to the next
  // page.
  generator->MoveTouchBy(0, 1);

  // Gradient mask should exist during the page drag.
  EXPECT_TRUE(HasGradientMask());

  // End the page drag and wait for the page to animate back to the correct
  // position.
  generator->ReleaseTouch();
  transition_waiter.Wait();

  // The gradient mask should be removed after the end of the page animation.
  EXPECT_FALSE(HasGradientMask());
  EXPECT_EQ(0, GetSelectedPage());
}

TEST_F(AppsContainerViewTest,
       PageSwitcherBoundsShouldNotChangeAfterOverviewMode) {
  TabletMode::Get()->SetEnabledForTest(true);

  auto* helper = GetAppListTestHelper();
  auto* overview_controller = Shell::Get()->overview_controller();

  const auto initial_bounds =
      helper->GetAppsContainerView()->page_switcher()->bounds();

  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  ExitOverview();
  EXPECT_FALSE(overview_controller->InOverviewSession());

  EXPECT_EQ(initial_bounds,
            helper->GetAppsContainerView()->page_switcher()->bounds());
}

// Verify that metrics are recorded when the grid changes page into the last
// page of the app list.
TEST_F(AppsContainerViewTest, NavigateToBottomPageLogsAction) {
  auto* helper = GetAppListTestHelper();
  helper->AddContinueSuggestionResults(4);
  helper->AddRecentApps(5);
  helper->AddAppItems(35);
  TabletMode::Get()->SetEnabledForTest(true);

  auto* apps_grid_view = helper->GetRootPagedAppsGridView();
  base::HistogramTester histograms;

  histograms.ExpectUniqueSample("Apps.AppList.UserAction.TabletMode",
                                AppListUserAction::kNavigatedToBottomOfAppList,
                                0);

  PaginationModel* pagination_model = apps_grid_view->pagination_model();
  int last_page = pagination_model->total_pages() - 1;

  // Select the second to last page. The metric should not be recorded.
  pagination_model->SelectPage(last_page - 1, /*animate=*/false);
  histograms.ExpectUniqueSample("Apps.AppList.UserAction.TabletMode",
                                AppListUserAction::kNavigatedToBottomOfAppList,
                                0);

  // Select the last page. The metric should be recorded.
  pagination_model->SelectPage(last_page, /*animate=*/false);
  histograms.ExpectUniqueSample("Apps.AppList.UserAction.TabletMode",
                                AppListUserAction::kNavigatedToBottomOfAppList,
                                1);

  // Select the second to last page again. The metric should not be recorded.
  pagination_model->SelectPage(last_page - 1, /*animate=*/false);
  histograms.ExpectUniqueSample("Apps.AppList.UserAction.TabletMode",
                                AppListUserAction::kNavigatedToBottomOfAppList,
                                1);

  // Select the last page again. The metric should be recorded one more time.
  pagination_model->SelectPage(last_page, /*animate=*/false);
  histograms.ExpectUniqueSample("Apps.AppList.UserAction.TabletMode",
                                AppListUserAction::kNavigatedToBottomOfAppList,
                                2);
}

}  // namespace ash
