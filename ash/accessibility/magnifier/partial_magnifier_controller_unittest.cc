// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/magnifier/partial_magnifier_controller.h"

#include "ash/accessibility/magnifier/fullscreen_magnifier_controller.h"
#include "ash/accessibility/magnifier/magnifier_glass.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/widget/widget.h"

namespace ash {

// Wrapper for PartialMagnifierController that exposes internal state to
// test functions.
class PartialMagnifierControllerTestApi {
 public:
  explicit PartialMagnifierControllerTestApi(
      PartialMagnifierController* controller)
      : controller_(controller) {}
  ~PartialMagnifierControllerTestApi() = default;

  bool is_enabled() const { return controller_->is_enabled_; }
  bool is_active() const { return controller_->is_active_; }
  views::Widget* host_widget() const {
    return controller_->magnifier_glass_->host_widget_;
  }

  gfx::Point GetWidgetOrigin() const {
    return host_widget()->GetWindowBoundsInScreen().origin();
  }

  aura::Window* GetWidgetRootWindow() const {
    return host_widget()->GetNativeView()->GetRootWindow();
  }

  gfx::Point GetWidgetRootOrigin() const {
    return GetWidgetRootWindow()->GetBoundsInScreen().origin();
  }

 private:
  raw_ptr<PartialMagnifierController> controller_;
};

class PartialMagnifierControllerTest : public AshTestBase {
 public:
  PartialMagnifierControllerTest() = default;
  ~PartialMagnifierControllerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    Shell::Get()->display_manager()->UpdateDisplays();
  }

 protected:
  PartialMagnifierController* GetController() const {
    return Shell::Get()->partial_magnifier_controller();
  }

  PartialMagnifierControllerTestApi GetTestApi() const {
    return PartialMagnifierControllerTestApi(
        Shell::Get()->partial_magnifier_controller());
  }
};

// The magnifier should not show up immediately after being enabled.
TEST_F(PartialMagnifierControllerTest, InactiveByDefault) {
  GetController()->SetEnabled(true);
  EXPECT_FALSE(GetTestApi().is_active());
  EXPECT_FALSE(GetTestApi().host_widget());
}

// The magnifier should show up only after a pointer is pressed while enabled.
TEST_F(PartialMagnifierControllerTest, ActiveOnPointerDown) {
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->EnterPenPointerMode();

  // While disabled no magnifier shows up.
  event_generator->PressTouch();
  EXPECT_FALSE(GetTestApi().is_active());
  EXPECT_FALSE(GetTestApi().host_widget());
  event_generator->ReleaseTouch();

  // While enabled the magnifier is only active while the pointer is down.
  GetController()->SetEnabled(true);
  event_generator->PressTouch();
  EXPECT_TRUE(GetTestApi().is_active());
  EXPECT_TRUE(GetTestApi().host_widget());
  event_generator->ReleaseTouch();
  EXPECT_FALSE(GetTestApi().is_active());
  EXPECT_FALSE(GetTestApi().host_widget());
}

// The magnifier should disappear after a pointer is cancelled while enabled.
TEST_F(PartialMagnifierControllerTest, InactiveOnPointerCancelled) {
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->EnterPenPointerMode();

  // While enabled the magnifier is disactivated when the pointer is cancelled.
  GetController()->SetEnabled(true);
  event_generator->PressTouch();
  EXPECT_TRUE(GetTestApi().is_active());
  EXPECT_TRUE(GetTestApi().host_widget());
  event_generator->CancelTouch();
  EXPECT_FALSE(GetTestApi().is_active());
  EXPECT_FALSE(GetTestApi().host_widget());
}

// Verifies that nothing bad happens if a second display is disconnected while
// the magnifier is active.
TEST_F(PartialMagnifierControllerTest, MultipleDisplays) {
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->EnterPenPointerMode();

  // Active magnifier with two displays, move it to the second display.
  UpdateDisplay("800x600,800x600");
  GetController()->SetEnabled(true);
  event_generator->PressTouch();
  event_generator->MoveTouch(gfx::Point(1200, 300));
  EXPECT_TRUE(GetTestApi().is_active());
  EXPECT_TRUE(GetTestApi().host_widget());

  // Disconnect the second display, verify that magnifier is still active.
  UpdateDisplay("800x600");
  GetController()->SwitchTargetRootWindowIfNeeded(
      Shell::GetPrimaryRootWindow());
  EXPECT_TRUE(GetTestApi().is_active());
  EXPECT_TRUE(GetTestApi().host_widget());
}

// Turning the magnifier off while it is active destroys the window.
TEST_F(PartialMagnifierControllerTest, DisablingDisablesActive) {
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->EnterPenPointerMode();

  GetController()->SetEnabled(true);
  event_generator->PressTouch();
  EXPECT_TRUE(GetTestApi().is_active());

  GetController()->SetEnabled(false);
  EXPECT_FALSE(GetTestApi().is_active());
  EXPECT_FALSE(GetTestApi().host_widget());
}

// The magnifier only activates for pointer events.
TEST_F(PartialMagnifierControllerTest, ActivatesOnlyForPointer) {
  GetController()->SetEnabled(true);
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->PressTouch();
  EXPECT_FALSE(GetTestApi().is_active());
}

// The magnifier is always located at pointer.
TEST_F(PartialMagnifierControllerTest, MagnifierFollowsPointer) {
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->EnterPenPointerMode();
  GetController()->SetEnabled(true);

  // The window does not have to be centered on the press; compute the initial
  // window placement offset. Use a Vector2d for the + operator overload.
  event_generator->PressTouch();
  gfx::Vector2d offset(GetTestApi().GetWidgetOrigin().x(),
                       GetTestApi().GetWidgetOrigin().y());

  // Move the pointer around, make sure the window follows it.
  event_generator->MoveTouch(gfx::Point(32, 32));
  EXPECT_EQ(event_generator->current_screen_location() + offset,
            GetTestApi().GetWidgetOrigin());

  event_generator->MoveTouch(gfx::Point(0, 10));
  EXPECT_EQ(event_generator->current_screen_location() + offset,
            GetTestApi().GetWidgetOrigin());

  event_generator->MoveTouch(gfx::Point(10, 0));
  EXPECT_EQ(event_generator->current_screen_location() + offset,
            GetTestApi().GetWidgetOrigin());

  event_generator->ReleaseTouch();

  // Make sure the window is initially placed correctly.
  event_generator->set_current_screen_location(gfx::Point(50, 20));
  EXPECT_FALSE(GetTestApi().is_active());
  event_generator->PressTouch();
  EXPECT_EQ(event_generator->current_screen_location() + offset,
            GetTestApi().GetWidgetOrigin());
}

// The magnifier appears on the root window associated with the
// correct display.
TEST_F(PartialMagnifierControllerTest, MagnifierAppearsCorrectDisplay) {
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->EnterPenPointerMode();
  UpdateDisplay("800x600,800x600");
  GetController()->SetEnabled(true);
  display::Screen* screen = display::Screen::GetScreen();

  event_generator->PressTouch(gfx::Point(400, 300));
  EXPECT_EQ(GetPrimaryDisplay(),
            screen->GetDisplayNearestPoint(GetTestApi().GetWidgetRootOrigin()));

  event_generator->ReleaseTouch();

  event_generator->PressTouch(gfx::Point(1200, 300));
  EXPECT_EQ(GetSecondaryDisplay(),
            screen->GetDisplayNearestPoint(GetTestApi().GetWidgetRootOrigin()));
}

// The magnifier appears under the pen, not the mouse.
TEST_F(PartialMagnifierControllerTest, MagnifierAppearsUnderPen) {
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  UpdateDisplay("800x600,800x600");
  GetController()->SetEnabled(true);
  display::Screen* screen = display::Screen::GetScreen();

  // Hold pen on primary; use mouse to move pointer to secondary
  event_generator->EnterPenPointerMode();
  event_generator->PressTouch(gfx::Point(400, 300));
  event_generator->ExitPenPointerMode();
  event_generator->MoveMouseTo(gfx::Point(1200, 300));
  EXPECT_EQ(GetPrimaryDisplay(),
            screen->GetDisplayNearestPoint(GetTestApi().GetWidgetRootOrigin()));

  // Pen must be lifted before it is touched to the next display.
  event_generator->EnterPenPointerMode();
  event_generator->ReleaseTouch();
  event_generator->ExitPenPointerMode();

  // Hold pen on secondary; use mouse to move pointer to primary
  event_generator->EnterPenPointerMode();
  event_generator->PressTouch(gfx::Point(1200, 300));
  event_generator->ExitPenPointerMode();
  event_generator->MoveMouseTo(gfx::Point(400, 300));
  EXPECT_EQ(GetSecondaryDisplay(),
            screen->GetDisplayNearestPoint(GetTestApi().GetWidgetRootOrigin()));
}

}  // namespace ash
