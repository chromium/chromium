// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/toplevel_window_event_handler.h"

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/overview/overview_button_tray.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_window_builder.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/resize_shadow.h"
#include "ash/wm/resize_shadow_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/workspace_controller.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_observer.h"
#include "ui/base/hit_test.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/display/display_layout_builder.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/event.h"
#include "ui/events/test/event_generator.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/window_move_client.h"

namespace ash {

namespace {

using aura::test::TestWindowDelegate;
using ::chromeos::WindowStateType;

aura::test::TestWindowDelegate* CreateTestWindowDelegate(int hittest_code) {
  auto* delegate =
      aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate();
  delegate->set_window_component(hittest_code);
  return delegate;
}

class ResizeLoopWindowObserver : public aura::WindowObserver {
 public:
  explicit ResizeLoopWindowObserver(aura::Window* w) : window_(w) {
    window_->AddObserver(this);
  }

  ResizeLoopWindowObserver(const ResizeLoopWindowObserver&) = delete;
  ResizeLoopWindowObserver& operator=(const ResizeLoopWindowObserver&) = delete;

  ~ResizeLoopWindowObserver() override {
    if (window_) {
      window_->RemoveObserver(this);
    }
  }

  bool in_resize_loop() const { return in_resize_loop_; }

  // aura::WindowObserver:
  void OnResizeLoopStarted(aura::Window* window) override {
    EXPECT_FALSE(in_resize_loop_);
    in_resize_loop_ = true;
  }
  void OnResizeLoopEnded(aura::Window* window) override {
    EXPECT_TRUE(in_resize_loop_);
    in_resize_loop_ = false;
  }
  void OnWindowDestroying(aura::Window* window) override {
    window_->RemoveObserver(this);
    window_ = nullptr;
  }

 private:
  raw_ptr<aura::Window> window_;
  bool in_resize_loop_ = false;
};

class ToplevelWindowEventHandlerTest : public AshTestBase {
 public:
  ToplevelWindowEventHandlerTest() = default;

  ToplevelWindowEventHandlerTest(const ToplevelWindowEventHandlerTest&) =
      delete;
  ToplevelWindowEventHandlerTest& operator=(
      const ToplevelWindowEventHandlerTest&) = delete;

  ~ToplevelWindowEventHandlerTest() override = default;

 protected:
  void DragFromCenterBy(aura::Window* window, int dx, int dy) {
    ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(), window);
    generator.DragMouseBy(dx, dy);
  }

  void TouchDragFromCenterBy(aura::Window* window, int dx, int dy) {
    ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(), window);
    generator.PressMoveAndReleaseTouchBy(dx, dy);
  }

  std::unique_ptr<ToplevelWindowEventHandler> handler_;
};

aura::Window* CreateWindow(int hittest_code) {
  aura::Window* w1 = new aura::Window(CreateTestWindowDelegate(hittest_code),
                                      aura::client::WINDOW_TYPE_NORMAL);
  w1->SetId(1);
  w1->Init(ui::LAYER_TEXTURED);
  aura::Window* parent = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(), desks_util::GetActiveDeskContainerId());
  parent->AddChild(w1);
  w1->SetBounds(gfx::Rect(0, 0, 100, 100));
  w1->Show();
  return w1;
}

}  // namespace

TEST_F(ToplevelWindowEventHandlerTest, Caption) {
  std::unique_ptr<aura::Window> w1(CreateWindow(HTCAPTION));
  gfx::Size size = w1->bounds().size();
  DragFromCenterBy(w1.get(), 100, 100);
  // Position should have been offset by 100,100.
  EXPECT_EQ("100,100", w1->bounds().origin().ToString());
  // Size should not have.
  EXPECT_EQ(size.ToString(), w1->bounds().size().ToString());

  TouchDragFromCenterBy(w1.get(), 100, 100);
  // Position should have been offset by 100,100.
  EXPECT_EQ("200,200", w1->bounds().origin().ToString());
  // Size should not have.
  EXPECT_EQ(size.ToString(), w1->bounds().size().ToString());
}

namespace {

void ContinueAndCompleteDrag(ui::test::EventGenerator* generator,
                             WindowState* window_state,
                             aura::Window* window) {
  ASSERT_TRUE(window->HasCapture());
  ASSERT_FALSE(window_state->GetWindowPositionManaged());
  generator->DragMouseBy(100, 100);
  generator->ReleaseLeftButton();
}

}  // namespace

// Tests dragging restores expected window position auto manage property
// correctly.
TEST_F(ToplevelWindowEventHandlerTest, WindowPositionAutoManagement) {
  std::unique_ptr<aura::Window> w1(CreateWindow(HTNOWHERE));
  const gfx::Size size = w1->bounds().size();
  WindowState* window_state = WindowState::Get(w1.get());
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(), w1.get());

  // Explicitly enable window position auto management, and expect it to be
  // restored after drag completes.
  window_state->SetWindowPositionManaged(true);
  generator.PressLeftButton();
  ::wm::WindowMoveClient* move_client =
      ::wm::GetWindowMoveClient(w1->GetRootWindow());
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ContinueAndCompleteDrag, base::Unretained(&generator),
                     base::Unretained(window_state),
                     base::Unretained(w1.get())));
  EXPECT_EQ(::wm::MOVE_SUCCESSFUL,
            move_client->RunMoveLoop(w1.get(), gfx::Vector2d(100, 100),
                                     ::wm::WINDOW_MOVE_SOURCE_MOUSE));
  // Window position auto manage property should be restored to true.
  EXPECT_TRUE(window_state->GetWindowPositionManaged());
  // Position should have been offset by 100,100.
  EXPECT_EQ("100,100", w1->bounds().origin().ToString());
  // Size should remain the same.
  EXPECT_EQ(size.ToString(), w1->bounds().size().ToString());

  // Explicitly disable window position auto management, and expect it to be
  // restored after drag completes.
  window_state->SetWindowPositionManaged(false);
  generator.PressLeftButton();
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ContinueAndCompleteDrag, base::Unretained(&generator),
                     base::Unretained(window_state),
                     base::Unretained(w1.get())));
  EXPECT_EQ(::wm::MOVE_SUCCESSFUL,
            move_client->RunMoveLoop(w1.get(), gfx::Vector2d(100, 100),
                                     ::wm::WINDOW_MOVE_SOURCE_MOUSE));
  // Window position auto manage property should be restored to true.
  EXPECT_FALSE(window_state->GetWindowPositionManaged());
  // Position should have been offset by 100,100.
  EXPECT_EQ("200,200", w1->bounds().origin().ToString());
  // Size should remain the same.
  EXPECT_EQ(size.ToString(), w1->bounds().size().ToString());
}

TEST_F(ToplevelWindowEventHandlerTest, BottomRight) {
  std::unique_ptr<aura::Window> w1(CreateWindow(HTBOTTOMRIGHT));
  gfx::Point position = w1->bounds().origin();
  DragFromCenterBy(w1.get(), 100, 100);
  // Position should not have changed.
  EXPECT_EQ(position.ToString(), w1->bounds().origin().ToString());
  // Size should have increased by 100,100.
  EXPECT_EQ(gfx::Size(200, 200).ToString(), w1->bounds().size().ToString());
}

TEST_F(ToplevelWindowEventHandlerTest, GrowBox) {
  std::unique_ptr<aura::Window> w1(CreateWindow(HTGROWBOX));
  TestWindowDelegate* window_delegate =
      static_cast<TestWindowDelegate*>(w1->delegate());
  window_delegate->set_minimum_size(gfx::Size(40, 40));

  gfx::Point position = w1->bounds().origin();
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  generator.MoveMouseToCenterOf(w1.get());
  generator.DragMouseBy(100, 100);
  // Position should not have changed.
  EXPECT_EQ(position.ToString(), w1->bounds().origin().ToString());
  // Size should have increased by 100,100.
  EXPECT_EQ(gfx::Size(200, 200).ToString(), w1->bounds().size().ToString());

  // Shrink the window by (-100, -100).
  generator.DragMouseBy(-100, -100);
  // Position should not have changed.
  EXPECT_EQ(position.ToString(), w1->bounds().origin().ToString());
  // Size should have decreased by 100,100.
  EXPECT_EQ(gfx::Size(100, 100).ToString(), w1->bounds().size().ToString());

  // Enforce minimum size.
  generator.DragMouseBy(-60, -60);
  EXPECT_EQ(position.ToString(), w1->bounds().origin().ToString());
  EXPECT_EQ(gfx::Size(40, 40).ToString(), w1->bounds().size().ToString());
}

TEST_F(ToplevelWindowEventHandlerTest, Right) {
  std::unique_ptr<aura::Window> w1(CreateWindow(HTRIGHT));
  gfx::Point position = w1->bounds().origin();
  DragFromCenterBy(w1.get(), 100, 100);
  // Position should not have changed.
  EXPECT_EQ(position.ToString(), w1->bounds().origin().ToString());
  // Size should have increased by 100,0.
  EXPECT_EQ(gfx::Size(200, 100).ToString(), w1->bounds().size().ToString());
}

TEST_F(ToplevelWindowEventHandlerTest, Bottom) {
  std::unique_ptr<aura::Window> w1(CreateWindow(HTBOTTOM));
  gfx::Point position = w1->bounds().origin();
  DragFromCenterBy(w1.get(), 100, 100);
  // Position should not have changed.
  EXPECT_EQ(position.ToString(), w1->bounds().origin().ToString());
  // Size should have increased by 0,100.
  EXPECT_EQ(gfx::Size(100, 200).ToString(), w1->bounds().size().ToString());
}

TEST_F(ToplevelWindowEventHandlerTest, TopRight) {
  std::unique_ptr<aura::Window> w1(CreateWindow(HTTOPRIGHT));
  DragFromCenterBy(w1.get(), -50, 50);
  // Position should have been offset by 0,50.
  EXPECT_EQ(gfx::Point(0, 50).ToString(), w1->bounds().origin().ToString());
  // Size should have decreased by 50,50.
  EXPECT_EQ(gfx::Size(50, 50).ToString(), w1->bounds().size().ToString());
}

TEST_F(ToplevelWindowEventHandlerTest, Top) {
  std::unique_ptr<aura::Window> w1(CreateWindow(HTTOP));
  DragFromCenterBy(w1.get(), 50, 50);
  // Position should have been offset by 0,50.
  EXPECT_EQ(gfx::Point(0, 50).ToString(), w1->bounds().origin().ToString());
  // Size should have decreased by 0,50.
  EXPECT_EQ(gfx::Size(100, 50).ToString(), w1->bounds().size().ToString());
}

TEST_F(ToplevelWindowEventHandlerTest, Left) {
  std::unique_ptr<aura::Window> w1(CreateWindow(HTLEFT));
  DragFromCenterBy(w1.get(), 50, 50);
  // Position should have been offset by 50,0.
  EXPECT_EQ(gfx::Point(50, 0).ToString(), w1->bounds().origin().ToString());
  // Size should have decreased by 50,0.
  EXPECT_EQ(gfx::Size(50, 100).ToString(), w1->bounds().size().ToString());
}

TEST_F(ToplevelWindowEventHandlerTest, BottomLeft) {
  std::unique_ptr<aura::Window> w1(CreateWindow(HTBOTTOMLEFT));
  DragFromCenterBy(w1.get(), 50, -50);
  // Position should have been offset by 50,0.
  EXPECT_EQ(gfx::Point(50, 0).ToString(), w1->bounds().origin().ToString());
  // Size should have decreased by 50,50.
  EXPECT_EQ(gfx::Size(50, 50).ToString(), w1->bounds().size().ToString());
}

TEST_F(ToplevelWindowEventHandlerTest, TopLeft) {
  std::unique_ptr<aura::Window> w1(CreateWindow(HTTOPLEFT));
  DragFromCenterBy(w1.get(), 50, 50);
  // Position should have been offset by 50,50.
  EXPECT_EQ(gfx::Point(50, 50).ToString(), w1->bounds().origin().ToString());
  // Size should have decreased by 50,50.
  EXPECT_EQ(gfx::Size(50, 50).ToString(), w1->bounds().size().ToString());
}

TEST_F(ToplevelWindowEventHandlerTest, Client) {
  std::unique_ptr<aura::Window> w1(CreateWindow(HTCLIENT));
  gfx::Rect bounds = w1->bounds();
  DragFromCenterBy(w1.get(), 100, 100);
  // Neither position nor size should have changed.
  EXPECT_EQ(bounds.ToString(), w1->bounds().ToString());
}

TEST_F(ToplevelWindowEventHandlerTest, LeftPastMinimum) {
  std::unique_ptr<aura::Window> w1(CreateWindow(HTLEFT));
  TestWindowDelegate* window_delegate =
      static_cast<TestWindowDelegate*>(w1->delegate());
  window_delegate->set_minimum_size(gfx::Size(40, 40));

  // Simulate a large left-to-right drag.  Window width should be clamped to
  // minimum and position change should be limited as well.
  DragFromCenterBy(w1.get(), 333, 0);
  EXPECT_EQ(gfx::Point(60, 0).ToString(), w1->bounds().origin().ToString());
  EXPECT_EQ(gfx::Size(40, 100).ToString(), w1->bounds().size().ToString());
}

TEST_F(ToplevelWindowEventHandlerTest, RightPastMinimum) {
  std::unique_ptr<aura::Window> w1(CreateWindow(HTRIGHT));
  TestWindowDelegate* window_delegate =
      static_cast<TestWindowDelegate*>(w1->delegate());
  window_delegate->set_minimum_size(gfx::Size(40, 40));
  gfx::Point position = w1->bounds().origin();

  // Simulate a large right-to-left drag.  Window width should be clamped to
  // minimum and position should not change.
  DragFromCenterBy(w1.get(), -333, 0);
  EXPECT_EQ(position.ToString(), w1->bounds().origin().ToString());
  EXPECT_EQ(gfx::Size(40, 100).ToString(), w1->bounds().size().ToString());
}

TEST_F(ToplevelWindowEventHandlerTest, TopLeftPastMinimum) {
  std::unique_ptr<aura::Window> w1(CreateWindow(HTTOPLEFT));
  TestWindowDelegate* window_delegate =
      static_cast<TestWindowDelegate*>(w1->delegate());
  window_delegate->set_minimum_size(gfx::Size(40, 40));

  // Simulate a large top-left to bottom-right drag.  Window width should be
  // clamped to minimum and position should be limited.
  DragFromCenterBy(w1.get(), 333, 444);
  EXPECT_EQ(gfx::Point(60, 60).ToString(), w1->bounds().origin().ToString());
  EXPECT_EQ(gfx::Size(40, 40).ToString(), w1->bounds().size().ToString());
}

TEST_F(ToplevelWindowEventHandlerTest, TopRightPastMinimum) {
  std::unique_ptr<aura::Window> w1(CreateWindow(HTTOPRIGHT));
  TestWindowDelegate* window_delegate =
      static_cast<TestWindowDelegate*>(w1->delegate());
  window_delegate->set_minimum_size(gfx::Size(40, 40));

  // Simulate a large top-right to bottom-left drag.  Window size should be
  // clamped to minimum, x position should not change, and y position should
  // be clamped.
  DragFromCenterBy(w1.get(), -333, 444);
  EXPECT_EQ(gfx::Point(0, 60).ToString(), w1->bounds().origin().ToString());
  EXPECT_EQ(gfx::Size(40, 40).ToString(), w1->bounds().size().ToString());
}

TEST_F(ToplevelWindowEventHandlerTest, BottomLeftPastMinimum) {
  std::unique_ptr<aura::Window> w1(CreateWindow(HTBOTTOMLEFT));
  TestWindowDelegate* window_delegate =
      static_cast<TestWindowDelegate*>(w1->delegate());
  window_delegate->set_minimum_size(gfx::Size(40, 40));

  // Simulate a large bottom-left to top-right drag.  Window size should be
  // clamped to minimum, x position should be clamped, and y position should
  // not change.
  DragFromCenterBy(w1.get(), 333, -444);
  EXPECT_EQ(gfx::Point(60, 0).ToString(), w1->bounds().origin().ToString());
  EXPECT_EQ(gfx::Size(40, 40).ToString(), w1->bounds().size().ToString());
}

TEST_F(ToplevelWindowEventHandlerTest, BottomRightPastMinimum) {
  std::unique_ptr<aura::Window> w1(CreateWindow(HTBOTTOMRIGHT));
  TestWindowDelegate* window_delegate =
      static_cast<TestWindowDelegate*>(w1->delegate());
  window_delegate->set_minimum_size(gfx::Size(40, 40));
  gfx::Point position = w1->bounds().origin();

  // Simulate a large bottom-right to top-left drag.  Window size should be
  // clamped to minimum and position should not change.
  DragFromCenterBy(w1.get(), -333, -444);
  EXPECT_EQ(position.ToString(), w1->bounds().origin().ToString());
  EXPECT_EQ(gfx::Size(40, 40).ToString(), w1->bounds().size().ToString());
}

TEST_F(ToplevelWindowEventHandlerTest, BottomRightWorkArea) {
  std::unique_ptr<aura::Window> target(CreateWindow(HTBOTTOMRIGHT));
  gfx::Rect work_area = display::Screen::GetScreen()
                            ->GetDisplayNearestWindow(target.get())
                            .work_area();
  gfx::Point position = target->bounds().origin();
  // Drag further than work_area bottom.
  DragFromCenterBy(target.get(), 100, work_area.height());
  // Position should not have changed.
  EXPECT_EQ(position.ToString(), target->bounds().origin().ToString());
  // Size should have increased by 100, work_area.height() - target->bounds.y()
  EXPECT_EQ(
      gfx::Size(200, work_area.height() - target->bounds().y()).ToString(),
      target->bounds().size().ToString());
}

TEST_F(ToplevelWindowEventHandlerTest, BottomLeftWorkArea) {
  std::unique_ptr<aura::Window> target(CreateWindow(HTBOTTOMLEFT));
  gfx::Rect work_area = display::Screen::GetScreen()
                            ->GetDisplayNearestWindow(target.get())
                            .work_area();
  gfx::Point position = target->bounds().origin();
  // Drag further than work_area bottom.
  DragFromCenterBy(target.get(), -30, work_area.height());
  // origin is now at 70, 100.
  EXPECT_EQ(position.x() - 30, target->bounds().x());
  EXPECT_EQ(position.y(), target->bounds().y());
  // Size should have increased by 30, work_area.height() - target->bounds.y()
  EXPECT_EQ(
      gfx::Size(130, work_area.height() - target->bounds().y()).ToString(),
      target->bounds().size().ToString());
}

TEST_F(ToplevelWindowEventHandlerTest, BottomWorkArea) {
  std::unique_ptr<aura::Window> target(CreateWindow(HTBOTTOM));
  gfx::Rect work_area = display::Screen::GetScreen()
                            ->GetDisplayNearestWindow(target.get())
                            .work_area();
  gfx::Point position = target->bounds().origin();
  // Drag further than work_area bottom.
  DragFromCenterBy(target.get(), 0, work_area.height());
  // Position should not have changed.
  EXPECT_EQ(position.ToString(), target->bounds().origin().ToString());
  // Size should have increased by 0, work_area.height() - target->bounds.y()
  EXPECT_EQ(
      gfx::Size(100, work_area.height() - target->bounds().y()).ToString(),
      target->bounds().size().ToString());
}

TEST_F(ToplevelWindowEventHandlerTest, DontDragIfModalChild) {
  std::unique_ptr<aura::Window> w1(CreateWindow(HTCAPTION));
  std::unique_ptr<aura::Window> w2(CreateWindow(HTCAPTION));
  w2->SetBounds(gfx::Rect(100, 0, 100, 100));
  w2->SetProperty(aura::client::kModalKey, ui::mojom::ModalType::kWindow);
  ::wm::AddTransientChild(w1.get(), w2.get());
  gfx::Size size = w1->bounds().size();

  // Attempt to drag w1, position and size should not change because w1 has a
  // modal child.
  DragFromCenterBy(w1.get(), 100, 100);
  EXPECT_EQ("0,0", w1->bounds().origin().ToString());
  EXPECT_EQ(size.ToString(), w1->bounds().size().ToString());

  TouchDragFromCenterBy(w1.get(), 100, 100);
  EXPECT_EQ("0,0", w1->bounds().origin().ToString());
  EXPECT_EQ(size.ToString(), w1->bounds().size().ToString());
}

// Verifies we don't let windows drag to a -y location.
TEST_F(ToplevelWindowEventHandlerTest, DontDragToNegativeY) {
  std::unique_ptr<aura::Window> target(CreateWindow(HTTOP));
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     target.get());
  generator.MoveMouseTo(0, 5);
  generator.DragMouseBy(0, -5);
  // The y location and height should not have changed.
  EXPECT_EQ(0, target->bounds().y());
  EXPECT_EQ(100, target->bounds().height());
}

// Verifies we don't let windows go bigger than the display width.
TEST_F(ToplevelWindowEventHandlerTest, DontGotWiderThanScreen) {
  std::unique_ptr<aura::Window> target(CreateWindow(HTRIGHT));
  gfx::Rect work_area = display::Screen::GetScreen()
                            ->GetDisplayNearestWindow(target.get())
                            .bounds();
  DragFromCenterBy(target.get(), work_area.width() * 2, 0);
  // The y location and height should not have changed.
  EXPECT_EQ(work_area.width(), target->bounds().width());
}

// Verifies that touch-gestures drag the window correctly.
TEST_F(ToplevelWindowEventHandlerTest, GestureDrag) {
  UpdateDisplay("800x600");
  std::unique_ptr<aura::Window> target(CreateTestWindowInShellWithDelegate(
      CreateTestWindowDelegate(HTCAPTION), 0, gfx::Rect(0, 0, 100, 100)));
  WindowState* window_state = WindowState::Get(target.get());
  EXPECT_EQ(WindowStateType::kDefault, window_state->GetStateType());
  EXPECT_EQ(gfx::Rect(), window_state->GetRestoreBoundsInScreen());
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     target.get());
  gfx::Rect old_bounds = target->bounds();
  gfx::Point location(5, 5);
  target->SetProperty(aura::client::kResizeBehaviorKey,
                      aura::client::kResizeBehaviorCanResize |
                          aura::client::kResizeBehaviorCanMaximize |
                          aura::client::kResizeBehaviorCanMinimize);

  // Snap right;
  gfx::Point end(790, 0);
  generator.GestureScrollSequence(location, end, base::Milliseconds(5), 10);
  base::RunLoop().RunUntilIdle();

  // Verify that the window has moved after the gesture.
  EXPECT_NE(old_bounds.ToString(), target->bounds().ToString());
  EXPECT_EQ(WindowStateType::kSecondarySnapped, window_state->GetStateType());
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100),
            window_state->GetRestoreBoundsInScreen());

  old_bounds = target->bounds();

  // Snap left.
  end = location = target->GetBoundsInRootWindow().CenterPoint();
  end.Offset(-100, 0);
  generator.GestureScrollSequence(location, end, base::Milliseconds(5), 10);
  base::RunLoop().RunUntilIdle();

  EXPECT_NE(old_bounds.ToString(), target->bounds().ToString());
  EXPECT_EQ(WindowStateType::kPrimarySnapped, window_state->GetStateType());
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100),
            window_state->GetRestoreBoundsInScreen());

  window_state->Restore();
  EXPECT_EQ(WindowStateType::kNormal, window_state->GetStateType());
  EXPECT_EQ(gfx::Rect(), window_state->GetRestoreBoundsInScreen());
  gfx::Rect bounds_before_maximization = target->bounds();
  bounds_before_maximization.Offset(0, 100);
  target->SetBounds(bounds_before_maximization);
  old_bounds = target->bounds();

  // Maximize.
  end = location = target->GetBoundsInRootWindow().CenterPoint();
  end.Offset(0, -100);
  generator.GestureScrollSequence(location, end, base::Milliseconds(5), 10);
  base::RunLoop().RunUntilIdle();

  EXPECT_NE(old_bounds.ToString(), target->bounds().ToString());
  EXPECT_TRUE(window_state->IsMaximized());
  EXPECT_EQ(old_bounds.ToString(),
            window_state->GetRestoreBoundsInScreen().ToString());

  window_state->Restore();
  target->SetBounds(old_bounds);

  // Minimize.
  end = location = target->GetBoundsInRootWindow().CenterPoint();
  end.Offset(0, 100);
  generator.GestureScrollSequence(location, end, base::Milliseconds(5), 10);
  base::RunLoop().RunUntilIdle();
  EXPECT_NE(old_bounds.ToString(), target->bounds().ToString());
  EXPECT_TRUE(window_state->IsMinimized());
  EXPECT_TRUE(window_state->unminimize_to_restore_bounds());
  EXPECT_EQ(old_bounds.ToString(),
            window_state->GetRestoreBoundsInScreen().ToString());
}

// Verifies that window dragged by touch-gestures to the edge of display
// will not lead to system crash (see https://crbug.com/917060).
TEST_F(ToplevelWindowEventHandlerTest, GestureDragMultiDisplays) {
  UpdateDisplay("800x600, 800x600");
  std::unique_ptr<aura::Window> target(CreateTestWindowInShellWithDelegate(
      CreateTestWindowDelegate(HTCAPTION), 0, gfx::Rect(0, 0, 100, 100)));
  WindowState* window_state = WindowState::Get(target.get());
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     target.get());
  gfx::Rect old_bounds = target->bounds();
  gfx::Point location(5, 5);
  gfx::Point end = location;

  // On real device, gesture event's location may not be accurate. For example,
  // when window is dragged by touch-gestures to the edge of display, it may
  // create gesture events with location out of the display bounds. Let |end| be
  // out of the primary display's bounds to emulate this situation.
  end.Offset(800, 0);
  generator.GestureScrollSequence(location, end, base::Milliseconds(5), 10);

  // Verify that the window has moved after the gesture.
  EXPECT_NE(old_bounds.ToString(), target->bounds().ToString());
  EXPECT_EQ(WindowStateType::kSecondarySnapped, window_state->GetStateType());
}

// Tests that a gesture cannot minimize an unminimizeable window.
TEST_F(ToplevelWindowEventHandlerTest,
       GestureAttemptMinimizeUnminimizeableWindow) {
  std::unique_ptr<aura::Window> target(CreateWindow(HTCAPTION));
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     target.get());
  gfx::Point location(5, 5);
  target->SetProperty(aura::client::kResizeBehaviorKey,
                      aura::client::kResizeBehaviorCanMaximize);

  gfx::Point end = location;
  end.Offset(0, 100);
  generator.GestureScrollSequence(location, end, base::Milliseconds(5), 10);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(WindowState::Get(target.get())->IsMinimized());
}

TEST_F(ToplevelWindowEventHandlerTest, TwoFingerDragDifferentDelta) {
  std::unique_ptr<aura::Window> target(CreateWindow(HTCAPTION));
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     target.get());

  const int kSteps = 10;
  const int kTouchPoints = 2;
  gfx::Point points[kTouchPoints] = {
      gfx::Point(5, 5),   // Within caption.
      gfx::Point(55, 5),  // Within caption.
  };
  gfx::Vector2d delta[kTouchPoints] = {
      gfx::Vector2d(80, 80),
      gfx::Vector2d(20, 20),
  };
  int delay_adding_finger_ms[kTouchPoints] = {0, 0};
  int delay_releasing_finger_ms[kTouchPoints] = {150, 150};

  gfx::Rect bounds = target->bounds();
  // Swipe right and down starting with two fingers. Two fingers have different
  // moving deltas. The window position should move along the average vector of
  // these two fingers.
  generator.GestureMultiFingerScrollWithDelays(
      kTouchPoints, points, delta, delay_adding_finger_ms,
      delay_releasing_finger_ms, 15, kSteps);
  bounds += gfx::Vector2d(50, 50);
  EXPECT_EQ(bounds.ToString(), target->bounds().ToString());
}

TEST_F(ToplevelWindowEventHandlerTest, TwoFingerDragDelayAddFinger) {
  std::unique_ptr<aura::Window> target(CreateWindow(HTCAPTION));
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     target.get());

  const int kSteps = 10;
  const int kTouchPoints = 2;
  gfx::Point points[kTouchPoints] = {
      gfx::Point(5, 5),   // Within caption.
      gfx::Point(55, 5),  // Within caption.
  };
  gfx::Vector2d delta[kTouchPoints] = {
      gfx::Vector2d(50, 50),
      gfx::Vector2d(50, 50),
  };
  int delay_adding_finger_ms[kTouchPoints] = {0, 90};
  int delay_releasing_finger_ms[kTouchPoints] = {150, 150};

  gfx::Rect bounds = target->bounds();
  // Swipe right and down starting with one fingers. Add another finger at 90ms
  // and continue dragging. The drag should continue without interrupt.
  generator.GestureMultiFingerScrollWithDelays(
      kTouchPoints, points, delta, delay_adding_finger_ms,
      delay_releasing_finger_ms, 15, kSteps);
  bounds += gfx::Vector2d(50, 50);
  EXPECT_EQ(bounds.ToString(), target->bounds().ToString());
}

TEST_F(ToplevelWindowEventHandlerTest, TwoFingerDragDelayReleaseFinger) {
  std::unique_ptr<aura::Window> target(CreateWindow(HTCAPTION));
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     target.get());

  const int kSteps = 10;
  const int kTouchPoints = 2;
  gfx::Point points[kTouchPoints] = {
      gfx::Point(5, 5),   // Within caption.
      gfx::Point(55, 5),  // Within caption.
  };
  gfx::Vector2d delta[kTouchPoints] = {
      gfx::Vector2d(50, 50),
      gfx::Vector2d(50, 50),
  };
  int delay_adding_finger_ms[kTouchPoints] = {0, 0};
  int delay_releasing_finger_ms[kTouchPoints] = {150, 90};

  gfx::Rect bounds = target->bounds();
  // Swipe right and down starting with two fingers. Remove one finger at 90ms
  // and continue dragging. The drag should continue without interrupt.
  generator.GestureMultiFingerScrollWithDelays(
      kTouchPoints, points, delta, delay_adding_finger_ms,
      delay_releasing_finger_ms, 15, kSteps);
  bounds += gfx::Vector2d(50, 50);
  EXPECT_EQ(bounds.ToString(), target->bounds().ToString());
}

TEST_F(ToplevelWindowEventHandlerTest,
       TwoFingerDragDelayAdd2ndAndRelease2ndFinger) {
  std::unique_ptr<aura::Window> target(CreateWindow(HTCAPTION));
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     target.get());

  const int kSteps = 10;
  const int kTouchPoints = 2;
  gfx::Point points[kTouchPoints] = {
      gfx::Point(5, 5),   // Within caption.
      gfx::Point(55, 5),  // Within caption.
  };
  gfx::Vector2d delta[kTouchPoints] = {
      gfx::Vector2d(50, 50),
      gfx::Vector2d(50, 50),
  };
  int delay_adding_finger_ms[kTouchPoints] = {0, 30};
  int delay_releasing_finger_ms[kTouchPoints] = {150, 120};

  gfx::Rect bounds = target->bounds();
  // Swipe right and down starting with one fingers. Add second finger at 30ms,
  // continue dragging, release second finger at 120ms and continue dragging.
  // The drag should continue without interrupt.
  generator.GestureMultiFingerScrollWithDelays(
      kTouchPoints, points, delta, delay_adding_finger_ms,
      delay_releasing_finger_ms, 15, kSteps);
  bounds += gfx::Vector2d(50, 50);
  EXPECT_EQ(bounds.ToString(), target->bounds().ToString());
}

TEST_F(ToplevelWindowEventHandlerTest,
       TwoFingerDragDelayAdd2ndAndRelease1stFinger) {
  std::unique_ptr<aura::Window> target(CreateWindow(HTCAPTION));
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     target.get());

  const int kSteps = 10;
  const int kTouchPoints = 2;
  gfx::Point points[kTouchPoints] = {
      gfx::Point(5, 5),   // Within caption.
      gfx::Point(55, 5),  // Within caption.
  };
  gfx::Vector2d delta[kTouchPoints] = {
      gfx::Vector2d(50, 50),
      gfx::Vector2d(50, 50),
  };
  int delay_adding_finger_ms[kTouchPoints] = {0, 30};
  int delay_releasing_finger_ms[kTouchPoints] = {120, 150};

  gfx::Rect bounds = target->bounds();
  // Swipe right and down starting with one fingers. Add second finger at 30ms,
  // continue dragging, release first finger at 120ms and continue dragging.
  // The drag should continue without interrupt.
  generator.GestureMultiFingerScrollWithDelays(
      kTouchPoints, points, delta, delay_adding_finger_ms,
      delay_releasing_finger_ms, 15, kSteps);
  bounds += gfx::Vector2d(50, 50);
  EXPECT_EQ(bounds.ToString(), target->bounds().ToString());
}

TEST_F(ToplevelWindowEventHandlerTest, GestureDragToRestore) {
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
      CreateTestWindowDelegate(HTCAPTION), 0, gfx::Rect(10, 20, 30, 40)));
  window->Show();
  WindowState* window_state = WindowState::Get(window.get());
  window_state->Activate();

  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     window.get());
  gfx::Rect old_bounds = window->bounds();
  gfx::Point location, end;
  end = location = window->GetBoundsInRootWindow().CenterPoint();
  end.Offset(0, 100);
  generator.GestureScrollSequence(location, end, base::Milliseconds(5), 10);
  base::RunLoop().RunUntilIdle();
  EXPECT_NE(old_bounds.ToString(), window->bounds().ToString());
  EXPECT_TRUE(window_state->IsMinimized());
  EXPECT_TRUE(window_state->unminimize_to_restore_bounds());
  EXPECT_EQ(old_bounds.ToString(),
            window_state->GetRestoreBoundsInScreen().ToString());
}

// Tests that EasyResizeWindowTargeter expands the hit-test area when a
// top-level window can be resized but not when the window is not resizable.
TEST_F(ToplevelWindowEventHandlerTest, EasyResizerUsedForTopLevel) {
  std::unique_ptr<aura::Window> w1(CreateTestWindowInShellWithDelegate(
      CreateTestWindowDelegate(HTCAPTION), -1, gfx::Rect(0, 0, 100, 100)));
  std::unique_ptr<aura::Window> w2(CreateTestWindowInShellWithDelegate(
      CreateTestWindowDelegate(HTCAPTION), -2, gfx::Rect(40, 40, 100, 100)));
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     gfx::Point(5, 5));

  generator.PressMoveAndReleaseTouchTo(gfx::Point(5, 5));
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));

  // Make |w1| resizable to allow touch events to go to it (and not |w2|) thanks
  // to EasyResizeWindowTargeter.
  w1->SetProperty(aura::client::kResizeBehaviorKey,
                  aura::client::kResizeBehaviorCanMaximize |
                      aura::client::kResizeBehaviorCanMinimize |
                      aura::client::kResizeBehaviorCanResize);
  // Clicking a point within |w2| but close to |w1| should not activate |w2|.
  const gfx::Point touch_point(105, 105);
  generator.MoveTouch(touch_point);
  generator.PressMoveAndReleaseTouchTo(touch_point);
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));

  // Make |w1| not resizable to allow touch events to go to |w2| even when close
  // to |w1|.
  w1->SetProperty(aura::client::kResizeBehaviorKey,
                  aura::client::kResizeBehaviorCanMaximize |
                      aura::client::kResizeBehaviorCanMinimize);
  // Clicking a point within |w2| should activate that window.
  generator.PressMoveAndReleaseTouchTo(touch_point);
  EXPECT_TRUE(wm::IsActiveWindow(w2.get()));
}

// Tests that EasyResizeWindowTargeter expands the hit-test area when a
// window is a transient child of a top-level window and is resizable.
TEST_F(ToplevelWindowEventHandlerTest, EasyResizerUsedForTransient) {
  std::unique_ptr<aura::Window> w1(CreateTestWindowInShellWithDelegate(
      CreateTestWindowDelegate(HTCAPTION), -1, gfx::Rect(0, 0, 100, 100)));
  std::unique_ptr<aura::Window> w11(CreateTestWindowInShellWithDelegate(
      CreateTestWindowDelegate(HTCAPTION), -11, gfx::Rect(20, 20, 50, 50)));
  ::wm::AddTransientChild(w1.get(), w11.get());
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     gfx::Point(10, 10));

  // Make |w11| non-resizable to avoid touch events inside its transient parent
  // |w1| from going to |w11| because of EasyResizeWindowTargeter.
  w11->SetProperty(aura::client::kResizeBehaviorKey,
                   aura::client::kResizeBehaviorCanMaximize |
                       aura::client::kResizeBehaviorCanMinimize);
  // Clicking a point within w1 should activate that window.
  generator.PressMoveAndReleaseTouchTo(gfx::Point(10, 10));
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));

  // Make |w11| resizable to allow touch events inside its transient parent
  // |w1| that are close to |w11| border to go to |w11| thanks to
  // EasyResizeWindowTargeter.
  w11->SetProperty(aura::client::kResizeBehaviorKey,
                   aura::client::kResizeBehaviorCanMaximize |
                       aura::client::kResizeBehaviorCanMinimize |
                       aura::client::kResizeBehaviorCanResize);
  // Clicking a point within |w1| but close to |w11| should activate |w11|.
  generator.PressMoveAndReleaseTouchTo(gfx::Point(10, 10));
  EXPECT_TRUE(wm::IsActiveWindow(w11.get()));
}

// Tests that an unresizable window cannot be dragged or snapped using gestures.
TEST_F(ToplevelWindowEventHandlerTest, GestureDragForUnresizableWindow) {
  std::unique_ptr<aura::Window> target(CreateWindow(HTCAPTION));
  WindowState* window_state = WindowState::Get(target.get());

  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     target.get());
  gfx::Rect old_bounds = target->bounds();
  gfx::Point location(5, 5);

  target->SetProperty(aura::client::kResizeBehaviorKey,
                      aura::client::kResizeBehaviorNone);

  gfx::Point end = location;

  // Try to snap right. The window is not resizable. So it should not snap.
  end.Offset(100, 0);
  generator.GestureScrollSequence(location, end, base::Milliseconds(5), 10);
  base::RunLoop().RunUntilIdle();

  // Verify that the window has moved after the gesture.
  gfx::Rect expected_bounds(old_bounds);
  expected_bounds.Offset(gfx::Vector2d(100, 0));
  EXPECT_EQ(expected_bounds.ToString(), target->bounds().ToString());

  // Verify that the window did not snap left.
  EXPECT_TRUE(window_state->IsNormalStateType());

  old_bounds = target->bounds();

  // Try to snap left. It should not snap.
  end = location = target->GetBoundsInRootWindow().CenterPoint();
  end.Offset(-100, 0);
  generator.GestureScrollSequence(location, end, base::Milliseconds(5), 10);
  base::RunLoop().RunUntilIdle();

  // Verify that the window has moved after the gesture.
  expected_bounds = old_bounds;
  expected_bounds.Offset(gfx::Vector2d(-100, 0));
  EXPECT_EQ(expected_bounds.ToString(), target->bounds().ToString());

  // Verify that the window did not snap left.
  EXPECT_TRUE(window_state->IsNormalStateType());
}

// Tests that dragging multiple windows at the same time is not allowed.
TEST_F(ToplevelWindowEventHandlerTest, GestureDragMultipleWindows) {
  std::unique_ptr<aura::Window> target(CreateTestWindowInShellWithDelegate(
      CreateTestWindowDelegate(HTCAPTION), 0, gfx::Rect(0, 0, 100, 100)));
  std::unique_ptr<aura::Window> notmoved(CreateTestWindowInShellWithDelegate(
      CreateTestWindowDelegate(HTCAPTION), 1, gfx::Rect(100, 0, 100, 100)));

  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     target.get());
  gfx::Point location(5, 5);

  // Send some touch events to start dragging |target|.
  generator.MoveTouch(location);
  generator.PressTouch();
  location.Offset(40, 5);
  generator.MoveTouch(location);

  // Try to drag |notmoved| window. This should not move the window.
  {
    gfx::Rect bounds = notmoved->bounds();
    ui::test::EventGenerator gen(Shell::GetPrimaryRootWindow(), notmoved.get());
    gfx::Point start = notmoved->bounds().origin() + gfx::Vector2d(10, 10);
    gfx::Point end = start + gfx::Vector2d(100, 10);
    gen.GestureScrollSequence(start, end, base::Milliseconds(10), 10);
    EXPECT_EQ(bounds.ToString(), notmoved->bounds().ToString());
  }
}

// Verifies pressing escape resets the bounds to the original bounds.
TEST_F(ToplevelWindowEventHandlerTest, EscapeReverts) {
  std::unique_ptr<aura::Window> target(CreateWindow(HTBOTTOMRIGHT));
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     target.get());
  generator.PressLeftButton();
  generator.MoveMouseBy(10, 11);

  // Execute any scheduled draws so that pending mouse events are processed.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ("0,0 110x111", target->bounds().ToString());
  generator.PressKey(ui::VKEY_ESCAPE, 0);
  generator.ReleaseKey(ui::VKEY_ESCAPE, 0);
  EXPECT_EQ("0,0 100x100", target->bounds().ToString());
}

// Verifies window minimization/maximization completes drag.
TEST_F(ToplevelWindowEventHandlerTest, MinimizeMaximizeCompletes) {
  // Once window is minimized, window dragging completes.
  {
    std::unique_ptr<aura::Window> target(CreateWindow(HTCAPTION));
    target->Focus();
    ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                       target.get());
    generator.PressLeftButton();
    generator.MoveMouseBy(10, 11);
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ("10,11 100x100", target->bounds().ToString());
    WindowState* window_state = WindowState::Get(target.get());
    window_state->Minimize();
    window_state->Restore();

    generator.PressLeftButton();
    generator.MoveMouseBy(10, 11);
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ("10,11 100x100", target->bounds().ToString());
  }

  // Once window is maximized, window dragging completes.
  {
    std::unique_ptr<aura::Window> target(CreateWindow(HTCAPTION));
    target->Focus();
    ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                       target.get());
    generator.PressLeftButton();
    generator.MoveMouseBy(10, 11);
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ("10,11 100x100", target->bounds().ToString());
    WindowState* window_state = WindowState::Get(target.get());
    window_state->Maximize();
    window_state->Restore();

    generator.PressLeftButton();
    generator.MoveMouseBy(10, 11);
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ("10,11 100x100", target->bounds().ToString());
  }
}

// Verifies that a drag cannot be started via
// wm::WindowMoveClient::RunMoveLoop() while another drag is already
// in progress.
TEST_F(ToplevelWindowEventHandlerTest, RunMoveLoopFailsDuringInProgressDrag) {
  std::unique_ptr<aura::Window> window1(CreateWindow(HTCAPTION));
  EXPECT_EQ("0,0 100x100", window1->bounds().ToString());
  std::unique_ptr<aura::Window> window2(CreateWindow(HTCAPTION));

  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     window1.get());
  window1->Focus();
  generator.PressLeftButton();
  generator.MoveMouseBy(10, 11);
  EXPECT_EQ("10,11 100x100", window1->bounds().ToString());

  ::wm::WindowMoveClient* move_client =
      ::wm::GetWindowMoveClient(window2->GetRootWindow());
  EXPECT_EQ(::wm::MOVE_CANCELED,
            move_client->RunMoveLoop(window2.get(), gfx::Vector2d(),
                                     ::wm::WINDOW_MOVE_SOURCE_MOUSE));

  generator.ReleaseLeftButton();
  EXPECT_EQ("10,11 100x100", window1->bounds().ToString());
}

namespace {

void SendMouseReleaseAndReleaseCapture(ui::test::EventGenerator* generator,
                                       aura::Window* window) {
  generator->ReleaseLeftButton();
  window->ReleaseCapture();
}

}  // namespace

// Test that a drag is successful even if EventType::kMouseCaptureChanged is
// sent immediately after the mouse release. views::Widget has this behavior.
TEST_F(ToplevelWindowEventHandlerTest, CaptureLossAfterMouseRelease) {
  std::unique_ptr<aura::Window> window(CreateWindow(HTNOWHERE));
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     window.get());
  generator.PressLeftButton();
  window->SetCapture();

  ::wm::WindowMoveClient* move_client =
      ::wm::GetWindowMoveClient(window->GetRootWindow());
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&SendMouseReleaseAndReleaseCapture,
                                base::Unretained(&generator),
                                base::Unretained(window.get())));
  EXPECT_EQ(::wm::MOVE_SUCCESSFUL,
            move_client->RunMoveLoop(window.get(), gfx::Vector2d(),
                                     ::wm::WINDOW_MOVE_SOURCE_MOUSE));
}

namespace {

// Checks that |window| has capture and releases capture.
void CheckHasCaptureAndReleaseCapture(aura::Window* window) {
  ASSERT_TRUE(window->HasCapture());
  window->ReleaseCapture();
}

}  // namespace

// Test that releasing capture completes an in-progress gesture drag.
TEST_F(ToplevelWindowEventHandlerTest, GestureDragCaptureLoss) {
  std::unique_ptr<aura::Window> window(CreateWindow(HTNOWHERE));
  auto* generator = GetEventGenerator();

  generator->PressTouch(window->GetBoundsInScreen().CenterPoint());

  ::wm::WindowMoveClient* move_client =
      ::wm::GetWindowMoveClient(window->GetRootWindow());
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&CheckHasCaptureAndReleaseCapture,
                                base::Unretained(window.get())));
  EXPECT_EQ(::wm::MOVE_SUCCESSFUL,
            move_client->RunMoveLoop(window.get(), gfx::Vector2d(),
                                     ::wm::WINDOW_MOVE_SOURCE_TOUCH));
}

// Tests that dragging a snapped window to another display updates the
// window's bounds correctly.
TEST_F(ToplevelWindowEventHandlerTest, DragSnappedWindowToExternalDisplay) {
  UpdateDisplay("940x550,940x550");
  display::test::DisplayManagerTestApi display_manager_test(display_manager());
  int64_t primary_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  int64_t secondary_id = display_manager_test.GetSecondaryDisplay().id();
  display::DisplayLayoutBuilder builder(primary_id);
  builder.SetSecondaryPlacement(secondary_id, display::DisplayPlacement::TOP,
                                0);
  display_manager()->SetLayoutForCurrentDisplays(builder.Build());

  const gfx::Size initial_window_size(330, 230);
  std::unique_ptr<aura::Window> w1(CreateTestWindowInShellWithDelegateAndType(
      CreateTestWindowDelegate(HTCAPTION), aura::client::WINDOW_TYPE_NORMAL, 0,
      gfx::Rect(initial_window_size)));

  // Snap the window to the right.
  WindowState* window_state = WindowState::Get(w1.get());
  ASSERT_TRUE(window_state->CanSnap());
  const WindowSnapWMEvent event(WM_EVENT_CYCLE_SNAP_SECONDARY);
  window_state->OnWMEvent(&event);
  ASSERT_TRUE(window_state->IsSnapped());

  // Drag the window to the secondary display.
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(), w1.get());
  generator.DragMouseTo(472, -462);

  // Expect the window is no longer snapped and its size was restored to the
  // initial size.
  EXPECT_FALSE(window_state->IsSnapped());
  EXPECT_EQ(initial_window_size.ToString(), w1->bounds().size().ToString());

  // The window is now fully contained in the secondary display.
  EXPECT_TRUE(display_manager_test.GetSecondaryDisplay().bounds().Contains(
      w1->GetBoundsInScreen()));
}

TEST_F(ToplevelWindowEventHandlerTest, MoveDoesntEnterResizeLoop) {
  std::unique_ptr<aura::Window> w1(CreateWindow(HTCAPTION));
  ResizeLoopWindowObserver window_observer(w1.get());
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(), w1.get());
  // A click on the caption does not trigger the resize loop.
  generator.PressLeftButton();
  EXPECT_FALSE(window_observer.in_resize_loop());

  // A move in the caption does not trigger the resize loop either.
  generator.MoveMouseBy(100, 100);
  EXPECT_FALSE(window_observer.in_resize_loop());
  w1->RemoveObserver(&window_observer);
}

TEST_F(ToplevelWindowEventHandlerTest, EnterResizeLoopOnResize) {
  std::unique_ptr<aura::Window> w1(CreateWindow(HTGROWBOX));
  ResizeLoopWindowObserver window_observer(w1.get());
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(), w1.get());
  // The resize loop is entered once a possible resize is detected.
  generator.PressLeftButton();
  EXPECT_TRUE(window_observer.in_resize_loop());

  // Should remain in the resize loop while dragging.
  generator.MoveMouseBy(100, 100);
  EXPECT_TRUE(window_observer.in_resize_loop());

  // Releasing the button should end the loop.
  generator.ReleaseLeftButton();
  EXPECT_FALSE(window_observer.in_resize_loop());
}

// Explicitly start a window drag using a child window as a target of drag
// events.
TEST_F(ToplevelWindowEventHandlerTest, ExplicitDragWithChildWindow) {
  std::unique_ptr<aura::Window> w1(CreateWindow(HTCLIENT));
  w1->SetName("parent");
  auto* c1 = new aura::Window(
      aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate(),
      aura::client::WINDOW_TYPE_CONTROL);
  c1->SetName("child");
  c1->Init(ui::LAYER_TEXTURED);
  w1->AddChild(c1);
  c1->SetBounds(gfx::Rect(25, 25, 50, 50));
  c1->Show();
  c1->SetBounds(gfx::Rect(1, 1, 100, 100));

  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(), c1);
  auto* toplevel_window_event_handler =
      Shell::Get()->toplevel_window_event_handler();

  generator.PressLeftButton();

  // Do not capture where because we're dragging using a window that is
  // different from w1.
  ASSERT_TRUE(toplevel_window_event_handler->AttemptToStartDrag(
      w1.get(), gfx::PointF(50, 50), HTCAPTION, ::wm::WINDOW_MOVE_SOURCE_MOUSE,
      base::DoNothing(),
      /*update_gesture_target=*/false,
      /*grab_capture=*/false));

  EXPECT_TRUE(toplevel_window_event_handler->is_drag_in_progress());
  generator.MoveMouseBy(20, 10);
  EXPECT_EQ(gfx::Rect(21, 11, 100, 100), w1->bounds());
  generator.MoveMouseBy(20, 10);
  EXPECT_EQ(gfx::Rect(41, 21, 100, 100), w1->bounds());
}

// Test that if a display is detached during dragging, the drag will be ended.
TEST_F(ToplevelWindowEventHandlerTest, DetachDisplayDuringDragging) {
  UpdateDisplay("800x600,1200x800");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2U, root_windows.size());
  const display::Display display1 =
      display::Screen::GetScreen()->GetDisplayNearestWindow(root_windows[1]);

  // Move the window to the second display and start drag the window.
  std::unique_ptr<aura::Window> dragged_window(CreateWindow(HTCAPTION));
  dragged_window->SetBoundsInScreen(gfx::Rect(900, 0, 50, 60), display1);
  EXPECT_EQ(dragged_window->GetRootWindow(), root_windows[1]);

  ui::test::EventGenerator generator(root_windows[1], dragged_window.get());
  generator.PressLeftButton();
  generator.MoveMouseBy(20, 10);

  EXPECT_TRUE(WindowState::Get(dragged_window.get())->is_dragged());
  ToplevelWindowEventHandler* event_handler =
      Shell::Get()->toplevel_window_event_handler();
  EXPECT_TRUE(event_handler->is_drag_in_progress());
  EXPECT_EQ(dragged_window->GetRootWindow(), root_windows[1]);

  // Detach the display.
  UpdateDisplay("800x600");
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(WindowState::Get(dragged_window.get())->is_dragged());
}

namespace {

// Provides common setup and convenience for a handful of tests.
class ToplevelWindowEventHandlerDragTest : public AshTestBase {
 public:
  ToplevelWindowEventHandlerDragTest() = default;

  ToplevelWindowEventHandlerDragTest(
      const ToplevelWindowEventHandlerDragTest&) = delete;
  ToplevelWindowEventHandlerDragTest& operator=(
      const ToplevelWindowEventHandlerDragTest&) = delete;

  ~ToplevelWindowEventHandlerDragTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    dragged_window_ = CreateTestWindow();
    non_dragged_window_ = CreateTestWindow();
    dragged_window_->SetProperty(chromeos::kAppTypeKey,
                                 chromeos::AppType::CHROME_APP);
  }

  void TearDown() override {
    non_dragged_window_.reset();
    dragged_window_.reset();
    AshTestBase::TearDown();
  }

 protected:
  // Send gesture event with `type` to the toplevel window event handler.
  // This is for gestures that require the `delta` arguments for
  // `GestureEventDetails` construction.
  void SendGestureEvent(const gfx::Point& position,
                        int scroll_x,
                        int scroll_y,
                        ui::EventType type) {
    SendGestureEventImpl(position,
                         ui::GestureEventDetails(type, scroll_x, scroll_y));
  }

  // Send gesture event with `type` to the toplevel window event handler.
  // This is for gestures that do not require the `delta` arguments for
  // `GestureEventDetails` construction.
  void SendGestureEvent(const gfx::Point& position, ui::EventType type) {
    SendGestureEventImpl(position, ui::GestureEventDetails(type));
  }

  void SendGestureEventImpl(const gfx::Point& position,
                            ui::GestureEventDetails gesture_details) {
    ui::GestureEvent event(position.x(), position.y(), ui::EF_NONE,
                           base::TimeTicks::Now(), gesture_details);
    // This assume gesture types generated here are all 'pressed' state.
    aura::Env::GetInstance()->SetTouchDown(true);
    ui::Event::DispatcherApi(&event).set_target(dragged_window_.get());
    ui::Event::DispatcherApi(&event).set_phase(ui::EP_PRETARGET);
    Shell::Get()->toplevel_window_event_handler()->OnGestureEvent(&event);
  }

  std::unique_ptr<aura::Window> dragged_window_;
  std::unique_ptr<aura::Window> non_dragged_window_;
};

}  // namespace

// Contrary to tablet mode, in non-tablet mode, non resizable windows cannot be
// dragged.
TEST_F(ToplevelWindowEventHandlerDragTest,
       NonResizableWindowsCannotBeDraggedInClamshellMode) {
  ASSERT_FALSE(display::Screen::GetScreen()->InTabletMode());

  dragged_window_->SetProperty(aura::client::kResizeBehaviorKey,
                               aura::client::kResizeBehaviorNone);

  SendGestureEvent(gfx::Point(0, 0), 0, 5, ui::EventType::kGestureScrollBegin);
  SendGestureEvent(gfx::Point(700, 500), 700, 500,
                   ui::EventType::kGestureScrollUpdate);
  EXPECT_FALSE(WindowState::Get(dragged_window_.get())->is_dragged());

  EXPECT_FALSE(OverviewController::Get()->InOverviewSession());
}

// Test that if window destroyed during resize/dragging, no crash should happen.
TEST_F(ToplevelWindowEventHandlerDragTest, WindowDestroyedDuringDragging) {
  SendGestureEvent(gfx::Point(0, 0), 0, 5, ui::EventType::kGestureScrollBegin);
  SendGestureEvent(gfx::Point(700, 500), 700, 500,
                   ui::EventType::kGestureScrollUpdate);
  EXPECT_TRUE(WindowState::Get(dragged_window_.get())->is_dragged());
  ToplevelWindowEventHandler* event_handler =
      Shell::Get()->toplevel_window_event_handler();
  EXPECT_TRUE(event_handler->is_drag_in_progress());

  dragged_window_.reset();
  EXPECT_FALSE(event_handler->is_drag_in_progress());
}

// Test that `gesture_target_` is set immediately with
// `EventType::kGestureBegin`. The client may call `AttemptToStartDrag()` after
// `EventType::kGestureBegin` but before `EventType::kGestureScrollBegin` or
// `EventType::kGesturePinchBegin`.
TEST_F(ToplevelWindowEventHandlerDragTest,
       GestureTargetIsSetAsSoonAsGestureStarts) {
  SendGestureEvent(gfx::Point(0, 0), ui::EventType::kGestureBegin);
  ToplevelWindowEventHandler* event_handler =
      Shell::Get()->toplevel_window_event_handler();

  EXPECT_TRUE(event_handler->gesture_target());
  dragged_window_.reset();
}

TEST_F(ToplevelWindowEventHandlerDragTest,
       DragShouldNotStartWithoutPressedEvents) {
  auto* env = aura::Env::GetInstance();
  auto* toplevel_window_event_handler =
      Shell::Get()->toplevel_window_event_handler();

  std::unique_ptr<aura::Window> w1(CreateWindow(HTCAPTION));
  ASSERT_FALSE(env->is_touch_down());
  ASSERT_FALSE(env->IsMouseButtonDown());

  EXPECT_FALSE(toplevel_window_event_handler->AttemptToStartDrag(
      w1.get(), gfx::PointF(50, 50), HTCAPTION, ::wm::WINDOW_MOVE_SOURCE_MOUSE,
      base::DoNothing(), /*updaete_gesture_target=*/false));
  EXPECT_FALSE(toplevel_window_event_handler->AttemptToStartDrag(
      w1.get(), gfx::PointF(50, 50), HTCAPTION, ::wm::WINDOW_MOVE_SOURCE_TOUCH,
      base::DoNothing(), /*updaete_gesture_target=*/false));
}

class ToplevelWindowEventHandlerPipPinchToResizeTest : public AshTestBase {
 public:
  ToplevelWindowEventHandlerPipPinchToResizeTest() = default;
  ToplevelWindowEventHandlerPipPinchToResizeTest(
      const ToplevelWindowEventHandlerPipPinchToResizeTest&) = delete;
  ToplevelWindowEventHandlerPipPinchToResizeTest& operator=(
      const ToplevelWindowEventHandlerPipPinchToResizeTest&) = delete;

  ~ToplevelWindowEventHandlerPipPinchToResizeTest() override = default;

 protected:
  // Send gesture event with `type` to the toplevel window event handler.
  aura::Window* CreatePipWindow() {
    gfx::Size kMaxWindowSize(600, 400);
    gfx::Size kMinWindowSize(300, 200);
    gfx::SizeF kWindowAspectRatio(3.f, 2.f);

    gfx::Rect bounds(0, 0, 300, 200);
    aura::test::TestWindowDelegate* delegate =
        aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate();
    aura::Window* window(TestWindowBuilder()
                             .AllowAllWindowStates()
                             .SetBounds(bounds)
                             .SetDelegate(delegate)
                             .SetShow(false)
                             .Build()
                             .release());

    delegate->set_maximum_size(kMaxWindowSize);
    delegate->set_minimum_size(kMinWindowSize);
    delegate->set_window_component(HTCAPTION);
    window->SetProperty(aura::client::kAspectRatio, kWindowAspectRatio);
    window->SetProperty(aura::client::kZOrderingKey,
                        ui::ZOrderLevel::kFloatingWindow);

    WindowState* window_state = WindowState::Get(window);
    const WMEvent enter_pip(WM_EVENT_PIP);
    window_state->OnWMEvent(&enter_pip);
    EXPECT_TRUE(window_state->IsPip());
    window->Show();
    return window;
  }
};

TEST_F(ToplevelWindowEventHandlerPipPinchToResizeTest,
       PinchToResizeOnPipChangesWindowBounds) {
  std::unique_ptr<aura::Window> window(CreatePipWindow());
  ASSERT_TRUE(WindowState::Get(window.get())->IsPip());
  EXPECT_EQ(gfx::Rect(8, 8, 300, 200), window->bounds());

  ui::test::EventGenerator* gen = GetEventGenerator();
  const int kTouchPoints = 2;
  int delay_adding_finger_ms[kTouchPoints] = {0, 0};
  int delay_releasing_finger_ms[kTouchPoints] = {1500, 1500};

  {
    // Execute pinch (zoom in) event on PiP window.
    gfx::Point start[kTouchPoints] = {gfx::Point(100, 100),
                                      gfx::Point(250, 100)};
    gfx::Vector2d delta[kTouchPoints] = {gfx::Vector2d(100, 100),
                                         gfx::Vector2d(200, 200)};

    gen->GestureMultiFingerScrollWithDelays(kTouchPoints, start, delta,
                                            delay_adding_finger_ms,
                                            delay_releasing_finger_ms, 15, 100);
    base::RunLoop().RunUntilIdle();

    // Verify that PiP window bounds (origin and size) have changed.
    EXPECT_EQ(gfx::Rect(8, 94, 506, 337), window->bounds());
  }

  {
    // Execute pinch (zoom out) event on PiP window.
    gfx::Point start[kTouchPoints] = {gfx::Point(200, 200),
                                      gfx::Point(450, 300)};
    gfx::Vector2d delta[kTouchPoints] = {gfx::Vector2d(-100, -100),
                                         gfx::Vector2d(-200, -200)};

    gen->GestureMultiFingerScrollWithDelays(kTouchPoints, start, delta,
                                            delay_adding_finger_ms,
                                            delay_releasing_finger_ms, 15, 100);
    base::RunLoop().RunUntilIdle();

    // Verify that PiP window bounds (origin and size) have changed again.
    EXPECT_EQ(gfx::Rect(8, 8, 300, 200), window->bounds());
  }

  const WMEvent exit_pip(WM_EVENT_NORMAL);
  WindowState::Get(window.get())->OnWMEvent(&exit_pip);
}

TEST_F(ToplevelWindowEventHandlerPipPinchToResizeTest,
       PinchToResizeOnPipRespectsMaxSizeAndMinSize) {
  UpdateDisplay("1500x1000");

  ui::test::EventGenerator* gen = GetEventGenerator();
  const int kTouchPoints = 2;
  int delay_adding_finger_ms[kTouchPoints] = {0, 0};
  int delay_releasing_finger_ms[kTouchPoints] = {1500, 1500};
  gfx::Point start[kTouchPoints] = {gfx::Point(100, 100), gfx::Point(250, 100)};

  {
    // Create a PiP window with maximum_size and minimum_size.
    std::unique_ptr<aura::Window> window(CreatePipWindow());
    ASSERT_TRUE(WindowState::Get(window.get())->IsPip());

    // Execute pinch (zoom in) event on PiP window.
    gfx::Vector2d delta[kTouchPoints] = {gfx::Vector2d(100, 100),
                                         gfx::Vector2d(400, 400)};

    gen->GestureMultiFingerScrollWithDelays(kTouchPoints, start, delta,
                                            delay_adding_finger_ms,
                                            delay_releasing_finger_ms, 15, 100);
    base::RunLoop().RunUntilIdle();

    // Verify that PiP window did not exceed the maximum size.
    EXPECT_EQ(gfx::Rect(8, 166, 600, 400), window->bounds());

    const WMEvent exit_pip(WM_EVENT_NORMAL);
    WindowState::Get(window.get())->OnWMEvent(&exit_pip);
  }

  {
    // Create a PiP window with maximum_size and minimum_size set.
    std::unique_ptr<aura::Window> window(CreatePipWindow());
    ASSERT_TRUE(WindowState::Get(window.get())->IsPip());

    // Execute pinch (zoom out) event on PiP window.
    gfx::Vector2d delta[kTouchPoints] = {gfx::Vector2d(0, 0),
                                         gfx::Vector2d(-100, 0)};

    gen->GestureMultiFingerScrollWithDelays(kTouchPoints, start, delta,
                                            delay_adding_finger_ms,
                                            delay_releasing_finger_ms, 15, 100);
    base::RunLoop().RunUntilIdle();

    // Verify that PiP window size did not become smaller than the minimum size.
    EXPECT_EQ(gfx::Rect(8, 8, 300, 200), window->bounds());

    const WMEvent exit_pip(WM_EVENT_NORMAL);
    WindowState::Get(window.get())->OnWMEvent(&exit_pip);
  }
}

TEST_F(ToplevelWindowEventHandlerPipPinchToResizeTest,
       PlacingThirdFingerWithDifferentTargetDuringPipPinchToResizeEndsDrag) {
  std::unique_ptr<aura::Window> window(CreatePipWindow());
  auto* toplevel_window_event_handler =
      Shell::Get()->toplevel_window_event_handler();
  ui::test::EventGenerator* gen = GetEventGenerator();

  // Start a two-finger pinch with the PiP window as the target.
  gen->PressTouchId(0, gfx::Point(100, 100));
  gen->PressTouchId(1, gfx::Point(250, 100));
  gen->MoveTouchId(gfx::Point(10, 0), 0);
  gen->MoveTouchId(gfx::Point(10, 0), 1);

  // Place another finger on the screen, one that has a target
  // other than the PiP window.
  gen->PressTouchId(2, gfx::Point(600, 600));
  gen->MoveTouchId(gfx::Point(10, 10), 2);
  gen->ReleaseTouchId(2);

  // Expect that the drag has ended.
  EXPECT_FALSE(toplevel_window_event_handler->gesture_target());

  const WMEvent exit_pip(WM_EVENT_NORMAL);
  WindowState::Get(window.get())->OnWMEvent(&exit_pip);
}
// Showing the resize shadows when the mouse is over the window edges is
// tested in resize_shadow_and_cursor_test.cc

}  // namespace ash
