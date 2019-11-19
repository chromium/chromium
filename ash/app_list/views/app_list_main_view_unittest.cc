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
#include "ash/app_list/views/search_box_view.h"
#include "ash/app_list/views/test/apps_grid_view_test_api.h"
#include "ash/public/cpp/app_list/app_list_switches.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view_model.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace test {

namespace {

const int kInitialItems = 2;

class GridViewVisibleWaiter {
 public:
  explicit GridViewVisibleWaiter(AppsGridView* grid_view)
      : grid_view_(grid_view) {}
  ~GridViewVisibleWaiter() {}

  void Wait() {
    if (grid_view_->GetVisible())
      return;

    check_timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(50),
                       base::Bind(&GridViewVisibleWaiter::OnTimerCheck,
                                  base::Unretained(this)));
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    check_timer_.Stop();
  }

 private:
  void OnTimerCheck() {
    if (grid_view_->GetVisible())
      run_loop_->Quit();
  }

  AppsGridView* grid_view_;
  std::unique_ptr<base::RunLoop> run_loop_;
  base::RepeatingTimer check_timer_;

  DISALLOW_COPY_AND_ASSIGN(GridViewVisibleWaiter);
};

class AppListMainViewTest : public views::ViewsTestBase {
 public:
  AppListMainViewTest()
      : main_widget_(nullptr),
        main_view_(nullptr),
        search_box_widget_(nullptr),
        search_box_view_(nullptr) {}

  ~AppListMainViewTest() override {}

  // testing::Test overrides:
  void SetUp() override {
    // TODO(newcomer): this test needs to be reevaluated for the fullscreen app
    // list (http://crbug.com/759779).
    views::ViewsTestBase::SetUp();
#if 0
    delegate_ = std::make_unique<AppListTestViewDelegate>();
    main_view_ = new AppListMainView(delegate_.get(), nullptr);
    main_view_->SetPaintToLayer();

    search_box_view_ = new SearchBoxView(main_view_, delegate_.get());
    main_view_->Init(0, search_box_view_);

    main_widget_ = new views::Widget;
    views::Widget::InitParams main_widget_params =
        CreateParams(views::Widget::InitParams::TYPE_POPUP);
    main_widget_params.bounds.set_size(main_view_->GetPreferredSize());
    main_widget_->Init(main_widget_params);
    main_widget_->SetContentsView(main_view_);

    search_box_widget_ = new views::Widget;
    views::Widget::InitParams search_box_widget_params =
        CreateParams(views::Widget::InitParams::TYPE_CONTROL);
    search_box_widget_params.parent = main_widget_->GetNativeView();
    search_box_widget_params.opacity =
        views::Widget::InitParams::TRANSLUCENT_WINDOW;
    search_box_widget_->Init(search_box_widget_params);
    search_box_widget_->SetContentsView(search_box_view_);
#endif
  }

  void TearDown() override {
    // TODO(newcomer): this test needs to be reevaluated for the fullscreen app
    // list (http://crbug.com/759779).
    views::ViewsTestBase::TearDown();
#if 0
    main_widget_->Close();
    views::ViewsTestBase::TearDown();
    delegate_.reset();
#endif
  }

  // |point| is in |grid_view|'s coordinates.
  AppListItemView* GetItemViewAtPointInGrid(AppsGridView* grid_view,
                                            const gfx::Point& point) {
    const views::ViewModelT<AppListItemView>* view_model =
        grid_view->view_model();
    for (int i = 0; i < view_model->view_size(); ++i) {
      views::View* view = view_model->view_at(i);
      if (view->bounds().Contains(point)) {
        return static_cast<AppListItemView*>(view);
      }
    }

    return nullptr;
  }

  void SimulateClick(views::View* view) {
    gfx::Point center = view->GetLocalBounds().CenterPoint();
    view->OnMousePressed(ui::MouseEvent(
        ui::ET_MOUSE_PRESSED, center, center, ui::EventTimeForNow(),
        ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
    view->OnMouseReleased(ui::MouseEvent(
        ui::ET_MOUSE_RELEASED, center, center, ui::EventTimeForNow(),
        ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  }

  // |point| is in |grid_view|'s coordinates.
  AppListItemView* SimulateInitiateDrag(AppsGridView* grid_view,
                                        AppsGridView::Pointer pointer,
                                        const gfx::Point& point) {
    AppListItemView* view = GetItemViewAtPointInGrid(grid_view, point);
    DCHECK(view);

    gfx::Point translated =
        gfx::PointAtOffsetFromOrigin(point - view->origin());
    grid_view->InitiateDrag(view, pointer, translated, point);
    return view;
  }

  // |point| is in |grid_view|'s coordinates.
  void SimulateUpdateDrag(AppsGridView* grid_view,
                          AppsGridView::Pointer pointer,
                          AppListItemView* drag_view,
                          const gfx::Point& point) {
    DCHECK(drag_view);
    gfx::Point translated =
        gfx::PointAtOffsetFromOrigin(point - drag_view->origin());
    ui::MouseEvent drag_event(ui::ET_MOUSE_DRAGGED, translated, point,
                              ui::EventTimeForNow(), 0, 0);
    grid_view->UpdateDragFromItem(pointer, drag_event);
  }

  ContentsView* GetContentsView() { return main_view_->contents_view(); }

  AppsGridView* RootGridView() {
    return GetContentsView()->GetAppsContainerView()->apps_grid_view();
  }

  AppListFolderView* FolderView() {
    return GetContentsView()->GetAppsContainerView()->app_list_folder_view();
  }

  AppsGridView* FolderGridView() { return FolderView()->items_grid_view(); }

  const views::ViewModelT<AppListItemView>* RootViewModel() {
    return RootGridView()->view_model();
  }

  const views::ViewModelT<AppListItemView>* FolderViewModel() {
    return FolderGridView()->view_model();
  }

  AppListItemView* CreateAndOpenSingleItemFolder() {
    // Prepare single folder with a single item in it.
    AppListFolderItem* folder_item =
        delegate_->GetTestModel()->CreateSingleItemFolder("single_item_folder",
                                                          "single");
    EXPECT_EQ(folder_item,
              delegate_->GetTestModel()->FindFolderItem("single_item_folder"));
    EXPECT_EQ(AppListFolderItem::kItemType, folder_item->GetItemType());

    EXPECT_EQ(1, RootViewModel()->view_size());
    AppListItemView* folder_item_view =
        static_cast<AppListItemView*>(RootViewModel()->view_at(0));
    EXPECT_EQ(folder_item_view->item(), folder_item);

    // Click on the folder to open it.
    EXPECT_FALSE(FolderView()->GetVisible());
    SimulateClick(folder_item_view);
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(FolderView()->GetVisible());

    return folder_item_view;
  }

  AppListItemView* StartDragForReparent(int index_in_folder) {
    // Start to drag the item in folder.
    views::View* item_view = FolderViewModel()->view_at(index_in_folder);
    gfx::Point point = item_view->bounds().CenterPoint();
    AppListItemView* dragged =
        SimulateInitiateDrag(FolderGridView(), AppsGridView::MOUSE, point);
    EXPECT_EQ(item_view, dragged);
    EXPECT_FALSE(RootGridView()->GetVisible());
    EXPECT_TRUE(FolderView()->GetVisible());

    // Drag it to top left corner.
    point = gfx::Point(0, 0);
    // Two update drags needed to actually drag the view. The first changes
    // state and the 2nd one actually moves the view. The 2nd call can be
    // removed when UpdateDrag is fixed.
    SimulateUpdateDrag(FolderGridView(), AppsGridView::MOUSE, dragged, point);
    SimulateUpdateDrag(FolderGridView(), AppsGridView::MOUSE, dragged, point);
    base::RunLoop().RunUntilIdle();

    // Wait until the folder view is invisible and root grid view shows up.
    GridViewVisibleWaiter(RootGridView()).Wait();
    EXPECT_TRUE(RootGridView()->GetVisible());
    EXPECT_EQ(0, FolderView()->layer()->opacity());

    return dragged;
  }

 protected:
  views::Widget* main_widget_;  // Owned by native window.
  AppListMainView* main_view_;  // Owned by |main_widget_|.
  std::unique_ptr<AppListTestViewDelegate> delegate_;
  views::Widget* search_box_widget_;  // Owned by |main_widget_|.
  SearchBoxView* search_box_view_;    // Owned by |search_box_widget_|.

 private:
  DISALLOW_COPY_AND_ASSIGN(AppListMainViewTest);
};

}  // namespace

// Tests changing the AppListModel when switching profiles.
TEST_F(AppListMainViewTest, DISABLED_ModelChanged) {
  // TODO(newcomer): this test needs to be reevaluated for the fullscreen app
  // list (http://crbug.com/759779).
  delegate_->GetTestModel()->PopulateApps(kInitialItems);
  EXPECT_EQ(kInitialItems, RootViewModel()->view_size());

  // The model is owned by a profile keyed service, which is never destroyed
  // until after profile switching.
  std::unique_ptr<AppListModel> old_model(delegate_->ReleaseTestModel());

  const int kReplacementItems = 5;
  delegate_->ReplaceTestModel(kReplacementItems);
  main_view_->ModelChanged();
  EXPECT_EQ(kReplacementItems, RootViewModel()->view_size());
}

// Tests dragging an item out of a single item folder and drop it at the last
// slot.
TEST_F(AppListMainViewTest, DISABLED_DragLastItemFromFolderAndDropAtLastSlot) {
  // TODO(newcomer): this test needs to be reevaluated for the fullscreen app
  // list (http://crbug.com/759779).
  AppListItemView* folder_item_view = CreateAndOpenSingleItemFolder();
  const gfx::Rect first_slot_tile = folder_item_view->bounds();

  EXPECT_EQ(1, FolderViewModel()->view_size());

  AppListItemView* dragged = StartDragForReparent(0);

  // Drop it to the slot on the right of first slot.
  gfx::Rect drop_target_tile(first_slot_tile);
  drop_target_tile.Offset(first_slot_tile.width() * 2, 0);
  gfx::Point point = drop_target_tile.CenterPoint();
  SimulateUpdateDrag(FolderGridView(), AppsGridView::MOUSE, dragged, point);

  // Drop it.
  FolderGridView()->EndDrag(false);

  // Folder icon view should be gone and there is only one item view.
  EXPECT_EQ(1, RootViewModel()->view_size());
  EXPECT_EQ(
      AppListItemView::kViewClassName,
      static_cast<views::View*>(RootViewModel()->view_at(0))->GetClassName());

  // The item view should be in slot 1 instead of slot 2 where it is dropped.
  AppsGridViewTestApi root_grid_view_test_api(RootGridView());
  root_grid_view_test_api.LayoutToIdealBounds();
  EXPECT_EQ(first_slot_tile, RootViewModel()->view_at(0)->bounds());

  // Single item folder should be auto removed.
  EXPECT_EQ(nullptr,
            delegate_->GetTestModel()->FindFolderItem("single_item_folder"));

  // Ensure keyboard selection works on the root grid view after a reparent.
  // This is a regression test for https://crbug.com/466058.
  ui::KeyEvent key_event(ui::ET_KEY_PRESSED, ui::VKEY_RIGHT, ui::EF_NONE);
  GetContentsView()->GetAppsContainerView()->OnKeyPressed(key_event);

  EXPECT_TRUE(RootGridView()->has_selected_view());
  EXPECT_FALSE(FolderGridView()->has_selected_view());
}

// Tests dragging an item out of a single item folder and dropping it onto the
// page switcher. Regression test for http://crbug.com/415530/.
TEST_F(AppListMainViewTest, DISABLED_DragReparentItemOntoPageSwitcher) {
  // TODO(newcomer): this test needs to be reevaluated for the fullscreen app
  // list (http://crbug.com/759779).
  // Number of apps to populate. Should provide more than 1 page of apps (6*4 =
  // 24).
  const int kNumApps = 30;

  // Ensure we are on the apps grid view page.
  ContentsView* contents_view = GetContentsView();
  contents_view->SetActiveState(ash::AppListState::kStateApps);
  contents_view->Layout();

  AppListItemView* folder_item_view = CreateAndOpenSingleItemFolder();
  const gfx::Rect first_slot_tile = folder_item_view->bounds();

  delegate_->GetTestModel()->PopulateApps(kNumApps);

  EXPECT_EQ(1, FolderViewModel()->view_size());
  EXPECT_EQ(kNumApps + 1, RootViewModel()->view_size());

  AppListItemView* dragged = StartDragForReparent(0);

  gfx::Rect grid_view_bounds = RootGridView()->bounds();
  // Drag the reparent item to the page switcher.
  gfx::Point point =
      gfx::Point(grid_view_bounds.width() / 2,
                 grid_view_bounds.bottom() - first_slot_tile.height());
  SimulateUpdateDrag(FolderGridView(), AppsGridView::MOUSE, dragged, point);

  // Drop it.
  FolderGridView()->EndDrag(false);

  // The folder should be destroyed.
  EXPECT_EQ(kNumApps + 1, RootViewModel()->view_size());
  EXPECT_EQ(nullptr,
            delegate_->GetTestModel()->FindFolderItem("single_item_folder"));
}

// Test that an interrupted drag while reparenting an item from a folder, when
// canceled via the root grid, correctly forwards the cancelation to the drag
// ocurring from the folder.
TEST_F(AppListMainViewTest, DISABLED_MouseDragItemOutOfFolderWithCancel) {
  // TODO(newcomer): this test needs to be reevaluated for the fullscreen app
  // list (http://crbug.com/759779).
  CreateAndOpenSingleItemFolder();
  AppListItemView* dragged = StartDragForReparent(0);

  // Now add an item to the model, not in any folder, e.g., as if by Sync.
  EXPECT_TRUE(RootGridView()->has_dragged_view());
  EXPECT_TRUE(FolderGridView()->has_dragged_view());
  delegate_->GetTestModel()->CreateAndAddItem("Extra");

  // The drag operation should get canceled.
  EXPECT_FALSE(RootGridView()->has_dragged_view());
  EXPECT_FALSE(FolderGridView()->has_dragged_view());

  // Additional mouse move operations should be ignored.
  gfx::Point point(1, 1);
  SimulateUpdateDrag(FolderGridView(), AppsGridView::MOUSE, dragged, point);
  EXPECT_FALSE(RootGridView()->has_dragged_view());
  EXPECT_FALSE(FolderGridView()->has_dragged_view());
}

// Test that dragging an app out of a single item folder and reparenting it
// back into its original folder results in a cancelled reparent. This is a
// regression test for http://crbug.com/429083.
TEST_F(AppListMainViewTest, DISABLED_ReparentSingleItemOntoSelf) {
  // TODO(newcomer): this test needs to be reevaluated for the fullscreen app
  // list (http://crbug.com/759779).
  // Add a folder with 1 item.
  AppListItemView* folder_item_view = CreateAndOpenSingleItemFolder();
  std::string folder_id = folder_item_view->item()->id();

  // Add another top level app.
  delegate_->GetTestModel()->PopulateApps(1);
  gfx::Point drag_point = folder_item_view->bounds().CenterPoint();

  views::View::ConvertPointToTarget(RootGridView(), FolderGridView(),
                                    &drag_point);

  AppListItemView* dragged = StartDragForReparent(0);

  // Drag the reparent item back into its folder.
  SimulateUpdateDrag(FolderGridView(), AppsGridView::MOUSE, dragged,
                     drag_point);
  FolderGridView()->EndDrag(false);

  // The app list model should remain unchanged.
  EXPECT_EQ(1, FolderViewModel()->view_size());
  EXPECT_EQ(2, RootViewModel()->view_size());
  EXPECT_EQ(folder_id, RootGridView()->GetItemViewAt(0)->item()->id());
  EXPECT_NE(nullptr,
            delegate_->GetTestModel()->FindFolderItem("single_item_folder"));
}

}  // namespace test
}  // namespace ash
