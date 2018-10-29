// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/autoclick/autoclick_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/display/manager/display_manager.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_handler.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/test/event_generator.h"

namespace ash {

class MouseEventCapturer : public ui::EventHandler {
 public:
  MouseEventCapturer() { Reset(); }
  ~MouseEventCapturer() override = default;

  void Reset() { events_.clear(); }

  void OnMouseEvent(ui::MouseEvent* event) override {
    bool save_event = false;
    bool stop_event = false;
    // Filter out extraneous mouse events like mouse entered, exited,
    // capture changed, etc.
    ui::EventType type = event->type();
    if (type == ui::ET_MOUSE_PRESSED || type == ui::ET_MOUSE_RELEASED) {
      // Only track left and right mouse button events, ensuring that we get
      // left-click, right-click and double-click.
      if (!(event->flags() & ui::EF_LEFT_MOUSE_BUTTON) &&
          (!(event->flags() & ui::EF_RIGHT_MOUSE_BUTTON)))
        return;
      save_event = true;
      // Stop event propagation so we don't click on random stuff that
      // might break test assumptions.
      stop_event = true;
    } else if (type == ui::ET_MOUSE_DRAGGED) {
      save_event = true;
      stop_event = false;
    }
    if (save_event) {
      events_.push_back(ui::MouseEvent(event->type(), event->location(),
                                       event->root_location(),
                                       ui::EventTimeForNow(), event->flags(),
                                       event->changed_button_flags()));
    }
    if (stop_event)
      event->StopPropagation();

    // If there is a possibility that we're in an infinite loop, we should
    // exit early with a sensible error rather than letting the test time out.
    ASSERT_LT(events_.size(), 100u);
  }

  const std::vector<ui::MouseEvent>& captured_events() const { return events_; }

 private:
  std::vector<ui::MouseEvent> events_;

  DISALLOW_COPY_AND_ASSIGN(MouseEventCapturer);
};

class AutoclickTest : public AshTestBase {
 public:
  AutoclickTest() = default;
  ~AutoclickTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    Shell::Get()->AddPreTargetHandler(&mouse_event_capturer_);
    GetAutoclickController()->SetAutoclickDelay(base::TimeDelta());

    // Move mouse to deterministic location at the start of each test.
    GetEventGenerator()->MoveMouseTo(100, 100);

    // Make sure the display is initialized so we don't fail the test due to any
    // input events caused from creating the display.
    Shell::Get()->display_manager()->UpdateDisplays();
    RunAllPendingInMessageLoop();
  }

  void TearDown() override {
    Shell::Get()->RemovePreTargetHandler(&mouse_event_capturer_);
    AshTestBase::TearDown();
  }

  void MoveMouseWithFlagsTo(int x, int y, ui::EventFlags flags) {
    GetEventGenerator()->set_flags(flags);
    GetEventGenerator()->MoveMouseTo(x, y);
    GetEventGenerator()->set_flags(ui::EF_NONE);
  }

  const std::vector<ui::MouseEvent>& WaitForMouseEvents() {
    ClearMouseEvents();
    RunAllPendingInMessageLoop();
    return GetMouseEvents();
  }

  AutoclickController* GetAutoclickController() {
    return Shell::Get()->autoclick_controller();
  }

  void ClearMouseEvents() { mouse_event_capturer_.Reset(); }

  const std::vector<ui::MouseEvent>& GetMouseEvents() {
    return mouse_event_capturer_.captured_events();
  }

 private:
  MouseEventCapturer mouse_event_capturer_;

  DISALLOW_COPY_AND_ASSIGN(AutoclickTest);
};

TEST_F(AutoclickTest, ToggleEnabled) {
  std::vector<ui::MouseEvent> events;

  // We should not see any events initially.
  EXPECT_FALSE(GetAutoclickController()->IsEnabled());
  events = WaitForMouseEvents();
  EXPECT_EQ(0u, events.size());

  // Enable autoclick, and we should see a mouse pressed and
  // a mouse released event, simulating a click.
  GetAutoclickController()->SetEnabled(true);
  GetEventGenerator()->MoveMouseTo(0, 0);
  EXPECT_TRUE(GetAutoclickController()->IsEnabled());
  events = WaitForMouseEvents();
  ASSERT_EQ(2u, events.size());
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, events[0].type());
  EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & events[0].flags());
  EXPECT_EQ(ui::ET_MOUSE_RELEASED, events[1].type());
  EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & events[1].flags());

  // We should not get any more clicks until we move the mouse.
  events = WaitForMouseEvents();
  EXPECT_EQ(0u, events.size());
  GetEventGenerator()->MoveMouseTo(30, 30);
  events = WaitForMouseEvents();
  ASSERT_EQ(2u, events.size());
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, events[0].type());
  EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & events[0].flags());
  EXPECT_EQ(ui::ET_MOUSE_RELEASED, events[1].type());
  EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & events[1].flags());

  // Disable autoclick, and we should see the original behaviour.
  GetAutoclickController()->SetEnabled(false);
  EXPECT_FALSE(GetAutoclickController()->IsEnabled());
  events = WaitForMouseEvents();
  EXPECT_EQ(0u, events.size());
}

TEST_F(AutoclickTest, MouseMovement) {
  std::vector<ui::MouseEvent> events;
  GetAutoclickController()->SetEnabled(true);

  gfx::Point p1(0, 0);
  gfx::Point p2(20, 20);
  gfx::Point p3(40, 40);

  // Move mouse to p1.
  GetEventGenerator()->MoveMouseTo(p1);
  events = WaitForMouseEvents();
  EXPECT_EQ(2u, events.size());
  EXPECT_EQ(p1.ToString(), events[0].root_location().ToString());
  EXPECT_EQ(p1.ToString(), events[1].root_location().ToString());

  // Move mouse to multiple locations and finally arrive at p3.
  GetEventGenerator()->MoveMouseTo(p2);
  GetEventGenerator()->MoveMouseTo(p1);
  GetEventGenerator()->MoveMouseTo(p3);
  events = WaitForMouseEvents();
  EXPECT_EQ(2u, events.size());
  EXPECT_EQ(p3.ToString(), events[0].root_location().ToString());
  EXPECT_EQ(p3.ToString(), events[1].root_location().ToString());
}

TEST_F(AutoclickTest, MovementThreshold) {
  UpdateDisplay("1280x1024,800x600");
  RunAllPendingInMessageLoop();
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  EXPECT_EQ(2u, root_windows.size());

  // Run test for the secondary display too to test fix for crbug.com/449870.
  for (auto* root_window : root_windows) {
    gfx::Point center = root_window->GetBoundsInScreen().CenterPoint();

    GetAutoclickController()->SetEnabled(true);
    GetEventGenerator()->MoveMouseTo(center);
    EXPECT_EQ(2u, WaitForMouseEvents().size());

    // Small mouse movements should not trigger an autoclick.
    GetEventGenerator()->MoveMouseTo(center + gfx::Vector2d(1, 1));
    EXPECT_EQ(0u, WaitForMouseEvents().size());
    GetEventGenerator()->MoveMouseTo(center + gfx::Vector2d(2, 2));
    EXPECT_EQ(0u, WaitForMouseEvents().size());
    GetEventGenerator()->MoveMouseTo(center);
    EXPECT_EQ(0u, WaitForMouseEvents().size());

    // A large mouse movement should trigger an autoclick.
    GetEventGenerator()->MoveMouseTo(center + gfx::Vector2d(100, 100));
    EXPECT_EQ(2u, WaitForMouseEvents().size());
  }
}

TEST_F(AutoclickTest, SingleKeyModifier) {
  GetAutoclickController()->SetEnabled(true);
  MoveMouseWithFlagsTo(20, 20, ui::EF_SHIFT_DOWN);
  std::vector<ui::MouseEvent> events = WaitForMouseEvents();
  EXPECT_EQ(2u, events.size());
  EXPECT_EQ(ui::EF_SHIFT_DOWN, events[0].flags() & ui::EF_SHIFT_DOWN);
  EXPECT_EQ(ui::EF_SHIFT_DOWN, events[1].flags() & ui::EF_SHIFT_DOWN);
}

TEST_F(AutoclickTest, MultipleKeyModifiers) {
  GetAutoclickController()->SetEnabled(true);
  ui::EventFlags modifier_flags = static_cast<ui::EventFlags>(
      ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN);
  MoveMouseWithFlagsTo(30, 30, modifier_flags);
  std::vector<ui::MouseEvent> events = WaitForMouseEvents();
  EXPECT_EQ(2u, events.size());
  EXPECT_EQ(modifier_flags, events[0].flags() & modifier_flags);
  EXPECT_EQ(modifier_flags, events[1].flags() & modifier_flags);
}

TEST_F(AutoclickTest, KeyModifiersReleased) {
  GetAutoclickController()->SetEnabled(true);

  ui::EventFlags modifier_flags = static_cast<ui::EventFlags>(
      ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN);
  MoveMouseWithFlagsTo(12, 12, modifier_flags);

  // Simulate releasing key modifiers by sending key released events.
  GetEventGenerator()->ReleaseKey(
      ui::VKEY_CONTROL,
      static_cast<ui::EventFlags>(ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN));
  GetEventGenerator()->ReleaseKey(ui::VKEY_SHIFT, ui::EF_ALT_DOWN);

  std::vector<ui::MouseEvent> events;
  events = WaitForMouseEvents();
  EXPECT_EQ(2u, events.size());
  EXPECT_EQ(0, events[0].flags() & ui::EF_CONTROL_DOWN);
  EXPECT_EQ(0, events[0].flags() & ui::EF_SHIFT_DOWN);
  EXPECT_EQ(ui::EF_ALT_DOWN, events[0].flags() & ui::EF_ALT_DOWN);
}

TEST_F(AutoclickTest, UserInputCancelsAutoclick) {
  GetAutoclickController()->SetEnabled(true);
  std::vector<ui::MouseEvent> events;

  // Pressing a normal key should cancel the autoclick.
  GetEventGenerator()->MoveMouseTo(200, 200);
  GetEventGenerator()->PressKey(ui::VKEY_K, ui::EF_NONE);
  GetEventGenerator()->ReleaseKey(ui::VKEY_K, ui::EF_NONE);
  events = WaitForMouseEvents();
  EXPECT_EQ(0u, events.size());

  // Pressing a modifier key should NOT cancel the autoclick.
  GetEventGenerator()->MoveMouseTo(100, 100);
  GetEventGenerator()->PressKey(ui::VKEY_SHIFT, ui::EF_SHIFT_DOWN);
  GetEventGenerator()->ReleaseKey(ui::VKEY_SHIFT, ui::EF_NONE);
  events = WaitForMouseEvents();
  EXPECT_EQ(2u, events.size());

  // Performing a gesture should cancel the autoclick.
  GetEventGenerator()->MoveMouseTo(200, 200);
  GetEventGenerator()->GestureTapDownAndUp(gfx::Point(100, 100));
  events = WaitForMouseEvents();
  EXPECT_EQ(0u, events.size());

  // Test another gesture.
  GetEventGenerator()->MoveMouseTo(100, 100);
  GetEventGenerator()->GestureScrollSequence(
      gfx::Point(100, 100), gfx::Point(200, 200),
      base::TimeDelta::FromMilliseconds(200), 3);
  events = WaitForMouseEvents();
  EXPECT_EQ(0u, events.size());

  // Test scroll events.
  GetEventGenerator()->MoveMouseTo(200, 200);
  GetEventGenerator()->ScrollSequence(gfx::Point(100, 100),
                                      base::TimeDelta::FromMilliseconds(200), 0,
                                      100, 3, 2);
  events = WaitForMouseEvents();
  EXPECT_EQ(0u, events.size());
}

TEST_F(AutoclickTest, SynthesizedMouseMovesIgnored) {
  GetAutoclickController()->SetEnabled(true);
  std::vector<ui::MouseEvent> events;
  GetEventGenerator()->MoveMouseTo(100, 100);
  events = WaitForMouseEvents();
  EXPECT_EQ(2u, events.size());

  // Show a window and make sure the new window is under the cursor. As a
  // result, synthesized mouse events will be dispatched to the window, but it
  // should not trigger an autoclick.
  aura::test::EventCountDelegate delegate;
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
      &delegate, 123, gfx::Rect(50, 50, 100, 100)));
  window->Show();
  events = WaitForMouseEvents();
  EXPECT_EQ(0u, events.size());
  EXPECT_EQ("1 1 0", delegate.GetMouseMotionCountsAndReset());
}

TEST_F(AutoclickTest, AutoclickChangeEventTypes) {
  GetAutoclickController()->SetEnabled(true);
  GetAutoclickController()->set_revert_to_left_click(false);
  GetAutoclickController()->SetAutoclickEventType(
      mojom::AutoclickEventType::kRightClick);
  std::vector<ui::MouseEvent> events;

  GetEventGenerator()->MoveMouseTo(30, 30);
  events = WaitForMouseEvents();
  ASSERT_EQ(2u, events.size());
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, events[0].type());
  EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & events[0].flags());
  EXPECT_EQ(ui::ET_MOUSE_RELEASED, events[1].type());
  EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & events[1].flags());

  // Changing the event type cancels the event
  GetEventGenerator()->MoveMouseTo(60, 60);
  GetAutoclickController()->SetAutoclickEventType(
      mojom::AutoclickEventType::kLeftClick);
  events = WaitForMouseEvents();
  EXPECT_EQ(0u, events.size());

  // Changing the event type to the same thing does not cancel the event.
  // kLeftClick type does not produce a double-click.
  GetEventGenerator()->MoveMouseTo(90, 90);
  GetAutoclickController()->SetAutoclickEventType(
      mojom::AutoclickEventType::kLeftClick);
  events = WaitForMouseEvents();
  ASSERT_EQ(2u, events.size());
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, events[0].type());
  EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & events[0].flags());
  EXPECT_FALSE(ui::EF_IS_DOUBLE_CLICK & events[0].flags());
  EXPECT_EQ(ui::ET_MOUSE_RELEASED, events[1].type());
  EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & events[1].flags());
  EXPECT_FALSE(ui::EF_IS_DOUBLE_CLICK & events[1].flags());

  // Double-click works as expected.
  GetAutoclickController()->SetAutoclickEventType(
      mojom::AutoclickEventType::kDoubleClick);
  GetEventGenerator()->MoveMouseTo(120, 120);
  events = WaitForMouseEvents();
  ASSERT_EQ(4u, events.size());
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, events[0].type());
  EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & events[0].flags());
  EXPECT_FALSE(ui::EF_IS_DOUBLE_CLICK & events[0].flags());
  EXPECT_EQ(ui::ET_MOUSE_RELEASED, events[1].type());
  EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & events[1].flags());
  EXPECT_FALSE(ui::EF_IS_DOUBLE_CLICK & events[1].flags());
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, events[2].type());
  EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & events[2].flags());
  EXPECT_TRUE(ui::EF_IS_DOUBLE_CLICK & events[2].flags());
  EXPECT_EQ(ui::ET_MOUSE_RELEASED, events[3].type());
  EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & events[3].flags());
  EXPECT_TRUE(ui::EF_IS_DOUBLE_CLICK & events[3].flags());

  // Pause / no action does not cause events to be generated even when the
  // mouse moves.
  GetAutoclickController()->SetAutoclickEventType(
      mojom::AutoclickEventType::kNoAction);
  GetEventGenerator()->MoveMouseTo(120, 120);
  events = WaitForMouseEvents();
  EXPECT_EQ(0u, events.size());
}

TEST_F(AutoclickTest, AutoclickDragAndDropEvents) {
  GetAutoclickController()->SetEnabled(true);
  GetAutoclickController()->set_revert_to_left_click(false);
  GetAutoclickController()->SetAutoclickEventType(
      mojom::AutoclickEventType::kDragAndDrop);
  std::vector<ui::MouseEvent> events;

  GetEventGenerator()->MoveMouseTo(30, 30);
  events = WaitForMouseEvents();
  ASSERT_EQ(1u, events.size());
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, events[0].type());
  EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & events[0].flags());

  ClearMouseEvents();
  GetEventGenerator()->MoveMouseTo(60, 60);
  events = GetMouseEvents();
  ASSERT_EQ(1u, events.size());
  EXPECT_EQ(ui::ET_MOUSE_DRAGGED, events[0].type());

  // Another move creates a drag
  ClearMouseEvents();
  GetEventGenerator()->MoveMouseTo(90, 90);
  events = GetMouseEvents();
  ASSERT_EQ(1u, events.size());
  EXPECT_EQ(ui::ET_MOUSE_DRAGGED, events[0].type());

  // Waiting in place creates the released event.
  events = WaitForMouseEvents();
  ASSERT_EQ(1u, events.size());
  EXPECT_EQ(ui::ET_MOUSE_RELEASED, events[0].type());
}

TEST_F(AutoclickTest, AutoclickRevertsToLeftClick) {
  GetAutoclickController()->SetEnabled(true);
  GetAutoclickController()->set_revert_to_left_click(true);
  GetAutoclickController()->SetAutoclickEventType(
      mojom::AutoclickEventType::kRightClick);
  std::vector<ui::MouseEvent> events;

  GetEventGenerator()->MoveMouseTo(30, 30);
  events = WaitForMouseEvents();
  ASSERT_EQ(2u, events.size());
  EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & events[0].flags());
  EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & events[1].flags());

  // Another event is now left-click; we've reverted to left click.
  GetEventGenerator()->MoveMouseTo(90, 90);
  GetAutoclickController()->SetAutoclickEventType(
      mojom::AutoclickEventType::kLeftClick);
  events = WaitForMouseEvents();
  ASSERT_EQ(2u, events.size());
  EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & events[0].flags());
  EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & events[1].flags());

  // The next event is also a left click.
  GetEventGenerator()->MoveMouseTo(120, 120);
  GetAutoclickController()->SetAutoclickEventType(
      mojom::AutoclickEventType::kLeftClick);
  events = WaitForMouseEvents();
  ASSERT_EQ(2u, events.size());
  EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & events[0].flags());
  EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & events[1].flags());

  // Changing revert to false doesn't change that we are on left click at
  // present.
  GetAutoclickController()->set_revert_to_left_click(false);
  GetEventGenerator()->MoveMouseTo(150, 150);
  GetAutoclickController()->SetAutoclickEventType(
      mojom::AutoclickEventType::kLeftClick);
  events = WaitForMouseEvents();
  ASSERT_EQ(2u, events.size());
  EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & events[0].flags());
  EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & events[1].flags());

  // But we should no longer revert to left click if the type is something else.
  GetAutoclickController()->SetAutoclickEventType(
      mojom::AutoclickEventType::kRightClick);
  GetEventGenerator()->MoveMouseTo(180, 180);
  events = WaitForMouseEvents();
  ASSERT_EQ(2u, events.size());
  EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & events[0].flags());
  EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & events[1].flags());

  // Should still be right click.
  GetEventGenerator()->MoveMouseTo(210, 210);
  events = WaitForMouseEvents();
  ASSERT_EQ(2u, events.size());
  EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & events[0].flags());
  EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & events[1].flags());
}

}  // namespace ash
