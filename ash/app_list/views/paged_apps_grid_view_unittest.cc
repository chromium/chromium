// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/paged_apps_grid_view.h"

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/apps_container_view.h"
#include "ash/app_list/views/apps_grid_view_test_api.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/pagination/pagination_model.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"

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

  std::unique_ptr<test::AppsGridViewTestApi> grid_test_api_;
};

// Tests with ProductivityLauncher enabled.
class PagedAppsGridViewTest : public PagedAppsGridViewTestBase {
 public:
  PagedAppsGridViewTest() {
    scoped_features_.InitAndEnableFeature(ash::features::kProductivityLauncher);
  }

  base::test::ScopedFeatureList scoped_features_;
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
  EXPECT_EQ(3, GetPagedAppsGridView()->GetFirstPageRowsForTesting());
  EXPECT_EQ(4, GetPagedAppsGridView()->GetRowsForTesting());
  EXPECT_EQ(5, GetPagedAppsGridView()->cols());

  // Test with a display in portrait mode.
  UpdateDisplay("700x1100");
  EXPECT_EQ(4, GetPagedAppsGridView()->GetFirstPageRowsForTesting());
  EXPECT_EQ(5, GetPagedAppsGridView()->GetRowsForTesting());
  EXPECT_EQ(5, GetPagedAppsGridView()->cols());

  // Test with a display in portrait mode with more height. This should have
  // more rows.
  UpdateDisplay("700x1400");
  EXPECT_EQ(4, GetPagedAppsGridView()->GetFirstPageRowsForTesting());
  EXPECT_EQ(6, GetPagedAppsGridView()->GetRowsForTesting());
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

}  // namespace
}  // namespace ash
