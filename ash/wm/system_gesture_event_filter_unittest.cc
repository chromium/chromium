// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/system_gesture_event_filter.h"

#include <vector>

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/window_factory.h"
#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/window_state.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/aura/env.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/hit_test.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/events/event_utils.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/test/test_event_handler.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/non_client_view.h"
#include "ui/views/window/window_button_order_provider.h"

namespace ash {

namespace {

class ResizableWidgetDelegate : public views::WidgetDelegateView {
 public:
  ResizableWidgetDelegate() = default;
  ~ResizableWidgetDelegate() override = default;

 private:
  bool CanResize() const override { return true; }
  bool CanMaximize() const override { return true; }
  bool CanMinimize() const override { return true; }
  void DeleteDelegate() override { delete this; }

  DISALLOW_COPY_AND_ASSIGN(ResizableWidgetDelegate);
};

// Support class for testing windows with a maximum size.
class MaxSizeNCFV : public views::NonClientFrameView {
 public:
  MaxSizeNCFV() = default;

 private:
  gfx::Size GetMaximumSize() const override { return gfx::Size(200, 200); }
  gfx::Rect GetBoundsForClientView() const override { return gfx::Rect(); }

  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override {
    return gfx::Rect();
  }

  // This function must ask the ClientView to do a hittest.  We don't do this in
  // the parent NonClientView because that makes it more difficult to calculate
  // hittests for regions that are partially obscured by the ClientView, e.g.
  // HTSYSMENU.
  int NonClientHitTest(const gfx::Point& point) override { return HTNOWHERE; }
  void GetWindowMask(const gfx::Size& size, SkPath* window_mask) override {}
  void ResetWindowControls() override {}
  void UpdateWindowIcon() override {}
  void UpdateWindowTitle() override {}
  void SizeConstraintsChanged() override {}

  DISALLOW_COPY_AND_ASSIGN(MaxSizeNCFV);
};

class MaxSizeWidgetDelegate : public views::WidgetDelegateView {
 public:
  MaxSizeWidgetDelegate() = default;
  ~MaxSizeWidgetDelegate() override = default;

 private:
  bool CanResize() const override { return true; }
  bool CanMaximize() const override { return false; }
  bool CanMinimize() const override { return true; }
  void DeleteDelegate() override { delete this; }
  views::NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) override {
    return new MaxSizeNCFV;
  }

  DISALLOW_COPY_AND_ASSIGN(MaxSizeWidgetDelegate);
};

}  // namespace

class SystemGestureEventFilterTest : public AshTestBase {
 public:
  SystemGestureEventFilterTest() : AshTestBase() {}
  ~SystemGestureEventFilterTest() override = default;

  // Overridden from AshTestBase:
  void SetUp() override {
    // TODO(jonross): TwoFingerDragDelayed() and ThreeFingerGestureStopsDrag()
    // both use hardcoded touch points, assuming that they target empty header
    // space. Window control order now reflects configuration files and can
    // change. The tests should be improved to dynamically decide touch points.
    // To address this we specify the originally expected window control
    // positions to be consistent across tests.
    std::vector<views::FrameButton> leading;
    std::vector<views::FrameButton> trailing;
    trailing.push_back(views::FrameButton::kMinimize);
    trailing.push_back(views::FrameButton::kMaximize);
    trailing.push_back(views::FrameButton::kClose);
    views::WindowButtonOrderProvider::GetInstance()->SetWindowButtonOrder(
        leading, trailing);

    AshTestBase::SetUp();
    // Enable brightness key.
    display::test::DisplayManagerTestApi(Shell::Get()->display_manager())
        .SetFirstDisplayAsInternalDisplay();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemGestureEventFilterTest);
};

ui::GestureEvent* CreateGesture(ui::EventType type,
                                int x,
                                int y,
                                float delta_x,
                                float delta_y,
                                int touch_id) {
  return new ui::GestureEvent(x, y, 0, base::TimeTicks::Now(),
                              ui::GestureEventDetails(type, delta_x, delta_y));
}

TEST_F(SystemGestureEventFilterTest, TwoFingerDrag) {
  gfx::Rect bounds(0, 0, 600, 600);
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  views::Widget* toplevel = views::Widget::CreateWindowWithContextAndBounds(
      new ResizableWidgetDelegate, root_window, bounds);
  toplevel->Show();

  const int kSteps = 15;
  const int kTouchPoints = 2;
  gfx::Point points[kTouchPoints] = {
      gfx::Point(250, 250), gfx::Point(350, 350),
  };

  ui::test::EventGenerator generator(root_window, toplevel->GetNativeWindow());

  WindowState* toplevel_state = WindowState::Get(toplevel->GetNativeWindow());
  // Swipe down to minimize.
  generator.GestureMultiFingerScroll(kTouchPoints, points, 15, kSteps, 0, 150);
  EXPECT_TRUE(toplevel_state->IsMinimized());

  toplevel->Restore();
  toplevel->GetNativeWindow()->SetBounds(bounds);

  // Swipe up to maximize.
  generator.GestureMultiFingerScroll(kTouchPoints, points, 15, kSteps, 0, -150);
  EXPECT_TRUE(toplevel_state->IsMaximized());

  toplevel->Restore();
  toplevel->GetNativeWindow()->SetBounds(bounds);

  // Swipe right to snap.
  gfx::Rect normal_bounds = toplevel->GetWindowBoundsInScreen();
  generator.GestureMultiFingerScroll(kTouchPoints, points, 15, kSteps, 150, 0);
  gfx::Rect right_tile_bounds = toplevel->GetWindowBoundsInScreen();
  EXPECT_NE(normal_bounds.ToString(), right_tile_bounds.ToString());

  // Swipe left to snap.
  gfx::Point left_points[kTouchPoints];
  for (int i = 0; i < kTouchPoints; ++i) {
    left_points[i] = points[i];
    left_points[i].Offset(right_tile_bounds.x(), right_tile_bounds.y());
  }
  generator.GestureMultiFingerScroll(kTouchPoints, left_points, 15, kSteps,
                                     -150, 0);
  gfx::Rect left_tile_bounds = toplevel->GetWindowBoundsInScreen();
  EXPECT_NE(normal_bounds.ToString(), left_tile_bounds.ToString());
  EXPECT_NE(right_tile_bounds.ToString(), left_tile_bounds.ToString());

  // Swipe right again.
  generator.GestureMultiFingerScroll(kTouchPoints, points, 15, kSteps, 150, 0);
  gfx::Rect current_bounds = toplevel->GetWindowBoundsInScreen();
  EXPECT_NE(current_bounds.ToString(), left_tile_bounds.ToString());
  EXPECT_EQ(current_bounds.ToString(), right_tile_bounds.ToString());
}

TEST_F(SystemGestureEventFilterTest, WindowsWithMaxSizeDontSnap) {
  gfx::Rect bounds(250, 150, 100, 100);
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  views::Widget* toplevel = views::Widget::CreateWindowWithContextAndBounds(
      new MaxSizeWidgetDelegate, root_window, bounds);
  toplevel->Show();

  const int kSteps = 15;
  const int kTouchPoints = 2;
  gfx::Point points[kTouchPoints] = {
      gfx::Point(bounds.x() + 10, bounds.y() + 30),
      gfx::Point(bounds.x() + 30, bounds.y() + 20),
  };

  ui::test::EventGenerator generator(root_window, toplevel->GetNativeWindow());

  // Swipe down to minimize.
  generator.GestureMultiFingerScroll(kTouchPoints, points, 15, kSteps, 0, 150);
  WindowState* toplevel_state = WindowState::Get(toplevel->GetNativeWindow());
  EXPECT_TRUE(toplevel_state->IsMinimized());

  toplevel->Restore();
  toplevel->GetNativeWindow()->SetBounds(bounds);

  // Check that swiping up doesn't maximize.
  generator.GestureMultiFingerScroll(kTouchPoints, points, 15, kSteps, 0, -150);
  EXPECT_FALSE(toplevel_state->IsMaximized());

  toplevel->Restore();
  toplevel->GetNativeWindow()->SetBounds(bounds);

  // Check that swiping right doesn't snap.
  gfx::Rect normal_bounds = toplevel->GetWindowBoundsInScreen();
  generator.GestureMultiFingerScroll(kTouchPoints, points, 15, kSteps, 150, 0);
  normal_bounds.set_x(normal_bounds.x() + 150);
  EXPECT_EQ(normal_bounds.ToString(),
            toplevel->GetWindowBoundsInScreen().ToString());

  toplevel->GetNativeWindow()->SetBounds(bounds);

  // Check that swiping left doesn't snap.
  normal_bounds = toplevel->GetWindowBoundsInScreen();
  generator.GestureMultiFingerScroll(kTouchPoints, points, 15, kSteps, -150, 0);
  normal_bounds.set_x(normal_bounds.x() - 150);
  EXPECT_EQ(normal_bounds.ToString(),
            toplevel->GetWindowBoundsInScreen().ToString());

  toplevel->GetNativeWindow()->SetBounds(bounds);

  // Swipe right again, make sure the window still doesn't snap.
  normal_bounds = toplevel->GetWindowBoundsInScreen();
  normal_bounds.set_x(normal_bounds.x() + 150);
  generator.GestureMultiFingerScroll(kTouchPoints, points, 15, kSteps, 150, 0);
  EXPECT_EQ(normal_bounds.ToString(),
            toplevel->GetWindowBoundsInScreen().ToString());
}

TEST_F(SystemGestureEventFilterTest, DISABLED_TwoFingerDragEdge) {
  gfx::Rect bounds(0, 0, 200, 100);
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  views::Widget* toplevel = views::Widget::CreateWindowWithContextAndBounds(
      new ResizableWidgetDelegate, root_window, bounds);
  toplevel->Show();

  const int kSteps = 15;
  const int kTouchPoints = 2;
  gfx::Point points[kTouchPoints] = {
      gfx::Point(30, 20),  // Caption
      gfx::Point(0, 40),   // Left edge
  };

  EXPECT_EQ(HTCAPTION,
            toplevel->GetNativeWindow()->delegate()->GetNonClientComponent(
                points[0]));
  EXPECT_EQ(HTLEFT,
            toplevel->GetNativeWindow()->delegate()->GetNonClientComponent(
                points[1]));

  ui::test::EventGenerator generator(root_window, toplevel->GetNativeWindow());

  bounds = toplevel->GetNativeWindow()->bounds();
  // Swipe down. Nothing should happen.
  generator.GestureMultiFingerScroll(kTouchPoints, points, 15, kSteps, 0, 150);
  EXPECT_EQ(bounds.ToString(),
            toplevel->GetNativeWindow()->bounds().ToString());
}

// We do not allow resizing a window via multiple edges simultaneously. Test
// that the behavior is reasonable if a user attempts to resize a window via
// several edges.
TEST_F(SystemGestureEventFilterTest,
       TwoFingerAttemptResizeLeftAndRightEdgesSimultaneously) {
  gfx::Rect initial_bounds(0, 0, 400, 400);
  views::Widget* toplevel = views::Widget::CreateWindowWithContextAndBounds(
      new ResizableWidgetDelegate, CurrentContext(), initial_bounds);
  toplevel->Show();

  const int kSteps = 15;
  const int kTouchPoints = 2;
  gfx::Point points[kTouchPoints] = {
      gfx::Point(0, 40),    // Left edge
      gfx::Point(399, 40),  // Right edge
  };
  int delays[kTouchPoints] = {0, 120};

  EXPECT_EQ(HTLEFT, toplevel->GetNonClientComponent(points[0]));
  EXPECT_EQ(HTRIGHT, toplevel->GetNonClientComponent(points[1]));

  GetEventGenerator()->GestureMultiFingerScrollWithDelays(
      kTouchPoints, points, delays, 15, kSteps, 0, 40);

  // The window bounds should not have changed because neither of the fingers
  // moved horizontally.
  EXPECT_EQ(initial_bounds.ToString(),
            toplevel->GetNativeWindow()->bounds().ToString());
}

TEST_F(SystemGestureEventFilterTest, TwoFingerDragDelayed) {
  gfx::Rect bounds(0, 0, 200, 100);
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  views::Widget* toplevel = views::Widget::CreateWindowWithContextAndBounds(
      new ResizableWidgetDelegate, root_window, bounds);
  toplevel->Show();

  const int kSteps = 15;
  const int kTouchPoints = 2;
  gfx::Point points[kTouchPoints] = {
      gfx::Point(30, 20),  // Caption
      gfx::Point(34, 20),  // Caption
  };
  int delays[kTouchPoints] = {0, 120};

  EXPECT_EQ(HTCAPTION,
            toplevel->GetNativeWindow()->delegate()->GetNonClientComponent(
                points[0]));
  EXPECT_EQ(HTCAPTION,
            toplevel->GetNativeWindow()->delegate()->GetNonClientComponent(
                points[1]));

  ui::test::EventGenerator generator(root_window, toplevel->GetNativeWindow());

  bounds = toplevel->GetNativeWindow()->bounds();
  // Swipe right and down starting with one finger.
  // Add another finger after 120ms and continue dragging.
  // The window should not move (see crbug.com/363625) and drag should be
  // determined by the delta of center point between the fingers.
  generator.GestureMultiFingerScrollWithDelays(kTouchPoints, points, delays, 15,
                                               kSteps, 150, 150);
  bounds += gfx::Vector2d(150, 150);
  EXPECT_EQ(bounds.ToString(),
            toplevel->GetNativeWindow()->bounds().ToString());
}

TEST_F(SystemGestureEventFilterTest, ThreeFingerGestureStopsDrag) {
  gfx::Rect bounds(0, 0, 200, 100);
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  views::Widget* toplevel = views::Widget::CreateWindowWithContextAndBounds(
      new ResizableWidgetDelegate, root_window, bounds);
  toplevel->Show();

  const int kSteps = 10;
  const int kTouchPoints = 3;
  gfx::Point points[kTouchPoints] = {
      gfx::Point(30, 20),  // Caption
      gfx::Point(34, 20),  // Caption
      gfx::Point(38, 20),  // Caption
  };
  int delays[kTouchPoints] = {0, 0, 120};

  EXPECT_EQ(HTCAPTION,
            toplevel->GetNativeWindow()->delegate()->GetNonClientComponent(
                points[0]));
  EXPECT_EQ(HTCAPTION,
            toplevel->GetNativeWindow()->delegate()->GetNonClientComponent(
                points[1]));

  ui::test::EventGenerator generator(root_window, toplevel->GetNativeWindow());

  bounds = toplevel->GetNativeWindow()->bounds();
  // Swipe right and down starting with two fingers.
  // Add third finger after 120ms and continue dragging.
  // The window should start moving but stop when the 3rd finger touches down.
  const int kEventSeparation = 15;
  generator.GestureMultiFingerScrollWithDelays(
      kTouchPoints, points, delays, kEventSeparation, kSteps, 150, 150);
  int expected_drag = 150 / kSteps * 120 / kEventSeparation;
  bounds += gfx::Vector2d(expected_drag, expected_drag);
  EXPECT_EQ(bounds.ToString(),
            toplevel->GetNativeWindow()->bounds().ToString());
}

TEST_F(SystemGestureEventFilterTest, DragLeftNearEdgeSnaps) {
  gfx::Rect bounds(200, 150, 400, 100);
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  views::Widget* toplevel = views::Widget::CreateWindowWithContextAndBounds(
      new ResizableWidgetDelegate, root_window, bounds);
  toplevel->Show();

  const int kSteps = 15;
  const int kTouchPoints = 2;
  gfx::Point points[kTouchPoints] = {
      gfx::Point(bounds.x() + bounds.width() / 2, bounds.y() + 5),
      gfx::Point(bounds.x() + bounds.width() / 2, bounds.y() + 5),
  };
  aura::Window* toplevel_window = toplevel->GetNativeWindow();
  ui::test::EventGenerator generator(root_window, toplevel_window);

  // Check that dragging left snaps before reaching the screen edge.
  gfx::Rect work_area = display::Screen::GetScreen()
                            ->GetDisplayNearestWindow(root_window)
                            .work_area();
  int drag_x = work_area.x() + 20 - points[0].x();
  generator.GestureMultiFingerScroll(kTouchPoints, points, 120, kSteps, drag_x,
                                     0);

  EXPECT_EQ(
      GetDefaultLeftSnappedWindowBoundsInParent(toplevel_window).ToString(),
      toplevel_window->bounds().ToString());
}

TEST_F(SystemGestureEventFilterTest, DragRightNearEdgeSnaps) {
  gfx::Rect bounds(200, 150, 400, 100);
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  views::Widget* toplevel = views::Widget::CreateWindowWithContextAndBounds(
      new ResizableWidgetDelegate, root_window, bounds);
  toplevel->Show();

  const int kSteps = 15;
  const int kTouchPoints = 2;
  gfx::Point points[kTouchPoints] = {
      gfx::Point(bounds.x() + bounds.width() / 2, bounds.y() + 5),
      gfx::Point(bounds.x() + bounds.width() / 2, bounds.y() + 5),
  };
  aura::Window* toplevel_window = toplevel->GetNativeWindow();
  ui::test::EventGenerator generator(root_window, toplevel_window);

  // Check that dragging right snaps before reaching the screen edge.
  gfx::Rect work_area = display::Screen::GetScreen()
                            ->GetDisplayNearestWindow(root_window)
                            .work_area();
  int drag_x = work_area.right() - 20 - points[0].x();
  generator.GestureMultiFingerScroll(kTouchPoints, points, 120, kSteps, drag_x,
                                     0);
  EXPECT_EQ(
      GetDefaultRightSnappedWindowBoundsInParent(toplevel_window).ToString(),
      toplevel_window->bounds().ToString());
}

// Tests that the window manager does not consume gesture events targeted to
// windows of type WINDOW_TYPE_CONTROL. This is important because the web
// contents are often (but not always) of type WINDOW_TYPE_CONTROL.
TEST_F(SystemGestureEventFilterTest,
       ControlWindowGetsMultiFingerGestureEvents) {
  std::unique_ptr<aura::Window> parent(
      CreateTestWindowInShellWithBounds(gfx::Rect(100, 100)));

  aura::test::EventCountDelegate delegate;
  delegate.set_window_component(HTCLIENT);
  std::unique_ptr<aura::Window> child =
      window_factory::NewWindow(&delegate, aura::client::WINDOW_TYPE_CONTROL);
  child->Init(ui::LAYER_TEXTURED);
  parent->AddChild(child.get());
  child->SetBounds(gfx::Rect(100, 100));
  child->Show();

  ui::test::TestEventHandler event_handler;
  aura::Env::GetInstance()->AddPreTargetHandler(
      &event_handler, ui::EventTarget::Priority::kSystem);

  GetEventGenerator()->MoveMouseTo(0, 0);
  for (int i = 1; i <= 3; ++i)
    GetEventGenerator()->PressTouchId(i);
  for (int i = 1; i <= 3; ++i)
    GetEventGenerator()->ReleaseTouchId(i);
  EXPECT_EQ(event_handler.num_gesture_events(),
            delegate.GetGestureCountAndReset());

  aura::Env::GetInstance()->RemovePreTargetHandler(&event_handler);
}

}  // namespace ash
