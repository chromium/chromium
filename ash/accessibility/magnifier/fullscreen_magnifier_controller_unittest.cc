// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/magnifier/fullscreen_magnifier_controller.h"

#include "ash/accessibility/magnifier/magnifier_test_utils.h"
#include "ash/accessibility/magnifier/magnifier_utils.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_session.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/keyboard/ui/keyboard_util.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_state.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "ui/aura/env.h"
#include "ui/aura/test/aura_test_utils.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/input_method.h"
#include "ui/compositor/layer.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {
namespace {

const int kRootHeight = 600;
const int kRootWidth = 800;

class TouchEventWatcher : public ui::EventHandler {
 public:
  TouchEventWatcher() = default;
  ~TouchEventWatcher() override = default;

  void OnTouchEvent(ui::TouchEvent* touch_event) override {
    touch_events.push_back(*touch_event);
  }

  std::vector<ui::TouchEvent> touch_events;
};

}  // namespace

class FullscreenMagnifierControllerTest : public AshTestBase {
 public:
  FullscreenMagnifierControllerTest() {}
  ~FullscreenMagnifierControllerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    UpdateDisplay(base::StringPrintf("%dx%d", kRootWidth, kRootHeight));

    touch_event_watcher_ = std::make_unique<TouchEventWatcher>();
    GetRootWindow()->AddPreTargetHandler(touch_event_watcher_.get(),
                                         ui::EventTarget::Priority::kSystem);
  }

  void TearDown() override {
    GetRootWindow()->RemovePreTargetHandler(touch_event_watcher_.get());
    AshTestBase::TearDown();
  }

 protected:
  enum class ScrollDirection {
    kPositiveX,
    kNegativeX,
    kPositiveY,
    kNegativeY,
  };

  std::unique_ptr<TouchEventWatcher> touch_event_watcher_;

  aura::Window* GetRootWindow() const { return Shell::GetPrimaryRootWindow(); }

  std::string GetHostMouseLocation() {
    const gfx::Point& location =
        aura::test::QueryLatestMousePositionRequestInHost(
            GetRootWindow()->GetHost());
    return location.ToString();
  }

  FullscreenMagnifierController* GetFullscreenMagnifierController() const {
    return Shell::Get()->fullscreen_magnifier_controller();
  }

  gfx::Rect GetViewport() const {
    gfx::Rect bounds(0, 0, kRootWidth, kRootHeight);
    return GetRootWindow()
        ->layer()
        ->transform()
        .InverseMapRect(bounds)
        .value_or(bounds);
  }

  std::string CurrentPointOfInterest() const {
    return GetFullscreenMagnifierController()
        ->GetPointOfInterestForTesting()
        .ToString();
  }

  void DispatchTouchEvent(ui::EventType event_type,
                          const gfx::Point& point,
                          const base::TimeTicks& time,
                          const ui::PointerDetails& pointer_details) {
    ui::TouchEvent event(event_type, point, time, pointer_details);
    GetEventGenerator()->Dispatch(&event);
  }

  void DispatchMouseMove(const gfx::PointF location_in_dip) {
    GetFullscreenMagnifierController()->OnMouseMove(location_in_dip);
  }

  // Performs a two-finger scroll gesture in the given |direction|.
  void PerformTwoFingersScrollGesture(ScrollDirection direction) {
    base::TimeTicks time = base::TimeTicks::Now();
    ui::PointerDetails pointer_details1(ui::EventPointerType::kTouch, 0);
    ui::PointerDetails pointer_details2(ui::EventPointerType::kTouch, 1);

    // The offset by which the two fingers will move according to the given
    // direction.
    constexpr int kOffset = 50;
    // The start and end points of both fingers.
    gfx::Point start1(150, 150);
    gfx::Point start2(150, 160);
    gfx::Point end1 = start1;
    gfx::Point end2 = start2;

    gfx::Point offset;
    switch (direction) {
      case ScrollDirection::kPositiveX:
        offset.Offset(kOffset, 0);
        break;

      case ScrollDirection::kNegativeX:
        offset.Offset(-kOffset, 0);
        break;

      case ScrollDirection::kPositiveY:
        offset.Offset(0, kOffset);
        break;

      case ScrollDirection::kNegativeY:
        offset.Offset(0, -kOffset);
        break;
    }

    // The above calculated offsets are in dip, so apply the display rotation
    // transform to convert to pixel.
    const auto display = display_manager()->GetDisplayAt(0);
    gfx::Transform rotation_transform;
    rotation_transform.Rotate(display.PanelRotationAsDegree());
    offset = rotation_transform.MapPoint(offset);

    end1.Offset(offset.x(), offset.y());
    end2.Offset(offset.x(), offset.y());

    DispatchTouchEvent(ui::EventType::kTouchPressed, start1, time,
                       pointer_details1);
    DispatchTouchEvent(ui::EventType::kTouchPressed, start2, time,
                       pointer_details2);

    DispatchTouchEvent(ui::EventType::kTouchMoved, end1, time,
                       pointer_details1);
    DispatchTouchEvent(ui::EventType::kTouchMoved, end2, time,
                       pointer_details2);

    DispatchTouchEvent(ui::EventType::kTouchReleased, end1, time,
                       pointer_details1);
    DispatchTouchEvent(ui::EventType::kTouchReleased, end2, time,
                       pointer_details2);
  }

  MagnifierTextInputTestHelper text_input_helper_;
};

TEST_F(FullscreenMagnifierControllerTest, EnableAndDisable) {
  // Confirms the magnifier is disabled.
  EXPECT_TRUE(GetRootWindow()->layer()->transform().IsIdentity());
  EXPECT_EQ(1.0f, GetFullscreenMagnifierController()->GetScale());
  EXPECT_EQ("0,0 800x600", GetViewport().ToString());

  // Enables magnifier.
  GetFullscreenMagnifierController()->SetEnabled(true);
  EXPECT_FALSE(GetRootWindow()->layer()->transform().IsIdentity());
  EXPECT_EQ(2.0f, GetFullscreenMagnifierController()->GetScale());
  EXPECT_EQ("200,150 400x300", GetViewport().ToString());

  // Disables magnifier.
  GetFullscreenMagnifierController()->SetEnabled(false);
  EXPECT_TRUE(GetRootWindow()->layer()->transform().IsIdentity());
  EXPECT_EQ(1.0f, GetFullscreenMagnifierController()->GetScale());
  EXPECT_EQ("0,0 800x600", GetViewport().ToString());

  // Confirms the the scale can't be changed.
  GetFullscreenMagnifierController()->SetScale(4.0f, false);
  EXPECT_TRUE(GetRootWindow()->layer()->transform().IsIdentity());
  EXPECT_EQ(1.0f, GetFullscreenMagnifierController()->GetScale());
  EXPECT_EQ("0,0 800x600", GetViewport().ToString());
}

TEST_F(FullscreenMagnifierControllerTest, MagnifyAndUnmagnify) {
  // Enables magnifier and confirms the default scale is 2.0x.
  GetFullscreenMagnifierController()->SetEnabled(true);
  EXPECT_FALSE(GetRootWindow()->layer()->transform().IsIdentity());
  EXPECT_EQ(2.0f, GetFullscreenMagnifierController()->GetScale());
  EXPECT_EQ("200,150 400x300", GetViewport().ToString());
  EXPECT_EQ("400,300", CurrentPointOfInterest());

  // Changes the scale.
  GetFullscreenMagnifierController()->SetScale(4.0f, false);
  EXPECT_EQ(4.0f, GetFullscreenMagnifierController()->GetScale());
  EXPECT_EQ("300,225 200x150", GetViewport().ToString());
  EXPECT_EQ("400,300", CurrentPointOfInterest());

  GetFullscreenMagnifierController()->SetScale(1.0f, false);
  EXPECT_EQ(1.0f, GetFullscreenMagnifierController()->GetScale());
  EXPECT_EQ("0,0 800x600", GetViewport().ToString());
  EXPECT_EQ("400,300", CurrentPointOfInterest());

  GetFullscreenMagnifierController()->SetScale(3.0f, false);
  EXPECT_EQ(3.0f, GetFullscreenMagnifierController()->GetScale());
  EXPECT_EQ("266,200 267x200", GetViewport().ToString());
  EXPECT_EQ("400,300", CurrentPointOfInterest());
}

TEST_F(FullscreenMagnifierControllerTest, MoveWindow) {
  // Enables magnifier and confirm the viewport is at center.
  GetFullscreenMagnifierController()->SetEnabled(true);
  EXPECT_EQ(2.0f, GetFullscreenMagnifierController()->GetScale());
  EXPECT_EQ("200,150 400x300", GetViewport().ToString());

  // Move the viewport.
  GetFullscreenMagnifierController()->MoveWindow(0, 0, false);
  EXPECT_EQ("0,0 400x300", GetViewport().ToString());

  GetFullscreenMagnifierController()->MoveWindow(200, 300, false);
  EXPECT_EQ("200,300 400x300", GetViewport().ToString());

  GetFullscreenMagnifierController()->MoveWindow(400, 0, false);
  EXPECT_EQ("400,0 400x300", GetViewport().ToString());

  GetFullscreenMagnifierController()->MoveWindow(400, 300, false);
  EXPECT_EQ("400,300 400x300", GetViewport().ToString());

  // Confirms that the viewport can't across the top-left border.
  GetFullscreenMagnifierController()->MoveWindow(-100, 0, false);
  EXPECT_EQ("0,0 400x300", GetViewport().ToString());

  GetFullscreenMagnifierController()->MoveWindow(0, -100, false);
  EXPECT_EQ("0,0 400x300", GetViewport().ToString());

  GetFullscreenMagnifierController()->MoveWindow(-100, -100, false);
  EXPECT_EQ("0,0 400x300", GetViewport().ToString());

  // Confirms that the viewport can't across the bittom-right border.
  GetFullscreenMagnifierController()->MoveWindow(800, 0, false);
  EXPECT_EQ("400,0 400x300", GetViewport().ToString());

  GetFullscreenMagnifierController()->MoveWindow(0, 400, false);
  EXPECT_EQ("0,300 400x300", GetViewport().ToString());

  GetFullscreenMagnifierController()->MoveWindow(200, 400, false);
  EXPECT_EQ("200,300 400x300", GetViewport().ToString());

  GetFullscreenMagnifierController()->MoveWindow(1000, 1000, false);
  EXPECT_EQ("400,300 400x300", GetViewport().ToString());
}

TEST_F(FullscreenMagnifierControllerTest, PointOfInterest) {
  ui::test::EventGenerator* event_generator = GetEventGenerator();

  event_generator->MoveMouseToInHost(gfx::Point(0, 0));
  EXPECT_EQ("0,0", CurrentPointOfInterest());

  event_generator->MoveMouseToInHost(gfx::Point(799, 599));
  EXPECT_EQ("799,599", CurrentPointOfInterest());

  event_generator->MoveMouseToInHost(gfx::Point(400, 300));
  EXPECT_EQ("400,300", CurrentPointOfInterest());

  GetFullscreenMagnifierController()->SetEnabled(true);
  EXPECT_EQ("400,300", CurrentPointOfInterest());

  event_generator->MoveMouseToInHost(gfx::Point(500, 400));
  EXPECT_EQ("450,350", CurrentPointOfInterest());
}

TEST_F(FullscreenMagnifierControllerTest, PanWindow2xLeftToRight) {
  const aura::Env* env = aura::Env::GetInstance();

  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->MoveMouseToInHost(gfx::Point(0, 0));
  EXPECT_EQ(1.f, GetFullscreenMagnifierController()->GetScale());
  EXPECT_EQ("0,0 800x600", GetViewport().ToString());
  EXPECT_EQ("0,0", env->last_mouse_location().ToString());

  // Enables magnifier and confirm the viewport is at center.
  GetFullscreenMagnifierController()->SetEnabled(true);
  EXPECT_EQ(2.0f, GetFullscreenMagnifierController()->GetScale());

  GetFullscreenMagnifierController()->MoveWindow(0, 0, false);
  event_generator->MoveMouseToInHost(gfx::Point(0, 0));
  EXPECT_EQ("0,0", env->last_mouse_location().ToString());
  EXPECT_EQ("0,0 400x300", GetViewport().ToString());

  event_generator->MoveMouseToInHost(gfx::Point(300, 150));
  EXPECT_EQ("150,75", env->last_mouse_location().ToString());
  EXPECT_EQ("0,0 400x300", GetViewport().ToString());

  // Center in the screen.
  event_generator->MoveMouseToInHost(gfx::Point(700, 150));
  EXPECT_EQ("350,75", env->last_mouse_location().ToString());
  EXPECT_EQ("0,0 400x300", GetViewport().ToString());

  // Moving half a DIP (1 px from center) will round up to a full DIP.
  event_generator->MoveMouseToInHost(gfx::Point(701, 150));
  EXPECT_EQ("351,75", env->last_mouse_location().ToString());
  EXPECT_EQ("1,0 400x300", GetViewport().ToString());

  // Moving a full DIP will also create a full DIP of movement.
  event_generator->MoveMouseToInHost(gfx::Point(702, 150));
  EXPECT_EQ("352,75", env->last_mouse_location().ToString());
  EXPECT_EQ("2,0 400x300", GetViewport().ToString());

  // Moving 1.5 DIP (3 px from center) will create 2 DIP of movement, because
  // we round pixels to DIPs rather than truncate.
  event_generator->MoveMouseToInHost(gfx::Point(703, 150));
  EXPECT_EQ("354,75", env->last_mouse_location().ToString());
  EXPECT_EQ("4,0 400x300", GetViewport().ToString());

  // Moving 2 DIP (4 px from center) will create 2 DIP of movement.
  event_generator->MoveMouseToInHost(gfx::Point(704, 150));
  EXPECT_EQ("356,75", env->last_mouse_location().ToString());
  EXPECT_EQ("6,0 400x300", GetViewport().ToString());

  event_generator->MoveMouseToInHost(gfx::Point(712, 150));
  EXPECT_EQ("362,75", env->last_mouse_location().ToString());
  EXPECT_EQ("12,0 400x300", GetViewport().ToString());

  event_generator->MoveMouseToInHost(gfx::Point(600, 150));
  EXPECT_EQ("312,75", env->last_mouse_location().ToString());
  EXPECT_EQ("12,0 400x300", GetViewport().ToString());

  event_generator->MoveMouseToInHost(gfx::Point(720, 150));
  EXPECT_EQ("372,75", env->last_mouse_location().ToString());
  EXPECT_EQ("22,0 400x300", GetViewport().ToString());

  event_generator->MoveMouseToInHost(gfx::Point(780, 150));
  EXPECT_EQ("412,75", env->last_mouse_location().ToString());
  EXPECT_EQ("412,75", CurrentPointOfInterest());
  EXPECT_EQ("62,0 400x300", GetViewport().ToString());

  event_generator->MoveMouseToInHost(gfx::Point(799, 150));
  EXPECT_EQ("462,75", env->last_mouse_location().ToString());
  EXPECT_EQ("112,0 400x300", GetViewport().ToString());

  event_generator->MoveMouseToInHost(gfx::Point(702, 150));
  EXPECT_EQ("463,75", env->last_mouse_location().ToString());
  EXPECT_EQ("113,0 400x300", GetViewport().ToString());

  event_generator->MoveMouseToInHost(gfx::Point(780, 150));
  EXPECT_EQ("503,75", env->last_mouse_location().ToString());
  EXPECT_EQ("153,0 400x300", GetViewport().ToString());

  event_generator->MoveMouseToInHost(gfx::Point(780, 150));
  EXPECT_EQ("543,75", env->last_mouse_location().ToString());
  EXPECT_EQ("193,0 400x300", GetViewport().ToString());

  event_generator->MoveMouseToInHost(gfx::Point(780, 150));
  EXPECT_EQ("583,75", env->last_mouse_location().ToString());
  EXPECT_EQ("233,0 400x300", GetViewport().ToString());

  event_generator->MoveMouseToInHost(gfx::Point(780, 150));
  EXPECT_EQ("623,75", env->last_mouse_location().ToString());
  EXPECT_EQ("273,0 400x300", GetViewport().ToString());

  event_generator->MoveMouseToInHost(gfx::Point(780, 150));
  EXPECT_EQ("663,75", env->last_mouse_location().ToString());
  EXPECT_EQ("313,0 400x300", GetViewport().ToString());

  event_generator->MoveMouseToInHost(gfx::Point(780, 150));
  EXPECT_EQ("703,75", env->last_mouse_location().ToString());
  EXPECT_EQ("353,0 400x300", GetViewport().ToString());

  event_generator->MoveMouseToInHost(gfx::Point(780, 150));
  EXPECT_EQ("743,75", env->last_mouse_location().ToString());
  EXPECT_EQ("393,0 400x300", GetViewport().ToString());

  // Reached the corner.
  event_generator->MoveMouseToInHost(gfx::Point(780, 150));
  EXPECT_EQ("783,75", env->last_mouse_location().ToString());
  EXPECT_EQ("400,0 400x300", GetViewport().ToString());

  event_generator->MoveMouseToInHost(gfx::Point(799, 150));
  EXPECT_EQ("799,75", env->last_mouse_location().ToString());
  EXPECT_EQ("400,0 400x300", GetViewport().ToString());
}

TEST_F(FullscreenMagnifierControllerTest, PanWindow2xRightToLeft) {
  const aura::Env* env = aura::Env::GetInstance();

  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->MoveMouseToInHost(gfx::Point(799, 300));
  EXPECT_EQ(1.f, GetFullscreenMagnifierController()->GetScale());
  EXPECT_EQ("0,0 800x600", GetViewport().ToString());
  EXPECT_EQ("799,300", env->last_mouse_location().ToString());

  // Enables magnifier and confirm the viewport is at center.
  GetFullscreenMagnifierController()->SetEnabled(true);

  event_generator->MoveMouseToInHost(gfx::Point(799, 300));
  EXPECT_EQ("799,300", env->last_mouse_location().ToString());
  EXPECT_EQ("400,150 400x300", GetViewport().ToString());

  event_generator->MoveMouseToInHost(gfx::Point(0, 300));
  EXPECT_EQ("400,300", env->last_mouse_location().ToString());
  EXPECT_EQ("350,150 400x300", GetViewport().ToString());

  event_generator->MoveMouseToInHost(gfx::Point(0, 300));
  EXPECT_EQ("350,300", env->last_mouse_location().ToString());
  EXPECT_EQ("300,150 400x300", GetViewport().ToString());

  event_generator->MoveMouseToInHost(gfx::Point(0, 300));
  EXPECT_EQ("300,300", env->last_mouse_location().ToString());
  EXPECT_EQ("250,150 400x300", GetViewport().ToString());

  event_generator->MoveMouseToInHost(gfx::Point(0, 300));
  EXPECT_EQ("250,300", env->last_mouse_location().ToString());
  EXPECT_EQ("200,150 400x300", GetViewport().ToString());

  event_generator->MoveMouseToInHost(gfx::Point(0, 300));
  EXPECT_EQ("200,300", env->last_mouse_location().ToString());
  EXPECT_EQ("150,150 400x300", GetViewport().ToString());

  event_generator->MoveMouseToInHost(gfx::Point(0, 300));
  EXPECT_EQ("150,300", env->last_mouse_location().ToString());
  EXPECT_EQ("100,150 400x300", GetViewport().ToString());

  event_generator->MoveMouseToInHost(gfx::Point(0, 300));
  EXPECT_EQ("100,300", env->last_mouse_location().ToString());
  EXPECT_EQ("50,150 400x300", GetViewport().ToString());

  event_generator->MoveMouseToInHost(gfx::Point(0, 300));
  EXPECT_EQ("50,300", env->last_mouse_location().ToString());
  EXPECT_EQ("0,150 400x300", GetViewport().ToString());

  event_generator->MoveMouseToInHost(gfx::Point(0, 300));
  EXPECT_EQ("0,300", env->last_mouse_location().ToString());
  EXPECT_EQ("0,150 400x300", GetViewport().ToString());
}

TEST_F(FullscreenMagnifierControllerTest, PanWindowToRight) {
  const aura::Env* env = aura::Env::GetInstance();

  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->MoveMouseToInHost(gfx::Point(400, 300));
  EXPECT_EQ(1.f, GetFullscreenMagnifierController()->GetScale());
  EXPECT_EQ("0,0 800x600", GetViewport().ToString());
  EXPECT_EQ("400,300", env->last_mouse_location().ToString());

  float scale = 2.f;

  // Enables magnifier and confirm the viewport is at center.
  GetFullscreenMagnifierController()->SetEnabled(true);
  EXPECT_FLOAT_EQ(2.f, GetFullscreenMagnifierController()->GetScale());

  scale *= magnifier_utils::kMagnificationScaleFactor;
  GetFullscreenMagnifierController()->SetScale(scale, false);
  EXPECT_FLOAT_EQ(2.3784142, GetFullscreenMagnifierController()->GetScale());
  event_generator->MoveMouseToInHost(gfx::Point(400, 300));
  EXPECT_EQ("400,300", env->last_mouse_location().ToString());
  event_generator->MoveMouseToInHost(gfx::Point(799, 300));
  EXPECT_EQ("567,299", env->last_mouse_location().ToString());
  EXPECT_EQ("705,300", GetHostMouseLocation());

  scale *= magnifier_utils::kMagnificationScaleFactor;
  GetFullscreenMagnifierController()->SetScale(scale, false);
  EXPECT_FLOAT_EQ(2.8284268, GetFullscreenMagnifierController()->GetScale());
  event_generator->MoveMouseToInHost(gfx::Point(799, 300));
  EXPECT_EQ("599,299", env->last_mouse_location().ToString());
  EXPECT_EQ("705,300", GetHostMouseLocation());

  scale *= magnifier_utils::kMagnificationScaleFactor;
  GetFullscreenMagnifierController()->SetScale(scale, false);
  EXPECT_FLOAT_EQ(3.3635852, GetFullscreenMagnifierController()->GetScale());
  event_generator->MoveMouseToInHost(gfx::Point(799, 300));
  EXPECT_EQ("627,298", env->last_mouse_location().ToString());
  EXPECT_EQ("707,300", GetHostMouseLocation());

  scale *= magnifier_utils::kMagnificationScaleFactor;
  GetFullscreenMagnifierController()->SetScale(scale, false);
  EXPECT_FLOAT_EQ(4.f, GetFullscreenMagnifierController()->GetScale());
  event_generator->MoveMouseToInHost(gfx::Point(799, 300));
  EXPECT_EQ("650,298", env->last_mouse_location().ToString());
  EXPECT_EQ("704,300", GetHostMouseLocation());
}

TEST_F(FullscreenMagnifierControllerTest, PanWindowToLeft) {
  const aura::Env* env = aura::Env::GetInstance();

  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->MoveMouseToInHost(gfx::Point(400, 300));
  EXPECT_EQ(1.f, GetFullscreenMagnifierController()->GetScale());
  EXPECT_EQ("0,0 800x600", GetViewport().ToString());
  EXPECT_EQ("400,300", env->last_mouse_location().ToString());

  float scale = 2.f;

  // Enables magnifier and confirm the viewport is at center.
  GetFullscreenMagnifierController()->SetEnabled(true);
  EXPECT_FLOAT_EQ(2.f, GetFullscreenMagnifierController()->GetScale());

  scale *= magnifier_utils::kMagnificationScaleFactor;
  GetFullscreenMagnifierController()->SetScale(scale, false);
  EXPECT_FLOAT_EQ(2.3784142, GetFullscreenMagnifierController()->GetScale());
  event_generator->MoveMouseToInHost(gfx::Point(400, 300));
  EXPECT_EQ("400,300", env->last_mouse_location().ToString());
  event_generator->MoveMouseToInHost(gfx::Point(0, 300));
  EXPECT_EQ("231,299", env->last_mouse_location().ToString());
  EXPECT_EQ("100,300", GetHostMouseLocation());

  scale *= magnifier_utils::kMagnificationScaleFactor;
  GetFullscreenMagnifierController()->SetScale(scale, false);
  EXPECT_FLOAT_EQ(2.8284268, GetFullscreenMagnifierController()->GetScale());
  event_generator->MoveMouseToInHost(gfx::Point(0, 300));
  EXPECT_EQ("195,299", env->last_mouse_location().ToString());
  EXPECT_EQ("99,300", GetHostMouseLocation());

  scale *= magnifier_utils::kMagnificationScaleFactor;
  GetFullscreenMagnifierController()->SetScale(scale, false);
  EXPECT_FLOAT_EQ(3.3635852, GetFullscreenMagnifierController()->GetScale());
  event_generator->MoveMouseToInHost(gfx::Point(0, 300));
  EXPECT_EQ("165,298", env->last_mouse_location().ToString());
  EXPECT_EQ("98,300", GetHostMouseLocation());

  scale *= magnifier_utils::kMagnificationScaleFactor;
  GetFullscreenMagnifierController()->SetScale(scale, false);
  EXPECT_FLOAT_EQ(4.f, GetFullscreenMagnifierController()->GetScale());
  event_generator->MoveMouseToInHost(gfx::Point(0, 300));
  EXPECT_EQ("140,298", env->last_mouse_location().ToString());
  EXPECT_EQ("100,300", GetHostMouseLocation());
}

// Make sure that unified desktop can enter magnified mode.
TEST_F(FullscreenMagnifierControllerTest, EnableMagnifierInUnifiedDesktop) {
  Shell::Get()->display_manager()->SetUnifiedDesktopEnabled(true);

  EXPECT_EQ(1.0f, GetFullscreenMagnifierController()->GetScale());

  GetFullscreenMagnifierController()->SetEnabled(true);

  display::Screen* screen = display::Screen::GetScreen();

  UpdateDisplay("500x400, 500x400");
  EXPECT_EQ("0,0 1000x400", screen->GetPrimaryDisplay().bounds().ToString());
  EXPECT_EQ(2.0f, GetFullscreenMagnifierController()->GetScale());

  GetFullscreenMagnifierController()->SetEnabled(false);

  EXPECT_EQ(1.0f, GetFullscreenMagnifierController()->GetScale());

  GetFullscreenMagnifierController()->SetEnabled(true);
  EXPECT_EQ(2.0f, GetFullscreenMagnifierController()->GetScale());

  UpdateDisplay("500x400");
  EXPECT_EQ("0,0 500x400", screen->GetPrimaryDisplay().bounds().ToString());
  EXPECT_EQ(2.0f, GetFullscreenMagnifierController()->GetScale());

  GetFullscreenMagnifierController()->SetEnabled(false);
  EXPECT_EQ("0,0 500x400", screen->GetPrimaryDisplay().bounds().ToString());
  EXPECT_EQ(1.0f, GetFullscreenMagnifierController()->GetScale());
}

// Make sure that mouse can move across display in magnified mode.
TEST_F(FullscreenMagnifierControllerTest, MoveMouseToSecondDisplay) {
  UpdateDisplay("0+0-500x400, 400+0-500x400");
  EXPECT_EQ(2ul, display::Screen::GetScreen()->GetAllDisplays().size());

  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(gfx::Point(250, 250));
  EXPECT_TRUE(root_windows[1]->layer()->transform().IsIdentity());
  EXPECT_TRUE(root_windows[0]->layer()->transform().IsIdentity());

  GetFullscreenMagnifierController()->SetEnabled(true);
  EXPECT_FALSE(root_windows[0]->layer()->transform().IsIdentity());
  EXPECT_TRUE(root_windows[1]->layer()->transform().IsIdentity());

  event_generator->MoveMouseTo(gfx::Point(750, 250));
  EXPECT_FALSE(root_windows[1]->layer()->transform().IsIdentity());
  EXPECT_TRUE(root_windows[0]->layer()->transform().IsIdentity());

  GetFullscreenMagnifierController()->SetEnabled(false);
  EXPECT_TRUE(root_windows[1]->layer()->transform().IsIdentity());
  EXPECT_TRUE(root_windows[0]->layer()->transform().IsIdentity());
}

TEST_F(FullscreenMagnifierControllerTest, MoveToSecondDisplayWithTouch) {
  UpdateDisplay("0+0-500x400, 500+0-500x400");
  EXPECT_EQ(2ul, display::Screen::GetScreen()->GetAllDisplays().size());

  ui::test::EventGenerator* event_generator = GetEventGenerator();
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  event_generator->GestureTapAt(gfx::Point(250, 250));
  ASSERT_TRUE(root_windows[0]->layer()->transform().IsIdentity());
  ASSERT_TRUE(root_windows[1]->layer()->transform().IsIdentity());

  GetFullscreenMagnifierController()->SetEnabled(true);
  EXPECT_FALSE(root_windows[0]->layer()->transform().IsIdentity());
  EXPECT_TRUE(root_windows[1]->layer()->transform().IsIdentity());

  event_generator->GestureTapAt(gfx::Point(750, 250));
  EXPECT_TRUE(root_windows[0]->layer()->transform().IsIdentity());
  EXPECT_FALSE(root_windows[1]->layer()->transform().IsIdentity());

  GetFullscreenMagnifierController()->SetEnabled(false);
  EXPECT_TRUE(root_windows[0]->layer()->transform().IsIdentity());
  EXPECT_TRUE(root_windows[1]->layer()->transform().IsIdentity());
}

// Performs pinch zoom and confirm that zoom level is changed. This test case
// also tests touch event handling.
TEST_F(FullscreenMagnifierControllerTest, PinchZoom) {
  ASSERT_EQ(0u, touch_event_watcher_->touch_events.size());

  GetFullscreenMagnifierController()->SetEnabled(true);
  ASSERT_EQ(2.0f, GetFullscreenMagnifierController()->GetScale());

  base::TimeTicks time = base::TimeTicks::Now();
  ui::PointerDetails pointer_details1(ui::EventPointerType::kTouch, 0);
  ui::PointerDetails pointer_details2(ui::EventPointerType::kTouch, 1);

  // Simulate pinch gesture.
  DispatchTouchEvent(ui::EventType::kTouchPressed, gfx::Point(900, 10), time,
                     pointer_details1);
  DispatchTouchEvent(ui::EventType::kTouchPressed, gfx::Point(1100, 10), time,
                     pointer_details2);

  ASSERT_EQ(2u, touch_event_watcher_->touch_events.size());
  EXPECT_EQ(ui::EventType::kTouchPressed,
            touch_event_watcher_->touch_events[0].type());
  EXPECT_EQ(ui::EventType::kTouchPressed,
            touch_event_watcher_->touch_events[1].type());

  DispatchTouchEvent(ui::EventType::kTouchMoved, gfx::Point(850, 10), time,
                     pointer_details1);
  DispatchTouchEvent(ui::EventType::kTouchMoved, gfx::Point(1150, 10), time,
                     pointer_details2);

  // Expect that event watcher receives touch cancelled events. Magnification
  // controller should cancel existing touches when it detects interested
  // gestures.
  ASSERT_EQ(4u, touch_event_watcher_->touch_events.size());
  EXPECT_EQ(ui::EventType::kTouchCancelled,
            touch_event_watcher_->touch_events[2].type());
  EXPECT_EQ(ui::EventType::kTouchCancelled,
            touch_event_watcher_->touch_events[3].type());

  DispatchTouchEvent(ui::EventType::kTouchMoved, gfx::Point(800, 10), time,
                     pointer_details1);
  DispatchTouchEvent(ui::EventType::kTouchMoved, gfx::Point(1200, 10), time,
                     pointer_details2);

  DispatchTouchEvent(ui::EventType::kTouchReleased, gfx::Point(800, 10), time,
                     pointer_details1);
  DispatchTouchEvent(ui::EventType::kTouchReleased, gfx::Point(1200, 10), time,
                     pointer_details2);

  // All events are consumed by the controller after it detects gesture.
  EXPECT_EQ(4u, touch_event_watcher_->touch_events.size());

  EXPECT_LT(2.0f, GetFullscreenMagnifierController()->GetScale());

  float ratio = GetFullscreenMagnifierController()->GetScale() / 2.0f;

  // Peform pinch gesture again with 4.0x.
  GetFullscreenMagnifierController()->SetScale(4.0f, false /* animate */);

  DispatchTouchEvent(ui::EventType::kTouchPressed, gfx::Point(900, 10), time,
                     pointer_details1);
  DispatchTouchEvent(ui::EventType::kTouchPressed, gfx::Point(1100, 10), time,
                     pointer_details2);

  DispatchTouchEvent(ui::EventType::kTouchMoved, gfx::Point(800, 10), time,
                     pointer_details1);
  DispatchTouchEvent(ui::EventType::kTouchMoved, gfx::Point(1200, 10), time,
                     pointer_details2);

  DispatchTouchEvent(ui::EventType::kTouchReleased, gfx::Point(800, 10), time,
                     pointer_details1);
  DispatchTouchEvent(ui::EventType::kTouchReleased, gfx::Point(1200, 10), time,
                     pointer_details2);

  float ratio_zoomed = GetFullscreenMagnifierController()->GetScale() / 4.0f;

  // Ratio of zoom level change should be the same regardless of current zoom
  // level.
  EXPECT_GT(0.01f, std::abs(ratio - ratio_zoomed));
}

// Performs pinch zoom and then receive cancelled touch events. This test case
// tests giving back control to other event watchers.
TEST_F(FullscreenMagnifierControllerTest, PinchZoomCancel) {
  ASSERT_EQ(0u, touch_event_watcher_->touch_events.size());

  GetFullscreenMagnifierController()->SetEnabled(true);
  ASSERT_EQ(2.0f, GetFullscreenMagnifierController()->GetScale());

  base::TimeTicks time = base::TimeTicks::Now();
  ui::PointerDetails pointer_details1(ui::EventPointerType::kTouch, 0);
  ui::PointerDetails pointer_details2(ui::EventPointerType::kTouch, 1);

  // Simulate pinch gesture.
  DispatchTouchEvent(ui::EventType::kTouchPressed, gfx::Point(900, 10), time,
                     pointer_details1);
  DispatchTouchEvent(ui::EventType::kTouchPressed, gfx::Point(1100, 10), time,
                     pointer_details2);

  ASSERT_EQ(2u, touch_event_watcher_->touch_events.size());
  EXPECT_EQ(ui::EventType::kTouchPressed,
            touch_event_watcher_->touch_events[0].type());
  EXPECT_EQ(ui::EventType::kTouchPressed,
            touch_event_watcher_->touch_events[1].type());

  DispatchTouchEvent(ui::EventType::kTouchMoved, gfx::Point(850, 10), time,
                     pointer_details1);
  DispatchTouchEvent(ui::EventType::kTouchMoved, gfx::Point(1150, 10), time,
                     pointer_details2);

  // Expect that event watcher receives touch cancelled events. Magnification
  // controller should cancel existing touches when it detects interested
  // gestures.
  ASSERT_EQ(4u, touch_event_watcher_->touch_events.size());
  EXPECT_EQ(ui::EventType::kTouchCancelled,
            touch_event_watcher_->touch_events[2].type());
  EXPECT_EQ(ui::EventType::kTouchCancelled,
            touch_event_watcher_->touch_events[3].type());

  // Dispatch cancelled events (for example due to palm detection).
  DispatchTouchEvent(ui::EventType::kTouchCancelled, gfx::Point(850, 10), time,
                     pointer_details1);
  DispatchTouchEvent(ui::EventType::kTouchCancelled, gfx::Point(1150, 10), time,
                     pointer_details2);

  // All events are consumed by the controller after it detects gesture.
  ASSERT_EQ(4u, touch_event_watcher_->touch_events.size());
  ASSERT_EQ(0, GetFullscreenMagnifierController()->GetTouchPointsForTesting());

  // Touch the screen again.
  DispatchTouchEvent(ui::EventType::kTouchPressed, gfx::Point(900, 10), time,
                     pointer_details1);

  // Events should again be passed to touch_event_watcher.
  ASSERT_EQ(5u, touch_event_watcher_->touch_events.size());
  EXPECT_EQ(ui::EventType::kTouchPressed,
            touch_event_watcher_->touch_events[4].type());

  DispatchTouchEvent(ui::EventType::kTouchMoved, gfx::Point(800, 10), time,
                     pointer_details1);
  ASSERT_EQ(6u, touch_event_watcher_->touch_events.size());
  EXPECT_EQ(ui::EventType::kTouchMoved,
            touch_event_watcher_->touch_events[5].type());

  DispatchTouchEvent(ui::EventType::kTouchReleased, gfx::Point(800, 10), time,
                     pointer_details1);
  ASSERT_EQ(7u, touch_event_watcher_->touch_events.size());
  EXPECT_EQ(ui::EventType::kTouchReleased,
            touch_event_watcher_->touch_events[6].type());
}

TEST_F(FullscreenMagnifierControllerTest, TwoFingersScroll) {
  GetFullscreenMagnifierController()->SetEnabled(true);
  ASSERT_EQ(2.0f, GetFullscreenMagnifierController()->GetScale());

  const gfx::Point initial_position =
      GetFullscreenMagnifierController()->GetWindowPosition();
  PerformTwoFingersScrollGesture(ScrollDirection::kPositiveX);
  const gfx::Point moved_position =
      GetFullscreenMagnifierController()->GetWindowPosition();

  // Confirm that two fingers scroll gesture moves viewport.
  EXPECT_GT(initial_position.x(), moved_position.x());
  EXPECT_EQ(initial_position.y(), moved_position.y());
  EXPECT_EQ(2.0f, GetFullscreenMagnifierController()->GetScale());

  int32_t delta = initial_position.x() - moved_position.x();

  // Perform the same gesture with 4.0x.
  GetFullscreenMagnifierController()->SetScale(4.0f, false /* animate */);

  const gfx::Point initial_position_zoomed =
      GetFullscreenMagnifierController()->GetWindowPosition();
  PerformTwoFingersScrollGesture(ScrollDirection::kPositiveX);
  const gfx::Point moved_position_zoomed =
      GetFullscreenMagnifierController()->GetWindowPosition();

  EXPECT_GT(initial_position_zoomed.x(), moved_position_zoomed.x());
  EXPECT_EQ(initial_position_zoomed.y(), moved_position_zoomed.y());
  EXPECT_EQ(4.0f, GetFullscreenMagnifierController()->GetScale());

  int32_t delta_zoomed =
      initial_position_zoomed.x() - moved_position_zoomed.x();

  // Scrolled delta becomes half with 4.0x compared to 2.0x.
  EXPECT_EQ(delta, delta_zoomed * 2);
}

TEST_F(FullscreenMagnifierControllerTest, TwoFingersScrollRotation) {
  const int64_t internal_display_id =
      display::test::DisplayManagerTestApi(display_manager())
          .SetFirstDisplayAsInternalDisplay();

  GetFullscreenMagnifierController()->SetEnabled(true);
  ASSERT_EQ(2.0f, GetFullscreenMagnifierController()->GetScale());

  // Test two-finger scroll gestures in all rotations in all directions.
  for (const auto& rotation :
       {display::Display::ROTATE_0, display::Display::ROTATE_90,
        display::Display::ROTATE_180, display::Display::ROTATE_270}) {
    SCOPED_TRACE(::testing::Message() << "Testing in rotation: " << rotation);
    display_manager()->SetDisplayRotation(
        internal_display_id, rotation, display::Display::RotationSource::USER);

    for (const auto& scroll_direction : {
             ScrollDirection::kPositiveX,
             ScrollDirection::kNegativeX,
             ScrollDirection::kPositiveY,
             ScrollDirection::kNegativeY,
         }) {
      SCOPED_TRACE(::testing::Message()
                   << "Scroll direction: " << (int)scroll_direction);
      const gfx::Point initial_position =
          GetFullscreenMagnifierController()->GetWindowPosition();
      PerformTwoFingersScrollGesture(scroll_direction);
      const gfx::Point moved_position =
          GetFullscreenMagnifierController()->GetWindowPosition();

      // Confirm that two fingers scroll gesture moves viewport in the right
      // direction.
      switch (scroll_direction) {
        case ScrollDirection::kPositiveX:
          // Viewport moves horizontally to the left.
          EXPECT_GT(initial_position.x(), moved_position.x());
          EXPECT_EQ(initial_position.y(), moved_position.y());
          break;

        case ScrollDirection::kNegativeX:
          // Viewport moves horizontally to the right.
          EXPECT_GT(moved_position.x(), initial_position.x());
          EXPECT_EQ(initial_position.y(), moved_position.y());
          break;

        case ScrollDirection::kPositiveY:
          // Viewport moves vertically up.
          EXPECT_EQ(initial_position.x(), moved_position.x());
          EXPECT_GT(initial_position.y(), moved_position.y());
          break;

        case ScrollDirection::kNegativeY:
          // Viewport moves vertically down.
          EXPECT_EQ(initial_position.x(), moved_position.x());
          EXPECT_GT(moved_position.y(), initial_position.y());
          break;
      }
    }
  }
}

TEST_F(FullscreenMagnifierControllerTest, ZoomsIntoCenter) {
  UpdateDisplay("500x600");

  GetFullscreenMagnifierController()->SetEnabled(true);
  ASSERT_EQ(2.0f, GetFullscreenMagnifierController()->GetScale());

  GetFullscreenMagnifierController()->CenterOnPoint(gfx::Point(250, 300));
  ASSERT_EQ(
      gfx::Point(250, 300),
      GetFullscreenMagnifierController()->GetViewportRect().CenterPoint());

  base::TimeTicks time = base::TimeTicks::Now();
  ui::PointerDetails pointer_details1(ui::EventPointerType::kTouch, 0);
  ui::PointerDetails pointer_details2(ui::EventPointerType::kTouch, 1);

  // Simulate pinch gesture with keeping center of bounding box of touches at
  // (250, 300). Note that GestureProvider dispatches scroll gesture from this
  // touch sequence as well.
  DispatchTouchEvent(ui::EventType::kTouchPressed, gfx::Point(245, 300), time,
                     pointer_details1);
  DispatchTouchEvent(ui::EventType::kTouchPressed, gfx::Point(255, 300), time,
                     pointer_details2);

  DispatchTouchEvent(ui::EventType::kTouchMoved, gfx::Point(145, 300), time,
                     pointer_details1);
  DispatchTouchEvent(ui::EventType::kTouchMoved, gfx::Point(355, 300), time,
                     pointer_details2);

  DispatchTouchEvent(ui::EventType::kTouchReleased, gfx::Point(145, 300), time,
                     pointer_details1);
  DispatchTouchEvent(ui::EventType::kTouchReleased, gfx::Point(355, 300), time,
                     pointer_details2);

  // Confirms that scale has increased with the gesture.
  ASSERT_LT(2.0f, GetFullscreenMagnifierController()->GetScale());

  // Confirms that center is kept at center of pinch gesture. In ideal
  // situation, center of viewport should be kept at (250, 300). But as noted
  // above, scroll gesture caused by touch events for simulating pinch gesture
  // moves the viewport a little. We accept 5 pixels viewport move for the
  // scroll gesture.
  EXPECT_TRUE(gfx::Rect(245, 295, 10, 10)
                  .Contains(GetFullscreenMagnifierController()
                                ->GetViewportRect()
                                .CenterPoint()));
}

// Tests to see if keyboard overscroll is disabled when fullscreen magnification
// is enabled.
TEST_F(FullscreenMagnifierControllerTest, KeyboardOverscrollDisabled) {
  GetFullscreenMagnifierController()->SetEnabled(false);

  auto* keyboard_controller = keyboard::KeyboardUIController::Get();
  bool old_keyboard_overscroll_value =
      keyboard_controller->IsKeyboardOverscrollEnabled();

  // Enable magnification. Keyboard overscroll should be disabled.
  GetFullscreenMagnifierController()->SetEnabled(true);
  EXPECT_FALSE(keyboard_controller->IsKeyboardOverscrollEnabled());

  // Disable magnification. Keyboard overscroll should be back to the way it was
  // before magnification was enabled.
  GetFullscreenMagnifierController()->SetEnabled(false);
  EXPECT_EQ(keyboard_controller->IsKeyboardOverscrollEnabled(),
            old_keyboard_overscroll_value);
}

// Tests that the magnifier gets updated when dragging a window.
TEST_F(FullscreenMagnifierControllerTest, DragWindow) {
  UpdateDisplay("800x700");

  // Create a window and start dragging by grabbing its caption.
  const gfx::Rect initial_window_bounds(200, 200, 400, 400);
  auto window = CreateTestWindow(initial_window_bounds);
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->set_current_screen_location(gfx::Point(205, 205));
  event_generator->PressLeftButton();
  ASSERT_TRUE(WindowState::Get(window.get())->is_dragged());

  GetFullscreenMagnifierController()->SetEnabled(true);
  const gfx::Rect initial_viewport_bounds(GetViewport());

  // Move the mouse around a bit. The viewport should change, and the window
  // bounds should change too.
  event_generator->MoveMouseTo(gfx::Point(1, 1));
  EXPECT_NE(initial_viewport_bounds, GetViewport());

  event_generator->MoveMouseTo(gfx::Point(799, 799));
  EXPECT_NE(initial_viewport_bounds, GetViewport());

  EXPECT_NE(initial_window_bounds, window->bounds());
}

// Tests that the magnifier gets updated while drag a window across displays.
TEST_F(FullscreenMagnifierControllerTest, DragWindowAcrossDisplays) {
  UpdateDisplay("0+0-500x450, 500+0-500x450");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  // Create a window and start dragging by grabbing its caption.
  std::unique_ptr<aura::Window> window =
      CreateTestWindow(gfx::Rect(100, 100, 300, 300));
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->set_current_screen_location(gfx::Point(105, 105));
  event_generator->PressLeftButton();
  ASSERT_TRUE(WindowState::Get(window.get())->is_dragged());

  GetFullscreenMagnifierController()->SetEnabled(true);
  event_generator->MoveMouseToInHost(gfx::Point(250, 250));
  EXPECT_FALSE(root_windows[0]->layer()->transform().IsIdentity());
  EXPECT_TRUE(root_windows[1]->layer()->transform().IsIdentity());

  // Move the cursor manually since EventGenerator uses a hack to move the
  // cursor between displays.
  root_windows[1]->MoveCursorTo(gfx::Point(950, 250));
  event_generator->MoveMouseToInHost(gfx::Point(950, 250));
  EXPECT_TRUE(root_windows[0]->layer()->transform().IsIdentity());
  EXPECT_FALSE(root_windows[1]->layer()->transform().IsIdentity());
}

TEST_F(FullscreenMagnifierControllerTest, CaptureMode) {
  auto* magnifier = GetFullscreenMagnifierController();
  magnifier->SetEnabled(true);
  magnifier->set_mouse_following_mode(MagnifierMouseFollowingMode::kCentered);

  auto* capture_mode_controller = CaptureModeController::Get();
  capture_mode_controller->Start(CaptureModeEntryType::kQuickSettings);

  // Test that the magnifier viewport changes as the cursor moves on random
  // points on the screen or the capture mode bar.
  gfx::Point viewport_center = GetViewport().CenterPoint();
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(gfx::Point{10, 20});
  EXPECT_NE(viewport_center, GetViewport().CenterPoint());
  viewport_center = GetViewport().CenterPoint();
  event_generator->MoveMouseTo(gfx::Point{510, 420});
  EXPECT_NE(viewport_center, GetViewport().CenterPoint());
  viewport_center = GetViewport().CenterPoint();
  const auto* bar_widget = capture_mode_controller->capture_mode_session()
                               ->GetCaptureModeBarWidget();
  const auto point_of_interest =
      bar_widget->GetWindowBoundsInScreen().CenterPoint();
  event_generator->MoveMouseTo(point_of_interest);
  EXPECT_NE(viewport_center, GetViewport().CenterPoint());
}

TEST_F(FullscreenMagnifierControllerTest, ContinuousFollowingReachesEdges) {
  auto* magnifier = GetFullscreenMagnifierController();
  magnifier->SetEnabled(true);
  float scale = 10.0;
  magnifier->SetScale(scale, /*animate=*/false);
  magnifier->set_mouse_following_mode(MagnifierMouseFollowingMode::kContinuous);
  ui::test::EventGenerator* event_generator = GetEventGenerator();

  gfx::Point top_left(0, 0);
  gfx::Point top_right(kRootWidth - 2, 0);
  gfx::Point bottom_left(0, kRootHeight - 2);
  gfx::Point bottom_right(kRootWidth - 1, kRootHeight - 2);

  // Move until viewport upper left corner is at (0, 0).
  // The generator moves the mouse within the scaled window,
  // so it takes several iterations to get to the top corner.
  while (GetViewport().ToString() != "0,0 80x60") {
    event_generator->MoveMouseToInHost(top_left);
  }
  // Reset out of the corner a bit.
  gfx::Point center(400, 300);
  event_generator->MoveMouseToInHost(center);

  // Move until viewport is all the way at the top right.
  while (GetViewport().ToString() != "720,0 80x60") {
    event_generator->MoveMouseToInHost(top_right);
  }
  event_generator->MoveMouseToInHost(center);

  while (GetViewport().ToString() != "0,540 80x60") {
    event_generator->MoveMouseToInHost(bottom_left);
  }
  event_generator->MoveMouseToInHost(center);

  while (GetViewport().ToString() != "720,540 80x60") {
    event_generator->MoveMouseToInHost(bottom_right);
  }

  magnifier->SetEnabled(false);
}

TEST_F(FullscreenMagnifierControllerTest,
       DoesNotGetStuckInCenteredModeAtHighZoom) {
  auto* magnifier = GetFullscreenMagnifierController();
  magnifier->SetEnabled(true);
  // Very high zoom.
  float scale = 20.0;
  magnifier->SetScale(scale, /*animate=*/false);
  magnifier->set_mouse_following_mode(MagnifierMouseFollowingMode::kCentered);

  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->MoveMouseToInHost(kRootWidth / 2, kRootHeight / 2);
  gfx::Rect initial_viewport = GetViewport();

  // Note: EventGenerator cannot generate mouse events at less than 1 DIP of
  // resolution. Call OnMouseEvent manually to try floating-point mouse event
  // locations.

  // Move a partial DIP down. The viewport should move down.
  DispatchMouseMove(gfx::PointF(kRootWidth / 2, kRootHeight / 2 + .5));
  EXPECT_EQ(initial_viewport.y() + 1, GetViewport().y());
  EXPECT_EQ(initial_viewport.x(), GetViewport().x());

  initial_viewport = GetViewport();

  // Move a partial DIP right. The viewport should move right.
  DispatchMouseMove(gfx::PointF(kRootWidth / 2 + .5, kRootHeight / 2 + .5));
  EXPECT_EQ(initial_viewport.x() + 1, GetViewport().x());
  EXPECT_EQ(initial_viewport.y(), GetViewport().y());

  initial_viewport = GetViewport();

  // Move a partial DIP up. The viewport should move up.
  DispatchMouseMove(gfx::PointF(kRootWidth / 2 + .5, kRootHeight / 2));
  EXPECT_EQ(initial_viewport.y() - 1, GetViewport().y());
  EXPECT_EQ(initial_viewport.x(), GetViewport().x());

  initial_viewport = GetViewport();

  // Move a partial DIP left. The viewport should move left.
  DispatchMouseMove(gfx::PointF(kRootWidth / 2, kRootHeight / 2));
  EXPECT_EQ(initial_viewport.x() - 1, GetViewport().x());
  EXPECT_EQ(initial_viewport.y(), GetViewport().y());

  magnifier->SetEnabled(false);
}

TEST_F(FullscreenMagnifierControllerTest, DoesNotRedrawIfViewportIsNotChanged) {
  auto* magnifier = GetFullscreenMagnifierController();
  // Picking a floating point scale makes the float/int conversion
  // more likely to fail (which is what this test guards against).
  magnifier->SetEnabled(true);
  magnifier->set_mouse_following_mode(MagnifierMouseFollowingMode::kEdge);
  magnifier->SetScale(10.345, /*animate=*/false);
  int num_cursor_moves = 0;
  magnifier->set_cursor_moved_callback_for_testing(base::BindLambdaForTesting(
      [&num_cursor_moves](const gfx::Point& point) { num_cursor_moves++; }));

  ui::test::EventGenerator* event_generator = GetEventGenerator();

  // Move until viewport is in bottom right corner, asymetrically.
  gfx::Point bottom_right(kRootWidth - 70, kRootHeight - 53);
  while (GetViewport().ToString() != "722,542 78x58") {
    event_generator->MoveMouseToInHost(bottom_right);
  }

  // The cursor has been moved.
  EXPECT_GT(num_cursor_moves, 1);

  num_cursor_moves = 0;

  // Moving around further doesn't try to move the cursor position and
  // doesn't change the viewport.
  // If this were to happen we could end up in an infinite cursor-moving loop.
  event_generator->MoveMouseToInHost(bottom_right);
  EXPECT_EQ(GetViewport().ToString(), "722,542 78x58");
  EXPECT_EQ(0, num_cursor_moves);
}

}  // namespace ash
