// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_event_handler.h"

#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/window_factory.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/workspace_controller.h"
#include "ash/wm/workspace_controller_test_api.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/hit_test.h"
#include "ui/display/screen.h"
#include "ui/events/event_processor.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/window_move_client.h"

namespace ash {

namespace {

// Clicks |button| with |flags|.
void ClickButtonWithFlags(ui::test::EventGenerator* generator,
                          int button,
                          int flags) {
  gfx::Point location = generator->current_screen_location();
  ui::MouseEvent press(ui::ET_MOUSE_PRESSED, location, location,
                       ui::EventTimeForNow(), button | flags, button);
  generator->Dispatch(&press);
  ui::MouseEvent release(ui::ET_MOUSE_RELEASED, location, location,
                         ui::EventTimeForNow(), button | flags, button);
  generator->Dispatch(&release);
}

}  // namespace

class WorkspaceEventHandlerTest : public AshTestBase {
 public:
  WorkspaceEventHandlerTest() = default;
  ~WorkspaceEventHandlerTest() override = default;

 protected:
  aura::Window* CreateTestWindow(aura::WindowDelegate* delegate,
                                 const gfx::Rect& bounds) {
    aura::Window* window =
        window_factory::NewWindow(delegate, aura::client::WINDOW_TYPE_NORMAL)
            .release();
    window->Init(ui::LAYER_TEXTURED);
    ParentWindowInPrimaryRootWindow(window);
    window->SetBounds(bounds);
    window->Show();
    return window;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WorkspaceEventHandlerTest);
};

// Keeps track of the properties changed of a particular window.
class WindowPropertyObserver : public aura::WindowObserver {
 public:
  explicit WindowPropertyObserver(aura::Window* window) : window_(window) {
    window->AddObserver(this);
  }

  ~WindowPropertyObserver() override { window_->RemoveObserver(this); }

  bool DidPropertyChange(const void* property) const {
    return base::Contains(properties_changed_, property);
  }

 private:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override {
    properties_changed_.push_back(key);
  }

  aura::Window* window_;
  std::vector<const void*> properties_changed_;

  DISALLOW_COPY_AND_ASSIGN(WindowPropertyObserver);
};

TEST_F(WorkspaceEventHandlerTest, DoubleClickSingleAxisResizeEdge) {
  // Double clicking the vertical resize edge of a window should maximize it
  // vertically.
  gfx::Rect restored_bounds(10, 10, 50, 50);
  aura::test::TestWindowDelegate delegate;
  std::unique_ptr<aura::Window> window(
      CreateTestWindow(&delegate, restored_bounds));

  wm::ActivateWindow(window.get());

  gfx::Rect work_area = display::Screen::GetScreen()
                            ->GetDisplayNearestWindow(window.get())
                            .work_area();

  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     window.get());

  // Double-click the top resize edge.
  delegate.set_window_component(HTTOP);
  // On X a double click actually generates a drag between each press/release.
  // Explicitly trigger this path since we had bugs in dealing with it
  // correctly.
  generator.PressLeftButton();
  generator.ReleaseLeftButton();
  generator.set_flags(ui::EF_IS_DOUBLE_CLICK);
  generator.PressLeftButton();
  generator.MoveMouseTo(generator.current_screen_location(), 1);
  generator.ReleaseLeftButton();
  gfx::Rect bounds_in_screen = window->GetBoundsInScreen();
  EXPECT_EQ(restored_bounds.x(), bounds_in_screen.x());
  EXPECT_EQ(restored_bounds.width(), bounds_in_screen.width());
  EXPECT_EQ(work_area.y(), bounds_in_screen.y());
  EXPECT_EQ(work_area.height(), bounds_in_screen.height());

  WindowState* window_state = WindowState::Get(window.get());
  // Single-axis maximization is not considered real maximization.
  EXPECT_FALSE(window_state->IsMaximized());

  // Restore.
  generator.DoubleClickLeftButton();
  bounds_in_screen = window->GetBoundsInScreen();
  EXPECT_EQ(restored_bounds.ToString(), bounds_in_screen.ToString());
  // Note that it should not even be restored at this point, it should have
  // also cleared the restore rectangle.
  EXPECT_FALSE(window_state->HasRestoreBounds());

  // Double clicking the left resize edge should maximize horizontally.
  delegate.set_window_component(HTLEFT);
  generator.DoubleClickLeftButton();
  bounds_in_screen = window->GetBoundsInScreen();
  EXPECT_EQ(restored_bounds.y(), bounds_in_screen.y());
  EXPECT_EQ(restored_bounds.height(), bounds_in_screen.height());
  EXPECT_EQ(work_area.x(), bounds_in_screen.x());
  EXPECT_EQ(work_area.width(), bounds_in_screen.width());
  // Single-axis maximization is not considered real maximization.
  EXPECT_FALSE(window_state->IsMaximized());

  // Restore.
  generator.DoubleClickLeftButton();
  EXPECT_EQ(restored_bounds.ToString(), window->GetBoundsInScreen().ToString());

  // Verify the double clicking the resize edge works on 2nd display too.
  UpdateDisplay("200x200,400x300");
  gfx::Rect work_area2 = GetSecondaryDisplay().work_area();
  restored_bounds.SetRect(220, 20, 50, 50);
  window->SetBoundsInScreen(restored_bounds, GetSecondaryDisplay());
  aura::Window* second_root = Shell::GetAllRootWindows()[1];
  EXPECT_EQ(second_root, window->GetRootWindow());
  ui::test::EventGenerator generator2(second_root, window.get());
  // TODO(crbug.com/990589): Unit tests should be able to simulate mouse input
  // without having to call |CursorManager::SetDisplay|.
  Shell::Get()->cursor_manager()->SetDisplay(
      display::Screen::GetScreen()->GetDisplayNearestWindow(second_root));

  // Y-axis maximization.
  delegate.set_window_component(HTTOP);
  generator2.PressLeftButton();
  generator2.ReleaseLeftButton();
  generator2.set_flags(ui::EF_IS_DOUBLE_CLICK);
  generator2.PressLeftButton();
  generator2.MoveMouseTo(generator2.current_screen_location(), 1);
  generator2.ReleaseLeftButton();
  bounds_in_screen = window->GetBoundsInScreen();
  EXPECT_EQ(restored_bounds.x(), bounds_in_screen.x());
  EXPECT_EQ(restored_bounds.width(), bounds_in_screen.width());
  EXPECT_EQ(work_area2.y(), bounds_in_screen.y());
  EXPECT_EQ(work_area2.height(), bounds_in_screen.height());
  EXPECT_FALSE(window_state->IsMaximized());

  // Restore.
  generator2.DoubleClickLeftButton();
  EXPECT_EQ(restored_bounds.ToString(), window->GetBoundsInScreen().ToString());

  // X-axis maximization.
  delegate.set_window_component(HTLEFT);
  generator2.DoubleClickLeftButton();
  bounds_in_screen = window->GetBoundsInScreen();
  EXPECT_EQ(restored_bounds.y(), bounds_in_screen.y());
  EXPECT_EQ(restored_bounds.height(), bounds_in_screen.height());
  EXPECT_EQ(work_area2.x(), bounds_in_screen.x());
  EXPECT_EQ(work_area2.width(), bounds_in_screen.width());
  EXPECT_FALSE(window_state->IsMaximized());

  // Restore.
  generator2.DoubleClickLeftButton();
  EXPECT_EQ(restored_bounds.ToString(), window->GetBoundsInScreen().ToString());
}

// Tests the behavior when double clicking the border of a side snapped window.
TEST_F(WorkspaceEventHandlerTest, DoubleClickSingleAxisWhenSideSnapped) {
  gfx::Rect restored_bounds(10, 10, 50, 50);
  aura::test::TestWindowDelegate delegate;
  std::unique_ptr<aura::Window> window(
      CreateTestWindow(&delegate, restored_bounds));

  gfx::Rect work_area_in_screen = display::Screen::GetScreen()
                                      ->GetDisplayNearestWindow(window.get())
                                      .work_area();

  WindowState* window_state = WindowState::Get(window.get());
  const WMEvent snap_event(WM_EVENT_SNAP_LEFT);
  window_state->OnWMEvent(&snap_event);

  gfx::Rect snapped_bounds_in_screen = window->GetBoundsInScreen();
  EXPECT_EQ(work_area_in_screen.x(), snapped_bounds_in_screen.x());
  EXPECT_EQ(work_area_in_screen.y(), snapped_bounds_in_screen.y());
  EXPECT_GT(work_area_in_screen.width(), snapped_bounds_in_screen.width());
  EXPECT_EQ(work_area_in_screen.height(), snapped_bounds_in_screen.height());

  // Double clicking the top border should not do anything for side snapped
  // windows. (They already take up the entire workspace height and reverting
  // to the restored bounds would be weird).
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     window.get());
  delegate.set_window_component(HTTOP);
  generator.DoubleClickLeftButton();
  EXPECT_EQ(WindowStateType::kLeftSnapped, window_state->GetStateType());
  EXPECT_EQ(snapped_bounds_in_screen.ToString(),
            window->GetBoundsInScreen().ToString());

  // Double clicking the right border should exit the side snapped state and
  // make the window take up the entire work area.
  delegate.set_window_component(HTRIGHT);
  generator.DoubleClickLeftButton();
  EXPECT_TRUE(window_state->IsNormalStateType());
  EXPECT_EQ(work_area_in_screen.ToString(),
            window->GetBoundsInScreen().ToString());
}

TEST_F(WorkspaceEventHandlerTest,
       DoubleClickSingleAxisDoesntResizeVerticalEdgeIfConstrained) {
  gfx::Rect restored_bounds(10, 10, 50, 50);
  aura::test::TestWindowDelegate delegate;
  std::unique_ptr<aura::Window> window(
      CreateTestWindow(&delegate, restored_bounds));

  wm::ActivateWindow(window.get());

  delegate.set_maximum_size(gfx::Size(0, 100));

  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     window.get());
  // Double-click the top resize edge.
  delegate.set_window_component(HTTOP);
  generator.DoubleClickLeftButton();

  // The size of the window should be unchanged.
  EXPECT_EQ(restored_bounds.y(), window->bounds().y());
  EXPECT_EQ(restored_bounds.height(), window->bounds().height());
}

TEST_F(WorkspaceEventHandlerTest,
       DoubleClickSingleAxisDoesntResizeHorizontalEdgeIfConstrained) {
  gfx::Rect restored_bounds(10, 10, 50, 50);
  aura::test::TestWindowDelegate delegate;
  std::unique_ptr<aura::Window> window(
      CreateTestWindow(&delegate, restored_bounds));

  wm::ActivateWindow(window.get());

  delegate.set_maximum_size(gfx::Size(100, 0));

  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     window.get());
  // Double-click the top resize edge.
  delegate.set_window_component(HTRIGHT);
  generator.DoubleClickLeftButton();

  // The size of the window should be unchanged.
  EXPECT_EQ(restored_bounds.x(), window->bounds().x());
  EXPECT_EQ(restored_bounds.width(), window->bounds().width());
}

TEST_F(WorkspaceEventHandlerTest,
       DoubleClickOrTapWithModalChildDoesntMaximize) {
  aura::test::TestWindowDelegate delegate1;
  aura::test::TestWindowDelegate delegate2;
  std::unique_ptr<aura::Window> window(
      CreateTestWindow(&delegate1, gfx::Rect(10, 20, 30, 40)));
  std::unique_ptr<aura::Window> child(
      CreateTestWindow(&delegate2, gfx::Rect(0, 0, 1, 1)));
  window->SetProperty(aura::client::kResizeBehaviorKey,
                      aura::client::kResizeBehaviorCanMaximize);
  delegate1.set_window_component(HTCAPTION);

  child->SetProperty(aura::client::kModalKey, ui::MODAL_TYPE_WINDOW);
  ::wm::AddTransientChild(window.get(), child.get());

  WindowState* window_state = WindowState::Get(window.get());
  EXPECT_FALSE(window_state->IsMaximized());
  aura::Window* root = Shell::GetPrimaryRootWindow();
  ui::test::EventGenerator generator(root, window.get());
  generator.DoubleClickLeftButton();
  EXPECT_EQ("10,20 30x40", window->bounds().ToString());
  EXPECT_FALSE(window_state->IsMaximized());

  generator.GestureTapAt(gfx::Point(25, 25));
  generator.GestureTapAt(gfx::Point(25, 25));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("10,20 30x40", window->bounds().ToString());
  EXPECT_FALSE(window_state->IsMaximized());
}

// Test the behavior as a result of double clicking the window header.
TEST_F(WorkspaceEventHandlerTest, DoubleClickCaptionTogglesMaximize) {
  aura::test::TestWindowDelegate delegate;
  std::unique_ptr<aura::Window> window(
      CreateTestWindow(&delegate, gfx::Rect(1, 2, 30, 40)));
  window->SetProperty(aura::client::kResizeBehaviorKey,
                      aura::client::kResizeBehaviorCanMaximize);

  WindowState* window_state = WindowState::Get(window.get());
  gfx::Rect restore_bounds = window->bounds();
  gfx::Rect work_area_in_parent =
      screen_util::GetDisplayWorkAreaBoundsInParent(window.get());

  EXPECT_FALSE(window_state->IsMaximized());

  // 1) Double clicking a normal window should maximize.
  delegate.set_window_component(HTCAPTION);
  aura::Window* root = Shell::GetPrimaryRootWindow();
  ui::test::EventGenerator generator(root, window.get());
  generator.DoubleClickLeftButton();
  EXPECT_NE(restore_bounds.ToString(), window->bounds().ToString());
  EXPECT_TRUE(window_state->IsMaximized());

  generator.DoubleClickLeftButton();
  EXPECT_TRUE(window_state->IsNormalStateType());
  EXPECT_EQ(restore_bounds.ToString(), window->bounds().ToString());

  // 2) Double clicking a horizontally maximized window should maximize.
  delegate.set_window_component(HTLEFT);
  generator.DoubleClickLeftButton();
  EXPECT_TRUE(window_state->IsNormalStateType());
  EXPECT_EQ(work_area_in_parent.x(), window->bounds().x());
  EXPECT_EQ(restore_bounds.y(), window->bounds().y());
  EXPECT_EQ(work_area_in_parent.width(), window->bounds().width());
  EXPECT_EQ(restore_bounds.height(), window->bounds().height());

  delegate.set_window_component(HTCAPTION);
  generator.DoubleClickLeftButton();
  EXPECT_TRUE(window_state->IsMaximized());

  generator.DoubleClickLeftButton();
  EXPECT_TRUE(window_state->IsNormalStateType());
  EXPECT_EQ(restore_bounds.ToString(), window->bounds().ToString());

  // 3) Double clicking a snapped window should maximize.
  const WMEvent snap_event(WM_EVENT_SNAP_LEFT);
  window_state->OnWMEvent(&snap_event);
  EXPECT_TRUE(window_state->IsSnapped());
  generator.MoveMouseTo(window->GetBoundsInRootWindow().CenterPoint());
  generator.DoubleClickLeftButton();
  EXPECT_TRUE(window_state->IsMaximized());

  generator.DoubleClickLeftButton();
  EXPECT_TRUE(window_state->IsNormalStateType());
  EXPECT_EQ(restore_bounds.ToString(), window->bounds().ToString());
}

// Test that double clicking the middle button on the window header does not
// toggle the maximized state.
TEST_F(WorkspaceEventHandlerTest,
       DoubleClickMiddleButtonDoesNotToggleMaximize) {
  aura::test::TestWindowDelegate delegate;
  std::unique_ptr<aura::Window> window(
      CreateTestWindow(&delegate, gfx::Rect(1, 2, 30, 40)));
  window->SetProperty(aura::client::kResizeBehaviorKey,
                      aura::client::kResizeBehaviorCanMaximize);
  delegate.set_window_component(HTCAPTION);
  aura::Window* root = Shell::GetPrimaryRootWindow();
  ui::test::EventGenerator generator(root, window.get());

  WindowPropertyObserver observer(window.get());
  ClickButtonWithFlags(&generator, ui::EF_MIDDLE_MOUSE_BUTTON, ui::EF_NONE);
  ClickButtonWithFlags(&generator, ui::EF_MIDDLE_MOUSE_BUTTON,
                       ui::EF_IS_DOUBLE_CLICK);

  EXPECT_FALSE(WindowState::Get(window.get())->IsMaximized());
  EXPECT_EQ("1,2 30x40", window->bounds().ToString());
  EXPECT_FALSE(observer.DidPropertyChange(aura::client::kShowStateKey));
}

TEST_F(WorkspaceEventHandlerTest, DoubleTapCaptionTogglesMaximize) {
  aura::test::TestWindowDelegate delegate;
  gfx::Rect bounds(10, 20, 30, 40);
  std::unique_ptr<aura::Window> window(CreateTestWindow(&delegate, bounds));
  window->SetProperty(aura::client::kResizeBehaviorKey,
                      aura::client::kResizeBehaviorCanMaximize);
  delegate.set_window_component(HTCAPTION);

  WindowState* window_state = WindowState::Get(window.get());
  EXPECT_FALSE(window_state->IsMaximized());
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     window.get());
  generator.GestureTapAt(gfx::Point(25, 25));
  generator.GestureTapAt(gfx::Point(25, 25));
  base::RunLoop().RunUntilIdle();
  EXPECT_NE(bounds.ToString(), window->bounds().ToString());
  EXPECT_TRUE(window_state->IsMaximized());

  generator.GestureTapAt(gfx::Point(5, 5));
  generator.GestureTapAt(gfx::Point(10, 10));

  EXPECT_FALSE(window_state->IsMaximized());
  EXPECT_EQ(bounds.ToString(), window->bounds().ToString());
}

// Verifies deleting the window while dragging doesn't crash.
TEST_F(WorkspaceEventHandlerTest, DeleteWhenDragging) {
  // Create a large window in the background. This is necessary so that when we
  // delete |window| WorkspaceEventHandler is still the active event handler.
  aura::test::TestWindowDelegate delegate2;
  std::unique_ptr<aura::Window> window2(
      CreateTestWindow(&delegate2, gfx::Rect(0, 0, 500, 500)));

  aura::test::TestWindowDelegate delegate;
  const gfx::Rect bounds(10, 20, 30, 40);
  std::unique_ptr<aura::Window> window(CreateTestWindow(&delegate, bounds));
  delegate.set_window_component(HTCAPTION);
  ui::test::EventGenerator generator(window->GetRootWindow());
  generator.MoveMouseToCenterOf(window.get());
  generator.PressLeftButton();
  generator.MoveMouseTo(generator.current_screen_location() +
                        gfx::Vector2d(50, 50));
  DCHECK_NE(bounds.origin().ToString(), window->bounds().origin().ToString());
  window.reset();
  generator.MoveMouseTo(generator.current_screen_location() +
                        gfx::Vector2d(50, 50));
}

// Verifies deleting the window while in a run loop doesn't crash.
TEST_F(WorkspaceEventHandlerTest, DeleteWhileInRunLoop) {
  aura::test::TestWindowDelegate delegate;
  const gfx::Rect bounds(10, 20, 30, 40);
  std::unique_ptr<aura::Window> window(CreateTestWindow(&delegate, bounds));
  delegate.set_window_component(HTCAPTION);

  ASSERT_TRUE(::wm::GetWindowMoveClient(window->GetRootWindow()));
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, window.get());
  ::wm::GetWindowMoveClient(window->GetRootWindow())
      ->RunMoveLoop(window.release(), gfx::Vector2d(),
                    ::wm::WINDOW_MOVE_SOURCE_MOUSE);
}

// Verifies that double clicking in the header does not maximize if the target
// component has changed.
TEST_F(WorkspaceEventHandlerTest,
       DoubleClickTwoDifferentTargetsDoesntMaximize) {
  aura::test::TestWindowDelegate delegate;
  std::unique_ptr<aura::Window> window(
      CreateTestWindow(&delegate, gfx::Rect(1, 2, 30, 40)));
  window->SetProperty(aura::client::kResizeBehaviorKey,
                      aura::client::kResizeBehaviorCanMaximize);

  WindowState* window_state = WindowState::Get(window.get());
  EXPECT_FALSE(window_state->IsMaximized());

  // First click will go to a client
  delegate.set_window_component(HTCLIENT);
  aura::Window* root = Shell::GetPrimaryRootWindow();
  ui::test::EventGenerator generator(root, window.get());
  generator.ClickLeftButton();
  EXPECT_FALSE(window_state->IsMaximized());

  // Second click will go to the header
  delegate.set_window_component(HTCAPTION);
  ClickButtonWithFlags(&generator, ui::EF_LEFT_MOUSE_BUTTON,
                       ui::EF_IS_DOUBLE_CLICK);
  EXPECT_FALSE(window_state->IsMaximized());
}

// Verifies that double tapping in the header does not maximize if the target
// component has changed.
TEST_F(WorkspaceEventHandlerTest, DoubleTapTwoDifferentTargetsDoesntMaximize) {
  aura::test::TestWindowDelegate delegate;
  std::unique_ptr<aura::Window> window(
      CreateTestWindow(&delegate, gfx::Rect(1, 2, 30, 40)));
  window->SetProperty(aura::client::kResizeBehaviorKey,
                      aura::client::kResizeBehaviorCanMaximize);

  WindowState* window_state = WindowState::Get(window.get());
  EXPECT_FALSE(window_state->IsMaximized());

  // First tap will go to a client
  delegate.set_window_component(HTCLIENT);
  aura::Window* root = Shell::GetPrimaryRootWindow();
  ui::test::EventGenerator generator(root, window.get());
  generator.GestureTapAt(gfx::Point(25, 25));
  EXPECT_FALSE(window_state->IsMaximized());

  // Second tap will go to the header
  delegate.set_window_component(HTCAPTION);
  generator.GestureTapAt(gfx::Point(25, 25));
  EXPECT_FALSE(window_state->IsMaximized());
}

TEST_F(WorkspaceEventHandlerTest, RightClickDuringDoubleClickDoesntMaximize) {
  aura::test::TestWindowDelegate delegate;
  std::unique_ptr<aura::Window> window(
      CreateTestWindow(&delegate, gfx::Rect(1, 2, 30, 40)));
  window->SetProperty(aura::client::kResizeBehaviorKey,
                      aura::client::kResizeBehaviorCanMaximize);

  WindowState* window_state = WindowState::Get(window.get());
  EXPECT_FALSE(window_state->IsMaximized());

  // First click will go to a client
  delegate.set_window_component(HTCLIENT);
  aura::Window* root = Shell::GetPrimaryRootWindow();
  ui::test::EventGenerator generator(root, window.get());
  generator.ClickLeftButton();
  EXPECT_FALSE(window_state->IsMaximized());

  // Second click will go to the header
  delegate.set_window_component(HTCAPTION);
  generator.PressRightButton();
  generator.ReleaseRightButton();
  EXPECT_FALSE(window_state->IsMaximized());
  ClickButtonWithFlags(&generator, ui::EF_LEFT_MOUSE_BUTTON,
                       ui::EF_IS_DOUBLE_CLICK);
  EXPECT_FALSE(window_state->IsMaximized());
}

}  // namespace ash
