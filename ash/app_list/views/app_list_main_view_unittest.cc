// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_main_view.h"

#include <list>
#include <memory>
#include <string>
#include <utility>

#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/model/app_list_test_model.h"
#include "ash/app_list/quick_app_access_model.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/app_list_folder_view.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/apps_container_view.h"
#include "ash/app_list/views/apps_grid_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/page_switcher.h"
#include "ash/app_list/views/paged_apps_grid_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "ui/compositor/layer.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/types/event_type.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view_model.h"

namespace ash {

// Parameterized by drag and drop refactor enabled/disabled.
class AppListMainViewTest : public AshTestBase,
                            public testing::WithParamInterface<bool> {
 public:
  AppListMainViewTest() : is_drag_drop_refactor_enabled_(GetParam()) {}
  AppListMainViewTest(const AppListMainViewTest& other) = delete;
  AppListMainViewTest& operator=(const AppListMainViewTest& other) = delete;
  ~AppListMainViewTest() override = default;

  // testing::Test overrides:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatureState(
        app_list_features::kDragAndDropRefactor,
        is_drag_drop_refactor_enabled_);
    AshTestBase::SetUp();

    // Create and show the app list in fullscreen apps grid state.
    // Tablet mode uses a fullscreen AppListMainView.
    auto* helper = GetAppListTestHelper();
    Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
    app_list_view_ = helper->GetAppListView();
  }

  test::AppListTestModel* GetTestModel() {
    return GetAppListTestHelper()->model();
  }

  // |point| is in |grid_view|'s coordinates.
  AppListItemView* GetItemViewAtPointInGrid(AppsGridView* grid_view,
                                            const gfx::Point& point) {
    const auto& entries = grid_view->view_model()->entries();
    const auto iter =
        base::ranges::find_if(entries, [&point](const auto& entry) {
          return entry.view->bounds().Contains(point);
        });
    return iter == entries.end() ? nullptr
                                 : static_cast<AppListItemView*>(iter->view);
  }

  // |point| is in |grid_view|'s coordinates.
  void SimulateUpdateDragInGridView(AppsGridView* grid_view,
                                    AppListItemView* drag_view,
                                    const gfx::Point& point) {
    // NOTE: Assumes that the app list view window bounds match the root window
    // bounds.
    gfx::Point root_window_point = point;
    views::View::ConvertPointToScreen(grid_view, &root_window_point);
    GetEventGenerator()->MoveMouseTo(root_window_point);
  }

  AppListMainView* main_view() { return app_list_view_->app_list_main_view(); }

  ContentsView* contents_view() { return main_view()->contents_view(); }

  SearchBoxView* search_box_view() { return main_view()->search_box_view(); }

  AppsGridView* GetRootGridView() {
    return contents_view()->apps_container_view()->apps_grid_view();
  }

  AppListFolderView* GetFolderView() {
    return contents_view()->apps_container_view()->app_list_folder_view();
  }

  PageSwitcher* GetPageSwitcherView() {
    return contents_view()->apps_container_view()->page_switcher();
  }

  AppsGridView* GetFolderGridView() {
    return GetFolderView()->items_grid_view();
  }

  const views::ViewModelT<AppListItemView>* GetRootViewModel() {
    return GetRootGridView()->view_model();
  }

  const views::ViewModelT<AppListItemView>* GetFolderViewModel() {
    return GetFolderGridView()->view_model();
  }

  AppListItemView* CreateAndOpenSingleItemFolder() {
    // Prepare single folder with a single item in it.
    AppListFolderItem* folder_item =
        GetTestModel()->CreateSingleItemFolder("single_item_folder", "single");
    views::test::RunScheduledLayout(GetRootGridView());
    EXPECT_EQ(folder_item,
              GetTestModel()->FindFolderItem("single_item_folder"));
    EXPECT_EQ(AppListFolderItem::kItemType, folder_item->GetItemType());

    EXPECT_EQ(1u, GetRootViewModel()->view_size());
    AppListItemView* folder_item_view =
        static_cast<AppListItemView*>(GetRootViewModel()->view_at(0));
    EXPECT_EQ(folder_item_view->item(), folder_item);

    // Click on the folder to open it.
    EXPECT_FALSE(GetFolderView()->GetVisible());
    LeftClickOn(folder_item_view);

    EXPECT_TRUE(GetFolderView()->GetVisible());

    return folder_item_view;
  }

  AppListItemView* StartDragOnItemInFolderAt(int index_in_folder) {
    DCHECK(GetAppListTestHelper()->IsInFolderView());
    views::View* item_view = GetFolderViewModel()->view_at(index_in_folder);

    AppListItemView* view = GetItemViewAtPointInGrid(
        GetFolderGridView(), item_view->bounds().CenterPoint());
    DCHECK(view);
    EXPECT_EQ(view, item_view);

    GetEventGenerator()->MoveMouseTo(
        view->GetIconBoundsInScreen().CenterPoint());
    GetEventGenerator()->PressLeftButton();
    EXPECT_TRUE(view->FireMouseDragTimerForTest());
    return view;
  }

  AppListItemView* DragItemOutsideFolder(AppListItemView* item_view) {
    DCHECK(GetAppListTestHelper()->IsInFolderView());
    // Drag the item completely outside the folder bounds.
    GetEventGenerator()->MoveMouseTo(
        GetFolderGridView()->GetBoundsInScreen().bottom_right());
    GetEventGenerator()->MoveMouseBy(10, 10);

    // Fire reparent timer, which should start when the item exits the folder
    // bounds. The timer closes the folder view.
    EXPECT_TRUE(GetFolderGridView()->FireFolderItemReparentTimerForTest());

    // Generate OnDragExit/OnDragEnter
    GetEventGenerator()->MoveMouseTo(
        GetRootGridView()->GetBoundsInScreen().CenterPoint());

    // Note: with the old behaviour, the folder item is expected to remain
    // visible so it keeps getting drag events, but it should become
    // completely transparent.
    // The drag and drop refactor, expects the folder grid view to end drag once
    // the dragged view exits the host.
    EXPECT_EQ(!is_drag_drop_refactor_enabled_, GetFolderView()->GetVisible());
    if (!is_drag_drop_refactor_enabled_) {
      EXPECT_EQ(0.0f, GetFolderGridView()->layer()->opacity());
    }
    EXPECT_TRUE(GetRootGridView()->has_dragged_item());
    EXPECT_EQ(!is_drag_drop_refactor_enabled_,
              GetFolderGridView()->has_dragged_item());
    return item_view;
  }

  void RunInitialReparentChecks() {
    EXPECT_TRUE(GetRootGridView()->GetVisible());
    EXPECT_TRUE(GetFolderView()->GetVisible());
    EXPECT_FALSE(GetRootGridView()->has_dragged_item());
    EXPECT_TRUE(GetFolderGridView()->has_dragged_item());
  }

  void ClickButton(views::Button* button) {
    views::test::ButtonTestApi(button).NotifyClick(ui::MouseEvent(
        ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(), base::TimeTicks(),
        ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  }

  bool is_drag_drop_refactor_enabled() {
    return is_drag_drop_refactor_enabled_;
  }

 protected:
  raw_ptr<AppListView, DanglingUntriaged | ExperimentalAsh> app_list_view_ =
      nullptr;  // Owned by native widget.
 private:
  const bool is_drag_drop_refactor_enabled_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All, AppListMainViewTest, testing::Bool());

// Tests that the close button becomes invisible after close button is clicked.
TEST_P(AppListMainViewTest, CloseButtonInvisibleAfterCloseButtonClicked) {
  PressAndReleaseKey(ui::VKEY_A);
  ClickButton(search_box_view()->close_button());
  EXPECT_FALSE(search_box_view()->close_button()->GetVisible());
}

// Tests that the search box becomes empty after close button is clicked.
TEST_P(AppListMainViewTest, SearchBoxEmptyAfterCloseButtonClicked) {
  PressAndReleaseKey(ui::VKEY_A);
  ClickButton(search_box_view()->close_button());
  EXPECT_TRUE(search_box_view()->search_box()->GetText().empty());
}

// Tests that the search box is no longer active after close button is clicked.
TEST_P(AppListMainViewTest, SearchBoxActiveAfterCloseButtonClicked) {
  PressAndReleaseKey(ui::VKEY_A);
  ClickButton(search_box_view()->close_button());
  EXPECT_FALSE(search_box_view()->is_search_box_active());
}

// Tests changing the AppListModel when switching profiles.
TEST_P(AppListMainViewTest, ModelChanged) {
  const size_t kInitialItems = 2;
  GetTestModel()->PopulateApps(kInitialItems);
  EXPECT_EQ(kInitialItems, GetRootViewModel()->view_size());

  AppListModel* old_model = GetAppListTestHelper()->model();
  SearchModel* old_search_model = GetAppListTestHelper()->search_model();
  QuickAppAccessModel* old_quick_app_access_model =
      GetAppListTestHelper()->quick_app_access_model();

  // Simulate a profile switch (which switches the app list models).
  auto search_model = std::make_unique<SearchModel>();
  auto model = std::make_unique<test::AppListTestModel>();
  auto quick_app_access_model = std::make_unique<QuickAppAccessModel>();
  const size_t kReplacementItems = 5;
  model->PopulateApps(kReplacementItems);
  AppListModelProvider::Get()->SetActiveModel(model.get(), search_model.get(),
                                              quick_app_access_model.get());
  EXPECT_EQ(kReplacementItems, GetRootViewModel()->view_size());

  // Replace the old model so observers on `model` are removed before test
  // shutdown.
  AppListModelProvider::Get()->SetActiveModel(old_model, old_search_model,
                                              old_quick_app_access_model);
}

// Tests dragging an item out of a single item folder and dropping it onto the
// page switcher. Regression test for http://crbug.com/415530/.
TEST_P(AppListMainViewTest, DragReparentItemOntoPageSwitcher) {
  AppListItemView* folder_item_view = CreateAndOpenSingleItemFolder();
  ASSERT_TRUE(folder_item_view);

  // Number of apps to populate. Should provide more than 1 page of apps (5*4 =
  // 20).
  const size_t kNumApps = 30;
  GetTestModel()->PopulateApps(kNumApps);
  views::test::RunScheduledLayout(GetRootGridView());

  EXPECT_EQ(1u, GetFolderViewModel()->view_size());
  EXPECT_EQ(kNumApps + 1, GetRootViewModel()->view_size());

  AppListItemView* dragged = StartDragOnItemInFolderAt(0);

  auto* generator = GetEventGenerator();
  std::list<base::OnceClosure> tasks;
  tasks.push_back(
      base::BindLambdaForTesting([&]() { RunInitialReparentChecks(); }));
  tasks.push_back(
      base::BindLambdaForTesting([&]() { DragItemOutsideFolder(dragged); }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Drag the reparent item to the page switcher.
    gfx::Point point = GetPageSwitcherView()->GetLocalBounds().CenterPoint();
    views::View::ConvertPointToTarget(GetPageSwitcherView(),
                                      GetFolderGridView(), &point);
    SimulateUpdateDragInGridView(GetFolderGridView(), dragged, point);
  }));
  tasks.push_back(
      base::BindLambdaForTesting([&]() { generator->ReleaseLeftButton(); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch=*/false);

  // The folder should not be destroyed.
  EXPECT_EQ(kNumApps + 1, GetRootViewModel()->view_size());
  AppListFolderItem* const folder_item =
      GetTestModel()->FindFolderItem("single_item_folder");
  ASSERT_TRUE(folder_item);
  EXPECT_EQ(1u, folder_item->item_list()->item_count());
}

// Test that an interrupted drag while reparenting an item from a folder, when
// canceled via the root grid, correctly forwards the cancelation to the drag
// occurring from the folder.
TEST_P(AppListMainViewTest, MouseDragItemOutOfFolderWithCancel) {
  CreateAndOpenSingleItemFolder();
  AppListItemView* dragged = StartDragOnItemInFolderAt(0);

  std::list<base::OnceClosure> tasks;
  tasks.push_back(
      base::BindLambdaForTesting([&]() { RunInitialReparentChecks(); }));
  tasks.push_back(
      base::BindLambdaForTesting([&]() { DragItemOutsideFolder(dragged); }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Now add an item to the model, not in any folder, e.g., as if by Sync.
    GetTestModel()->CreateAndAddItem("Extra");
    // The drag operation is canceled.
    EXPECT_FALSE(GetRootGridView()->has_dragged_item());
    EXPECT_FALSE(GetFolderGridView()->has_dragged_item());
  }));
  if (is_drag_drop_refactor_enabled()) {
    tasks.push_back(base::BindLambdaForTesting([&]() {
      // Required by the drag and drop controller to end the loop, since the
      // action does not cancel the drag sequence.
      GetEventGenerator()->ReleaseLeftButton();
    }));
  }
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch=*/false);

  // Additional mouse move operations should be ignored.
  gfx::Point point(1, 1);
  SimulateUpdateDragInGridView(GetFolderGridView(), dragged, point);
  EXPECT_FALSE(GetRootGridView()->has_dragged_item());
  EXPECT_FALSE(GetFolderGridView()->has_dragged_item());
}

// Test that dragging an app out of a single item folder and reparenting it
// back into its original folder results in a cancelled reparent. This is a
// regression test for http://crbug.com/429083.
TEST_P(AppListMainViewTest, ReparentSingleItemOntoSelf) {
  // Add a folder with 1 item.
  AppListItemView* folder_item_view = CreateAndOpenSingleItemFolder();
  std::string folder_id = folder_item_view->item()->id();

  // Add another top level app.
  GetTestModel()->PopulateApps(1);
  gfx::Point drag_point = folder_item_view->bounds().CenterPoint();

  views::View::ConvertPointToTarget(GetRootGridView(), GetFolderGridView(),
                                    &drag_point);

  AppListItemView* dragged = StartDragOnItemInFolderAt(0);

  auto* generator = GetEventGenerator();
  std::list<base::OnceClosure> tasks;
  tasks.push_back(
      base::BindLambdaForTesting([&]() { RunInitialReparentChecks(); }));
  tasks.push_back(
      base::BindLambdaForTesting([&]() { DragItemOutsideFolder(dragged); }));
  tasks.push_back(base::BindLambdaForTesting([&]() {
    // Drag the reparent item back into its folder.
    SimulateUpdateDragInGridView(GetFolderGridView(), dragged, drag_point);
  }));
  tasks.push_back(
      base::BindLambdaForTesting([&]() { generator->ReleaseLeftButton(); }));
  MaybeRunDragAndDropSequenceForAppList(&tasks, /*is_touch=*/false);

  // The app list model should remain unchanged.
  EXPECT_EQ(2u, GetRootViewModel()->view_size());
  EXPECT_EQ(folder_id, GetRootGridView()->GetItemViewAt(0)->item()->id());
  AppListFolderItem* const folder_item =
      GetTestModel()->FindFolderItem("single_item_folder");
  ASSERT_TRUE(folder_item);
  EXPECT_EQ(1u, folder_item->item_list()->item_count());
}

}  // namespace ash
