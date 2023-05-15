// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/scrollable_apps_grid_view.h"

#include <limits>
#include <list>
#include <memory>
#include <string>
#include <utility>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/model/app_list_folder_item.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/app_list_item_list.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/model/app_list_test_model.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/test_app_list_client.h"
#include "ash/app_list/views/app_list_folder_view.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/apps_grid_view_test_api.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/constants/ash_features.h"
#include "ash/drag_drop/drag_drop_controller.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/public/cpp/test/test_shelf_item_delegate.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

class ShelfItemFactoryFake : public ShelfModel::ShelfItemFactory {
 public:
  virtual ~ShelfItemFactoryFake() = default;
  bool CreateShelfItemForAppId(
      const std::string& app_id,
      ShelfItem* item,
      std::unique_ptr<ShelfItemDelegate>* delegate) override {
    *item = ShelfItem();
    item->id = ShelfID(app_id);
    *delegate = std::make_unique<TestShelfItemDelegate>(item->id);
    return true;
  }
};

}  // namespace

class ScrollableAppsGridViewTest : public AshTestBase,
                                   public testing::WithParamInterface<bool> {
 public:
  ScrollableAppsGridViewTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~ScrollableAppsGridViewTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatureState(
        app_list_features::kDragAndDropRefactor, GetParam());
    AshTestBase::SetUp();

    shelf_item_factory_ = std::make_unique<ShelfItemFactoryFake>();
    ShelfModel::Get()->SetShelfItemFactory(shelf_item_factory_.get());
  }

  void TearDown() override {
    ShelfModel::Get()->SetShelfItemFactory(nullptr);
    AshTestBase::TearDown();
  }

  test::AppListTestModel::AppListTestItem* AddAppListItem(
      const std::string& id) {
    return GetAppListTestHelper()->model()->CreateAndAddItem(id);
  }

  void PopulateApps(int n) { GetAppListTestHelper()->model()->PopulateApps(n); }

  void DeleteApps(int n) {
    AppListItemList* item_list =
        GetAppListTestHelper()->model()->top_level_item_list();
    for (int i = 0; i < n; i++) {
      GetAppListTestHelper()->model()->DeleteItem(item_list->item_at(0)->id());
    }
  }

  AppListFolderItem* CreateAndPopulateFolderWithApps(int n) {
    return GetAppListTestHelper()->model()->CreateAndPopulateFolderWithApps(n);
  }

  void SimulateKeyPress(ui::KeyboardCode key_code, int flags = ui::EF_NONE) {
    GetEventGenerator()->PressKey(key_code, flags);
  }

  void SimulateKeyReleased(ui::KeyboardCode key_code, int flags = ui::EF_NONE) {
    GetEventGenerator()->ReleaseKey(key_code, flags);
  }

  void ShowAppList() {
    GetAppListTestHelper()->ShowAppList();

    apps_grid_view_ = GetAppListTestHelper()->GetScrollableAppsGridView();
    scroll_view_ = apps_grid_view_->scroll_view_for_test();
  }

  void MaybeRunDragAndDropSequence(std::list<base::OnceClosure>* tasks) {
    if (!GetParam()) {
      while (!tasks->empty()) {
        std::move(tasks->front()).Run();
        tasks->pop_front();
      }
      return;
    }

    ShellTestApi().drag_drop_controller()->SetLoopClosureForTesting(
        base::BindLambdaForTesting([&]() {
          auto task = std::move(tasks->front());
          tasks->pop_front();
          std::move(task).Run();
        }),
        base::DoNothing());
    tasks->push_front(base::BindLambdaForTesting([&]() {
      // Generate OnDragEnter() event for the host view.
      GetEventGenerator()->MoveMouseBy(10, 10);
    }));
    // Start Drag and Drop Sequence by moving the mouse.
    GetEventGenerator()->MoveMouseBy(10, 10);
  }

  AppListItemView* StartDragOnView(AppListItemView* item) {
    auto* generator = GetEventGenerator();
    generator->MoveMouseTo(item->GetBoundsInScreen().CenterPoint());
    generator->PressLeftButton();
    EXPECT_TRUE(item->FireMouseDragTimerForTest());
    return item;
  }

  AppListItemView* StartDragOnItemViewAt(int item_index) {
    return StartDragOnView(apps_grid_view_->GetItemViewAt(item_index));
  }

  AppListItemView* StartDragOnItemInFolderAt(int item_index) {
    DCHECK(GetAppListTestHelper()->IsInFolderView());
    auto* folder_view = GetAppListTestHelper()->GetBubbleFolderView();
    AppListItemView* item =
        folder_view->items_grid_view()->GetItemViewAt(item_index);
    return StartDragOnView(item);
  }

  void DragItemOutOfFolder() {
    ASSERT_TRUE(GetAppListTestHelper()->IsInFolderView());
    auto* folder_view = GetAppListTestHelper()->GetBubbleFolderView();
    ASSERT_TRUE(folder_view->items_grid_view()->has_dragged_item());
    gfx::Point outside_view = folder_view->GetBoundsInScreen().bottom_right();
    GetEventGenerator()->MoveMouseTo(outside_view);
    GetEventGenerator()->MoveMouseBy(10, 10);
    folder_view->items_grid_view()->FireFolderItemReparentTimerForTest();
  }

  ScrollableAppsGridView* GetScrollableAppsGridView() {
    return GetAppListTestHelper()->GetScrollableAppsGridView();
  }

  // Verifies the visible item index range.
  bool IsIndexRangeExpected(size_t first_index, size_t last_index) {
    const absl::optional<AppsGridView::VisibleItemIndexRange> index_range =
        apps_grid_view_->GetVisibleItemIndexRange();

    return index_range->first_index == first_index &&
           index_range->last_index == last_index;
  }

  std::unique_ptr<ShelfItemFactoryFake> shelf_item_factory_;

  // Cache some view pointers to make the tests more concise.
  raw_ptr<ScrollableAppsGridView, ExperimentalAsh> apps_grid_view_ = nullptr;
  raw_ptr<views::ScrollView, ExperimentalAsh> scroll_view_ = nullptr;
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All, ScrollableAppsGridViewTest, testing::Bool());

TEST_P(ScrollableAppsGridViewTest, ClickOnApp) {
  AddAppListItem("id");

  ShowAppList();

  // Click on the first icon.
  ScrollableAppsGridView* view = GetScrollableAppsGridView();
  views::View* icon = view->GetItemViewAt(0);
  GetEventGenerator()->MoveMouseTo(icon->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();

  // The item was activated.
  EXPECT_EQ(1, GetTestAppListClient()->activate_item_count());
  EXPECT_EQ("id", GetTestAppListClient()->activate_item_last_id());
}

TEST_P(ScrollableAppsGridViewTest, DragApp) {
  base::HistogramTester histogram_tester;
  AddAppListItem("id1");
  AddAppListItem("id2");
  ShowAppList();

  // Start dragging the first item.
  StartDragOnItemViewAt(0);

  auto* generator = GetEventGenerator();
  // Drag to the right of the second item.
  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    gfx::Size tile_size = apps_grid_view_->GetTotalTileSize(/*page=*/0);
    generator->MoveMouseBy(tile_size.width() * 2, 0);
  }));
  tasks.push_back(base::BindLambdaForTesting(
      [&]() { GetEventGenerator()->ReleaseLeftButton(); }));
  MaybeRunDragAndDropSequence(&tasks);

  generator->ReleaseLeftButton();

  // The item was not activated.
  EXPECT_EQ(0, GetTestAppListClient()->activate_item_count());

  // Items were reordered.
  AppListItemList* item_list =
      GetAppListTestHelper()->model()->top_level_item_list();
  ASSERT_EQ(2u, item_list->item_count());
  EXPECT_EQ("id2", item_list->item_at(0)->id());
  EXPECT_EQ("id1", item_list->item_at(1)->id());

  // Reordering apps is recorded in the histogram tester.
  histogram_tester.ExpectBucketCount("Apps.AppListBubbleAppMovingType",
                                     kReorderByDragInTopLevel, 1);
}

TEST_P(ScrollableAppsGridViewTest, SearchBoxHasFocusAfterDrag) {
  PopulateApps(2);
  ShowAppList();

  // Drag the first item to the right.
  AppListItemView* item = StartDragOnItemViewAt(0);

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting(
      [&]() { GetEventGenerator()->MoveMouseBy(250, 0); }));
  tasks.push_back(base::BindLambdaForTesting(
      [&]() { GetEventGenerator()->ReleaseLeftButton(); }));
  MaybeRunDragAndDropSequence(&tasks);

  // The item does not have focus, but it might be selected.
  EXPECT_FALSE(item->HasFocus());

  // The search box has focus.
  auto* search_box_view = GetAppListTestHelper()->GetBubbleSearchBoxView();
  EXPECT_TRUE(search_box_view->search_box()->HasFocus());
  EXPECT_TRUE(search_box_view->is_search_box_active());
}

TEST_P(ScrollableAppsGridViewTest, DragAppAfterScrollingDown) {
  // Simulate data from another device.
  PopulateApps(20);
  AddAppListItem("aaa");
  AddAppListItem("bbb");
  ShowAppList();

  // "aaa" and "bbb" are the last two items.
  AppListItemList* item_list =
      GetAppListTestHelper()->model()->top_level_item_list();
  ASSERT_EQ(22u, item_list->item_count());
  ASSERT_EQ("aaa", item_list->item_at(20)->id());
  ASSERT_EQ("bbb", item_list->item_at(21)->id());

  // Scroll down to the "aaa" item.
  auto* apps_grid_view = GetScrollableAppsGridView();
  AppListItemView* item = apps_grid_view->GetItemViewAt(20);
  ASSERT_EQ("aaa", item->item()->id());
  item->ScrollViewToVisible();

  auto* generator = GetEventGenerator();
  // Drag the "aaa" item to the right.
  StartDragOnView(item);

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    gfx::Size tile_size = apps_grid_view->GetTotalTileSize(/*page=*/0);
    generator->MoveMouseBy(tile_size.width() * 2, 0);
  }));
  tasks.push_back(
      base::BindLambdaForTesting([&]() { generator->ReleaseLeftButton(); }));
  MaybeRunDragAndDropSequence(&tasks);

  // The last 2 items were reordered.
  EXPECT_EQ("bbb", item_list->item_at(20)->id()) << item_list->ToString();
  EXPECT_EQ("aaa", item_list->item_at(21)->id()) << item_list->ToString();
}

TEST_P(ScrollableAppsGridViewTest, AutoScrollDown) {
  PopulateApps(30);
  ShowAppList();

  // Scroll view starts at the top.
  const int initial_scroll_offset = scroll_view_->GetVisibleRect().y();
  EXPECT_EQ(initial_scroll_offset, 0);

  // Drag an item into the bottom auto-scroll margin.
  StartDragOnItemViewAt(0);

  auto* generator = GetEventGenerator();
  int scroll_offset_start, scroll_offset_end;
  // Drag an item into the bottom auto-scroll margin.
  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    generator->MoveMouseTo(scroll_view_->GetBoundsInScreen().bottom_center() +
                           gfx::Vector2d(0, -5));
    // The scroll view scrolls immediately.
    scroll_offset_start = scroll_view_->GetVisibleRect().y();
    EXPECT_GT(scroll_offset_start, 0);
    // Scroll timer is running.
    EXPECT_TRUE(apps_grid_view_->auto_scroll_timer_for_test()->IsRunning());
    // Reordering is paused.
    EXPECT_FALSE(apps_grid_view_->reorder_timer_for_test()->IsRunning());
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Holding the mouse in place for a while scrolls down more.
    task_environment()->FastForwardBy(base::Milliseconds(100));
    scroll_offset_end = scroll_view_->GetVisibleRect().y();
    EXPECT_GT(scroll_offset_end, scroll_offset_start);
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Move the mouse back into the (vertical) center of the view (not in the
    // scroll margin). Use a point within the grid and not the scroll view,
    // since the apps grid is the target for drop events.
    generator->MoveMouseTo(apps_grid_view_->GetBoundsInScreen().left_center());
    // Scroll position didn't change, auto-scrolling is stopped, and reordering
    // started again.
    EXPECT_EQ(scroll_offset_end, scroll_view_->GetVisibleRect().y());
    EXPECT_FALSE(apps_grid_view_->auto_scroll_timer_for_test()->IsRunning());
    EXPECT_TRUE(apps_grid_view_->reorder_timer_for_test()->IsRunning());
    GetEventGenerator()->ReleaseLeftButton();
  }));
  MaybeRunDragAndDropSequence(&tasks);
}

TEST_P(ScrollableAppsGridViewTest, DoesNotAutoScrollUpWhenAtTop) {
  PopulateApps(30);
  ShowAppList();

  auto* generator = GetEventGenerator();
  // Drag an item into the top auto-scroll margin and wait a while.
  StartDragOnItemViewAt(0);

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    generator->MoveMouseTo(apps_grid_view_->GetBoundsInScreen().top_center());
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    task_environment()->FastForwardBy(base::Milliseconds(500));
    // View did not scroll.
    int scroll_offset = scroll_view_->GetVisibleRect().y();
    EXPECT_EQ(scroll_offset, 0);
    EXPECT_FALSE(apps_grid_view_->auto_scroll_timer_for_test()->IsRunning());
    generator->ReleaseLeftButton();
  }));
  MaybeRunDragAndDropSequence(&tasks);
}

TEST_P(ScrollableAppsGridViewTest, DoesNotAutoScrollDownWhenAtBottom) {
  PopulateApps(30);
  ShowAppList();

  // Scroll the view to the bottom.
  scroll_view_->ScrollToPosition(scroll_view_->vertical_scroll_bar(),
                                 std::numeric_limits<int>::max());
  int initial_scroll_offset = scroll_view_->GetVisibleRect().y();

  // Drag an item into the bottom auto-scroll margin and wait a while.
  StartDragOnItemViewAt(29);

  auto* generator = GetEventGenerator();

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    generator->MoveMouseTo(
        apps_grid_view_->GetBoundsInScreen().bottom_center());
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    task_environment()->FastForwardBy(base::Milliseconds(500));
    // View did not scroll.
    int scroll_offset = scroll_view_->GetVisibleRect().y();
    EXPECT_EQ(scroll_offset, initial_scroll_offset);
    EXPECT_FALSE(apps_grid_view_->auto_scroll_timer_for_test()->IsRunning());
    generator->ReleaseLeftButton();
  }));
  MaybeRunDragAndDropSequence(&tasks);
}

TEST_P(ScrollableAppsGridViewTest, DoesNotAutoScrollWhenDraggedToTheRight) {
  PopulateApps(30);
  ShowAppList();

  // Drag an item outside the bottom-right corner of the scroll view (i.e.
  // towards the shelf).
  StartDragOnItemViewAt(0);

  auto* generator = GetEventGenerator();
  gfx::Point point = apps_grid_view_->GetBoundsInScreen().bottom_right();
  point.Offset(10, 10);

  std::list<base::OnceClosure> tasks;
  tasks.push_back(
      base::BindLambdaForTesting([&]() { generator->MoveMouseTo(point); }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    task_environment()->FastForwardBy(base::Milliseconds(500));
    // View did not scroll.
    int scroll_offset = scroll_view_->GetVisibleRect().y();
    EXPECT_EQ(scroll_offset, 0);
    EXPECT_FALSE(apps_grid_view_->auto_scroll_timer_for_test()->IsRunning());
    generator->ReleaseLeftButton();
  }));
  MaybeRunDragAndDropSequence(&tasks);
}

TEST_P(ScrollableAppsGridViewTest, DoesNotAutoScrollWhenAboveWidget) {
  PopulateApps(30);
  ShowAppList();

  // Scroll the view to the bottom.
  scroll_view_->ScrollToPosition(scroll_view_->vertical_scroll_bar(),
                                 std::numeric_limits<int>::max());
  int initial_scroll_offset = scroll_view_->GetVisibleRect().y();

  // Drag an item above the widget scroll margin.
  StartDragOnItemViewAt(29);

  auto* generator = GetEventGenerator();
  gfx::Point point =
      scroll_view_->GetWidget()->GetWindowBoundsInScreen().top_center();
  point.Offset(0, -10);

  std::list<base::OnceClosure> tasks;
  tasks.push_back(
      base::BindLambdaForTesting([&]() { generator->MoveMouseTo(point); }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    task_environment()->FastForwardBy(base::Milliseconds(500));
    // View did not scroll.
    int scroll_offset = scroll_view_->GetVisibleRect().y();
    EXPECT_EQ(scroll_offset, initial_scroll_offset);
    EXPECT_FALSE(apps_grid_view_->auto_scroll_timer_for_test()->IsRunning());
    generator->ReleaseLeftButton();
  }));
  MaybeRunDragAndDropSequence(&tasks);
}

TEST_P(ScrollableAppsGridViewTest, DoesNotAutoScrollWhenBelowWidget) {
  PopulateApps(30);
  ShowAppList();

  // Drag an item below the widget scroll margin.
  StartDragOnItemViewAt(0);

  auto* generator = GetEventGenerator();
  gfx::Point point =
      scroll_view_->GetWidget()->GetWindowBoundsInScreen().bottom_center();
  point.Offset(0, 10);

  std::list<base::OnceClosure> tasks;
  tasks.push_back(
      base::BindLambdaForTesting([&]() { generator->MoveMouseTo(point); }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    task_environment()->FastForwardBy(base::Milliseconds(500));
    // View did not scroll.
    int scroll_offset = scroll_view_->GetVisibleRect().y();
    EXPECT_EQ(scroll_offset, 0);
    EXPECT_FALSE(apps_grid_view_->auto_scroll_timer_for_test()->IsRunning());
    generator->ReleaseLeftButton();
  }));
  MaybeRunDragAndDropSequence(&tasks);
}

// Regression test for https://crbug.com/1258954
TEST_P(ScrollableAppsGridViewTest, DragItemIntoEmptySpaceWillReorderToEnd) {
  AddAppListItem("id1");
  AddAppListItem("id2");
  AddAppListItem("id3");
  ShowAppList();

  // The grid view is taller than the single row of apps, so it can handle drops
  // in the empty region.
  EXPECT_GT(apps_grid_view_->height(),
            apps_grid_view_->GetTileGridSize().height());

  // Drag and drop the first item straight down below the first row.
  StartDragOnItemViewAt(0);

  gfx::Size tile_size = apps_grid_view_->GetTotalTileSize(/*page=*/0);
  auto* generator = GetEventGenerator();

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    generator->MoveMouseBy(0, tile_size.height());
    generator->ReleaseLeftButton();
  }));
  MaybeRunDragAndDropSequence(&tasks);

  // The first item was reordered to the end.
  AppListItemList* item_list =
      GetAppListTestHelper()->model()->top_level_item_list();
  ASSERT_EQ(3u, item_list->item_count());
  EXPECT_EQ("id2", item_list->item_at(0)->id());
  EXPECT_EQ("id3", item_list->item_at(1)->id());
  EXPECT_EQ("id1", item_list->item_at(2)->id());
}

TEST_P(ScrollableAppsGridViewTest, ChangingAppListModelUpdatesAppsGridHeight) {
  // Start with 4 rows of 5.
  PopulateApps(20);
  ShowAppList();

  // Adding one row of 5 causes the grid size to expand.
  const int height_before_adding = apps_grid_view_->height();
  PopulateApps(5);
  apps_grid_view_->GetWidget()->LayoutRootViewIfNecessary();
  EXPECT_GT(apps_grid_view_->height(), height_before_adding);

  // Removing one row of 5 causes the grid size to contract.
  const int height_before_removing = apps_grid_view_->height();
  DeleteApps(5);
  apps_grid_view_->GetWidget()->LayoutRootViewIfNecessary();
  EXPECT_LT(apps_grid_view_->height(), height_before_removing);
}

TEST_P(ScrollableAppsGridViewTest, SmallFolderHasCorrectWidth) {
  CreateAndPopulateFolderWithApps(2);
  ShowAppList();

  // Enter the folder view.
  LeftClickOn(apps_grid_view_->GetItemViewAt(0));
  ASSERT_TRUE(GetAppListTestHelper()->IsInFolderView());

  auto* folder_view = GetAppListTestHelper()->GetBubbleFolderView();
  auto* items_grid_view = folder_view->items_grid_view();
  const int tile_width = items_grid_view->app_list_config()->grid_tile_width();

  // Spec calls for 8 dips of padding at edges and between tiles.
  EXPECT_EQ(folder_view->width(), 8 + tile_width + 8 + tile_width + 8);

  // The leftmost item is flush to the left of the grid.
  EXPECT_EQ(items_grid_view->GetItemViewAt(0)->bounds().x(), 0);

  // The rightmost item is flush to the right of the grid.
  EXPECT_EQ(items_grid_view->GetItemViewAt(1)->bounds().right(),
            items_grid_view->GetLocalBounds().right());
}

TEST_P(ScrollableAppsGridViewTest, DragItemToReorderInFolderRecordsHistogram) {
  base::HistogramTester histogram_tester;
  // Create a folder with 3 apps.
  AppListFolderItem* folder_item = CreateAndPopulateFolderWithApps(3);
  ShowAppList();

  // Enter the folder view.
  LeftClickOn(apps_grid_view_->GetItemViewAt(0));
  ASSERT_TRUE(GetAppListTestHelper()->IsInFolderView());

  // Drag the first app in the folder.
  AppListItemView* item_view = StartDragOnItemInFolderAt(0);

  // Drag the item to the third position in the folder.
  gfx::Size tile_size = apps_grid_view_->GetTotalTileSize(/*page=*/0);
  auto* generator = GetEventGenerator();

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    generator->MoveMouseBy(0, tile_size.height());
    generator->ReleaseLeftButton();
  }));
  MaybeRunDragAndDropSequence(&tasks);

  // The item is now reordered in the folder and the reordering is recorded.
  EXPECT_EQ(3u, folder_item->ChildItemCount());
  EXPECT_EQ(folder_item->item_list()->item_at(2)->id(),
            item_view->item()->id());
  histogram_tester.ExpectBucketCount("Apps.AppListBubbleAppMovingType",
                                     kReorderByDragInFolder, 1);
}

TEST_P(ScrollableAppsGridViewTest, DragItemIntoFolderRecordsHistogram) {
  base::HistogramTester histogram_tester;
  // Create a folder and an app.
  AppListFolderItem* folder_item = CreateAndPopulateFolderWithApps(3);
  AddAppListItem("dragged_item");
  ShowAppList();

  // Drag the app in the top level app list into the folder.
  StartDragOnItemViewAt(1);
  ASSERT_TRUE(apps_grid_view_->GetItemViewAt(0)->item()->is_folder());
  auto* generator = GetEventGenerator();

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    generator->MoveMouseTo(
        apps_grid_view_->GetItemViewAt(0)->GetBoundsInScreen().CenterPoint());
    generator->ReleaseLeftButton();
  }));
  MaybeRunDragAndDropSequence(&tasks);

  // The dragged app is now in the folder and the reordering is recorded.
  EXPECT_EQ(4u, folder_item->ChildItemCount());
  EXPECT_EQ(folder_item->item_list()->item_at(3)->id(), "dragged_item");
  histogram_tester.ExpectBucketCount("Apps.AppListBubbleAppMovingType",
                                     kMoveByDragIntoFolder, 1);
}

TEST_P(ScrollableAppsGridViewTest, DragItemOutOfFolderRecordsHistogram) {
  base::HistogramTester histogram_tester;
  // Create a folder with 3 apps.
  AppListFolderItem* folder_item = CreateAndPopulateFolderWithApps(3);
  ShowAppList();

  // Enter the folder view.
  LeftClickOn(apps_grid_view_->GetItemViewAt(0));
  ASSERT_TRUE(GetAppListTestHelper()->IsInFolderView());

  // Drag the first app in the folder and move it out of the folder.
  AppListItemView* item_view = StartDragOnItemInFolderAt(0);
  std::string item_id = item_view->item()->id();
  auto* generator = GetEventGenerator();

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() { DragItemOutOfFolder(); }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Drag the app item to near the expected end position and end the drag.
    generator->MoveMouseTo(
        apps_grid_view_->GetItemViewAt(0)->GetBoundsInScreen().right_center() +
        gfx::Vector2d(20, 0));
    generator->ReleaseLeftButton();
  }));
  MaybeRunDragAndDropSequence(&tasks);

  // The folder view should be closed and invisible after releasing the drag.
  EXPECT_FALSE(GetAppListTestHelper()->IsInFolderView());
  EXPECT_FALSE(GetAppListTestHelper()->GetBubbleFolderView()->GetVisible());

  // The dragged item is now in the top level item list and the reordering is
  // recorded.
  AppListItemList* item_list =
      GetAppListTestHelper()->model()->top_level_item_list();
  EXPECT_EQ(2u, item_list->item_count());
  EXPECT_EQ(item_list->item_at(1)->id(), item_id);
  EXPECT_EQ(2u, folder_item->item_list()->item_count());
  histogram_tester.ExpectBucketCount("Apps.AppListBubbleAppMovingType",
                                     kMoveByDragOutOfFolder, 1);
}

TEST_P(ScrollableAppsGridViewTest,
       DragItemFromOneFolderToAnotherRecordsHistogram) {
  base::HistogramTester histogram_tester;
  // Create two folders.
  AppListFolderItem* folder_item_1 = CreateAndPopulateFolderWithApps(3);
  AppListFolderItem* folder_item_2 = CreateAndPopulateFolderWithApps(2);
  ShowAppList();

  // Enter the view of the first folder.
  LeftClickOn(apps_grid_view_->GetItemViewAt(0));
  ASSERT_TRUE(GetAppListTestHelper()->IsInFolderView());

  // Drag the first app in the folder and move it out of the folder.
  StartDragOnItemInFolderAt(0);
  auto* generator = GetEventGenerator();

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() { DragItemOutOfFolder(); }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Move the app item into the other folder and end the drag.
    generator->MoveMouseTo(
        apps_grid_view_->GetItemViewAt(1)->GetBoundsInScreen().CenterPoint());
    generator->ReleaseLeftButton();
  }));
  MaybeRunDragAndDropSequence(&tasks);

  // No folder view is showing now.
  EXPECT_FALSE(GetAppListTestHelper()->IsInFolderView());
  EXPECT_FALSE(GetAppListTestHelper()->GetBubbleFolderView()->GetVisible());

  // The dragged item was moved to another folder and the reordering is
  // recorded.
  EXPECT_EQ(2u, folder_item_1->item_list()->item_count());
  EXPECT_EQ(3u, folder_item_2->item_list()->item_count());
  histogram_tester.ExpectBucketCount("Apps.AppListBubbleAppMovingType",
                                     kMoveIntoAnotherFolder, 1);
}

TEST_P(ScrollableAppsGridViewTest, ReparentDragToNewRow) {
  const int kInitialItems = 20;
  AppListFolderItem* folder_item = CreateAndPopulateFolderWithApps(3);
  PopulateApps(kInitialItems - 1);
  ShowAppList();

  ASSERT_EQ(
      0u, apps_grid_view_->view_model()->view_size() % apps_grid_view_->cols());

  // Enter the view of the first folder.
  LeftClickOn(apps_grid_view_->GetItemViewAt(0));
  ASSERT_TRUE(GetAppListTestHelper()->IsInFolderView());

  // Drag the first app in the folder and move it out of the folder.
  AppListItemView* const dragged_view = StartDragOnItemInFolderAt(0);
  const std::string dragged_item_id = dragged_view->item()->id();
  auto* generator = GetEventGenerator();

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    DragItemOutOfFolder();
    ASSERT_TRUE(apps_grid_view_->reorder_timer_for_test()->IsRunning());
    apps_grid_view_->reorder_timer_for_test()->FireNow();
    apps_grid_view_->GetWidget()->LayoutRootViewIfNecessary();
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Scroll the grid to the bottom.
    apps_grid_view_->ScrollRectToVisible(gfx::Rect(
        apps_grid_view_->GetLocalBounds().bottom_center() - gfx::Vector2d(0, 1),
        gfx::Size(1, 1)));
    // Drop the item over expected first empty slot bounds. This should drop the
    // item into the last slot.
    gfx::Rect last_slot_bounds =
        test::AppsGridViewTestApi(apps_grid_view_)
            .GetItemTileRectOnCurrentPageAt(
                kInitialItems / apps_grid_view_->cols(), 1);
    views::View::ConvertRectToScreen(apps_grid_view_, &last_slot_bounds);
    generator->MoveMouseTo(last_slot_bounds.CenterPoint());
    ASSERT_TRUE(apps_grid_view_->reorder_timer_for_test()->IsRunning());
    apps_grid_view_->reorder_timer_for_test()->FireNow();
  }));
  tasks.push_back(
      base::BindLambdaForTesting([&]() { generator->ReleaseLeftButton(); }));
  MaybeRunDragAndDropSequence(&tasks);

  AppListItemView* last_item = apps_grid_view_->GetItemViewAt(kInitialItems);
  ASSERT_TRUE(last_item);
  EXPECT_EQ(dragged_item_id, last_item->item()->id());
  EXPECT_EQ("", last_item->item()->folder_id());
  EXPECT_EQ(2u, folder_item->ChildItemCount());
}

TEST_P(ScrollableAppsGridViewTest, CanceledReparentDragToNewRow) {
  const int kInitialItems = 20;
  AppListFolderItem* folder_item = CreateAndPopulateFolderWithApps(3);
  PopulateApps(kInitialItems - 1);
  ShowAppList();

  ASSERT_EQ(
      0u, apps_grid_view_->view_model()->view_size() % apps_grid_view_->cols());

  const gfx::Size initial_preferred_size = apps_grid_view_->GetPreferredSize();

  // Enter the view of the first folder.
  LeftClickOn(apps_grid_view_->GetItemViewAt(0));
  ASSERT_TRUE(GetAppListTestHelper()->IsInFolderView());

  // Drag the first app in the folder and move it out of the folder.
  AppListItemView* const dragged_view = StartDragOnItemInFolderAt(0);
  const std::string dragged_item_id = dragged_view->item()->id();
  auto* generator = GetEventGenerator();

  std::list<base::OnceClosure> tasks;
  tasks.push_back(base::BindLambdaForTesting([&]() {
    DragItemOutOfFolder();
    ASSERT_TRUE(apps_grid_view_->reorder_timer_for_test()->IsRunning());
    apps_grid_view_->reorder_timer_for_test()->FireNow();
    apps_grid_view_->GetWidget()->LayoutRootViewIfNecessary();
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Scroll the grid to the bottom.
    apps_grid_view_->ScrollRectToVisible(gfx::Rect(
        apps_grid_view_->GetLocalBounds().bottom_center() - gfx::Vector2d(0, 1),
        gfx::Size(1, 1)));
  }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Move the mouse pointer outside the apps grid bounds, and release it. This
    // should cancel the reparent drag operation.
    generator->MoveMouseTo(
        apps_grid_view_->GetBoundsInScreen().bottom_center() +
        gfx::Vector2d(0, 50));
    generator->ReleaseLeftButton();
  }));
  MaybeRunDragAndDropSequence(&tasks);

  EXPECT_EQ(initial_preferred_size, apps_grid_view_->GetPreferredSize());
  AppListItemView* last_item = apps_grid_view_->GetItemViewAt(kInitialItems);
  EXPECT_FALSE(last_item);
  AppListItem* dragged_item =
      GetAppListTestHelper()->model()->FindItem(dragged_item_id);
  ASSERT_TRUE(dragged_item);
  EXPECT_EQ(folder_item->id(), dragged_item->folder_id());
  EXPECT_EQ(3u, folder_item->ChildItemCount());
}

TEST_P(ScrollableAppsGridViewTest, LeftAndRightArrowKeysMoveSelection) {
  PopulateApps(2);
  ShowAppList();

  auto* apps_grid_view = GetScrollableAppsGridView();
  AppListItemView* item1 = apps_grid_view->GetItemViewAt(0);
  AppListItemView* item2 = apps_grid_view->GetItemViewAt(1);

  apps_grid_view->GetFocusManager()->SetFocusedView(item1);
  EXPECT_TRUE(item1->HasFocus());

  PressAndReleaseKey(ui::VKEY_RIGHT);
  EXPECT_FALSE(item1->HasFocus());
  EXPECT_TRUE(item2->HasFocus());

  PressAndReleaseKey(ui::VKEY_LEFT);
  EXPECT_TRUE(item1->HasFocus());
  EXPECT_FALSE(item2->HasFocus());
}

TEST_P(ScrollableAppsGridViewTest, ArrowKeysCanMoveFocusOutOfGrid) {
  PopulateApps(2);
  ShowAppList();

  auto* apps_grid_view = GetScrollableAppsGridView();
  AppListItemView* item1 = apps_grid_view->GetItemViewAt(0);
  AppListItemView* item2 = apps_grid_view->GetItemViewAt(1);

  // Moving left from the first item removes focus from the grid.
  apps_grid_view->GetFocusManager()->SetFocusedView(item1);
  PressAndReleaseKey(ui::VKEY_LEFT);
  EXPECT_FALSE(item1->HasFocus());
  EXPECT_FALSE(item2->HasFocus());

  // Moving up from the first item removes focus from the grid.
  apps_grid_view->GetFocusManager()->SetFocusedView(item1);
  PressAndReleaseKey(ui::VKEY_UP);
  EXPECT_FALSE(item1->HasFocus());
  EXPECT_FALSE(item2->HasFocus());

  // Moving right from the last item removes focus from the grid.
  apps_grid_view->GetFocusManager()->SetFocusedView(item2);
  PressAndReleaseKey(ui::VKEY_RIGHT);
  EXPECT_FALSE(item1->HasFocus());
  EXPECT_FALSE(item2->HasFocus());

  // Moving down from the last item removes focus from the grid.
  apps_grid_view->GetFocusManager()->SetFocusedView(item2);
  PressAndReleaseKey(ui::VKEY_DOWN);
  EXPECT_FALSE(item1->HasFocus());
  EXPECT_FALSE(item2->HasFocus());
}

// Tests that histograms are recorded when apps are moved with control+arrow.
TEST_P(ScrollableAppsGridViewTest, ControlArrowRecordsHistogramBasic) {
  base::HistogramTester histogram_tester;
  PopulateApps(20);
  ShowAppList();
  ScrollableAppsGridView* apps_grid_view = GetScrollableAppsGridView();

  AppListItemView* moving_item = apps_grid_view->GetItemViewAt(0);
  apps_grid_view->GetFocusManager()->SetFocusedView(moving_item);

  // Make one move right and expect a histogram is recorded.
  SimulateKeyPress(ui::VKEY_RIGHT, ui::EF_CONTROL_DOWN);
  SimulateKeyReleased(ui::VKEY_RIGHT, ui::EF_NONE);

  histogram_tester.ExpectBucketCount("Apps.AppListBubbleAppMovingType",
                                     kReorderByKeyboardInTopLevel, 1);

  // Make one move down and expect a histogram is recorded.
  SimulateKeyPress(ui::VKEY_DOWN, ui::EF_CONTROL_DOWN);
  SimulateKeyReleased(ui::VKEY_DOWN, ui::EF_NONE);

  histogram_tester.ExpectBucketCount("Apps.AppListBubbleAppMovingType",
                                     kReorderByKeyboardInTopLevel, 2);

  // Make one move up and expect a histogram is recorded.
  SimulateKeyPress(ui::VKEY_UP, ui::EF_CONTROL_DOWN);
  SimulateKeyReleased(ui::VKEY_UP, ui::EF_NONE);

  histogram_tester.ExpectBucketCount("Apps.AppListBubbleAppMovingType",
                                     kReorderByKeyboardInTopLevel, 3);

  // Make one move left and expect a histogram is recorded.
  SimulateKeyPress(ui::VKEY_LEFT, ui::EF_CONTROL_DOWN);
  SimulateKeyReleased(ui::VKEY_LEFT, ui::EF_NONE);

  histogram_tester.ExpectBucketCount("Apps.AppListBubbleAppMovingType",
                                     kReorderByKeyboardInTopLevel, 4);
}

// Test that histograms do not record when the keyboard move is a no-op.
TEST_P(ScrollableAppsGridViewTest,
       ControlArrowDoesNotRecordHistogramWithNoOpMove) {
  base::HistogramTester histogram_tester;
  PopulateApps(20);
  ShowAppList();
  ScrollableAppsGridView* apps_grid_view = GetScrollableAppsGridView();

  AppListItemView* moving_item = apps_grid_view->GetItemViewAt(0);
  apps_grid_view->GetFocusManager()->SetFocusedView(moving_item);

  // Make 2 no-op moves and one successful move from 0,0 and expect a histogram
  // is recorded only once.
  SimulateKeyPress(ui::VKEY_LEFT, ui::EF_CONTROL_DOWN);
  SimulateKeyReleased(ui::VKEY_LEFT, ui::EF_NONE);

  SimulateKeyPress(ui::VKEY_UP, ui::EF_CONTROL_DOWN);
  SimulateKeyReleased(ui::VKEY_UP, ui::EF_NONE);

  SimulateKeyPress(ui::VKEY_RIGHT, ui::EF_CONTROL_DOWN);
  SimulateKeyReleased(ui::VKEY_RIGHT, ui::EF_NONE);

  histogram_tester.ExpectBucketCount("Apps.AppListBubbleAppMovingType",
                                     kReorderByKeyboardInTopLevel, 1);
}

// Tests that histograms are recorded in folder view when apps are moved with
// control+arrow.
TEST_P(ScrollableAppsGridViewTest, ControlArrowRecordsHistogramInFolderBasic) {
  base::HistogramTester histogram_tester;
  CreateAndPopulateFolderWithApps(4);
  ShowAppList();
  ScrollableAppsGridView* apps_grid_view = GetScrollableAppsGridView();

  // Select the folder item in the grid.
  AppListItemView* folder_item_view = apps_grid_view->GetItemViewAt(0);
  EXPECT_TRUE(folder_item_view->item()->is_folder());

  // Enter the folder view.
  apps_grid_view->GetFocusManager()->SetFocusedView(folder_item_view);
  SimulateKeyPress(ui::VKEY_RETURN);
  ASSERT_TRUE(GetAppListTestHelper()->IsInFolderView());

  // If the folder view is entered by pressing return key while the focus is on
  // the folder, the focus will move to the first item inside the folder view.
  AppsGridView* folder_grid_view =
      GetAppListTestHelper()->GetBubbleFolderView()->items_grid_view();
  EXPECT_EQ(apps_grid_view->GetFocusManager()->GetFocusedView(),
            folder_grid_view->GetItemViewAt(0));

  // Make one move right and expect a histogram is recorded.
  SimulateKeyPress(ui::VKEY_RIGHT, ui::EF_CONTROL_DOWN);
  SimulateKeyReleased(ui::VKEY_RIGHT, ui::EF_NONE);
  histogram_tester.ExpectBucketCount("Apps.AppListBubbleAppMovingType",
                                     kReorderByKeyboardInFolder, 1);

  // Make one move down and expect a histogram is recorded.
  SimulateKeyPress(ui::VKEY_DOWN, ui::EF_CONTROL_DOWN);
  SimulateKeyReleased(ui::VKEY_DOWN, ui::EF_NONE);
  histogram_tester.ExpectBucketCount("Apps.AppListBubbleAppMovingType",
                                     kReorderByKeyboardInFolder, 2);

  // Make one move left and expect a histogram is recorded.
  SimulateKeyPress(ui::VKEY_LEFT, ui::EF_CONTROL_DOWN);
  SimulateKeyReleased(ui::VKEY_LEFT, ui::EF_NONE);
  histogram_tester.ExpectBucketCount("Apps.AppListBubbleAppMovingType",
                                     kReorderByKeyboardInFolder, 3);

  // Make one move up and expect a histogram is recorded.
  SimulateKeyPress(ui::VKEY_UP, ui::EF_CONTROL_DOWN);
  SimulateKeyReleased(ui::VKEY_UP, ui::EF_NONE);
  histogram_tester.ExpectBucketCount("Apps.AppListBubbleAppMovingType",
                                     kReorderByKeyboardInFolder, 4);
}

// Tests that histograms do not record when the keyboard move is a no-op in the
// folder view.
TEST_P(ScrollableAppsGridViewTest,
       ControlArrowDoesNotRecordHistogramWithNoOpMoveInFolder) {
  base::HistogramTester histogram_tester;
  CreateAndPopulateFolderWithApps(4);
  ShowAppList();
  ScrollableAppsGridView* apps_grid_view = GetScrollableAppsGridView();

  // Select the folder item in the grid.
  AppListItemView* folder_item_view = apps_grid_view->GetItemViewAt(0);
  EXPECT_TRUE(folder_item_view->item()->is_folder());

  // Enter the folder view.
  apps_grid_view->GetFocusManager()->SetFocusedView(folder_item_view);
  SimulateKeyPress(ui::VKEY_RETURN);
  ASSERT_TRUE(GetAppListTestHelper()->IsInFolderView());

  // If the folder view is entered by pressing return key while the focus is on
  // the folder, the focus will move to the first item inside the folder view.
  AppsGridView* folder_grid_view =
      GetAppListTestHelper()->GetBubbleFolderView()->items_grid_view();
  EXPECT_EQ(apps_grid_view->GetFocusManager()->GetFocusedView(),
            folder_grid_view->GetItemViewAt(0));

  // Make 2 no-op moves and one successful move from 0,0 and expect a histogram
  // is recorded only once.
  SimulateKeyPress(ui::VKEY_LEFT, ui::EF_CONTROL_DOWN);
  SimulateKeyReleased(ui::VKEY_LEFT, ui::EF_NONE);

  SimulateKeyPress(ui::VKEY_UP, ui::EF_CONTROL_DOWN);
  SimulateKeyReleased(ui::VKEY_UP, ui::EF_NONE);

  histogram_tester.ExpectBucketCount("Apps.AppListBubbleAppMovingType",
                                     kReorderByKeyboardInFolder, 0);

  SimulateKeyPress(ui::VKEY_RIGHT, ui::EF_CONTROL_DOWN);
  SimulateKeyReleased(ui::VKEY_RIGHT, ui::EF_NONE);

  histogram_tester.ExpectBucketCount("Apps.AppListBubbleAppMovingType",
                                     kReorderByKeyboardInFolder, 1);
}

// Tests that control + shift + arrow moves selected item out of a folder.
TEST_P(ScrollableAppsGridViewTest, ControlShiftArrowMovesItemOutOfFolder) {
  base::HistogramTester histogram_tester;
  AppListFolderItem* folder_item = CreateAndPopulateFolderWithApps(5);
  ShowAppList();
  ScrollableAppsGridView* apps_grid_view = GetScrollableAppsGridView();

  // Select the folder item in the grid.
  AppListItemView* folder_item_view = apps_grid_view->GetItemViewAt(0);
  EXPECT_TRUE(folder_item_view->item()->is_folder());

  // Enter the folder view and move the item out of and to the left of the
  // folder.
  apps_grid_view->GetFocusManager()->SetFocusedView(folder_item_view);
  SimulateKeyPress(ui::VKEY_RETURN);
  ASSERT_TRUE(GetAppListTestHelper()->IsInFolderView());
  SimulateKeyPress(ui::VKEY_LEFT, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
  EXPECT_EQ(apps_grid_view->selected_view(), apps_grid_view->GetItemViewAt(0));
  EXPECT_EQ(4u, folder_item->ChildItemCount());
  histogram_tester.ExpectBucketCount("Apps.AppListBubbleAppMovingType",
                                     kMoveByKeyboardOutOfFolder, 1);

  // Enter the folder view and move the item out of and to the right of the
  // folder.
  apps_grid_view->GetFocusManager()->SetFocusedView(folder_item_view);
  SimulateKeyPress(ui::VKEY_RETURN);
  ASSERT_TRUE(GetAppListTestHelper()->IsInFolderView());
  SimulateKeyPress(ui::VKEY_RIGHT, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
  EXPECT_EQ(apps_grid_view->selected_view(), apps_grid_view->GetItemViewAt(2));
  EXPECT_EQ(3u, folder_item->ChildItemCount());
  histogram_tester.ExpectBucketCount("Apps.AppListBubbleAppMovingType",
                                     kMoveByKeyboardOutOfFolder, 2);

  // Enter the folder view and move the item out of and to the above of the
  // folder.
  apps_grid_view->GetFocusManager()->SetFocusedView(folder_item_view);
  SimulateKeyPress(ui::VKEY_RETURN);
  SimulateKeyPress(ui::VKEY_UP, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
  EXPECT_EQ(apps_grid_view->selected_view(), apps_grid_view->GetItemViewAt(1));
  EXPECT_EQ(2u, folder_item->ChildItemCount());
  histogram_tester.ExpectBucketCount("Apps.AppListBubbleAppMovingType",
                                     kMoveByKeyboardOutOfFolder, 3);

  // Enter the folder view and move the item out of and to the below of the
  // folder.
  apps_grid_view->GetFocusManager()->SetFocusedView(folder_item_view);
  SimulateKeyPress(ui::VKEY_RETURN);
  ASSERT_TRUE(GetAppListTestHelper()->IsInFolderView());
  SimulateKeyPress(ui::VKEY_DOWN, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
  EXPECT_EQ(apps_grid_view->selected_view(), apps_grid_view->GetItemViewAt(3));
  histogram_tester.ExpectBucketCount("Apps.AppListBubbleAppMovingType",
                                     kMoveByKeyboardOutOfFolder, 4);
  EXPECT_FALSE(GetAppListTestHelper()->IsInFolderView());
}

// Verify on the apps grid with zero scroll offset.
TEST_P(ScrollableAppsGridViewTest, VerifyVisibleRangeByDefault) {
  PopulateApps(33);
  ShowAppList();

  const int cols = apps_grid_view_->cols();

  // Assume that the column count is 5 so choose a number that is not the
  // multiple of 5 as the total item count.
  ASSERT_EQ(5, cols);

  // Verify that the items on row 0 to row 3 are visible at default.
  EXPECT_TRUE(
      IsIndexRangeExpected(/*first_index=*/0, /*last_index=*/4 * cols - 1));
}

// Verify on the apps grid whose first row is unfilled.
TEST_P(ScrollableAppsGridViewTest, VerifyVisibleRangeFirstRowUnfilled) {
  PopulateApps(4);
  ShowAppList();

  const int cols = apps_grid_view_->cols();

  // Assume that the column count is 5 so choose a smaller number as the total
  // item count.
  ASSERT_EQ(5, cols);

  // Verify that the items on the first row are visible at default.
  EXPECT_TRUE(IsIndexRangeExpected(/*first_index=*/0, /*last_index=*/3));
}

// Verify on the apps grid whose first row is filled.
TEST_P(ScrollableAppsGridViewTest, VerifyVisibleRangeFirstRowFilled) {
  PopulateApps(5);
  ShowAppList();

  const int cols = apps_grid_view_->cols();

  // Assume that the column count is 5 so apps just fill the first row.
  ASSERT_EQ(5, cols);

  // Verify that the items on the first row are visible at default.
  EXPECT_TRUE(IsIndexRangeExpected(/*first_index=*/0, /*last_index=*/4));
}

// Verify on the apps grid with a non-zero scroll offset.
TEST_P(ScrollableAppsGridViewTest, VerifyVisibleRangeAfterScrolling) {
  PopulateApps(33);
  ShowAppList();

  const int cols = apps_grid_view_->cols();

  // Assume that the column count is 5 so choose a number that is not the
  // multiple of 5 as the total item count.
  ASSERT_EQ(5, cols);

  // Scroll the apps grid so that the item views on the first row are hidden.
  // To calculate the scroll offset, the origin of the item view at (row 1,
  // column 0) should be translated into the scroll content's coordinates.
  views::View* item_view = apps_grid_view_->GetItemViewAt(5);
  views::ScrollView* scroll_view = apps_grid_view_->scroll_view_for_test();
  gfx::Point local_origin;
  views::View::ConvertPointToTarget(item_view, scroll_view->contents(),
                                    &local_origin);
  const int offset = local_origin.y() - scroll_view->GetVisibleRect().y();
  scroll_view_->ScrollToPosition(scroll_view_->vertical_scroll_bar(), offset);

  // Verify that in this case the items on row 1 to row 5 are visible.
  EXPECT_TRUE(
      IsIndexRangeExpected(/*first_index=*/cols, /*last_index=*/6 * cols - 1));
}

// Verify visible items' index range by scrolling to the end on a partially
// filled apps grid.
TEST_P(ScrollableAppsGridViewTest,
       VerifyVisibleRangeAfterScrollingToEndPartiallyFilled) {
  constexpr int populated_app_count = 33;
  PopulateApps(populated_app_count);
  ShowAppList();

  const int cols = apps_grid_view_->cols();

  // Assume that the column count is 5 so choose a number that is not the
  // multiple of 5 as the total item count.
  ASSERT_EQ(5, cols);

  // Scroll to the end.
  scroll_view_->ScrollToPosition(scroll_view_->vertical_scroll_bar(),
                                 std::numeric_limits<int>::max());

  // Verify that the items on row 3 to row 6 are visible.
  EXPECT_TRUE(IsIndexRangeExpected(/*first_index=*/3 * cols,
                                   /*last_index=*/populated_app_count - 1));
}

// Verify visible items' item index range by scrolling to the end on a full
// apps grid.
TEST_P(ScrollableAppsGridViewTest,
       VerifyVisibleRangeAfterScrollingToEndFilled) {
  constexpr int populated_app_count = 35;
  PopulateApps(populated_app_count);
  ShowAppList();

  const int cols = apps_grid_view_->cols();

  // Assume that the column count is 5 so choose a column count's multiple as
  // the total item count.
  ASSERT_EQ(5, cols);

  // Scroll to the end.
  scroll_view_->ScrollToPosition(scroll_view_->vertical_scroll_bar(),
                                 std::numeric_limits<int>::max());

  // Verify that the items on row 3 to row 6 are visible.
  EXPECT_TRUE(IsIndexRangeExpected(/*first_index=*/3 * cols,
                                   /*last_index=*/populated_app_count - 1));
}

// Tests the scrollable apps grid view with app list nudge enabled.
class ScrollableAppsGridViewWithNudgeTest : public ScrollableAppsGridViewTest {
 public:
  // ScrollableAppsGridViewTest:
  void SetUp() override {
    ScrollableAppsGridViewTest::SetUp();
    GetAppListTestHelper()->DisableAppListNudge(false);
  }
};

// Verify on the apps grid with zero scroll offset.
TEST_P(ScrollableAppsGridViewWithNudgeTest, VerifyVisibleRangeByDefault) {
  PopulateApps(33);
  ShowAppList();

  const int cols = apps_grid_view_->cols();

  // Assume that the column count is 5 so choose a number that is not the
  // multiple of 5 as the total item count.
  ASSERT_EQ(5, cols);

  // With the app list reorder nudge is showing, there's enough space to fit
  // only 4 rows of apps in the visible portion of the app list.
  EXPECT_TRUE(
      IsIndexRangeExpected(/*first_index=*/0, /*last_index=*/4 * cols - 1));
}
INSTANTIATE_TEST_SUITE_P(All,
                         ScrollableAppsGridViewWithNudgeTest,
                         testing::Bool());

}  // namespace ash
