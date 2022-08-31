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
#include "chromeos/ui/frame/multitask_menu/split_button.h"
#include "chromeos/ui/wm/features.h"
#include "ui/aura/test/test_window_delegate.h"

namespace ash {

class TabletModeMultitaskMenuEventHandlerTest : public AshTestBase {
 public:
  TabletModeMultitaskMenuEventHandlerTest() = default;
  TabletModeMultitaskMenuEventHandlerTest(
      const TabletModeMultitaskMenuEventHandlerTest&) = delete;
  TabletModeMultitaskMenuEventHandlerTest& operator=(
      const TabletModeMultitaskMenuEventHandlerTest&) = delete;
  ~TabletModeMultitaskMenuEventHandlerTest() override = default;

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

  chromeos::MultitaskMenuView* GetMultitaskMenuView(
      TabletModeMultitaskMenu* multitask_menu) const {
    // The contents view of the widget is a `TabletModeMultitaskMenuView` class,
    // which has one child that is the `MultitaskMenuView`.
    views::View* contents_view =
        multitask_menu->multitask_menu_widget_for_testing()->GetContentsView();
    EXPECT_EQ(1u, contents_view->children().size());

    views::View* multitask_menu_view = contents_view->children().front();
    EXPECT_EQ(chromeos::MultitaskMenuView::kViewClassName,
              multitask_menu_view->GetClassName());
    return static_cast<chromeos::MultitaskMenuView*>(multitask_menu_view);
  }

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        chromeos::wm::features::kFloatWindow);

    AshTestBase::SetUp();

    TabletModeControllerTestApi().EnterTabletMode();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verify that a swipe down gesture from the top center activates the multitask
// menu.
TEST_F(TabletModeMultitaskMenuEventHandlerTest, ShowMultitaskMenu) {
  auto window = CreateTestWindow();

  ShowMultitaskMenu(window.get());

  TabletModeMultitaskMenu* multitask_menu = GetMultitaskMenu();
  ASSERT_TRUE(multitask_menu);
  EXPECT_TRUE(multitask_menu->multitask_menu_widget_for_testing()
                  ->GetContentsView()
                  ->GetVisible());

  // Tests that a regular window that can be snapped and floated has all
  // buttons.
  chromeos::MultitaskMenuView* multitask_menu_view =
      GetMultitaskMenuView(multitask_menu);
  ASSERT_TRUE(multitask_menu_view);
  EXPECT_TRUE(multitask_menu_view->half_button_for_testing());
  EXPECT_TRUE(multitask_menu_view->partial_button_for_testing());
  EXPECT_TRUE(multitask_menu_view->full_button_for_testing());
  EXPECT_TRUE(multitask_menu_view->float_button_for_testing());
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

// Tests that if a window cannot be snapped or floated, the buttons will not be
// shown.
TEST_F(TabletModeMultitaskMenuEventHandlerTest, HiddenButtons) {
  UpdateDisplay("800x600");

  // A window with a minimum size of 600x600 will not be snappable or floatable.
  aura::test::TestWindowDelegate window_delegate;
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
      &window_delegate, /*id=*/-1, gfx::Rect(700, 700)));
  window_delegate.set_minimum_size(gfx::Size(600, 600));

  ShowMultitaskMenu(window.get());

  // Tests that only one button, the fullscreen button shows up.
  TabletModeMultitaskMenu* multitask_menu = GetMultitaskMenu();
  ASSERT_TRUE(multitask_menu);
  chromeos::MultitaskMenuView* multitask_menu_view =
      GetMultitaskMenuView(multitask_menu);
  ASSERT_TRUE(multitask_menu_view);
  EXPECT_FALSE(multitask_menu_view->half_button_for_testing());
  EXPECT_FALSE(multitask_menu_view->partial_button_for_testing());
  EXPECT_TRUE(multitask_menu_view->full_button_for_testing());
  EXPECT_FALSE(multitask_menu_view->float_button_for_testing());
}

}  // namespace ash
