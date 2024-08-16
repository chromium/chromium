// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_folder_view.h"

#include <utility>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/model/app_list_test_model.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/app_list_a11y_announcer.h"
#include "ash/app_list/views/scrollable_apps_grid_view.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/bind.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/scroll_view.h"

namespace ash {

class AppListFolderViewTest : public AshTestBase {
 public:
  AppListFolderViewTest() = default;
  ~AppListFolderViewTest() override = default;
};

TEST_F(AppListFolderViewTest, ScrollViewSizeIsCappedForLargeFolders) {
  // Create a large number of apps, more than a 4 rows.
  GetAppListTestHelper()->model()->CreateAndPopulateFolderWithApps(30);

  // Open the app list and open the folder.
  auto* helper = GetAppListTestHelper();
  helper->ShowAppList();
  auto* apps_grid_view = helper->GetScrollableAppsGridView();
  views::View* folder_item = apps_grid_view->GetItemViewAt(0);
  LeftClickOn(folder_item);
  ASSERT_TRUE(helper->IsInFolderView());

  auto* folder_view = helper->GetBubbleFolderView();
  auto* scroll_view = folder_view->scroll_view_for_test();
  const int tile_height =
      folder_view->items_grid_view()->GetTotalTileSize(/*page=*/0).height();

  // The scroll view has space for at least 4 full rows, but not 5.
  EXPECT_GE(scroll_view->height(), tile_height * 4);
  EXPECT_LT(scroll_view->height(), tile_height * 5);
}

TEST_F(AppListFolderViewTest, CloseFolderMakesA11yAnnouncement) {
  // Create a folder with a couple items.
  GetAppListTestHelper()->model()->CreateAndPopulateFolderWithApps(2);

  // Open the app list and open the folder.
  auto* helper = GetAppListTestHelper();
  helper->ShowAppList();
  auto* apps_grid_view = helper->GetScrollableAppsGridView();
  views::View* folder_item = apps_grid_view->GetItemViewAt(0);
  LeftClickOn(folder_item);
  ASSERT_TRUE(helper->IsInFolderView());

  // Get the accessibility announcement view.
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

  // Press escape to close the folder.
  PressAndReleaseKey(ui::VKEY_ESCAPE);
  run_loop.Run();
  ASSERT_FALSE(helper->IsInFolderView());

  // An alert fired with a message.
  EXPECT_EQ(event, ax::mojom::Event::kAlert);
  ui::AXNodeData node_data;
  announcement_view->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_EQ(node_data.GetStringAttribute(ax::mojom::StringAttribute::kName),
            "Close folder");
}

TEST_F(AppListFolderViewTest, ExpandedCollapsedAccessibleState) {
  GetAppListTestHelper()->model()->CreateSingleWebAppShortcutItemFolder(
      "folder_id", "shortcut_id");

  // Open the app list and open the folder.
  auto* helper = GetAppListTestHelper();
  helper->ShowAppList();
  auto* apps_grid_view = helper->GetScrollableAppsGridView();
  AppListItemView* folder_item_view = apps_grid_view->GetItemViewAt(0);
  LeftClickOn(folder_item_view);

  auto* folder_view = helper->GetBubbleFolderView();

  ui::AXNodeData node_data;
  folder_view->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_TRUE(node_data.HasState(ax::mojom::State::kExpanded));
  EXPECT_FALSE(node_data.HasState(ax::mojom::State::kCollapsed));

  folder_view->ScheduleShowHideAnimation(false, false);

  // Check accessibility of app list view folder while it's closed.
  node_data = ui::AXNodeData();
  folder_view->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_FALSE(node_data.HasState(ax::mojom::State::kExpanded));
  EXPECT_TRUE(node_data.HasState(ax::mojom::State::kCollapsed));
}

TEST_F(AppListFolderViewTest, AccessibleProperties) {
  GetAppListTestHelper()->model()->CreateSingleWebAppShortcutItemFolder(
      "folder_id", "shortcut_id");

  GetAppListTestHelper()->ShowAppList();
  LeftClickOn(
      GetAppListTestHelper()->GetScrollableAppsGridView()->GetItemViewAt(0));
  auto* folder_view = GetAppListTestHelper()->GetBubbleFolderView();

  ui::AXNodeData node_data;
  folder_view->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_EQ(node_data.role, ax::mojom::Role::kGenericContainer);
}

}  // namespace ash
