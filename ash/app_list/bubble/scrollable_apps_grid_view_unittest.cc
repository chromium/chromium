// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/bubble/scrollable_apps_grid_view.h"

#include <memory>
#include <string>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/model/app_list_folder_item.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/app_list_item_list.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/test_app_list_client.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"

namespace ash {
namespace {

void AddAppListItem(const std::string& id) {
  Shell::Get()->app_list_controller()->GetModel()->AddItem(
      std::make_unique<AppListItem>(id));
}

void PopulateApps(int n) {
  AppListModel* model = Shell::Get()->app_list_controller()->GetModel();
  for (int i = 0; i < n; ++i) {
    model->AddItem(std::make_unique<AppListItem>(base::NumberToString(i)));
  }
}

}  // namespace

class ScrollableAppsGridViewTest : public AshTestBase {
 public:
  ScrollableAppsGridViewTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kAppListBubble);
  }
  ~ScrollableAppsGridViewTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    Shell::Get()->app_list_controller()->SetClient(&app_list_client_);
  }

  // TODO(crbug.com/1222777): Convert the methods below to use
  // GetEventGenerator().
  void SimulateKeyPress(ui::KeyboardCode key_code) {
    SimulateKeyPress(key_code, ui::EF_NONE);
  }

  void SimulateKeyPress(ui::KeyboardCode key_code, int flags) {
    ui::KeyEvent key_event(ui::ET_KEY_PRESSED, key_code, flags);
    GetScrollableAppsGridView()->OnKeyPressed(key_event);
  }

  void SimulateKeyReleased(ui::KeyboardCode key_code, int flags) {
    ui::KeyEvent key_event(ui::ET_KEY_RELEASED, key_code, flags);
    GetScrollableAppsGridView()->OnKeyReleased(key_event);
  }

  // Simulates typing a key.
  void PressAndReleaseKey(ui::KeyboardCode key) {
    GetEventGenerator()->PressKey(key, ui::EF_NONE);
    GetEventGenerator()->ReleaseKey(key, ui::EF_NONE);
  }

  void ShowAppList() { GetAppListTestHelper()->ShowAppList(); }

  ScrollableAppsGridView* GetScrollableAppsGridView() {
    return GetAppListTestHelper()->GetScrollableAppsGridView();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  TestAppListClient app_list_client_;
};

TEST_F(ScrollableAppsGridViewTest, ClickOnApp) {
  AddAppListItem("id");

  ShowAppList();

  // Click on the first icon.
  ScrollableAppsGridView* view = GetScrollableAppsGridView();
  views::View* icon = view->GetItemViewAt(0);
  GetEventGenerator()->MoveMouseTo(icon->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();

  // The item was activated.
  EXPECT_EQ(1, app_list_client_.activate_item_count());
  EXPECT_EQ("id", app_list_client_.activate_item_last_id());
}

TEST_F(ScrollableAppsGridViewTest, DragApp) {
  AddAppListItem("id1");
  AddAppListItem("id2");
  ShowAppList();

  // Start dragging the first item.
  ScrollableAppsGridView* view = GetScrollableAppsGridView();
  AppListItemView* item = view->GetItemViewAt(0);
  auto* generator = GetEventGenerator();
  generator->MoveMouseTo(item->GetBoundsInScreen().CenterPoint());
  generator->PressLeftButton();
  item->FireMouseDragTimerForTest();

  // Drag to the right of the second item.
  gfx::Size tile_size = view->GetTileViewSize();
  generator->MoveMouseBy(tile_size.width() * 2, 0);
  generator->ReleaseLeftButton();

  // The item was not activated.
  EXPECT_EQ(0, app_list_client_.activate_item_count());

  // Items were reordered.
  AppListItemList* item_list =
      Shell::Get()->app_list_controller()->GetModel()->top_level_item_list();
  ASSERT_EQ(2u, item_list->item_count());
  EXPECT_EQ("id2", item_list->item_at(0)->id());
  EXPECT_EQ("id1", item_list->item_at(1)->id());
}

TEST_F(ScrollableAppsGridViewTest, LeftAndRightArrowKeysMoveSelection) {
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

TEST_F(ScrollableAppsGridViewTest, ArrowKeysCanMoveFocusOutOfGrid) {
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
TEST_F(ScrollableAppsGridViewTest, ControlArrowRecordsHistogramBasic) {
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
TEST_F(ScrollableAppsGridViewTest,
       ControlArrowDoesNotRecordHistogramWithNoOpMove) {
  base::HistogramTester histogram_tester;
  PopulateApps(20);
  ShowAppList();
  ScrollableAppsGridView* apps_grid_view = GetScrollableAppsGridView();

  AppListItemView* moving_item = apps_grid_view->GetItemViewAt(0);
  apps_grid_view->GetFocusManager()->SetFocusedView(moving_item);

  // Make 2 no-op moves and one successful move from 0,0 ane expect a histogram
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

// Tests that control + shift + arrow puts selected item into a folder or
// creates a folder if one does not exist.
TEST_F(ScrollableAppsGridViewTest, ControlShiftArrowFoldersItem) {
  base::HistogramTester histogram_tester;
  PopulateApps(20);
  ShowAppList();
  ScrollableAppsGridView* apps_grid_view = GetScrollableAppsGridView();

  // Select the first item in the grid, folder it with the item to the right.
  AppListItemView* first_item = apps_grid_view->GetItemViewAt(0);
  apps_grid_view->GetFocusManager()->SetFocusedView(first_item);
  const std::string first_item_id = first_item->item()->id();
  const std::string second_item_id =
      apps_grid_view->GetItemViewAt(1)->item()->id();
  SimulateKeyPress(ui::VKEY_RIGHT, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);

  // Test that the first item in the grid is now a folder with the first and
  // second items, and that the folder is the selected view.
  AppListItemView* new_folder = apps_grid_view->GetItemViewAt(0);
  ASSERT_TRUE(apps_grid_view->IsSelectedView(new_folder));
  EXPECT_TRUE(new_folder->item()->is_folder());
  AppListFolderItem* folder_item =
      static_cast<AppListFolderItem*>(new_folder->item());
  EXPECT_EQ(2u, folder_item->ChildItemCount());
  EXPECT_TRUE(folder_item->FindChildItem(first_item_id));
  EXPECT_TRUE(folder_item->FindChildItem(second_item_id));
  histogram_tester.ExpectBucketCount("Apps.AppListBubbleAppMovingType",
                                     kMoveByKeyboardIntoFolder, 1);

  // Test that when a folder is selected, control+shift+arrow does nothing.
  SimulateKeyPress(ui::VKEY_RIGHT, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
  EXPECT_TRUE(apps_grid_view->IsSelectedView(new_folder));
  EXPECT_EQ(2u, folder_item->ChildItemCount());
  histogram_tester.ExpectBucketCount("Apps.AppListBubbleAppMovingType",
                                     kMoveByKeyboardIntoFolder, 1);

  // Move selection to the item to the right of the folder and put it in the
  // folder.
  apps_grid_view->GetFocusManager()->SetFocusedView(
      apps_grid_view->GetItemViewAt(1));

  SimulateKeyPress(ui::VKEY_LEFT, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);

  EXPECT_TRUE(apps_grid_view->IsSelectedView(new_folder));
  EXPECT_EQ(3u, folder_item->ChildItemCount());
  histogram_tester.ExpectBucketCount("Apps.AppListBubbleAppMovingType",
                                     kMoveByKeyboardIntoFolder, 2);

  // Move selection to the item below the folder and put it in the folder.
  SimulateKeyPress(ui::VKEY_DOWN);
  SimulateKeyPress(ui::VKEY_UP, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);

  EXPECT_TRUE(apps_grid_view->IsSelectedView(new_folder));
  EXPECT_EQ(4u, folder_item->ChildItemCount());
  histogram_tester.ExpectBucketCount("Apps.AppListBubbleAppMovingType",
                                     kMoveByKeyboardIntoFolder, 3);

  // Move the folder to the second row, then put the item above the folder in
  // the folder.
  SimulateKeyPress(ui::VKEY_DOWN, ui::EF_CONTROL_DOWN);
  SimulateKeyPress(ui::VKEY_UP);
  SimulateKeyPress(ui::VKEY_DOWN, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);

  EXPECT_TRUE(apps_grid_view->IsSelectedView(new_folder));
  EXPECT_EQ(5u, folder_item->ChildItemCount());
  histogram_tester.ExpectBucketCount("Apps.AppListBubbleAppMovingType",
                                     kMoveByKeyboardIntoFolder, 4);
}

}  // namespace ash
