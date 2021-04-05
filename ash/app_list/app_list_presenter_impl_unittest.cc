// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_presenter_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/model/app_list_test_model.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/apps_grid_view.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/run_loop.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/events/test/event_generator.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

class AppListPresenterImplTest : public AshTestBase {
 public:
  AppListPresenterImplTest() = default;
  ~AppListPresenterImplTest() override = default;

  AppListPresenterImpl* presenter() {
    return Shell::Get()->app_list_controller()->presenter();
  }

  // Shows the app list on the primary display.
  void ShowAppList() {
    presenter()->Show(AppListViewState::kPeeking, GetPrimaryDisplay().id(),
                      base::TimeTicks());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AppListPresenterImplTest);
};

// Tests that app launcher is dismissed when focus moves to another window.
TEST_F(AppListPresenterImplTest, HideOnFocusOut) {
  // Show the app list and focus it.
  ShowAppList();
  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(presenter()->GetWindow());
  focus_client->FocusWindow(presenter()->GetWindow());
  EXPECT_TRUE(presenter()->GetTargetVisibility());

  // Focus a different window.
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  focus_client->FocusWindow(window.get());

  // App list closes.
  EXPECT_FALSE(presenter()->GetTargetVisibility());
}

// Tests that the app list is dismissed when the app list's widget is destroyed.
TEST_F(AppListPresenterImplTest, WidgetDestroyed) {
  ShowAppList();
  EXPECT_TRUE(presenter()->GetTargetVisibility());
  presenter()->GetView()->GetWidget()->CloseNow();
  EXPECT_FALSE(presenter()->GetTargetVisibility());
}

// Test that clicking on app list context menus doesn't close the app list.
TEST_F(AppListPresenterImplTest, ClickingContextMenuDoesNotDismiss) {
  // Populate some apps since we will show the context menu over a view.
  AppListModel* model = Shell::Get()->app_list_controller()->GetModel();
  model->AddItem(std::make_unique<AppListItem>("item 1"));
  model->AddItem(std::make_unique<AppListItem>("item 2"));

  // Show the app list on the primary display.
  ShowAppList();
  aura::Window* window = presenter()->GetWindow();
  ASSERT_TRUE(window);

  // Show a context menu for the first app list item view.
  AppListView::TestApi test_api(presenter()->GetView());
  AppsGridView* grid_view = test_api.GetRootAppsGridView();
  AppListItemView* item_view = grid_view->GetItemViewAt(0);
  DCHECK(item_view);
  item_view->ShowContextMenu(gfx::Point(), ui::MENU_SOURCE_MOUSE);

  // Find the context menu.
  aura::Window* menu_container =
      Shell::GetPrimaryRootWindow()->GetChildById(kShellWindowId_MenuContainer);
  ASSERT_EQ(1u, menu_container->children().size());
  aura::Window* menu = menu_container->children()[0];

  // Press the left mouse button on the menu window, it should not close the
  // app list nor the context menu on this pointer event.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->set_current_screen_location(
      menu->GetBoundsInScreen().origin());
  event_generator->PressLeftButton();

  // Check that the window and the app list are still open.
  ASSERT_EQ(window, presenter()->GetWindow());
  EXPECT_EQ(1u, menu_container->children().size());

  // Close app list so that views are destructed and unregistered from the
  // model's observer list.
  presenter()->Dismiss(base::TimeTicks());
  base::RunLoop().RunUntilIdle();
}

}  // namespace
}  // namespace ash
