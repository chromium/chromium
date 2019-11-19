// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/magnifier/magnification_controller.h"

#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/keyboard/ui/keyboard_util.h"
#include "ash/magnifier/magnifier_test_utils.h"
#include "ash/magnifier/magnifier_utils.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "ui/aura/env.h"
#include "ui/aura/test/aura_test_utils.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/input_method.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
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

 private:
  DISALLOW_COPY_AND_ASSIGN(TouchEventWatcher);
};

}  // namespace

class MagnificationControllerTest : public AshTestBase {
 public:
  MagnificationControllerTest() {}
  ~MagnificationControllerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    UpdateDisplay(base::StringPrintf("%dx%d", kRootWidth, kRootHeight));

    GetMagnificationController()->DisableMoveMagnifierDelayForTesting();

    touch_event_watcher_ = std::make_unique<TouchEventWatcher>();
    GetRootWindow()->AddPreTargetHandler(touch_event_watcher_.get(),
                                         ui::EventTarget::Priority::kSystem);
  }

  void TearDown() override {
    GetRootWindow()->RemovePreTargetHandler(touch_event_watcher_.get());
    AshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<TouchEventWatcher> touch_event_watcher_;

  aura::Window* GetRootWindow() const { return Shell::GetPrimaryRootWindow(); }

  std::string GetHostMouseLocation() {
    const gfx::Point& location =
        aura::test::QueryLatestMousePositionRequestInHost(
            GetRootWindow()->GetHost());
    return location.ToString();
  }

  ash::MagnificationController* GetMagnificationController() const {
    return ash::Shell::Get()->magnification_controller();
  }

  gfx::Rect GetViewport() const {
    gfx::RectF bounds(0, 0, kRootWidth, kRootHeight);
    GetRootWindow()->layer()->transform().TransformRectReverse(&bounds);
    return gfx::ToEnclosingRect(bounds);
  }

  std::string CurrentPointOfInterest() const {
    return GetMagnificationController()
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

  void PerformTwoFingersScrollGesture() {
    base::TimeTicks time = base::TimeTicks::Now();
    ui::PointerDetails pointer_details1(
        ui::EventPointerType::POINTER_TYPE_TOUCH, 0);
    ui::PointerDetails pointer_details2(
        ui::EventPointerType::POINTER_TYPE_TOUCH, 1);

    DispatchTouchEvent(ui::ET_TOUCH_PRESSED, gfx::Point(150, 10), time,
                       pointer_details1);
    DispatchTouchEvent(ui::ET_TOUCH_PRESSED, gfx::Point(150, 20), time,
                       pointer_details2);

    DispatchTouchEvent(ui::ET_TOUCH_MOVED, gfx::Point(200, 10), time,
                       pointer_details1);
    DispatchTouchEvent(ui::ET_TOUCH_MOVED, gfx::Point(200, 20), time,
                       pointer_details2);

    DispatchTouchEvent(ui::ET_TOUCH_RELEASED, gfx::Point(200, 10), time,
                       pointer_details1);
    DispatchTouchEvent(ui::ET_TOUCH_RELEASED, gfx::Point(200, 20), time,
                       pointer_details2);
  }

  MagnifierTextInputTestHelper text_input_helper_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MagnificationControllerTest);
};

TEST_F(MagnificationControllerTest, EnableAndDisable) {
  // Confirms the magnifier is disabled.
  EXPECT_TRUE(GetRootWindow()->layer()->transform().IsIdentity());
  EXPECT_EQ(1.0f, GetMagnificationController()->GetScale());
  EXPECT_EQ("0,0 800x600", GetViewport().ToString());

  // Enables magnifier.
  GetMagnificationController()->SetEnabled(true);
  EXPECT_FALSE(GetRootWindow()->layer()->transform().IsIdentity());
  EXPECT_EQ(2.0f, GetMagnificationController()->GetScale());
  EXPECT_EQ("200,150 400x300", GetViewport().ToString());

  // Disables magnifier.
  GetMagnificationController()->SetEnabled(false);
  EXPECT_TRUE(GetRootWindow()->layer()->transform().IsIdentity());
  EXPECT_EQ(1.0f, GetMagnificationController()->GetScale());
  EXPECT_EQ("0,0 800x600", GetViewport().ToString());

  // Confirms the the scale can't be changed.
  GetMagnificationController()->SetScale(4.0f, false);
  EXPECT_TRUE(GetRootWindow()->layer()->transform().IsIdentity());
  EXPECT_EQ(1.0f, GetMagnificationController()->GetScale());
  EXPECT_EQ("0,0 800x600", GetViewport().ToString());
}

TEST_F(MagnificationControllerTest, MagnifyAndUnmagnify) {
  // Enables magnifier and confirms the default scale is 2.0x.
  GetMagnificationController()->SetEnabled(true);
  EXPECT_FALSE(GetRootWindow()->layer()->transform().IsIdentity());
  EXPECT_EQ(2.0f, GetMagnificationController()->GetScale());
  EXPECT_EQ("200,150 400x300", GetViewport().ToString());
  EXPECT_EQ("400,300", CurrentPointOfInterest());

  // Changes the scale.
  GetMagnificationController()->SetScale(4.0f, false);
  EXPECT_EQ(4.0f, GetMagnificationController()->GetScale());
  EXPECT_EQ("300,225 200x150", GetViewport().ToString());
  EXPECT_EQ("400,300", CurrentPointOfInterest());

  GetMagnificationController()->SetScale(1.0f, false);
  EXPECT_EQ(1.0f, GetMagnificationController()->GetScale());
  EXPECT_EQ("0,0 800x600", GetViewport().ToString());
  EXPECT_EQ("400,300", CurrentPointOfInterest());

  GetMagnificationController()->SetScale(3.0f, false);
  EXPECT_EQ(3.0f, GetMagnificationController()->GetScale());
  EXPECT_EQ("266,200 267x200", GetViewport().ToString());
  EXPECT_EQ("400,300", CurrentPointOfInterest());
}

TEST_F(MagnificationControllerTest, MoveWindow) {
  // Enables magnifier and confirm the viewport is at center.
  GetMagnificationController()->SetEnabled(true);
  EXPECT_EQ(2.0f, GetMagnificationController()->GetScale());
  EXPECT_EQ("200,150 400x300", GetViewport().ToString());

  // Move the viewport.
  GetMagnificationController()->MoveWindow(0, 0, false);
  EXPECT_EQ("0,0 400x300", GetViewport().ToString());

  GetMagnificationController()->MoveWindow(200, 300, false);
  EXPECT_EQ("200,300 400x300", GetViewport().ToString());

  GetMagnificationController()->MoveWindow(400, 0, false);
  EXPECT_EQ("400,0 400x300", GetViewport().ToString());

  GetMagnificationController()->MoveWindow(400, 300, false);
  EXPECT_EQ("400,300 400x300", GetViewport().ToString());

  // Confirms that the viewport can't across the top-left border.
  GetMagnificationController()->MoveWindow(-100, 0, false);
  EXPECT_EQ("0,0 400x300", GetViewport().ToString());

  GetMagnificationController()->MoveWindow(0, -100, false);
  EXPECT_EQ("0,0 400x300", GetViewport().ToString());

  GetMagnificationController()->MoveWindow(-100, -100, false);
  EXPECT_EQ("0,0 400x300", GetViewport().ToString());

  // Confirms that the viewport can't across the bittom-right border.
  GetMagnificationController()->MoveWindow(800, 0, false);
  EXPECT_EQ("400,0 400x300", GetViewport().ToString());

  GetMagnificationController()->MoveWindow(0, 400, false);
  EXPECT_EQ("0,300 400x300", GetViewport().ToString());

  GetMagnificationController()->MoveWindow(200, 400, false);
  EXPECT_EQ("200,300 400x300", GetViewport().ToString());

  GetMagnificationController()->MoveWindow(1000, 1000, false);
  EXPECT_EQ("400,300 400x300", GetViewport().ToString());
}

TEST_F(MagnificationControllerTest, PointOfInterest) {
  ui::test::EventGenerator* event_generator = GetEventGenerator();

  event_generator->MoveMouseToInHost(gfx::Point(0, 0));
  EXPECT_EQ("0,0", CurrentPointOfInterest());

  event_generator->MoveMouseToInHost(gfx::Point(799, 599));
  EXPECT_EQ("799,599", CurrentPointOfInterest());

  event_generator->MoveMouseToInHost(gfx::Point(400, 300));
  EXPECT_EQ("400,300", CurrentPointOfInterest());

  GetMagnificationController()->SetEnabled(true);
  EXPECT_EQ("400,300", CurrentPointOfInterest());

  event_generator->MoveMouseToInHost(gfx::Point(500, 400));
  EXPECT_EQ("450,350", CurrentPointOfInterest());
}

// TODO(warx): move this test to unit_tests.
TEST_F(MagnificationControllerTest, FollowFocusChanged) {
  // Enables magnifier and confirm the viewport is at center.
  GetMagnificationController()->SetEnabled(true);
  EXPECT_EQ(2.0f, GetMagnificationController()->GetScale());
  EXPECT_EQ("200,150 400x300", GetViewport().ToString());

  // Don't move viewport when focusing edit box.
  GetMagnificationController()->HandleFocusedNodeChanged(
      true, gfx::Rect(0, 0, 10, 10));
  EXPECT_EQ("200,150 400x300", GetViewport().ToString());

  // Move viewport to element in upper left.
  GetMagnificationController()->HandleFocusedNodeChanged(
      false, gfx::Rect(0, 0, 10, 10));
  EXPECT_EQ("0,0 400x300", GetViewport().ToString());

  // Move viewport to element in lower right.
  GetMagnificationController()->HandleFocusedNodeChanged(
      false, gfx::Rect(790, 590, 10, 10));
  EXPECT_EQ("400,300 400x300", GetViewport().ToString());

  // Don't follow focus onto empty rectangle.
  GetMagnificationController()->HandleFocusedNodeChanged(false,
                                                         gfx::Rect(0, 0, 0, 0));
  EXPECT_EQ("400,300 400x300", GetViewport().ToString());
}

TEST_F(MagnificationControllerTest, PanWindow2xLeftToRight) {
  const aura::Env* env = aura::Env::GetInstance();

  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->MoveMouseToInHost(gfx::Point(0, 0));
  EXPECT_EQ(1.f, GetMagnificationController()->GetScale());
  EXPECT_EQ("0,0 800x600", GetViewport().ToString());
  EXPECT_EQ("0,0", env->last_mouse_location().ToString());

  // Enables magnifier and confirm the viewport is at center.
  GetMagnificationController()->SetEnabled(true);
  EXPECT_EQ(2.0f, GetMagnificationController()->GetScale());

  GetMagnificationController()->MoveWindow(0, 0, false);
  event_generator->MoveMouseToInHost(gfx::Point(0, 0));
  EXPECT_EQ("0,0", env->last_mouse_location().ToString());
  EXPECT_EQ("0,0 400x300", GetViewport().ToString());

  event_generator->MoveMouseToInHost(gfx::Point(300, 150));
  EXPECT_EQ("150,75", env->last_mouse_location().ToString());
  EXPECT_EQ("0,0 400x300", GetViewport().ToString());

  event_generator->MoveMouseToInHost(gfx::Point(700, 150));
  EXPECT_EQ("350,75", env->last_mouse_location().ToString());
  EXPECT_EQ("0,0 400x300", GetViewport().ToString());

  event_generator->MoveMouseToInHost(gfx::Point(701, 150));
  EXPECT_EQ("350,75", env->last_mouse_location().ToString());
  EXPECT_EQ("0,0 400x300", GetViewport().ToString());

  event_generator->MoveMouseToInHost(gfx::Point(702, 150));
  EXPECT_EQ("351,75", env->last_mouse_location().ToString());
  EXPECT_EQ("1,0 400x300", GetViewport().ToString());

  event_generator->MoveMouseToInHost(gfx::Point(703, 150));
  EXPECT_EQ("352,75", env->last_mouse_location().ToString());
  EXPECT_EQ("2,0 400x300", GetViewport().ToString());

  event_generator->MoveMouseToInHost(gfx::Point(704, 150));
  EXPECT_EQ("354,75", env->last_mouse_location().ToString());
  EXPECT_EQ("4,0 400x300", GetViewport().ToString());

  event_generator->MoveMouseToInHost(gfx::Point(712, 150));
  EXPECT_EQ("360,75", env->last_mouse_location().ToString());
  EXPECT_EQ("10,0 400x300", GetViewport().ToString());

  event_generator->MoveMouseToInHost(gfx::Point(600, 150));
  EXPECT_EQ("310,75", env->last_mouse_location().ToString());
  EXPECT_EQ("10,0 400x300", GetViewport().ToString());

  event_generator->MoveMouseToInHost(gfx::Point(720, 150));
  EXPECT_EQ("370,75", env->last_mouse_location().ToString());
  EXPECT_EQ("20,0 400x300", GetViewport().ToString());

  event_generator->MoveMouseToInHost(gfx::Point(780, 150));
  EXPECT_EQ("410,75", env->last_mouse_location().ToString());
  EXPECT_EQ("410,75", CurrentPointOfInterest());
  EXPECT_EQ("60,0 400x300", GetViewport().ToString());

  event_generator->MoveMouseToInHost(gfx::Point(799, 150));
  EXPECT_EQ("459,75", env->last_mouse_location().ToString());
  EXPECT_EQ("109,0 400x300", GetViewport().ToString());

  event_generator->MoveMouseToInHost(gfx::Point(702, 150));
  EXPECT_EQ("460,75", env->last_mouse_location().ToString());
  EXPECT_EQ("110,0 400x300", GetViewport().ToString());

  event_generator->MoveMouseToInHost(gfx::Point(780, 150));
  EXPECT_EQ("500,75", env->last_mouse_location().ToString());
  EXPECT_EQ("150,0 400x300", GetViewport().ToString());

  event_generator->MoveMouseToInHost(gfx::Point(780, 150));
  EXPECT_EQ("540,75", env->last_mouse_location().ToString());
  EXPECT_EQ("190,0 400x300", GetViewport().ToString());

  event_generator->MoveMouseToInHost(gfx::Point(780, 150));
  EXPECT_EQ("580,75", env->last_mouse_location().ToString());
  EXPECT_EQ("230,0 400x300", GetViewport().ToString());

  event_generator->MoveMouseToInHost(gfx::Point(780, 150));
  EXPECT_EQ("620,75", env->last_mouse_location().ToString());
  EXPECT_EQ("270,0 400x300", GetViewport().ToString());

  event_generator->MoveMouseToInHost(gfx::Point(780, 150));
  EXPECT_EQ("660,75", env->last_mouse_location().ToString());
  EXPECT_EQ("310,0 400x300", GetViewport().ToString());

  event_generator->MoveMouseToInHost(gfx::Point(780, 150));
  EXPECT_EQ("700,75", env->last_mouse_location().ToString());
  EXPECT_EQ("350,0 400x300", GetViewport().ToString());

  event_generator->MoveMouseToInHost(gfx::Point(780, 150));
  EXPECT_EQ("740,75", env->last_mouse_location().ToString());
  EXPECT_EQ("390,0 400x300", GetViewport().ToString());

  event_generator->MoveMouseToInHost(gfx::Point(780, 150));
  EXPECT_EQ("780,75", env->last_mouse_location().ToString());
  EXPECT_EQ("400,0 400x300", GetViewport().ToString());

  event_generator->MoveMouseToInHost(gfx::Point(799, 150));
  EXPECT_EQ("799,75", env->last_mouse_location().ToString());
  EXPECT_EQ("400,0 400x300", GetViewport().ToString());
}

TEST_F(MagnificationControllerTest, PanWindow2xRightToLeft) {
  const aura::Env* env = aura::Env::GetInstance();

  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->MoveMouseToInHost(gfx::Point(799, 300));
  EXPECT_EQ(1.f, GetMagnificationController()->GetScale());
  EXPECT_EQ("0,0 800x600", GetViewport().ToString());
  EXPECT_EQ("799,300", env->last_mouse_location().ToString());

  // Enables magnifier and confirm the viewport is at center.
  GetMagnificationController()->SetEnabled(true);

  event_generator->MoveMouseToInHost(gfx::Point(799, 300));
  EXPECT_EQ("798,300", env->last_mouse_location().ToString());
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

TEST_F(MagnificationControllerTest, PanWindowToRight) {
  const aura::Env* env = aura::Env::GetInstance();

  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->MoveMouseToInHost(gfx::Point(400, 300));
  EXPECT_EQ(1.f, GetMagnificationController()->GetScale());
  EXPECT_EQ("0,0 800x600", GetViewport().ToString());
  EXPECT_EQ("400,300", env->last_mouse_location().ToString());

  float scale = 2.f;

  // Enables magnifier and confirm the viewport is at center.
  GetMagnificationController()->SetEnabled(true);
  EXPECT_FLOAT_EQ(2.f, GetMagnificationController()->GetScale());

  scale *= magnifier_utils::kMagnificationScaleFactor;
  GetMagnificationController()->SetScale(scale, false);
  EXPECT_FLOAT_EQ(2.3784142, GetMagnificationController()->GetScale());
  event_generator->MoveMouseToInHost(gfx::Point(400, 300));
  EXPECT_EQ("400,300", env->last_mouse_location().ToString());
  event_generator->MoveMouseToInHost(gfx::Point(799, 300));
  EXPECT_EQ("566,299", env->last_mouse_location().ToString());
  EXPECT_EQ("705,300", GetHostMouseLocation());

  scale *= magnifier_utils::kMagnificationScaleFactor;
  GetMagnificationController()->SetScale(scale, false);
  EXPECT_FLOAT_EQ(2.8284268, GetMagnificationController()->GetScale());
  event_generator->MoveMouseToInHost(gfx::Point(799, 300));
  EXPECT_EQ("599,299", env->last_mouse_location().ToString());
  EXPECT_EQ("702,300", GetHostMouseLocation());

  scale *= magnifier_utils::kMagnificationScaleFactor;
  GetMagnificationController()->SetScale(scale, false);
  EXPECT_FLOAT_EQ(3.3635852, GetMagnificationController()->GetScale());
  event_generator->MoveMouseToInHost(gfx::Point(799, 300));
  EXPECT_EQ("627,298", env->last_mouse_location().ToString());
  EXPECT_EQ("707,300", GetHostMouseLocation());

  scale *= magnifier_utils::kMagnificationScaleFactor;
  GetMagnificationController()->SetScale(scale, false);
  EXPECT_FLOAT_EQ(4.f, GetMagnificationController()->GetScale());
  event_generator->MoveMouseToInHost(gfx::Point(799, 300));
  EXPECT_EQ("649,298", env->last_mouse_location().ToString());
  EXPECT_EQ("704,300", GetHostMouseLocation());
}

TEST_F(MagnificationControllerTest, PanWindowToLeft) {
  const aura::Env* env = aura::Env::GetInstance();

  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->MoveMouseToInHost(gfx::Point(400, 300));
  EXPECT_EQ(1.f, GetMagnificationController()->GetScale());
  EXPECT_EQ("0,0 800x600", GetViewport().ToString());
  EXPECT_EQ("400,300", env->last_mouse_location().ToString());

  float scale = 2.f;

  // Enables magnifier and confirm the viewport is at center.
  GetMagnificationController()->SetEnabled(true);
  EXPECT_FLOAT_EQ(2.f, GetMagnificationController()->GetScale());

  scale *= magnifier_utils::kMagnificationScaleFactor;
  GetMagnificationController()->SetScale(scale, false);
  EXPECT_FLOAT_EQ(2.3784142, GetMagnificationController()->GetScale());
  event_generator->MoveMouseToInHost(gfx::Point(400, 300));
  EXPECT_EQ("400,300", env->last_mouse_location().ToString());
  event_generator->MoveMouseToInHost(gfx::Point(0, 300));
  EXPECT_EQ("231,299", env->last_mouse_location().ToString());
  EXPECT_EQ("100,300", GetHostMouseLocation());

  scale *= magnifier_utils::kMagnificationScaleFactor;
  GetMagnificationController()->SetScale(scale, false);
  EXPECT_FLOAT_EQ(2.8284268, GetMagnificationController()->GetScale());
  event_generator->MoveMouseToInHost(gfx::Point(0, 300));
  EXPECT_EQ("194,299", env->last_mouse_location().ToString());
  EXPECT_EQ("99,300", GetHostMouseLocation());

  scale *= magnifier_utils::kMagnificationScaleFactor;
  GetMagnificationController()->SetScale(scale, false);
  EXPECT_FLOAT_EQ(3.3635852, GetMagnificationController()->GetScale());
  event_generator->MoveMouseToInHost(gfx::Point(0, 300));
  EXPECT_EQ("164,298", env->last_mouse_location().ToString());
  EXPECT_EQ("98,300", GetHostMouseLocation());

  scale *= magnifier_utils::kMagnificationScaleFactor;
  GetMagnificationController()->SetScale(scale, false);
  EXPECT_FLOAT_EQ(4.f, GetMagnificationController()->GetScale());
  event_generator->MoveMouseToInHost(gfx::Point(0, 300));
  EXPECT_EQ("139,298", env->last_mouse_location().ToString());
  EXPECT_EQ("100,300", GetHostMouseLocation());
}

TEST_F(MagnificationControllerTest, FocusChangeEvents) {
  MagnifierFocusTestHelper focus_test_helper;
  focus_test_helper.CreateAndShowFocusTestView(gfx::Point(100, 200));

  // Enables magnifier and confirm the viewport is at center.
  GetMagnificationController()->SetEnabled(true);
  EXPECT_EQ(2.0f, GetMagnificationController()->GetScale());
  EXPECT_EQ("200,150 400x300", GetViewport().ToString());
  EXPECT_FALSE(GetMagnificationController()->KeepFocusCentered());

  // Focus on the first button and expect the magnifier to be centered around
  // its center.
  focus_test_helper.FocusFirstButton();
  gfx::Point button_1_center(
      focus_test_helper.GetFirstButtonBoundsInRoot().CenterPoint());
  EXPECT_EQ(button_1_center, GetViewport().CenterPoint());

  // Similarly if we focus on the second button.
  focus_test_helper.FocusSecondButton();
  gfx::Point button_2_center(
      focus_test_helper.GetSecondButtonBoundsInRoot().CenterPoint());
  EXPECT_EQ(button_2_center, GetViewport().CenterPoint());
}

TEST_F(MagnificationControllerTest, FollowTextInputFieldFocus) {
  text_input_helper_.CreateAndShowTextInputView(gfx::Rect(500, 300, 80, 80));
  gfx::Rect text_input_bounds = text_input_helper_.GetTextInputViewBounds();

  // Enables magnifier and confirm the viewport is at center.
  GetMagnificationController()->SetEnabled(true);
  EXPECT_EQ(2.0f, GetMagnificationController()->GetScale());
  EXPECT_EQ("200,150 400x300", GetViewport().ToString());
  EXPECT_FALSE(GetMagnificationController()->KeepFocusCentered());

  // Move the viewport to (0, 0), so that text input field will be out of
  // the viewport region.
  GetMagnificationController()->MoveWindow(0, 0, false);
  EXPECT_EQ("0,0 400x300", GetViewport().ToString());
  EXPECT_FALSE(GetViewport().Intersects(text_input_bounds));

  // Focus on the text input field.
  text_input_helper_.FocusOnTextInputView();

  // Verify the view port has been moved to the place where the text field is
  // contained in the view port and the caret is at the center of the view port.
  gfx::Rect view_port = GetViewport();
  EXPECT_TRUE(view_port.Contains(text_input_bounds));
  gfx::Rect caret_bounds = text_input_helper_.GetCaretBounds();
  EXPECT_TRUE(text_input_bounds.Contains(caret_bounds));
  EXPECT_EQ(caret_bounds.CenterPoint(), view_port.CenterPoint());
}

// Tests the following case. First the text input field intersects on the right
// edge with the view port, with focus caret sitting just a little left to the
// caret panning margin, so that when it gets focus, the view port won't move.
// Then when user types a character, the caret moves beyond the right panning
// edge, the view port will be moved to center the caret horizontally.
TEST_F(MagnificationControllerTest, FollowTextInputFieldKeyPress) {
  const int kCaretPanningMargin = 50;
  const int kScale = 2.0f;
  const int kViewportWidth = 400;
  // Add some extra distance horizontally from text caret to to left edge of
  // the text input view.
  int x = kViewportWidth - (kCaretPanningMargin + 20) / kScale;
  text_input_helper_.CreateAndShowTextInputView(gfx::Rect(x, 200, 80, 80));
  gfx::Rect text_input_bounds = text_input_helper_.GetTextInputViewBounds();

  // Enables magnifier and confirm the viewport is at center.
  GetMagnificationController()->SetEnabled(true);
  EXPECT_EQ(2.0f, GetMagnificationController()->GetScale());
  EXPECT_EQ("200,150 400x300", GetViewport().ToString());
  EXPECT_FALSE(GetMagnificationController()->KeepFocusCentered());

  // Move the viewport to (0, 0), so that text input field intersects the
  // view port at the right edge.
  GetMagnificationController()->MoveWindow(0, 0, false);
  EXPECT_EQ("0,0 400x300", GetViewport().ToString());
  EXPECT_TRUE(GetViewport().Intersects(text_input_bounds));

  // Focus on the text input field.
  text_input_helper_.FocusOnTextInputView();

  // Verify the view port is not moved, and the caret is inside the view port
  // and not beyond the caret right panning margin.
  gfx::Rect view_port = GetViewport();
  EXPECT_EQ("0,0 400x300", view_port.ToString());
  EXPECT_TRUE(text_input_bounds.Contains(text_input_helper_.GetCaretBounds()));
  EXPECT_GT(view_port.right() - kCaretPanningMargin / kScale,
            text_input_helper_.GetCaretBounds().x());

  // Press keys on text input simulate typing on text field and the caret
  // moves beyond the caret right panning margin. The view port is moved to the
  // place where caret's x coordinate is centered at the new view port.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->PressKey(ui::VKEY_A, 0);
  event_generator->ReleaseKey(ui::VKEY_A, 0);
  gfx::Rect caret_bounds = text_input_helper_.GetCaretBounds();
  EXPECT_LT(view_port.right() - kCaretPanningMargin / kScale,
            text_input_helper_.GetCaretBounds().x());

  gfx::Rect new_view_port = GetViewport();
  EXPECT_EQ(caret_bounds.CenterPoint().x(), new_view_port.CenterPoint().x());
}

TEST_F(MagnificationControllerTest, CenterTextCaretNotInsideViewport) {
  text_input_helper_.CreateAndShowTextInputView(gfx::Rect(500, 300, 50, 30));
  gfx::Rect text_input_bounds = text_input_helper_.GetTextInputViewBounds();

  // Enables magnifier and confirm the viewport is at center.
  GetMagnificationController()->SetKeepFocusCentered(true);
  GetMagnificationController()->SetEnabled(true);
  EXPECT_EQ(2.0f, GetMagnificationController()->GetScale());
  EXPECT_EQ("200,150 400x300", GetViewport().ToString());
  EXPECT_TRUE(GetMagnificationController()->KeepFocusCentered());

  // Move the viewport to (0, 0), so that text input field will be out of
  // the viewport region.
  GetMagnificationController()->MoveWindow(0, 0, false);
  EXPECT_EQ("0,0 400x300", GetViewport().ToString());
  EXPECT_FALSE(GetViewport().Contains(text_input_bounds));

  // Focus on the text input field.
  text_input_helper_.FocusOnTextInputView();
  base::RunLoop().RunUntilIdle();
  // Verify the view port has been moved to the place where the text field is
  // contained in the view port and the caret is at the center of the view port.
  gfx::Rect view_port = GetViewport();
  EXPECT_TRUE(view_port.Contains(text_input_bounds));
  gfx::Rect caret_bounds = text_input_helper_.GetCaretBounds();
  EXPECT_EQ(caret_bounds.CenterPoint(), view_port.CenterPoint());

  // Press keys on text input simulate typing on text field and the view port
  // should be moved to keep the caret centered.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->PressKey(ui::VKEY_A, 0);
  event_generator->ReleaseKey(ui::VKEY_A, 0);
  base::RunLoop().RunUntilIdle();
  gfx::Rect new_caret_bounds = text_input_helper_.GetCaretBounds();
  EXPECT_NE(caret_bounds, new_caret_bounds);

  gfx::Rect new_view_port = GetViewport();
  EXPECT_NE(view_port, new_view_port);
  EXPECT_TRUE(new_view_port.Contains(new_caret_bounds));
  EXPECT_EQ(new_caret_bounds.CenterPoint(), new_view_port.CenterPoint());
}

TEST_F(MagnificationControllerTest, CenterTextCaretInViewport) {
  text_input_helper_.CreateAndShowTextInputView(gfx::Rect(250, 200, 50, 30));
  gfx::Rect text_input_bounds = text_input_helper_.GetTextInputViewBounds();

  // Enables magnifier and confirm the viewport is at center.
  GetMagnificationController()->SetKeepFocusCentered(true);
  GetMagnificationController()->SetEnabled(true);
  EXPECT_EQ(2.0f, GetMagnificationController()->GetScale());
  EXPECT_EQ("200,150 400x300", GetViewport().ToString());
  EXPECT_TRUE(GetMagnificationController()->KeepFocusCentered());

  // Verify the text input field is inside the view port.
  gfx::Rect view_port = GetViewport();
  EXPECT_TRUE(view_port.Contains(text_input_bounds));

  // Focus on the text input field.
  text_input_helper_.FocusOnTextInputView();
  base::RunLoop().RunUntilIdle();

  // Verify the view port has been moved to the place where the text field is
  // contained in the view port and the caret is at the center of the view port.
  gfx::Rect new_view_port = GetViewport();
  EXPECT_NE(view_port, new_view_port);
  EXPECT_TRUE(new_view_port.Contains(text_input_bounds));
  gfx::Rect caret_bounds = text_input_helper_.GetCaretBounds();
  EXPECT_EQ(caret_bounds.CenterPoint(), new_view_port.CenterPoint());
}

// Make sure that unified desktop can enter magnified mode.
TEST_F(MagnificationControllerTest, EnableMagnifierInUnifiedDesktop) {
  Shell::Get()->display_manager()->SetUnifiedDesktopEnabled(true);

  EXPECT_EQ(1.0f, GetMagnificationController()->GetScale());

  GetMagnificationController()->SetEnabled(true);

  display::Screen* screen = display::Screen::GetScreen();

  UpdateDisplay("500x500, 500x500");
  EXPECT_EQ("0,0 1000x500", screen->GetPrimaryDisplay().bounds().ToString());
  EXPECT_EQ(2.0f, GetMagnificationController()->GetScale());

  GetMagnificationController()->SetEnabled(false);

  EXPECT_EQ(1.0f, GetMagnificationController()->GetScale());

  GetMagnificationController()->SetEnabled(true);
  EXPECT_EQ(2.0f, GetMagnificationController()->GetScale());

  UpdateDisplay("500x500");
  EXPECT_EQ("0,0 500x500", screen->GetPrimaryDisplay().bounds().ToString());
  EXPECT_EQ(2.0f, GetMagnificationController()->GetScale());

  GetMagnificationController()->SetEnabled(false);
  EXPECT_EQ("0,0 500x500", screen->GetPrimaryDisplay().bounds().ToString());
  EXPECT_EQ(1.0f, GetMagnificationController()->GetScale());
}

// Make sure that mouse can move across display in magnified mode.
TEST_F(MagnificationControllerTest, MoveMouseToSecondDisplay) {
  UpdateDisplay("0+0-500x500, 500+0-500x500");
  EXPECT_EQ(2ul, display::Screen::GetScreen()->GetAllDisplays().size());

  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(gfx::Point(250, 250));
  EXPECT_TRUE(root_windows[1]->layer()->transform().IsIdentity());
  EXPECT_TRUE(root_windows[0]->layer()->transform().IsIdentity());

  GetMagnificationController()->SetEnabled(true);
  EXPECT_FALSE(root_windows[0]->layer()->transform().IsIdentity());
  EXPECT_TRUE(root_windows[1]->layer()->transform().IsIdentity());

  event_generator->MoveMouseTo(gfx::Point(750, 250));
  EXPECT_FALSE(root_windows[1]->layer()->transform().IsIdentity());
  EXPECT_TRUE(root_windows[0]->layer()->transform().IsIdentity());

  GetMagnificationController()->SetEnabled(false);
  EXPECT_TRUE(root_windows[1]->layer()->transform().IsIdentity());
  EXPECT_TRUE(root_windows[0]->layer()->transform().IsIdentity());
}

TEST_F(MagnificationControllerTest, MoveToSecondDisplayWithTouch) {
  UpdateDisplay("0+0-500x500, 500+0-500x500");
  EXPECT_EQ(2ul, display::Screen::GetScreen()->GetAllDisplays().size());

  ui::test::EventGenerator* event_generator = GetEventGenerator();
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  event_generator->GestureTapAt(gfx::Point(250, 250));
  ASSERT_TRUE(root_windows[0]->layer()->transform().IsIdentity());
  ASSERT_TRUE(root_windows[1]->layer()->transform().IsIdentity());

  GetMagnificationController()->SetEnabled(true);
  EXPECT_FALSE(root_windows[0]->layer()->transform().IsIdentity());
  EXPECT_TRUE(root_windows[1]->layer()->transform().IsIdentity());

  event_generator->GestureTapAt(gfx::Point(750, 250));
  EXPECT_TRUE(root_windows[0]->layer()->transform().IsIdentity());
  EXPECT_FALSE(root_windows[1]->layer()->transform().IsIdentity());

  GetMagnificationController()->SetEnabled(false);
  EXPECT_TRUE(root_windows[0]->layer()->transform().IsIdentity());
  EXPECT_TRUE(root_windows[1]->layer()->transform().IsIdentity());
}

// Performs pinch zoom and confirm that zoom level is changed. This test case
// also tests touch event handling.
TEST_F(MagnificationControllerTest, PinchZoom) {
  ASSERT_EQ(0u, touch_event_watcher_->touch_events.size());

  GetMagnificationController()->SetEnabled(true);
  ASSERT_EQ(2.0f, GetMagnificationController()->GetScale());

  base::TimeTicks time = base::TimeTicks::Now();
  ui::PointerDetails pointer_details1(ui::EventPointerType::POINTER_TYPE_TOUCH,
                                      0);
  ui::PointerDetails pointer_details2(ui::EventPointerType::POINTER_TYPE_TOUCH,
                                      1);

  // Simulate pinch gesture.
  DispatchTouchEvent(ui::ET_TOUCH_PRESSED, gfx::Point(900, 10), time,
                     pointer_details1);
  DispatchTouchEvent(ui::ET_TOUCH_PRESSED, gfx::Point(1100, 10), time,
                     pointer_details2);

  ASSERT_EQ(2u, touch_event_watcher_->touch_events.size());
  EXPECT_EQ(ui::ET_TOUCH_PRESSED, touch_event_watcher_->touch_events[0].type());
  EXPECT_EQ(ui::ET_TOUCH_PRESSED, touch_event_watcher_->touch_events[1].type());

  DispatchTouchEvent(ui::ET_TOUCH_MOVED, gfx::Point(850, 10), time,
                     pointer_details1);
  DispatchTouchEvent(ui::ET_TOUCH_MOVED, gfx::Point(1150, 10), time,
                     pointer_details2);

  // Expect that event watcher receives touch cancelled events. Magnification
  // controller should cancel existing touches when it detects interested
  // gestures.
  ASSERT_EQ(4u, touch_event_watcher_->touch_events.size());
  EXPECT_EQ(ui::ET_TOUCH_CANCELLED,
            touch_event_watcher_->touch_events[2].type());
  EXPECT_EQ(ui::ET_TOUCH_CANCELLED,
            touch_event_watcher_->touch_events[3].type());

  DispatchTouchEvent(ui::ET_TOUCH_MOVED, gfx::Point(800, 10), time,
                     pointer_details1);
  DispatchTouchEvent(ui::ET_TOUCH_MOVED, gfx::Point(1200, 10), time,
                     pointer_details2);

  DispatchTouchEvent(ui::ET_TOUCH_RELEASED, gfx::Point(800, 10), time,
                     pointer_details1);
  DispatchTouchEvent(ui::ET_TOUCH_RELEASED, gfx::Point(1200, 10), time,
                     pointer_details2);

  // All events are consumed by the controller after it detects gesture.
  EXPECT_EQ(4u, touch_event_watcher_->touch_events.size());

  EXPECT_LT(2.0f, GetMagnificationController()->GetScale());

  float ratio = GetMagnificationController()->GetScale() / 2.0f;

  // Peform pinch gesture again with 4.0x.
  GetMagnificationController()->SetScale(4.0f, false /* animate */);

  DispatchTouchEvent(ui::ET_TOUCH_PRESSED, gfx::Point(900, 10), time,
                     pointer_details1);
  DispatchTouchEvent(ui::ET_TOUCH_PRESSED, gfx::Point(1100, 10), time,
                     pointer_details2);

  DispatchTouchEvent(ui::ET_TOUCH_MOVED, gfx::Point(800, 10), time,
                     pointer_details1);
  DispatchTouchEvent(ui::ET_TOUCH_MOVED, gfx::Point(1200, 10), time,
                     pointer_details2);

  DispatchTouchEvent(ui::ET_TOUCH_RELEASED, gfx::Point(800, 10), time,
                     pointer_details1);
  DispatchTouchEvent(ui::ET_TOUCH_RELEASED, gfx::Point(1200, 10), time,
                     pointer_details2);

  float ratio_zoomed = GetMagnificationController()->GetScale() / 4.0f;

  // Ratio of zoom level change should be the same regardless of current zoom
  // level.
  EXPECT_GT(0.01f, std::abs(ratio - ratio_zoomed));
}

TEST_F(MagnificationControllerTest, TwoFingersScroll) {
  GetMagnificationController()->SetEnabled(true);
  ASSERT_EQ(2.0f, GetMagnificationController()->GetScale());

  const gfx::Point initial_position =
      GetMagnificationController()->GetWindowPosition();
  PerformTwoFingersScrollGesture();
  const gfx::Point moved_position =
      GetMagnificationController()->GetWindowPosition();

  // Confirm that two fingers scroll gesture moves viewport.
  EXPECT_GT(initial_position.x(), moved_position.x());
  EXPECT_EQ(initial_position.y(), moved_position.y());
  EXPECT_EQ(2.0f, GetMagnificationController()->GetScale());

  int32_t delta = initial_position.x() - moved_position.x();

  // Perform the same gesture with 4.0x.
  GetMagnificationController()->SetScale(4.0f, false /* animate */);

  const gfx::Point initial_position_zoomed =
      GetMagnificationController()->GetWindowPosition();
  PerformTwoFingersScrollGesture();
  const gfx::Point moved_position_zoomed =
      GetMagnificationController()->GetWindowPosition();

  EXPECT_GT(initial_position_zoomed.x(), moved_position_zoomed.x());
  EXPECT_EQ(initial_position_zoomed.y(), moved_position_zoomed.y());
  EXPECT_EQ(4.0f, GetMagnificationController()->GetScale());

  int32_t delta_zoomed =
      initial_position_zoomed.x() - moved_position_zoomed.x();

  // Scrolled delta becomes half with 4.0x compared to 2.0x.
  EXPECT_EQ(delta, delta_zoomed * 2);
}

TEST_F(MagnificationControllerTest, ZoomsIntoCenter) {
  UpdateDisplay("0+0-500x500");

  GetMagnificationController()->SetEnabled(true);
  ASSERT_EQ(2.0f, GetMagnificationController()->GetScale());

  GetMagnificationController()->CenterOnPoint(gfx::Point(250, 250));
  ASSERT_EQ(gfx::Point(250, 250),
            GetMagnificationController()->GetViewportRect().CenterPoint());

  base::TimeTicks time = base::TimeTicks::Now();
  ui::PointerDetails pointer_details1(ui::EventPointerType::POINTER_TYPE_TOUCH,
                                      0);
  ui::PointerDetails pointer_details2(ui::EventPointerType::POINTER_TYPE_TOUCH,
                                      1);

  // Simulate pinch gesture with keeping center of bounding box of touches at
  // (250, 250). Note that GestureProvider dispatches scroll gesture from this
  // touch sequence as well.
  DispatchTouchEvent(ui::ET_TOUCH_PRESSED, gfx::Point(245, 250), time,
                     pointer_details1);
  DispatchTouchEvent(ui::ET_TOUCH_PRESSED, gfx::Point(255, 250), time,
                     pointer_details2);

  DispatchTouchEvent(ui::ET_TOUCH_MOVED, gfx::Point(145, 250), time,
                     pointer_details1);
  DispatchTouchEvent(ui::ET_TOUCH_MOVED, gfx::Point(355, 250), time,
                     pointer_details2);

  DispatchTouchEvent(ui::ET_TOUCH_RELEASED, gfx::Point(145, 250), time,
                     pointer_details1);
  DispatchTouchEvent(ui::ET_TOUCH_RELEASED, gfx::Point(355, 250), time,
                     pointer_details2);

  // Confirms that scale has increased with the gesture.
  ASSERT_LT(2.0f, GetMagnificationController()->GetScale());

  // Confirms that center is kept at center of pinch gesture. In ideal
  // situation, center of viewport should be kept at (250, 250). But as noted
  // above, scroll gesture caused by touch events for simulating pinch gesture
  // moves the viewport a little. We accept 5 pixels viewport move for the
  // scroll gesture.
  EXPECT_TRUE(
      gfx::Rect(245, 245, 10, 10)
          .Contains(
              GetMagnificationController()->GetViewportRect().CenterPoint()));
}

// Tests to see if keyboard overscroll is disabled when fullscreen magnification
// is enabled.
TEST_F(MagnificationControllerTest, KeyboardOverscrollDisabled) {
  GetMagnificationController()->SetEnabled(false);

  auto* keyboard_controller = keyboard::KeyboardUIController::Get();
  bool old_keyboard_overscroll_value =
      keyboard_controller->IsKeyboardOverscrollEnabled();

  // Enable magnification. Keyboard overscroll should be disabled.
  GetMagnificationController()->SetEnabled(true);
  EXPECT_FALSE(keyboard_controller->IsKeyboardOverscrollEnabled());

  // Disable magnification. Keyboard overscroll should be back to the way it was
  // before magnification was enabled.
  GetMagnificationController()->SetEnabled(false);
  EXPECT_EQ(keyboard_controller->IsKeyboardOverscrollEnabled(),
            old_keyboard_overscroll_value);
}

// Disabled due to https://crbug.com/917113.
TEST_F(MagnificationControllerTest, DISABLED_TextfieldFocusedWithKeyboard) {
  // Set up text input view.
  text_input_helper_.CreateAndShowTextInputView(gfx::Rect(500, 200, 80, 80));
  gfx::Rect text_input_bounds = text_input_helper_.GetTextInputViewBounds();

  // Enables magnifier and confirm the viewport is at center.
  GetMagnificationController()->SetEnabled(true);
  EXPECT_EQ(2.0f, GetMagnificationController()->GetScale());
  EXPECT_EQ("200,150 400x300", GetViewport().ToString());
  EXPECT_FALSE(GetMagnificationController()->KeepFocusCentered());

  GetMagnificationController()->SetKeepFocusCentered(true);

  // Set up and show the keyboard.
  keyboard::SetAccessibilityKeyboardEnabled(true);
  auto* keyboard_controller = keyboard::KeyboardUIController::Get();
  keyboard_controller->ShowKeyboard(true);

  // Focus on the text input field.
  text_input_helper_.FocusOnTextInputView();
  base::RunLoop().RunUntilIdle();

  // Verify that the caret bounds is centered in the area above the keyboard.
  gfx::Rect viewport_outside_keyboard_bounds = GetViewport();
  viewport_outside_keyboard_bounds.set_height(
      viewport_outside_keyboard_bounds.height() -
      keyboard_controller->GetVisualBoundsInScreen().height() /
          GetMagnificationController()->GetScale());

  gfx::Rect caret_bounds = text_input_helper_.GetCaretBounds();

  EXPECT_TRUE(GetMagnificationController()->KeepFocusCentered());
  EXPECT_TRUE(viewport_outside_keyboard_bounds.Contains(text_input_bounds));
  EXPECT_TRUE(text_input_bounds.Contains(caret_bounds.CenterPoint()));
  EXPECT_EQ(caret_bounds.CenterPoint(),
            viewport_outside_keyboard_bounds.CenterPoint());
}

}  // namespace ash
