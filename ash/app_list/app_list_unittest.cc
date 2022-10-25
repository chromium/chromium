// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/app_list/app_list_bubble_presenter.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_presenter_impl.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shelf/home_button.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_view_test_api.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"

namespace ash {
namespace {

// Returns visibility from the presenter's perspective.
bool GetPresenterVisibility() {
  auto* controller = Shell::Get()->app_list_controller();
  return controller->bubble_presenter_for_test()->IsShowing();
}

}  // namespace

class AppListTest : public AshTestBase {
 public:
  AppListTest() = default;
};

// An integration test to toggle the app list by pressing the shelf button.
TEST_F(AppListTest, PressHomeButtonToShowAndDismiss) {
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  Shelf* shelf = Shelf::ForWindow(root_window);
  ShelfWidget* shelf_widget = shelf->shelf_widget();
  ShelfView* shelf_view = shelf->GetShelfViewForTesting();
  ShelfViewTestAPI(shelf_view).RunMessageLoopUntilAnimationsDone();
  HomeButton* home_button = shelf_widget->navigation_widget()->GetHomeButton();
  // Ensure animations progressed to give the home button a non-empty size.
  ASSERT_GT(home_button->GetBoundsInScreen().height(), 0);

  aura::Window* app_list_container =
      root_window->GetChildById(kShellWindowId_AppListContainer);

  // Click the home button to show the app list.
  auto* controller = Shell::Get()->app_list_controller();
  EXPECT_FALSE(controller->GetTargetVisibility(GetPrimaryDisplay().id()));
  EXPECT_FALSE(GetPresenterVisibility());
  EXPECT_EQ(0u, app_list_container->children().size());
  EXPECT_FALSE(home_button->IsShowingAppList());

  LeftClickOn(home_button);
  EXPECT_TRUE(GetPresenterVisibility());
  EXPECT_EQ(1u, app_list_container->children().size());
  EXPECT_TRUE(home_button->IsShowingAppList());

  // Click the button again to dismiss the app list; it will animate to close.
  LeftClickOn(home_button);
  EXPECT_FALSE(controller->GetTargetVisibility(GetPrimaryDisplay().id()));
  EXPECT_EQ(1u, app_list_container->children().size());
  EXPECT_FALSE(home_button->IsShowingAppList());
}

// Tests that the app list gets toggled by pressing the shelf button on
// secondary display.
TEST_F(AppListTest, PressHomeButtonToShowAndDismissOnSecondDisplay) {
  UpdateDisplay("1024x768,1024x768");
  aura::Window* root_window =
      Shell::GetRootWindowForDisplayId(GetSecondaryDisplay().id());
  Shelf* shelf = Shelf::ForWindow(root_window);
  ShelfWidget* shelf_widget = shelf->shelf_widget();
  ShelfView* shelf_view = shelf->GetShelfViewForTesting();
  ShelfViewTestAPI(shelf_view).RunMessageLoopUntilAnimationsDone();
  HomeButton* home_button = shelf_widget->navigation_widget()->GetHomeButton();
  // Ensure animations progressed to give the home button a non-empty size.
  ASSERT_GT(home_button->GetBoundsInScreen().height(), 0);

  aura::Window* app_list_container =
      root_window->GetChildById(kShellWindowId_AppListContainer);

  // Click the home button to show the app list.
  auto* controller = Shell::Get()->app_list_controller();
  EXPECT_FALSE(controller->GetTargetVisibility(GetPrimaryDisplay().id()));
  EXPECT_FALSE(controller->GetTargetVisibility(GetSecondaryDisplay().id()));
  EXPECT_FALSE(GetPresenterVisibility());
  EXPECT_EQ(0u, app_list_container->children().size());
  EXPECT_FALSE(home_button->IsShowingAppList());

  LeftClickOn(home_button);
  EXPECT_FALSE(controller->GetTargetVisibility(GetPrimaryDisplay().id()));
  EXPECT_TRUE(controller->GetTargetVisibility(GetSecondaryDisplay().id()));
  EXPECT_TRUE(GetPresenterVisibility());
  EXPECT_EQ(1u, app_list_container->children().size());
  EXPECT_TRUE(home_button->IsShowingAppList());

  // Click the button again to dismiss the app list; it will animate to close.
  LeftClickOn(home_button);
  EXPECT_FALSE(controller->GetTargetVisibility(GetPrimaryDisplay().id()));
  EXPECT_FALSE(controller->GetTargetVisibility(GetSecondaryDisplay().id()));
  EXPECT_EQ(1u, app_list_container->children().size());
  EXPECT_FALSE(home_button->IsShowingAppList());
}

}  // namespace ash
