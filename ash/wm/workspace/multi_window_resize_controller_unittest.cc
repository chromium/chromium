// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/multi_window_resize_controller.h"

#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_window_builder.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/test/fake_window_state.h"
#include "ash/wm/test/test_non_client_frame_view_ash.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_state_delegate.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/wm_metrics.h"
#include "ash/wm/workspace_controller.h"
#include "ash/wm/workspace_controller_test_api.h"
#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "chromeos/ui/base/chromeos_ui_constants.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/base/class_property.h"
#include "ui/base/hit_test.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

using chromeos::kResizeInsideBoundsSize;
using chromeos::kResizeOutsideBoundsSize;
using chromeos::WindowStateType;

namespace ash {

class MultiWindowResizeControllerTest : public AshTestBase {
 public:
  MultiWindowResizeControllerTest() = default;
  MultiWindowResizeControllerTest(const MultiWindowResizeControllerTest&) =
      delete;
  MultiWindowResizeControllerTest& operator=(
      const MultiWindowResizeControllerTest&) = delete;
  ~MultiWindowResizeControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    WorkspaceController* wc = ShellTestApi().workspace_controller();
    WorkspaceEventHandler* event_handler =
        WorkspaceControllerTestApi(wc).GetEventHandler();
    resize_controller_ = event_handler->multi_window_resize_controller();
  }

 protected:
  void ShowNow() { resize_controller_->ShowNow(); }

  bool IsShowing() { return resize_controller_->IsShowing(); }

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

  base::OneShotTimer* GetShowTimer() {
    return &(resize_controller_->show_timer_);
  }

  bool IsShowTimerRunning() {
    base::OneShotTimer* show_timer = GetShowTimer();
    return show_timer->IsRunning() &&
           show_timer->GetCurrentDelay() ==
               MultiWindowResizeController::kShowDelay;
  }

  raw_ptr<MultiWindowResizeController, DanglingUntriaged> resize_controller_ =
      nullptr;
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
  EXPECT_TRUE(IsShowTimerRunning());
  EXPECT_TRUE(IsShowing());

  // Force a show now.
  ShowNow();
  EXPECT_FALSE(IsShowTimerRunning());
  EXPECT_TRUE(IsShowing());

  EXPECT_FALSE(IsOverWindows(gfx::Point(200, 200)));

  // Have to explicitly invoke this as MouseWatcher listens for native events.
  resize_controller_->MouseMovedOutOfHost();
  EXPECT_FALSE(IsShowTimerRunning());
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
  views::Widget::InitParams params1(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  params1.delegate = new TestWidgetDelegateAsh();
  params1.bounds = gfx::Rect(100, 200);
  params1.context = GetContext();
  w1->Init(std::move(params1));
  w1->Show();

  std::unique_ptr<views::Widget> w2(new views::Widget);
  views::Widget::InitParams params2(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  params2.delegate = new TestWidgetDelegateAsh();
  params2.bounds = gfx::Rect(100, 0, 100, 100);
  params2.context = GetContext();
  w2->Init(std::move(params2));
  w2->Show();

  std::unique_ptr<views::Widget> w3(new views::Widget);
  views::Widget::InitParams params3(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  params3.delegate = new TestWidgetDelegateAsh();
  params3.bounds = gfx::Rect(100, 100, 100, 100);
  params3.context = GetContext();
  w3->Init(std::move(params3));
  w3->Show();

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(gfx::Point(100, 150));
  EXPECT_TRUE(IsShowTimerRunning());
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
  EXPECT_TRUE(IsShowTimerRunning());
  EXPECT_TRUE(IsShowing());

  // Force a show now.
  ShowNow();
  EXPECT_FALSE(IsShowTimerRunning());
  EXPECT_TRUE(IsShowing());

  // Move the mouse over the resize widget.
  ASSERT_TRUE(resize_widget());
  gfx::Rect bounds(resize_widget()->GetWindowBoundsInScreen());
  generator->MoveMouseTo(bounds.x() + 1, bounds.y() + 1);
  EXPECT_FALSE(IsShowTimerRunning());
  EXPECT_TRUE(IsShowing());

  // Move the resize widget
  generator->PressLeftButton();
  generator->MoveMouseTo(bounds.x() + 10, bounds.y() + 10);

  // Delete w2.
  w2.reset();
  EXPECT_FALSE(resize_widget());
  EXPECT_FALSE(IsShowTimerRunning());
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
  EXPECT_TRUE(IsShowTimerRunning());
  EXPECT_TRUE(IsShowing());

  // Force a show now.
  ShowNow();
  EXPECT_FALSE(IsShowTimerRunning());
  EXPECT_TRUE(IsShowing());

  // Move the mouse over the resize widget.
  ASSERT_TRUE(resize_widget());
  gfx::Rect bounds(resize_widget()->GetWindowBoundsInScreen());
  generator->MoveMouseTo(bounds.x() + 1, bounds.y() + 1);
  EXPECT_FALSE(IsShowTimerRunning());
  EXPECT_TRUE(IsShowing());

  // Move the resize widget
  generator->PressLeftButton();
  generator->MoveMouseTo(bounds.x() + 11, bounds.y() + 10);
  generator->ReleaseLeftButton();

  EXPECT_TRUE(resize_widget());
  EXPECT_FALSE(IsShowTimerRunning());
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
  EXPECT_TRUE(IsShowTimerRunning());
  EXPECT_TRUE(IsShowing());
  EXPECT_FALSE(HasTarget(w3.get()));

  ShowNow();
  EXPECT_FALSE(IsShowTimerRunning());
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
  EXPECT_TRUE(IsShowTimerRunning());
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
  w1->SetProperty(aura::client::kShowStateKey,
                  ui::mojom::WindowShowState::kMaximized);
  EXPECT_FALSE(IsShowing());

  w1->SetProperty(aura::client::kShowStateKey,
                  ui::mojom::WindowShowState::kNormal);
  generator->MoveMouseTo(w1_center_in_screen);
  ShowNow();
  EXPECT_TRUE(IsShowing());

  // Entering Fullscreen should dismiss the resizer.
  w1->SetProperty(aura::client::kShowStateKey,
                  ui::mojom::WindowShowState::kFullscreen);
  EXPECT_FALSE(IsShowing());

  w1->SetProperty(aura::client::kShowStateKey,
                  ui::mojom::WindowShowState::kNormal);
  generator->MoveMouseTo(w1_center_in_screen);
  ShowNow();
  EXPECT_TRUE(IsShowing());

  // Minimize one window should dimiss the resizer.
  w1->SetProperty(aura::client::kShowStateKey,
                  ui::mojom::WindowShowState::kMinimized);
  EXPECT_FALSE(IsShowing());

  w1->SetProperty(aura::client::kShowStateKey,
                  ui::mojom::WindowShowState::kNormal);
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
  auto child_of_w1 = ChildTestWindowBuilder(w1.get()).Build();

  aura::test::TestWindowDelegate delegate2;
  std::unique_ptr<aura::Window> w2(CreateTestWindowInShellWithDelegate(
      &delegate2, -2, gfx::Rect(100, 0, 100, 100)));
  delegate2.set_window_component(HTLEFT);

  ui::test::EventGenerator* generator = GetEventGenerator();
  gfx::Point w1_center_in_screen = w1->GetBoundsInScreen().CenterPoint();
  generator->MoveMouseTo(w1_center_in_screen);
  ShowNow();
  EXPECT_TRUE(IsShowing());

  // Hiding child window shouldn't dismiss the resizer.
  child_of_w1->Hide();
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
  EXPECT_FALSE(IsShowTimerRunning());
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
  EXPECT_FALSE(IsShowTimerRunning());
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
  EXPECT_TRUE(IsShowTimerRunning());
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
  const WindowSnapWMEvent snap_left(WM_EVENT_SNAP_PRIMARY);
  w1_state->OnWMEvent(&snap_left);
  EXPECT_EQ(WindowStateType::kPrimarySnapped, w1_state->GetStateType());
  aura::test::TestWindowDelegate delegate2;
  std::unique_ptr<aura::Window> w2(CreateTestWindowInShellWithDelegate(
      &delegate2, -2, gfx::Rect(100, 100, 100, 100)));
  delegate2.set_window_component(HTRIGHT);
  WindowState* w2_state = WindowState::Get(w2.get());
  const WindowSnapWMEvent snap_right(WM_EVENT_SNAP_SECONDARY);
  w2_state->OnWMEvent(&snap_right);
  EXPECT_EQ(WindowStateType::kSecondarySnapped, w2_state->GetStateType());
  EXPECT_EQ(0.5f, *w1_state->snap_ratio());
  EXPECT_EQ(0.5f, *w2_state->snap_ratio());

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(w1->bounds().CenterPoint());
  EXPECT_TRUE(IsShowTimerRunning());
  EXPECT_TRUE(IsShowing());
  // Force a show now.
  ShowNow();
  EXPECT_FALSE(IsShowTimerRunning());
  EXPECT_TRUE(IsShowing());

  // Setup delegate.
  auto window_state_delegate = std::make_unique<FakeWindowStateDelegate>();
  auto* window_state_delegate_ptr = window_state_delegate.get();
  w1_state->SetDelegate(std::move(window_state_delegate));

  // Move the mouse over the resize widget.
  ASSERT_TRUE(resize_widget());
  gfx::Rect bounds(resize_widget()->GetWindowBoundsInScreen());
  gfx::Point resize_widget_center = bounds.CenterPoint();
  generator->MoveMouseTo(resize_widget_center);
  EXPECT_FALSE(IsShowTimerRunning());
  EXPECT_TRUE(IsShowing());

  // Move the resize widget.
  generator->PressLeftButton();
  generator->MoveMouseTo(resize_widget_center.x() + 100,
                         resize_widget_center.y());
  generator->ReleaseLeftButton();

  // Check snapped states and bounds.
  EXPECT_EQ(WindowStateType::kPrimarySnapped, w1_state->GetStateType());
  EXPECT_EQ(gfx::Rect(0, 0, 300, bottom_inset), w1->bounds());
  EXPECT_EQ(WindowStateType::kSecondarySnapped, w2_state->GetStateType());
  EXPECT_EQ(gfx::Rect(300, 0, 100, bottom_inset), w2->bounds());
  EXPECT_EQ(0.75f, *w1_state->snap_ratio());
  EXPECT_EQ(0.25f, *w2_state->snap_ratio());

  // Dragging should call the WindowStateDelegate.
  EXPECT_EQ(HTRIGHT, window_state_delegate_ptr->drag_start_component());
  EXPECT_EQ(gfx::PointF(300, resize_widget_center.y()),
            window_state_delegate_ptr->drag_end_location());
}

TEST_F(MultiWindowResizeControllerTest, HiddenInOverview) {
  // Create two windows side by side, but not overlapping horizontally. Note
  // that when creating a window, the window is slightly larger than the given
  // bounds so position |window2| accordingly.
  auto window1 = CreateAppWindow(gfx::Rect(0, 0, 100, 100));
  auto window2 = CreateAppWindow(gfx::Rect(104, 0, 100, 100));

  // Move the mouse to the middle of the two windows. The multi window resizer
  // should appear.
  GetEventGenerator()->MoveMouseTo(gfx::Point(104, 50));
  EXPECT_TRUE(IsShowTimerRunning());
  EXPECT_TRUE(IsShowing());

  // Tests that after starting overview, the widget is hidden.
  EnterOverview();
  EXPECT_FALSE(IsShowTimerRunning());
  EXPECT_FALSE(IsShowing());
}

// Tests that the metrics to record the user action of initiating and clicking
// on the multi-window resizer widget for normal cases and the special cases
// when the two windows are snapped are recorded correctly in the metrics.
TEST_F(MultiWindowResizeControllerTest, MultiWindowResizeUserActionMetrics) {
  UpdateDisplay("400x300");
  // Create two windows with shared edge. Hover the mouse over the edge and only
  // `kMultiWindowResizerShow` will be recorded.
  aura::test::TestWindowDelegate delegate1;
  std::unique_ptr<aura::Window> w1(CreateTestWindowInShellWithDelegate(
      &delegate1, -1, gfx::Rect(0, 0, 100, 100)));
  delegate1.set_window_component(HTRIGHT);
  aura::test::TestWindowDelegate delegate2;
  std::unique_ptr<aura::Window> w2(CreateTestWindowInShellWithDelegate(
      &delegate2, -2, gfx::Rect(100, 0, 100, 100)));
  delegate2.set_window_component(HTRIGHT);

  // Verify the initial state of the metrics.
  base::UserActionTester user_action_tester;
  EXPECT_EQ(user_action_tester.GetActionCount(kMultiWindowResizerShow), 0);
  EXPECT_EQ(user_action_tester.GetActionCount(
                kMultiWindowResizerShowTwoWindowsSnapped),
            0);
  EXPECT_EQ(user_action_tester.GetActionCount(kMultiWindowResizerClick), 0);
  EXPECT_EQ(user_action_tester.GetActionCount(
                kMultiWindowResizerClickTwoWindowsSnapped),
            0);

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(w1->bounds().CenterPoint());
  EXPECT_TRUE(IsShowTimerRunning());
  EXPECT_TRUE(IsShowing());
  ShowNow();
  EXPECT_EQ(user_action_tester.GetActionCount(kMultiWindowResizerShow), 1);
  EXPECT_EQ(user_action_tester.GetActionCount(
                kMultiWindowResizerShowTwoWindowsSnapped),
            0);
  EXPECT_EQ(user_action_tester.GetActionCount(kMultiWindowResizerClick), 0);
  EXPECT_EQ(user_action_tester.GetActionCount(
                kMultiWindowResizerClickTwoWindowsSnapped),
            0);
  generator->MoveMouseTo(w1->GetBoundsInRootWindow().CenterPoint());
  generator->ClickLeftButton();
  EXPECT_FALSE(IsShowing());

  // Hover on the shared edge and click on the resize widget and both
  // `kMultiWindowResizerShow` and `kMultiWindowResizerClick` will be
  // incremented.
  generator->MoveMouseTo(w1->GetBoundsInRootWindow().CenterPoint());
  EXPECT_TRUE(IsShowTimerRunning());
  EXPECT_TRUE(IsShowing());
  ShowNow();
  ASSERT_TRUE(resize_widget());
  gfx::Rect bounds(resize_widget()->GetWindowBoundsInScreen());
  generator->MoveMouseTo(bounds.CenterPoint());
  generator->ClickLeftButton();
  EXPECT_EQ(user_action_tester.GetActionCount(kMultiWindowResizerShow), 2);
  EXPECT_EQ(user_action_tester.GetActionCount(
                kMultiWindowResizerShowTwoWindowsSnapped),
            0);
  EXPECT_EQ(user_action_tester.GetActionCount(kMultiWindowResizerClick), 1);
  EXPECT_EQ(user_action_tester.GetActionCount(
                kMultiWindowResizerClickTwoWindowsSnapped),
            0);
  generator->MoveMouseTo(w1->GetBoundsInRootWindow().CenterPoint());
  generator->ClickLeftButton();
  EXPECT_FALSE(IsShowing());

  // Snap two windows, one on the left, the other on the right. Hover the mouse
  // over the edge and both `kMultiWindowResizerShow` and
  // `kMultiWindowResizerShowTwoWindowsSnapped` will be recorded.
  WindowState* w1_state = WindowState::Get(w1.get());
  const WindowSnapWMEvent snap_left(WM_EVENT_SNAP_PRIMARY);
  w1_state->OnWMEvent(&snap_left);
  EXPECT_EQ(WindowStateType::kPrimarySnapped, w1_state->GetStateType());
  WindowState* w2_state = WindowState::Get(w2.get());
  const WindowSnapWMEvent snap_right(WM_EVENT_SNAP_SECONDARY);
  w2_state->OnWMEvent(&snap_right);
  EXPECT_EQ(WindowStateType::kSecondarySnapped, w2_state->GetStateType());
  EXPECT_EQ(0.5f, *w1_state->snap_ratio());
  EXPECT_EQ(0.5f, *w2_state->snap_ratio());

  generator->MoveMouseTo(w1->bounds().CenterPoint());
  EXPECT_TRUE(IsShowTimerRunning());
  EXPECT_TRUE(IsShowing());
  ShowNow();
  EXPECT_EQ(user_action_tester.GetActionCount(kMultiWindowResizerShow), 3);
  EXPECT_EQ(user_action_tester.GetActionCount(
                kMultiWindowResizerShowTwoWindowsSnapped),
            1);
  EXPECT_EQ(user_action_tester.GetActionCount(kMultiWindowResizerClick), 1);
  EXPECT_EQ(user_action_tester.GetActionCount(
                kMultiWindowResizerClickTwoWindowsSnapped),
            0);
  generator->MoveMouseTo(w1->GetBoundsInRootWindow().CenterPoint());
  generator->ClickLeftButton();
  EXPECT_FALSE(IsShowing());

  // Hover on the shared edge and click on the resize widget and all the metrics
  // will be incremented.
  generator->MoveMouseTo(w1->bounds().CenterPoint());
  EXPECT_TRUE(IsShowTimerRunning());
  EXPECT_TRUE(IsShowing());
  ShowNow();
  ASSERT_TRUE(resize_widget());
  generator->MoveMouseTo(
      resize_widget()->GetWindowBoundsInScreen().CenterPoint());
  generator->ClickLeftButton();
  EXPECT_EQ(user_action_tester.GetActionCount(kMultiWindowResizerShow), 4);
  EXPECT_EQ(user_action_tester.GetActionCount(
                kMultiWindowResizerShowTwoWindowsSnapped),
            2);
  EXPECT_EQ(user_action_tester.GetActionCount(kMultiWindowResizerClick), 2);
  EXPECT_EQ(user_action_tester.GetActionCount(
                kMultiWindowResizerClickTwoWindowsSnapped),
            1);
}

// Tests that the histogram metrics for the multi-window resizer are correctly
// recorded.
TEST_F(MultiWindowResizeControllerTest, MultiWindowResizeHistogramTest) {
  UpdateDisplay("400x300");

  // Create two windows with shared edge.
  aura::test::TestWindowDelegate delegate1;
  std::unique_ptr<aura::Window> w1(CreateTestWindowInShellWithDelegate(
      &delegate1, -1, gfx::Rect(0, 0, 100, 100)));
  delegate1.set_window_component(HTRIGHT);
  aura::test::TestWindowDelegate delegate2;
  std::unique_ptr<aura::Window> w2(CreateTestWindowInShellWithDelegate(
      &delegate2, -2, gfx::Rect(100, 0, 100, 100)));
  delegate2.set_window_component(HTRIGHT);

  base::HistogramTester histogram_tester;

  // Verify the initial count for the histogram metrics.
  histogram_tester.ExpectBucketCount(kMultiWindowResizerShowHistogramName, true,
                                     0);
  histogram_tester.ExpectBucketCount(
      kMultiWindowResizerShowTwoWindowsSnappedHistogramName, true, 0);
  histogram_tester.ExpectBucketCount(kMultiWindowResizerClickHistogramName,
                                     true, 0);
  histogram_tester.ExpectBucketCount(
      kMultiWindowResizerClickTwoWindowsSnappedHistogramName, true, 0);

  ui::test::EventGenerator* generator = GetEventGenerator();
  auto move_mouse_on_and_off_resizer_n_times = [&](int n, bool click) {
    for (int i = 0; i < n; i++) {
      generator->MoveMouseTo(w1->bounds().CenterPoint());
      EXPECT_TRUE(IsShowTimerRunning());
      EXPECT_TRUE(IsShowing());
      ShowNow();
      ASSERT_TRUE(resize_widget());

      if (click) {
        generator->MoveMouseTo(
            resize_widget()->GetWindowBoundsInScreen().CenterPoint());
        generator->ClickLeftButton();
        generator->ReleaseLeftButton();
      }

      generator->MoveMouseTo(w1->GetBoundsInRootWindow().CenterPoint());
      generator->ClickLeftButton();
      EXPECT_FALSE(IsShowing());
    }
  };

  // Verify that the multi-window resizer show and click histogram metrics are
  // recorded correctly.
  move_mouse_on_and_off_resizer_n_times(1, /*click=*/false);
  histogram_tester.ExpectBucketCount(kMultiWindowResizerShowHistogramName, true,
                                     1);

  move_mouse_on_and_off_resizer_n_times(2, true);
  histogram_tester.ExpectBucketCount(kMultiWindowResizerShowHistogramName, true,
                                     3);
  histogram_tester.ExpectBucketCount(kMultiWindowResizerClickHistogramName,
                                     true, 2);

  // Snap two windows
  WindowState* w1_state = WindowState::Get(w1.get());
  const WindowSnapWMEvent snap_left(WM_EVENT_SNAP_PRIMARY);
  w1_state->OnWMEvent(&snap_left);
  EXPECT_EQ(WindowStateType::kPrimarySnapped, w1_state->GetStateType());
  WindowState* w2_state = WindowState::Get(w2.get());
  const WindowSnapWMEvent snap_right(WM_EVENT_SNAP_SECONDARY);
  w2_state->OnWMEvent(&snap_right);
  EXPECT_EQ(WindowStateType::kSecondarySnapped, w2_state->GetStateType());

  // Verify that the multi-window resizer show and click histogram metrics with
  // two windows snapped are recorded correctly.
  move_mouse_on_and_off_resizer_n_times(5, /*click=*/false);
  histogram_tester.ExpectBucketCount(kMultiWindowResizerShowHistogramName, true,
                                     8);
  histogram_tester.ExpectBucketCount(
      kMultiWindowResizerShowTwoWindowsSnappedHistogramName, true, 5);

  move_mouse_on_and_off_resizer_n_times(7, true);
  histogram_tester.ExpectBucketCount(kMultiWindowResizerShowHistogramName, true,
                                     15);
  histogram_tester.ExpectBucketCount(
      kMultiWindowResizerShowTwoWindowsSnappedHistogramName, true, 12);
  histogram_tester.ExpectBucketCount(kMultiWindowResizerClickHistogramName,
                                     true, 9);
  histogram_tester.ExpectBucketCount(
      kMultiWindowResizerClickTwoWindowsSnappedHistogramName, true, 7);
}

}  // namespace ash
