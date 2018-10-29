// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/multi_window_resize_controller.h"

#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/public/cpp/ash_constants.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shell.h"
#include "ash/shell_test_api.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_state_delegate.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/workspace/workspace_event_handler_test_helper.h"
#include "ash/wm/workspace_controller.h"
#include "ash/wm/workspace_controller_test_api.h"
#include "base/stl_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/base/class_property.h"
#include "ui/base/hit_test.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

namespace {

// WidgetDelegate for a resizable widget which creates a NonClientFrameView
// which is actually used in Ash.
class TestWidgetDelegate : public views::WidgetDelegateView {
 public:
  TestWidgetDelegate() = default;
  ~TestWidgetDelegate() override = default;

  // views::WidgetDelegateView:
  bool CanResize() const override { return true; }

  views::NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) override {
    return new NonClientFrameViewAsh(widget);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestWidgetDelegate);
};

}  // namespace

class MultiWindowResizeControllerTest : public AshTestBase {
 public:
  MultiWindowResizeControllerTest() = default;
  ~MultiWindowResizeControllerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    WorkspaceController* wc = ShellTestApi(Shell::Get()).workspace_controller();
    WorkspaceEventHandler* event_handler =
        WorkspaceControllerTestApi(wc).GetEventHandler();
    resize_controller_ =
        WorkspaceEventHandlerTestHelper(event_handler).resize_controller();
  }

 protected:
  void ShowNow() { resize_controller_->ShowNow(); }

  bool IsShowing() { return resize_controller_->IsShowing(); }

  bool HasPendingShow() { return resize_controller_->show_timer_.IsRunning(); }

  void Hide() { resize_controller_->Hide(); }

  bool HasTarget(aura::Window* window) {
    if (!resize_controller_->windows_.is_valid())
      return false;
    if (resize_controller_->windows_.window1 == window ||
        resize_controller_->windows_.window2 == window) {
      return true;
    }
    return base::ContainsValue(resize_controller_->windows_.other_windows,
                               window);
  }

  bool IsOverWindows(const gfx::Point& loc) {
    return resize_controller_->IsOverWindows(loc);
  }

  views::Widget* resize_widget() {
    return resize_controller_->resize_widget_.get();
  }

  MultiWindowResizeController* resize_controller_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(MultiWindowResizeControllerTest);
};

// Assertions around moving mouse over 2 windows.
TEST_F(MultiWindowResizeControllerTest, BasicTests) {
  aura::test::TestWindowDelegate delegate1;
  std::unique_ptr<aura::Window> w1(CreateTestWindowInShellWithDelegate(
      &delegate1, -1, gfx::Rect(0, 0, 100, 100)));
  delegate1.set_window_component(HTRIGHT);
  aura::test::TestWindowDelegate delegate2;
  std::unique_ptr<aura::Window> w2(CreateTestWindowInShellWithDelegate(
      &delegate2, -2, gfx::Rect(100, 0, 100, 100)));
  delegate2.set_window_component(HTRIGHT);
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(w1->bounds().CenterPoint());
  EXPECT_TRUE(HasPendingShow());
  EXPECT_TRUE(IsShowing());

  // Force a show now.
  ShowNow();
  EXPECT_FALSE(HasPendingShow());
  EXPECT_TRUE(IsShowing());

  EXPECT_FALSE(IsOverWindows(gfx::Point(200, 200)));

  // Have to explicitly invoke this as MouseWatcher listens for native events.
  resize_controller_->MouseMovedOutOfHost();
  EXPECT_FALSE(HasPendingShow());
  EXPECT_FALSE(IsShowing());
}

// Test the behavior of IsOverWindows().
TEST_F(MultiWindowResizeControllerTest, IsOverWindows) {
  // Create the following layout:
  //  __________________
  //  | w1     | w2     |
  //  |        |________|
  //  |        | w3     |
  //  |________|________|
  std::unique_ptr<views::Widget> w1(new views::Widget);
  views::Widget::InitParams params1;
  params1.delegate = new TestWidgetDelegate;
  params1.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params1.bounds = gfx::Rect(100, 200);
  params1.context = CurrentContext();
  w1->Init(params1);
  w1->Show();

  std::unique_ptr<views::Widget> w2(new views::Widget);
  views::Widget::InitParams params2;
  params2.delegate = new TestWidgetDelegate;
  params2.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params2.bounds = gfx::Rect(100, 0, 100, 100);
  params2.context = CurrentContext();
  w2->Init(params2);
  w2->Show();

  std::unique_ptr<views::Widget> w3(new views::Widget);
  views::Widget::InitParams params3;
  params3.delegate = new TestWidgetDelegate;
  params3.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params3.bounds = gfx::Rect(100, 100, 100, 100);
  params3.context = CurrentContext();
  w3->Init(params3);
  w3->Show();

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(gfx::Point(100, 150));
  EXPECT_TRUE(HasPendingShow());
  EXPECT_TRUE(IsShowing());
  ShowNow();
  EXPECT_TRUE(IsShowing());

  // Check that the multi-window resize handle does not hide while the mouse is
  // over a window's resize area. A window's resize area extends outside the
  // window's bounds.
  EXPECT_TRUE(w3->IsActive());
  ASSERT_LT(kResizeInsideBoundsSize, kResizeOutsideBoundsSize);

  EXPECT_TRUE(IsOverWindows(gfx::Point(100, 150)));
  EXPECT_TRUE(IsOverWindows(gfx::Point(100 - kResizeOutsideBoundsSize, 150)));
  EXPECT_FALSE(
      IsOverWindows(gfx::Point(100 - kResizeOutsideBoundsSize - 1, 150)));
  EXPECT_TRUE(
      IsOverWindows(gfx::Point(100 + kResizeInsideBoundsSize - 1, 150)));
  EXPECT_FALSE(IsOverWindows(gfx::Point(100 + kResizeInsideBoundsSize, 150)));
  EXPECT_FALSE(
      IsOverWindows(gfx::Point(100 + kResizeOutsideBoundsSize - 1, 150)));

  w1->Activate();
  EXPECT_TRUE(IsOverWindows(gfx::Point(100, 150)));
  EXPECT_TRUE(IsOverWindows(gfx::Point(100 - kResizeInsideBoundsSize, 150)));
  EXPECT_FALSE(
      IsOverWindows(gfx::Point(100 - kResizeInsideBoundsSize - 1, 150)));
  EXPECT_FALSE(IsOverWindows(gfx::Point(100 - kResizeOutsideBoundsSize, 150)));
  EXPECT_TRUE(
      IsOverWindows(gfx::Point(100 + kResizeOutsideBoundsSize - 1, 150)));
  EXPECT_FALSE(IsOverWindows(gfx::Point(100 + kResizeOutsideBoundsSize, 150)));

  // Check that the multi-window resize handles eventually hide if the mouse
  // moves between |w1| and |w2|.
  EXPECT_FALSE(IsOverWindows(gfx::Point(100, 50)));
}

// Makes sure deleting a window hides.
TEST_F(MultiWindowResizeControllerTest, DeleteWindow) {
  aura::test::TestWindowDelegate delegate1;
  std::unique_ptr<aura::Window> w1(CreateTestWindowInShellWithDelegate(
      &delegate1, -1, gfx::Rect(0, 0, 100, 100)));
  delegate1.set_window_component(HTRIGHT);
  aura::test::TestWindowDelegate delegate2;
  std::unique_ptr<aura::Window> w2(CreateTestWindowInShellWithDelegate(
      &delegate2, -2, gfx::Rect(100, 0, 100, 100)));
  delegate2.set_window_component(HTRIGHT);
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(w1->bounds().CenterPoint());
  EXPECT_TRUE(HasPendingShow());
  EXPECT_TRUE(IsShowing());

  // Force a show now.
  ShowNow();
  EXPECT_FALSE(HasPendingShow());
  EXPECT_TRUE(IsShowing());

  // Move the mouse over the resize widget.
  ASSERT_TRUE(resize_widget());
  gfx::Rect bounds(resize_widget()->GetWindowBoundsInScreen());
  generator->MoveMouseTo(bounds.x() + 1, bounds.y() + 1);
  EXPECT_FALSE(HasPendingShow());
  EXPECT_TRUE(IsShowing());

  // Move the resize widget
  generator->PressLeftButton();
  generator->MoveMouseTo(bounds.x() + 10, bounds.y() + 10);

  // Delete w2.
  w2.reset();
  EXPECT_FALSE(resize_widget());
  EXPECT_FALSE(HasPendingShow());
  EXPECT_FALSE(IsShowing());
  EXPECT_FALSE(HasTarget(w1.get()));
}

// Tests resizing.
TEST_F(MultiWindowResizeControllerTest, Drag) {
  aura::test::TestWindowDelegate delegate1;
  std::unique_ptr<aura::Window> w1(CreateTestWindowInShellWithDelegate(
      &delegate1, -1, gfx::Rect(0, 0, 100, 100)));
  delegate1.set_window_component(HTRIGHT);
  aura::test::TestWindowDelegate delegate2;
  std::unique_ptr<aura::Window> w2(CreateTestWindowInShellWithDelegate(
      &delegate2, -2, gfx::Rect(100, 0, 100, 100)));
  delegate2.set_window_component(HTRIGHT);
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(w1->bounds().CenterPoint());
  EXPECT_TRUE(HasPendingShow());
  EXPECT_TRUE(IsShowing());

  // Force a show now.
  ShowNow();
  EXPECT_FALSE(HasPendingShow());
  EXPECT_TRUE(IsShowing());

  // Move the mouse over the resize widget.
  ASSERT_TRUE(resize_widget());
  gfx::Rect bounds(resize_widget()->GetWindowBoundsInScreen());
  generator->MoveMouseTo(bounds.x() + 1, bounds.y() + 1);
  EXPECT_FALSE(HasPendingShow());
  EXPECT_TRUE(IsShowing());

  // Move the resize widget
  generator->PressLeftButton();
  generator->MoveMouseTo(bounds.x() + 11, bounds.y() + 10);
  generator->ReleaseLeftButton();

  EXPECT_TRUE(resize_widget());
  EXPECT_FALSE(HasPendingShow());
  EXPECT_TRUE(IsShowing());
  EXPECT_EQ(gfx::Rect(0, 0, 110, 100), w1->bounds());
  EXPECT_EQ(gfx::Rect(110, 0, 100, 100), w2->bounds());
}

// Makes sure three windows are picked up.
TEST_F(MultiWindowResizeControllerTest, Three) {
  aura::test::TestWindowDelegate delegate1;
  std::unique_ptr<aura::Window> w1(CreateTestWindowInShellWithDelegate(
      &delegate1, -1, gfx::Rect(0, 0, 100, 100)));
  delegate1.set_window_component(HTRIGHT);
  aura::test::TestWindowDelegate delegate2;
  std::unique_ptr<aura::Window> w2(CreateTestWindowInShellWithDelegate(
      &delegate2, -2, gfx::Rect(100, 0, 100, 100)));
  delegate2.set_window_component(HTRIGHT);
  aura::test::TestWindowDelegate delegate3;
  std::unique_ptr<aura::Window> w3(CreateTestWindowInShellWithDelegate(
      &delegate3, -3, gfx::Rect(200, 0, 100, 100)));
  delegate3.set_window_component(HTRIGHT);

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(w1->bounds().CenterPoint());
  EXPECT_TRUE(HasPendingShow());
  EXPECT_TRUE(IsShowing());
  EXPECT_FALSE(HasTarget(w3.get()));

  ShowNow();
  EXPECT_FALSE(HasPendingShow());
  EXPECT_TRUE(IsShowing());

  // w3 should be picked up when resize is started.
  gfx::Rect bounds(resize_widget()->GetWindowBoundsInScreen());
  generator->MoveMouseTo(bounds.x() + 1, bounds.y() + 1);
  generator->PressLeftButton();
  generator->MoveMouseTo(bounds.x() + 11, bounds.y() + 10);

  EXPECT_TRUE(HasTarget(w3.get()));

  // Release the mouse. The resizer should still be visible and a subsequent
  // press should not trigger a DCHECK.
  generator->ReleaseLeftButton();
  EXPECT_TRUE(IsShowing());
  generator->PressLeftButton();
}

// Tests that clicking outside of the resize handle dismisses it.
TEST_F(MultiWindowResizeControllerTest, ClickOutside) {
  aura::test::TestWindowDelegate delegate1;
  std::unique_ptr<aura::Window> w1(CreateTestWindowInShellWithDelegate(
      &delegate1, -1, gfx::Rect(0, 0, 100, 100)));
  delegate1.set_window_component(HTRIGHT);
  aura::test::TestWindowDelegate delegate2;
  std::unique_ptr<aura::Window> w2(CreateTestWindowInShellWithDelegate(
      &delegate2, -2, gfx::Rect(100, 0, 100, 100)));
  delegate2.set_window_component(HTLEFT);

  ui::test::EventGenerator* generator = GetEventGenerator();
  gfx::Point w1_center_in_screen = w1->GetBoundsInScreen().CenterPoint();
  generator->MoveMouseTo(w1_center_in_screen);
  EXPECT_TRUE(HasPendingShow());
  EXPECT_TRUE(IsShowing());
  ShowNow();
  EXPECT_TRUE(IsShowing());

  gfx::Rect resize_widget_bounds_in_screen =
      resize_widget()->GetWindowBoundsInScreen();

  // Clicking on the resize handle should not do anything.
  generator->MoveMouseTo(resize_widget_bounds_in_screen.CenterPoint());
  generator->ClickLeftButton();
  EXPECT_TRUE(IsShowing());

  // Clicking outside the resize handle should immediately hide the resize
  // handle.
  EXPECT_FALSE(resize_widget_bounds_in_screen.Contains(w1_center_in_screen));
  generator->MoveMouseTo(w1_center_in_screen);
  generator->ClickLeftButton();
  EXPECT_FALSE(IsShowing());
}

// Tests that if the resized window is maximized/fullscreen/minimized, the
// resizer widget should be dismissed.
TEST_F(MultiWindowResizeControllerTest, WindowStateChange) {
  aura::test::TestWindowDelegate delegate1;
  std::unique_ptr<aura::Window> w1(CreateTestWindowInShellWithDelegate(
      &delegate1, -1, gfx::Rect(0, 0, 100, 100)));
  delegate1.set_window_component(HTRIGHT);
  aura::test::TestWindowDelegate delegate2;
  std::unique_ptr<aura::Window> w2(CreateTestWindowInShellWithDelegate(
      &delegate2, -2, gfx::Rect(100, 0, 100, 100)));
  delegate2.set_window_component(HTLEFT);

  ui::test::EventGenerator* generator = GetEventGenerator();
  gfx::Point w1_center_in_screen = w1->GetBoundsInScreen().CenterPoint();
  generator->MoveMouseTo(w1_center_in_screen);
  ShowNow();
  EXPECT_TRUE(IsShowing());

  // Maxmize one window should dismiss the resizer.
  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  EXPECT_FALSE(IsShowing());

  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  generator->MoveMouseTo(w1_center_in_screen);
  ShowNow();
  EXPECT_TRUE(IsShowing());

  // Entering Fullscreen should dismiss the resizer.
  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_FULLSCREEN);
  EXPECT_FALSE(IsShowing());

  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  generator->MoveMouseTo(w1_center_in_screen);
  ShowNow();
  EXPECT_TRUE(IsShowing());

  // Minimize one window should dimiss the resizer.
  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MINIMIZED);
  EXPECT_FALSE(IsShowing());

  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  generator->MoveMouseTo(w1_center_in_screen);
  ShowNow();
  EXPECT_TRUE(IsShowing());

  // When entering tablet mode, the windows will be maximized, thus the resizer
  // widget should be dismissed.
  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(true);
  EXPECT_FALSE(IsShowing());
}

// Tests that if one of the resized windows visibility changes to hidden, the
// resize widget should be dismissed.
TEST_F(MultiWindowResizeControllerTest, HideWindowTest) {
  aura::test::TestWindowDelegate delegate1;
  std::unique_ptr<aura::Window> w1(CreateTestWindowInShellWithDelegate(
      &delegate1, -1, gfx::Rect(0, 0, 100, 100)));
  delegate1.set_window_component(HTRIGHT);
  aura::test::TestWindowDelegate delegate2;
  std::unique_ptr<aura::Window> w2(CreateTestWindowInShellWithDelegate(
      &delegate2, -2, gfx::Rect(100, 0, 100, 100)));
  delegate2.set_window_component(HTLEFT);

  ui::test::EventGenerator* generator = GetEventGenerator();
  gfx::Point w1_center_in_screen = w1->GetBoundsInScreen().CenterPoint();
  generator->MoveMouseTo(w1_center_in_screen);
  ShowNow();
  EXPECT_TRUE(IsShowing());

  // Hide one window should dimiss the resizer.
  w1->Hide();
  EXPECT_FALSE(IsShowing());
}

namespace {

class TestWindowStateDelegate : public wm::WindowStateDelegate {
 public:
  TestWindowStateDelegate() = default;
  ~TestWindowStateDelegate() override = default;

  // wm::WindowStateDelegate:
  void OnDragStarted(int component) override { component_ = component; }
  void OnDragFinished(bool cancel, const gfx::Point& location) override {
    location_ = location;
  }

  int GetComponentAndReset() {
    int result = component_;
    component_ = -1;
    return result;
  }

  gfx::Point GetLocationAndReset() {
    gfx::Point p = location_;
    location_.SetPoint(0, 0);
    return p;
  }

 private:
  gfx::Point location_;
  int component_ = -1;
  DISALLOW_COPY_AND_ASSIGN(TestWindowStateDelegate);
};

}  // namespace

// Tests dragging to resize two snapped windows.
TEST_F(MultiWindowResizeControllerTest, TwoSnappedWindows) {
  UpdateDisplay("400x300");
  const int bottom_inset = 300 - ShelfConstants::shelf_size();
  // Create two snapped windows, one left snapped, one right snapped.
  aura::test::TestWindowDelegate delegate1;
  std::unique_ptr<aura::Window> w1(CreateTestWindowInShellWithDelegate(
      &delegate1, -1, gfx::Rect(100, 100, 100, 100)));
  delegate1.set_window_component(HTRIGHT);
  wm::WindowState* w1_state = wm::GetWindowState(w1.get());
  const wm::WMEvent snap_left(wm::WM_EVENT_SNAP_LEFT);
  w1_state->OnWMEvent(&snap_left);
  EXPECT_EQ(mojom::WindowStateType::LEFT_SNAPPED, w1_state->GetStateType());
  aura::test::TestWindowDelegate delegate2;
  std::unique_ptr<aura::Window> w2(CreateTestWindowInShellWithDelegate(
      &delegate2, -2, gfx::Rect(100, 100, 100, 100)));
  delegate2.set_window_component(HTRIGHT);
  wm::WindowState* w2_state = wm::GetWindowState(w2.get());
  const wm::WMEvent snap_right(wm::WM_EVENT_SNAP_RIGHT);
  w2_state->OnWMEvent(&snap_right);
  EXPECT_EQ(mojom::WindowStateType::RIGHT_SNAPPED, w2_state->GetStateType());
  EXPECT_EQ(0.5f, *w1_state->snapped_width_ratio());
  EXPECT_EQ(0.5f, *w2_state->snapped_width_ratio());

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(w1->bounds().CenterPoint());
  EXPECT_TRUE(HasPendingShow());
  EXPECT_TRUE(IsShowing());
  // Force a show now.
  ShowNow();
  EXPECT_FALSE(HasPendingShow());
  EXPECT_TRUE(IsShowing());

  // Setup delegates
  auto* window_state_delegate1 = new TestWindowStateDelegate();
  w1_state->SetDelegate(base::WrapUnique(window_state_delegate1));

  // Move the mouse over the resize widget.
  ASSERT_TRUE(resize_widget());
  gfx::Rect bounds(resize_widget()->GetWindowBoundsInScreen());
  gfx::Point resize_widget_center = bounds.CenterPoint();
  generator->MoveMouseTo(resize_widget_center);
  EXPECT_FALSE(HasPendingShow());
  EXPECT_TRUE(IsShowing());

  // Move the resize widget.
  generator->PressLeftButton();
  generator->MoveMouseTo(resize_widget_center.x() + 100,
                         resize_widget_center.y());
  generator->ReleaseLeftButton();

  // Check snapped states and bounds.
  EXPECT_EQ(mojom::WindowStateType::LEFT_SNAPPED, w1_state->GetStateType());
  EXPECT_EQ(gfx::Rect(0, 0, 300, bottom_inset), w1->bounds());
  EXPECT_EQ(mojom::WindowStateType::RIGHT_SNAPPED, w2_state->GetStateType());
  EXPECT_EQ(gfx::Rect(300, 0, 100, bottom_inset), w2->bounds());
  EXPECT_EQ(0.75f, *w1_state->snapped_width_ratio());
  EXPECT_EQ(0.25f, *w2_state->snapped_width_ratio());

  // Dragging should call the WindowStateDelegate.
  EXPECT_EQ(HTRIGHT, window_state_delegate1->GetComponentAndReset());
  EXPECT_EQ(gfx::Point(300, bottom_inset - 75),
            window_state_delegate1->GetLocationAndReset());
}

}  // namespace ash
