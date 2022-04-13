// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/paged_apps_grid_view.h"

#include <utility>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/test/test_focus_change_listener.h"
#include "ash/app_list/views/app_list_a11y_announcer.h"
#include "ash/app_list/views/app_list_toast_container_view.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/apps_container_view.h"
#include "ash/app_list/views/apps_grid_view_test_api.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/pagination/pagination_model.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/textfield/textfield.h"

namespace ash {
namespace {

class PageFlipWaiter : public PaginationModelObserver {
 public:
  explicit PageFlipWaiter(PaginationModel* model) : model_(model) {
    model_->AddObserver(this);
  }
  ~PageFlipWaiter() override { model_->RemoveObserver(this); }

  void Wait() {
    ui_run_loop_ = std::make_unique<base::RunLoop>();
    ui_run_loop_->Run();
  }

 private:
  void SelectedPageChanged(int old_selected, int new_selected) override {
    ui_run_loop_->QuitWhenIdle();
  }

  std::unique_ptr<base::RunLoop> ui_run_loop_;
  PaginationModel* model_ = nullptr;
};

class PagedAppsGridViewTestBase : public AshTestBase {
 public:
  PagedAppsGridViewTestBase()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~PagedAppsGridViewTestBase() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
    grid_test_api_ = std::make_unique<test::AppsGridViewTestApi>(
        GetAppListTestHelper()->GetRootPagedAppsGridView());
  }

  AppListItemView* StartDragOnItemViewAtVisualIndex(int page, int slot) {
    AppListItemView* item = grid_test_api_->GetViewAtVisualIndex(page, slot);
    auto* generator = GetEventGenerator();
    generator->MoveMouseTo(item->GetBoundsInScreen().CenterPoint());
    generator->PressLeftButton();
    item->FireMouseDragTimerForTest();
    return item;
  }

  PagedAppsGridView* GetPagedAppsGridView() {
    return GetAppListTestHelper()->GetRootPagedAppsGridView();
  }

  void UpdateLayout() {
    GetAppListTestHelper()
        ->GetAppsContainerView()
        ->GetWidget()
        ->LayoutRootViewIfNecessary();
  }

  void OnReorderAnimationDone(base::OnceClosure closure,
                              bool aborted,
                              AppListReorderAnimationStatus status) {
    EXPECT_FALSE(aborted);
    EXPECT_EQ(AppListReorderAnimationStatus::kFadeInAnimation, status);
    std::move(closure).Run();
  }

  std::unique_ptr<test::AppsGridViewTestApi> grid_test_api_;
};

// Tests with ProductivityLauncher enabled.
class PagedAppsGridViewTest : public PagedAppsGridViewTestBase {
 public:
  PagedAppsGridViewTest() {
    scoped_features_.InitWithFeatures(
        {features::kProductivityLauncher,
         features::kLauncherDismissButtonsOnSortNudgeAndToast},
        {});
  }

  // Sorts app list with the specified order. If `wait` is true, wait for the
  // reorder animation to complete.
  void SortAppList(const absl::optional<AppListSortOrder>& order, bool wait) {
    AppListController::Get()->UpdateAppListWithNewTemporarySortOrder(
        order,
        /*animate=*/true, /*update_position_closure=*/base::DoNothing());

    if (!wait)
      return;

    base::RunLoop run_loop;
    GetAppListTestHelper()
        ->GetAppsContainerView()
        ->apps_grid_view()
        ->AddReorderCallbackForTest(base::BindRepeating(
            &PagedAppsGridViewTest::OnReorderAnimationDone,
            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  base::test::ScopedFeatureList scoped_features_;
};

// Tests with ProductivityLauncher and app list nudge enabled.
class PagedAppsGridViewWithNudgeTest : public PagedAppsGridViewTest {
 public:
  void SetUp() override {
    PagedAppsGridViewTest::SetUp();
    GetAppListTestHelper()->DisableAppListNudge(false);
    // Update the toast container to make sure the nudge is shown, if required.
    GetAppListTestHelper()
        ->GetAppsContainerView()
        ->toast_container_for_test()
        ->MaybeUpdateReorderNudgeView();
  }
};

// Tests with ProductivityLauncher disabled, which disables the bubble launcher.
class PagedAppsGridViewNonBubbleTest : public PagedAppsGridViewTestBase {
 public:
  PagedAppsGridViewNonBubbleTest() {
    scoped_features_.InitAndDisableFeature(features::kProductivityLauncher);
  }

  base::test::ScopedFeatureList scoped_features_;
};

TEST_F(PagedAppsGridViewNonBubbleTest, CreatePage) {
  PagedAppsGridView* apps_grid_view =
      GetAppListTestHelper()->GetRootPagedAppsGridView();

  // 20 items fills the first page.
  GetAppListTestHelper()->AddAppItems(20);
  EXPECT_EQ(1, apps_grid_view->pagination_model()->total_pages());
  EXPECT_EQ(20, grid_test_api_->AppsOnPage(0));

  // Adding 1 item creates a second page.
  GetAppListTestHelper()->AddAppItems(1);
  EXPECT_EQ(2, apps_grid_view->pagination_model()->total_pages());
  EXPECT_EQ(20, grid_test_api_->AppsOnPage(0));
  EXPECT_EQ(1, grid_test_api_->AppsOnPage(1));
}

// Test that the first page of the root level paged apps grid holds less apps to
// accommodate the recent apps which are show at the top of the first page. Then
// check that the subsequent page holds more apps.
TEST_F(PagedAppsGridViewTest, PageMaxAppCounts) {
  GetAppListTestHelper()->AddAppItems(40);

  // Add some recent apps and re-layout so the first page of the apps grid has
  // less rows to accommodate.
  GetAppListTestHelper()->AddRecentApps(4);
  GetAppListTestHelper()->GetAppsContainerView()->ResetForShowApps();
  UpdateLayout();

  // There should be a total of 40 items in the item list.
  AppListItemList* item_list =
      AppListModelProvider::Get()->model()->top_level_item_list();
  ASSERT_EQ(40u, item_list->item_count());

  // The first page should be maxed at 15 apps, the second page maxed at 20
  // apps, and the third page should hold the leftover 5 apps totalling to 40
  // apps.
  EXPECT_EQ(15, grid_test_api_->AppsOnPage(0));
  EXPECT_EQ(20, grid_test_api_->AppsOnPage(1));
  EXPECT_EQ(5, grid_test_api_->AppsOnPage(2));
}

// Test that the grid dimensions change according to differently sized displays.
// The number of rows should change depending on the display height and the
// first page should most of the time have less rows to accommodate the recents
// apps.
TEST_F(PagedAppsGridViewTest, GridDimensionsChangesWithDisplaySize) {
  // Add some recent apps to take up space on the first page.
  GetAppListTestHelper()->AddAppItems(4);
  GetAppListTestHelper()->AddRecentApps(4);
  GetAppListTestHelper()->GetAppsContainerView()->ResetForShowApps();

  // Test with a display in landscape mode.
  UpdateDisplay("1000x600");
  EXPECT_EQ(3, GetPagedAppsGridView()->GetFirstPageRowsForTesting());
  EXPECT_EQ(4, GetPagedAppsGridView()->GetRowsForTesting());
  EXPECT_EQ(5, GetPagedAppsGridView()->cols());

  // Test with a display in landscape mode with less height. This should have
  // less rows.
  UpdateDisplay("1000x500");
  EXPECT_EQ(2, GetPagedAppsGridView()->GetFirstPageRowsForTesting());
  EXPECT_EQ(3, GetPagedAppsGridView()->GetRowsForTesting());
  EXPECT_EQ(5, GetPagedAppsGridView()->cols());

  // Test with a display in landscape mode with more height. This should have
  // more rows.
  UpdateDisplay("1400x1100");
  EXPECT_EQ(4, GetPagedAppsGridView()->GetFirstPageRowsForTesting());
  EXPECT_EQ(5, GetPagedAppsGridView()->GetRowsForTesting());
  EXPECT_EQ(5, GetPagedAppsGridView()->cols());

  // Test with a display in portrait mode.
  UpdateDisplay("700x1100");
  EXPECT_EQ(4, GetPagedAppsGridView()->GetFirstPageRowsForTesting());
  EXPECT_EQ(5, GetPagedAppsGridView()->GetRowsForTesting());
  EXPECT_EQ(5, GetPagedAppsGridView()->cols());

  // Test with a display in portrait mode with more height. This should have
  // more rows.
  UpdateDisplay("700x1400");
  EXPECT_EQ(5, GetPagedAppsGridView()->GetFirstPageRowsForTesting());
  EXPECT_EQ(7, GetPagedAppsGridView()->GetRowsForTesting());
  EXPECT_EQ(5, GetPagedAppsGridView()->cols());
}

// Test that spacing between pages is removed when the remove empty space flag
// is enabled.
TEST_F(PagedAppsGridViewTest, TestPaging) {
  GetAppListTestHelper()->AddAppItems(1);
  GetAppListTestHelper()->AddPageBreakItem();
  GetAppListTestHelper()->AddAppItems(1);
  GetAppListTestHelper()->AddPageBreakItem();
  GetAppListTestHelper()->AddAppItems(1);

  EXPECT_EQ(1, GetAppListTestHelper()
                   ->GetRootPagedAppsGridView()
                   ->pagination_model()
                   ->total_pages());
  EXPECT_EQ(3, grid_test_api_->AppsOnPage(0));
}

// Test that an app cannot be dragged to create a new page when the remove empty
// space flag is enabled.
TEST_F(PagedAppsGridViewTest, DragItemToNextPage) {
  PaginationModel* pagination_model =
      GetAppListTestHelper()->GetRootPagedAppsGridView()->pagination_model();

  // Populate with enough apps to fill 2 pages.
  GetAppListTestHelper()->AddAppItems(35);
  EXPECT_EQ(2, pagination_model->total_pages());
  GetPagedAppsGridView()->GetWidget()->LayoutRootViewIfNecessary();

  // Drag the item at page 0 slot 0 to the next page.
  StartDragOnItemViewAtVisualIndex(0, 0);
  auto page_flip_waiter = std::make_unique<PageFlipWaiter>(pagination_model);
  const gfx::Rect apps_grid_bounds =
      GetPagedAppsGridView()->GetBoundsInScreen();
  gfx::Point next_page_point =
      gfx::Point(apps_grid_bounds.width() / 2, apps_grid_bounds.bottom() - 1);
  GetEventGenerator()->MoveMouseTo(next_page_point);
  page_flip_waiter->Wait();
  GetEventGenerator()->ReleaseLeftButton();

  // With the drag complete, check that page 1 is now selected.
  EXPECT_EQ(1, pagination_model->selected_page());

  // Drag the item at page 1 slot 0 to the next page and hold it there.
  StartDragOnItemViewAtVisualIndex(1, 0);
  GetEventGenerator()->MoveMouseTo(next_page_point);
  task_environment()->FastForwardBy(base::Seconds(2));
  GetEventGenerator()->ReleaseLeftButton();

  // With the drag complete, check that page 1 is still selected, because a new
  // page cannot be created.
  EXPECT_EQ(1, pagination_model->selected_page());
}

// Test that dragging an app item just above or just below the background card
// of the selected page will trigger a page flip.
TEST_F(PagedAppsGridViewTest, PageFlipBufferSizedByBackgroundCard) {
  PaginationModel* pagination_model =
      GetAppListTestHelper()->GetRootPagedAppsGridView()->pagination_model();

  // Populate with enough apps to fill 2 pages.
  GetAppListTestHelper()->AddAppItems(30);
  EXPECT_EQ(2, pagination_model->total_pages());
  GetPagedAppsGridView()->GetWidget()->LayoutRootViewIfNecessary();
  auto page_flip_waiter = std::make_unique<PageFlipWaiter>(pagination_model);

  // Drag down to the next page.
  StartDragOnItemViewAtVisualIndex(0, 0);
  GetEventGenerator()->MoveMouseBy(10, 10);

  // Test that dragging an item to just past the bottom of the background card
  // causes a page flip.
  gfx::Point bottom_of_card = GetPagedAppsGridView()
                                  ->GetBackgroundCardBoundsForTesting(0)
                                  .bottom_left();
  bottom_of_card.Offset(0, 1);
  views::View::ConvertPointToScreen(GetPagedAppsGridView(), &bottom_of_card);
  GetEventGenerator()->MoveMouseTo(bottom_of_card);
  page_flip_waiter->Wait();
  GetEventGenerator()->ReleaseLeftButton();

  EXPECT_EQ(1, pagination_model->selected_page());

  // Drag up to the previous page.
  StartDragOnItemViewAtVisualIndex(1, 0);
  GetEventGenerator()->MoveMouseBy(10, 10);

  // Test that dragging an item to just past the top of the background card
  // causes a page flip.
  gfx::Point top_of_card =
      GetPagedAppsGridView()->GetBackgroundCardBoundsForTesting(1).origin();
  top_of_card.Offset(0, -1);
  views::View::ConvertPointToScreen(GetPagedAppsGridView(), &top_of_card);
  GetEventGenerator()->MoveMouseTo(top_of_card);
  page_flip_waiter->Wait();
  GetEventGenerator()->ReleaseLeftButton();

  EXPECT_EQ(0, pagination_model->selected_page());
}

// Test that dragging an item to just past the top of the first page
// background card does not cause a page flip.
TEST_F(PagedAppsGridViewTest, NoPageFlipUpOnFirstPage) {
  PaginationModel* pagination_model =
      GetAppListTestHelper()->GetRootPagedAppsGridView()->pagination_model();

  // Populate with enough apps to fill 2 pages.
  GetAppListTestHelper()->AddAppItems(30);
  EXPECT_EQ(2, pagination_model->total_pages());
  GetPagedAppsGridView()->GetWidget()->LayoutRootViewIfNecessary();

  StartDragOnItemViewAtVisualIndex(0, 0);
  GetEventGenerator()->MoveMouseBy(10, 10);

  // Drag an item to just past the top of the first page background card.
  gfx::Point top_of_first_card =
      GetPagedAppsGridView()->GetBackgroundCardBoundsForTesting(0).origin();
  top_of_first_card.Offset(0, -1);

  views::View::ConvertPointToScreen(GetPagedAppsGridView(), &top_of_first_card);
  GetEventGenerator()->MoveMouseTo(top_of_first_card);
  task_environment()->FastForwardBy(base::Seconds(2));
  GetEventGenerator()->ReleaseLeftButton();

  // Selected page should still be at the first page.
  EXPECT_EQ(0, pagination_model->selected_page());
}

// Test that dragging an item to just past the bottom of the last background
// card does not cause a page flip.
TEST_F(PagedAppsGridViewTest, NoPageFlipDownOnLastPage) {
  PaginationModel* pagination_model =
      GetAppListTestHelper()->GetRootPagedAppsGridView()->pagination_model();

  // Populate with enough apps to fill 2 pages.
  GetAppListTestHelper()->AddAppItems(30);
  EXPECT_EQ(2, pagination_model->total_pages());
  GetPagedAppsGridView()->GetWidget()->LayoutRootViewIfNecessary();

  // Select the last page.
  pagination_model->SelectPage(pagination_model->total_pages() - 1, false);
  EXPECT_EQ(1, pagination_model->selected_page());

  StartDragOnItemViewAtVisualIndex(1, 0);
  GetEventGenerator()->MoveMouseBy(10, 10);

  // Drag an item to just past the bottom of the last background card.
  gfx::Point bottom_of_last_card = GetPagedAppsGridView()
                                       ->GetBackgroundCardBoundsForTesting(1)
                                       .bottom_left();
  bottom_of_last_card.Offset(0, 1);
  views::View::ConvertPointToScreen(GetPagedAppsGridView(),
                                    &bottom_of_last_card);
  GetEventGenerator()->MoveMouseTo(bottom_of_last_card);
  task_environment()->FastForwardBy(base::Seconds(2));
  GetEventGenerator()->ReleaseLeftButton();

  // Selected page should not have changed and should still be the last page.
  EXPECT_EQ(1, pagination_model->selected_page());
}

// Test that the first page of the root level paged apps grid holds less apps to
// accommodate the recent apps, which are shown at the top of the first page,
// and the app list nudge, which is shown right above the apps grid view. Then
// check that the subsequent page holds more apps.
TEST_F(PagedAppsGridViewWithNudgeTest, PageMaxAppCounts) {
  GetAppListTestHelper()->AddAppItems(40);

  // Add some recent apps and re-layout so the first page of the apps grid has
  // less rows to accommodate.
  GetAppListTestHelper()->AddRecentApps(4);
  GetAppListTestHelper()->GetAppsContainerView()->ResetForShowApps();
  UpdateLayout();

  // There should be a total of 40 items in the item list.
  AppListItemList* item_list =
      AppListModelProvider::Get()->model()->top_level_item_list();
  ASSERT_EQ(40u, item_list->item_count());

  // With the recent apps and app list reorder nudge, the first page should be
  // maxed at 10 apps, the second page maxed at 20 apps, and the third page
  // should hold the leftover 10 apps totalling to 40 apps.
  EXPECT_EQ(10, grid_test_api_->AppsOnPage(0));
  EXPECT_EQ(20, grid_test_api_->AppsOnPage(1));
  EXPECT_EQ(10, grid_test_api_->AppsOnPage(2));
}

// Test that the grid dimensions change according to differently sized displays.
// The number of rows should change depending on the display height and the
// first page should most of the time have less rows to accommodate the recents
// apps. With the app list nudge enabled in this test, the number of rows
// showing could be less to accommodate the toast nudge.
TEST_F(PagedAppsGridViewWithNudgeTest, GridDimensionsChangesWithDisplaySize) {
  // Add some recent apps to take up space on the first page.
  GetAppListTestHelper()->AddAppItems(4);
  GetAppListTestHelper()->AddRecentApps(4);
  GetAppListTestHelper()->GetAppsContainerView()->ResetForShowApps();

  // Test with a display in landscape mode.
  UpdateDisplay("1000x600");
  EXPECT_EQ(2, GetPagedAppsGridView()->GetFirstPageRowsForTesting());
  EXPECT_EQ(4, GetPagedAppsGridView()->GetRowsForTesting());
  EXPECT_EQ(5, GetPagedAppsGridView()->cols());

  // Test with a display in landscape mode with less height. This should have
  // less rows.
  UpdateDisplay("1000x500");
  EXPECT_EQ(1, GetPagedAppsGridView()->GetFirstPageRowsForTesting());
  EXPECT_EQ(3, GetPagedAppsGridView()->GetRowsForTesting());
  EXPECT_EQ(5, GetPagedAppsGridView()->cols());

  // Test with a display in landscape mode with more height. This should have
  // more rows.
  UpdateDisplay("1400x1100");
  EXPECT_EQ(4, GetPagedAppsGridView()->GetFirstPageRowsForTesting());
  EXPECT_EQ(5, GetPagedAppsGridView()->GetRowsForTesting());
  EXPECT_EQ(5, GetPagedAppsGridView()->cols());

  // Test with a display in portrait mode.
  UpdateDisplay("700x1100");
  EXPECT_EQ(4, GetPagedAppsGridView()->GetFirstPageRowsForTesting());
  EXPECT_EQ(5, GetPagedAppsGridView()->GetRowsForTesting());
  EXPECT_EQ(5, GetPagedAppsGridView()->cols());

  // Test with a display in portrait mode with more height. This should have
  // more rows.
  UpdateDisplay("700x1400");
  EXPECT_EQ(5, GetPagedAppsGridView()->GetFirstPageRowsForTesting());
  EXPECT_EQ(7, GetPagedAppsGridView()->GetRowsForTesting());
  EXPECT_EQ(5, GetPagedAppsGridView()->cols());
}

TEST_F(PagedAppsGridViewTest, SortAppsMakesA11yAnnouncement) {
  auto* helper = GetAppListTestHelper();
  helper->AddAppItems(5);
  helper->GetAppsContainerView()->ResetForShowApps();

  AppsContainerView* container_view = helper->GetAppsContainerView();
  views::View* announcement_view = container_view->toast_container_for_test()
                                       ->a11y_announcer_for_test()
                                       ->announcement_view_for_test();
  ASSERT_TRUE(announcement_view);

  // Add a callback to wait for an accessibility event.
  ax::mojom::Event event = ax::mojom::Event::kNone;
  base::RunLoop run_loop;
  announcement_view->GetViewAccessibility().set_accessibility_events_callback(
      base::BindLambdaForTesting([&](const ui::AXPlatformNodeDelegate* unused,
                                     const ax::mojom::Event event_in) {
        event = event_in;
        run_loop.Quit();
      }));

  // Simulate sorting the apps. Because `run_loop` waits for the a11y event,
  // it is unnecessary to wait for app list sort.
  SortAppList(AppListSortOrder::kNameAlphabetical, /*wait=*/false);

  run_loop.Run();

  // An alert fired with a message.
  EXPECT_EQ(event, ax::mojom::Event::kAlert);
  ui::AXNodeData node_data;
  announcement_view->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_EQ(node_data.GetStringAttribute(ax::mojom::StringAttribute::kName),
            "Apps are sorted by name");
}

// Verifies that sorting app list with an app item focused works as expected.
TEST_F(PagedAppsGridViewTest, SortAppsWithItemFocused) {
  ui::ScopedAnimationDurationScaleMode scope_duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Show an app list with enough apps to create multiple pages.
  auto* helper = GetAppListTestHelper();
  helper->AddAppItems(grid_test_api_->AppsOnPage(0) + 50);
  helper->AddRecentApps(5);
  helper->GetAppsContainerView()->ResetForShowApps();

  PaginationModel* pagination_model =
      helper->GetRootPagedAppsGridView()->pagination_model();
  EXPECT_GT(pagination_model->total_pages(), 1);

  AppsContainerView* container_view = helper->GetAppsContainerView();
  AppListToastContainerView* reorder_undo_toast_container =
      container_view->toast_container_for_test();
  EXPECT_FALSE(reorder_undo_toast_container->is_toast_visible());

  SortAppList(AppListSortOrder::kNameAlphabetical, /*wait=*/true);

  // After sorting, the undo toast should be visible.
  EXPECT_TRUE(reorder_undo_toast_container->is_toast_visible());

  views::View* first_item =
      grid_test_api_->GetViewAtVisualIndex(/*page=*/0, /*slot=*/0);
  first_item->RequestFocus();

  // Install the focus listener before reorder.
  TestFocusChangeListener listener(
      helper->GetRootPagedAppsGridView()->GetFocusManager());

  // Wait until the fade out animation ends.
  {
    AppListController::Get()->UpdateAppListWithNewTemporarySortOrder(
        AppListSortOrder::kColor,
        /*animate=*/true, /*update_position_closure=*/base::DoNothing());

    base::RunLoop run_loop;
    container_view->apps_grid_view()->AddFadeOutAnimationDoneClosureForTest(
        run_loop.QuitClosure());
    run_loop.Run();
  }

  // Verify that the reorder undo toast's layer opacity does not change.
  EXPECT_EQ(1.f, reorder_undo_toast_container->layer()->opacity());

  // Verify that the focus moves twice. It first goes to the search box during
  // the animation and then the undo button on the undo toast after the end of
  // animation.
  EXPECT_EQ(2, listener.focus_change_count());
  EXPECT_FALSE(first_item->HasFocus());
  EXPECT_TRUE(reorder_undo_toast_container->GetToastButton()->HasFocus());

  // Simulate the sort undo by setting the new order to nullopt. The focus
  // should be on the search box after undoing the sort.
  SortAppList(absl::nullopt, /*wait=*/true);
  EXPECT_TRUE(helper->GetSearchBoxView()->search_box()->HasFocus());
}

// Verify on the paged apps grid the undo toast should show after scrolling.
TEST_F(PagedAppsGridViewTest, ScrollToShowUndoToastWhenSorting) {
  ui::ScopedAnimationDurationScaleMode scope_duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Show an app list with enough apps to create multiple pages.
  auto* helper = GetAppListTestHelper();
  helper->AddAppItems(grid_test_api_->AppsOnPage(0) + 50);
  helper->AddRecentApps(5);
  helper->GetAppsContainerView()->ResetForShowApps();

  PaginationModel* pagination_model =
      helper->GetRootPagedAppsGridView()->pagination_model();
  const int total_pages = pagination_model->total_pages();
  EXPECT_GT(total_pages, 1);

  AppsContainerView* container_view =
      GetAppListTestHelper()->GetAppsContainerView();
  AppListToastContainerView* reorder_undo_toast_container =
      container_view->toast_container_for_test();
  EXPECT_FALSE(reorder_undo_toast_container->is_toast_visible());

  SortAppList(AppListSortOrder::kNameAlphabetical, /*wait=*/true);

  // After sorting, the undo toast should be visible.
  EXPECT_TRUE(reorder_undo_toast_container->is_toast_visible());

  pagination_model->SelectPage(total_pages - 1, /*animate=*/false);

  // After selecting the last page, the undo toast should be out of the apps
  // container's view port.
  const gfx::Rect apps_container_screen_bounds =
      container_view->GetBoundsInScreen();
  EXPECT_FALSE(apps_container_screen_bounds.Contains(
      reorder_undo_toast_container->GetBoundsInScreen()));

  SortAppList(AppListSortOrder::kColor, /*wait=*/true);

  // After sorting, the undo toast should still be visible.
  EXPECT_TRUE(reorder_undo_toast_container->is_toast_visible());

  // The undo toast should be within the apps container's view port.
  EXPECT_TRUE(apps_container_screen_bounds.Contains(
      reorder_undo_toast_container->GetBoundsInScreen()));
}

// Test tapping on the close button to dismiss the reorder toast.
TEST_F(PagedAppsGridViewTest, CloseReorderToast) {
  ui::ScopedAnimationDurationScaleMode scope_duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  auto* helper = GetAppListTestHelper();
  helper->AddAppItems(50);
  helper->AddRecentApps(5);
  helper->GetAppsContainerView()->ResetForShowApps();

  AppsContainerView* container_view = helper->GetAppsContainerView();

  // Trigger a sort to show the reorder toast.
  SortAppList(AppListSortOrder::kNameAlphabetical, /*wait=*/true);

  EXPECT_TRUE(
      container_view->toast_container_for_test()->GetToastButton()->HasFocus());
  EXPECT_TRUE(container_view->toast_container_for_test()->is_toast_visible());
  EXPECT_EQ(2, GetPagedAppsGridView()->GetFirstPageRowsForTesting());

  // Tap on the close button to remove the toast.
  gfx::Point close_button_point = container_view->toast_container_for_test()
                                      ->GetCloseButton()
                                      ->GetBoundsInScreen()
                                      .CenterPoint();
  GetEventGenerator()->GestureTapAt(close_button_point);

  // Verify that another row appears once the toast is closed.
  EXPECT_EQ(3, GetPagedAppsGridView()->GetFirstPageRowsForTesting());
  EXPECT_FALSE(container_view->toast_container_for_test()->is_toast_visible());
}

}  // namespace
}  // namespace ash
