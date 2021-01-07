// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_main_view.h"

#include <memory>

#include "ash/app_list/test/app_list_test_model.h"
#include "ash/app_list/test/app_list_test_view_delegate.h"
#include "ash/app_list/views/app_list_folder_view.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/apps_container_view.h"
#include "ash/app_list/views/apps_grid_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/page_switcher.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/app_list/views/test/apps_grid_view_test_api.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view_model.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace test {

namespace {

const int kInitialItems = 2;

class AppListMainViewTest : public views::ViewsTestBase {
 public:
  AppListMainViewTest() = default;
  AppListMainViewTest(const AppListMainViewTest& other) = delete;
  AppListMainViewTest& operator=(const AppListMainViewTest& other) = delete;
  ~AppListMainViewTest() override = default;

  // testing::Test overrides:
  void SetUp() override {
    AppListView::SetShortAnimationForTesting(true);
    views::ViewsTestBase::SetUp();

    // Create, and show the app list is fullscreen apps grid state.
    delegate_ = std::make_unique<AppListTestViewDelegate>();
    app_list_view_ = new AppListView(delegate_.get());
    app_list_view_->InitView(GetContext());
    app_list_view_->Show(AppListViewState::kFullscreenAllApps,
                         /*is_side_shelf=*/false);
    EXPECT_TRUE(app_list_view_->GetWidget()->IsVisible());
  }

  void TearDown() override {
    app_list_view_->GetWidget()->Close();
    views::ViewsTestBase::TearDown();
    AppListView::SetShortAnimationForTesting(false);
  }

  // |point| is in |grid_view|'s coordinates.
  AppListItemView* GetItemViewAtPointInGrid(AppsGridView* grid_view,
                                            const gfx::Point& point) {
    const auto& entries = grid_view->view_model()->entries();
    const auto iter = std::find_if(
        entries.begin(), entries.end(), [&point](const auto& entry) {
          return entry.view->bounds().Contains(point);
        });
    return iter == entries.end() ? nullptr
                                 : static_cast<AppListItemView*>(iter->view);
  }

  void SimulateKeyPress(ui::KeyboardCode key_code) {
    ui::KeyEvent key_press(ui::ET_KEY_PRESSED, key_code, ui::EF_NONE);
    app_list_view_->GetWidget()->OnKeyEvent(&key_press);

    ui::KeyEvent key_release(ui::ET_KEY_RELEASED, key_code, ui::EF_NONE);
    app_list_view_->GetWidget()->OnKeyEvent(&key_release);
  }

  void SimulateClick(views::View* view) {
    gfx::Point center = view->GetLocalBounds().CenterPoint();
    views::View::ConvertPointToWidget(view, &center);

    ui::MouseEvent press_event(ui::ET_MOUSE_PRESSED, center, center,
                               ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                               ui::EF_RIGHT_MOUSE_BUTTON);
    view->GetWidget()->OnMouseEvent(&press_event);

    ui::MouseEvent release_event(
        ui::ET_MOUSE_RELEASED, center, center, ui::EventTimeForNow(),
        ui::EF_LEFT_MOUSE_BUTTON, ui::EF_RIGHT_MOUSE_BUTTON);
    view->GetWidget()->OnMouseEvent(&release_event);
  }

  // |point| is in |grid_view|'s coordinates.
  AppListItemView* SimulateInitiateDrag(AppsGridView* grid_view,
                                        AppsGridView::Pointer pointer,
                                        const gfx::Point& point) {
    AppListItemView* view = GetItemViewAtPointInGrid(grid_view, point);
    DCHECK(view);

    // NOTE: Assumes that the app list view window bounds match the root window
    // bounds.
    gfx::Point root_window_point = point;
    views::View::ConvertPointToWidget(grid_view, &root_window_point);

    grid_view->InitiateDrag(view, pointer, root_window_point, point);
    return view;
  }

  // |point| is in |grid_view|'s coordinates.
  void SimulateUpdateDrag(AppsGridView* grid_view,
                          AppsGridView::Pointer pointer,
                          AppListItemView* drag_view,
                          const gfx::Point& point) {
    DCHECK(drag_view);

    // NOTE: Assumes that the app list view window bounds match the root window
    // bounds.
    gfx::Point root_window_point = point;
    views::View::ConvertPointToWidget(grid_view, &root_window_point);

    ui::MouseEvent drag_event(ui::ET_MOUSE_DRAGGED, root_window_point, point,
                              ui::EventTimeForNow(), 0, 0);

    grid_view->UpdateDragFromItem(pointer, drag_event);
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
        delegate_->GetTestModel()->CreateSingleItemFolder("single_item_folder",
                                                          "single");
    EXPECT_EQ(folder_item,
              delegate_->GetTestModel()->FindFolderItem("single_item_folder"));
    EXPECT_EQ(AppListFolderItem::kItemType, folder_item->GetItemType());

    EXPECT_EQ(1, GetRootViewModel()->view_size());
    AppListItemView* folder_item_view =
        static_cast<AppListItemView*>(GetRootViewModel()->view_at(0));
    EXPECT_EQ(folder_item_view->item(), folder_item);

    // Click on the folder to open it.
    EXPECT_FALSE(GetFolderView()->GetVisible());
    SimulateClick(folder_item_view);

    EXPECT_TRUE(GetFolderView()->GetVisible());

    return folder_item_view;
  }

  AppListItemView* StartDragForReparent(int index_in_folder) {
    // Start to drag the item in folder.
    views::View* item_view = GetFolderViewModel()->view_at(index_in_folder);
    AppListItemView* dragged =
        SimulateInitiateDrag(GetFolderGridView(), AppsGridView::MOUSE,
                             item_view->bounds().CenterPoint());
    EXPECT_EQ(item_view, dragged);
    EXPECT_TRUE(GetRootGridView()->GetVisible());
    EXPECT_TRUE(GetFolderView()->GetVisible());

    // Drag the item completely outside the folder bounds.
    gfx::Point drag_target = gfx::Point(-(item_view->width() + 1) / 2,
                                        -(item_view->height() + 1) / 2);
    // Two update drags needed to actually drag the view. The first changes
    // state and the 2nd one actually moves the view. The 2nd call can be
    // removed when UpdateDrag is fixed.
    SimulateUpdateDrag(GetFolderGridView(), AppsGridView::MOUSE, dragged,
                       drag_target);
    SimulateUpdateDrag(GetFolderGridView(), AppsGridView::MOUSE, dragged,
                       drag_target);

    // Fire reparent timer, which should start when the item exits the folder
    // bounds. The timer closes the folder view.
    EXPECT_TRUE(GetFolderGridView()->FireFolderItemReparentTimerForTest());

    // Note: the folder item is expected to remain visible so it keeps getting
    // drag events, but it should become completely transparent.
    EXPECT_TRUE(GetFolderView()->GetVisible());
    EXPECT_EQ(0.0f, GetFolderGridView()->layer()->opacity());
    return dragged;
  }

 protected:
  AppListView* app_list_view_ = nullptr;  // Owned by native widget.
  std::unique_ptr<AppListTestViewDelegate> delegate_;
};

}  // namespace

// Tests changing the AppListModel when switching profiles.
TEST_F(AppListMainViewTest, ModelChanged) {
  delegate_->GetTestModel()->PopulateApps(kInitialItems);
  EXPECT_EQ(kInitialItems, GetRootViewModel()->view_size());

  // The model is owned by a profile keyed service, which is never destroyed
  // until after profile switching.
  std::unique_ptr<AppListModel> old_model(delegate_->ReleaseTestModel());
  std::unique_ptr<SearchModel> old_search_model(
      delegate_->ReleaseTestSearchModel());

  const int kReplacementItems = 5;
  delegate_->ReplaceTestModel(kReplacementItems);
  main_view()->ModelChanged();
  EXPECT_EQ(kReplacementItems, GetRootViewModel()->view_size());
}

// Tests dragging an item out of a single item folder and drop it at the last
// slot.
TEST_F(AppListMainViewTest, DragLastItemFromFolderAndDropAtLastSlot) {
  AppListItemView* folder_item_view = CreateAndOpenSingleItemFolder();
  const gfx::Rect first_slot_tile = folder_item_view->bounds();

  EXPECT_EQ(1, GetFolderViewModel()->view_size());

  AppListItemView* dragged = StartDragForReparent(0);

  // Drop it to the slot on the right of first slot.
  gfx::Rect drop_target_tile(first_slot_tile);
  drop_target_tile.Offset(first_slot_tile.width() * 2, 0);
  gfx::Point point = drop_target_tile.CenterPoint();
  SimulateUpdateDrag(GetFolderGridView(), AppsGridView::MOUSE, dragged, point);

  // Drop it.
  GetFolderGridView()->EndDrag(false);

  // Folder icon view should be gone and there is only one item view.
  EXPECT_EQ(1, GetRootViewModel()->view_size());
  EXPECT_EQ(AppListItemView::kViewClassName,
            static_cast<views::View*>(GetRootViewModel()->view_at(0))
                ->GetClassName());

  // The item view should be in slot 1 instead of slot 2 where it is dropped.
  AppsGridViewTestApi root_grid_view_test_api(GetRootGridView());
  root_grid_view_test_api.LayoutToIdealBounds();
  EXPECT_EQ(first_slot_tile, GetRootViewModel()->view_at(0)->bounds());

  // Single item folder should be auto removed.
  EXPECT_FALSE(delegate_->GetTestModel()->FindFolderItem("single_item_folder"));

  // Ensure keyboard selection works on the root grid view after a reparent.
  // This is a regression test for https://crbug.com/466058.
  SimulateKeyPress(ui::VKEY_RIGHT);

  // Initial key press moves focus to the search box. The next one should move
  // the focus to the root apps grid.
  EXPECT_TRUE(search_box_view()->search_box()->HasFocus());

  SimulateKeyPress(ui::VKEY_LEFT);

  EXPECT_TRUE(GetRootGridView()->has_selected_view());
  EXPECT_FALSE(GetFolderGridView()->has_selected_view());
}

// Tests dragging an item out of a single item folder and dropping it onto the
// page switcher. Regression test for http://crbug.com/415530/.
TEST_F(AppListMainViewTest, DragReparentItemOntoPageSwitcher) {
  AppListItemView* folder_item_view = CreateAndOpenSingleItemFolder();
  ASSERT_TRUE(folder_item_view);

  // Number of apps to populate. Should provide more than 1 page of apps (5*4 =
  // 20).
  const int kNumApps = 30;
  delegate_->GetTestModel()->PopulateApps(kNumApps);

  EXPECT_EQ(1, GetFolderViewModel()->view_size());
  EXPECT_EQ(kNumApps + 1, GetRootViewModel()->view_size());

  AppListItemView* dragged = StartDragForReparent(0);

  // Drag the reparent item to the page switcher.
  gfx::Point point = GetPageSwitcherView()->GetLocalBounds().CenterPoint();
  views::View::ConvertPointToTarget(GetPageSwitcherView(), GetFolderGridView(),
                                    &point);
  SimulateUpdateDrag(GetFolderGridView(), AppsGridView::MOUSE, dragged, point);

  // Drop it.
  GetFolderGridView()->EndDrag(false);

  // The folder should not be destroyed.
  EXPECT_EQ(kNumApps + 1, GetRootViewModel()->view_size());
  EXPECT_TRUE(delegate_->GetTestModel()->FindFolderItem("single_item_folder"));
  EXPECT_EQ(1, GetFolderViewModel()->view_size());
}

// Test that an interrupted drag while reparenting an item from a folder, when
// canceled via the root grid, correctly forwards the cancelation to the drag
// ocurring from the folder.
TEST_F(AppListMainViewTest, MouseDragItemOutOfFolderWithCancel) {
  CreateAndOpenSingleItemFolder();
  AppListItemView* dragged = StartDragForReparent(0);

  // Now add an item to the model, not in any folder, e.g., as if by Sync.
  EXPECT_TRUE(GetRootGridView()->has_dragged_view());
  EXPECT_TRUE(GetFolderGridView()->has_dragged_view());
  delegate_->GetTestModel()->CreateAndAddItem("Extra");

  // The drag operation should get canceled.
  EXPECT_FALSE(GetRootGridView()->has_dragged_view());
  EXPECT_FALSE(GetFolderGridView()->has_dragged_view());

  // Additional mouse move operations should be ignored.
  gfx::Point point(1, 1);
  SimulateUpdateDrag(GetFolderGridView(), AppsGridView::MOUSE, dragged, point);
  EXPECT_FALSE(GetRootGridView()->has_dragged_view());
  EXPECT_FALSE(GetFolderGridView()->has_dragged_view());
}

// Test that dragging an app out of a single item folder and reparenting it
// back into its original folder results in a cancelled reparent. This is a
// regression test for http://crbug.com/429083.
TEST_F(AppListMainViewTest, ReparentSingleItemOntoSelf) {
  // Add a folder with 1 item.
  AppListItemView* folder_item_view = CreateAndOpenSingleItemFolder();
  std::string folder_id = folder_item_view->item()->id();

  // Add another top level app.
  delegate_->GetTestModel()->PopulateApps(1);
  gfx::Point drag_point = folder_item_view->bounds().CenterPoint();

  views::View::ConvertPointToTarget(GetRootGridView(), GetFolderGridView(),
                                    &drag_point);

  AppListItemView* dragged = StartDragForReparent(0);

  // Drag the reparent item back into its folder.
  SimulateUpdateDrag(GetFolderGridView(), AppsGridView::MOUSE, dragged,
                     drag_point);
  GetFolderGridView()->EndDrag(false);

  // The app list model should remain unchanged.
  EXPECT_EQ(1, GetFolderViewModel()->view_size());
  EXPECT_EQ(2, GetRootViewModel()->view_size());
  EXPECT_EQ(folder_id, GetRootGridView()->GetItemViewAt(0)->item()->id());
  EXPECT_TRUE(delegate_->GetTestModel()->FindFolderItem("single_item_folder"));
}

}  // namespace test
}  // namespace ash
