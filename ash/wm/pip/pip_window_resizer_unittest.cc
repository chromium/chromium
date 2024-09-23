// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/pip/pip_window_resizer.h"
#include "base/memory/raw_ptr.h"

#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/keyboard/ui/test/keyboard_test_util.h"
#include "ash/metrics/pip_uma.h"
#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/pip/pip_controller.h"
#include "ash/wm/pip/pip_positioner.h"
#include "ash/wm/pip/pip_test_utils.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/test/fake_window_state.h"
#include "ash/wm/test/test_non_client_frame_view_ash.h"
#include "ash/wm/toplevel_window_event_handler.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/work_area_insets.h"
#include "base/functional/callback_helpers.h"
#include "base/numerics/angle_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/layer.h"
#include "ui/display/scoped_display_for_new_windows.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

using ::chromeos::WindowStateType;
using Sample = base::HistogramBase::Sample;

class PipWindowResizerTest : public AshTestBase,
                             public ::testing::WithParamInterface<
                                 std::tuple<std::string, std::size_t>> {
 public:
  PipWindowResizerTest() = default;

  PipWindowResizerTest(const PipWindowResizerTest&) = delete;
  PipWindowResizerTest& operator=(const PipWindowResizerTest&) = delete;

  ~PipWindowResizerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    SetVirtualKeyboardEnabled(true);

    const std::string& display_string = std::get<0>(GetParam());
    const std::size_t root_window_index = std::get<1>(GetParam());
    UpdateWorkArea(display_string);
    ASSERT_LT(root_window_index, Shell::GetAllRootWindows().size());
    scoped_display_ = std::make_unique<display::ScopedDisplayForNewWindows>(
        Shell::GetAllRootWindows()[root_window_index]);
    ForceHideShelvesForTest();
  }

  void TearDown() override {
    widget_.reset();
    scoped_display_.reset();
    SetVirtualKeyboardEnabled(false);
    AshTestBase::TearDown();
  }

 protected:
  views::Widget* widget() { return widget_.get(); }
  aura::Window* window() { return window_; }
  FakeWindowState* test_state() { return test_state_; }
  base::HistogramTester& histograms() { return histograms_; }

  std::unique_ptr<views::Widget> CreateWidgetForTest(const gfx::Rect& bounds) {
    auto* root_window = Shell::GetRootWindowForNewWindows();
    gfx::Rect screen_bounds = bounds;
    ::wm::ConvertRectToScreen(root_window, &screen_bounds);

    auto* pip_container =
        Shell::GetContainer(root_window, kShellWindowId_PipContainer);

    std::unique_ptr<views::Widget> widget(new views::Widget);
    views::Widget::InitParams params(
        views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
    params.bounds = screen_bounds;
    params.z_order = ui::ZOrderLevel::kFloatingWindow;
    params.context = root_window;
    params.parent = pip_container;

    // Add a delegate to make it possible to set the maximum and minimum
    // size for the window with `NonClientFrameViewAsh`.
    params.delegate = new TestWidgetDelegateAsh();

    widget->Init(std::move(params));
    widget->Show();
    return widget;
  }

  PipWindowResizer* CreateResizerForTest(int window_component) {
    return CreateResizerForTest(window_component, window(),
                                window()->bounds().CenterPoint());
  }

  PipWindowResizer* CreateResizerForTest(int window_component,
                                         const gfx::Point& point_in_parent) {
    return CreateResizerForTest(window_component, window(), point_in_parent);
  }

  PipWindowResizer* CreateResizerForTest(int window_component,
                                         aura::Window* window,
                                         const gfx::Point& point_in_parent) {
    WindowState* window_state = WindowState::Get(window);
    window_state->CreateDragDetails(gfx::PointF(point_in_parent),
                                    window_component,
                                    ::wm::WINDOW_MOVE_SOURCE_MOUSE);
    return new PipWindowResizer(window_state);
  }

  gfx::PointF CalculateDragPoint(const WindowResizer& resizer,
                                 int delta_x,
                                 int delta_y) const {
    gfx::PointF location = resizer.GetInitialLocation();
    location.set_x(location.x() + delta_x);
    location.set_y(location.y() + delta_y);
    return location;
  }

  void Fling(std::unique_ptr<WindowResizer> resizer,
             float velocity_x,
             float velocity_y) {
    aura::Window* target_window = resizer->GetTarget();
    base::TimeTicks timestamp = base::TimeTicks::Now();
    ui::GestureEventDetails details = ui::GestureEventDetails(
        ui::EventType::kScrollFlingStart, velocity_x, velocity_y);
    ui::GestureEvent event = ui::GestureEvent(
        target_window->bounds().origin().x(),
        target_window->bounds().origin().y(), ui::EF_NONE, timestamp, details);
    ui::Event::DispatcherApi(&event).set_target(target_window);
    resizer->FlingOrSwipe(&event);
  }

  void PreparePipWindow(const gfx::Rect& bounds) {
    widget_ = CreateWidgetForTest(bounds);
    window_ = widget_->GetNativeWindow();

    auto test_state = std::make_unique<FakeWindowState>(WindowStateType::kPip);
    test_state_ = test_state.get();
    WindowState::Get(window_)->SetStateObject(std::move(test_state));
    Shell::Get()->pip_controller()->SetPipWindow(window_);

    auto* custom_frame = static_cast<TestNonClientFrameViewAsh*>(
        NonClientFrameViewAsh::Get(window()));
    custom_frame->SetMaximumSize(gfx::Size(300, 200));
    custom_frame->SetMinimumSize(gfx::Size(30, 20));

    long root_window_index = static_cast<long>(std::get<1>(GetParam()));
    window_->SetProperty(aura::client::kFullscreenTargetDisplayIdKey,
                         root_window_index);
  }

 private:
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<aura::Window, DanglingUntriaged> window_;
  raw_ptr<FakeWindowState, DanglingUntriaged> test_state_;
  base::HistogramTester histograms_;
  std::unique_ptr<display::ScopedDisplayForNewWindows> scoped_display_;

  void UpdateWorkArea(const std::string& bounds) {
    UpdateDisplay(bounds);
    for (aura::Window* root : Shell::GetAllRootWindows()) {
      WorkAreaInsets::ForWindow(root)->UpdateWorkAreaInsetsForTest(
          root, gfx::Rect(), gfx::Insets(), gfx::Insets());
    }
  }
};

TEST_P(PipWindowResizerTest, PipWindowCanDrag) {
  PreparePipWindow(gfx::Rect(200, 200, 100, 100));

  std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
  ASSERT_TRUE(resizer.get());

  resizer->Drag(CalculateDragPoint(*resizer, 0, 10), 0);
  EXPECT_EQ(gfx::Rect(200, 210, 100, 100),
            test_state()->last_requested_bounds());
}

TEST_P(PipWindowResizerTest, PipWindowCanResize) {
  PreparePipWindow(gfx::Rect(200, 200, 100, 100));
  std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTBOTTOM));
  ASSERT_TRUE(resizer.get());

  resizer->Drag(CalculateDragPoint(*resizer, 0, 10), 0);
  EXPECT_EQ(gfx::Rect(200, 200, 100, 110),
            test_state()->last_requested_bounds());
}

TEST_P(PipWindowResizerTest, PipWindowCanPinchResize) {
  gfx::RectF initial_bounds(200, 200, 120, 80);
  gfx::PointF initial_location = initial_bounds.CenterPoint();
  gfx::Vector2dF location_change(0.f, 0.f);
  gfx::PointF new_location = initial_location + location_change;
  float scale = 1.5f;

  PreparePipWindow(gfx::ToRoundedRect(initial_bounds));

  std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
  ASSERT_TRUE(resizer.get());

  window()->SetProperty(aura::client::kAspectRatio, gfx::SizeF(3.f, 2.f));

  // Pinch zoom in.
  resizer->Pinch(
      CalculateDragPoint(*resizer, location_change.x(), location_change.y()),
      scale);

  // Calculate the expected new bounds.
  float left_ratio =
      (initial_location.x() - initial_bounds.x()) / initial_bounds.width();
  float top_ratio =
      (initial_location.y() - initial_bounds.y()) / initial_bounds.height();
  gfx::SizeF new_size(gfx::ScaleSize(initial_bounds.size(), scale));
  gfx::Rect expected_bounds(new_location.x() - new_size.width() * left_ratio,
                            new_location.y() - new_size.height() * top_ratio,
                            new_size.width(), new_size.height());

  // Verify that the window has expected new bounds.
  EXPECT_EQ(expected_bounds, test_state()->last_requested_bounds());

  // Pinch zoom out.
  resizer->Pinch(CalculateDragPoint(*resizer, 0, 0), /*scale=*/0.5f);

  // Calculate the expected new bounds.
  scale *= 0.5f;
  left_ratio =
      (initial_location.x() - initial_bounds.x()) / initial_bounds.width();
  top_ratio =
      (initial_location.y() - initial_bounds.y()) / initial_bounds.height();
  new_size = gfx::ScaleSize(initial_bounds.size(), scale);
  expected_bounds = gfx::Rect(new_location.x() - new_size.width() * left_ratio,
                              new_location.y() - new_size.height() * top_ratio,
                              new_size.width(), new_size.height());

  EXPECT_EQ(expected_bounds, test_state()->last_requested_bounds());
}

TEST_P(PipWindowResizerTest, PipWindowDragIsRestrictedToWorkArea) {
  PreparePipWindow(gfx::Rect(200, 200, 100, 100));
  // Specify point in parent as center so the drag point does not leave the
  // display. If the drag point is not in any display bounds, it causes the
  // window to be moved to the default display.
  auto landscape =
      display::Screen::GetScreen()->GetPrimaryDisplay().is_landscape();
  int right_x = landscape ? 392 : 292;
  int bottom_y = landscape ? 292 : 392;

  std::unique_ptr<PipWindowResizer> resizer(
      CreateResizerForTest(HTCAPTION, gfx::Point(250, 250)));
  ASSERT_TRUE(resizer.get());

  // Drag to the right.
  resizer->Drag(CalculateDragPoint(*resizer, 250, 0), 0);
  EXPECT_EQ(gfx::Rect(right_x, 200, 100, 100),
            test_state()->last_requested_bounds());

  // Drag down.
  resizer->Drag(CalculateDragPoint(*resizer, 0, 250), 0);
  EXPECT_EQ(gfx::Rect(200, bottom_y, 100, 100),
            test_state()->last_requested_bounds());

  // Drag to the left.
  resizer->Drag(CalculateDragPoint(*resizer, -250, 0), 0);
  EXPECT_EQ(gfx::Rect(8, 200, 100, 100), test_state()->last_requested_bounds());

  // Drag up.
  resizer->Drag(CalculateDragPoint(*resizer, 0, -250), 0);
  EXPECT_EQ(gfx::Rect(200, 8, 100, 100), test_state()->last_requested_bounds());
}

TEST_P(PipWindowResizerTest, PipWindowCanBeDraggedInTabletMode) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  PreparePipWindow(gfx::Rect(200, 200, 100, 100));
  std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
  ASSERT_TRUE(resizer.get());

  resizer->Drag(CalculateDragPoint(*resizer, 0, 10), 0);
  EXPECT_EQ(gfx::Rect(200, 210, 100, 100),
            test_state()->last_requested_bounds());
}

TEST_P(PipWindowResizerTest, PipWindowCanBeResizedInTabletMode) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  PreparePipWindow(gfx::Rect(200, 200, 100, 100));
  std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTBOTTOM));
  ASSERT_TRUE(resizer.get());

  resizer->Drag(CalculateDragPoint(*resizer, 0, 10), 0);
  EXPECT_EQ(gfx::Rect(200, 200, 100, 110),
            test_state()->last_requested_bounds());
}

TEST_P(PipWindowResizerTest, ResizingPipWindowDoesNotTriggerFling) {
  PreparePipWindow(gfx::Rect(8, 8, 100, 100));
  std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTBOTTOM));
  ASSERT_TRUE(resizer.get());

  Fling(std::move(resizer), 0.f, 4000.f);

  // Ensure that the PIP window isn't flung to the bottom edge during resize.
  EXPECT_EQ(gfx::Point(8, 8), test_state()->last_requested_bounds().origin());
}

TEST_P(PipWindowResizerTest, PipWindowCanBeSwipeDismissed) {
  PreparePipWindow(gfx::Rect(8, 8, 100, 100));
  std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
  ASSERT_TRUE(resizer.get());

  // Drag to the top.
  resizer->Drag(CalculateDragPoint(*resizer, 0, -100), 0);

  // Should be dismissed when the drag completes.
  resizer->CompleteDrag();
  EXPECT_TRUE(widget()->IsClosed());
}

TEST_P(PipWindowResizerTest, PipWindowPartiallySwipedDoesNotDismiss) {
  PreparePipWindow(gfx::Rect(8, 8, 100, 100));
  std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
  ASSERT_TRUE(resizer.get());

  // Drag to the top, but only a little bit.
  resizer->Drag(CalculateDragPoint(*resizer, 0, -30), 0);

  // Should not be dismissed when the drag completes.
  resizer->CompleteDrag();
  EXPECT_FALSE(widget()->IsClosed());
  EXPECT_EQ(gfx::Rect(8, 8, 100, 100), test_state()->last_requested_bounds());
}

TEST_P(PipWindowResizerTest, PipWindowInSwipeToDismissGestureLocksToAxis) {
  PreparePipWindow(gfx::Rect(8, 8, 100, 100));
  std::unique_ptr<PipWindowResizer> resizer(
      CreateResizerForTest(HTCAPTION, gfx::Point(50, 50)));
  ASSERT_TRUE(resizer.get());

  // Drag to the top, but only a little bit, to start a swipe-to-dismiss.
  resizer->Drag(CalculateDragPoint(*resizer, 0, -30), 0);
  EXPECT_EQ(gfx::Rect(8, -22, 100, 100), test_state()->last_requested_bounds());

  // Now try to drag to the right, it should be locked to the horizontal axis.
  resizer->Drag(CalculateDragPoint(*resizer, 30, -30), 0);
  EXPECT_EQ(gfx::Rect(8, -22, 100, 100), test_state()->last_requested_bounds());
}

TEST_P(PipWindowResizerTest,
       PipWindowMovedAwayFromScreenEdgeNoLongerCanSwipeToDismiss) {
  PreparePipWindow(gfx::Rect(8, 16, 100, 100));
  std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
  ASSERT_TRUE(resizer.get());

  // Drag to the bottom and left a bit.
  resizer->Drag(CalculateDragPoint(*resizer, -8, 30), 0);
  EXPECT_EQ(gfx::Rect(8, 46, 100, 100), test_state()->last_requested_bounds());

  // Now try to drag to the top start a swipe-to-dismiss. It should stop
  // at the edge of the work area.
  resizer->Drag(CalculateDragPoint(*resizer, -8, -30), 0);
  EXPECT_EQ(gfx::Rect(8, 8, 100, 100), test_state()->last_requested_bounds());
}

TEST_P(PipWindowResizerTest, PipWindowAtCornerLocksToOneAxisOnSwipeToDismiss) {
  PreparePipWindow(gfx::Rect(8, 8, 100, 100));
  std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
  ASSERT_TRUE(resizer.get());

  // Try dragging up and to the left. It should lock onto the axis with the
  // largest displacement.
  resizer->Drag(CalculateDragPoint(*resizer, -40, -30), 0);
  EXPECT_EQ(gfx::Rect(-32, 8, 100, 100), test_state()->last_requested_bounds());
}

TEST_P(
    PipWindowResizerTest,
    PipWindowMustBeDraggedMostlyInDirectionOfDismissToInitiateSwipeToDismiss) {
  PreparePipWindow(gfx::Rect(8, 8, 100, 100));
  std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
  ASSERT_TRUE(resizer.get());

  // Try a lot to the right and a bit to the top. Swiping should not be
  // initiated.
  resizer->Drag(CalculateDragPoint(*resizer, 50, -30), 0);
  EXPECT_EQ(gfx::Rect(58, 8, 100, 100), test_state()->last_requested_bounds());
}

TEST_P(PipWindowResizerTest,
       PipWindowDoesNotMoveUntilStatusOfSwipeToDismissGestureIsKnown) {
  PreparePipWindow(gfx::Rect(8, 8, 100, 100));
  std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
  ASSERT_TRUE(resizer.get());

  // Move a small amount - this should not trigger any bounds change, since
  // we don't know whether a swipe will start or not.
  resizer->Drag(CalculateDragPoint(*resizer, 0, -4), 0);
  EXPECT_TRUE(test_state()->last_requested_bounds().IsEmpty());
}

TEST_P(PipWindowResizerTest, PipWindowIsFlungToEdge) {
  auto landscape =
      display::Screen::GetScreen()->GetPrimaryDisplay().is_landscape();

  {
    PreparePipWindow(gfx::Rect(200, 200, 100, 100));
    std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
    ASSERT_TRUE(resizer.get());

    resizer->Drag(CalculateDragPoint(*resizer, 0, 10), 0);
    Fling(std::move(resizer), 0.f, 4000.f);

    auto origin = landscape ? gfx::Point(200, 292) : gfx::Point(200, 392);

    // Flung downwards.
    EXPECT_EQ(origin, test_state()->last_requested_bounds().origin());
  }

  {
    PreparePipWindow(gfx::Rect(200, 200, 100, 100));
    std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
    ASSERT_TRUE(resizer.get());

    resizer->Drag(CalculateDragPoint(*resizer, 0, -10), 0);
    Fling(std::move(resizer), 0.f, -4000.f);

    // Flung upwards.
    EXPECT_EQ(gfx::Rect(200, 8, 100, 100),
              test_state()->last_requested_bounds());
  }

  {
    PreparePipWindow(gfx::Rect(200, 200, 100, 100));
    std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
    ASSERT_TRUE(resizer.get());

    resizer->Drag(CalculateDragPoint(*resizer, 10, 0), 0);
    Fling(std::move(resizer), 4000.f, 0.f);

    auto origin = landscape ? gfx::Point(392, 200) : gfx::Point(292, 200);
    // Flung to the right.
    EXPECT_EQ(origin, test_state()->last_requested_bounds().origin());
  }

  {
    PreparePipWindow(gfx::Rect(200, 200, 100, 100));
    std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
    ASSERT_TRUE(resizer.get());

    resizer->Drag(CalculateDragPoint(*resizer, -10, 0), 0);
    Fling(std::move(resizer), -4000.f, 0.f);

    // Flung to the left.
    EXPECT_EQ(gfx::Rect(8, 200, 100, 100),
              test_state()->last_requested_bounds());
  }
}

TEST_P(PipWindowResizerTest, PipWindowIsFlungDiagonally) {
  auto landscape =
      display::Screen::GetScreen()->GetPrimaryDisplay().is_landscape();

  {
    PreparePipWindow(gfx::Rect(200, 200, 100, 100));
    std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
    ASSERT_TRUE(resizer.get());

    resizer->Drag(CalculateDragPoint(*resizer, 10, 10), 0);
    Fling(std::move(resizer), 3000.f, 3000.f);

    // Flung downward and to the right, into the corner.
    EXPECT_EQ(gfx::Rect(292, 292, 100, 100),
              test_state()->last_requested_bounds());
  }

  {
    PreparePipWindow(gfx::Rect(200, 200, 100, 100));
    std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
    ASSERT_TRUE(resizer.get());

    resizer->Drag(CalculateDragPoint(*resizer, 3, 4), 0);
    Fling(std::move(resizer), 3000.f, 4000.f);
    gfx::Point origin = landscape ? gfx::Point(269, 292) : gfx::Point(292, 322);

    // Flung downward and to the right, but reaching the bottom edge first.
    EXPECT_EQ(origin, test_state()->last_requested_bounds().origin());
  }
  {
    PreparePipWindow(gfx::Rect(200, 200, 100, 100));
    std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
    ASSERT_TRUE(resizer.get());

    resizer->Drag(CalculateDragPoint(*resizer, 4, 3), 0);
    Fling(std::move(resizer), 4000.f, 3000.f);
    gfx::Point origin = landscape ? gfx::Point(322, 292) : gfx::Point(292, 269);
    // Flung downward and to the right, but reaching the right edge first.
    EXPECT_EQ(origin, test_state()->last_requested_bounds().origin());
  }

  {
    PreparePipWindow(gfx::Rect(200, 200, 100, 100));
    std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
    ASSERT_TRUE(resizer.get());

    resizer->Drag(CalculateDragPoint(*resizer, -3, -4), 0);
    Fling(std::move(resizer), -3000.f, -4000.f);

    // Flung upward and to the left, but reaching the top edge first.
    EXPECT_EQ(gfx::Point(56, 8),
              test_state()->last_requested_bounds().origin());
  }

  {
    PreparePipWindow(gfx::Rect(200, 200, 100, 100));
    std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
    ASSERT_TRUE(resizer.get());

    resizer->Drag(CalculateDragPoint(*resizer, -4, -3), 0);
    Fling(std::move(resizer), -4000.f, -3000.f);

    // Flung upward and to the left, but reaching the left edge first.
    EXPECT_EQ(gfx::Rect(8, 56, 100, 100),
              test_state()->last_requested_bounds());
  }

  {
    PreparePipWindow(gfx::Rect(200, 200, 100, 100));
    std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
    ASSERT_TRUE(resizer.get());

    resizer->Drag(CalculateDragPoint(*resizer, 3, -9), 0);
    Fling(std::move(resizer), 3000.f, -9000.f);

    // Flung upward and to the right, but reaching the top edge first.
    EXPECT_EQ(gfx::Rect(264, 8, 100, 100),
              test_state()->last_requested_bounds());
  }

  {
    PreparePipWindow(gfx::Rect(200, 200, 100, 100));
    std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
    ASSERT_TRUE(resizer.get());

    resizer->Drag(CalculateDragPoint(*resizer, 3, -3), 0);
    Fling(std::move(resizer), 3000.f, -3000.f);

    gfx::Point origin = landscape ? gfx::Point(392, 8) : gfx::Point(292, 108);
    // Flung upward and to the right, but reaching the right edge first.
    EXPECT_EQ(origin, test_state()->last_requested_bounds().origin());
  }

  {
    PreparePipWindow(gfx::Rect(200, 200, 100, 100));
    std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
    ASSERT_TRUE(resizer.get());

    resizer->Drag(CalculateDragPoint(*resizer, -3, 3), 0);
    Fling(std::move(resizer), -3000.f, 3000.f);

    gfx::Point origin = landscape ? gfx::Point(108, 292) : gfx::Point(8, 392);
    // Flung downward and to the left, but reaching the bottom edge first.
    EXPECT_EQ(origin, test_state()->last_requested_bounds().origin());
  }

  {
    PreparePipWindow(gfx::Rect(200, 200, 100, 100));
    std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
    ASSERT_TRUE(resizer.get());

    resizer->Drag(CalculateDragPoint(*resizer, -9, 3), 0);
    Fling(std::move(resizer), -9000.f, 3000.f);

    // Flung downward and to the left, but reaching the left edge first.
    EXPECT_EQ(gfx::Rect(8, 264, 100, 100),
              test_state()->last_requested_bounds());
  }
}

TEST_P(PipWindowResizerTest, PipWindowFlungAvoidsFloatingKeyboard) {
  PreparePipWindow(gfx::Rect(200, 200, 75, 75));

  auto* keyboard_controller = keyboard::KeyboardUIController::Get();
  keyboard_controller->SetContainerType(keyboard::ContainerType::kFloating,
                                        gfx::Rect(0, 0, 1, 1),
                                        base::DoNothing());
  const display::Display display = WindowState::Get(window())->GetDisplay();
  keyboard_controller->ShowKeyboardInDisplay(display);
  ASSERT_TRUE(keyboard::test::WaitUntilShown());

  aura::Window* keyboard_window = keyboard_controller->GetKeyboardWindow();
  keyboard_window->SetBounds(gfx::Rect(8, 150, 100, 100));

  std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
  ASSERT_TRUE(resizer.get());

  // Fling to the left - but don't intersect with the floating keyboard.
  resizer->Drag(CalculateDragPoint(*resizer, -10, 0), 0);
  Fling(std::move(resizer), -4000.f, 0.f);

  // Appear below the keyboard.
  EXPECT_EQ(gfx::Rect(8, 258, 75, 75), test_state()->last_requested_bounds());
}

TEST_P(PipWindowResizerTest, PipWindowDoesNotChangeDisplayOnDrag) {
  PreparePipWindow(gfx::Rect(200, 200, 100, 100));
  const display::Display display = WindowState::Get(window())->GetDisplay();
  gfx::Rect rect_in_screen = window()->bounds();
  ::wm::ConvertRectToScreen(window()->parent(), &rect_in_screen);
  EXPECT_TRUE(display.bounds().Contains(rect_in_screen));

  // Drag inside the display.
  std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
  ASSERT_TRUE(resizer.get());
  resizer->Drag(CalculateDragPoint(*resizer, 10, 10), 0);

  // Ensure the position is still in the display.
  EXPECT_EQ(gfx::Rect(210, 210, 100, 100),
            test_state()->last_requested_bounds());
  EXPECT_EQ(display.id(), WindowState::Get(window())->GetDisplay().id());
  rect_in_screen = window()->bounds();
  ::wm::ConvertRectToScreen(window()->parent(), &rect_in_screen);
  EXPECT_TRUE(display.bounds().Contains(rect_in_screen));
}

TEST_P(PipWindowResizerTest, PipRestoreBoundsSetOnFling) {
  PreparePipWindow(gfx::Rect(200, 200, 100, 100));

  {
    std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
    ASSERT_TRUE(resizer.get());

    resizer->Drag(CalculateDragPoint(*resizer, 10, 10), 0);
    Fling(std::move(resizer), 3000.f, 3000.f);
  }

  WindowState* window_state = WindowState::Get(window());
  EXPECT_TRUE(PipPositioner::HasSnapFraction(window_state));
}

TEST_P(PipWindowResizerTest, PipStartAndFinishFreeResizeUmaMetrics) {
  PreparePipWindow(gfx::Rect(200, 200, 100, 100));
  std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTBOTTOM));
  ASSERT_TRUE(resizer.get());

  EXPECT_EQ(1, histograms().GetBucketCount(kAshPipEventsHistogramName,
                                           Sample(AshPipEvents::FREE_RESIZE)));
  histograms().ExpectTotalCount(kAshPipEventsHistogramName, 1);

  resizer->Drag(CalculateDragPoint(*resizer, 100, 0), 0);
  resizer->CompleteDrag();

  histograms().ExpectTotalCount(kAshPipEventsHistogramName, 1);
}

TEST_P(PipWindowResizerTest, PipPinchResizeTriggersResizeUmaMetrics) {
  PreparePipWindow(gfx::Rect(200, 200, 100, 100));

  // Send pinch event. This also creates a `WindowResizer`.
  base::TimeTicks timestamp = base::TimeTicks::Now();
  ui::GestureEventDetails details(ui::EventType::kGesturePinchBegin);
  ui::GestureEvent event(window()->bounds().origin().x(),
                         window()->bounds().origin().y(), ui::EF_NONE,
                         timestamp, details);
  ui::Event::DispatcherApi(&event).set_target(window());
  ui::Event::DispatcherApi(&event).set_phase(ui::EP_PRETARGET);
  Shell::Get()->toplevel_window_event_handler()->OnGestureEvent(&event);

  EXPECT_EQ(1, histograms().GetBucketCount(kAshPipEventsHistogramName,
                                           Sample(AshPipEvents::FREE_RESIZE)));
  histograms().ExpectTotalCount(kAshPipEventsHistogramName, 1);
}

TEST_P(PipWindowResizerTest, DragDetailsAreDestroyed) {
  PreparePipWindow(gfx::Rect(200, 200, 100, 100));
  WindowState* window_state = WindowState::Get(window());

  {
    std::unique_ptr<PipWindowResizer> resizer(CreateResizerForTest(HTCAPTION));
    ASSERT_TRUE(resizer.get());

    resizer->Drag(CalculateDragPoint(*resizer, 0, 10), 0);
    EXPECT_NE(nullptr, window_state->drag_details());

    resizer->CompleteDrag();
    EXPECT_NE(nullptr, window_state->drag_details());
  }
  EXPECT_EQ(nullptr, window_state->drag_details());
}

// TODO: UpdateDisplay() doesn't support different layouts of multiple displays.
// We should add some way to try multiple layouts.
INSTANTIATE_TEST_SUITE_P(All,
                         PipWindowResizerTest,
                         testing::Values(std::make_tuple("500x400", 0u),
                                         std::make_tuple("500x400/r", 0u),
                                         std::make_tuple("500x400/u", 0u),
                                         std::make_tuple("500x400/l", 0u),
                                         std::make_tuple("1000x800*2", 0u),
                                         std::make_tuple("500x400,500x400", 0u),
                                         std::make_tuple("500x400,500x400",
                                                         1u)));

}  // namespace ash
