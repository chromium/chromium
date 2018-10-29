// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/split_view_controller.h"

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/magnifier/docked_magnifier_controller.h"
#include "ash/public/cpp/app_types.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/screen_util.h"
#include "ash/session/session_controller.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/system/overview/overview_button_tray.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/test/ash_test_base.h"
#include "ash/wallpaper/wallpaper_controller.h"
#include "ash/wm/drag_window_resizer.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/window_grid.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ash/wm/overview/window_selector_item.h"
#include "ash/wm/splitview/split_view_divider.h"
#include "ash/wm/splitview/split_view_drag_indicators.h"
#include "ash/wm/tablet_mode/tablet_mode_app_window_drag_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_browser_window_drag_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_window_drag_delegate.h"
#include "ash/wm/window_resizer.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_state_delegate.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "base/stl_util.h"
#include "services/ws/public/mojom/window_tree_constants.mojom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/base/hit_test.h"
#include "ui/compositor_extra/shadow.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/shadow_controller.h"
#include "ui/wm/core/shadow_types.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

// The observer to observe the overview states in |root_window_|.
class OverviewStatesObserver : public ShellObserver {
 public:
  OverviewStatesObserver(aura::Window* root_window)
      : root_window_(root_window) {
    Shell::Get()->AddShellObserver(this);
  }
  ~OverviewStatesObserver() override {
    Shell::Get()->RemoveShellObserver(this);
  }

  // ShellObserver:
  void OnOverviewModeStarting() override {
    // Reset the value to true.
    overview_animate_when_exiting_ = true;
  }
  void OnOverviewModeEnding() override {
    WindowSelector* window_selector =
        Shell::Get()->window_selector_controller()->window_selector();
    WindowGrid* grid = window_selector->GetGridWithRootWindow(root_window_);
    if (!grid)
      return;
    overview_animate_when_exiting_ = grid->should_animate_when_exiting();
  }

  bool overview_animate_when_exiting() const {
    return overview_animate_when_exiting_;
  }

 private:
  bool overview_animate_when_exiting_ = true;
  aura::Window* root_window_;

  DISALLOW_COPY_AND_ASSIGN(OverviewStatesObserver);
};

// The test BubbleDialogDelegateView for bubbles.
class TestBubbleDialogDelegateView : public views::BubbleDialogDelegateView {
 public:
  explicit TestBubbleDialogDelegateView(views::View* anchor_view)
      : BubbleDialogDelegateView(anchor_view, views::BubbleBorder::NONE) {}
  ~TestBubbleDialogDelegateView() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestBubbleDialogDelegateView);
};

}  // namespace

class SplitViewControllerTest : public AshTestBase {
 public:
  SplitViewControllerTest() = default;
  ~SplitViewControllerTest() override = default;

  // test::AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    // Avoid TabletModeController::OnGetSwitchStates() from disabling tablet
    // mode.
    base::RunLoop().RunUntilIdle();
    Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(true);
  }

  aura::Window* CreateWindow(
      const gfx::Rect& bounds,
      aura::client::WindowType type = aura::client::WINDOW_TYPE_NORMAL) {
    aura::Window* window = CreateTestWindowInShellWithDelegateAndType(
        new SplitViewTestWindowDelegate, type, -1, bounds);
    return window;
  }

  aura::Window* CreateNonSnappableWindow(const gfx::Rect& bounds) {
    aura::Window* window = CreateWindow(bounds);
    window->SetProperty(aura::client::kResizeBehaviorKey,
                        ws::mojom::kResizeBehaviorNone);
    return window;
  }

  void EndSplitView() { split_view_controller()->EndSplitView(); }

  void ToggleOverview() {
    Shell::Get()->window_selector_controller()->ToggleOverview();
  }

  void LongPressOnOverivewButtonTray() {
    ui::GestureEvent event(0, 0, 0, base::TimeTicks(),
                           ui::GestureEventDetails(ui::ET_GESTURE_LONG_PRESS));
    StatusAreaWidgetTestHelper::GetStatusAreaWidget()
        ->overview_button_tray()
        ->OnGestureEvent(&event);
  }

  std::vector<aura::Window*> GetWindowsInOverviewGrids() {
    return Shell::Get()
        ->window_selector_controller()
        ->GetWindowsListInOverviewGridsForTesting();
  }

  SplitViewController* split_view_controller() {
    return Shell::Get()->split_view_controller();
  }

  SplitViewDivider* split_view_divider() {
    return split_view_controller()->split_view_divider();
  }

  OrientationLockType screen_orientation() {
    return split_view_controller()->GetCurrentScreenOrientation();
  }

  int divider_position() { return split_view_controller()->divider_position(); }

  float divider_closest_ratio() {
    return split_view_controller()->divider_closest_ratio_;
  }

 protected:
  class SplitViewTestWindowDelegate : public aura::test::TestWindowDelegate {
   public:
    SplitViewTestWindowDelegate() = default;
    ~SplitViewTestWindowDelegate() override = default;

    // aura::test::TestWindowDelegate:
    void OnWindowDestroying(aura::Window* window) override { window->Hide(); }
    void OnWindowDestroyed(aura::Window* window) override { delete this; }
  };

 private:
  DISALLOW_COPY_AND_ASSIGN(SplitViewControllerTest);
};

class TestWindowStateDelegate : public wm::WindowStateDelegate {
 public:
  TestWindowStateDelegate() = default;
  ~TestWindowStateDelegate() override = default;

  // wm::WindowStateDelegate:
  void OnDragStarted(int component) override { drag_in_progress_ = true; }
  void OnDragFinished(bool cancel, const gfx::Point& location) override {
    drag_in_progress_ = false;
  }

  bool drag_in_progress() { return drag_in_progress_; }

 private:
  bool drag_in_progress_ = false;
  DISALLOW_COPY_AND_ASSIGN(TestWindowStateDelegate);
};

// Tests the basic functionalities.
TEST_F(SplitViewControllerTest, Basic) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  EXPECT_EQ(split_view_controller()->state(), SplitViewController::NO_SNAP);
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), false);

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::LEFT_SNAPPED);
  EXPECT_EQ(split_view_controller()->left_window(), window1.get());
  EXPECT_NE(split_view_controller()->left_window(), window2.get());
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), true);
  EXPECT_EQ(window1->GetBoundsInScreen(),
            split_view_controller()->GetSnappedWindowBoundsInScreen(
                window1.get(), SplitViewController::LEFT));

  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::BOTH_SNAPPED);
  EXPECT_EQ(split_view_controller()->right_window(), window2.get());
  EXPECT_NE(split_view_controller()->right_window(), window1.get());
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), true);
  EXPECT_EQ(window2->GetBoundsInScreen(),
            split_view_controller()->GetSnappedWindowBoundsInScreen(
                window2.get(), SplitViewController::RIGHT));

  EndSplitView();
  EXPECT_EQ(split_view_controller()->state(), SplitViewController::NO_SNAP);
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), false);
}

// Tests that the default snapped window is the first window that gets snapped.
TEST_F(SplitViewControllerTest, DefaultSnappedWindow) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  EXPECT_EQ(window1.get(), split_view_controller()->GetDefaultSnappedWindow());

  EndSplitView();
  split_view_controller()->SnapWindow(window2.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window1.get(),
                                      SplitViewController::RIGHT);
  EXPECT_EQ(window2.get(), split_view_controller()->GetDefaultSnappedWindow());
}

// Tests that if there are two snapped windows, closing one of them will open
// overview window grid on the closed window side of the screen. If there is
// only one snapped windows, closing the snapped window will end split view mode
// and adjust the overview window grid bounds if the overview mode is active at
// that moment.
TEST_F(SplitViewControllerTest, WindowCloseTest) {
  // 1 - First test one snapped window scenario.
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window0(CreateWindow(bounds));
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), false);
  split_view_controller()->SnapWindow(window0.get(), SplitViewController::LEFT);
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), true);
  // Closing this snapped window should exit split view mode.
  window0.reset();
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), false);

  // 2 - Then test two snapped windows scenario.
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), false);
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), true);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::BOTH_SNAPPED);
  EXPECT_EQ(split_view_controller()->default_snap_position(),
            SplitViewController::LEFT);

  // Closing one of the two snapped windows will not end split view mode.
  window1.reset();
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), true);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::RIGHT_SNAPPED);
  // Since left window was closed, its default snap position changed to RIGHT.
  EXPECT_EQ(split_view_controller()->default_snap_position(),
            SplitViewController::RIGHT);
  // Window grid is showing no recent items, and has no windows, but it is still
  // available.
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());

  // Now close the other snapped window.
  window2.reset();
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), false);
  EXPECT_EQ(split_view_controller()->state(), SplitViewController::NO_SNAP);
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());

  // 3 - Then test the scenario with more than two windows.
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window4(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window5(CreateWindow(bounds));
  split_view_controller()->SnapWindow(window3.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window4.get(),
                                      SplitViewController::RIGHT);
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), true);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::BOTH_SNAPPED);
  EXPECT_EQ(split_view_controller()->default_snap_position(),
            SplitViewController::LEFT);

  // Close one of the snapped windows.
  window4.reset();
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), true);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::LEFT_SNAPPED);
  EXPECT_EQ(split_view_controller()->default_snap_position(),
            SplitViewController::LEFT);
  // Now overview window grid can be opened.
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());

  // Close the other snapped window.
  window3.reset();
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), false);
  EXPECT_EQ(split_view_controller()->state(), SplitViewController::NO_SNAP);
  // Test the overview winow grid should still open.
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());
}

// Tests that if there are two snapped windows, minimizing one of them will open
// overview window grid on the minimized window side of the screen. If there is
// only one snapped windows, minimizing the sanpped window will end split view
// mode and adjust the overview window grid bounds if the overview mode is
// active at that moment.
TEST_F(SplitViewControllerTest, MinimizeWindowTest) {
  const gfx::Rect bounds(0, 0, 400, 400);

  // 1 - First test one snapped window scenario.
  std::unique_ptr<aura::Window> window0(CreateWindow(bounds));
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), false);
  split_view_controller()->SnapWindow(window0.get(), SplitViewController::LEFT);
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), true);
  wm::WMEvent minimize_event(wm::WM_EVENT_MINIMIZE);
  wm::GetWindowState(window0.get())->OnWMEvent(&minimize_event);
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), false);

  // 2 - Then test the scenario that has 2 or more windows.
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), true);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::BOTH_SNAPPED);
  EXPECT_EQ(split_view_controller()->default_snap_position(),
            SplitViewController::LEFT);

  // Minimizing one of the two snapped windows will not end split view mode.
  wm::GetWindowState(window1.get())->OnWMEvent(&minimize_event);
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), true);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::RIGHT_SNAPPED);
  // Since left window was minimized, its default snap position changed to
  // RIGHT.
  EXPECT_EQ(split_view_controller()->default_snap_position(),
            SplitViewController::RIGHT);
  // The overview window grid will open.
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());

  // Now minimize the other snapped window.
  wm::GetWindowState(window2.get())->OnWMEvent(&minimize_event);
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), false);
  EXPECT_EQ(split_view_controller()->state(), SplitViewController::NO_SNAP);
  // The overview window grid is still open.
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());
}

// Tests that if one of the snapped window gets maximized / full-screened, the
// split view mode ends.
TEST_F(SplitViewControllerTest, WindowStateChangeTest) {
  const gfx::Rect bounds(0, 0, 400, 400);
  // 1 - First test one snapped window scenario.
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), false);

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), true);

  wm::WMEvent maximize_event(wm::WM_EVENT_MAXIMIZE);
  wm::GetWindowState(window1.get())->OnWMEvent(&maximize_event);
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), false);

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), true);

  wm::WMEvent fullscreen_event(wm::WM_EVENT_FULLSCREEN);
  wm::GetWindowState(window1.get())->OnWMEvent(&fullscreen_event);
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), false);

  // 2 - Then test two snapped window scenario.
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), true);

  // Maximize one of the snapped window will end the split view mode.
  wm::GetWindowState(window1.get())->OnWMEvent(&maximize_event);
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), false);

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), true);

  // Full-screen one of the snapped window will also end the split view mode.
  wm::GetWindowState(window1.get())->OnWMEvent(&fullscreen_event);
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), false);

  // 3 - Test the scenario that part of the screen is a snapped window and part
  // of the screen is the overview window grid.
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), true);
  ToggleOverview();
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), true);

  // Maximize the snapped window will end the split view mode and overview mode.
  wm::GetWindowState(window1.get())->OnWMEvent(&maximize_event);
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), false);
  EXPECT_FALSE(Shell::Get()->window_selector_controller()->IsSelecting());

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), true);
  ToggleOverview();
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), true);

  // Fullscreen the snapped window will end the split view mode and overview
  // mode.
  wm::GetWindowState(window1.get())->OnWMEvent(&fullscreen_event);
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), false);
  EXPECT_FALSE(Shell::Get()->window_selector_controller()->IsSelecting());
}

// Tests that if split view mode is active, activate another window will snap
// the window to the non-default side of the screen.
TEST_F(SplitViewControllerTest, WindowActivationTest) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), false);

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), true);
  EXPECT_EQ(split_view_controller()->left_window(), window1.get());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::LEFT_SNAPPED);

  wm::ActivateWindow(window2.get());
  EXPECT_EQ(split_view_controller()->right_window(), window2.get());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::BOTH_SNAPPED);

  wm::ActivateWindow(window3.get());
  EXPECT_EQ(split_view_controller()->right_window(), window3.get());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::BOTH_SNAPPED);
}

// Tests that if split view mode and overview mode are active at the same time,
// i.e., half of the screen is occupied by a snapped window and half of the
// screen is occupied by the overview windows grid, the next activatable window
// will be picked to snap when exiting the overview mode.
TEST_F(SplitViewControllerTest, ExitOverviewTest) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), false);

  ToggleOverview();
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), true);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::LEFT_SNAPPED);
  EXPECT_EQ(split_view_controller()->left_window(), window1.get());

  ToggleOverview();
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::BOTH_SNAPPED);
  EXPECT_EQ(split_view_controller()->right_window(), window3.get());
}

// Tests that if split view mode is active when entering overview, the overview
// windows grid should show in the non-default side of the screen, and the
// default snapped window should not be shown in the overview window grid.
TEST_F(SplitViewControllerTest, EnterOverviewTest) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::BOTH_SNAPPED);
  EXPECT_EQ(split_view_controller()->GetDefaultSnappedWindow(), window1.get());

  ToggleOverview();
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::LEFT_SNAPPED);
  EXPECT_FALSE(
      base::ContainsValue(GetWindowsInOverviewGrids(),
                          split_view_controller()->GetDefaultSnappedWindow()));
}

// Tests that the split divider was created when the split view mode is active
// and destroyed when the split view mode is ended. The split divider should be
// always above the two snapped windows.
TEST_F(SplitViewControllerTest, SplitDividerBasicTest) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  EXPECT_TRUE(!split_view_divider());
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  EXPECT_TRUE(split_view_divider());
  EXPECT_TRUE(split_view_divider()->divider_widget()->IsAlwaysOnTop());
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  EXPECT_TRUE(split_view_divider());
  EXPECT_TRUE(split_view_divider()->divider_widget()->IsAlwaysOnTop());

  // Test that activating an non-snappable window ends the split view mode.
  std::unique_ptr<aura::Window> window3(CreateNonSnappableWindow(bounds));
  wm::ActivateWindow(window3.get());
  EXPECT_FALSE(split_view_divider());
}

// Verifys that the bounds of the two windows in splitview are as expected.
TEST_F(SplitViewControllerTest, SplitDividerWindowBounds) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  ASSERT_TRUE(split_view_divider());

  // Verify with two freshly snapped windows are roughly the same width (off by
  // one pixel at most due to the display maybe being even and the divider being
  // a fixed odd pixel width).
  int window1_width = window1->GetBoundsInScreen().width();
  int window2_width = window2->GetBoundsInScreen().width();
  gfx::Rect divider_bounds =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);
  const int screen_width =
      screen_util::GetDisplayWorkAreaBoundsInParent(window1.get()).width();
  EXPECT_NEAR(window1_width, window2_width, 1);
  EXPECT_EQ(screen_width,
            window1_width + divider_bounds.width() + window2_width);

  // Drag the divider to a position two thirds of the screen size. Verify window
  // 1 is wider than window 2.
  GetEventGenerator()->set_current_location(divider_bounds.CenterPoint());
  GetEventGenerator()->DragMouseTo(screen_width * 0.67f, 0);
  window1_width = window1->GetBoundsInScreen().width();
  window2_width = window2->GetBoundsInScreen().width();
  const int old_window1_width = window1_width;
  const int old_window2_width = window2_width;
  EXPECT_GT(window1_width, 2 * window2_width);
  EXPECT_EQ(screen_width,
            window1_width + divider_bounds.width() + window2_width);

  // Drag the divider to a position close to two thirds of the screen size.
  // Verify the divider snaps to two thirds of the screen size, and the windows
  // remain the same size as previously.
  divider_bounds =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);
  GetEventGenerator()->set_current_location(divider_bounds.CenterPoint());
  GetEventGenerator()->DragMouseTo(screen_width * 0.7f, 0);
  window1_width = window1->GetBoundsInScreen().width();
  window2_width = window2->GetBoundsInScreen().width();
  EXPECT_EQ(window1_width, old_window1_width);
  EXPECT_EQ(window2_width, old_window2_width);

  // Drag the divider to a position one third of the screen size. Verify window
  // 1 is wider than window 2.
  divider_bounds =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);
  GetEventGenerator()->set_current_location(divider_bounds.CenterPoint());
  GetEventGenerator()->DragMouseTo(screen_width * 0.33f, 0);
  window1_width = window1->GetBoundsInScreen().width();
  window2_width = window2->GetBoundsInScreen().width();
  EXPECT_GT(window2_width, 2 * window1_width);
  EXPECT_EQ(screen_width,
            window1_width + divider_bounds.width() + window2_width);

  // Verify that the left window from dragging the divider to two thirds of the
  // screen size is roughly the same size as the right window after dragging the
  // divider to one third of the screen size, and vice versa.
  EXPECT_NEAR(window1_width, old_window2_width, 1);
  EXPECT_NEAR(window2_width, old_window1_width, 1);
}

// Tests that the bounds of the snapped windows and divider are adjusted when
// the screen display configuration changes.
TEST_F(SplitViewControllerTest, DisplayConfigurationChangeTest) {
  UpdateDisplay("407x400");
  const gfx::Rect bounds(0, 0, 200, 200);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);

  const gfx::Rect bounds_window1 = window1->GetBoundsInScreen();
  const gfx::Rect bounds_window2 = window2->GetBoundsInScreen();
  const gfx::Rect bounds_divider =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);

  // Test that |window1| and |window2| has the same width and height after snap.
  EXPECT_NEAR(bounds_window1.width(), bounds_window2.width(), 1);
  EXPECT_EQ(bounds_window1.height(), bounds_window2.height());
  EXPECT_EQ(bounds_divider.height(), bounds_window1.height());

  // Test that |window1|, divider, |window2| are aligned properly.
  EXPECT_EQ(bounds_divider.x(), bounds_window1.x() + bounds_window1.width());
  EXPECT_EQ(bounds_window2.x(), bounds_divider.x() + bounds_divider.width());

  // Now change the display configuration.
  UpdateDisplay("507x500");
  const gfx::Rect new_bounds_window1 = window1->GetBoundsInScreen();
  const gfx::Rect new_bounds_window2 = window2->GetBoundsInScreen();
  const gfx::Rect new_bounds_divider =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);

  // Test that the new bounds are different with the old ones.
  EXPECT_NE(bounds_window1, new_bounds_window1);
  EXPECT_NE(bounds_window2, new_bounds_window2);
  EXPECT_NE(bounds_divider, new_bounds_divider);

  // Test that |window1|, divider, |window2| are still aligned properly.
  EXPECT_EQ(new_bounds_divider.x(),
            new_bounds_window1.x() + new_bounds_window1.width());
  EXPECT_EQ(new_bounds_window2.x(),
            new_bounds_divider.x() + new_bounds_divider.width());
}

// Verify the left and right windows get swapped when SwapWindows is called or
// the divider is double tapped.
TEST_F(SplitViewControllerTest, SwapWindows) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  EXPECT_EQ(split_view_controller()->left_window(), window1.get());
  EXPECT_EQ(split_view_controller()->right_window(), window2.get());

  gfx::Rect left_bounds = window1->GetBoundsInScreen();
  gfx::Rect right_bounds = window2->GetBoundsInScreen();

  // Verify that after swapping windows, the windows and their bounds have been
  // swapped.
  split_view_controller()->SwapWindows();
  EXPECT_EQ(split_view_controller()->left_window(), window2.get());
  EXPECT_EQ(split_view_controller()->right_window(), window1.get());
  EXPECT_EQ(left_bounds, window2->GetBoundsInScreen());
  EXPECT_EQ(right_bounds, window1->GetBoundsInScreen());

  // End split view mode and snap the window to RIGHT first, verify the function
  // SwapWindows() still works properly.
  EndSplitView();
  split_view_controller()->SnapWindow(window1.get(),
                                      SplitViewController::RIGHT);
  split_view_controller()->SnapWindow(window2.get(), SplitViewController::LEFT);
  EXPECT_EQ(split_view_controller()->right_window(), window1.get());
  EXPECT_EQ(split_view_controller()->left_window(), window2.get());

  left_bounds = window2->GetBoundsInScreen();
  right_bounds = window1->GetBoundsInScreen();

  split_view_controller()->SwapWindows();
  EXPECT_EQ(split_view_controller()->left_window(), window1.get());
  EXPECT_EQ(split_view_controller()->right_window(), window2.get());
  EXPECT_EQ(left_bounds, window1->GetBoundsInScreen());
  EXPECT_EQ(right_bounds, window2->GetBoundsInScreen());

  // Perform a double tap on the divider center.
  const gfx::Point divider_center =
      split_view_divider()
          ->GetDividerBoundsInScreen(false /* is_dragging */)
          .CenterPoint();
  GetEventGenerator()->set_current_location(divider_center);
  GetEventGenerator()->DoubleClickLeftButton();

  EXPECT_EQ(split_view_controller()->left_window(), window2.get());
  EXPECT_EQ(split_view_controller()->right_window(), window1.get());
  EXPECT_EQ(left_bounds, window2->GetBoundsInScreen());
  EXPECT_EQ(right_bounds, window1->GetBoundsInScreen());
}

// Verifies that by long pressing on the overview button tray, split view gets
// activated iff we have two or more windows in the mru list.
TEST_F(SplitViewControllerTest, LongPressEntersSplitView) {
  // Verify that with no active windows, split view does not get activated.
  LongPressOnOverivewButtonTray();
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());

  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  wm::ActivateWindow(window1.get());

  // Verify that with only one window, split view does not get activated.
  LongPressOnOverivewButtonTray();
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());

  // Verify that with two windows, split view gets activated.
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  wm::ActivateWindow(window2.get());
  LongPressOnOverivewButtonTray();
  EXPECT_TRUE(split_view_controller()->IsSplitViewModeActive());
}

// Verify that when in split view mode with either one snapped or two snapped
// windows, split view mode gets exited when the overview button gets a long
// press event.
TEST_F(SplitViewControllerTest, LongPressExitsSplitView) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window1.get());

  // Snap |window1| to the left.
  ToggleOverview();
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  ASSERT_TRUE(split_view_controller()->IsSplitViewModeActive());

  // Verify that by long pressing on the overview button tray with left snapped
  // window, split view mode gets exited and the left window (|window1|) is the
  // current active window.
  LongPressOnOverivewButtonTray();
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());
  EXPECT_FALSE(Shell::Get()->window_selector_controller()->IsSelecting());
  EXPECT_EQ(window1.get(), wm::GetActiveWindow());

  // Snap |window1| to the right.
  ToggleOverview();
  split_view_controller()->SnapWindow(window1.get(),
                                      SplitViewController::RIGHT);
  ASSERT_TRUE(split_view_controller()->IsSplitViewModeActive());

  // Verify that by long pressing on the overview button tray with right snapped
  // window, split view mode gets exited and the right window (|window1|) is the
  // current active window.
  LongPressOnOverivewButtonTray();
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());
  EXPECT_FALSE(Shell::Get()->window_selector_controller()->IsSelecting());
  EXPECT_EQ(window1.get(), wm::GetActiveWindow());

  // Snap two windows and activate the left window, |window1|.
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  wm::ActivateWindow(window1.get());
  ASSERT_TRUE(split_view_controller()->IsSplitViewModeActive());

  // Verify that by long pressing on the overview button tray with two snapped
  // windows, split view mode gets exited.
  LongPressOnOverivewButtonTray();
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());
  EXPECT_EQ(window1.get(), wm::GetActiveWindow());

  // Snap two windows and activate the right window, |window2|.
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  wm::ActivateWindow(window2.get());
  ASSERT_TRUE(split_view_controller()->IsSplitViewModeActive());

  // Verify that by long pressing on the overview button tray with two snapped
  // windows, split view mode gets exited, and the activated window in splitview
  // is the current active window.
  LongPressOnOverivewButtonTray();
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());
  EXPECT_EQ(window2.get(), wm::GetActiveWindow());
}

// Verify that if a window with a transient child which is not snappable is
// activated, and the the overview tray is long pressed, we will enter splitview
// with the transient parent snapped.
TEST_F(SplitViewControllerTest, LongPressEntersSplitViewWithTransientChild) {
  // Add two windows with one being a transient child of the first.
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> parent(CreateWindow(bounds));
  std::unique_ptr<aura::Window> child(
      CreateWindow(bounds, aura::client::WINDOW_TYPE_POPUP));
  ::wm::AddTransientChild(parent.get(), child.get());
  ::wm::ActivateWindow(parent.get());
  ::wm::ActivateWindow(child.get());

  // Verify that long press on the overview button will not enter split view
  // mode, as there needs to be two non-transient child windows.
  LongPressOnOverivewButtonTray();
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());

  // Add a third window. Focus the transient child.
  std::unique_ptr<aura::Window> third_window(CreateWindow(bounds));
  ::wm::ActivateWindow(third_window.get());
  ::wm::ActivateWindow(parent.get());
  ::wm::ActivateWindow(child.get());

  // Verify that long press will snap the focused transient child's parent.
  LongPressOnOverivewButtonTray();
  EXPECT_TRUE(split_view_controller()->IsSplitViewModeActive());
  EXPECT_EQ(split_view_controller()->GetDefaultSnappedWindow(), parent.get());
}

TEST_F(SplitViewControllerTest, LongPressExitsSplitViewWithTransientChild) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> left_window(CreateWindow(bounds));
  std::unique_ptr<aura::Window> right_window(CreateWindow(bounds));
  wm::ActivateWindow(left_window.get());
  wm::ActivateWindow(right_window.get());

  ToggleOverview();
  split_view_controller()->SnapWindow(left_window.get(),
                                      SplitViewController::LEFT);
  split_view_controller()->SnapWindow(right_window.get(),
                                      SplitViewController::RIGHT);
  ASSERT_TRUE(split_view_controller()->IsSplitViewModeActive());

  // Add a transient child to |right_window|, and activate it.
  aura::Window* transient_child =
      aura::test::CreateTestWindowWithId(0, right_window.get());
  ::wm::AddTransientChild(right_window.get(), transient_child);
  wm::ActivateWindow(transient_child);

  // Verify that by long pressing on the overview button tray, split view mode
  // gets exited and the window which contained |transient_child| is the
  // current active window.
  LongPressOnOverivewButtonTray();
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());
  EXPECT_FALSE(Shell::Get()->window_selector_controller()->IsSelecting());
  EXPECT_EQ(right_window.get(), wm::GetActiveWindow());
}

// Verify that split view mode get activated when long pressing on the overview
// button while in overview mode iff we have more than two windows in the mru
// list.
TEST_F(SplitViewControllerTest, LongPressInOverviewMode) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  wm::ActivateWindow(window1.get());

  ToggleOverview();
  ASSERT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());
  ASSERT_FALSE(split_view_controller()->IsSplitViewModeActive());

  // Nothing happens if there is only one window.
  LongPressOnOverivewButtonTray();
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());

  // Verify that with two windows, a long press on the overview button tray will
  // enter splitview.
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  wm::ActivateWindow(window2.get());
  ToggleOverview();
  ASSERT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());
  ASSERT_FALSE(split_view_controller()->IsSplitViewModeActive());

  LongPressOnOverivewButtonTray();
  EXPECT_TRUE(split_view_controller()->IsSplitViewModeActive());
  EXPECT_EQ(window2.get(), split_view_controller()->left_window());
}

TEST_F(SplitViewControllerTest, LongPressWithUnsnappableWindow) {
  // Add one unsnappable window and two regular windows.
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> unsnappable_window(
      CreateNonSnappableWindow(bounds));
  ASSERT_FALSE(split_view_controller()->IsSplitViewModeActive());
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));
  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window3.get());
  wm::ActivateWindow(unsnappable_window.get());
  ASSERT_EQ(unsnappable_window.get(), wm::GetActiveWindow());

  // Verify split view is not activated when long press occurs when active
  // window is unsnappable.
  LongPressOnOverivewButtonTray();
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());

  // Verify split view is not activated when long press occurs in overview mode
  // and the most recent window is unsnappable.
  ToggleOverview();
  ASSERT_TRUE(
      Shell::Get()->mru_window_tracker()->BuildWindowForCycleList().size() > 0);
  ASSERT_EQ(unsnappable_window.get(),
            Shell::Get()->mru_window_tracker()->BuildWindowForCycleList()[0]);
  LongPressOnOverivewButtonTray();
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());
}

// Test the rotation functionalities in split view mode.
TEST_F(SplitViewControllerTest, RotationTest) {
  UpdateDisplay("807x407");
  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display::test::ScopedSetInternalDisplayId set_internal(display_manager,
                                                         display_id);
  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());

  // Set the screen orientation to LANDSCAPE_PRIMARY.
  test_api.SetDisplayRotation(display::Display::ROTATE_0,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            OrientationLockType::kLandscapePrimary);

  const gfx::Rect bounds(0, 0, 200, 200);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  gfx::Rect bounds_window1 = window1->GetBoundsInScreen();
  gfx::Rect bounds_window2 = window2->GetBoundsInScreen();
  gfx::Rect bounds_divider =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);

  // Test |window1|, divider and |window2| are aligned horizontally.
  // |window1| is on the left, then the divider, and then |window2|.
  EXPECT_EQ(bounds_divider.x(), bounds_window1.x() + bounds_window1.width());
  EXPECT_EQ(bounds_window2.x(), bounds_divider.x() + bounds_divider.width());
  EXPECT_EQ(bounds_window1.height(), bounds_divider.height());
  EXPECT_EQ(bounds_window1.height(), bounds_window2.height());

  // Rotate the screen by 270 degree.
  test_api.SetDisplayRotation(display::Display::ROTATE_270,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            OrientationLockType::kPortraitPrimary);

  bounds_window1 = window1->GetBoundsInScreen();
  bounds_window2 = window2->GetBoundsInScreen();
  bounds_divider =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);

  // Test that |window1|, divider, |window2| are now aligned vertically.
  // |window1| is on the top, then the divider, and then |window2|.
  EXPECT_EQ(bounds_divider.y(), bounds_window1.y() + bounds_window1.height());
  EXPECT_EQ(bounds_window2.y(), bounds_divider.y() + bounds_divider.height());
  EXPECT_EQ(bounds_window1.width(), bounds_divider.width());
  EXPECT_EQ(bounds_window1.width(), bounds_window2.width());

  // Rotate the screen by 180 degree.
  test_api.SetDisplayRotation(display::Display::ROTATE_180,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            OrientationLockType::kLandscapeSecondary);

  bounds_window1 = window1->GetBoundsInScreen();
  bounds_window2 = window2->GetBoundsInScreen();
  bounds_divider =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);

  // Test that |window1|, divider, |window2| are now aligned horizontally.
  // |window2| is on the left, then the divider, and then |window1|.
  EXPECT_EQ(bounds_divider.x(), bounds_window2.x() + bounds_window2.width());
  EXPECT_EQ(bounds_window1.x(), bounds_divider.x() + bounds_divider.width());
  EXPECT_EQ(bounds_window1.height(), bounds_divider.height());
  EXPECT_EQ(bounds_window1.height(), bounds_window2.height());

  // Rotate the screen by 90 degree.
  test_api.SetDisplayRotation(display::Display::ROTATE_90,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            OrientationLockType::kPortraitSecondary);
  bounds_window1 = window1->GetBoundsInScreen();
  bounds_window2 = window2->GetBoundsInScreen();
  bounds_divider =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);

  // Test that |window1|, divider, |window2| are now aligned vertically.
  // |window2| is on the top, then the divider, and then |window1|.
  EXPECT_EQ(bounds_divider.y(), bounds_window2.y() + bounds_window2.height());
  EXPECT_EQ(bounds_window1.y(), bounds_divider.y() + bounds_divider.height());
  EXPECT_EQ(bounds_window1.width(), bounds_divider.width());
  EXPECT_EQ(bounds_window1.width(), bounds_window2.width());

  // Rotate the screen back to 0 degree.
  test_api.SetDisplayRotation(display::Display::ROTATE_0,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            OrientationLockType::kLandscapePrimary);
  bounds_window1 = window1->GetBoundsInScreen();
  bounds_window2 = window2->GetBoundsInScreen();
  bounds_divider =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);

  // Test |window1|, divider and |window2| are aligned horizontally.
  // |window1| is on the left, then the divider, and then |window2|.
  EXPECT_EQ(bounds_divider.x(), bounds_window1.x() + bounds_window1.width());
  EXPECT_EQ(bounds_window2.x(), bounds_divider.x() + bounds_divider.width());
  EXPECT_EQ(bounds_window1.height(), bounds_divider.height());
  EXPECT_EQ(bounds_window1.height(), bounds_window2.height());
}

// Test that if the split view mode is active when exiting tablet mode, we
// should also end split view mode.
TEST_F(SplitViewControllerTest, ExitTabletModeEndSplitView) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  EXPECT_TRUE(split_view_controller()->IsSplitViewModeActive());

  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(false);
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());
}

// Tests that if a window's minimum size is larger than half of the display work
// area's size, it can't be snapped.
TEST_F(SplitViewControllerTest, SnapWindowWithMinimumSizeTest) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  EXPECT_TRUE(split_view_controller()->CanSnap(window1.get()));

  const gfx::Rect display_bounds =
      split_view_controller()->GetDisplayWorkAreaBoundsInScreen(window1.get());
  aura::test::TestWindowDelegate* delegate =
      static_cast<aura::test::TestWindowDelegate*>(window1->delegate());
  delegate->set_minimum_size(
      gfx::Size(display_bounds.width() * 0.5f, display_bounds.height()));
  EXPECT_TRUE(split_view_controller()->CanSnap(window1.get()));

  delegate->set_minimum_size(
      gfx::Size(display_bounds.width() * 0.67f, display_bounds.height()));
  EXPECT_FALSE(split_view_controller()->CanSnap(window1.get()));
}

// Tests that the snapped window can not be moved outside of work area when its
// minimum size is larger than its current desired resizing bounds.
TEST_F(SplitViewControllerTest, ResizingSnappedWindowWithMinimumSizeTest) {
  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display::test::ScopedSetInternalDisplayId set_internal(display_manager,
                                                         display_id);
  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());

  const gfx::Rect bounds(0, 0, 300, 200);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  aura::test::TestWindowDelegate* delegate1 =
      static_cast<aura::test::TestWindowDelegate*>(window1->delegate());

  // Set the screen orientation to LANDSCAPE_PRIMARY
  test_api.SetDisplayRotation(display::Display::ROTATE_0,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            OrientationLockType::kLandscapePrimary);

  gfx::Rect display_bounds =
      split_view_controller()->GetDisplayWorkAreaBoundsInScreen(window1.get());
  EXPECT_TRUE(split_view_controller()->CanSnap(window1.get()));
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  delegate1->set_minimum_size(
      gfx::Size(display_bounds.width() * 0.4f, display_bounds.height()));
  EXPECT_TRUE(window1->layer()->GetTargetTransform().IsIdentity());

  gfx::Rect divider_bounds =
      split_view_divider()->GetDividerBoundsInScreen(false);
  split_view_controller()->StartResize(divider_bounds.CenterPoint());
  gfx::Point resize_point(display_bounds.width() * 0.33f, 0);
  split_view_controller()->Resize(resize_point);

  gfx::Rect snapped_window_bounds =
      split_view_controller()->GetSnappedWindowBoundsInScreen(
          window1.get(), SplitViewController::LEFT);
  // The snapped window bounds can't be pushed outside of the display area.
  EXPECT_EQ(snapped_window_bounds.x(), display_bounds.x());
  EXPECT_EQ(snapped_window_bounds.width(),
            window1->delegate()->GetMinimumSize().width());
  EXPECT_FALSE(window1->layer()->GetTargetTransform().IsIdentity());
  split_view_controller()->EndResize(resize_point);
  EndSplitView();

  // Rotate the screen by 270 degree.
  test_api.SetDisplayRotation(display::Display::ROTATE_270,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            OrientationLockType::kPortraitPrimary);

  display_bounds =
      split_view_controller()->GetDisplayWorkAreaBoundsInScreen(window1.get());
  delegate1->set_minimum_size(
      gfx::Size(display_bounds.width(), display_bounds.height() * 0.4f));
  EXPECT_TRUE(split_view_controller()->CanSnap(window1.get()));
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  EXPECT_TRUE(window1->layer()->GetTargetTransform().IsIdentity());
  divider_bounds = split_view_divider()->GetDividerBoundsInScreen(false);
  split_view_controller()->StartResize(divider_bounds.CenterPoint());
  resize_point.SetPoint(0, display_bounds.height() * 0.33f);
  split_view_controller()->Resize(resize_point);

  snapped_window_bounds =
      split_view_controller()->GetSnappedWindowBoundsInScreen(
          window1.get(), SplitViewController::LEFT);
  EXPECT_EQ(snapped_window_bounds.y(), display_bounds.y());
  EXPECT_EQ(snapped_window_bounds.height(),
            window1->delegate()->GetMinimumSize().height());
  EXPECT_FALSE(window1->layer()->GetTargetTransform().IsIdentity());
  split_view_controller()->EndResize(resize_point);
  EndSplitView();

  // Rotate the screen by 180 degree.
  test_api.SetDisplayRotation(display::Display::ROTATE_180,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            OrientationLockType::kLandscapeSecondary);

  display_bounds =
      split_view_controller()->GetDisplayWorkAreaBoundsInScreen(window1.get());
  delegate1->set_minimum_size(
      gfx::Size(display_bounds.width() * 0.4f, display_bounds.height()));
  EXPECT_TRUE(split_view_controller()->CanSnap(window1.get()));
  split_view_controller()->SnapWindow(window1.get(),
                                      SplitViewController::RIGHT);
  EXPECT_TRUE(window1->layer()->GetTargetTransform().IsIdentity());

  divider_bounds = split_view_divider()->GetDividerBoundsInScreen(false);
  split_view_controller()->StartResize(divider_bounds.CenterPoint());
  resize_point.SetPoint(display_bounds.width() * 0.33f, 0);
  split_view_controller()->Resize(resize_point);

  snapped_window_bounds =
      split_view_controller()->GetSnappedWindowBoundsInScreen(
          window1.get(), SplitViewController::RIGHT);
  EXPECT_EQ(snapped_window_bounds.x(), display_bounds.x());
  EXPECT_EQ(snapped_window_bounds.width(),
            window1->delegate()->GetMinimumSize().width());
  EXPECT_FALSE(window1->layer()->GetTargetTransform().IsIdentity());
  split_view_controller()->EndResize(resize_point);
  EndSplitView();

  // Rotate the screen by 90 degree.
  test_api.SetDisplayRotation(display::Display::ROTATE_90,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            OrientationLockType::kPortraitSecondary);

  display_bounds =
      split_view_controller()->GetDisplayWorkAreaBoundsInScreen(window1.get());
  delegate1->set_minimum_size(
      gfx::Size(display_bounds.width(), display_bounds.height() * 0.4f));
  EXPECT_TRUE(split_view_controller()->CanSnap(window1.get()));
  split_view_controller()->SnapWindow(window1.get(),
                                      SplitViewController::RIGHT);
  EXPECT_TRUE(window1->layer()->GetTargetTransform().IsIdentity());

  divider_bounds = split_view_divider()->GetDividerBoundsInScreen(false);
  split_view_controller()->StartResize(divider_bounds.CenterPoint());
  resize_point.SetPoint(0, display_bounds.height() * 0.33f);
  split_view_controller()->Resize(resize_point);

  snapped_window_bounds =
      split_view_controller()->GetSnappedWindowBoundsInScreen(
          window1.get(), SplitViewController::RIGHT);
  EXPECT_EQ(snapped_window_bounds.y(), display_bounds.y());
  EXPECT_EQ(snapped_window_bounds.height(),
            window1->delegate()->GetMinimumSize().height());
  EXPECT_FALSE(window1->layer()->GetTargetTransform().IsIdentity());
  split_view_controller()->EndResize(resize_point);
  EndSplitView();
}

// Tests that the divider should not be moved to a position that is smaller than
// the snapped window's minimum size after resizing.
TEST_F(SplitViewControllerTest,
       DividerPositionOnResizingSnappedWindowWithMinimumSizeTest) {
  const gfx::Rect bounds(0, 0, 200, 200);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  aura::test::TestWindowDelegate* delegate1 =
      static_cast<aura::test::TestWindowDelegate*>(window1->delegate());
  ui::test::EventGenerator* generator = GetEventGenerator();
  EXPECT_EQ(OrientationLockType::kLandscapePrimary, screen_orientation());
  gfx::Rect workarea_bounds =
      split_view_controller()->GetDisplayWorkAreaBoundsInScreen(window1.get());

  // Snap the divider to one third position when there is only left window with
  // minimum size larger than one third of the display's width. The divider
  // should be snapped to the middle position after dragging.
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  delegate1->set_minimum_size(
      gfx::Size(workarea_bounds.width() * 0.4f, workarea_bounds.height()));
  gfx::Rect divider_bounds =
      split_view_divider()->GetDividerBoundsInScreen(false);
  generator->set_current_location(divider_bounds.CenterPoint());
  generator->DragMouseTo(gfx::Point(workarea_bounds.width() * 0.33f, 0));
  EXPECT_GT(divider_position(), 0.33f * workarea_bounds.width());
  EXPECT_LE(divider_position(), 0.5f * workarea_bounds.width());

  // Snap the divider to two third position, it should be kept at there after
  // dragging.
  generator->set_current_location(divider_bounds.CenterPoint());
  generator->DragMouseTo(gfx::Point(workarea_bounds.width() * 0.67f, 0));
  EXPECT_GT(divider_position(), 0.5f * workarea_bounds.width());
  EXPECT_LE(divider_position(), 0.67f * workarea_bounds.width());
  EndSplitView();

  // Snap the divider to two third position when there is only right window with
  // minium size larger than one third of the display's width. The divider
  // should be snapped to the middle position after dragging.
  delegate1->set_minimum_size(
      gfx::Size(workarea_bounds.width() * 0.4f, workarea_bounds.height()));
  split_view_controller()->SnapWindow(window1.get(),
                                      SplitViewController::RIGHT);
  divider_bounds = split_view_divider()->GetDividerBoundsInScreen(false);
  generator->set_current_location(divider_bounds.CenterPoint());
  generator->DragMouseTo(gfx::Point(workarea_bounds.width() * 0.67f, 0));
  EXPECT_GT(divider_position(), 0.33f * workarea_bounds.width());
  EXPECT_LE(divider_position(), 0.5f * workarea_bounds.width());

  // Snap the divider to one third position, it should be kept at there after
  // dragging.
  generator->set_current_location(divider_bounds.CenterPoint());
  generator->DragMouseTo(gfx::Point(workarea_bounds.width() * 0.33f, 0));
  EXPECT_GT(divider_position(), 0);
  EXPECT_LE(divider_position(), 0.33f * workarea_bounds.width());
  EndSplitView();

  // Snap the divider to one third position when there are both left and right
  // snapped windows with the same minimum size larger than one third of the
  // display's width. The divider should be snapped to the middle position after
  // dragging.
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  aura::test::TestWindowDelegate* delegate2 =
      static_cast<aura::test::TestWindowDelegate*>(window2->delegate());
  delegate2->set_minimum_size(
      gfx::Size(workarea_bounds.width() * 0.4f, workarea_bounds.height()));
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  divider_bounds = split_view_divider()->GetDividerBoundsInScreen(false);
  generator->set_current_location(divider_bounds.CenterPoint());
  generator->DragMouseTo(gfx::Point(workarea_bounds.width() * 0.33f, 0));
  EXPECT_GT(divider_position(), 0.33f * workarea_bounds.width());
  EXPECT_LE(divider_position(), 0.5f * workarea_bounds.width());

  // Snap the divider to two third position, it should be snapped to the middle
  // position after dragging.
  generator->set_current_location(divider_bounds.CenterPoint());
  generator->DragMouseTo(gfx::Point(workarea_bounds.width() * 0.67f, 0));
  EXPECT_GT(divider_position(), 0.33f * workarea_bounds.width());
  EXPECT_LE(divider_position(), 0.5f * workarea_bounds.width());
  EndSplitView();
}

// Tests that the divider and snapped windows bounds should be updated if
// snapping a new window with minimum size, which is larger than the bounds
// of its snap position.
TEST_F(SplitViewControllerTest,
       DividerPositionWithWindowMinimumSizeOnSnapTest) {
  const gfx::Rect bounds(0, 0, 200, 300);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  const gfx::Rect workarea_bounds =
      split_view_controller()->GetDisplayWorkAreaBoundsInScreen(window1.get());

  // Divider should be moved to the middle at the beginning.
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  ASSERT_TRUE(split_view_divider());
  EXPECT_GT(divider_position(), 0.33f * workarea_bounds.width());
  EXPECT_LE(divider_position(), 0.5f * workarea_bounds.width());

  // Drag the divider to two-third position.
  ui::test::EventGenerator* generator = GetEventGenerator();
  gfx::Rect divider_bounds =
      split_view_divider()->GetDividerBoundsInScreen(false);
  generator->set_current_location(divider_bounds.CenterPoint());
  generator->DragMouseTo(gfx::Point(workarea_bounds.width() * 0.67f, 0));
  EXPECT_GT(divider_position(), 0.5f * workarea_bounds.width());
  EXPECT_LE(divider_position(), 0.67f * workarea_bounds.width());

  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  aura::test::TestWindowDelegate* delegate2 =
      static_cast<aura::test::TestWindowDelegate*>(window2->delegate());
  delegate2->set_minimum_size(
      gfx::Size(workarea_bounds.width() * 0.4f, workarea_bounds.height()));
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  EXPECT_GT(divider_position(), 0.33f * workarea_bounds.width());
  EXPECT_LE(divider_position(), 0.5f * workarea_bounds.width());
}

// Test that if display configuration changes in lock screen, the split view
// mode doesn't end.
TEST_F(SplitViewControllerTest, DoNotEndSplitViewInLockScreen) {
  display::test::DisplayManagerTestApi(display_manager())
      .SetFirstDisplayAsInternalDisplay();
  UpdateDisplay("800x400");
  const gfx::Rect bounds(0, 0, 200, 300);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  EXPECT_TRUE(split_view_controller()->IsSplitViewModeActive());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::BOTH_SNAPPED);

  // Now lock the screen.
  Shell::Get()->session_controller()->LockScreenAndFlushForTest();
  // Change display configuration. Split view mode is still active.
  UpdateDisplay("400x800");
  EXPECT_TRUE(split_view_controller()->IsSplitViewModeActive());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::BOTH_SNAPPED);

  // Now unlock the screen.
  GetSessionControllerClient()->UnlockScreen();
  EXPECT_TRUE(split_view_controller()->IsSplitViewModeActive());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::BOTH_SNAPPED);
}

// Test that when split view and overview are both active when a new window is
// added to the window hierarchy, overview is not ended.
TEST_F(SplitViewControllerTest, NewWindowTest) {
  const gfx::Rect bounds(0, 0, 200, 300);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  ToggleOverview();
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::LEFT_SNAPPED);
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());

  // Now new a window. Test it won't end the overview mode
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());
}

// Tests that when split view ends because of a transition from tablet mode to
// laptop mode during a resize operation, drags are properly completed.
TEST_F(SplitViewControllerTest, ExitTabletModeDuringResizeCompletesDrags) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  auto* w1_state = wm::GetWindowState(window1.get());
  auto* w2_state = wm::GetWindowState(window2.get());

  // Setup delegates
  auto* window_state_delegate1 = new TestWindowStateDelegate();
  auto* window_state_delegate2 = new TestWindowStateDelegate();
  w1_state->SetDelegate(base::WrapUnique(window_state_delegate1));
  w2_state->SetDelegate(base::WrapUnique(window_state_delegate2));

  // Set up windows.
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);

  // Start a drag but don't release the mouse button.
  gfx::Rect divider_bounds =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);
  const int screen_width =
      screen_util::GetDisplayWorkAreaBoundsInParent(window1.get()).width();
  GetEventGenerator()->set_current_location(divider_bounds.CenterPoint());
  GetEventGenerator()->PressLeftButton();
  GetEventGenerator()->MoveMouseTo(screen_width * 0.67f, 0);

  // Drag is started for both windows.
  EXPECT_TRUE(window_state_delegate1->drag_in_progress());
  EXPECT_TRUE(window_state_delegate2->drag_in_progress());
  EXPECT_NE(nullptr, w1_state->drag_details());
  EXPECT_NE(nullptr, w2_state->drag_details());

  // End tablet mode.
  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(false);

  // Drag is ended for both windows.
  EXPECT_EQ(nullptr, w1_state->drag_details());
  EXPECT_EQ(nullptr, w2_state->drag_details());
  EXPECT_FALSE(window_state_delegate1->drag_in_progress());
  EXPECT_FALSE(window_state_delegate2->drag_in_progress());
}

// Tests that when a single window is present in split view mode is minimized
// during a resize operation, then drags are properly completed.
TEST_F(SplitViewControllerTest,
       MinimizeSingleWindowDuringResizeCompletesDrags) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  auto* w1_state = wm::GetWindowState(window1.get());

  // Setup delegate
  auto* window_state_delegate1 = new TestWindowStateDelegate();
  w1_state->SetDelegate(base::WrapUnique(window_state_delegate1));

  // Set up window.
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);

  // Start a drag but don't release the mouse button.
  gfx::Rect divider_bounds =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);
  const int screen_width =
      screen_util::GetDisplayWorkAreaBoundsInParent(window1.get()).width();
  GetEventGenerator()->set_current_location(divider_bounds.CenterPoint());
  GetEventGenerator()->PressLeftButton();
  GetEventGenerator()->MoveMouseTo(screen_width * 0.67f, 0);

  // Drag is started.
  EXPECT_TRUE(window_state_delegate1->drag_in_progress());
  EXPECT_NE(nullptr, w1_state->drag_details());

  // Minimize the window.
  wm::WMEvent minimize_event(wm::WM_EVENT_MINIMIZE);
  wm::GetWindowState(window1.get())->OnWMEvent(&minimize_event);

  // Drag is ended.
  EXPECT_FALSE(window_state_delegate1->drag_in_progress());
  EXPECT_EQ(nullptr, w1_state->drag_details());
}

// Tests that when two windows are present in split view mode and one of them
// is minimized during a resize, then drags are properly completed.
TEST_F(SplitViewControllerTest,
       MinimizeOneOfTwoWindowsDuringResizeCompletesDrags) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  auto* w1_state = wm::GetWindowState(window1.get());
  auto* w2_state = wm::GetWindowState(window2.get());

  // Setup delegates
  auto* window_state_delegate1 = new TestWindowStateDelegate();
  auto* window_state_delegate2 = new TestWindowStateDelegate();
  w1_state->SetDelegate(base::WrapUnique(window_state_delegate1));
  w2_state->SetDelegate(base::WrapUnique(window_state_delegate2));

  // Set up windows.
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);

  // Start a drag but don't release the mouse button.
  gfx::Rect divider_bounds =
      split_view_divider()->GetDividerBoundsInScreen(false /* is_dragging */);
  const int screen_width =
      screen_util::GetDisplayWorkAreaBoundsInParent(window1.get()).width();
  GetEventGenerator()->set_current_location(divider_bounds.CenterPoint());
  GetEventGenerator()->PressLeftButton();
  GetEventGenerator()->MoveMouseTo(screen_width * 0.67f, 0);

  // Drag is started for both windows.
  EXPECT_TRUE(window_state_delegate1->drag_in_progress());
  EXPECT_TRUE(window_state_delegate2->drag_in_progress());
  EXPECT_NE(nullptr, w1_state->drag_details());
  EXPECT_NE(nullptr, w2_state->drag_details());

  // Minimize the left window.
  wm::WMEvent minimize_event(wm::WM_EVENT_MINIMIZE);
  wm::GetWindowState(window1.get())->OnWMEvent(&minimize_event);

  // Drag is ended just for the left window.
  EXPECT_FALSE(window_state_delegate1->drag_in_progress());
  EXPECT_TRUE(window_state_delegate2->drag_in_progress());
  EXPECT_EQ(nullptr, w1_state->drag_details());
  EXPECT_NE(nullptr, w2_state->drag_details());
}

// Test that when a snapped window's resizablity property change from resizable
// to unresizable, the split view mode is ended.
TEST_F(SplitViewControllerTest, ResizabilityChangeTest) {
  const gfx::Rect bounds(0, 0, 200, 300);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  EXPECT_TRUE(split_view_controller()->IsSplitViewModeActive());

  window1->SetProperty(aura::client::kResizeBehaviorKey,
                       ws::mojom::kResizeBehaviorNone);
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());
}

// Tests that shadows on windows disappear when the window is snapped, and
// reappear when unsnapped.
TEST_F(SplitViewControllerTest, ShadowDisappearsWhenSnapped) {
  const gfx::Rect bounds(200, 200);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));

  ::wm::ShadowController* shadow_controller = Shell::Get()->shadow_controller();
  EXPECT_TRUE(shadow_controller->IsShadowVisibleForWindow(window1.get()));
  EXPECT_TRUE(shadow_controller->IsShadowVisibleForWindow(window2.get()));
  EXPECT_TRUE(shadow_controller->IsShadowVisibleForWindow(window3.get()));

  // Snap |window1| to the left. Its shadow should disappear.
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  EXPECT_FALSE(shadow_controller->IsShadowVisibleForWindow(window1.get()));
  EXPECT_TRUE(shadow_controller->IsShadowVisibleForWindow(window2.get()));
  EXPECT_TRUE(shadow_controller->IsShadowVisibleForWindow(window3.get()));

  // Snap |window2| to the right. Its shadow should also disappear.
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  EXPECT_FALSE(shadow_controller->IsShadowVisibleForWindow(window1.get()));
  EXPECT_FALSE(shadow_controller->IsShadowVisibleForWindow(window2.get()));
  EXPECT_TRUE(shadow_controller->IsShadowVisibleForWindow(window3.get()));

  // Snap |window3| to the right. Its shadow should disappear and |window2|'s
  // shadow should reappear.
  split_view_controller()->SnapWindow(window3.get(),
                                      SplitViewController::RIGHT);
  EXPECT_FALSE(shadow_controller->IsShadowVisibleForWindow(window1.get()));
  EXPECT_TRUE(shadow_controller->IsShadowVisibleForWindow(window2.get()));
  EXPECT_FALSE(shadow_controller->IsShadowVisibleForWindow(window3.get()));
}

// Tests that if snapping a window causes overview to end (e.g., select two
// windows in overview mode to snap to both side of the screen), or toggle
// overview to end overview causes a window to snap, we should not have the
// exiting animation.
TEST_F(SplitViewControllerTest, OverviewExitAnimationTest) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));

  // 1) For normal toggle overview case, we should have animation when
  // exiting overview.
  std::unique_ptr<OverviewStatesObserver> overview_observer =
      std::make_unique<OverviewStatesObserver>(window1->GetRootWindow());
  ToggleOverview();
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());
  ToggleOverview();
  EXPECT_FALSE(Shell::Get()->window_selector_controller()->IsSelecting());
  EXPECT_TRUE(overview_observer->overview_animate_when_exiting());

  // 2) If overview is ended because of activating a window:
  ToggleOverview();
  // It will end overview.
  wm::ActivateWindow(window1.get());
  EXPECT_FALSE(Shell::Get()->window_selector_controller()->IsSelecting());
  EXPECT_TRUE(overview_observer->overview_animate_when_exiting());

  // 3) If overview is ended because of snapping a window:
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  // Reset the observer as we'll need the OverviewStatesObserver to be added to
  // to ShellObserver list after SplitViewController.
  overview_observer.reset(new OverviewStatesObserver(window1->GetRootWindow()));
  ToggleOverview();  // Start overview.
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());
  // Test |overview_animate_when_exiting_| has been properly reset.
  EXPECT_TRUE(overview_observer->overview_animate_when_exiting());
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  EXPECT_FALSE(Shell::Get()->window_selector_controller()->IsSelecting());
  EXPECT_FALSE(overview_observer->overview_animate_when_exiting());

  // 4) If ending overview causes a window to snap:
  ToggleOverview();
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());
  // Test |overview_animate_when_exiting_| has been properly reset.
  EXPECT_TRUE(overview_observer->overview_animate_when_exiting());
  ToggleOverview();
  EXPECT_FALSE(Shell::Get()->window_selector_controller()->IsSelecting());
  EXPECT_FALSE(overview_observer->overview_animate_when_exiting());
}

// Test the window state is normally maximized on splitview end, except when we
// end it from home launcher.
TEST_F(SplitViewControllerTest, WindowStateOnExit) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  using svc = SplitViewController;
  // Tests that normally, window will maximize on splitview ended.
  split_view_controller()->SnapWindow(window1.get(), svc::LEFT);
  split_view_controller()->SnapWindow(window2.get(), svc::RIGHT);
  split_view_controller()->EndSplitView();
  EXPECT_TRUE(wm::GetWindowState(window1.get())->IsMaximized());
  EXPECT_TRUE(wm::GetWindowState(window2.get())->IsMaximized());

  // Tests that if we end splitview from home launcher, the windows do not get
  // maximized.
  split_view_controller()->SnapWindow(window1.get(), svc::LEFT);
  split_view_controller()->SnapWindow(window2.get(), svc::RIGHT);
  split_view_controller()->EndSplitView(
      SplitViewController::EndReason::kHomeLauncherPressed);
  EXPECT_FALSE(wm::GetWindowState(window1.get())->IsMaximized());
  EXPECT_FALSE(wm::GetWindowState(window2.get())->IsMaximized());
}

// Test that if overview and splitview are both active at the same time,
// activiate an unsnappable window should end both overview and splitview mode.
TEST_F(SplitViewControllerTest, ActivateNonSnappableWindow) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateNonSnappableWindow(bounds));

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  ToggleOverview();
  EXPECT_TRUE(split_view_controller()->IsSplitViewModeActive());
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());

  wm::ActivateWindow(window3.get());
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());
  EXPECT_FALSE(Shell::Get()->window_selector_controller()->IsSelecting());
}

// Tests that if a snapped window has a bubble transient child, the bubble's
// bounds should always align with the snapped window's bounds.
TEST_F(SplitViewControllerTest, AdjustTransientChildBounds) {
  std::unique_ptr<views::Widget> widget(CreateTestWidget());
  aura::Window* window = widget->GetNativeWindow();
  window->SetProperty(aura::client::kResizeBehaviorKey,
                      ws::mojom::kResizeBehaviorCanResize |
                          ws::mojom::kResizeBehaviorCanMaximize);
  split_view_controller()->SnapWindow(window, SplitViewController::LEFT);
  const gfx::Rect window_bounds = window->GetBoundsInScreen();

  // Create a bubble widget that's anchored to |widget|.
  views::Widget* bubble_widget = views::BubbleDialogDelegateView::CreateBubble(
      new TestBubbleDialogDelegateView(widget->GetContentsView()));
  aura::Window* bubble_window = bubble_widget->GetNativeWindow();
  EXPECT_TRUE(::wm::HasTransientAncestor(bubble_window, window));
  // Test that the bubble is created inside its anchor widget.
  EXPECT_TRUE(window_bounds.Contains(bubble_window->GetBoundsInScreen()));

  // Now try to manually move the bubble out of the snapped window.
  bubble_window->SetBoundsInScreen(
      split_view_controller()->GetSnappedWindowBoundsInScreen(
          window, SplitViewController::RIGHT),
      display::Screen::GetScreen()->GetDisplayNearestWindow(window));
  // Test that the bubble can't be moved outside of its anchor widget.
  EXPECT_TRUE(window_bounds.Contains(bubble_window->GetBoundsInScreen()));
  EndSplitView();
}

// Tests the divider closest position ratio if work area is not starts from the
// top of the display.
TEST_F(SplitViewControllerTest, DividerClosestRatioOnWorkArea) {
  UpdateDisplay("1200x800");
  // Docked magnifier will put a view port window on the top of the display.
  Shell::Get()->docked_magnifier_controller()->SetEnabled(true);

  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display::test::ScopedSetInternalDisplayId set_internal(display_manager,
                                                         display_id);
  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());
  ui::test::EventGenerator* generator = GetEventGenerator();
  ASSERT_EQ(OrientationLockType::kLandscapePrimary,
            test_api.GetCurrentOrientation());

  const gfx::Rect bounds(0, 0, 200, 200);
  std::unique_ptr<aura::Window> window(CreateWindow(bounds));
  split_view_controller()->SnapWindow(window.get(), SplitViewController::LEFT);

  test_api.SetDisplayRotation(display::Display::ROTATE_90,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(OrientationLockType::kPortraitSecondary,
            test_api.GetCurrentOrientation());
  EXPECT_EQ(divider_closest_ratio(), 0.5f);

  test_api.SetDisplayRotation(display::Display::ROTATE_0,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(OrientationLockType::kLandscapePrimary,
            test_api.GetCurrentOrientation());
  EXPECT_EQ(divider_closest_ratio(), 0.5f);
  gfx::Rect divider_bounds =
      split_view_divider()->GetDividerBoundsInScreen(false);
  gfx::Rect workarea_bounds =
      split_view_controller()->GetDisplayWorkAreaBoundsInScreen(window.get());
  generator->set_current_location(divider_bounds.CenterPoint());
  // Drag the divider to one third position of the work area's width.
  generator->DragMouseTo(
      gfx::Point(workarea_bounds.width() * 0.33f, workarea_bounds.y()));
  EXPECT_EQ(divider_closest_ratio(), 0.33f);

  // Divider closest position ratio changed from one third to two thirds if
  // left/top window changes.
  test_api.SetDisplayRotation(display::Display::ROTATE_90,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(OrientationLockType::kPortraitSecondary,
            test_api.GetCurrentOrientation());
  EXPECT_EQ(divider_closest_ratio(), 0.67f);

  // Divider closest position ratio is kept as one third if left/top window
  // doesn't changes.
  test_api.SetDisplayRotation(display::Display::ROTATE_270,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(OrientationLockType::kPortraitPrimary,
            test_api.GetCurrentOrientation());
  EXPECT_EQ(divider_closest_ratio(), 0.33f);
}

// Test that if we snap an always on top window in splitscreen, there should be
// no crash and the window should stay always on top.
TEST_F(SplitViewControllerTest, AlwaysOnTopWindow) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> always_on_top_window(CreateWindow(bounds));
  always_on_top_window->SetProperty(aura::client::kAlwaysOnTopKey, true);
  std::unique_ptr<aura::Window> normal_window(CreateWindow(bounds));

  split_view_controller()->SnapWindow(always_on_top_window.get(),
                                      SplitViewController::LEFT);
  split_view_controller()->SnapWindow(normal_window.get(),
                                      SplitViewController::RIGHT);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::BOTH_SNAPPED);
  EXPECT_TRUE(always_on_top_window->GetProperty(aura::client::kAlwaysOnTopKey));

  wm::ActivateWindow(always_on_top_window.get());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::BOTH_SNAPPED);
  EXPECT_TRUE(always_on_top_window->GetProperty(aura::client::kAlwaysOnTopKey));

  wm::ActivateWindow(normal_window.get());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::BOTH_SNAPPED);
  EXPECT_TRUE(always_on_top_window->GetProperty(aura::client::kAlwaysOnTopKey));
}

// Test the tab-dragging related functionalities in tablet mode. Tab(s) can be
// dragged out of a window and then put in split view mode or merge into another
// window.
class SplitViewTabDraggingTest : public SplitViewControllerTest {
 public:
  SplitViewTabDraggingTest() = default;
  ~SplitViewTabDraggingTest() override = default;

 protected:
  aura::Window* CreateWindowWithType(
      const gfx::Rect& bounds,
      AppType app_type,
      aura::client::WindowType window_type = aura::client::WINDOW_TYPE_NORMAL) {
    aura::Window* window = CreateTestWindowInShellWithDelegateAndType(
        new SplitViewTestWindowDelegate, window_type, -1, bounds);
    window->SetProperty(aura::client::kAppType, static_cast<int>(app_type));
    wm::GetWindowState(window)->Maximize();
    return window;
  }

  // Starts tab dragging on |dragged_window|. |source_window| indicates which
  // window the drag originates from. Returns the newly created WindowResizer
  // for the |dragged_window|.
  std::unique_ptr<WindowResizer> StartDrag(aura::Window* dragged_window,
                                           aura::Window* source_window) {
    SetIsInTabDragging(dragged_window, /*is_dragging=*/true, source_window);
    return CreateResizerForTest(dragged_window,
                                dragged_window->bounds().origin(), HTCAPTION);
  }

  // Drags the window to |end_position|.
  void DragWindowTo(WindowResizer* resizer, const gfx::Point& end_position) {
    ASSERT_TRUE(resizer);
    resizer->Drag(end_position, 0);
  }

  // Drags the window with offest (delta_x, delta_y) to its initial position.
  void DragWindowWithOffset(WindowResizer* resizer, int delta_x, int delta_y) {
    ASSERT_TRUE(resizer);
    gfx::Point location = resizer->GetInitialLocation();
    location.set_x(location.x() + delta_x);
    location.set_y(location.y() + delta_y);
    resizer->Drag(location, 0);
  }

  // Ends the drag. |resizer| will be deleted after exiting this function.
  void CompleteDrag(std::unique_ptr<WindowResizer> resizer) {
    ASSERT_TRUE(resizer.get());
    resizer->CompleteDrag();
    SetIsInTabDragging(resizer->GetTarget(), /*is_dragging=*/false);
  }

  // Fling to end the drag. |resizer| will be deleted after exiting this
  // function.
  void Fling(std::unique_ptr<WindowResizer> resizer, float velocity_y) {
    ASSERT_TRUE(resizer.get());
    aura::Window* target_window = resizer->GetTarget();
    base::TimeTicks timestamp = base::TimeTicks::Now();
    ui::GestureEventDetails details =
        ui::GestureEventDetails(ui::ET_SCROLL_FLING_START, 0.f, velocity_y);
    ui::GestureEvent event = ui::GestureEvent(
        target_window->bounds().origin().x(),
        target_window->bounds().origin().y(), ui::EF_NONE, timestamp, details);
    ui::Event::DispatcherApi(&event).set_target(target_window);
    resizer->FlingOrSwipe(&event);
    SetIsInTabDragging(resizer->GetTarget(), /*is_dragging=*/false);
  }

  std::unique_ptr<WindowResizer> CreateResizerForTest(
      aura::Window* window,
      const gfx::Point& point_in_parent,
      int window_component,
      ::wm::WindowMoveSource source = ::wm::WINDOW_MOVE_SOURCE_TOUCH) {
    return CreateWindowResizer(window, point_in_parent, window_component,
                               source);
  }

  // Sets if |dragged_window| is currently in tab-dragging process.
  // |source_window| is the window that the drag originates from. This method is
  // used to simulate the start/stop of a window's tab-dragging by setting the
  // two window properties, which are usually set in TabDragController::
  // UpdateTabDraggingInfo() function.
  void SetIsInTabDragging(aura::Window* dragged_window,
                          bool is_dragging,
                          aura::Window* source_window = nullptr) {
    if (!is_dragging) {
      dragged_window->ClearProperty(kIsDraggingTabsKey);
      dragged_window->ClearProperty(kTabDraggingSourceWindowKey);
    } else {
      dragged_window->SetProperty(kIsDraggingTabsKey, is_dragging);
      if (source_window != dragged_window)
        dragged_window->SetProperty(kTabDraggingSourceWindowKey, source_window);
    }
  }

  IndicatorState GetIndicatorState(WindowResizer* resizer) {
    WindowResizer* real_window_resizer;
    // TODO(xdai): This piece of codes seems knowing too much impl details about
    // WindowResizer. Revisit the logic here later to see if there is anything
    // we can do to simplify the logic and hide impl details.
    real_window_resizer = static_cast<DragWindowResizer*>(resizer)
                              ->next_window_resizer_for_testing();
    TabletModeBrowserWindowDragController* browser_controller =
        static_cast<TabletModeBrowserWindowDragController*>(
            real_window_resizer);

    return browser_controller->drag_delegate_for_testing()
        ->split_view_drag_indicators_for_testing()
        ->current_indicator_state();
  }

  int GetIndicatorsThreshold(aura::Window* dragged_window) {
    static const float kIndicatorsThresholdRatio = 0.1f;
    const gfx::Rect work_area_bounds =
        display::Screen::GetScreen()
            ->GetDisplayNearestWindow(dragged_window)
            .work_area();
    return work_area_bounds.y() +
           work_area_bounds.height() * kIndicatorsThresholdRatio;
  }

  gfx::Rect GetDropTargetBoundsDuringDrag(aura::Window* window) const {
    WindowSelector* window_selector =
        Shell::Get()->window_selector_controller()->window_selector();
    DCHECK(window_selector);
    WindowGrid* current_grid =
        window_selector->GetGridWithRootWindow(window->GetRootWindow());
    DCHECK(current_grid);

    WindowSelectorItem* selector_item = current_grid->GetDropTarget();
    return selector_item->GetTransformedBounds();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SplitViewTabDraggingTest);
};

// Test that in tablet mode, we only allow dragging on browser or chrome app
// window's caption area.
TEST_F(SplitViewTabDraggingTest, OnlyAllowDraggingOnBrowserOrChromeAppWindow) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(
      CreateWindowWithType(bounds, AppType::BROWSER));
  std::unique_ptr<aura::Window> window2(
      CreateWindowWithType(bounds, AppType::CHROME_APP));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window4(
      CreateWindowWithType(bounds, AppType::ARC_APP));

  std::unique_ptr<WindowResizer> resizer =
      CreateResizerForTest(window1.get(), gfx::Point(), HTCAPTION);
  EXPECT_TRUE(resizer.get());
  resizer->CompleteDrag();
  resizer.reset();

  resizer = CreateResizerForTest(window2.get(), gfx::Point(), HTCAPTION);
  EXPECT_TRUE(resizer.get());
  resizer->CompleteDrag();
  resizer.reset();

  resizer = CreateResizerForTest(window3.get(), gfx::Point(), HTCAPTION);
  EXPECT_FALSE(resizer.get());

  resizer = CreateResizerForTest(window4.get(), gfx::Point(), HTCAPTION);
  EXPECT_FALSE(resizer.get());
}

// Test that in tablet mode, we only allow dragging that happens on window
// caption or top area.
TEST_F(SplitViewTabDraggingTest, OnlyAllowDraggingOnCaptionOrTopArea) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window(
      CreateWindowWithType(bounds, AppType::BROWSER));

  // Only dragging on HTCAPTION or HTTOP area is allowed.
  std::unique_ptr<WindowResizer> resizer =
      CreateResizerForTest(window.get(), gfx::Point(), HTLEFT);
  EXPECT_FALSE(resizer.get());
  resizer = CreateResizerForTest(window.get(), gfx::Point(), HTRIGHT);
  EXPECT_FALSE(resizer.get());
  resizer = CreateResizerForTest(window.get(), gfx::Point(), HTCAPTION);
  EXPECT_TRUE(resizer.get());
  resizer->CompleteDrag();
  resizer.reset();
  resizer = CreateResizerForTest(window.get(), gfx::Point(), HTTOP);
  EXPECT_TRUE(resizer.get());
  resizer->CompleteDrag();
  resizer.reset();

  // No matter if we're in tab-dragging process, as long as the drag happens on
  // the caption or top area, it should be able to drag the window.
  SetIsInTabDragging(window.get(), /*is_dragging=*/true);
  resizer = CreateResizerForTest(window.get(), gfx::Point(), HTCAPTION);
  EXPECT_TRUE(resizer.get());
  resizer->CompleteDrag();
  resizer.reset();

  SetIsInTabDragging(window.get(), /*is_dragging=*/false);
  resizer = CreateResizerForTest(window.get(), gfx::Point(), HTCAPTION);
  EXPECT_TRUE(resizer.get());
  resizer->CompleteDrag();
  resizer.reset();
}

// Test that in tablet mode, if the dragging is from mouse event, the mouse
// cursor should be properly locked.
TEST_F(SplitViewTabDraggingTest, LockCursor) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window(
      CreateWindowWithType(bounds, AppType::BROWSER));
  SetIsInTabDragging(window.get(), /*is_dragging=*/true);
  EXPECT_FALSE(Shell::Get()->cursor_manager()->IsCursorLocked());

  std::unique_ptr<WindowResizer> resizer = CreateResizerForTest(
      window.get(), gfx::Point(), HTCAPTION, ::wm::WINDOW_MOVE_SOURCE_MOUSE);
  ASSERT_TRUE(resizer.get());
  EXPECT_TRUE(Shell::Get()->cursor_manager()->IsCursorLocked());

  resizer->CompleteDrag();
  resizer.reset();
  EXPECT_FALSE(Shell::Get()->cursor_manager()->IsCursorLocked());
}

// Test that in tablet mode, if a window is in tab-dragging process, its
// backdrop is disabled during dragging process.
TEST_F(SplitViewTabDraggingTest, NoBackDropDuringDragging) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window(
      CreateWindowWithType(bounds, AppType::BROWSER));
  EXPECT_EQ(window->GetProperty(kBackdropWindowMode),
            BackdropWindowMode::kAuto);

  std::unique_ptr<WindowResizer> resizer =
      StartDrag(window.get(), window.get());
  ASSERT_TRUE(resizer.get());
  EXPECT_EQ(window->GetProperty(kBackdropWindowMode),
            BackdropWindowMode::kDisabled);

  resizer->Drag(gfx::Point(), 0);
  EXPECT_EQ(window->GetProperty(kBackdropWindowMode),
            BackdropWindowMode::kDisabled);

  resizer->CompleteDrag();
  EXPECT_EQ(window->GetProperty(kBackdropWindowMode),
            BackdropWindowMode::kAuto);
}

// Test that in tablet mode, the window that is in tab-dragging process should
// not be shown in overview mode.
TEST_F(SplitViewTabDraggingTest, DoNotShowDraggedWindowInOverview) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(
      CreateWindowWithType(bounds, AppType::BROWSER));
  std::unique_ptr<aura::Window> window2(
      CreateWindowWithType(bounds, AppType::BROWSER));

  Shell::Get()->window_selector_controller()->ToggleOverview();
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());
  WindowSelector* window_selector =
      Shell::Get()->window_selector_controller()->window_selector();
  EXPECT_TRUE(window_selector->IsWindowInOverview(window1.get()));
  EXPECT_TRUE(window_selector->IsWindowInOverview(window2.get()));
  Shell::Get()->window_selector_controller()->ToggleOverview();

  std::unique_ptr<WindowResizer> resizer =
      StartDrag(window1.get(), window1.get());

  // Since the source window is the dragged window, the overview should have
  // been opened.
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());
  window_selector =
      Shell::Get()->window_selector_controller()->window_selector();
  EXPECT_FALSE(window_selector->IsWindowInOverview(window1.get()));
  EXPECT_TRUE(window_selector->IsWindowInOverview(window2.get()));

  CompleteDrag(std::move(resizer));
  EXPECT_FALSE(Shell::Get()->window_selector_controller()->IsSelecting());
}

// Test that if a window is in tab-dragging process, the split divider is placed
// below the current dragged window.
TEST_F(SplitViewTabDraggingTest, DividerIsBelowDraggedWindow) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(
      CreateWindowWithType(bounds, AppType::BROWSER));
  std::unique_ptr<aura::Window> window2(
      CreateWindowWithType(bounds, AppType::BROWSER));

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  views::Widget* split_divider_widget =
      split_view_controller()->split_view_divider()->divider_widget();
  EXPECT_TRUE(split_divider_widget->IsAlwaysOnTop());

  std::unique_ptr<WindowResizer> resizer =
      StartDrag(window1.get(), window1.get());
  ASSERT_TRUE(resizer.get());
  EXPECT_FALSE(split_divider_widget->IsAlwaysOnTop());

  resizer->Drag(gfx::Point(), 0);
  EXPECT_FALSE(split_divider_widget->IsAlwaysOnTop());

  resizer->CompleteDrag();
  EXPECT_TRUE(split_divider_widget->IsAlwaysOnTop());
}

// Test the functionalities that are related to dragging a maximized window's
// tabs. See the expected behaviors described in go/tab-dragging-in-tablet-mode.
TEST_F(SplitViewTabDraggingTest, DragMaximizedWindow) {
  UpdateDisplay("600x600");
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(
      CreateWindowWithType(bounds, AppType::BROWSER));
  std::unique_ptr<aura::Window> window2(
      CreateWindowWithType(bounds, AppType::BROWSER));
  wm::GetWindowState(window1.get())->Maximize();
  wm::GetWindowState(window2.get())->Maximize();
  EXPECT_TRUE(wm::GetWindowState(window1.get())->IsMaximized());
  EXPECT_TRUE(wm::GetWindowState(window2.get())->IsMaximized());

  // 1. If the dragged window is the source window:
  std::unique_ptr<WindowResizer> resizer =
      StartDrag(window1.get(), window1.get());
  ASSERT_TRUE(resizer.get());
  // Overview should have been opened because the dragged window is the source
  // window.
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());

  // 1.a. Drag the window to move a small amount of distance will maximize the
  // window again.
  DragWindowWithOffset(resizer.get(), 10, 10);
  EXPECT_EQ(GetIndicatorState(resizer.get()), IndicatorState::kNone);
  // Drag the window past the indicators threshold to show the indicators.
  DragWindowTo(resizer.get(),
               gfx::Point(200, GetIndicatorsThreshold(window1.get())));
  EXPECT_EQ(GetIndicatorState(resizer.get()), IndicatorState::kDragArea);
  CompleteDrag(std::move(resizer));
  EXPECT_TRUE(wm::GetWindowState(window1.get())->IsMaximized());
  EXPECT_TRUE(wm::GetWindowState(window2.get())->IsMaximized());
  EXPECT_FALSE(Shell::Get()->window_selector_controller()->IsSelecting());

  // 1.b. Drag the window long enough (pass one fourth of the screen vertical
  // height) to snap the window to splitscreen.
  resizer = StartDrag(window1.get(), window1.get());
  ASSERT_TRUE(resizer.get());
  DragWindowTo(resizer.get(), gfx::Point(0, 300));
  EXPECT_EQ(GetIndicatorState(resizer.get()), IndicatorState::kPreviewAreaLeft);
  resizer->CompleteDrag();
  EXPECT_EQ(window1->GetProperty(ash::kTabDroppedWindowStateTypeKey),
            mojom::WindowStateType::LEFT_SNAPPED);
  EXPECT_NE(split_view_controller()->left_window(), window1.get());
  SetIsInTabDragging(window1.get(), false);
  resizer.reset();
  EXPECT_TRUE(split_view_controller()->IsSplitViewModeActive());
  EXPECT_EQ(split_view_controller()->left_window(), window1.get());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::LEFT_SNAPPED);
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());

  // Maximize the snapped window to end split view mode and overview mode.
  wm::GetWindowState(window1.get())->Maximize();
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());
  EXPECT_FALSE(Shell::Get()->window_selector_controller()->IsSelecting());

  // 2. If the dragged window is not the source window:
  resizer = StartDrag(window1.get(), window2.get());
  ASSERT_TRUE(resizer.get());
  // Overview is not opened for this case.
  EXPECT_FALSE(Shell::Get()->window_selector_controller()->IsSelecting());
  // When the drag starts, the source window's bounds are the same with the
  // work area's bounds.
  const gfx::Rect work_area_bounds =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(window1.get())
          .work_area();
  EXPECT_EQ(window2->GetBoundsInScreen(), work_area_bounds);
  EXPECT_TRUE(window1->GetProperty(ash::kCanAttachToAnotherWindowKey));

  // 2.a. Drag the window a small amount of distance and release will maximize
  // the window.
  DragWindowWithOffset(resizer.get(), 10, 10);
  EXPECT_EQ(GetIndicatorState(resizer.get()), IndicatorState::kNone);
  // Drag the window past the indicators threshold to show the indicators.
  DragWindowTo(resizer.get(),
               gfx::Point(200, GetIndicatorsThreshold(window1.get())));
  EXPECT_EQ(GetIndicatorState(resizer.get()), IndicatorState::kDragArea);
  // The source window should also have been scaled.
  EXPECT_NE(window2->GetBoundsInScreen(), work_area_bounds);
  EXPECT_FALSE(window1->GetProperty(ash::kCanAttachToAnotherWindowKey));
  CompleteDrag(std::move(resizer));
  EXPECT_TRUE(wm::GetWindowState(window1.get())->IsMaximized());
  EXPECT_TRUE(wm::GetWindowState(window2.get())->IsMaximized());
  // The source window should have restored its bounds.
  EXPECT_EQ(window2->GetBoundsInScreen(), work_area_bounds);
  EXPECT_TRUE(window1->GetProperty(ash::kCanAttachToAnotherWindowKey));

  // 2.b. Drag the window long enough to snap the window. The source window will
  // snap to the other side of the splitscreen.
  resizer = StartDrag(window1.get(), window2.get());
  ASSERT_TRUE(resizer.get());
  DragWindowTo(resizer.get(), gfx::Point(600, 300));
  EXPECT_EQ(GetIndicatorState(resizer.get()),
            IndicatorState::kPreviewAreaRight);
  // The source window's bounds should be the same as the left snapped window
  // bounds as it's to be snapped to LEFT.
  EXPECT_EQ(window2->GetBoundsInScreen(),
            split_view_controller()->GetSnappedWindowBoundsInScreen(
                window2.get(), SplitViewController::LEFT));
  EXPECT_FALSE(window1->GetProperty(ash::kCanAttachToAnotherWindowKey));
  DragWindowTo(resizer.get(), gfx::Point(0, 300));
  EXPECT_EQ(GetIndicatorState(resizer.get()), IndicatorState::kPreviewAreaLeft);
  // The source window's bounds should be the same as the right snapped window
  // bounds as it's to be snapped to RIGHT.
  EXPECT_EQ(window2->GetBoundsInScreen(),
            split_view_controller()->GetSnappedWindowBoundsInScreen(
                window2.get(), SplitViewController::RIGHT));
  EXPECT_FALSE(window1->GetProperty(ash::kCanAttachToAnotherWindowKey));

  resizer->CompleteDrag();
  EXPECT_EQ(window1->GetProperty(kTabDroppedWindowStateTypeKey),
            mojom::WindowStateType::LEFT_SNAPPED);
  EXPECT_NE(split_view_controller()->left_window(), window1.get());
  SetIsInTabDragging(window1.get(), false);
  resizer.reset();
  EXPECT_TRUE(split_view_controller()->IsSplitViewModeActive());
  EXPECT_EQ(split_view_controller()->left_window(), window1.get());
  EXPECT_EQ(split_view_controller()->right_window(), window2.get());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::BOTH_SNAPPED);
  EXPECT_FALSE(Shell::Get()->window_selector_controller()->IsSelecting());
  EXPECT_TRUE(window1->GetProperty(ash::kCanAttachToAnotherWindowKey));

  EndSplitView();
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());

  // 3. If the dragged window is destroyed during dragging (may happen due to
  // all its tabs are attached into another window), nothing changes.
  resizer = StartDrag(window1.get(), window2.get());
  ASSERT_TRUE(resizer.get());
  resizer->Drag(gfx::Point(0, 300), 0);
  resizer->CompleteDrag();
  resizer.reset();
  window1.reset();
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());
  EXPECT_FALSE(Shell::Get()->window_selector_controller()->IsSelecting());
  EXPECT_TRUE(wm::GetWindowState(window2.get())->IsMaximized());
}

// Test the functionalities that are related to dragging a snapped window in
// splitscreen. There are always two snapped window when the drag starts (i.e.,
// the overview mode is not active). See the expected behaviors described in
// go/tab-dragging-in-tablet-mode.
TEST_F(SplitViewTabDraggingTest, DragSnappedWindow) {
  UpdateDisplay("600x600");
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(
      CreateWindowWithType(bounds, AppType::BROWSER));
  std::unique_ptr<aura::Window> window2(
      CreateWindowWithType(bounds, AppType::BROWSER));
  std::unique_ptr<aura::Window> window3(
      CreateWindowWithType(bounds, AppType::BROWSER));

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::BOTH_SNAPPED);
  EXPECT_EQ(split_view_controller()->left_window(), window1.get());
  EXPECT_EQ(split_view_controller()->right_window(), window2.get());

  // 1. If the dragged window is the source window:
  std::unique_ptr<WindowResizer> resizer =
      StartDrag(window1.get(), window1.get());
  ASSERT_TRUE(resizer.get());
  EXPECT_EQ(GetIndicatorState(resizer.get()), IndicatorState::kNone);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::RIGHT_SNAPPED);
  // In this case overview grid will be opened, containing |window3|.
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());
  WindowSelector* window_selector =
      Shell::Get()->window_selector_controller()->window_selector();
  EXPECT_FALSE(window_selector->IsWindowInOverview(window1.get()));
  EXPECT_FALSE(window_selector->IsWindowInOverview(window2.get()));
  EXPECT_TRUE(window_selector->IsWindowInOverview(window3.get()));

  // 1.a. If the window is only dragged for a small distance, the window will
  // be put back to its original position. Overview mode will be ended.
  DragWindowWithOffset(resizer.get(), 10, 10);
  CompleteDrag(std::move(resizer));
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::BOTH_SNAPPED);
  EXPECT_EQ(split_view_controller()->left_window(), window1.get());
  EXPECT_EQ(split_view_controller()->right_window(), window2.get());
  EXPECT_FALSE(Shell::Get()->window_selector_controller()->IsSelecting());

  // 1.b. If the window is dragged long enough, it can replace the other split
  // window.
  resizer = StartDrag(window1.get(), window1.get());
  ASSERT_TRUE(resizer.get());
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());
  DragWindowTo(resizer.get(), gfx::Point(600, 300));
  // No preview window shows up on overview side of the screen.
  EXPECT_EQ(GetIndicatorState(resizer.get()), IndicatorState::kNone);
  CompleteDrag(std::move(resizer));
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::RIGHT_SNAPPED);
  EXPECT_EQ(split_view_controller()->right_window(), window1.get());
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());
  window_selector =
      Shell::Get()->window_selector_controller()->window_selector();
  EXPECT_FALSE(window_selector->IsWindowInOverview(window1.get()));
  EXPECT_TRUE(window_selector->IsWindowInOverview(window2.get()));
  EXPECT_TRUE(window_selector->IsWindowInOverview(window3.get()));
  // Snap |window2| again to test 1.c.
  split_view_controller()->SnapWindow(window2.get(), SplitViewController::LEFT);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::BOTH_SNAPPED);

  // 1.c. If the dragged window is destroyed during dragging (may happen due to
  // all its tabs are attached into another window), nothing changes.
  resizer = StartDrag(window1.get(), window1.get());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::LEFT_SNAPPED);
  ASSERT_TRUE(resizer.get());
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());
  resizer->Drag(gfx::Point(100, 100), 0);
  resizer->CompleteDrag();
  resizer.reset();
  window1.reset();
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::LEFT_SNAPPED);
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());

  // Recreate |window1| and snap it to test the following senarioes.
  window1.reset(CreateWindowWithType(bounds, AppType::BROWSER));
  split_view_controller()->SnapWindow(window1.get(),
                                      SplitViewController::RIGHT);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::BOTH_SNAPPED);
  EXPECT_EQ(split_view_controller()->left_window(), window2.get());
  EXPECT_EQ(split_view_controller()->right_window(), window1.get());

  // 2. If the dragged window is not the source window:
  // In this case, |window3| can be regarded as a window that originates from
  // |window2|.
  resizer = StartDrag(window3.get(), window2.get());
  ASSERT_TRUE(resizer.get());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::BOTH_SNAPPED);
  EXPECT_EQ(split_view_controller()->left_window(), window2.get());
  EXPECT_EQ(split_view_controller()->right_window(), window1.get());
  EXPECT_FALSE(Shell::Get()->window_selector_controller()->IsSelecting());
  EXPECT_EQ(GetIndicatorState(resizer.get()), IndicatorState::kNone);
  EXPECT_EQ(window2->GetBoundsInScreen(),
            split_view_controller()->GetSnappedWindowBoundsInScreen(
                window2.get(), SplitViewController::LEFT));

  // 2.a. If the window is only dragged for a small amount of distance, it will
  // replace the same side of the split window that it originates from.
  DragWindowWithOffset(resizer.get(), 10, 10);
  EXPECT_EQ(GetIndicatorState(resizer.get()), IndicatorState::kNone);
  // Even though the window is dragged past the indicators threshold, the
  // indicators should not show up for this case.
  DragWindowTo(resizer.get(),
               gfx::Point(200, GetIndicatorsThreshold(window1.get())));
  EXPECT_EQ(GetIndicatorState(resizer.get()), IndicatorState::kNone);
  // The source window's bounds should remain the same.
  EXPECT_EQ(window2->GetBoundsInScreen(),
            split_view_controller()->GetSnappedWindowBoundsInScreen(
                window2.get(), SplitViewController::LEFT));
  CompleteDrag(std::move(resizer));
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::BOTH_SNAPPED);
  EXPECT_EQ(split_view_controller()->left_window(), window3.get());
  EXPECT_EQ(split_view_controller()->right_window(), window1.get());
  EXPECT_FALSE(Shell::Get()->window_selector_controller()->IsSelecting());

  // 2.b. If the window is dragged long enough, it can replace the other side of
  // the split window.
  resizer = StartDrag(window2.get(), window1.get());
  ASSERT_TRUE(resizer.get());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::BOTH_SNAPPED);
  EXPECT_EQ(split_view_controller()->left_window(), window3.get());
  EXPECT_EQ(split_view_controller()->right_window(), window1.get());
  EXPECT_FALSE(Shell::Get()->window_selector_controller()->IsSelecting());
  DragWindowTo(resizer.get(), gfx::Point(0, 300));
  // No preview window shows up on overview side of the screen.
  EXPECT_EQ(GetIndicatorState(resizer.get()), IndicatorState::kNone);
  CompleteDrag(std::move(resizer));
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::BOTH_SNAPPED);
  EXPECT_EQ(split_view_controller()->left_window(), window2.get());
  EXPECT_EQ(split_view_controller()->right_window(), window1.get());
  EXPECT_FALSE(Shell::Get()->window_selector_controller()->IsSelecting());
}

// Test the functionalities that are related to dragging a snapped window while
// overview grid is open on the other side of the screen. See the expected
// behaviors described in go/tab-dragging-in-tablet-mode.
TEST_F(SplitViewTabDraggingTest, DragSnappedWindowWhileOverviewOpen) {
  UpdateDisplay("600x600");
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(
      CreateWindowWithType(bounds, AppType::BROWSER));
  std::unique_ptr<aura::Window> window2(
      CreateWindowWithType(bounds, AppType::BROWSER));
  std::unique_ptr<aura::Window> window3(
      CreateWindowWithType(bounds, AppType::BROWSER));

  // Prepare the testing senario:
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  ToggleOverview();
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::LEFT_SNAPPED);
  EXPECT_EQ(split_view_controller()->left_window(), window1.get());

  // 1. If the dragged window is the source window:
  std::unique_ptr<WindowResizer> resizer =
      StartDrag(window1.get(), window1.get());
  ASSERT_TRUE(resizer.get());
  // Overivew mode is still active, but split view mode is ended due to dragging
  // the only snapped window.
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());

  // 1.a. If the window is only dragged for a small amount of distance
  DragWindowWithOffset(resizer.get(), 10, 10);
  EXPECT_EQ(GetIndicatorState(resizer.get()), IndicatorState::kNone);
  // Drag the window past the indicators threshold should show the indicators.
  DragWindowTo(resizer.get(),
               gfx::Point(200, GetIndicatorsThreshold(window1.get())));
  EXPECT_EQ(GetIndicatorState(resizer.get()), IndicatorState::kDragArea);
  CompleteDrag(std::move(resizer));
  EXPECT_TRUE(wm::GetWindowState(window1.get())->IsMaximized());
  EXPECT_FALSE(Shell::Get()->window_selector_controller()->IsSelecting());
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());

  // 1.b. If the window is dragged long enough, it can be snappped again.
  // Prepare the testing senario first.
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  ToggleOverview();
  resizer = StartDrag(window1.get(), window1.get());
  ASSERT_TRUE(resizer.get());
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());
  DragWindowTo(resizer.get(), gfx::Point(0, 300));
  EXPECT_EQ(GetIndicatorState(resizer.get()), IndicatorState::kPreviewAreaLeft);
  CompleteDrag(std::move(resizer));
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::LEFT_SNAPPED);
  EXPECT_EQ(split_view_controller()->left_window(), window1.get());
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());
  WindowSelector* window_selector =
      Shell::Get()->window_selector_controller()->window_selector();
  EXPECT_TRUE(window_selector->IsWindowInOverview(window2.get()));
  EXPECT_TRUE(window_selector->IsWindowInOverview(window3.get()));

  // 2. If the dragged window is not the source window:
  // Prepare the testing senario first. Remove |window2| from overview first
  // before tab-dragging.
  WindowGrid* current_grid =
      window_selector->GetGridWithRootWindow(window2->GetRootWindow());
  ASSERT_TRUE(current_grid);
  window_selector->RemoveWindowSelectorItem(
      current_grid->GetWindowSelectorItemContaining(window2.get()),
      /*reposition=*/false);
  resizer = StartDrag(window2.get(), window1.get());
  ASSERT_TRUE(resizer.get());
  EXPECT_EQ(GetIndicatorState(resizer.get()), IndicatorState::kNone);
  // Drag a samll amount of distance.
  DragWindowWithOffset(resizer.get(), 10, 10);
  EXPECT_EQ(GetIndicatorState(resizer.get()), IndicatorState::kNone);
  // Drag the window past the indicators threshold should not show the drag
  // indicators as Splitscreen is still active.
  DragWindowTo(resizer.get(),
               gfx::Point(200, GetIndicatorsThreshold(window1.get())));
  EXPECT_EQ(GetIndicatorState(resizer.get()), IndicatorState::kNone);
  // The source window still remains the same bounds.
  EXPECT_EQ(window1->GetBoundsInScreen(),
            split_view_controller()->GetSnappedWindowBoundsInScreen(
                window1.get(), SplitViewController::LEFT));

  // 2.a. The dragged window can replace the only snapped window in the split
  // screen. After that, the old snapped window will be put back in overview.
  DragWindowTo(resizer.get(), gfx::Point(0, 500));
  // No preview window shows up on overview side of the screen.
  EXPECT_EQ(GetIndicatorState(resizer.get()), IndicatorState::kNone);
  CompleteDrag(std::move(resizer));
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::LEFT_SNAPPED);
  EXPECT_EQ(split_view_controller()->left_window(), window2.get());
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());
  window_selector =
      Shell::Get()->window_selector_controller()->window_selector();
  EXPECT_TRUE(window_selector->IsWindowInOverview(window1.get()));
  EXPECT_TRUE(window_selector->IsWindowInOverview(window3.get()));

  // 2.b. The dragged window can snap to the other side of the splitscreen,
  // causing overview mode to end.
  // Remove |window1| from overview first before tab dragging.
  window_selector->RemoveWindowSelectorItem(
      current_grid->GetWindowSelectorItemContaining(window1.get()),
      /*reposition=*/false);
  resizer = StartDrag(window1.get(), window2.get());
  ASSERT_TRUE(resizer.get());
  DragWindowTo(resizer.get(), gfx::Point(600, 500));
  // No preview window shows up on overview side of the screen.
  EXPECT_EQ(GetIndicatorState(resizer.get()), IndicatorState::kNone);
  CompleteDrag(std::move(resizer));
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::BOTH_SNAPPED);
  EXPECT_EQ(split_view_controller()->left_window(), window2.get());
  EXPECT_EQ(split_view_controller()->right_window(), window1.get());
  EXPECT_FALSE(Shell::Get()->window_selector_controller()->IsSelecting());
}

// Test that if a window is in tab-dragging process when overview is open, the
// new window item widget shows up when the drag starts, and is destroyed after
// the drag ends.
TEST_F(SplitViewTabDraggingTest, ShowNewWindowItemWhenDragStarts) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(
      CreateWindowWithType(bounds, AppType::BROWSER));
  std::unique_ptr<aura::Window> window2(
      CreateWindowWithType(bounds, AppType::BROWSER));
  std::unique_ptr<aura::Window> window3(
      CreateWindowWithType(bounds, AppType::BROWSER));

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::BOTH_SNAPPED);
  EXPECT_FALSE(Shell::Get()->window_selector_controller()->IsSelecting());

  // Now drags |window1|.
  std::unique_ptr<WindowResizer> resizer =
      StartDrag(window1.get(), window1.get());
  // Overview should have been opened.
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::RIGHT_SNAPPED);

  // Test that the new window item widget shows up as the first one of the
  // windows in the grid.
  WindowSelector* window_selector =
      Shell::Get()->window_selector_controller()->window_selector();
  WindowGrid* current_grid =
      window_selector->GetGridWithRootWindow(window1->GetRootWindow());
  ASSERT_TRUE(current_grid);
  views::Widget* drop_target_widget =
      current_grid->drop_target_widget_for_testing();
  EXPECT_TRUE(drop_target_widget);

  WindowSelectorItem* drop_target =
      current_grid->GetWindowSelectorItemContaining(
          drop_target_widget->GetNativeWindow());
  ASSERT_TRUE(drop_target);
  EXPECT_EQ(drop_target, current_grid->window_list().front().get());
  const gfx::Rect drop_target_bounds = drop_target->target_bounds();
  DragWindowTo(resizer.get(), drop_target_bounds.CenterPoint());
  CompleteDrag(std::move(resizer));

  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::RIGHT_SNAPPED);
  // Test that the dragged window has been added to the overview mode, and it is
  // added at the front of the grid.
  EXPECT_EQ(current_grid->window_list().size(), 2u);
  WindowSelectorItem* first_selector_item =
      current_grid->GetWindowSelectorItemContaining(window1.get());
  EXPECT_EQ(first_selector_item, current_grid->window_list().front().get());
  EXPECT_TRUE(window_selector->IsWindowInOverview(window1.get()));
  EXPECT_TRUE(window_selector->IsWindowInOverview(window3.get()));
  // Test that the new window item widget has been destroyed.
  EXPECT_FALSE(current_grid->drop_target_widget_for_testing());
}

// Tests that if overview is ended because of releasing the dragged window, we
// should not do animation when exiting overview.
TEST_F(SplitViewTabDraggingTest, OverviewExitAnimationTest) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(
      CreateWindowWithType(bounds, AppType::BROWSER));
  std::unique_ptr<aura::Window> window2(
      CreateWindowWithType(bounds, AppType::BROWSER));
  std::unique_ptr<aura::Window> window3(
      CreateWindowWithType(bounds, AppType::BROWSER));
  std::unique_ptr<OverviewStatesObserver> overview_observer =
      std::make_unique<OverviewStatesObserver>(window1->GetRootWindow());

  // 1) If dragging a maximized window:
  std::unique_ptr<WindowResizer> resizer =
      StartDrag(window1.get(), window1.get());
  ASSERT_TRUE(resizer.get());
  // Overview should have been opened because the dragged window is the source
  // window.
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());
  // The value should be properly initialized.
  EXPECT_TRUE(overview_observer->overview_animate_when_exiting());

  // Now release the dragged window. There should be no animation when exiting
  // overview.
  CompleteDrag(std::move(resizer));
  EXPECT_FALSE(Shell::Get()->window_selector_controller()->IsSelecting());
  EXPECT_FALSE(overview_observer->overview_animate_when_exiting());

  // 2) If dragging a snapped window:
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  overview_observer.reset(new OverviewStatesObserver(window1->GetRootWindow()));
  resizer = StartDrag(window1.get(), window1.get());
  ASSERT_TRUE(resizer.get());
  // Overview should have been opened behind the dragged window.
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());
  // Split view should still be active.
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::RIGHT_SNAPPED);
  // The value should be properly initialized.
  EXPECT_TRUE(overview_observer->overview_animate_when_exiting());

  CompleteDrag(std::move(resizer));
  EXPECT_FALSE(Shell::Get()->window_selector_controller()->IsSelecting());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::BOTH_SNAPPED);
  EXPECT_FALSE(overview_observer->overview_animate_when_exiting());
}

// Tests that there is no top drag indicator if drag a window in portrait screen
// orientation.
TEST_F(SplitViewTabDraggingTest, DragIndicatorsInPortraitOrientationTest) {
  UpdateDisplay("800x600");
  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display::test::ScopedSetInternalDisplayId set_internal(display_manager,
                                                         display_id);
  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());
  ASSERT_EQ(test_api.GetCurrentOrientation(),
            OrientationLockType::kLandscapePrimary);

  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window(
      CreateWindowWithType(bounds, AppType::BROWSER));
  wm::GetWindowState(window.get())->Maximize();
  EXPECT_TRUE(wm::GetWindowState(window.get())->IsMaximized());
  std::unique_ptr<WindowResizer> resizer =
      StartDrag(window.get(), window.get());
  ASSERT_TRUE(resizer.get());
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());
  // Drag the window past the indicators threshold to show the indicators.
  DragWindowTo(resizer.get(),
               gfx::Point(200, GetIndicatorsThreshold(window.get())));
  EXPECT_EQ(GetIndicatorState(resizer.get()), IndicatorState::kDragArea);
  CompleteDrag(std::move(resizer));
  EXPECT_TRUE(wm::GetWindowState(window.get())->IsMaximized());

  // Rotate the screen by 270 degree to portrait primary orientation.
  test_api.SetDisplayRotation(display::Display::ROTATE_270,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            OrientationLockType::kPortraitPrimary);
  resizer = StartDrag(window.get(), window.get());
  ASSERT_TRUE(resizer.get());
  // Drag the window past the indicators threshold to show the indicators.
  DragWindowTo(resizer.get(),
               gfx::Point(200, GetIndicatorsThreshold(window.get())));
  EXPECT_EQ(GetIndicatorState(resizer.get()), IndicatorState::kDragAreaRight);
  CompleteDrag(std::move(resizer));
  EXPECT_TRUE(wm::GetWindowState(window.get())->IsMaximized());
}

// Tests that if dragging a window into the preview split area, overivew bounds
// should be adjusted accordingly.
TEST_F(SplitViewTabDraggingTest, AdjustOverviewBoundsDuringDragging) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(
      CreateWindowWithType(bounds, AppType::BROWSER));
  std::unique_ptr<aura::Window> window2(
      CreateWindowWithType(bounds, AppType::BROWSER));
  std::unique_ptr<aura::Window> window3(
      CreateWindowWithType(bounds, AppType::BROWSER));

  WindowSelectorController* selector_controller =
      Shell::Get()->window_selector_controller();
  EXPECT_FALSE(selector_controller->IsSelecting());

  // Start dragging |window1|.
  std::unique_ptr<WindowResizer> resizer =
      StartDrag(window1.get(), window1.get());
  // Overview should have been opened.
  EXPECT_TRUE(selector_controller->IsSelecting());

  // Test that the drop target shows up as the first item in overview.
  WindowGrid* current_grid =
      selector_controller->window_selector()->GetGridWithRootWindow(
          window1->GetRootWindow());
  EXPECT_TRUE(current_grid->GetDropTarget());
  const gfx::Rect work_area_bounds =
      split_view_controller()->GetDisplayWorkAreaBoundsInScreen(window1.get());
  EXPECT_EQ(current_grid->bounds(), work_area_bounds);
  // The drop target should be visible.
  views::Widget* drop_target_widget =
      current_grid->drop_target_widget_for_testing();
  EXPECT_TRUE(drop_target_widget);
  // Drop target's bounds has been set when added it into overview, which is not
  // equals to the window's bounds.
  EXPECT_EQ(drop_target_widget->GetNativeWindow()->bounds(),
            GetDropTargetBoundsDuringDrag(window1.get()));
  EXPECT_NE(drop_target_widget->GetNativeWindow()->bounds(), window1->bounds());
  EXPECT_TRUE(drop_target_widget->IsVisible());

  // Now drag |window1| to the left preview split area.
  DragWindowTo(resizer.get(),
               gfx::Point(0, work_area_bounds.CenterPoint().y()));
  EXPECT_EQ(current_grid->bounds(),
            split_view_controller()->GetSnappedWindowBoundsInScreen(
                window1.get(), SplitViewController::RIGHT));
  EXPECT_FALSE(drop_target_widget->IsVisible());

  // Drag it to middle.
  DragWindowTo(resizer.get(), work_area_bounds.CenterPoint());
  EXPECT_EQ(current_grid->bounds(), work_area_bounds);
  EXPECT_TRUE(drop_target_widget->IsVisible());

  // Drag |window1| to the right preview split area.
  DragWindowTo(resizer.get(), gfx::Point(work_area_bounds.right(),
                                         work_area_bounds.CenterPoint().y()));
  EXPECT_EQ(current_grid->bounds(),
            split_view_controller()->GetSnappedWindowBoundsInScreen(
                window1.get(), SplitViewController::LEFT));
  EXPECT_FALSE(drop_target_widget->IsVisible());

  CompleteDrag(std::move(resizer));
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::RIGHT_SNAPPED);
  EXPECT_TRUE(selector_controller->IsSelecting());

  // Snap another window should end overview.
  split_view_controller()->SnapWindow(window2.get(), SplitViewController::LEFT);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::BOTH_SNAPPED);
  EXPECT_FALSE(selector_controller->IsSelecting());

  // Now drag |window1| again. Overview and splitview should be both active at
  // the same time during dragging.
  resizer = StartDrag(window1.get(), window1.get());
  EXPECT_TRUE(selector_controller->IsSelecting());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::LEFT_SNAPPED);

  current_grid = selector_controller->window_selector()->GetGridWithRootWindow(
      window1->GetRootWindow());
  // The drop target should be visible.
  drop_target_widget = current_grid->drop_target_widget_for_testing();
  EXPECT_TRUE(drop_target_widget);
  EXPECT_TRUE(drop_target_widget->IsVisible());
  EXPECT_EQ(drop_target_widget->GetNativeWindow()->bounds(),
            GetDropTargetBoundsDuringDrag(window1.get()));
  EXPECT_NE(drop_target_widget->GetNativeWindow()->bounds(), window1->bounds());
  EXPECT_EQ(current_grid->bounds(),
            split_view_controller()->GetSnappedWindowBoundsInScreen(
                window1.get(), SplitViewController::RIGHT));

  // Drag |window1| to the right preview split area.
  DragWindowTo(resizer.get(), gfx::Point(work_area_bounds.right(),
                                         work_area_bounds.CenterPoint().y()));
  // Overview bounds stays the same.
  EXPECT_EQ(current_grid->bounds(),
            split_view_controller()->GetSnappedWindowBoundsInScreen(
                window1.get(), SplitViewController::RIGHT));
  EXPECT_TRUE(drop_target_widget->IsVisible());

  // Drag |window1| to the left preview split area.
  DragWindowTo(resizer.get(),
               gfx::Point(0, work_area_bounds.CenterPoint().y()));
  EXPECT_EQ(current_grid->bounds(),
            split_view_controller()->GetSnappedWindowBoundsInScreen(
                window1.get(), SplitViewController::RIGHT));
  EXPECT_TRUE(drop_target_widget->IsVisible());

  CompleteDrag(std::move(resizer));

  // |window1| should now snap to left. |window2| is put back in overview.
  EXPECT_EQ(split_view_controller()->left_window(), window1.get());
  EXPECT_TRUE(selector_controller->IsSelecting());
  EXPECT_TRUE(selector_controller->window_selector()->IsWindowInOverview(
      window2.get()));

  // Now drag |window1| again.
  resizer = StartDrag(window1.get(), window1.get());
  // Splitview should end now, but overview should still active.
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());
  EXPECT_TRUE(selector_controller->IsSelecting());
  // The size of drop target should still not be the same as the dragged
  // window's size.
  current_grid = selector_controller->window_selector()->GetGridWithRootWindow(
      window1->GetRootWindow());
  drop_target_widget = current_grid->drop_target_widget_for_testing();
  EXPECT_TRUE(drop_target_widget);
  EXPECT_TRUE(drop_target_widget->IsVisible());
  EXPECT_EQ(drop_target_widget->GetNativeWindow()->bounds(),
            GetDropTargetBoundsDuringDrag(window1.get()));
  EXPECT_NE(drop_target_widget->GetNativeWindow()->bounds(), window1->bounds());
  CompleteDrag(std::move(resizer));
}

// Tests that a dragged window's bounds should be updated before dropping onto
// the drop target to add into overview.
TEST_F(SplitViewTabDraggingTest, WindowBoundsUpdatedBeforeAddingToOverview) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(
      CreateWindowWithType(bounds, AppType::BROWSER));
  gfx::Rect tablet_mode_bounds = window1->bounds();
  EXPECT_NE(bounds, tablet_mode_bounds);

  // Drag |window1|. Overview should open behind the dragged window.
  std::unique_ptr<WindowResizer> resizer =
      StartDrag(window1.get(), window1.get());
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());

  // Change the |window1|'s bounds to simulate what might happen in reality.
  window1->SetBounds(bounds);
  EXPECT_EQ(bounds, window1->bounds());

  // Drop |window1| to the drop target in overview.
  WindowSelectorController* window_selector_controller =
      Shell::Get()->window_selector_controller();
  WindowSelector* window_selector =
      window_selector_controller->window_selector();
  WindowGrid* current_grid =
      window_selector->GetGridWithRootWindow(window1->GetRootWindow());
  ASSERT_TRUE(current_grid);
  EXPECT_EQ(1u, current_grid->window_list().size());

  WindowSelectorItem* selector_item = current_grid->GetDropTarget();
  ASSERT_TRUE(selector_item);
  gfx::Rect drop_target_bounds = selector_item->target_bounds();
  DragWindowTo(resizer.get(), drop_target_bounds.CenterPoint());

  CompleteDrag(std::move(resizer));
  // |window1| should have been merged into overview.
  EXPECT_EQ(current_grid->window_list().size(), 1u);
  EXPECT_TRUE(window_selector->IsWindowInOverview(window1.get()));
  // |window1|'s bounds should have been updated to its tablet mode bounds.
  EXPECT_EQ(tablet_mode_bounds, window1->bounds());
  selector_item = current_grid->window_list().front().get();
  // The new window selector item's bounds should be the same during drag and
  // after drag.
  EXPECT_EQ(drop_target_bounds, selector_item->target_bounds());
  ToggleOverview();
  EXPECT_FALSE(window_selector_controller->IsSelecting());

  // Drag |window1|. Overview should open behind the dragged window.
  resizer = StartDrag(window1.get(), window1.get());
  EXPECT_TRUE(window_selector_controller->IsSelecting());

  // Change the |window1|'s bounds to simulate what might happen in reality.
  window1->SetBounds(bounds);
  EXPECT_EQ(bounds, window1->bounds());

  // Drag the window to right bottom outside the drop target, the window's
  // bounds should also be updated before being dropped into overview.
  drop_target_bounds = GetDropTargetBoundsDuringDrag(window1.get());
  DragWindowTo(resizer.get(),
               drop_target_bounds.bottom_right() + gfx::Vector2d(10, 10));
  CompleteDrag(std::move(resizer));
  // |window1| should have been merged into overview.
  EXPECT_TRUE(window_selector_controller->window_selector()->IsWindowInOverview(
      window1.get()));
  // |window1|'s bounds should have been updated to its tablet mode bounds.
  EXPECT_EQ(tablet_mode_bounds, window1->bounds());
}

// Tests that window should be dropped into overview if has been dragged further
// than half of the distance from top of display to the top of drop target.
TEST_F(SplitViewTabDraggingTest, DropWindowIntoOverviewOnDragPositionTest) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> browser_window1(
      CreateWindowWithType(bounds, AppType::BROWSER));
  wm::GetWindowState(browser_window1.get())->Maximize();
  gfx::Rect work_area_bounds =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(browser_window1.get())
          .work_area();
  std::unique_ptr<WindowResizer> resizer =
      StartDrag(browser_window1.get(), browser_window1.get());

  // Restore window back to maximized if it has been dragged less than the
  // distance threshold.
  gfx::Rect drop_target_bounds =
      GetDropTargetBoundsDuringDrag(browser_window1.get());
  DragWindowTo(
      resizer.get(),
      gfx::Point(
          200, work_area_bounds.y() +
                   TabletModeWindowDragDelegate::kDragPositionToOverviewRatio *
                       (drop_target_bounds.y() - work_area_bounds.y()) -
                   10));
  CompleteDrag(std::move(resizer));
  EXPECT_TRUE(wm::GetWindowState(browser_window1.get())->IsMaximized());
  EXPECT_FALSE(Shell::Get()->window_selector_controller()->IsSelecting());

  // Drop window into overview if it has beenn dragged further than the distance
  // threshold.
  resizer = StartDrag(browser_window1.get(), browser_window1.get());
  drop_target_bounds = GetDropTargetBoundsDuringDrag(browser_window1.get());
  DragWindowTo(
      resizer.get(),
      gfx::Point(
          200, work_area_bounds.y() +
                   TabletModeWindowDragDelegate::kDragPositionToOverviewRatio *
                       (drop_target_bounds.y() - work_area_bounds.y()) +
                   10));
  CompleteDrag(std::move(resizer));
  WindowSelector* window_selector =
      Shell::Get()->window_selector_controller()->window_selector();
  EXPECT_TRUE(window_selector->IsWindowInOverview(browser_window1.get()));
  ToggleOverview();

  // Do not consider the drag position if preview area is shown. Window should
  // to be snapped in this case.
  resizer = StartDrag(browser_window1.get(), browser_window1.get());
  drop_target_bounds = GetDropTargetBoundsDuringDrag(browser_window1.get());
  DragWindowTo(resizer.get(), gfx::Point(0, drop_target_bounds.y() + 10));
  EXPECT_EQ(IndicatorState::kPreviewAreaLeft, GetIndicatorState(resizer.get()));
  CompleteDrag(std::move(resizer));
  EXPECT_TRUE(wm::GetWindowState(browser_window1.get())->IsSnapped());
  EXPECT_EQ(SplitViewController::LEFT_SNAPPED,
            split_view_controller()->state());

  // Should not consider the drag position if splitview is active. Window should
  // still back to be snapped.
  std::unique_ptr<aura::Window> browser_window2(
      CreateWindowWithType(bounds, AppType::BROWSER));
  split_view_controller()->SnapWindow(browser_window2.get(),
                                      SplitViewController::RIGHT);
  EXPECT_EQ(SplitViewController::BOTH_SNAPPED,
            split_view_controller()->state());
  resizer = StartDrag(browser_window1.get(), browser_window1.get());
  drop_target_bounds = GetDropTargetBoundsDuringDrag(browser_window1.get());
  DragWindowTo(resizer.get(), gfx::Point(0, drop_target_bounds.y() + 10));
  EXPECT_TRUE(split_view_controller()->IsSplitViewModeActive());
  CompleteDrag(std::move(resizer));
  EXPECT_EQ(SplitViewController::BOTH_SNAPPED,
            split_view_controller()->state());
  EXPECT_FALSE(Shell::Get()->window_selector_controller()->IsSelecting());
  EndSplitView();
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());

  // Restore window back to maximized if it has been dragged less than the
  // distance threshold when dock magnifier is enabled.
  Shell::Get()->docked_magnifier_controller()->SetEnabled(true);
  work_area_bounds = display::Screen::GetScreen()
                         ->GetDisplayNearestWindow(browser_window1.get())
                         .work_area();
  resizer = StartDrag(browser_window1.get(), browser_window1.get());
  drop_target_bounds = GetDropTargetBoundsDuringDrag(browser_window1.get());
  DragWindowTo(
      resizer.get(),
      gfx::Point(
          200, work_area_bounds.y() +
                   TabletModeWindowDragDelegate::kDragPositionToOverviewRatio *
                       (drop_target_bounds.y() - work_area_bounds.y()) -
                   10));
  CompleteDrag(std::move(resizer));
  EXPECT_TRUE(wm::GetWindowState(browser_window1.get())->IsMaximized());
  EXPECT_FALSE(Shell::Get()->window_selector_controller()->IsSelecting());
}

// Tests that a dragged window should have the active window shadow during
// dragging.
TEST_F(SplitViewTabDraggingTest, DraggedWindowShouldHaveActiveWindowShadow) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(
      CreateWindowWithType(bounds, AppType::BROWSER));
  std::unique_ptr<aura::Window> window2(
      CreateWindowWithType(bounds, AppType::BROWSER));
  wm::ActivateWindow(window1.get());
  wm::ActivateWindow(window2.get());
  window1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  window2->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);

  // 1) Start dragging |window2|. |window2| is the source window.
  std::unique_ptr<WindowResizer> resizer =
      StartDrag(window2.get(), window2.get());
  // |window2| should have the active window shadow.
  ::wm::ShadowController* shadow_controller = Shell::Get()->shadow_controller();
  ui::Shadow* shadow = shadow_controller->GetShadowForWindow(window2.get());
  ASSERT_TRUE(shadow);
  EXPECT_EQ(shadow->desired_elevation(), ::wm::kShadowElevationActiveWindow);
  EXPECT_TRUE(shadow_controller->IsShadowVisibleForWindow(window2.get()));

  CompleteDrag(std::move(resizer));
  Shell::Get()->shadow_controller()->UpdateShadowForWindow(window2.get());
  shadow = shadow_controller->GetShadowForWindow(window2.get());
  ASSERT_TRUE(shadow);
  EXPECT_FALSE(shadow_controller->IsShadowVisibleForWindow(window2.get()));

  // 2) Start dragging |window2|, but |window2| is not the source window.
  resizer = StartDrag(window2.get(), window1.get());
  shadow = shadow_controller->GetShadowForWindow(window2.get());
  ASSERT_TRUE(shadow);
  EXPECT_EQ(shadow->desired_elevation(), ::wm::kShadowElevationActiveWindow);
  EXPECT_TRUE(shadow_controller->IsShadowVisibleForWindow(window2.get()));

  CompleteDrag(std::move(resizer));
  Shell::Get()->shadow_controller()->UpdateShadowForWindow(window2.get());
  shadow = shadow_controller->GetShadowForWindow(window2.get());
  ASSERT_TRUE(shadow);
  EXPECT_FALSE(shadow_controller->IsShadowVisibleForWindow(window2.get()));
}

// Test that if the source window needs to be scaled up/down because of dragging
// a tab window out of it, other windows' visibilities and the home launcher's
// visibility should change accordingly.
TEST_F(SplitViewTabDraggingTest, SourceWindowBackgroundTest) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(
      CreateWindowWithType(bounds, AppType::BROWSER));
  std::unique_ptr<aura::Window> window2(
      CreateWindowWithType(bounds, AppType::BROWSER));
  std::unique_ptr<aura::Window> window3(
      CreateWindowWithType(bounds, AppType::BROWSER));
  std::unique_ptr<aura::Window> window4(
      CreateWindowWithType(bounds, AppType::BROWSER));
  EXPECT_TRUE(window1->IsVisible());
  EXPECT_TRUE(window2->IsVisible());
  EXPECT_TRUE(window3->IsVisible());
  EXPECT_TRUE(window4->IsVisible());

  if (Shell::Get()->app_list_controller()->IsHomeLauncherEnabledInTabletMode())
    EXPECT_TRUE(Shell::Get()->app_list_controller()->IsVisible());

  // 1) Start dragging |window1|. |window2| is the source window.
  std::unique_ptr<WindowResizer> resizer =
      StartDrag(window1.get(), window2.get());
  DragWindowWithOffset(resizer.get(), 10, 10);

  // Test that |window3| should be hidden now. |window1| and |window2| should
  // stay visible during dragging.
  EXPECT_TRUE(window1->IsVisible());
  EXPECT_TRUE(window2->IsVisible());
  EXPECT_FALSE(window3->IsVisible());
  EXPECT_FALSE(window4->IsVisible());

  // Test that home launcher should be dismissed.
  if (Shell::Get()->app_list_controller()->IsHomeLauncherEnabledInTabletMode())
    EXPECT_FALSE(Shell::Get()->app_list_controller()->IsVisible());

  // Test that during dragging, we could not show a hidden window.
  window3->Show();
  EXPECT_FALSE(window3->IsVisible());

  // After dragging, the windows' visibilities should have restored.
  CompleteDrag(std::move(resizer));
  EXPECT_TRUE(window1->IsVisible());
  EXPECT_TRUE(window2->IsVisible());
  EXPECT_TRUE(window3->IsVisible());
  EXPECT_TRUE(window4->IsVisible());

  // Test that home launcher should be reshown.
  if (Shell::Get()->app_list_controller()->IsHomeLauncherEnabledInTabletMode())
    EXPECT_TRUE(Shell::Get()->app_list_controller()->IsVisible());
}

// Tests that the dragged window should be the active and top window if overview
// ended because of window drag.
TEST_F(SplitViewTabDraggingTest, OverviewEndedOnWindowDrag) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(
      CreateWindowWithType(bounds, AppType::BROWSER));
  std::unique_ptr<aura::Window> window2(
      CreateWindowWithType(bounds, AppType::BROWSER));
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);

  // Drags |window2| to overview.
  std::unique_ptr<WindowResizer> resizer =
      StartDrag(window2.get(), window2.get());
  gfx::Rect drop_target_bounds = GetDropTargetBoundsDuringDrag(window1.get());
  DragWindowTo(resizer.get(), drop_target_bounds.CenterPoint());
  CompleteDrag(std::move(resizer));
  WindowSelectorController* selector_controller =
      Shell::Get()->window_selector_controller();
  EXPECT_TRUE(selector_controller->IsSelecting());
  EXPECT_TRUE(selector_controller->window_selector()->IsWindowInOverview(
      window2.get()));
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::LEFT_SNAPPED);

  // Drags |window1| by a small distance. Both splitview and overview should be
  // ended and |window1| is the active window and above |window2|.
  resizer = StartDrag(window1.get(), window1.get());
  DragWindowTo(resizer.get(), gfx::Point(10, 10));
  CompleteDrag(std::move(resizer));
  EXPECT_FALSE(selector_controller->IsSelecting());
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());
  EXPECT_TRUE(wm::GetWindowState(window1.get())->IsMaximized());
  EXPECT_TRUE(wm::GetWindowState(window2.get())->IsMaximized());
  EXPECT_TRUE(wm::GetWindowState(window1.get())->IsActive());
  EXPECT_FALSE(wm::GetWindowState(window2.get())->IsActive());
  // |window1| should above |window2|.
  const aura::Window::Windows windows = window1->parent()->children();
  auto window1_layer = std::find(windows.begin(), windows.end(), window1.get());
  auto window2_layer = std::find(windows.begin(), windows.end(), window2.get());
  EXPECT_TRUE(window1_layer > window2_layer);
}

// When tab dragging a window, the dragged window might need to merge back into
// the source window when the drag ends. Tests the related functionalities.
TEST_F(SplitViewTabDraggingTest, MergeBackToSourceWindow) {
  UpdateDisplay("600x600");
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> dragged_window(
      CreateWindowWithType(bounds, AppType::BROWSER));
  std::unique_ptr<aura::Window> source_window(
      CreateWindowWithType(bounds, AppType::BROWSER));

  // 1. If splitview is not active and the dragged window is not the source
  // window.
  // a. Drag the window to less than half of the display height, and not in the
  // snap preview area.
  std::unique_ptr<WindowResizer> resizer =
      StartDrag(dragged_window.get(), source_window.get());
  ASSERT_TRUE(resizer.get());
  EXPECT_FALSE(
      source_window->GetProperty(ash::kIsDeferredTabDraggingTargetWindowKey));
  DragWindowTo(resizer.get(), gfx::Point(300, 200));
  CompleteDrag(std::move(resizer));
  EXPECT_TRUE(
      source_window->GetProperty(ash::kIsDeferredTabDraggingTargetWindowKey));
  source_window->ClearProperty(ash::kIsDeferredTabDraggingTargetWindowKey);

  // b. Drag the window to more than half of the display height and not in the
  // snap preview area.
  resizer = StartDrag(dragged_window.get(), source_window.get());
  ASSERT_TRUE(resizer.get());
  EXPECT_FALSE(
      source_window->GetProperty(ash::kIsDeferredTabDraggingTargetWindowKey));
  DragWindowTo(resizer.get(), gfx::Point(300, 500));
  CompleteDrag(std::move(resizer));
  EXPECT_FALSE(
      source_window->GetProperty(ash::kIsDeferredTabDraggingTargetWindowKey));

  // c. Drag the window to the snap preview area.
  resizer = StartDrag(dragged_window.get(), source_window.get());
  ASSERT_TRUE(resizer.get());
  EXPECT_FALSE(
      source_window->GetProperty(ash::kIsDeferredTabDraggingTargetWindowKey));
  DragWindowTo(resizer.get(), gfx::Point(0, 200));
  CompleteDrag(std::move(resizer));
  EXPECT_FALSE(
      source_window->GetProperty(ash::kIsDeferredTabDraggingTargetWindowKey));
  EndSplitView();

  // d. The dragged window is already added into overview before drag ends:
  resizer = StartDrag(dragged_window.get(), source_window.get());
  ASSERT_TRUE(resizer.get());
  EXPECT_FALSE(
      source_window->GetProperty(ash::kIsDeferredTabDraggingTargetWindowKey));
  DragWindowTo(resizer.get(), gfx::Point(300, 200));
  dragged_window->SetProperty(ash::kIsShowingInOverviewKey, true);
  CompleteDrag(std::move(resizer));
  EXPECT_FALSE(
      source_window->GetProperty(ash::kIsDeferredTabDraggingTargetWindowKey));
  dragged_window->ClearProperty(ash::kIsShowingInOverviewKey);

  // 2. If splitview is active and the dragged window is not the source window.
  // a. Drag the window to less than half of the display height, in the same
  // split of the source window, and not in the snap preview area.
  split_view_controller()->SnapWindow(source_window.get(),
                                      SplitViewController::LEFT);
  resizer = StartDrag(dragged_window.get(), source_window.get());
  EXPECT_FALSE(
      source_window->GetProperty(ash::kIsDeferredTabDraggingTargetWindowKey));
  DragWindowTo(resizer.get(), gfx::Point(0, 200));
  CompleteDrag(std::move(resizer));
  EXPECT_TRUE(
      source_window->GetProperty(ash::kIsDeferredTabDraggingTargetWindowKey));
  EndSplitView();
  source_window->ClearProperty(ash::kIsDeferredTabDraggingTargetWindowKey);

  // b. Drag the window to less than half of the display height, in the
  // different split of the source window, and not in the snap preview area.
  split_view_controller()->SnapWindow(source_window.get(),
                                      SplitViewController::LEFT);
  resizer = StartDrag(dragged_window.get(), source_window.get());
  EXPECT_FALSE(
      source_window->GetProperty(ash::kIsDeferredTabDraggingTargetWindowKey));
  DragWindowTo(resizer.get(), gfx::Point(500, 200));
  CompleteDrag(std::move(resizer));
  EXPECT_FALSE(
      source_window->GetProperty(ash::kIsDeferredTabDraggingTargetWindowKey));
  EndSplitView();

  // c. Drag the window to move a small distance, but is still in the different
  // split of the source window, and not in the snap preview area.
  split_view_controller()->SnapWindow(source_window.get(),
                                      SplitViewController::LEFT);
  resizer = StartDrag(dragged_window.get(), source_window.get());
  EXPECT_FALSE(
      source_window->GetProperty(ash::kIsDeferredTabDraggingTargetWindowKey));
  DragWindowTo(resizer.get(), gfx::Point(500, 20));
  CompleteDrag(std::move(resizer));
  EXPECT_FALSE(
      source_window->GetProperty(ash::kIsDeferredTabDraggingTargetWindowKey));
  EndSplitView();

  // d. The dragged window was added to overview before the drag ends.
  split_view_controller()->SnapWindow(source_window.get(),
                                      SplitViewController::LEFT);
  resizer = StartDrag(dragged_window.get(), source_window.get());
  EXPECT_FALSE(
      source_window->GetProperty(ash::kIsDeferredTabDraggingTargetWindowKey));
  DragWindowTo(resizer.get(), gfx::Point(0, 200));
  dragged_window->SetProperty(ash::kIsShowingInOverviewKey, true);
  CompleteDrag(std::move(resizer));
  EXPECT_FALSE(
      source_window->GetProperty(ash::kIsDeferredTabDraggingTargetWindowKey));
  dragged_window->ClearProperty(ash::kIsShowingInOverviewKey);
}

// Tests that if window being dragged into drop target when preview area is
// shown, window should go to be snapped instead of being dropped into overview.
TEST_F(SplitViewTabDraggingTest, DragWindowIntoPreviewAreaAndDropTarget) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> browser_window(
      CreateWindowWithType(bounds, AppType::BROWSER));
  wm::GetWindowState(browser_window.get())->Maximize();

  std::unique_ptr<WindowResizer> resizer =
      StartDrag(browser_window.get(), browser_window.get());
  gfx::Rect drop_target_bounds =
      GetDropTargetBoundsDuringDrag(browser_window.get());
  // Drag window to inside the drop target.
  DragWindowTo(resizer.get(), gfx::Point(drop_target_bounds.x() + 5,
                                         drop_target_bounds.y() + 5));
  EXPECT_EQ(GetIndicatorState(resizer.get()), IndicatorState::kPreviewAreaLeft);
  CompleteDrag(std::move(resizer));
  EXPECT_EQ(SplitViewController::LEFT_SNAPPED,
            split_view_controller()->state());
}

// Tests that if a fling event happens on a tab, the tab might or might not
// merge back into the source window depending on the fling event velocity.
TEST_F(SplitViewTabDraggingTest, FlingTest) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> dragged_window(
      CreateWindowWithType(bounds, AppType::BROWSER));
  std::unique_ptr<aura::Window> source_window(
      CreateWindowWithType(bounds, AppType::BROWSER));

  std::unique_ptr<WindowResizer> resizer =
      StartDrag(dragged_window.get(), source_window.get());
  ASSERT_TRUE(resizer.get());
  Fling(std::move(resizer), /*velocity_y=*/3000.f);
  EXPECT_FALSE(
      source_window->GetProperty(ash::kIsDeferredTabDraggingTargetWindowKey));

  resizer = StartDrag(dragged_window.get(), source_window.get());
  ASSERT_TRUE(resizer.get());
  Fling(std::move(resizer), /*velocity_y=*/1000.f);
  EXPECT_TRUE(
      source_window->GetProperty(ash::kIsDeferredTabDraggingTargetWindowKey));
}

// Tests that in various cases, after the tab drag ends, the dragged window and
// the source window should have correct bounds.
TEST_F(SplitViewTabDraggingTest, BoundsTest) {
  UpdateDisplay("600x600");
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(
      CreateWindowWithType(bounds, AppType::BROWSER));
  std::unique_ptr<aura::Window> window2(
      CreateWindowWithType(bounds, AppType::BROWSER));
  std::unique_ptr<aura::Window> window3(
      CreateWindowWithType(bounds, AppType::BROWSER));
  const gfx::Rect bounds1 = window1->bounds();
  const gfx::Rect bounds2 = window2->bounds();
  EXPECT_EQ(bounds1, bounds2);

  // 1. If splitview is not active and the dragged window is the source window.
  std::unique_ptr<WindowResizer> resizer =
      StartDrag(window1.get(), window1.get());
  // Drag for a small distance.
  DragWindowWithOffset(resizer.get(), 10, 10);
  EXPECT_NE(window1->bounds(), bounds1);
  CompleteDrag(std::move(resizer));
  // The window should be maximized again and the bounds should restore to its
  // maximized window size.
  EXPECT_TRUE(wm::GetWindowState(window1.get())->IsMaximized());
  EXPECT_EQ(window1->bounds(), bounds1);

  // 2. If splitview is not active and the dragged window is not the source
  // window.
  resizer = StartDrag(window1.get(), window2.get());
  // a). Drag for a small distance.
  DragWindowWithOffset(resizer.get(), 10, 10);
  EXPECT_NE(window1->bounds(), bounds1);
  EXPECT_EQ(window2->bounds(), bounds2);
  // Now drag for a longer distance so that the source window scales down.
  DragWindowTo(resizer.get(), gfx::Point(300, 200));
  EXPECT_NE(window2->bounds(), bounds2);
  CompleteDrag(std::move(resizer));
  // As in this case the dragged window should merge back to source window,
  // which we can't test here. We only test the source window's bounds restore
  // to its maximized window size.
  EXPECT_TRUE(window2->GetProperty(ash::kIsDeferredTabDraggingTargetWindowKey));
  EXPECT_EQ(window2->bounds(), bounds2);
  window2->ClearProperty(ash::kIsDeferredTabDraggingTargetWindowKey);

  // b) Drag the window far enough so that the dragged window doesn't merge back
  // into the source window.
  resizer = StartDrag(window1.get(), window2.get());
  DragWindowTo(resizer.get(), gfx::Point(300, 400));
  EXPECT_NE(window1->bounds(), bounds1);
  EXPECT_NE(window2->bounds(), bounds2);
  CompleteDrag(std::move(resizer));
  EXPECT_FALSE(
      window2->GetProperty(ash::kIsDeferredTabDraggingTargetWindowKey));
  EXPECT_EQ(window1->bounds(), bounds1);
  EXPECT_EQ(window2->bounds(), bounds2);

  // 3. If splitview is active and the dragged window is the source window.
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  const gfx::Rect snapped_bounds1 = window1->bounds();
  const gfx::Rect snapped_bounds2 = window2->bounds();
  resizer = StartDrag(window1.get(), window1.get());
  // Drag the window for a small distance and release.
  DragWindowWithOffset(resizer.get(), 10, 10);
  EXPECT_NE(window1->bounds(), snapped_bounds1);
  EXPECT_EQ(window2->bounds(), snapped_bounds2);
  CompleteDrag(std::move(resizer));
  EXPECT_EQ(window1->bounds(), snapped_bounds1);
  EXPECT_EQ(window2->bounds(), snapped_bounds2);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::BOTH_SNAPPED);

  // 4. If splitview is active and the dragged window is not the source window.
  resizer = StartDrag(window3.get(), window1.get());
  // a). Drag the window for a small distance and release.
  DragWindowWithOffset(resizer.get(), 10, 10);
  EXPECT_EQ(window1->bounds(), snapped_bounds1);
  EXPECT_EQ(window2->bounds(), snapped_bounds2);
  // Drag the window for a long distance (but is still in merge-back distance
  // range), the source window should not scale down.
  DragWindowTo(resizer.get(), gfx::Point(100, 200));
  EXPECT_EQ(window1->bounds(), snapped_bounds1);
  EXPECT_EQ(window2->bounds(), snapped_bounds2);
  CompleteDrag(std::move(resizer));
  // In this case |window3| is supposed to merge back its source window
  // |window1|, so we only test the source window's bounds here.
  EXPECT_EQ(window1->bounds(), snapped_bounds1);
  EXPECT_EQ(window2->bounds(), snapped_bounds2);
  EXPECT_TRUE(window1->GetProperty(ash::kIsDeferredTabDraggingTargetWindowKey));
  window1->ClearProperty(ash::kIsDeferredTabDraggingTargetWindowKey);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::BOTH_SNAPPED);

  // b). Drag the window far enough so that the dragged window doesn't merge
  // back into its source window.
  resizer = StartDrag(window3.get(), window1.get());
  DragWindowTo(resizer.get(), gfx::Point(100, 400));
  EXPECT_EQ(window1->bounds(), snapped_bounds1);
  EXPECT_EQ(window2->bounds(), snapped_bounds2);
  CompleteDrag(std::move(resizer));
  EXPECT_FALSE(
      window1->GetProperty(ash::kIsDeferredTabDraggingTargetWindowKey));
  // |window3| replaced |window1| as the left snapped window.
  EXPECT_EQ(window3->bounds(), snapped_bounds1);
  EXPECT_EQ(window2->bounds(), snapped_bounds2);
}

class TestWindowDelegateWithWidget : public views::WidgetDelegate {
 public:
  TestWindowDelegateWithWidget(bool can_activate)
      : can_activate_(can_activate) {}
  ~TestWindowDelegateWithWidget() override = default;

  // views::WidgetDelegate:
  void DeleteDelegate() override { delete this; }
  views::Widget* GetWidget() override { return widget_; }
  const views::Widget* GetWidget() const override { return widget_; }
  bool CanActivate() const override { return can_activate_; }
  bool CanResize() const override { return true; }
  bool CanMaximize() const override { return true; }
  bool ShouldAdvanceFocusToTopLevelWidget() const override { return true; }

  void set_widget(views::Widget* widget) { widget_ = widget; }

 private:
  bool can_activate_ = false;
  views::Widget* widget_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TestWindowDelegateWithWidget);
};

class SplitViewAppDraggingTest : public SplitViewControllerTest {
 public:
  SplitViewAppDraggingTest() = default;
  ~SplitViewAppDraggingTest() override = default;

  // SplitViewControllerTest:
  void SetUp() override {
    SplitViewControllerTest::SetUp();
    controller_ = std::make_unique<TabletModeAppWindowDragController>();
  }

  // SplitViewControllerTest:
  void TearDown() override {
    controller_.reset();
    SplitViewControllerTest::TearDown();
  }

 protected:
  std::unique_ptr<aura::Window> CreateTestWindowWithWidget(
      bool can_activate = true) {
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
    params.show_state = ui::SHOW_STATE_MAXIMIZED;
    views::Widget* widget = new views::Widget;
    std::unique_ptr<TestWindowDelegateWithWidget> widget_delegate =
        std::make_unique<TestWindowDelegateWithWidget>(can_activate);
    widget_delegate->set_widget(widget);
    params.delegate = widget_delegate.release();
    params.context = CurrentContext();
    widget->Init(params);
    widget->Show();
    return base::WrapUnique<aura::Window>(widget->GetNativeView());
  }

  // Sends a gesture scroll sequence to TabletModeAppWindowDragController.
  void SendGestureEvents(const gfx::Point& start,
                         float scroll_delta,
                         aura::Window* window) {
    base::TimeTicks timestamp = base::TimeTicks::Now();
    SendScrollStartAndUpdate(start, scroll_delta, timestamp, window);

    EndScrollSequence(start, scroll_delta, timestamp, window);
  }

  void SendScrollStartAndUpdate(const gfx::Point& start,
                                float scroll_y,
                                base::TimeTicks& timestamp,
                                aura::Window* window,
                                float scroll_x = 0.f) {
    SendGestureEventToController(
        start.x(), start.y(), timestamp,
        ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_BEGIN, 0, 0), window);

    timestamp += base::TimeDelta::FromMilliseconds(100);
    SendGestureEventToController(
        start.x() + scroll_x, start.y() + scroll_y, timestamp,
        ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_UPDATE, scroll_x,
                                scroll_y),
        window);
  }

  void EndScrollSequence(const gfx::Point& start,
                         float scroll_delta,
                         base::TimeTicks& timestamp,
                         aura::Window* window,
                         bool is_fling = false,
                         float velocity_y = 0.f,
                         float velocity_x = 0.f) {
    timestamp += base::TimeDelta::FromMilliseconds(100);
    ui::GestureEventDetails details =
        is_fling ? ui::GestureEventDetails(ui::ET_SCROLL_FLING_START,
                                           velocity_x, velocity_y)
                 : ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_END);
    SendGestureEventToController(start.x(), start.y() + scroll_delta, timestamp,
                                 details, window);
  }

  IndicatorState GetIndicatorState() {
    return controller_->drag_delegate_for_testing()
        ->split_view_drag_indicators_for_testing()
        ->current_indicator_state();
  }

 private:
  void SendGestureEventToController(int x,
                                    int y,
                                    base::TimeTicks& timestamp,
                                    const ui::GestureEventDetails& details,
                                    aura::Window* window) {
    ui::GestureEvent event =
        ui::GestureEvent(x, y, ui::EF_NONE, timestamp, details);
    ui::Event::DispatcherApi(&event).set_target(window);
    controller_->DragWindowFromTop(&event);
  }

  std::unique_ptr<TabletModeAppWindowDragController> controller_;

  DISALLOW_COPY_AND_ASSIGN(SplitViewAppDraggingTest);
};

// Tests that drag the window that cannot be snapped from top of the display
// will not snap the window into splitscreen.
TEST_F(SplitViewAppDraggingTest, DragNoneActiveMaximizedWindow) {
  UpdateDisplay("800x600");
  std::unique_ptr<aura::Window> window = CreateTestWindowWithWidget(false);
  EXPECT_TRUE(wm::GetWindowState(window.get())->IsMaximized());
  gfx::Rect display_bounds =
      split_view_controller()->GetDisplayWorkAreaBoundsInScreen(window.get());
  const float long_scroll_delta = display_bounds.height() / 4 + 5;

  const gfx::Point start;
  // Drag the window that cannot be snapped long enough, the window will be
  // dropped into overview.
  base::TimeTicks timestamp = base::TimeTicks::Now();
  SendScrollStartAndUpdate(start, long_scroll_delta, timestamp, window.get());
  WindowSelectorController* window_selector_controller =
      Shell::Get()->window_selector_controller();
  EXPECT_TRUE(window_selector_controller->IsSelecting());
  EXPECT_FALSE(
      window_selector_controller->window_selector()->IsWindowInOverview(
          window.get()));
  EndScrollSequence(start, long_scroll_delta, timestamp, window.get());
  EXPECT_TRUE(window_selector_controller->IsSelecting());
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());
  EXPECT_TRUE(window_selector_controller->window_selector()->IsWindowInOverview(
      window.get()));
}

// Tests the functionalities that are related to dragging a maximized window
// into splitscreen.
TEST_F(SplitViewAppDraggingTest, DragActiveMaximizedWindow) {
  UpdateDisplay("800x600");
  std::unique_ptr<aura::Window> window = CreateTestWindowWithWidget();
  EXPECT_TRUE(wm::GetWindowState(window.get())->IsMaximized());
  gfx::Rect display_bounds =
      split_view_controller()->GetDisplayWorkAreaBoundsInScreen(window.get());

  // Move the window by a small amount of distance will maximize the window
  // again.
  const gfx::Point start;
  SendGestureEvents(start, 10, window.get());
  EXPECT_TRUE(wm::GetWindowState(window.get())->IsMaximized());

  // Drag the window long enough (pass one fourth of the screen vertical
  // height) to snap the window to splitscreen.
  const float long_scroll_delta = display_bounds.height() / 4 + 5;
  base::TimeTicks timestamp = base::TimeTicks::Now();
  SendScrollStartAndUpdate(start, long_scroll_delta, timestamp, window.get());
  WindowSelectorController* window_selector_controller =
      Shell::Get()->window_selector_controller();
  EXPECT_TRUE(window_selector_controller->IsSelecting());
  EXPECT_FALSE(
      window_selector_controller->window_selector()->IsWindowInOverview(
          window.get()));
  EndScrollSequence(start, long_scroll_delta, timestamp, window.get());
  EXPECT_TRUE(window_selector_controller->IsSelecting());
  EXPECT_TRUE(split_view_controller()->IsSplitViewModeActive());
  EXPECT_EQ(split_view_controller()->left_window(), window.get());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::LEFT_SNAPPED);
  EXPECT_TRUE(wm::GetWindowState(window.get())->IsSnapped());

  // FLING the window with small velocity (smaller than
  // kFlingToOverviewThreshold) will not able to drop the window into overview.
  timestamp = base::TimeTicks::Now();
  SendScrollStartAndUpdate(start, 10, timestamp, window.get());
  window_selector_controller = Shell::Get()->window_selector_controller();
  EXPECT_TRUE(window_selector_controller->IsSelecting());
  EndScrollSequence(
      start, 10, timestamp, window.get(), /*is_fling=*/true,
      /*velocity_y=*/
      TabletModeWindowDragDelegate::kFlingToOverviewThreshold - 10.f);
  EXPECT_FALSE(window_selector_controller->IsSelecting());

  // FLING the window with large veloicty (larger than
  // kFlingToOverviewThreshold) will drop the window into overview.
  timestamp = base::TimeTicks::Now();
  SendScrollStartAndUpdate(start, 10, timestamp, window.get());
  window_selector_controller = Shell::Get()->window_selector_controller();
  EXPECT_TRUE(window_selector_controller->IsSelecting());
  EndScrollSequence(
      start, 10, timestamp, window.get(), /*is_fling=*/true,
      /*velocity_y=*/
      TabletModeWindowDragDelegate::kFlingToOverviewThreshold + 10.f);
  EXPECT_TRUE(window_selector_controller->IsSelecting());
}

// Tests the shelf visibility when a fullscreened window is being dragged.
TEST_F(SplitViewAppDraggingTest, ShelfVisibilityIfDraggingFullscreenedWindow) {
  UpdateDisplay("800x600");
  std::unique_ptr<aura::Window> window = CreateTestWindowWithWidget();
  ShelfLayoutManager* shelf_layout_manager =
      AshTestBase::GetPrimaryShelf()->shelf_layout_manager();
  gfx::Rect display_bounds =
      split_view_controller()->GetDisplayWorkAreaBoundsInScreen(window.get());

  // Shelf will be auto-hidden if the winodw requests to be fullscreened.
  wm::WindowState* window_state = wm::GetWindowState(window.get());
  const wm::WMEvent fullscreen_event(wm::WM_EVENT_TOGGLE_FULLSCREEN);
  window_state->OnWMEvent(&fullscreen_event);
  window_state->SetHideShelfWhenFullscreen(false);
  window_state->SetInImmersiveFullscreen(true);
  shelf_layout_manager->UpdateVisibilityState();
  EXPECT_TRUE(window_state->IsFullscreen());
  EXPECT_FALSE(shelf_layout_manager->IsVisible());

  // Drag the window by a small amount of distance, the window will back to
  // fullscreened, and shelf will be hidden again.
  const gfx::Point start;
  SendGestureEvents(start, 10, window.get());
  EXPECT_TRUE(wm::GetWindowState(window.get())->IsFullscreen());
  EXPECT_FALSE(shelf_layout_manager->IsVisible());

  // Shelf is visible during dragging.
  base::TimeTicks timestamp = base::TimeTicks::Now();
  const float long_scroll_delta = display_bounds.height() / 4 + 5;
  SendScrollStartAndUpdate(start, long_scroll_delta, timestamp, window.get());
  EXPECT_TRUE(shelf_layout_manager->IsVisible());
  EndScrollSequence(start, long_scroll_delta, timestamp, window.get());
  EXPECT_TRUE(split_view_controller()->IsSplitViewModeActive());
  EXPECT_TRUE(wm::GetWindowState(window.get())->IsSnapped());
  EXPECT_TRUE(shelf_layout_manager->IsVisible());
}

// Tests the auto-hide shelf state during window dragging.
TEST_F(SplitViewAppDraggingTest, AutoHideShelf) {
  UpdateDisplay("800x600");
  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);
  std::unique_ptr<aura::Window> window = CreateTestWindowWithWidget();
  gfx::Rect display_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  ShelfLayoutManager* shelf_layout_manager =
      AshTestBase::GetPrimaryShelf()->shelf_layout_manager();
  shelf_layout_manager->LayoutShelf();
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
  EXPECT_TRUE(wm::GetWindowState(window.get())->IsMaximized());

  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  shelf_layout_manager->LayoutShelf();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  base::TimeTicks timestamp = base::TimeTicks::Now();
  const gfx::Point start;
  const float long_scroll_delta = display_bounds.height() / 4 + 5;
  SendScrollStartAndUpdate(start, long_scroll_delta, timestamp, window.get());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  // Shelf should be shown during drag.
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EndScrollSequence(start, long_scroll_delta, timestamp, window.get());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::LEFT_SNAPPED);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  // Shelf should be shown after drag and snapped window should be covered by
  // the auto-hide-shown shelf.
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_EQ(split_view_controller()
                ->GetSnappedWindowBoundsInScreen(window.get(),
                                                 SplitViewController::LEFT)
                .height(),
            display_bounds.height());
}

// Tests that the app drag will be reverted if the screen is being rotated.
TEST_F(SplitViewAppDraggingTest, DisplayConfigurationChangeTest) {
  UpdateDisplay("800x600");
  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display::test::ScopedSetInternalDisplayId set_internal(display_manager,
                                                         display_id);
  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());
  // Set the screen orientation to LANDSCAPE_PRIMARY.
  test_api.SetDisplayRotation(display::Display::ROTATE_0,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            OrientationLockType::kLandscapePrimary);

  std::unique_ptr<aura::Window> window = CreateTestWindowWithWidget();
  EXPECT_TRUE(wm::GetWindowState(window.get())->IsMaximized());

  // Drag the window a small distance that will not drop the window into
  // overview.
  base::TimeTicks timestamp = base::TimeTicks::Now();
  SendScrollStartAndUpdate(gfx::Point(0, 0), 10, timestamp, window.get());
  WindowSelectorController* window_selector_controller =
      Shell::Get()->window_selector_controller();
  EXPECT_TRUE(window_selector_controller->IsSelecting());
  EXPECT_TRUE(wm::GetWindowState(window.get())->is_dragged());

  // Rotate the screen during drag.
  test_api.SetDisplayRotation(display::Display::ROTATE_270,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            OrientationLockType::kPortraitPrimary);
  EXPECT_TRUE(wm::GetWindowState(window.get())->IsMaximized());
  EXPECT_FALSE(window_selector_controller->IsSelecting());
  EXPECT_FALSE(wm::GetWindowState(window.get())->is_dragged());
}

// Tests the functionalities that fling the window when preview area is shown.
TEST_F(SplitViewAppDraggingTest, FlingWhenPreviewAreaIsShown) {
  std::unique_ptr<aura::Window> window = CreateTestWindowWithWidget();
  EXPECT_TRUE(wm::GetWindowState(window.get())->IsMaximized());
  gfx::Rect display_bounds =
      split_view_controller()->GetDisplayWorkAreaBoundsInScreen(window.get());

  const float long_scroll_delta = display_bounds.height() / 4 + 5;
  float large_velocity =
      TabletModeWindowDragDelegate::kFlingToOverviewFromSnappingAreaThreshold +
      10.f;
  float small_velocity =
      TabletModeWindowDragDelegate::kFlingToOverviewFromSnappingAreaThreshold -
      10.f;
  gfx::Point start;
  base::TimeTicks timestamp = base::TimeTicks::Now();

  // Fling to the right with large enough velocity when trying to snap the
  // window to the left should drop the window to overview.
  SendScrollStartAndUpdate(start, long_scroll_delta, timestamp, window.get());
  EXPECT_EQ(IndicatorState::kPreviewAreaLeft, GetIndicatorState());
  EndScrollSequence(start, long_scroll_delta, timestamp, window.get(),
                    /*is_fling=*/true, /*velocity_y*/ 0,
                    /*velocity_x=*/large_velocity);
  WindowSelectorController* window_selector_controller =
      Shell::Get()->window_selector_controller();
  WindowSelector* window_selector =
      window_selector_controller->window_selector();
  EXPECT_TRUE(window_selector_controller->IsSelecting());
  EXPECT_TRUE(window_selector->IsWindowInOverview(window.get()));
  ToggleOverview();
  EXPECT_TRUE(wm::GetWindowState(window.get())->IsMaximized());

  // Fling to the right with small velocity when trying to snap the
  // window to the left should still snap the window to left.
  SendScrollStartAndUpdate(start, long_scroll_delta, timestamp, window.get());
  EXPECT_EQ(IndicatorState::kPreviewAreaLeft, GetIndicatorState());
  EndScrollSequence(start, long_scroll_delta, timestamp, window.get(),
                    /*is_fling=*/true, /*velocity_y*/ 0,
                    /*velocity_x=*/small_velocity);
  EXPECT_TRUE(wm::GetWindowState(window.get())->IsSnapped());
  EndSplitView();
  EXPECT_TRUE(wm::GetWindowState(window.get())->IsMaximized());

  // Fling to the left with large enough velocity when trying to snap the window
  // to the right should drop the window to overvie.
  start = gfx::Point(display_bounds.right(), 0);
  SendScrollStartAndUpdate(start, long_scroll_delta, timestamp, window.get());
  EXPECT_EQ(IndicatorState::kPreviewAreaRight, GetIndicatorState());
  EndScrollSequence(start, long_scroll_delta, timestamp, window.get(),
                    /*is_fling=*/true, /*velocity_y*/ 0,
                    /*velocity_x=*/-large_velocity);
  window_selector = window_selector_controller->window_selector();
  EXPECT_TRUE(window_selector_controller->IsSelecting());
  EXPECT_TRUE(window_selector->IsWindowInOverview(window.get()));
  ToggleOverview();

  // Fling to the left with small velocity when trying to snap the
  // window to the right should still snap the window to right.
  SendScrollStartAndUpdate(start, long_scroll_delta, timestamp, window.get());
  EXPECT_EQ(IndicatorState::kPreviewAreaRight, GetIndicatorState());
  EndScrollSequence(start, long_scroll_delta, timestamp, window.get(),
                    /*is_fling=*/true, /*velocity_y*/ 0,
                    /*velocity_x=*/-small_velocity);
  EXPECT_TRUE(wm::GetWindowState(window.get())->IsSnapped());
}

// Tests the functionalities that fling a window when splitview is active.
TEST_F(SplitViewAppDraggingTest, FlingWhenSplitViewIsActive) {
  std::unique_ptr<aura::Window> window1 = CreateTestWindowWithWidget();
  std::unique_ptr<aura::Window> window2 = CreateTestWindowWithWidget();

  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);

  gfx::Rect display_bounds =
      split_view_controller()->GetDisplayWorkAreaBoundsInScreen(window1.get());
  const float long_scroll_y = display_bounds.bottom() - 10;
  float large_velocity =
      TabletModeWindowDragDelegate::kFlingToOverviewFromSnappingAreaThreshold +
      10.f;
  const gfx::Point start;

  // Fling the window in left snapping area to left should still snap the
  // window.
  base::TimeTicks timestamp = base::TimeTicks::Now();
  SendScrollStartAndUpdate(start, long_scroll_y, timestamp, window1.get());
  EndScrollSequence(start, long_scroll_y, timestamp, window1.get(),
                    /*is_fling=*/true, /*velocity_y=*/0,
                    /*velocity_x=*/-large_velocity);
  EXPECT_TRUE(wm::GetWindowState(window1.get())->IsSnapped());
  EXPECT_EQ(SplitViewController::BOTH_SNAPPED,
            split_view_controller()->state());

  // Fling the window in left snapping area to right should drop the window
  // into overview.
  SendScrollStartAndUpdate(start, long_scroll_y, timestamp, window1.get());
  EndScrollSequence(start, long_scroll_y, timestamp, window1.get(),
                    /*is_fling=*/true, /*velocity_y=*/0,
                    /*velocity_x=*/large_velocity);
  WindowSelectorController* selector_controller =
      Shell::Get()->window_selector_controller();
  EXPECT_TRUE(selector_controller->window_selector()->IsWindowInOverview(
      window1.get()));
  EXPECT_EQ(SplitViewController::RIGHT_SNAPPED,
            split_view_controller()->state());
  ToggleOverview();

  // Fling the window in right snapping area to left should drop the window into
  // overview.
  EXPECT_EQ(SplitViewController::BOTH_SNAPPED,
            split_view_controller()->state());
  const int scroll_x = display_bounds.CenterPoint().x() + 10;
  SendScrollStartAndUpdate(start, long_scroll_y, timestamp, window1.get(),
                           scroll_x);
  gfx::Point end(scroll_x, 0);
  EndScrollSequence(end, long_scroll_y, timestamp, window1.get(),
                    /*is_fling=*/true, /*velocity_y=*/0,
                    /*velocity_x=*/-large_velocity);
  EXPECT_TRUE(selector_controller->window_selector()->IsWindowInOverview(
      window1.get()));
  EXPECT_EQ(SplitViewController::RIGHT_SNAPPED,
            split_view_controller()->state());
  ToggleOverview();

  // Fling the window in right snapping area to right should snap the window to
  // right side.
  EXPECT_EQ(SplitViewController::BOTH_SNAPPED,
            split_view_controller()->state());
  SendScrollStartAndUpdate(start, long_scroll_y, timestamp, window1.get(),
                           scroll_x);
  EndScrollSequence(end, long_scroll_y, timestamp, window1.get(),
                    /*is_fling=*/true, /*velocity_y=*/0,
                    /*velocity_x=*/large_velocity);
  EXPECT_EQ(split_view_controller()->right_window(), window1.get());
  EXPECT_TRUE(selector_controller->window_selector()->IsWindowInOverview(
      window2.get()));
  EXPECT_EQ(SplitViewController::RIGHT_SNAPPED,
            split_view_controller()->state());
}

}  // namespace ash
