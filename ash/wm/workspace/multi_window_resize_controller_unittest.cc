// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/multi_window_resize_controller.h"

#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/public/cpp/ash_constants.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/shell.h"
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
    WorkspaceController* wc = ShellTestApi().workspace_controller();
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
    return base::Contains(resize_controller_->windows_.other_windows, window);
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
  w1->Init(std::move(params1));
  w1->Show();

  std::unique_ptr<views::Widget> w2(new views::Widget);
  views::Widget::InitParams params2;
  params2.delegate = new TestWidgetDelegate;
  params2.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params2.bounds = gfx::Rect(100, 0, 100, 100);
  params2.context = CurrentContext();
  w2->Init(std::move(params2));
  w2->Show();

  std::unique_ptr<views::Widget> w3(new views::Widget);
  views::Widget::InitParams params3;
  params3.delegate = new TestWidgetDelegate;
  params3.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params3.bounds = gfx::Rect(100, 100, 100, 100);
  params3.context = CurrentContext();
  w3->Init(std::move(params3));
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

  // It should be possible to move 1px.
  bounds = resize_widget()->GetWindowBoundsInScreen();

  generator->MoveMouseTo(bounds.x() + 1, bounds.y() + 1);
  generator->PressLeftButton();
  generator->MoveMouseBy(1, 0);
  generator->ReleaseLeftButton();

  EXPECT_EQ(gfx::Rect(0, 0, 111, 100), w1->bounds());
  EXPECT_EQ(gfx::Rect(111, 0, 100, 100), w2->bounds());
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

  // Test that when drag starts, drag details are created for each window.
  EXPECT_TRUE(WindowState::Get(w1.get())->is_dragged());
  EXPECT_TRUE(WindowState::Get(w2.get())->is_dragged());
  EXPECT_TRUE(WindowState::Get(w3.get())->is_dragged());
  // Test the window components for each window.
  EXPECT_EQ(WindowState::Get(w1.get())->drag_details()->window_component,
            HTRIGHT);
  EXPECT_EQ(WindowState::Get(w2.get())->drag_details()->window_component,
            HTLEFT);
  EXPECT_EQ(WindowState::Get(w3.get())->drag_details()->window_component,
            HTLEFT);

  generator->MoveMouseTo(bounds.x() + 11, bounds.y() + 10);

  // Drag details should exist during dragging.
  EXPECT_TRUE(WindowState::Get(w1.get())->is_dragged());
  EXPECT_TRUE(WindowState::Get(w2.get())->is_dragged());
  EXPECT_TRUE(WindowState::Get(w3.get())->is_dragged());

  EXPECT_TRUE(HasTarget(w3.get()));

  // Release the mouse. The resizer should still be visible and a subsequent
  // press should not trigger a DCHECK.
  generator->ReleaseLeftButton();
  EXPECT_TRUE(IsShowing());

  // Test that drag details are correctly deleted after dragging.
  EXPECT_FALSE(WindowState::Get(w1.get())->is_dragged());
  EXPECT_FALSE(WindowState::Get(w2.get())->is_dragged());
  EXPECT_FALSE(WindowState::Get(w3.get())->is_dragged());

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
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
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

// Tests that the resizer does not appear while the mouse resides in a
// non-resizeable window.
TEST_F(MultiWindowResizeControllerTest, NonResizeableWindowTestA) {
  aura::test::TestWindowDelegate delegate1;
  std::unique_ptr<aura::Window> w1(CreateTestWindowInShellWithDelegate(
      &delegate1, -1, gfx::Rect(0, 0, 100, 100)));
  delegate1.set_window_component(HTRIGHT);
  aura::test::TestWindowDelegate delegate2;
  std::unique_ptr<aura::Window> w2(CreateTestWindowInShellWithDelegate(
      &delegate2, -2, gfx::Rect(0, 0, 100, 100)));
  w2->SetProperty(aura::client::kResizeBehaviorKey, 0);
  delegate2.set_window_component(HTRIGHT);
  aura::test::TestWindowDelegate delegate3;
  std::unique_ptr<aura::Window> w3(CreateTestWindowInShellWithDelegate(
      &delegate3, -3, gfx::Rect(100, 0, 100, 100)));
  delegate3.set_window_component(HTRIGHT);
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(w1->bounds().CenterPoint());
  EXPECT_FALSE(HasPendingShow());
}

// Tests that the resizer does not appear while the mouse resides in a window
// bordering two other windows, one of which is non-resizeable and obscures the
// other.
TEST_F(MultiWindowResizeControllerTest, NonResizeableWindowTestB) {
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
      &delegate3, -3, gfx::Rect(100, 0, 100, 100)));
  w3->SetProperty(aura::client::kResizeBehaviorKey, 0);
  delegate3.set_window_component(HTRIGHT);
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(w1->bounds().CenterPoint());
  EXPECT_FALSE(HasPendingShow());
}

// Tests that the resizer appears while the mouse resides in a window bordering
// two other windows, one of which is non-resizeable but obscured by the other.
TEST_F(MultiWindowResizeControllerTest, NonResizeableWindowTestC) {
  aura::test::TestWindowDelegate delegate1;
  std::unique_ptr<aura::Window> w1(CreateTestWindowInShellWithDelegate(
      &delegate1, -1, gfx::Rect(0, 0, 100, 100)));
  delegate1.set_window_component(HTRIGHT);
  aura::test::TestWindowDelegate delegate2;
  std::unique_ptr<aura::Window> w2(CreateTestWindowInShellWithDelegate(
      &delegate2, -2, gfx::Rect(100, 0, 100, 100)));
  w2->SetProperty(aura::client::kResizeBehaviorKey, 0);
  delegate2.set_window_component(HTRIGHT);
  aura::test::TestWindowDelegate delegate3;
  std::unique_ptr<aura::Window> w3(CreateTestWindowInShellWithDelegate(
      &delegate3, -3, gfx::Rect(100, 0, 100, 100)));
  delegate3.set_window_component(HTRIGHT);
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(w1->bounds().CenterPoint());
  EXPECT_TRUE(HasPendingShow());
  EXPECT_FALSE(HasTarget(w2.get()));
}

// Tests that the resizer is dismissed when one of the resized windows becomes
// non-resizeable.
TEST_F(MultiWindowResizeControllerTest, MakeWindowNonResizeable) {
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

  // Making one window non-resizeable should dismiss the resizer.
  w1->SetProperty(aura::client::kResizeBehaviorKey, 0);
  EXPECT_FALSE(IsShowing());
}

namespace {

class TestWindowStateDelegate : public WindowStateDelegate {
 public:
  TestWindowStateDelegate() = default;
  ~TestWindowStateDelegate() override = default;

  // WindowStateDelegate:
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
  const int bottom_inset = 300 - ShelfConfig::Get()->shelf_size();
  // Create two snapped windows, one left snapped, one right snapped.
  aura::test::TestWindowDelegate delegate1;
  std::unique_ptr<aura::Window> w1(CreateTestWindowInShellWithDelegate(
      &delegate1, -1, gfx::Rect(100, 100, 100, 100)));
  delegate1.set_window_component(HTRIGHT);
  WindowState* w1_state = WindowState::Get(w1.get());
  const WMEvent snap_left(WM_EVENT_SNAP_LEFT);
  w1_state->OnWMEvent(&snap_left);
  EXPECT_EQ(WindowStateType::kLeftSnapped, w1_state->GetStateType());
  aura::test::TestWindowDelegate delegate2;
  std::unique_ptr<aura::Window> w2(CreateTestWindowInShellWithDelegate(
      &delegate2, -2, gfx::Rect(100, 100, 100, 100)));
  delegate2.set_window_component(HTRIGHT);
  WindowState* w2_state = WindowState::Get(w2.get());
  const WMEvent snap_right(WM_EVENT_SNAP_RIGHT);
  w2_state->OnWMEvent(&snap_right);
  EXPECT_EQ(WindowStateType::kRightSnapped, w2_state->GetStateType());
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
  EXPECT_EQ(WindowStateType::kLeftSnapped, w1_state->GetStateType());
  EXPECT_EQ(gfx::Rect(0, 0, 300, bottom_inset), w1->bounds());
  EXPECT_EQ(WindowStateType::kRightSnapped, w2_state->GetStateType());
  EXPECT_EQ(gfx::Rect(300, 0, 100, bottom_inset), w2->bounds());
  EXPECT_EQ(0.75f, *w1_state->snapped_width_ratio());
  EXPECT_EQ(0.25f, *w2_state->snapped_width_ratio());

  // Dragging should call the WindowStateDelegate.
  EXPECT_EQ(HTRIGHT, window_state_delegate1->GetComponentAndReset());
  EXPECT_EQ(gfx::Point(300, resize_widget_center.y()),
            window_state_delegate1->GetLocationAndReset());
}

}  // namespace ash
