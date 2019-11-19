// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_presenter_impl.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shelf/home_button.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_view_test_api.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ui/aura/window.h"
#include "ui/events/test/event_generator.h"

namespace ash {

using AppListTest = AshTestBase;

// An integration test to toggle the app list by pressing the shelf button.
TEST_F(AppListTest, PressHomeButtonToShowAndDismiss) {
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  Shelf* shelf = Shelf::ForWindow(root_window);
  ShelfWidget* shelf_widget = shelf->shelf_widget();
  ShelfView* shelf_view = shelf->GetShelfViewForTesting();
  ShelfViewTestAPI(shelf_view).RunMessageLoopUntilAnimationsDone();
  HomeButton* home_button = shelf_widget->GetHomeButton();
  // Ensure animations progressed to give the home button a non-empty size.
  ASSERT_GT(home_button->GetBoundsInScreen().height(), 0);

  aura::Window* app_list_container =
      root_window->GetChildById(kShellWindowId_AppListContainer);
  ui::test::EventGenerator generator(root_window);

  // Click the home button to show the app list.
  auto* controller = Shell::Get()->app_list_controller();
  auto* presenter = controller->presenter();
  EXPECT_FALSE(controller->GetTargetVisibility());
  EXPECT_FALSE(presenter->GetTargetVisibility());
  EXPECT_EQ(0u, app_list_container->children().size());
  EXPECT_FALSE(home_button->IsShowingAppList());
  generator.set_current_screen_location(
      home_button->GetBoundsInScreen().CenterPoint());
  generator.ClickLeftButton();
  EXPECT_TRUE(presenter->GetTargetVisibility());
  EXPECT_EQ(1u, app_list_container->children().size());
  EXPECT_TRUE(home_button->IsShowingAppList());

  // Click the button again to dismiss the app list; it will animate to close.
  generator.ClickLeftButton();
  EXPECT_FALSE(controller->GetTargetVisibility());
  EXPECT_EQ(1u, app_list_container->children().size());
  EXPECT_FALSE(home_button->IsShowingAppList());
}

}  // namespace ash
