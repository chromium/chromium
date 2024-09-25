// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_bubble_apps_collections_page.h"

#include <utility>

#include "ash/app_list/apps_collections_controller.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/app_list_bubble_apps_page.h"
#include "ash/app_list/views/app_list_bubble_search_page.h"
#include "ash/app_list/views/app_list_bubble_view.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/app_list_menu_model_adapter.h"
#include "ash/app_list/views/apps_collection_section_view.h"
#include "ash/app_list/views/apps_collections_dismiss_dialog.h"
#include "ash/app_list/views/apps_grid_context_menu.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/app_list/views/search_result_page_anchored_dialog.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/app_menu_constants.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/layer_animation_stopped_waiter.h"
#include "ui/compositor/test/test_utils.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_utils.h"

namespace ash {

class AppListBubbleAppsCollectionsPageTest : public AshTestBase {
 public:
  AppListBubbleAppsCollectionsPageTest() = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures({app_list_features::kAppsCollections},
                                          {});
    AshTestBase::SetUp();
    AppsCollectionsController::Get()->ForceAppsCollectionsForTesting(
        /*force=*/true);
  }

  AppsCollectionSectionView* GetViewForCollection(AppCollection id) {
    views::View* collections_container =
        GetAppListTestHelper()->GetAppCollectionsSectionsContainer();
    for (views::View* child : collections_container->children()) {
      AppsCollectionSectionView* collection =
          views::AsViewClass<AppsCollectionSectionView>(child);
      if (collection->collection() == id) {
        return collection;
      }
    }
    return nullptr;
  }

  AppListItemView* GetAppItemAtIndex(AppsCollectionSectionView* collection,
                                     size_t index) {
    return index < collection->item_views_.view_size()
               ? collection->item_views_.view_at(index)
               : nullptr;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AppListBubbleAppsCollectionsPageTest,
       AppsCollectionsPageVisibleAfterQuicklyClearingSearch) {
  // Open the app list without animation.
  ASSERT_EQ(ui::ScopedAnimationDurationScaleMode::duration_multiplier(),
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  auto* helper = GetAppListTestHelper();
  helper->ShowAppList();

  auto* apps_collections_page = helper->GetBubbleAppsCollectionsPage();
  ASSERT_TRUE(apps_collections_page->GetVisible());

  // Enable animations.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Type a key to trigger the animation to transition to the search page.
  PressAndReleaseKey(ui::VKEY_A);
  ASSERT_TRUE(apps_collections_page->GetPageAnimationLayerForTest()
                  ->GetAnimator()
                  ->is_animating());

  // Before the animation completes, delete the search. This should abort
  // animations, animate back to the apps page, and leave the apps page visible.
  PressAndReleaseKey(ui::VKEY_BACK);
  ui::LayerAnimationStoppedWaiter().Wait(
      apps_collections_page->GetPageAnimationLayerForTest());
  EXPECT_TRUE(apps_collections_page->GetVisible());
  EXPECT_EQ(
      1.0f,
      apps_collections_page->scroll_view()->contents()->layer()->opacity());
}

TEST_F(AppListBubbleAppsCollectionsPageTest, AnimateHidePage) {
  // Open the app list without animation.
  ASSERT_EQ(ui::ScopedAnimationDurationScaleMode::duration_multiplier(),
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  auto* helper = GetAppListTestHelper();
  helper->ShowAppList();

  auto* apps_collections_page = helper->GetBubbleAppsCollectionsPage();
  ASSERT_TRUE(apps_collections_page->GetVisible());

  // Enable animations.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Type a key to trigger the animation to transition to the search page.
  PressAndReleaseKey(ui::VKEY_A);
  ui::Layer* layer = apps_collections_page->GetPageAnimationLayerForTest();
  ui::LayerAnimationStoppedWaiter().Wait(layer);

  // Ensure there is one more frame presented after animation finishes to allow
  // animation throughput data to be passed from cc to ui.
  layer->GetCompositor()->ScheduleFullRedraw();
  EXPECT_TRUE(ui::WaitForNextFrameToBePresented(layer->GetCompositor()));

  // Apps page is not visible.
  EXPECT_FALSE(apps_collections_page->GetVisible());
}

TEST_F(AppListBubbleAppsCollectionsPageTest, AnimateShowPage) {
  // Open the app list without animation.
  ASSERT_EQ(ui::ScopedAnimationDurationScaleMode::duration_multiplier(),
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  auto* helper = GetAppListTestHelper();
  helper->ShowAppList();

  // Type a key switch to the search page.
  PressAndReleaseKey(ui::VKEY_A);

  auto* apps_collections_page = helper->GetBubbleAppsCollectionsPage();
  ASSERT_FALSE(apps_collections_page->GetVisible());

  // Enable animations.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Press escape to trigger animation back to the apps page.
  PressAndReleaseKey(ui::VKEY_ESCAPE);
  ui::Layer* layer = apps_collections_page->GetPageAnimationLayerForTest();
  ui::LayerAnimationStoppedWaiter().Wait(layer);

  // Ensure there is one more frame presented after animation finishes to allow
  // animation throughput data to be passed from cc to ui.
  layer->GetCompositor()->ScheduleFullRedraw();
  EXPECT_TRUE(ui::WaitForNextFrameToBePresented(layer->GetCompositor()));

  // Apps page is visible.
  EXPECT_TRUE(apps_collections_page->GetVisible());
}

TEST_F(AppListBubbleAppsCollectionsPageTest, DismissNudgeIsVisible) {
  // Open the app list without animation.
  ASSERT_EQ(ui::ScopedAnimationDurationScaleMode::duration_multiplier(),
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  auto* helper = GetAppListTestHelper();
  helper->ShowAppList();

  AppListToastContainerView* toast_container =
      helper->GetBubbleAppsCollectionsPage()->GetToastContainerViewForTest();
  EXPECT_TRUE(toast_container->IsToastVisible());
}

TEST_F(AppListBubbleAppsCollectionsPageTest, ShowAppsPageAfterDismissingNudge) {
  // Open the app list without animation.
  ASSERT_EQ(ui::ScopedAnimationDurationScaleMode::duration_multiplier(),
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  auto* helper = GetAppListTestHelper();
  helper->ShowAppList();

  auto* apps_collections_page = helper->GetBubbleAppsCollectionsPage();
  AppListToastContainerView* toast_container =
      apps_collections_page->GetToastContainerViewForTest();
  EXPECT_TRUE(toast_container->IsToastVisible());

  // Click on close button to dismiss the toast.
  LeftClickOn(toast_container->GetToastButton());
  EXPECT_FALSE(toast_container->IsToastVisible());

  // Apps collections page is not visible.
  EXPECT_FALSE(apps_collections_page->GetVisible());
}

TEST_F(AppListBubbleAppsCollectionsPageTest,
       CancelDismissDialogAfterAttempingSort) {
  // Open the app list without animation.
  ASSERT_EQ(ui::ScopedAnimationDurationScaleMode::duration_multiplier(),
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  auto* helper = GetAppListTestHelper();
  helper->ShowAppList();

  auto* apps_collections_page = helper->GetBubbleAppsCollectionsPage();
  AppsGridContextMenu* context_menu =
      apps_collections_page->context_menu_for_test();
  EXPECT_FALSE(context_menu->IsMenuShowing());

  // Get a point in `apps_collections_page` that doesn't have an item on it.
  const gfx::Point empty_space =
      apps_collections_page->GetBoundsInScreen().CenterPoint();

  // Open the menu to test the alphabetical sort option.
  GetEventGenerator()->MoveMouseTo(empty_space);
  GetEventGenerator()->ClickRightButton();
  EXPECT_TRUE(context_menu->IsMenuShowing());

  // Open the Reorder by Name submenu.
  views::MenuItemView* reorder_option =
      context_menu->root_menu_item_view()->GetSubmenu()->GetMenuItemAt(1);
  ASSERT_EQ(reorder_option->title(), u"Name");
  LeftClickOn(reorder_option);

  ASSERT_TRUE(helper->GetBubbleSearchPageDialog());

  views::Widget* widget = helper->GetBubbleSearchPageDialog()->widget();
  views::WidgetDelegate* widget_delegate = widget->widget_delegate();
  views::test::WidgetDestroyedWaiter widget_waiter(widget);
  LeftClickOn(static_cast<AppsCollectionsDismissDialog*>(widget_delegate)
                  ->cancel_button_for_test());
  widget_waiter.Wait();

  // Apps collections page is still visible.
  EXPECT_TRUE(apps_collections_page->GetVisible());
  EXPECT_EQ(AppListSortOrder::kCustom, helper->model()->requested_sort_order());
}

TEST_F(AppListBubbleAppsCollectionsPageTest, ShowAppsPageAfterClearingSearch) {
  // Open the app list without animation.
  ASSERT_EQ(ui::ScopedAnimationDurationScaleMode::duration_multiplier(),
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  auto* helper = GetAppListTestHelper();
  helper->ShowAppList();

  auto* apps_collections_page = helper->GetBubbleAppsCollectionsPage();
  AppListToastContainerView* toast_container =
      apps_collections_page->GetToastContainerViewForTest();
  EXPECT_TRUE(toast_container->IsToastVisible());

  // Click on close button to dismiss the toast.
  LeftClickOn(toast_container->GetToastButton());
  EXPECT_FALSE(toast_container->IsToastVisible());

  // Apps collections page is not visible.
  EXPECT_FALSE(apps_collections_page->GetVisible());
  EXPECT_TRUE(helper->GetBubbleAppsPage()->GetVisible());

  // Start a search query and verify visibility.
  helper->GetBubbleSearchBoxView()->RequestFocus();
  GetEventGenerator()->PressAndReleaseKey(ui::VKEY_A);
  EXPECT_TRUE(helper->GetBubbleSearchPage()->GetVisible());
  EXPECT_FALSE(apps_collections_page->GetVisible());
  EXPECT_FALSE(helper->GetBubbleAppsPage()->GetVisible());

  // Delete the character in the textfield and check visibility.
  GetEventGenerator()->PressAndReleaseKey(ui::VKEY_BACK);
  EXPECT_FALSE(helper->GetBubbleSearchPage()->GetVisible());
  EXPECT_FALSE(apps_collections_page->GetVisible());
  EXPECT_TRUE(helper->GetBubbleAppsPage()->GetVisible());
}

TEST_F(AppListBubbleAppsCollectionsPageTest,
       ShowAppsPageAfterSortingFromAppsGrid) {
  // Open the app list without animation.
  ASSERT_EQ(ui::ScopedAnimationDurationScaleMode::duration_multiplier(),
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  auto* helper = GetAppListTestHelper();
  helper->ShowAppList();

  auto* apps_collections_page = helper->GetBubbleAppsCollectionsPage();
  AppsGridContextMenu* context_menu =
      apps_collections_page->context_menu_for_test();
  EXPECT_FALSE(context_menu->IsMenuShowing());

  // Get a point in `apps_collections_page` that doesn't have an item on it.
  const gfx::Point empty_space =
      apps_collections_page->GetBoundsInScreen().CenterPoint();

  // Open the menu to test the alphabetical sort option.
  GetEventGenerator()->MoveMouseTo(empty_space);
  GetEventGenerator()->ClickRightButton();
  EXPECT_TRUE(context_menu->IsMenuShowing());

  // Open the Reorder by Name submenu.
  views::MenuItemView* reorder_option =
      context_menu->root_menu_item_view()->GetSubmenu()->GetMenuItemAt(1);
  ASSERT_EQ(reorder_option->title(), u"Name");
  LeftClickOn(reorder_option);

  ASSERT_TRUE(helper->GetBubbleSearchPageDialog());

  views::Widget* widget = helper->GetBubbleSearchPageDialog()->widget();
  views::WidgetDelegate* widget_delegate = widget->widget_delegate();
  views::test::WidgetDestroyedWaiter widget_waiter(widget);
  LeftClickOn(static_cast<AppsCollectionsDismissDialog*>(widget_delegate)
                  ->accept_button_for_test());
  widget_waiter.Wait();

  // Apps collections page is not visible.
  EXPECT_FALSE(apps_collections_page->GetVisible());
  EXPECT_EQ(AppListSortOrder::kNameAlphabetical,
            helper->model()->requested_sort_order());
}

// Verifies that a UserAction is recorded for scrolling to the bottom of the
// Apps Grid.
TEST_F(AppListBubbleAppsCollectionsPageTest, ScrollToBottomLogsAction) {
  ui::ScopedAnimationDurationScaleMode scope_duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  auto* helper = GetAppListTestHelper();
  helper->AddAppListItemsWithCollection(AppCollection::kEntertainment, 50);
  helper->ShowAppList();

  auto* apps_collections_page = helper->GetBubbleAppsCollectionsPage();
  base::HistogramTester histograms;

  // Scroll the apps page but do not hit the end.
  views::ScrollView* scroll_view = apps_collections_page->scroll_view();
  scroll_view->ScrollToPosition(scroll_view->vertical_scroll_bar(), 10);

  histograms.ExpectUniqueSample("Apps.AppList.UserAction.ClamshellMode",
                                AppListUserAction::kNavigatedToBottomOfAppList,
                                0);

  // Scroll the apps page to the end.
  scroll_view->ScrollToPosition(scroll_view->vertical_scroll_bar(), INT_MAX);

  histograms.ExpectUniqueSample("Apps.AppList.UserAction.ClamshellMode",
                                AppListUserAction::kNavigatedToBottomOfAppList,
                                1);

  // Scroll upwards and check that the bucket count keeps the same.
  scroll_view->ScrollToPosition(scroll_view->vertical_scroll_bar(), 10);

  histograms.ExpectUniqueSample("Apps.AppList.UserAction.ClamshellMode",
                                AppListUserAction::kNavigatedToBottomOfAppList,
                                1);

  // Scroll the apps page to the end one more time.
  scroll_view->ScrollToPosition(scroll_view->vertical_scroll_bar(), INT_MAX);

  histograms.ExpectUniqueSample("Apps.AppList.UserAction.ClamshellMode",
                                AppListUserAction::kNavigatedToBottomOfAppList,
                                2);
}

// Verifies that a UserAction is recorded for keyboard navigating to the bottom
// of the Apps Grid.
TEST_F(AppListBubbleAppsCollectionsPageTest, KeyboardSelectToBottomLogsAction) {
  ui::ScopedAnimationDurationScaleMode scope_duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Show an app list with enough apps to allow scrolling.
  auto* helper = GetAppListTestHelper();
  helper->AddAppListItemsWithCollection(AppCollection::kEntertainment, 50);
  helper->ShowAppList();
  base::HistogramTester histograms;

  // Verify histogram initial state
  histograms.ExpectUniqueSample("Apps.AppList.UserAction.ClamshellMode",
                                AppListUserAction::kNavigatedToBottomOfAppList,
                                0);

  // Select the last app on the grid with the up arrow.
  GetEventGenerator()->PressAndReleaseKey(ui::VKEY_UP);
  histograms.ExpectUniqueSample("Apps.AppList.UserAction.ClamshellMode",
                                AppListUserAction::kNavigatedToBottomOfAppList,
                                1);

  // Move down twice to return to the top of the grid.
  GetEventGenerator()->PressAndReleaseKey(ui::VKEY_DOWN);
  GetEventGenerator()->PressAndReleaseKey(ui::VKEY_DOWN);
  histograms.ExpectUniqueSample("Apps.AppList.UserAction.ClamshellMode",
                                AppListUserAction::kNavigatedToBottomOfAppList,
                                1);

  // Move to the bottom again and verify that the metric is recorded again.
  GetEventGenerator()->PressAndReleaseKey(ui::VKEY_UP);
  GetEventGenerator()->PressAndReleaseKey(ui::VKEY_UP);
  histograms.ExpectUniqueSample("Apps.AppList.UserAction.ClamshellMode",
                                AppListUserAction::kNavigatedToBottomOfAppList,
                                2);
}

TEST_F(AppListBubbleAppsCollectionsPageTest,
       ShowAppsPageAfterSortingFromAppItem) {
  ASSERT_EQ(ui::ScopedAnimationDurationScaleMode::duration_multiplier(),
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  auto* helper = GetAppListTestHelper();
  helper->AddAppListItemsWithCollection(AppCollection::kEntertainment, 2);
  helper->ShowAppList();

  AppsCollectionSectionView* entertainment_collection =
      GetViewForCollection(AppCollection::kEntertainment);
  ASSERT_TRUE(entertainment_collection);
  ASSERT_EQ(entertainment_collection->GetItemViewCount(), 2u);

  AppListItemView* app_list_item_view =
      GetAppItemAtIndex(entertainment_collection, 0);
  GetEventGenerator()->MoveMouseTo(
      app_list_item_view->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickRightButton();

  AppListMenuModelAdapter* menu_model_adapter =
      app_list_item_view->item_menu_model_adapter();
  ASSERT_TRUE(menu_model_adapter);
  EXPECT_TRUE(menu_model_adapter->IsShowingMenu());

  menu_model_adapter->ExecuteCommand(REORDER_BY_COLOR, 0);
  ASSERT_TRUE(helper->GetBubbleSearchPageDialog());

  views::Widget* widget = helper->GetBubbleSearchPageDialog()->widget();
  views::WidgetDelegate* widget_delegate = widget->widget_delegate();
  views::test::WidgetDestroyedWaiter widget_waiter(widget);
  LeftClickOn(static_cast<AppsCollectionsDismissDialog*>(widget_delegate)
                  ->accept_button_for_test());
  widget_waiter.Wait();

  // Apps collections page is not visible.
  EXPECT_FALSE(helper->GetBubbleAppsCollectionsPage()->GetVisible());
  EXPECT_EQ(AppListSortOrder::kColor, helper->model()->requested_sort_order());
}

TEST_F(AppListBubbleAppsCollectionsPageTest,
       CancelDismissDialogAfterAttempingSortFromAppItem) {
  ASSERT_EQ(ui::ScopedAnimationDurationScaleMode::duration_multiplier(),
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  auto* helper = GetAppListTestHelper();
  helper->AddAppListItemsWithCollection(AppCollection::kEntertainment, 2);
  helper->ShowAppList();

  AppsCollectionSectionView* entertainment_collection =
      GetViewForCollection(AppCollection::kEntertainment);
  ASSERT_TRUE(entertainment_collection);
  ASSERT_EQ(entertainment_collection->GetItemViewCount(), 2u);

  AppListItemView* app_list_item_view =
      GetAppItemAtIndex(entertainment_collection, 0);
  GetEventGenerator()->MoveMouseTo(
      app_list_item_view->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickRightButton();

  ash::AppListMenuModelAdapter* menu_model_adapter =
      app_list_item_view->item_menu_model_adapter();
  ASSERT_TRUE(menu_model_adapter);
  EXPECT_TRUE(menu_model_adapter->IsShowingMenu());

  menu_model_adapter->ExecuteCommand(REORDER_BY_NAME_ALPHABETICAL, 0);
  ASSERT_TRUE(helper->GetBubbleSearchPageDialog());

  views::Widget* widget = helper->GetBubbleSearchPageDialog()->widget();
  views::WidgetDelegate* widget_delegate = widget->widget_delegate();
  views::test::WidgetDestroyedWaiter widget_waiter(widget);
  LeftClickOn(static_cast<AppsCollectionsDismissDialog*>(widget_delegate)
                  ->cancel_button_for_test());
  widget_waiter.Wait();

  // Apps collections page is still visible.
  EXPECT_TRUE(helper->GetBubbleAppsCollectionsPage()->GetVisible());
  EXPECT_EQ(AppListSortOrder::kCustom, helper->model()->requested_sort_order());
}

// Verifies that the metrics for launching the Showoff app by clicking the
// Discovery Chip are recorded.
TEST_F(AppListBubbleAppsCollectionsPageTest, DiscoveryChipLogsMetric) {
  ui::ScopedAnimationDurationScaleMode scope_duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  auto* helper = GetAppListTestHelper();
  helper->AddAppListItemsWithCollection(AppCollection::kEntertainment, 50);
  helper->ShowAppList();

  auto* apps_collections_page = helper->GetBubbleAppsCollectionsPage();
  base::HistogramTester histograms;
  views::ScrollView* scroll_view = apps_collections_page->scroll_view();

  histograms.ExpectUniqueSample("Apps.AppListAppLaunchedV2.BubbleAllApps",
                                AppListLaunchedFrom::kLaunchedFromDiscoveryChip,
                                0);

  // Scroll the apps page to the end.
  scroll_view->ScrollToPosition(scroll_view->vertical_scroll_bar(), INT_MAX);
  GetEventGenerator()->MoveMouseTo(
      apps_collections_page->discovery_chip_for_test()
          ->GetBoundsInScreen()
          .CenterPoint());
  GetEventGenerator()->ClickLeftButton();

  histograms.ExpectUniqueSample("Apps.AppListAppLaunchedV2.BubbleAllApps",
                                AppListLaunchedFrom::kLaunchedFromDiscoveryChip,
                                1);
}

// Verifies that apps visibility is correctly calculated.
TEST_F(AppListBubbleAppsCollectionsPageTest, AppsVisibility) {
  ui::ScopedAnimationDurationScaleMode scope_duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Create enough apps so that the launcher can be scrolled.
  auto* helper = GetAppListTestHelper();
  helper->AddAppListItemsWithCollection(AppCollection::kEntertainment, 25);
  // This will be items with collection kUnknown.
  // kUnknown collection is always above all categories.
  helper->AddAppItems(25);
  helper->ShowAppList();

  auto* apps_collections_page = helper->GetBubbleAppsCollectionsPage();
  views::ScrollView* scroll_view = apps_collections_page->scroll_view();

  ASSERT_NE(scroll_view->GetVisibleBounds(),
            scroll_view->contents()->GetLocalBounds());

  AppsCollectionSectionView* entertainment_collection =
      GetViewForCollection(AppCollection::kEntertainment);
  ASSERT_TRUE(entertainment_collection);
  ASSERT_EQ(entertainment_collection->GetItemViewCount(), 25u);

  AppsCollectionSectionView* unknown_collection =
      GetViewForCollection(AppCollection::kUnknown);
  ASSERT_TRUE(unknown_collection);
  ASSERT_EQ(unknown_collection->GetItemViewCount(), 25u);
  EXPECT_EQ(0, GetTestAppListClient()->activate_item_above_the_fold());
  EXPECT_EQ(0, GetTestAppListClient()->activate_item_below_the_fold());

  AppListItemView* above_the_fold_item =
      GetAppItemAtIndex(unknown_collection, 0);
  GetEventGenerator()->MoveMouseTo(
      above_the_fold_item->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();

  EXPECT_TRUE(apps_collections_page->IsAboveTheFold(above_the_fold_item));
  EXPECT_EQ(1, GetTestAppListClient()->activate_item_above_the_fold());
  EXPECT_EQ(0, GetTestAppListClient()->activate_item_below_the_fold());

  AppListItemView* below_the_fold_item =
      GetAppItemAtIndex(entertainment_collection, 24);

  // Scroll the apps page to the end.
  scroll_view->ScrollToPosition(scroll_view->vertical_scroll_bar(), INT_MAX);
  GetEventGenerator()->MoveMouseTo(
      below_the_fold_item->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();

  EXPECT_FALSE(apps_collections_page->IsAboveTheFold(below_the_fold_item));
  EXPECT_EQ(1, GetTestAppListClient()->activate_item_above_the_fold());
  EXPECT_EQ(1, GetTestAppListClient()->activate_item_below_the_fold());
}

// Verifies that apps visibility is correctly calculated.
TEST_F(AppListBubbleAppsCollectionsPageTest, AppsVisibilityOnShow) {
  // Create enough apps so that the launcher can be scrolled.
  auto* helper = GetAppListTestHelper();
  helper->AddAppListItemsWithCollection(AppCollection::kEntertainment, 50);
  helper->ShowAppList();

  auto* apps_collections_page = helper->GetBubbleAppsCollectionsPage();
  views::ScrollView* scroll_view = apps_collections_page->scroll_view();

  ASSERT_NE(scroll_view->GetVisibleBounds(),
            scroll_view->contents()->GetLocalBounds());

  AppsCollectionSectionView* entertainment_collection =
      GetViewForCollection(AppCollection::kEntertainment);
  ASSERT_TRUE(entertainment_collection);
  ASSERT_EQ(entertainment_collection->GetItemViewCount(), 50u);

  int apps_above = 0;
  int apps_below = 0;

  for (size_t index = 0; index < 50; index++) {
    if (apps_collections_page->IsAboveTheFold(
            GetAppItemAtIndex(entertainment_collection, index))) {
      ++apps_above;
    } else {
      ++apps_below;
    }
  }

  EXPECT_EQ(apps_above, GetTestAppListClient()->items_above_the_fold_count());
  EXPECT_EQ(apps_below, GetTestAppListClient()->items_below_the_fold_count());
}

}  // namespace ash
