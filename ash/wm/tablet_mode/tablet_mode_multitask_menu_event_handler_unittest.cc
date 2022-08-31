// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_multitask_menu_event_handler.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/splitview/split_view_divider.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/tablet_mode/tablet_mode_multitask_menu.h"
#include "ash/wm/tablet_mode/tablet_mode_window_manager.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ui/frame/multitask_menu/multitask_menu_view.h"
#include "chromeos/ui/wm/features.h"

namespace ash {

class TabletModeMultitaskMenuEventHandlerTest : public AshTestBase {
 public:
  TabletModeMultitaskMenuEventHandlerTest() = default;
  TabletModeMultitaskMenuEventHandlerTest(
      const TabletModeMultitaskMenuEventHandlerTest&) = delete;
  TabletModeMultitaskMenuEventHandlerTest& operator=(
      const TabletModeMultitaskMenuEventHandlerTest&) = delete;
  ~TabletModeMultitaskMenuEventHandlerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        chromeos::wm::features::kFloatWindow);

    AshTestBase::SetUp();

    TabletModeControllerTestApi().EnterTabletMode();
  }

  void ShowMultitaskMenu(aura::Window* window) {
    // Swipe down from the top center of the window.
    const int point_x = window->bounds().CenterPoint().x();
    GetEventGenerator()->GestureScrollSequence(gfx::Point(point_x, 1),
                                               gfx::Point(point_x, 10),
                                               base::Milliseconds(100), 3);
  }

  TabletModeMultitaskMenu* GetMultitaskMenu() {
    return TabletModeControllerTestApi()
        .tablet_mode_window_manager()
        ->tablet_mode_multitask_menu_event_handler_for_testing()
        ->multitask_menu_for_testing();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verify that a swipe down gesture from the top center activates the multitask
// menu.
TEST_F(TabletModeMultitaskMenuEventHandlerTest, ShowMultitaskMenu) {
  auto window = CreateTestWindow();

  ShowMultitaskMenu(window.get());

  TabletModeWindowManager* manager =
      TabletModeControllerTestApi().tablet_mode_window_manager();
  ASSERT_TRUE(manager);

  auto* event_handler =
      manager->tablet_mode_multitask_menu_event_handler_for_testing();
  ASSERT_TRUE(event_handler);

  auto* multitask_menu = event_handler->multitask_menu_for_testing();
  ASSERT_TRUE(multitask_menu);
  ASSERT_TRUE(multitask_menu->multitask_menu_widget_for_testing()
                  ->GetContentsView()
                  ->GetVisible());
}

// Verify that the menu is closed when the window is closed or destroyed.
TEST_F(TabletModeMultitaskMenuEventHandlerTest, OnWindowDestroying) {
  auto window = CreateTestWindow();

  ShowMultitaskMenu(window.get());

  auto* event_handler =
      TabletModeControllerTestApi()
          .tablet_mode_window_manager()
          ->tablet_mode_multitask_menu_event_handler_for_testing();
  auto* multitask_menu = event_handler->multitask_menu_for_testing();
  ASSERT_TRUE(multitask_menu);
  ASSERT_TRUE(multitask_menu->multitask_menu_widget_for_testing()
                  ->GetContentsView()
                  ->GetVisible());

  // Close the window.
  window.reset();
  EXPECT_FALSE(event_handler->multitask_menu_for_testing());
}

TEST_F(TabletModeMultitaskMenuEventHandlerTest, HideMultitaskMenuInOverview) {
  auto window = CreateTestWindow();

  ShowMultitaskMenu(window.get());

  auto* event_handler =
      TabletModeControllerTestApi()
          .tablet_mode_window_manager()
          ->tablet_mode_multitask_menu_event_handler_for_testing();
  auto* multitask_menu = event_handler->multitask_menu_for_testing();
  ASSERT_TRUE(multitask_menu);
  ASSERT_TRUE(multitask_menu->multitask_menu_widget_for_testing()
                  ->GetContentsView()
                  ->GetVisible());

  EnterOverview();

  // Verify that the menu is hidden.
  EXPECT_FALSE(event_handler->multitask_menu_for_testing()
                   ->multitask_menu_widget_for_testing()
                   ->IsVisible());
}

// Tests that the multitask menu gets updated after a button is pressed.
TEST_F(TabletModeMultitaskMenuEventHandlerTest, ButtonFunctionality) {
  auto window = CreateTestWindow();

  ShowMultitaskMenu(window.get());

  // Press the primary half split button.
  auto* half_button = GetMultitaskMenu()
                          ->GetMultitaskMenuViewForTesting()
                          ->half_button_for_testing();
  GetEventGenerator()->GestureTapAt(
      half_button->GetBoundsInScreen().left_center());

  // Verify that the window has been snapped in half.
  ASSERT_EQ(chromeos::WindowStateType::kPrimarySnapped,
            WindowState::Get(window.get())->GetStateType());
  const gfx::Rect work_area_bounds_in_screen =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  auto* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  const gfx::Rect divider_bounds =
      split_view_controller->split_view_divider()->GetDividerBoundsInScreen(
          /*is_dragging*/ false);
  ASSERT_NEAR(work_area_bounds_in_screen.width() / 2,
              window->GetBoundsInScreen().width(), divider_bounds.width());

  // Verify that the multitask menu has been closed.
  ASSERT_FALSE(GetMultitaskMenu());

  // Swipe down again.
  ShowMultitaskMenu(window.get());

  // Verify that the multitask menu has been centered on the new window size.
  auto* multitask_menu = GetMultitaskMenu();
  ASSERT_TRUE(multitask_menu);
  EXPECT_EQ(window->GetBoundsInScreen().CenterPoint().x(),
            multitask_menu->multitask_menu_widget_for_testing()
                ->GetContentsView()
                ->GetBoundsInScreen()
                .CenterPoint()
                .x());
}

}  // namespace ash