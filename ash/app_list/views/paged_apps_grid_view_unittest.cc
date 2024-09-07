// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/paged_apps_grid_view.h"

#include <list>
#include <utility>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/apps_grid_row_change_animator.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/test/test_focus_change_listener.h"
#include "ash/app_list/views/app_list_folder_view.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/app_list_toast_container_view.h"
#include "ash/app_list/views/app_list_toast_view.h"
#include "ash/app_list/views/apps_container_view.h"
#include "ash/app_list/views/apps_grid_view_test_api.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/pagination/pagination_model.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/layer_animation_stopped_waiter.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/bounds_animator.h"
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
  raw_ptr<PaginationModel> model_ = nullptr;
};

}  // namespace

class PagedAppsGridViewTest : public AshTestBase {
 public:
  PagedAppsGridViewTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~PagedAppsGridViewTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    ash::TabletModeControllerTestApi().EnterTabletMode();
    grid_test_api_ = std::make_unique<test::AppsGridViewTestApi>(
        GetAppListTestHelper()->GetRootPagedAppsGridView());
  }

  AppListItemView* StartDragOnItemView(AppListItemView* item) {
    auto* generator = GetEventGenerator();
    generator->MoveMouseTo(item->GetBoundsInScreen().CenterPoint());
    generator->PressLeftButton();
    EXPECT_TRUE(item->FireMouseDragTimerForTest());
    return item;
  }

  AppListItemView* StartDragOnItemViewAtVisualIndex(int page, int slot) {
    return StartDragOnItemView(
        grid_test_api_->GetViewAtVisualIndex(page, slot));
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
                              AppListGridAnimationStatus status) {
    EXPECT_FALSE(aborted);
    EXPECT_EQ(AppListGridAnimationStatus::kReorderFadeIn, status);
    std::move(closure).Run();
  }

  int GetNumberOfRowChangeLayersForTest() {
    return GetPagedAppsGridView()
        ->row_change_animator_->GetNumberOfRowChangeLayersForTest();
  }

  bool IsRowChangeAnimatorAnimating() {
    return GetPagedAppsGridView()->row_change_animator_->IsAnimating();
  }

  void WaitForItemLayerAnimations() {
    ui::LayerAnimationStoppedWaiter animation_waiter;
    const views::ViewModelT<AppListItemView>* view_model =
        GetPagedAppsGridView()->view_model();

    for (size_t i = 0; i < view_model->view_size(); i++) {
      if (view_model->view_at(i)->layer())
        animation_waiter.Wait(view_model->view_at(i)->layer());
    }
  }

  // Sorts app list with the specified order. If `wait` is true, wait for the
  // reorder animation to complete.
  void SortAppList(const std::optional<AppListSortOrder>& order, bool wait) {
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

  std::unique_ptr<test::AppsGridViewTestApi> grid_test_api_;
};

// Tests with app list nudge enabled.
class PagedAppsGridViewWithNudgeTest : public PagedAppsGridViewTest {
 public:
  void SetUp() override {
    PagedAppsGridViewTest::SetUp();
    GetAppListTestHelper()->DisableAppListNudge(false);
    // Update the toast container to make sure the nudge is shown, if required.
    GetAppListTestHelper()
        ->GetAppsContainerView()
        ->toast_container()
        ->MaybeUpdateReorderNudgeView();
  }
};

TEST_F(PagedAppsGridViewTest, CreatePage) {
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

  // Test with a display in landscape mode a with a little more height. This
  // should have equal rows on the first and second pages.
  UpdateDisplay("1600x910");
  EXPECT_EQ(4, GetPagedAppsGridView()->GetFirstPageRowsForTesting());
  EXPECT_EQ(4, GetPagedAppsGridView()->GetRowsForTesting());
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
  EXPECT_EQ(6, GetPagedAppsGridView()->GetRowsForTesting());
  EXPECT_EQ(5, GetPagedAppsGridView()->cols());
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

  const gfx::Rect apps_grid_bounds =
      GetPagedAppsGridView()->GetBoundsInScreen();
  gfx::Point next_page_point =
      gfx::Point(apps_grid_bounds.width() / 2, apps_grid_bounds.bottom() - 1);
  auto page_flip_waiter = std::make_unique<PageFlipWaiter>(pagination_model);

  // Drag the item at page 0 slot 0 to the next page.
  StartDragOnItemViewAtVisualIndex(0, 0);

  // Test that dragging an item to just past the bottom of the background
  // card causes a page flip.
  std::list<base::OnceClosure> drag_page_flip;
  drag_page_flip.push_back(base::BindLambdaForTesting([&]() {
    // Start cardified  state.
    GetEventGenerator()->MoveMouseBy(10, 10);
    GetEventGenerator()->MoveMouseTo(next_page_point);
  }));
  drag_page_flip.push_back(base::BindLambdaForTesting([&]() {
    page_flip_waiter->Wait();
    GetEventGenerator()->ReleaseLeftButton();
  }));
  MaybeRunDragAndDropSequenceForAppList(&drag_page_flip, /*is_touch=*/false);

  // With the drag complete, check that page 1 is now selected.
  EXPECT_EQ(1, pagination_model->selected_page());

  // Drag the item at page 1 slot 0 to the next page and hold it there.
  StartDragOnItemViewAtVisualIndex(1, 0);

  std::list<base::OnceClosure> drag_does_nothing;
  drag_does_nothing.push_back(base::BindLambdaForTesting([&]() {
    // Start cardified  state.
    GetEventGenerator()->MoveMouseBy(10, 10);
    GetEventGenerator()->MoveMouseTo(next_page_point);
  }));
  drag_does_nothing.push_back(base::BindLambdaForTesting([&]() {
    task_environment()->FastForwardBy(base::Seconds(2));
    GetEventGenerator()->ReleaseLeftButton();
  }));
  MaybeRunDragAndDropSequenceForAppList(&drag_does_nothing, /*is_touch=*/false);

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

  std::list<base::OnceClosure> drag_page_flip_down;
  // Test that dragging an item to just past the bottom of the background
  // card causes a page flip.
  drag_page_flip_down.push_back(base::BindLambdaForTesting([&]() {
    // Start cardified  state.
    GetEventGenerator()->MoveMouseBy(10, 10);
    gfx::Point bottom_of_card = GetPagedAppsGridView()
                                    ->GetBackgroundCardBoundsForTesting(0)
                                    .bottom_left();
    bottom_of_card.Offset(0, 1);
    views::View::ConvertPointToScreen(GetPagedAppsGridView(), &bottom_of_card);
    GetEventGenerator()->MoveMouseTo(bottom_of_card);
  }));
  drag_page_flip_down.push_back(base::BindLambdaForTesting([&]() {
    page_flip_waiter->Wait();
    GetEventGenerator()->ReleaseLeftButton();
  }));
  MaybeRunDragAndDropSequenceForAppList(&drag_page_flip_down,
                                        /*is_touch=*/false);

  EXPECT_EQ(1, pagination_model->selected_page());

  // Drag up to the previous page.
  StartDragOnItemViewAtVisualIndex(1, 0);

  std::list<base::OnceClosure> drag_page_flip_top;
  // Test that dragging an item to just past the top of the background
  // card causes a page flip.
  drag_page_flip_top.push_back(base::BindLambdaForTesting([&]() {
    // Start cardified  state.
    GetEventGenerator()->MoveMouseBy(10, 10);
    gfx::Point top_of_card =
        GetPagedAppsGridView()->GetBackgroundCardBoundsForTesting(1).origin();
    top_of_card.Offset(0, -1);
    views::View::ConvertPointToScreen(GetPagedAppsGridView(), &top_of_card);
    GetEventGenerator()->MoveMouseTo(top_of_card);
  }));
  drag_page_flip_top.push_back(base::BindLambdaForTesting([&]() {
    page_flip_waiter->Wait();
    GetEventGenerator()->ReleaseLeftButton();
  }));
  MaybeRunDragAndDropSequenceForAppList(&drag_page_flip_top,
                                        /*is_touch=*/false);

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

  // Drag down to the next page.
  StartDragOnItemViewAtVisualIndex(0, 0);

  std::list<base::OnceClosure> tasks;
  // Drag an item to just past the top of the first page background card.
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Start cardified  state.
    GetEventGenerator()->MoveMouseBy(10, 10);
    gfx::Point top_of_first_card =
        GetPagedAppsGridView()->GetBackgroundCardBoundsForTesting(0).origin();
    top_of_first_card.Offset(0, -1);

    views::View::ConvertPointToScreen(GetPagedAppsGridView(),
                                      &top_of_first_card);
    GetEventGenerator()->MoveMouseTo(top_of_first_card);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    task_environment()->FastForwardBy(base::Seconds(2));
    GetEventGenerator()->ReleaseLeftButton();
  }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch=*/false);

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

  // Drag down to the next page.
  StartDragOnItemViewAtVisualIndex(1, 0);

  std::list<base::OnceClosure> tasks;
  // Drag an item to just past the bottom of the last background card.
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Start cardified  state.
    GetEventGenerator()->MoveMouseBy(10, 10);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    gfx::Point bottom_of_last_card = GetPagedAppsGridView()
                                         ->GetBackgroundCardBoundsForTesting(1)
                                         .bottom_left();
    bottom_of_last_card.Offset(0, 1);
    views::View::ConvertPointToScreen(GetPagedAppsGridView(),
                                      &bottom_of_last_card);
    GetEventGenerator()->MoveMouseTo(bottom_of_last_card);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    task_environment()->FastForwardBy(base::Seconds(2));
    GetEventGenerator()->ReleaseLeftButton();
  }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch=*/false);

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
  EXPECT_EQ(6, GetPagedAppsGridView()->GetRowsForTesting());
  EXPECT_EQ(5, GetPagedAppsGridView()->cols());
}

TEST_F(PagedAppsGridViewTest, SortAppsMakesA11yAnnouncement) {
  auto* helper = GetAppListTestHelper();
  helper->AddAppItems(5);
  helper->GetAppsContainerView()->ResetForShowApps();

  views::View* announcement_view = helper->GetAccessibilityAnnounceView();
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
      container_view->toast_container();
  EXPECT_FALSE(reorder_undo_toast_container->IsToastVisible());

  SortAppList(AppListSortOrder::kNameAlphabetical, /*wait=*/true);

  // After sorting, the undo toast should be visible.
  EXPECT_TRUE(reorder_undo_toast_container->IsToastVisible());

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
  SortAppList(std::nullopt, /*wait=*/true);
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
      container_view->toast_container();
  EXPECT_FALSE(reorder_undo_toast_container->IsToastVisible());

  SortAppList(AppListSortOrder::kNameAlphabetical, /*wait=*/true);

  // After sorting, the undo toast should be visible.
  EXPECT_TRUE(reorder_undo_toast_container->IsToastVisible());

  pagination_model->SelectPage(total_pages - 1, /*animate=*/false);

  // After selecting the last page, the undo toast should be out of the apps
  // container's view port.
  const gfx::Rect apps_container_screen_bounds =
      container_view->GetBoundsInScreen();
  EXPECT_FALSE(apps_container_screen_bounds.Contains(
      reorder_undo_toast_container->GetBoundsInScreen()));

  SortAppList(AppListSortOrder::kColor, /*wait=*/true);

  // After sorting, the undo toast should still be visible.
  EXPECT_TRUE(reorder_undo_toast_container->IsToastVisible());

  // The undo toast should be within the apps container's view port.
  EXPECT_TRUE(apps_container_screen_bounds.Contains(
      reorder_undo_toast_container->GetBoundsInScreen()));
}

// Test tapping on the close button to dismiss the reorder toast. Also make sure
// that items animate upward to take the place of the closed toast.
TEST_F(PagedAppsGridViewTest, CloseReorderToast) {
  ui::ScopedAnimationDurationScaleMode scope_duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  auto* helper = GetAppListTestHelper();
  helper->AddAppItems(50);
  helper->AddRecentApps(5);
  helper->GetAppsContainerView()->ResetForShowApps();

  // Trigger a sort to show the reorder toast.
  SortAppList(AppListSortOrder::kNameAlphabetical, /*wait=*/true);

  AppListToastContainerView* toast_container =
      helper->GetAppsContainerView()->toast_container();
  EXPECT_TRUE(toast_container->GetToastButton()->HasFocus());
  EXPECT_TRUE(toast_container->IsToastVisible());
  EXPECT_EQ(2, GetPagedAppsGridView()->GetFirstPageRowsForTesting());

  // Tap on the close button to remove the toast.
  GestureTapOn(toast_container->GetCloseButton());

  // Wait for the toast to finish fade out animation.
  EXPECT_EQ(toast_container->toast_view()->layer()->GetTargetOpacity(), 0.0f);
  ui::LayerAnimationStoppedWaiter().Wait(
      toast_container->toast_view()->layer());

  const views::ViewModelT<AppListItemView>* view_model =
      GetPagedAppsGridView()->view_model();

  // Item views should animate upwards to take the place of the closed reorder
  // toast.
  for (size_t i = 1; i < view_model->view_size(); i++) {
    AppListItemView* item_view = view_model->view_at(i);
    // The items off screen on the second page should not animate.
    if (i >= grid_test_api_->TilesPerPageInPagedGrid(0)) {
      EXPECT_FALSE(GetPagedAppsGridView()->IsAnimatingView(item_view));
      continue;
    }

    // Make sure that no between rows animation is occurring by checking that
    // all items are animating upward vertically and not horizontally.
    EXPECT_TRUE(GetPagedAppsGridView()->IsAnimatingView(item_view));
    gfx::RectF bounds(item_view->GetMirroredBounds());
    bounds = item_view->layer()->transform().MapRect(bounds);
    gfx::Rect current_bounds_in_animation = gfx::ToRoundedRect(bounds);
    EXPECT_GT(current_bounds_in_animation.y(), item_view->bounds().y());
    EXPECT_EQ(current_bounds_in_animation.x(), item_view->bounds().x());
  }

  // Verify that another row appears once the toast is closed.
  EXPECT_EQ(3, GetPagedAppsGridView()->GetFirstPageRowsForTesting());
  EXPECT_FALSE(toast_container->IsToastVisible());
}

// Test that when quickly dragging and removing the last item from a folder, the
// item view layers which are created when entering cardified state are
// destroyed once the exit cardified item animations are complete.
TEST_F(PagedAppsGridViewTest, DestroyLayersOnDragLastItemFromFolder) {
  ui::ScopedAnimationDurationScaleMode scope_duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  GetAppListTestHelper()->model()->CreateSingleItemFolder("folder_id",
                                                          "Item_0");
  GetAppListTestHelper()->model()->PopulateApps(5);
  UpdateLayout();

  auto* generator = GetEventGenerator();
  auto* helper = GetAppListTestHelper();

  GetPagedAppsGridView()->SetCardifiedStateEndedTestCallback(
      base::BindLambdaForTesting(
          [&]() { LOG(ERROR) << "wowee TESTING OnCardifiedStateEnded!!!"; }));

  // Open the folder.
  EXPECT_TRUE(GetPagedAppsGridView()->GetItemViewAt(0)->is_folder());
  LeftClickOn(GetPagedAppsGridView()->GetItemViewAt(0));
  ASSERT_TRUE(helper->IsInFolderView());

  // Wait for folder opening animations to complete.
  base::RunLoop folder_animation_waiter;
  helper->GetAppsContainerView()
      ->app_list_folder_view()
      ->SetAnimationDoneTestCallback(base::BindLambdaForTesting(
          [&]() { folder_animation_waiter.Quit(); }));
  folder_animation_waiter.Run();

  AppListItemView* item_view = helper->GetFullscreenFolderView()
                                   ->items_grid_view()
                                   ->view_model()
                                   ->view_at(0);

  StartDragOnItemView(item_view);

  std::list<base::OnceClosure> tasks;
  // Move the mouse outside of the folder.
  tasks.push_back(base::BindLambdaForTesting([&]() {
    generator->MoveMouseTo(helper->GetAppsContainerView()
                               ->app_list_folder_view()
                               ->GetBoundsInScreen()
                               .bottom_center() +
                           gfx::Vector2d(0, item_view->height()));
      // Generate OnDragExit() event for the folder apps grid view.
      generator->MoveMouseBy(10, 10);
    GetEventGenerator()->ReleaseLeftButton();
  }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch=*/false);

  ASSERT_FALSE(helper->IsInFolderView());

  const views::ViewModelT<AppListItemView>* view_model =
      GetPagedAppsGridView()->view_model();
  EXPECT_EQ(6u, view_model->view_size());

  // Ensure all items have layers right after ending drag.
  for (size_t i = 0; i < view_model->view_size(); i++)
    EXPECT_TRUE(view_model->view_at(i)->layer());

  WaitForItemLayerAnimations();

  // When each item's layer animation is complete, their layers should have been
  // removed.
  for (size_t i = 0; i < view_model->view_size(); i++)
    EXPECT_FALSE(view_model->view_at(i)->layer());

  EXPECT_FALSE(GetPagedAppsGridView()->IsItemAnimationRunning());
}

// Test that when quickly dragging an item into a second page, and then into the
// search box while the reorder animation is running, does not results in a
// crash.
TEST_F(PagedAppsGridViewTest, EnterSearchBoxDuringDragNoCrash) {
  const size_t kTotalApps = grid_test_api_->TilesPerPageInPagedGrid(0) + 1;
  GetAppListTestHelper()->model()->PopulateApps(kTotalApps);
  UpdateLayout();

  PaginationModel* pagination_model =
      GetAppListTestHelper()->GetRootPagedAppsGridView()->pagination_model();
  EXPECT_EQ(0, pagination_model->selected_page());
  EXPECT_EQ(2, pagination_model->total_pages());

  auto* generator = GetEventGenerator();

  AppListItemView* item_view = GetPagedAppsGridView()->GetItemViewAt(0);

  StartDragOnItemView(item_view);

  std::list<base::OnceClosure> tasks;

  // Move to the second page.
  tasks.push_back(base::BindLambdaForTesting([&]() {
    generator->MoveMouseTo(
        GetPagedAppsGridView()->GetBoundsInScreen().bottom_left() +
        gfx::Vector2d(0, -1));
    EXPECT_TRUE(GetPagedAppsGridView()->cardified_state_for_testing());
    auto page_flip_waiter = std::make_unique<PageFlipWaiter>(pagination_model);
    page_flip_waiter->Wait();
    // Second page should be selected.
    EXPECT_EQ(1, pagination_model->selected_page());
  }));
  // Trigger animation for reordering, and move to the search box while it is
  // still animating.
  tasks.push_back(base::BindLambdaForTesting([&]() {
    ui::ScopedAnimationDurationScaleMode scope_duration(
        ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
    generator->MoveMouseTo(
        GetPagedAppsGridView()->GetBoundsInScreen().CenterPoint());
    ASSERT_TRUE(GetPagedAppsGridView()->reorder_timer_for_test()->IsRunning());
    GetPagedAppsGridView()->reorder_timer_for_test()->FireNow();
    generator->MoveMouseTo(GetAppListTestHelper()
                               ->GetSearchBoxView()
                               ->GetBoundsInScreen()
                               .CenterPoint());
  }));
  // Release drag, required by the drag and drop controller
  tasks.push_back(base::BindLambdaForTesting(
      [&]() { GetEventGenerator()->ReleaseLeftButton(); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch=*/false);
}

// Test the case of beginning an item drag and then immediately ending the drag.
// This will cause the entering cardified state animations to get interrupted by
// the exiting animations. It could be possible that this animation interrupt
// triggers `OnCardifiedStateEnded()` twice, so test that cardified state ended
// only happens once.
TEST_F(PagedAppsGridViewTest, QuicklyDragAndDropItem) {
  ui::ScopedAnimationDurationScaleMode scope_duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  GetAppListTestHelper()->model()->PopulateApps(5);
  UpdateLayout();

  auto* generator = GetEventGenerator();

  // Set the callback to count how many times the cardified state is ended.
  int number_of_times_cardified_state_ended = 0;
  GetPagedAppsGridView()->SetCardifiedStateEndedTestCallback(
      base::BindLambdaForTesting(
          [&]() { number_of_times_cardified_state_ended++; }));

  const views::ViewModelT<AppListItemView>* view_model =
      GetPagedAppsGridView()->view_model();

  // Drag down to the next page.
  StartDragOnItemView(view_model->view_at(1));

  std::list<base::OnceClosure> tasks;
  // Drag the item a short distance and immediately release drag.
  tasks.push_back(base::BindLambdaForTesting([&]() {
    generator->MoveMouseBy(100, 100);
    GetEventGenerator()->ReleaseLeftButton();
  }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch=*/false);

  EXPECT_FALSE(IsRowChangeAnimatorAnimating());

  WaitForItemLayerAnimations();

  // When each item's layer animation is complete, their layers should have been
  // removed.
  for (size_t i = 0; i < view_model->view_size(); i++)
    EXPECT_FALSE(view_model->view_at(i)->layer());
  EXPECT_FALSE(GetPagedAppsGridView()->IsItemAnimationRunning());

  // Now that cardified item animations are complete, make sure that
  // `OnCardifiedStateEnded()` is only called once.
  EXPECT_EQ(1, number_of_times_cardified_state_ended);
}

// When quickly dragging and dropping an item from one row to another, test that
// row change animations are not interrupted during cardified state exit.
TEST_F(PagedAppsGridViewTest, QuicklyDragAndDropItemToNewRow) {
  ui::ScopedAnimationDurationScaleMode scope_duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  GetAppListTestHelper()->model()->PopulateApps(10);
  UpdateLayout();

  auto* generator = GetEventGenerator();

  // Set the callback to count how many times the cardified state is ended.
  int number_of_times_cardified_state_ended = 0;
  GetPagedAppsGridView()->SetCardifiedStateEndedTestCallback(
      base::BindLambdaForTesting(
          [&]() { number_of_times_cardified_state_ended++; }));

  const views::ViewModelT<AppListItemView>* view_model =
      GetPagedAppsGridView()->view_model();

  // Drag down to the next page.
  StartDragOnItemView(view_model->view_at(1));

  std::list<base::OnceClosure> tasks;
  // Quickly drag the item from the first row to the second row, which should
  // cause a row change animation when the drag is released.
  tasks.push_back(base::BindLambdaForTesting([&]() {
    gfx::Point second_row_drag_point =
        view_model->view_at(5)->GetBoundsInScreen().right_center();
    second_row_drag_point.Offset(50, 0);
    generator->MoveMouseTo(second_row_drag_point);
    GetEventGenerator()->ReleaseLeftButton();
  }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch=*/false);

  // There should be a row change animation happening.
  EXPECT_TRUE(IsRowChangeAnimatorAnimating());
  EXPECT_EQ(1, GetNumberOfRowChangeLayersForTest());

  // Fast forward and make sure that the row change animator was not interrupted
  // and is still animating.
  task_environment()->FastForwardBy(base::Milliseconds(100));
  EXPECT_TRUE(IsRowChangeAnimatorAnimating());
  EXPECT_EQ(1, GetNumberOfRowChangeLayersForTest());

  WaitForItemLayerAnimations();

  // When each item's layer animation is complete, their layers should have been
  // removed.
  for (size_t i = 0; i < view_model->view_size(); i++)
    EXPECT_FALSE(view_model->view_at(i)->layer());
  EXPECT_FALSE(GetPagedAppsGridView()->IsItemAnimationRunning());
  EXPECT_FALSE(IsRowChangeAnimatorAnimating());
  EXPECT_EQ(0, GetNumberOfRowChangeLayersForTest());

  // Now that cardified item animations are complete, make sure that
  // `OnCardifiedStateEnded()` is only called once.
  EXPECT_EQ(1, number_of_times_cardified_state_ended);
}

TEST_F(PagedAppsGridViewTest, CardifiedEnterAnimationInterruptedByExit) {
  ui::ScopedAnimationDurationScaleMode scope_duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  GetAppListTestHelper()->model()->PopulateApps(5);
  UpdateLayout();

  AppListItemView* item_view = GetPagedAppsGridView()->view_model()->view_at(0);
  EXPECT_GE(GetPagedAppsGridView()->view_model()->view_size(), 2u);
  auto* generator = GetEventGenerator();

  StartDragOnItemView(GetPagedAppsGridView()->view_model()->view_at(1));

  std::list<base::OnceClosure> first_animation_completes;
  first_animation_completes.push_back(base::BindLambdaForTesting([&]() {
    // Start cardified state.
    generator->MoveMouseBy(10, 10);
    EXPECT_TRUE(GetPagedAppsGridView()->cardified_state_for_testing());
    EXPECT_TRUE(item_view->layer()->GetAnimator()->is_animating());
  }));
  // Check that the first item completes the entering cardified state
  // animation.
  first_animation_completes.push_back(base::BindLambdaForTesting([&]() {
    WaitForItemLayerAnimations();
    EXPECT_FALSE(item_view->layer()->GetAnimator()->is_animating());
    GetEventGenerator()->ReleaseLeftButton();
  }));
  MaybeRunDragAndDropSequenceForAppList(&first_animation_completes,
                                        /*is_touch=*/false);

  EXPECT_FALSE(GetPagedAppsGridView()->cardified_state_for_testing());

  // With the item view animating from its completed cardified position, to the
  // non-cardified position, check that the layer transform is not identity.
  EXPECT_TRUE(item_view->layer()->GetAnimator()->is_animating());
  EXPECT_FALSE(item_view->layer()->transform().IsIdentity());

  WaitForItemLayerAnimations();
  EXPECT_FALSE(item_view->layer());

  StartDragOnItemView(GetPagedAppsGridView()->view_model()->view_at(1));

  std::list<base::OnceClosure> animation_not_completes;
  // Exit cardified state, without waiting for the enter animation to complete.
  animation_not_completes.push_back(base::BindLambdaForTesting([&]() {
    // Start cardified state.
    generator->MoveMouseBy(10, 10);
    EXPECT_TRUE(GetPagedAppsGridView()->cardified_state_for_testing());
    GetEventGenerator()->ReleaseLeftButton();
  }));
  MaybeRunDragAndDropSequenceForAppList(&animation_not_completes,
                                        /*is_touch=*/false);

  // With the item view animating from its current position at the start of the
  // begin cardified state, to its non-cardified position, the layer transform
  // should be the identity transform, indicating a smoothly interrupted
  // animation.
  EXPECT_TRUE(item_view->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(item_view->layer()->transform().IsIdentity());

  WaitForItemLayerAnimations();
  EXPECT_FALSE(item_view->layer());
}

// Test that a first page item released outside of the grid with second page
// shown will visually change back to the first page.
TEST_F(PagedAppsGridViewTest, DragOutsideOfNextPageSelectsOriginalPage) {
  const size_t kTotalApps = grid_test_api_->TilesPerPageInPagedGrid(0) + 1;
  GetAppListTestHelper()->model()->PopulateApps(kTotalApps);
  UpdateLayout();

  PaginationModel* pagination_model =
      GetAppListTestHelper()->GetRootPagedAppsGridView()->pagination_model();
  EXPECT_EQ(0, pagination_model->selected_page());
  EXPECT_EQ(2, pagination_model->total_pages());

  auto* item_view = GetPagedAppsGridView()->view_model()->view_at(0);
  auto* generator = GetEventGenerator();
  std::list<base::OnceClosure> tasks;

  // Start dragging an item on the first page.
  StartDragOnItemView(item_view);

  // Exit cardified state, without waiting for the enter animation to complete.
  tasks.push_back(base::BindLambdaForTesting([&]() {
    generator->MoveMouseTo(
        GetPagedAppsGridView()->GetBoundsInScreen().bottom_left() +
        gfx::Vector2d(0, -1));
    EXPECT_TRUE(GetPagedAppsGridView()->cardified_state_for_testing());
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    auto page_flip_waiter = std::make_unique<PageFlipWaiter>(pagination_model);
    page_flip_waiter->Wait();
    ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
        ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
    // Second page should be selected.
    EXPECT_EQ(1, pagination_model->selected_page());
  }));
  // Move the mouse down to be completely below the grid view. Releasing
  // a drag here will move the item back to its starting position.
  tasks.push_back(base::BindLambdaForTesting([&]() {
    generator->MoveMouseBy(0, 50);
    EXPECT_EQ(1, pagination_model->selected_page());
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // End Drag
    GetEventGenerator()->ReleaseLeftButton();
  }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch=*/false);

  WaitForItemLayerAnimations();
  UpdateLayout();

  // First page should be selected.
  EXPECT_EQ(0, pagination_model->selected_page());

  // Dragged item should be in starting position.
  EXPECT_EQ(item_view, grid_test_api_->GetViewAtIndex(GridIndex(0, 0)));

  auto app_list_item_view_visible = [this](const views::View* view) -> bool {
    return GetPagedAppsGridView()
        ->GetWidget()
        ->GetWindowBoundsInScreen()
        .Contains(view->GetBoundsInScreen());
  };

  // It is possible that the selected page is correct but visually never
  // changes, so check that the dragged 'item_view' is visible, and the item
  // on the second page is not visible.
  EXPECT_TRUE(app_list_item_view_visible(item_view));
  EXPECT_FALSE(app_list_item_view_visible(
      grid_test_api_->GetViewAtIndex(GridIndex(1, 0))));
}

}  // namespace ash
