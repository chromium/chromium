// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_multitask_menu_event_handler.h"

#include <memory>

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/splitview/split_view_divider.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/tablet_mode/tablet_mode_multitask_menu.h"
#include "ash/wm/tablet_mode/tablet_mode_window_manager.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chromeos/ui/frame/multitask_menu/multitask_menu_view.h"
#include "chromeos/ui/frame/multitask_menu/split_button.h"
#include "chromeos/ui/wm/features.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/events/event_handler.h"

namespace ash {

// A simple event monitor that records the gesture event type, used to check
// gesture event generation.
class TestEventHandler : public ui::EventHandler {
 public:
  TestEventHandler() { Shell::Get()->AddPreTargetHandler(this); }

  TestEventHandler(const TestEventHandler&) = delete;
  TestEventHandler& operator=(const TestEventHandler&) = delete;

  ~TestEventHandler() override { Shell::Get()->RemovePreTargetHandler(this); }

  // ui::EventHandler:
  void OnEvent(ui::Event* event) override {
    switch (event->type()) {
      case ui::ET_GESTURE_SCROLL_BEGIN:
      case ui::ET_GESTURE_SCROLL_END:
      case ui::ET_GESTURE_SCROLL_UPDATE:
        is_scroll_ = true;
        break;
      case ui::ET_GESTURE_SWIPE:
        is_swipe_ = true;
        break;
      case ui::ET_SCROLL_FLING_START:
        is_fling_ = true;
        break;
      default:
        break;
    }
  }

  void ResetGestures() {
    is_scroll_ = false;
    is_swipe_ = false;
    is_fling_ = false;
  }

  bool is_scroll() const { return is_scroll_; }
  bool is_swipe() const { return is_swipe_; }
  bool is_fling() const { return is_fling_; }

 private:
  bool is_scroll_ = false;
  bool is_swipe_ = false;
  bool is_fling_ = false;
};

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
    scoped_feature_list_.InitWithFeatures(
        {chromeos::wm::features::kFloatWindow,
         chromeos::wm::features::kPartialSplit},
        {});

    AshTestBase::SetUp();

    TabletModeControllerTestApi().EnterTabletMode();
  }

  void GenerateScroll(int x, int start_y, int end_y) {
    GetEventGenerator()->GestureScrollSequence(
        gfx::Point(x, start_y), gfx::Point(x, end_y), base::Milliseconds(100),
        /*steps=*/3);
  }

  void GenerateSwipe(int x, int start_y, int end_y) {
    GetEventGenerator()->GestureScrollSequence(
        gfx::Point(x, start_y), gfx::Point(x, end_y), base::Milliseconds(10),
        /*steps=*/10);
  }

  void GenerateFling(int x, int start_y, int end_y) {
    GetEventGenerator()->GestureScrollSequence(
        gfx::Point(x, start_y), gfx::Point(x, end_y), base::Milliseconds(1),
        /*steps=*/1);
  }

  void ShowMultitaskMenu(const aura::Window& window) {
    GenerateScroll(/*x=*/window.bounds().CenterPoint().x(),
                   /*start_y=*/1, /*end_y=*/50);
  }

  TabletModeMultitaskMenuEventHandler* GetMultitaskMenuEventHandler() {
    return TabletModeControllerTestApi()
        .tablet_mode_window_manager()
        ->tablet_mode_multitask_menu_event_handler_for_testing();
  }

  TabletModeMultitaskMenu* GetMultitaskMenu() {
    return TabletModeControllerTestApi()
        .tablet_mode_window_manager()
        ->tablet_mode_multitask_menu_event_handler_for_testing()
        ->multitask_menu_for_testing();
  }

  chromeos::MultitaskMenuView* GetMultitaskMenuView(
      TabletModeMultitaskMenu* multitask_menu) const {
    // The contents view of the widget is a `TabletModeMultitaskMenuView`
    // class, which has one child that is the `MultitaskMenuView`.
    views::View* contents_view =
        multitask_menu->multitask_menu_widget()->GetContentsView();
    EXPECT_EQ(1u, contents_view->children().size());

    views::View* multitask_menu_view = contents_view->children().front();
    EXPECT_EQ(chromeos::MultitaskMenuView::kViewClassName,
              multitask_menu_view->GetClassName());
    return static_cast<chromeos::MultitaskMenuView*>(multitask_menu_view);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that the gesture generation used in later tests works as expected.
TEST_F(TabletModeMultitaskMenuEventHandlerTest, GestureEventGeneration) {
  TestEventHandler event_handler = TestEventHandler();
  auto window = CreateTestWindow();

  // Verify that scroll can open and close the menu.
  GenerateScroll(/*x=*/window->bounds().CenterPoint().x(), /*start_y=*/1,
                 /*end_y=*/50);
  ASSERT_TRUE(event_handler.is_scroll());
  ASSERT_TRUE(GetMultitaskMenu());

  GenerateScroll(/*x=*/window->bounds().CenterPoint().x(), /*start_y=*/50,
                 /*end_y=*/8);
  ASSERT_FALSE(GetMultitaskMenu());

  // Verify that swipe can open and close the menu.
  event_handler.ResetGestures();
  GenerateSwipe(/*x=*/window->bounds().CenterPoint().x(), /*start_y=*/1,
                /*end_y=*/50);
  ASSERT_TRUE(event_handler.is_swipe());
  ASSERT_TRUE(GetMultitaskMenu());

  GenerateSwipe(/*x=*/window->bounds().CenterPoint().x(), /*start_y=*/50,
                /*end_y=*/8);
  ASSERT_FALSE(GetMultitaskMenu());

  // Verify that fling can open and close the menu.
  event_handler.ResetGestures();
  GenerateFling(/*x=*/window->bounds().CenterPoint().x(), /*start_y=*/1,
                /*end_y=*/50);
  ASSERT_TRUE(event_handler.is_fling());
  ASSERT_TRUE(GetMultitaskMenu());

  GenerateFling(/*x=*/window->bounds().CenterPoint().x(), /*start_y=*/50,
                /*end_y=*/8);
  ASSERT_FALSE(GetMultitaskMenu());
}

// Tests that a scroll down gesture from the top center activates the
// multitask menu.
TEST_F(TabletModeMultitaskMenuEventHandlerTest, ShowMultitaskMenu) {
  auto window = CreateTestWindow();

  ShowMultitaskMenu(*window);

  TabletModeMultitaskMenu* multitask_menu = GetMultitaskMenu();
  ASSERT_TRUE(multitask_menu);
  ASSERT_TRUE(
      multitask_menu->multitask_menu_widget()->GetContentsView()->GetVisible());

  // Tests that a regular window that can be snapped and floated has all
  // buttons.
  chromeos::MultitaskMenuView* multitask_menu_view =
      GetMultitaskMenuView(multitask_menu);
  ASSERT_TRUE(multitask_menu_view);
  EXPECT_TRUE(multitask_menu_view->half_button_for_testing());
  EXPECT_TRUE(multitask_menu_view->partial_button_for_testing());
  EXPECT_TRUE(multitask_menu_view->full_button_for_testing());
  EXPECT_TRUE(multitask_menu_view->float_button_for_testing());

  // Verify that the menu is horizontally centered.
  EXPECT_EQ(multitask_menu->multitask_menu_widget()
                ->GetContentsView()
                ->GetBoundsInScreen()
                .CenterPoint()
                .x(),
            window->GetBoundsInScreen().CenterPoint().x());
}

// Tests that the menu is closed when the window is closed or destroyed.
TEST_F(TabletModeMultitaskMenuEventHandlerTest, OnWindowDestroying) {
  auto window = CreateTestWindow();

  ShowMultitaskMenu(*window);
  ASSERT_TRUE(GetMultitaskMenu());

  // Close the window.
  window.reset();
  EXPECT_FALSE(GetMultitaskMenu());
}

// Tests that scroll down shows the menu as expected.
TEST_F(TabletModeMultitaskMenuEventHandlerTest, ScrollDownGestures) {
  auto window = CreateTestWindow();

  // Scroll down from the top left. Verify that we do not show the menu.
  GenerateScroll(0, 1, 50);
  ASSERT_FALSE(GetMultitaskMenu());

  // Scroll down from the top right. Verify that we do not show the menu.
  GenerateScroll(window->bounds().right(), 1, 50);
  ASSERT_FALSE(GetMultitaskMenu());

  // Scroll down from the top center. Verify that we show the menu.
  GenerateScroll(window->bounds().CenterPoint().x(), 1, 50);
  ASSERT_TRUE(GetMultitaskMenu());

  // Scroll up on the menu. Verify that we close the menu.
  GenerateScroll(window->bounds().CenterPoint().x(), 50, 8);
  EXPECT_FALSE(GetMultitaskMenu());

  // Scroll down from the top left. Verify that we do not show the menu.
  GenerateScroll(0, 1, 50);
  ASSERT_FALSE(GetMultitaskMenu());
}

// Tests that fast swipes/flings show the menu as expected.
TEST_F(TabletModeMultitaskMenuEventHandlerTest, SwipeFlingGestures) {
  auto window = CreateTestWindow();

  // Swipe down fast to send a ET_SCROLL_FLING_START event. Verify that we
  // open the menu.
  GenerateFling(window->bounds().CenterPoint().x(), 1, 50);
  ASSERT_TRUE(GetMultitaskMenu());

  // Swipe up fast to send a ET_SCROLL_FLING_START event. Verify that we close
  // the menu.
  GenerateFling(window->bounds().CenterPoint().x(), 50, 8);
  ASSERT_FALSE(GetMultitaskMenu());
}

// Tests that scroll up closes the menu as expected.
TEST_F(TabletModeMultitaskMenuEventHandlerTest, ScrollUpGestures) {
  auto window = CreateTestWindow();

  // Scroll up with no menu open. Verify no change.
  GenerateScroll(window->bounds().CenterPoint().x(), 50, 8);
  ASSERT_FALSE(GetMultitaskMenu());

  ShowMultitaskMenu(*window);
  ASSERT_TRUE(GetMultitaskMenu());

  // Scroll down again. Verify that we still show the menu.
  GenerateScroll(window->bounds().CenterPoint().x(), 1, 50);
  ASSERT_TRUE(GetMultitaskMenu());

  // Scroll up on the menu. Verify that we close the menu.
  GenerateScroll(window->bounds().CenterPoint().x(), 50, 8);
  EXPECT_FALSE(GetMultitaskMenu());
}

TEST_F(TabletModeMultitaskMenuEventHandlerTest, HideMultitaskMenuInOverview) {
  auto window = CreateTestWindow();

  ShowMultitaskMenu(*window);

  auto* event_handler =
      TabletModeControllerTestApi()
          .tablet_mode_window_manager()
          ->tablet_mode_multitask_menu_event_handler_for_testing();
  auto* multitask_menu = event_handler->multitask_menu_for_testing();
  ASSERT_TRUE(multitask_menu);
  ASSERT_TRUE(
      multitask_menu->multitask_menu_widget()->GetContentsView()->GetVisible());

  EnterOverview();

  // Verify that the menu is hidden.
  EXPECT_FALSE(GetMultitaskMenu());
}

// Tests that the multitask menu gets updated after a button is pressed.
TEST_F(TabletModeMultitaskMenuEventHandlerTest, ButtonFunctionality) {
  auto window = CreateTestWindow();

  ShowMultitaskMenu(*window);

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

  // Scroll down again.
  ShowMultitaskMenu(*window);

  // Verify that the multitask menu has been centered on the new window size.
  auto* multitask_menu = GetMultitaskMenu();
  ASSERT_TRUE(multitask_menu);
  EXPECT_EQ(window->GetBoundsInScreen().CenterPoint().x(),
            multitask_menu->multitask_menu_widget()
                ->GetContentsView()
                ->GetBoundsInScreen()
                .CenterPoint()
                .x());
}

// Tests that tap outside the menu will close the menu.
TEST_F(TabletModeMultitaskMenuEventHandlerTest, CloseMultitaskMenuOnTap) {
  // Create a display and window that is bigger than the menu.
  UpdateDisplay("1600x1000");
  auto window = CreateAppWindow();

  ShowMultitaskMenu(*window);
  ASSERT_TRUE(GetMultitaskMenu());

  // Tap outside the menu. Verify that we close the menu.
  GetEventGenerator()->GestureTapAt(window->GetBoundsInScreen().CenterPoint());
  EXPECT_FALSE(GetMultitaskMenu());
}

// Tests that if a window cannot be snapped or floated, the buttons will not
// be shown.
TEST_F(TabletModeMultitaskMenuEventHandlerTest, HiddenButtons) {
  UpdateDisplay("800x600");

  // A window with a minimum size of 600x600 will not be snappable or
  // floatable.
  aura::test::TestWindowDelegate window_delegate;
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
      &window_delegate, /*id=*/-1, gfx::Rect(700, 700)));
  window_delegate.set_minimum_size(gfx::Size(600, 600));

  ShowMultitaskMenu(*window);

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
