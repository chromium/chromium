// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/autoclick/autoclick_controller.h"
#include "ash/events/test_event_capturer.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/accessibility/accessibility_feature_disable_dialog.h"
#include "ash/system/accessibility/autoclick_menu_bubble_controller.h"
#include "ash/system/accessibility/autoclick_menu_view.h"
#include "ash/system/accessibility/autoclick_scroll_bubble_controller.h"
#include "ash/system/accessibility/autoclick_scroll_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/collision_detection/collision_detection_utils.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/display/manager/display_manager.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_handler.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/types/event_type.h"

namespace ash {

namespace {
const int kScrollToMenuBoundsBuffer = 18;
}  // namespace

class AutoclickTest : public AshTestBase {
 public:
  AutoclickTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  AutoclickTest(const AutoclickTest&) = delete;
  AutoclickTest& operator=(const AutoclickTest&) = delete;

  ~AutoclickTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    mouse_event_capturer_.set_capture_mouse_move(false);
    mouse_event_capturer_.set_capture_mouse_enter_exit(false);
    Shell::Get()->AddPreTargetHandler(&mouse_event_capturer_);
    GetAutoclickController()->SetAutoclickDelay(base::TimeDelta());

    // Move mouse to deterministic location at the start of each test.
    GetEventGenerator()->MoveMouseTo(100, 100);

    // Make sure the display is initialized so we don't fail the test due to any
    // input events caused from creating the display.
    Shell::Get()->display_manager()->UpdateDisplays();
    base::RunLoop().RunUntilIdle();
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
    // TODO(katie): Consider using quit closure instead of RunUntilIdle.
    base::RunLoop().RunUntilIdle();
    return GetMouseEvents();
  }

  void FastForwardBy(int milliseconds) {
    task_environment()->FastForwardBy(base::Milliseconds(milliseconds));
  }

  AutoclickController* GetAutoclickController() {
    return Shell::Get()->autoclick_controller();
  }

  // Calculates and returns full delay from the animation delay, after setting
  // that delay on the autoclick controller.
  int UpdateAnimationDelayAndGetFullDelay(float animation_delay) {
    float ratio =
        GetAutoclickController()->GetStartGestureDelayRatioForTesting();
    int full_delay = ceil(1.0 / ratio) * animation_delay;
    GetAutoclickController()->SetAutoclickDelay(base::Milliseconds(full_delay));
    return full_delay;
  }

  AutoclickMenuView* GetAutoclickMenuView() {
    return GetAutoclickController()
        ->GetMenuBubbleControllerForTesting()
        ->menu_view_;
  }

  AutoclickScrollView* GetAutoclickScrollView() {
    AutoclickScrollBubbleController* controller =
        GetAutoclickController()
            ->GetMenuBubbleControllerForTesting()
            ->scroll_bubble_controller_.get();
    return controller ? controller->scroll_view_.get() : nullptr;
  }

  views::Widget* GetAutoclickBubbleWidget() {
    return GetAutoclickController()
        ->GetMenuBubbleControllerForTesting()
        ->bubble_widget_;
  }

  views::View* GetMenuButton(AutoclickMenuView::ButtonId view_id) {
    AutoclickMenuView* menu_view = GetAutoclickMenuView();
    if (!menu_view)
      return nullptr;
    return menu_view->GetViewByID(static_cast<int>(view_id));
  }

  views::View* GetScrollButton(AutoclickScrollView::ButtonId view_id) {
    AutoclickScrollView* scroll_view = GetAutoclickScrollView();
    if (!scroll_view)
      return nullptr;
    return scroll_view->GetViewByID(static_cast<int>(view_id));
  }

  void ClearMouseEvents() { mouse_event_capturer_.ClearEvents(); }

  const std::vector<ui::MouseEvent>& GetMouseEvents() {
    return mouse_event_capturer_.mouse_events();
  }

  const std::vector<ui::MouseWheelEvent>& GetMouseWheelEvents() {
    return mouse_event_capturer_.captured_mouse_wheel_events();
  }

 private:
  TestEventCapturer mouse_event_capturer_;
};

TEST_F(AutoclickTest, ToggleEnabled) {
  std::vector<ui::MouseEvent> events;

  // We should not see any events initially.
  EXPECT_FALSE(GetAutoclickController()->IsEnabled());
  events = WaitForMouseEvents();
  EXPECT_EQ(0u, events.size());

  // Enable autoclick, and we should see a mouse pressed and
  // a mouse released event, simulating a click.
  GetAutoclickController()->SetEnabled(true, false /* do not show dialog */);
  GetEventGenerator()->MoveMouseTo(0, 0);
  EXPECT_TRUE(GetAutoclickController()->IsEnabled());
  events = WaitForMouseEvents();
  ASSERT_EQ(2u, events.size());
  EXPECT_EQ(ui::EventType::kMousePressed, events[0].type());
  EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & events[0].flags());
  EXPECT_EQ(ui::EventType::kMouseReleased, events[1].type());
  EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & events[1].flags());

  // We should not get any more clicks until we move the mouse.
  events = WaitForMouseEvents();
  EXPECT_EQ(0u, events.size());
  GetEventGenerator()->MoveMouseTo(30, 30);
  events = WaitForMouseEvents();
  ASSERT_EQ(2u, events.size());
  EXPECT_EQ(ui::EventType::kMousePressed, events[0].type());
  EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & events[0].flags());
  EXPECT_EQ(ui::EventType::kMouseReleased, events[1].type());
  EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & events[1].flags());

  // Disable autoclick, and we should see the original behaviour.
  GetAutoclickController()->SetEnabled(false, false /* do not show dialog */);
  EXPECT_FALSE(GetAutoclickController()->IsEnabled());
  events = WaitForMouseEvents();
  EXPECT_EQ(0u, events.size());

  // After disable, autoclick should be set back to left click.
  EXPECT_EQ(AutoclickEventType::kLeftClick,
            Shell::Get()->accessibility_controller()->GetAutoclickEventType());
}

TEST_F(AutoclickTest, MouseMovement) {
  std::vector<ui::MouseEvent> events;
  GetAutoclickController()->SetEnabled(true, false /* do not show dialog */);

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
  base::RunLoop().RunUntilIdle();
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  EXPECT_EQ(2u, root_windows.size());

  int animation_delay = 5;

  const struct {
    int movement_threshold;
    bool stabilize_click_position;
  } kTestCases[] = {
      {10, false}, {20, false}, {30, false}, {40, false}, {50, false},
      {10, true},  {20, true},  {30, true},  {40, true},  {50, true},
  };

  for (const auto& test : kTestCases) {
    GetAutoclickController()->set_stabilize_click_position(
        test.stabilize_click_position);
    int movement_threshold = test.movement_threshold;
    GetAutoclickController()->SetMovementThreshold(movement_threshold);

    // Run test for the secondary display too to test fix for crbug.com/449870.
    for (aura::Window* root_window : root_windows) {
      gfx::Point center = root_window->GetBoundsInScreen().CenterPoint();

      GetAutoclickController()->SetEnabled(true,
                                           false /* do not show dialog */);
      GetEventGenerator()->MoveMouseTo(center);
      ClearMouseEvents();
      EXPECT_EQ(2u, WaitForMouseEvents().size());

      // Small mouse movements should not trigger an autoclick, i.e. movements
      // within the radius of the movement_threshold.
      GetEventGenerator()->MoveMouseTo(
          center + gfx::Vector2d(std::sqrt(movement_threshold) - 1,
                                 std::sqrt(movement_threshold) - 1));
      EXPECT_EQ(0u, WaitForMouseEvents().size());
      GetEventGenerator()->MoveMouseTo(
          center + gfx::Vector2d(movement_threshold - 1, 0));
      EXPECT_EQ(0u, WaitForMouseEvents().size());
      GetEventGenerator()->MoveMouseTo(
          center + gfx::Vector2d(0, -movement_threshold + 1));
      EXPECT_EQ(0u, WaitForMouseEvents().size());
      GetEventGenerator()->MoveMouseTo(center);
      EXPECT_EQ(0u, WaitForMouseEvents().size());

      // A larger mouse movement should trigger an autoclick.
      GetEventGenerator()->MoveMouseTo(
          center +
          gfx::Vector2d(movement_threshold + 1, movement_threshold + 1));
      EXPECT_EQ(2u, WaitForMouseEvents().size());

      // Moving outside the threshold after the gesture begins should cancel
      // the autoclick. Update the delay so we can do events between the initial
      // trigger of the feature and the click.
      int full_delay = UpdateAnimationDelayAndGetFullDelay(animation_delay);
      GetEventGenerator()->MoveMouseTo(
          center - gfx::Vector2d(movement_threshold, movement_threshold));
      FastForwardBy(animation_delay + 1);
      GetEventGenerator()->MoveMouseTo(center);
      ClearMouseEvents();

      // After a time, a new click will occur at the second location. The first
      // location should never get a click.
      FastForwardBy(full_delay * 2);
      EXPECT_EQ(2u, GetMouseEvents().size());
      gfx::Rect display_bounds = display::Screen::GetScreen()
                                     ->GetDisplayNearestWindow(root_window)
                                     .bounds();
      EXPECT_EQ(center - gfx::Vector2d(display_bounds.origin().x(),
                                       display_bounds.origin().y()),
                GetMouseEvents()[0].location());

      // Move it out of the way so the next cycle starts properly.
      GetEventGenerator()->MoveMouseTo(gfx::Point(0, 0));
      GetAutoclickController()->SetAutoclickDelay(base::TimeDelta());
    }
  }

  // Reset to defaults.
  GetAutoclickController()->SetMovementThreshold(20);
  GetAutoclickController()->set_stabilize_click_position(false);
}

TEST_F(AutoclickTest, MovementWithinThresholdWhileTimerRunning) {
  GetAutoclickController()->SetEnabled(true, false /* do not show dialog */);
  GetAutoclickController()->SetMovementThreshold(20);
  int animation_delay = 5;
  int full_delay = UpdateAnimationDelayAndGetFullDelay(animation_delay);

  GetAutoclickController()->set_stabilize_click_position(true);
  GetEventGenerator()->MoveMouseTo(100, 100);
  FastForwardBy(animation_delay + 1);

  // Move the mouse within the threshold. It shouldn't change the eventual
  // target of the event, or cancel the click.
  GetEventGenerator()->MoveMouseTo(110, 110);
  ClearMouseEvents();
  FastForwardBy(full_delay);
  std::vector<ui::MouseEvent> events = GetMouseEvents();

  EXPECT_EQ(2u, events.size());
  EXPECT_EQ(gfx::Point(100, 100), events[0].location());
  EXPECT_EQ(ui::EventType::kMousePressed, events[0].type());
  EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, events[0].flags());
  EXPECT_EQ(gfx::Point(100, 100), events[1].location());
  EXPECT_EQ(ui::EventType::kMouseReleased, events[1].type());
  EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, events[1].flags());

  // When the click position is not stabilized, the mouse movement should
  // translate into the target of the event, but not cancel the click.
  GetAutoclickController()->set_stabilize_click_position(false);
  GetEventGenerator()->MoveMouseTo(200, 200);
  FastForwardBy(animation_delay + 1);
  GetEventGenerator()->MoveMouseTo(210, 210);

  ClearMouseEvents();
  FastForwardBy(full_delay);
  events = GetMouseEvents();

  EXPECT_EQ(2u, events.size());
  EXPECT_EQ(gfx::Point(210, 210), events[0].location());
  EXPECT_EQ(ui::EventType::kMousePressed, events[0].type());
  EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, events[0].flags());
  EXPECT_EQ(gfx::Point(210, 210), events[1].location());
  EXPECT_EQ(ui::EventType::kMouseReleased, events[1].type());
  EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, events[1].flags());

  // Reset delay.
  GetAutoclickController()->SetAutoclickDelay(base::TimeDelta());
}

TEST_F(AutoclickTest, SingleKeyModifier) {
  GetAutoclickController()->SetEnabled(true, false /* do not show dialog */);
  MoveMouseWithFlagsTo(20, 20, ui::EF_SHIFT_DOWN);
  std::vector<ui::MouseEvent> events = WaitForMouseEvents();
  EXPECT_EQ(2u, events.size());
  EXPECT_EQ(ui::EF_SHIFT_DOWN, events[0].flags() & ui::EF_SHIFT_DOWN);
  EXPECT_EQ(ui::EF_SHIFT_DOWN, events[1].flags() & ui::EF_SHIFT_DOWN);
}

TEST_F(AutoclickTest, MultipleKeyModifiers) {
  GetAutoclickController()->SetEnabled(true, false /* do not show dialog */);
  ui::EventFlags modifier_flags = static_cast<ui::EventFlags>(
      ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN);
  MoveMouseWithFlagsTo(30, 30, modifier_flags);
  std::vector<ui::MouseEvent> events = WaitForMouseEvents();
  EXPECT_EQ(2u, events.size());
  EXPECT_EQ(modifier_flags, events[0].flags() & modifier_flags);
  EXPECT_EQ(modifier_flags, events[1].flags() & modifier_flags);
}

TEST_F(AutoclickTest, KeyModifiersReleased) {
  GetAutoclickController()->SetEnabled(true, false /* do not show dialog */);

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
  GetAutoclickController()->SetEnabled(true, false /* do not show dialog */);
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

  // Mouse-wheel scroll events should cancel the autoclick.
  GetEventGenerator()->MoveMouseTo(300, 300);
  GetEventGenerator()->MoveMouseWheel(0, 20);
  events = WaitForMouseEvents();
  EXPECT_EQ(0u, events.size());

  // Performing a gesture should cancel the autoclick.
  GetEventGenerator()->MoveMouseTo(200, 200);
  GetEventGenerator()->GestureTapDownAndUp(gfx::Point(100, 100));
  events = WaitForMouseEvents();
  EXPECT_EQ(0u, events.size());

  // Test another gesture.
  GetEventGenerator()->MoveMouseTo(100, 100);
  GetEventGenerator()->GestureScrollSequence(
      gfx::Point(100, 100), gfx::Point(200, 200), base::Milliseconds(200), 3);
  events = WaitForMouseEvents();
  EXPECT_EQ(0u, events.size());

  // Test scroll events.
  GetEventGenerator()->MoveMouseTo(200, 200);
  GetEventGenerator()->ScrollSequence(gfx::Point(100, 100),
                                      base::Milliseconds(200), 0, 100, 3, 2);
  events = WaitForMouseEvents();
  EXPECT_EQ(0u, events.size());

  // However, just starting a scroll doesn't cancel. If you tap a touchpad on
  // an Eve chromebook, for example, it can send an
  // EventType::kScrollFlingCancel event, which shouldn't actually cancel
  // autoclick.
  GetEventGenerator()->MoveMouseTo(100, 100);
  GetEventGenerator()->ScrollSequence(gfx::Point(100, 100), base::TimeDelta(),
                                      0, 0, 0, 1);
  events = WaitForMouseEvents();
  EXPECT_EQ(2u, events.size());
}

TEST_F(AutoclickTest, SynthesizedMouseMovesIgnored) {
  GetAutoclickController()->SetEnabled(true, false /* do not show dialog */);
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
  GetAutoclickController()->SetEnabled(true, false /* do not show dialog */);
  GetAutoclickController()->set_revert_to_left_click(false);
  GetAutoclickController()->SetAutoclickEventType(
      AutoclickEventType::kRightClick);
  std::vector<ui::MouseEvent> events;

  GetEventGenerator()->MoveMouseTo(30, 30);
  events = WaitForMouseEvents();
  ASSERT_EQ(2u, events.size());
  EXPECT_EQ(ui::EventType::kMousePressed, events[0].type());
  EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & events[0].flags());
  EXPECT_EQ(ui::EventType::kMouseReleased, events[1].type());
  EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & events[1].flags());

  // Changing the event type cancels the event
  GetEventGenerator()->MoveMouseTo(60, 60);
  GetAutoclickController()->SetAutoclickEventType(
      AutoclickEventType::kLeftClick);
  events = WaitForMouseEvents();
  EXPECT_EQ(0u, events.size());

  // Changing the event type to the same thing does not cancel the event.
  // kLeftClick type does not produce a double-click.
  GetEventGenerator()->MoveMouseTo(90, 90);
  GetAutoclickController()->SetAutoclickEventType(
      AutoclickEventType::kLeftClick);
  events = WaitForMouseEvents();
  ASSERT_EQ(2u, events.size());
  EXPECT_EQ(ui::EventType::kMousePressed, events[0].type());
  EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & events[0].flags());
  EXPECT_FALSE(ui::EF_IS_DOUBLE_CLICK & events[0].flags());
  EXPECT_EQ(ui::EventType::kMouseReleased, events[1].type());
  EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & events[1].flags());
  EXPECT_FALSE(ui::EF_IS_DOUBLE_CLICK & events[1].flags());

  // Double-click works as expected.
  GetAutoclickController()->SetAutoclickEventType(
      AutoclickEventType::kDoubleClick);
  GetEventGenerator()->MoveMouseTo(120, 120);
  events = WaitForMouseEvents();
  ASSERT_EQ(4u, events.size());
  EXPECT_EQ(ui::EventType::kMousePressed, events[0].type());
  EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & events[0].flags());
  EXPECT_FALSE(ui::EF_IS_DOUBLE_CLICK & events[0].flags());
  EXPECT_EQ(ui::EventType::kMouseReleased, events[1].type());
  EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & events[1].flags());
  EXPECT_FALSE(ui::EF_IS_DOUBLE_CLICK & events[1].flags());
  EXPECT_EQ(ui::EventType::kMousePressed, events[2].type());
  EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & events[2].flags());
  EXPECT_TRUE(ui::EF_IS_DOUBLE_CLICK & events[2].flags());
  EXPECT_EQ(ui::EventType::kMouseReleased, events[3].type());
  EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & events[3].flags());
  EXPECT_TRUE(ui::EF_IS_DOUBLE_CLICK & events[3].flags());

  // Pause / no action does not cause events to be generated even when the
  // mouse moves.
  GetAutoclickController()->SetAutoclickEventType(
      AutoclickEventType::kNoAction);
  GetEventGenerator()->MoveMouseTo(120, 120);
  events = WaitForMouseEvents();
  EXPECT_EQ(0u, events.size());
}

TEST_F(AutoclickTest, AutoclickDragAndDropEvents) {
  GetAutoclickController()->SetEnabled(true, false /* do not show dialog */);
  GetAutoclickController()->set_revert_to_left_click(false);
  GetAutoclickController()->SetAutoclickEventType(
      AutoclickEventType::kDragAndDrop);
  std::vector<ui::MouseEvent> events;

  GetEventGenerator()->MoveMouseTo(30, 30);
  events = WaitForMouseEvents();
  ASSERT_EQ(1u, events.size());
  EXPECT_EQ(ui::EventType::kMousePressed, events[0].type());
  EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & events[0].flags());

  ClearMouseEvents();
  GetEventGenerator()->MoveMouseTo(60, 60);
  events = GetMouseEvents();
  ASSERT_EQ(1u, events.size());
  EXPECT_EQ(ui::EventType::kMouseDragged, events[0].type());

  // Another move creates a drag
  ClearMouseEvents();
  GetEventGenerator()->MoveMouseTo(90, 90);
  events = GetMouseEvents();
  ASSERT_EQ(1u, events.size());
  EXPECT_EQ(ui::EventType::kMouseDragged, events[0].type());

  // Waiting in place creates the released event.
  events = WaitForMouseEvents();
  ASSERT_EQ(1u, events.size());
  EXPECT_EQ(ui::EventType::kMouseReleased, events[0].type());
}

TEST_F(AutoclickTest, AutoclickScrollEvents) {
  UpdateDisplay("800x600");
  GetAutoclickController()->SetEnabled(true, false /* do not show dialog */);
  GetAutoclickController()->SetAutoclickEventType(AutoclickEventType::kScroll);
  std::vector<ui::MouseEvent> events;
  std::vector<ui::MouseWheelEvent> wheel_events;

  // Expect that the first scroll action occurs at the middle of the screen,
  // since the position has not been set by the user.
  GetAutoclickController()->DoScrollAction(
      AutoclickController::ScrollPadAction::kScrollUp);
  events = GetMouseEvents();
  wheel_events = GetMouseWheelEvents();
  EXPECT_EQ(0u, events.size());
  ASSERT_EQ(1u, wheel_events.size());
  EXPECT_EQ(ui::EventType::kMousewheel, wheel_events[0].type());
  EXPECT_EQ(gfx::Point(400, 300), wheel_events[0].location());
  EXPECT_GT(wheel_events[0].y_offset(), 0);
  ClearMouseEvents();

  // Expect that a dwell will set the scroll location.
  GetEventGenerator()->MoveMouseTo(90, 90);
  base::RunLoop().RunUntilIdle();
  GetAutoclickController()->DoScrollAction(
      AutoclickController::ScrollPadAction::kScrollUp);
  events = GetMouseEvents();
  wheel_events = GetMouseWheelEvents();
  EXPECT_EQ(0u, events.size());
  ASSERT_EQ(1u, wheel_events.size());
  EXPECT_EQ(ui::EventType::kMousewheel, wheel_events[0].type());
  EXPECT_EQ(gfx::Point(90, 90), wheel_events[0].location());
  EXPECT_GT(wheel_events[0].y_offset(), 0);
  ClearMouseEvents();

  GetAutoclickController()->DoScrollAction(
      AutoclickController::ScrollPadAction::kScrollLeft);
  events = GetMouseEvents();
  wheel_events = GetMouseWheelEvents();
  EXPECT_EQ(0u, events.size());
  ASSERT_EQ(1u, wheel_events.size());
  EXPECT_EQ(ui::EventType::kMousewheel, wheel_events[0].type());
  EXPECT_EQ(gfx::Point(90, 90), wheel_events[0].location());
  EXPECT_GT(wheel_events[0].x_offset(), 0);
  ClearMouseEvents();

  // Try another position, and the other two types of scroll action.
  GetEventGenerator()->MoveMouseTo(200, 200);
  base::RunLoop().RunUntilIdle();
  GetAutoclickController()->DoScrollAction(
      AutoclickController::ScrollPadAction::kScrollDown);
  events = GetMouseEvents();
  wheel_events = GetMouseWheelEvents();
  EXPECT_EQ(0u, events.size());
  ASSERT_EQ(1u, wheel_events.size());
  EXPECT_EQ(ui::EventType::kMousewheel, wheel_events[0].type());
  EXPECT_EQ(gfx::Point(200, 200), wheel_events[0].location());
  EXPECT_LT(wheel_events[0].y_offset(), 0);
  ClearMouseEvents();

  GetAutoclickController()->DoScrollAction(
      AutoclickController::ScrollPadAction::kScrollRight);
  events = GetMouseEvents();
  wheel_events = GetMouseWheelEvents();
  EXPECT_EQ(0u, events.size());
  ASSERT_EQ(1u, wheel_events.size());
  EXPECT_EQ(ui::EventType::kMousewheel, wheel_events[0].type());
  EXPECT_EQ(gfx::Point(200, 200), wheel_events[0].location());
  EXPECT_LT(wheel_events[0].x_offset(), 0);
  ClearMouseEvents();
}

TEST_F(AutoclickTest, AutoclickRevertsToLeftClick) {
  GetAutoclickController()->SetEnabled(true, false /* do not show dialog */);
  GetAutoclickController()->set_revert_to_left_click(true);
  GetAutoclickController()->SetAutoclickEventType(
      AutoclickEventType::kRightClick);
  std::vector<ui::MouseEvent> events;

  GetEventGenerator()->MoveMouseTo(30, 30);
  events = WaitForMouseEvents();
  ASSERT_EQ(2u, events.size());
  EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & events[0].flags());
  EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & events[1].flags());

  // Another event is now left-click; we've reverted to left click.
  GetEventGenerator()->MoveMouseTo(90, 90);
  GetAutoclickController()->SetAutoclickEventType(
      AutoclickEventType::kLeftClick);
  events = WaitForMouseEvents();
  ASSERT_EQ(2u, events.size());
  EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & events[0].flags());
  EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & events[1].flags());

  // The next event is also a left click.
  GetEventGenerator()->MoveMouseTo(120, 120);
  GetAutoclickController()->SetAutoclickEventType(
      AutoclickEventType::kLeftClick);
  events = WaitForMouseEvents();
  ASSERT_EQ(2u, events.size());
  EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & events[0].flags());
  EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & events[1].flags());

  // Changing revert to false doesn't change that we are on left click at
  // present.
  GetAutoclickController()->set_revert_to_left_click(false);
  GetEventGenerator()->MoveMouseTo(150, 150);
  GetAutoclickController()->SetAutoclickEventType(
      AutoclickEventType::kLeftClick);
  events = WaitForMouseEvents();
  ASSERT_EQ(2u, events.size());
  EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & events[0].flags());
  EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & events[1].flags());

  // But we should no longer revert to left click if the type is something else.
  GetAutoclickController()->SetAutoclickEventType(
      AutoclickEventType::kRightClick);
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

TEST_F(AutoclickTest, WaitsToDrawAnimationAfterDwellBegins) {
  int animation_delay = 5;
  int full_delay = UpdateAnimationDelayAndGetFullDelay(animation_delay);
  GetAutoclickController()->SetEnabled(true, false /* do not show dialog */);
  std::vector<ui::MouseEvent> events;

  // Start a dwell at (210, 210).
  GetEventGenerator()->MoveMouseTo(210, 210);

  // The center should change to (205, 205) if the adjustment is made before
  // the animation starts.
  FastForwardBy(animation_delay - 1);
  GetEventGenerator()->MoveMouseTo(205, 205);

  // Now wait the full delay to ensure the click has happened, then check
  // the result.
  FastForwardBy(full_delay);
  events = GetMouseEvents();
  ASSERT_EQ(2u, events.size());
  EXPECT_EQ(gfx::Point(205, 205), events[0].location());

  // Start another dwell at (100, 100).
  ClearMouseEvents();
  GetEventGenerator()->MoveMouseTo(100, 100);

  // Move the mouse a little to (105, 105), which should become the center.
  FastForwardBy(animation_delay - 1);
  GetEventGenerator()->MoveMouseTo(105, 105);

  // Moving the mouse during the animation changes the center point.
  FastForwardBy(animation_delay);
  GetEventGenerator()->MoveMouseTo(110, 110);

  // Now wait until the click. It should be at the center point from before
  // the animation started.
  FastForwardBy(full_delay);
  events = GetMouseEvents();
  ASSERT_EQ(2u, events.size());
  EXPECT_EQ(gfx::Point(110, 110), events[0].location());

  // Turn off stabilize_click_position and try again, the position should update
  // with the cursor's new position until the click occurs.
  GetAutoclickController()->set_stabilize_click_position(true);
  ClearMouseEvents();
  GetEventGenerator()->MoveMouseTo(200, 200);

  // (205, 205) will become the center of the animation.
  FastForwardBy(animation_delay - 1);
  GetEventGenerator()->MoveMouseTo(205, 205);

  // Fast forward until the animation would have started. Now moving the mouse
  // a little does not change the center point because we have stabilize on.
  FastForwardBy(animation_delay);
  GetEventGenerator()->MoveMouseTo(210, 210);
  FastForwardBy(full_delay);
  events = GetMouseEvents();
  ASSERT_EQ(2u, events.size());
  EXPECT_EQ(gfx::Point(205, 205), events[0].location());

  // Reset state.
  GetAutoclickController()->set_stabilize_click_position(false);
}

TEST_F(AutoclickTest, DoesActionOnBubbleWhenInDifferentModes) {
  AccessibilityController* accessibility_controller =
      Shell::Get()->accessibility_controller();
  // Enable autoclick from the accessibility controller so that the bubble is
  // constructed too.
  accessibility_controller->autoclick().SetEnabled(true);
  GetAutoclickController()->set_revert_to_left_click(false);
  std::vector<ui::MouseEvent> events;

  // Test at different screen sizes and densities because the fake click on
  // the button involves coordinating between dips and pixels. Try two different
  // positions to ensure offsets are calculated correctly.
  const struct {
    const std::string display_spec;
    float scale;
    FloatingMenuPosition position;
  } kTestCases[] = {
      {"800x600", 1.0f, FloatingMenuPosition::kBottomRight},
      {"1024x800*2.0", 2.0f, FloatingMenuPosition::kBottomRight},
      {"800x600", 1.0f, FloatingMenuPosition::kTopLeft},
      {"1024x800*2.0", 2.0f, FloatingMenuPosition::kTopLeft},
  };
  for (const auto& test : kTestCases) {
    UpdateDisplay(test.display_spec);
    accessibility_controller->SetAutoclickMenuPosition(test.position);
    accessibility_controller->SetAutoclickEventType(
        AutoclickEventType::kRightClick);

    AutoclickMenuView* menu = GetAutoclickMenuView();
    ASSERT_TRUE(menu);

    // Outside of the bubble, a right-click still occurs.
    // Move to a central position which will not have any menu but will still be
    // on-screen.
    GetEventGenerator()->MoveMouseTo(200 * test.scale, 200 * test.scale);
    events = WaitForMouseEvents();
    ASSERT_EQ(2u, events.size());
    EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & events[0].flags());
    EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & events[1].flags());

    // Over the bubble, we get no real click, although the autoclick event
    // type does get changed properly over a button.
    gfx::Point button_location = gfx::ScaleToRoundedPoint(
        GetMenuButton(AutoclickMenuView::ButtonId::kDoubleClick)
            ->GetBoundsInScreen()
            .CenterPoint(),
        test.scale);
    GetEventGenerator()->MoveMouseTo(button_location);
    events = WaitForMouseEvents();
    EXPECT_EQ(0u, events.size());
    // But the event type did change with a the hover on the button.
    EXPECT_EQ(AutoclickEventType::kDoubleClick,
              accessibility_controller->GetAutoclickEventType());

    // Change to a pause action type.
    accessibility_controller->SetAutoclickEventType(
        AutoclickEventType::kNoAction);

    // Outside the bubble, no action occurs.
    GetEventGenerator()->MoveMouseTo(200 * test.scale, 200 * test.scale);
    events = WaitForMouseEvents();
    EXPECT_EQ(0u, events.size());

    // If we move over the bubble but not over any button than no real click
    // occurs. There is no button at the top of the bubble.
    button_location = gfx::ScaleToRoundedPoint(
        GetAutoclickMenuView()->GetBoundsInScreen().top_center() +
            gfx::Vector2d(0, 1),
        test.scale);
    GetEventGenerator()->MoveMouseTo(button_location);
    events = WaitForMouseEvents();
    EXPECT_EQ(0u, events.size());
    // The event type did not change because we were not over any button.
    EXPECT_EQ(AutoclickEventType::kNoAction,
              accessibility_controller->GetAutoclickEventType());

    // Leaving the bubble we are still paused.
    GetEventGenerator()->MoveMouseTo(200 * test.scale, 200 * test.scale);
    events = WaitForMouseEvents();
    EXPECT_EQ(0u, events.size());

    // Moving over another button takes an action.
    button_location = gfx::ScaleToRoundedPoint(
        GetMenuButton(AutoclickMenuView::ButtonId::kLeftClick)
            ->GetBoundsInScreen()
            .CenterPoint(),
        test.scale);
    GetEventGenerator()->MoveMouseTo(button_location);
    events = WaitForMouseEvents();
    EXPECT_EQ(0u, events.size());
    EXPECT_EQ(AutoclickEventType::kLeftClick,
              accessibility_controller->GetAutoclickEventType());
  }
}

TEST_F(AutoclickTest,
       StartsGestureOnBubbleButDoesNotClickIfMouseMovedWhenPaused) {
  Shell::Get()->accessibility_controller()->autoclick().SetEnabled(true);
  GetAutoclickController()->set_revert_to_left_click(false);
  GetAutoclickController()->SetAutoclickEventType(
      AutoclickEventType::kNoAction);
  Shell::Get()->accessibility_controller()->SetAutoclickMenuPosition(
      FloatingMenuPosition::kBottomRight);

  int animation_delay = 5;
  int full_delay = UpdateAnimationDelayAndGetFullDelay(animation_delay);

  std::vector<ui::MouseEvent> events;
  AutoclickMenuView* menu = GetAutoclickMenuView();
  ASSERT_TRUE(menu);

  // Start a dwell over the bubble.
  GetEventGenerator()->MoveMouseTo(menu->GetBoundsInScreen().origin());

  // Move back off the bubble before anything happens.
  FastForwardBy(animation_delay - 1);
  GetEventGenerator()->MoveMouseTo(30, 30);

  // Now wait the full delay to ensure pause could have happened.
  FastForwardBy(full_delay);
  events = GetMouseEvents();
  ASSERT_EQ(0u, events.size());

  // This time, dwell over the bubble long enough for the animation to begin.
  // No action should occur if we move off during the dwell.
  GetEventGenerator()->MoveMouseTo(menu->GetBoundsInScreen().origin());

  // Move back off the bubble after the animation begins, but before a click
  // would occur.
  FastForwardBy(animation_delay + 1);
  GetEventGenerator()->MoveMouseTo(30, 30);

  // Now wait the full delay to ensure pause could have happened.
  FastForwardBy(full_delay);
  events = GetMouseEvents();
  ASSERT_EQ(0u, events.size());
}

// The autoclick tray shouldn't stop the shelf from auto-hiding.
TEST_F(AutoclickTest, ShelfAutohidesWithAutoclickBubble) {
  Shelf* shelf = GetPrimaryShelf();

  // Create a visible window so auto-hide behavior is enforced.
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                       nullptr, desks_util::GetActiveDeskContainerId(),
                       gfx::Rect(0, 0, 200, 200), true /* show */);

  // Turn on auto-hide for the shelf.
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Enable autoclick. The shelf should remain invisible.
  Shell::Get()->accessibility_controller()->autoclick().SetEnabled(true);
  AutoclickMenuView* menu = GetAutoclickMenuView();
  ASSERT_TRUE(menu);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
}

TEST_F(AutoclickTest, BubbleMovesWithShelfPositionChange) {
  UpdateDisplay("800x600");
  int screen_width = 800;
  int screen_height = 600;

  // Create a visible window so WMEvents occur.
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                       nullptr, desks_util::GetActiveDeskContainerId(),
                       gfx::Rect(0, 0, 200, 200), true /* show */);

  // Set up autoclick and the shelf.
  Shell::Get()->accessibility_controller()->autoclick().SetEnabled(true);
  Shell::Get()->accessibility_controller()->SetAutoclickMenuPosition(
      FloatingMenuPosition::kBottomRight);
  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kNever);
  EXPECT_EQ(shelf->GetVisibilityState(), SHELF_VISIBLE);
  AutoclickMenuView* menu = GetAutoclickMenuView();
  ASSERT_TRUE(menu);

  shelf->SetAlignment(ShelfAlignment::kBottom);
  // The menu should be positioned above the shelf, not overlapping.
  EXPECT_EQ(menu->GetBoundsInScreen().bottom_right().y(),
            screen_height - shelf->GetIdealBounds().height() -
                kCollisionWindowWorkAreaInsetsDp);
  // And all the way to the right.
  EXPECT_EQ(menu->GetBoundsInScreen().bottom_right().x(),
            screen_width - kCollisionWindowWorkAreaInsetsDp);

  shelf->SetAlignment(ShelfAlignment::kLeft);
  // The menu should move to the bottom of the screen.
  EXPECT_EQ(menu->GetBoundsInScreen().bottom_right().y(),
            screen_height - kCollisionWindowWorkAreaInsetsDp);
  // Still be at the far right.
  EXPECT_EQ(menu->GetBoundsInScreen().bottom_right().x(),
            screen_width - kCollisionWindowWorkAreaInsetsDp);

  shelf->SetAlignment(ShelfAlignment::kRight);
  // The menu should stay at the bottom of the screen.
  EXPECT_EQ(menu->GetBoundsInScreen().bottom_right().y(),
            screen_height - kCollisionWindowWorkAreaInsetsDp);
  // And should be offset from the far right by the shelf width.
  EXPECT_EQ(menu->GetBoundsInScreen().bottom_right().x(),
            screen_width - kCollisionWindowWorkAreaInsetsDp -
                shelf->GetIdealBounds().width());

  // Reset state.
  shelf->SetAlignment(ShelfAlignment::kBottom);
}

TEST_F(AutoclickTest, AvoidsShelfBubble) {
  const struct {
    session_manager::SessionState session_state;
  } kTestCases[]{
      {session_manager::SessionState::OOBE},
      {session_manager::SessionState::LOGIN_PRIMARY},
      {session_manager::SessionState::ACTIVE},
      {session_manager::SessionState::LOCKED},
  };

  for (auto test : kTestCases) {
    GetSessionControllerClient()->SetSessionState(test.session_state);
    // Set up autoclick and the shelf.
    Shell::Get()->accessibility_controller()->autoclick().SetEnabled(true);
    Shell::Get()->accessibility_controller()->SetAutoclickMenuPosition(
        FloatingMenuPosition::kBottomRight);
    auto* unified_system_tray = GetPrimaryUnifiedSystemTray();
    EXPECT_FALSE(unified_system_tray->IsBubbleShown());
    AutoclickMenuView* menu = GetAutoclickMenuView();
    ASSERT_TRUE(menu);
    gfx::Rect menu_bounds = menu->GetBoundsInScreen();

    unified_system_tray->ShowBubble();
    gfx::Rect new_menu_bounds = menu->GetBoundsInScreen();

    const int dx = abs(menu_bounds.x() - new_menu_bounds.x());
    const int dy = abs(menu_bounds.y() - new_menu_bounds.y());
    const gfx::Rect bubble_bounds =
        unified_system_tray->GetBubbleBoundsInScreen();

    // The height of the system tray bubble is dependent on the number of
    // feature pods, and whether the a11y widget goes above or to the side of
    // the system tray is dependent on the height of the system tray. Test that
    // one and only one of these situations is applicable.
    EXPECT_TRUE((dx < 5 && dy > bubble_bounds.height()) ||
                (dy < 5 && dx > bubble_bounds.width()));

    unified_system_tray->CloseBubble();
    new_menu_bounds = menu->GetBoundsInScreen();
    EXPECT_EQ(menu_bounds, new_menu_bounds);
  }
}

TEST_F(AutoclickTest, ConfirmationDialogShownWhenDisablingFeature) {
  // Enable and disable with the AccessibilityController to get real use-case
  // of the dialog.

  // No dialog shown at start-up.
  EXPECT_FALSE(GetAutoclickController()->GetDisableDialogForTesting());

  // No dialog shown when enabling the feature.
  Shell::Get()->accessibility_controller()->autoclick().SetEnabled(true);
  EXPECT_FALSE(GetAutoclickController()->GetDisableDialogForTesting());

  // A dialog should be shown when disabling the feature.
  Shell::Get()->accessibility_controller()->autoclick().SetEnabled(false);
  AccessibilityFeatureDisableDialog* dialog =
      GetAutoclickController()->GetDisableDialogForTesting();
  ASSERT_TRUE(dialog);

  // Canceling the dialog will cause the feature to continue to be enabled.
  dialog->CancelDialog();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(GetAutoclickController()->GetDisableDialogForTesting());
  EXPECT_TRUE(Shell::Get()->accessibility_controller()->autoclick().enabled());
  EXPECT_TRUE(GetAutoclickController()->IsEnabled());

  // Try to disable it again, and this time accept the dialog to actually
  // disable the feature.
  Shell::Get()->accessibility_controller()->autoclick().SetEnabled(false);
  dialog = GetAutoclickController()->GetDisableDialogForTesting();
  ASSERT_TRUE(dialog);
  dialog->AcceptDialog();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(GetAutoclickController()->GetDisableDialogForTesting());
  EXPECT_FALSE(Shell::Get()->accessibility_controller()->autoclick().enabled());
  EXPECT_FALSE(GetAutoclickController()->IsEnabled());
}

TEST_F(AutoclickTest, HidesBubbleInFullscreenWhenCursorHides) {
  Shell::Get()->accessibility_controller()->autoclick().SetEnabled(true);
  ::wm::CursorManager* cursor_manager = Shell::Get()->cursor_manager();
  cursor_manager->SetCursor(ui::mojom::CursorType::kPointer);

  const struct {
    const std::string display_spec;
    gfx::Rect widget_position;
  } kTestCases[] = {
      {"800x600", gfx::Rect(0, 0, 200, 200)},
      {"800x600,800x600", gfx::Rect(0, 0, 200, 200)},
      {"800x600,800x600", gfx::Rect(1000, 0, 200, 200)},
  };
  for (const auto& test : kTestCases) {
    SCOPED_TRACE(test.display_spec);
    UpdateDisplay(test.display_spec);

    std::unique_ptr<views::Widget> widget = CreateTestWidget(
        views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET, nullptr,
        desks_util::GetActiveDeskContainerId(), test.widget_position,
        /*show=*/true);
    EXPECT_TRUE(GetAutoclickBubbleWidget()->IsVisible());

    // Move the mouse over the widget, so it's on the same screen as the widget.
    GetEventGenerator()->MoveMouseTo(test.widget_position.origin());

    // When the widget is not fullscreen, hiding the cursor does not cause
    // the bubble to be hidden.
    cursor_manager->HideCursor();
    EXPECT_TRUE(GetAutoclickBubbleWidget()->IsVisible());
    cursor_manager->ShowCursor();
    EXPECT_TRUE(GetAutoclickBubbleWidget()->IsVisible());

    // Bubble is visible in fullscreen mode because the mouse cursor is visible.
    widget->SetFullscreen(true);
    EXPECT_TRUE(GetAutoclickBubbleWidget()->IsVisible());

    // Bubble is hidden when the cursor is hidden in fullscreen mode, and shown
    // when the cursor is shown.
    cursor_manager->HideCursor();
    EXPECT_FALSE(GetAutoclickBubbleWidget()->IsVisible());
    cursor_manager->ShowCursor();
    EXPECT_TRUE(GetAutoclickBubbleWidget()->IsVisible());

    // Changing the type to another visible type doesn't cause the bubble to
    // hide.
    cursor_manager->SetCursor(ui::mojom::CursorType::kHand);
    EXPECT_TRUE(GetAutoclickBubbleWidget()->IsVisible());

    // Changing the type to an kNone causes the bubble to hide.
    cursor_manager->SetCursor(ui::mojom::CursorType::kNone);
    EXPECT_FALSE(GetAutoclickBubbleWidget()->IsVisible());

    // Hiding and showing don't re-show the bubble because the type is still
    // kNone.
    cursor_manager->HideCursor();
    EXPECT_FALSE(GetAutoclickBubbleWidget()->IsVisible());
    cursor_manager->ShowCursor();
    EXPECT_FALSE(GetAutoclickBubbleWidget()->IsVisible());

    // The bubble is shown when the cursor is a visible type again.
    cursor_manager->SetCursor(ui::mojom::CursorType::kPointer);
    EXPECT_TRUE(GetAutoclickBubbleWidget()->IsVisible());
  }
}

TEST_F(AutoclickTest, DoesNotHideBubbleWhenNotOverFullscreenWindow) {
  UpdateDisplay("800x600,800x600");
  Shell::Get()->accessibility_controller()->autoclick().SetEnabled(true);
  ::wm::CursorManager* cursor_manager = Shell::Get()->cursor_manager();
  cursor_manager->SetCursor(ui::mojom::CursorType::kPointer);

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                       nullptr, desks_util::GetActiveDeskContainerId(),
                       gfx::Rect(1000, 0, 200, 200), true);

  EXPECT_TRUE(GetAutoclickBubbleWidget()->IsVisible());

  // Move the mouse over the other display.
  GetEventGenerator()->MoveMouseTo(gfx::Point(10, 10));

  // When the widget is fullscreen, hiding the cursor does not hide the bubble
  // because the cursor is on a different display.
  widget->SetFullscreen(true);
  cursor_manager->HideCursor();
  EXPECT_TRUE(GetAutoclickBubbleWidget()->IsVisible());
}

TEST_F(AutoclickTest, DoesNotHideBubbleWhenOverInactiveFullscreenWindow) {
  Shell::Get()->accessibility_controller()->autoclick().SetEnabled(true);
  ::wm::CursorManager* cursor_manager = Shell::Get()->cursor_manager();
  cursor_manager->SetCursor(ui::mojom::CursorType::kPointer);

  std::unique_ptr<views::Widget> widget = CreateTestWidget(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET, nullptr,
      desks_util::GetActiveDeskContainerId(), gfx::Rect(0, 0, 200, 200), true);
  GetEventGenerator()->MoveMouseTo(gfx::Point(10, 10));
  widget->SetFullscreen(true);
  EXPECT_TRUE(widget->IsActive());
  views::Widget* popup_widget = views::Widget::CreateWindowWithContext(
      nullptr, GetContext(), gfx::Rect(200, 200, 200, 200));
  popup_widget->Show();

  cursor_manager->HideCursor();
  EXPECT_FALSE(widget->IsActive());
  EXPECT_TRUE(popup_widget->IsActive());
  EXPECT_TRUE(GetAutoclickBubbleWidget()->IsVisible());
}

TEST_F(AutoclickTest, ScrollClosesWhenHoveredOverScrollButton) {
  GetAutoclickController()->SetEnabled(true, false /* do not show dialog */);
  GetAutoclickController()->SetAutoclickEventType(
      AutoclickEventType::kLeftClick);
  EXPECT_FALSE(GetAutoclickScrollView());

  // Enable scroll.
  GetAutoclickController()->SetAutoclickEventType(AutoclickEventType::kScroll);
  ASSERT_TRUE(GetAutoclickScrollView());
  views::View* close_button =
      GetScrollButton(AutoclickScrollView::ButtonId::kCloseScroll);
  ASSERT_TRUE(close_button);

  gfx::Point button_location = close_button->GetBoundsInScreen().CenterPoint();
  GetEventGenerator()->MoveMouseTo(button_location);
  // No mouse event.
  std::vector<ui::MouseEvent> events = WaitForMouseEvents();
  EXPECT_EQ(0u, events.size());

  // Reset to left click and scroll view is gone.
  EXPECT_EQ(AutoclickEventType::kLeftClick,
            Shell::Get()->accessibility_controller()->GetAutoclickEventType());
  EXPECT_FALSE(GetAutoclickScrollView());
}

TEST_F(AutoclickTest, ScrollOccursWhenHoveredOverScrollButtons) {
  UpdateDisplay("800x600");
  GetAutoclickController()->SetEnabled(true, false /* do not show dialog */);

  // Enable scroll.
  GetAutoclickController()->SetAutoclickEventType(AutoclickEventType::kScroll);
  ASSERT_TRUE(GetAutoclickScrollView());

  // TODO: Do for all four buttons, wait a few scrolls each.
  const struct {
    AutoclickScrollView::ButtonId button_id;
    int scroll_x;
    int scroll_y;
  } kTestCases[] = {
      {AutoclickScrollView::ButtonId::kScrollUp, 0, 10},
      {AutoclickScrollView::ButtonId::kScrollDown, 0, -10},
      {AutoclickScrollView::ButtonId::kScrollLeft, 10, 0},
      {AutoclickScrollView::ButtonId::kScrollRight, -10, 0},
  };
  for (auto& test : kTestCases) {
    views::View* button = GetScrollButton(test.button_id);
    ASSERT_TRUE(button);

    gfx::Point button_location = button->GetBoundsInScreen().CenterPoint();
    GetEventGenerator()->MoveMouseTo(button_location);
    // No mouse event during hover, no wheel events yet.
    FastForwardBy(AutoclickScrollView::kAutoclickScrollDelayMs - 1);
    std::vector<ui::MouseEvent> events = GetMouseEvents();
    EXPECT_EQ(0u, events.size());
    std::vector<ui::MouseWheelEvent> wheel_events = GetMouseWheelEvents();
    EXPECT_EQ(0u, wheel_events.size());

    // But we should get a scroll event after kAutoclickScrollDelayMs.
    FastForwardBy(2);
    wheel_events = GetMouseWheelEvents();
    EXPECT_EQ(1u, wheel_events.size());
    EXPECT_EQ(gfx::Point(400, 300), wheel_events[0].location());
    EXPECT_EQ(test.scroll_x, wheel_events[0].x_offset());
    EXPECT_EQ(test.scroll_y, wheel_events[0].y_offset());
    // No other events expected.
    events = GetMouseEvents();
    EXPECT_EQ(0u, events.size());
    ClearMouseEvents();

    // Wait until another kAutoclickScrollDelayMs has elapsed and expect another
    // scroll to have occurred.
    FastForwardBy(AutoclickScrollView::kAutoclickScrollDelayMs);
    wheel_events = GetMouseWheelEvents();
    EXPECT_EQ(1u, wheel_events.size());
    EXPECT_EQ(gfx::Point(400, 300), wheel_events[0].location());
    EXPECT_EQ(test.scroll_x, wheel_events[0].x_offset());
    EXPECT_EQ(test.scroll_y, wheel_events[0].y_offset());
    // No other events expected.
    events = GetMouseEvents();
    EXPECT_EQ(0u, events.size());
    ClearMouseEvents();
  }
}

TEST_F(AutoclickTest, ScrollMenuBubblePostioning) {
  UpdateDisplay("800x600");
  GetAutoclickController()->SetEnabled(true, false /* do not show dialog */);

  Shell::Get()->accessibility_controller()->SetAutoclickMenuPosition(
      FloatingMenuPosition::kBottomRight);
  GetAutoclickController()->SetAutoclickEventType(AutoclickEventType::kScroll);

  ASSERT_TRUE(GetAutoclickScrollView());

  // Set the bounds to be the entire window.
  gfx::Rect display_bounds = gfx::Rect(0, 0, 800, 600);
  GetAutoclickController()->HandleAutoclickScrollableBoundsFound(
      display_bounds);

  // The scroll bubble should start near the autoclick menu.
  gfx::Rect scroll_bounds = GetAutoclickScrollView()->GetBoundsInScreen();
  gfx::Rect menu_bounds = GetAutoclickMenuView()->GetBoundsInScreen();
  EXPECT_LT(menu_bounds.ManhattanInternalDistance(scroll_bounds),
            kScrollToMenuBoundsBuffer);

  // Moving the autoclick menu around the screen moves the scroll bubble too.
  Shell::Get()->accessibility_controller()->SetAutoclickMenuPosition(
      FloatingMenuPosition::kBottomLeft);
  scroll_bounds = GetAutoclickScrollView()->GetBoundsInScreen();
  menu_bounds = GetAutoclickMenuView()->GetBoundsInScreen();
  EXPECT_LT(menu_bounds.ManhattanInternalDistance(scroll_bounds),
            kScrollToMenuBoundsBuffer);

  Shell::Get()->accessibility_controller()->SetAutoclickMenuPosition(
      FloatingMenuPosition::kTopLeft);
  scroll_bounds = GetAutoclickScrollView()->GetBoundsInScreen();
  menu_bounds = GetAutoclickMenuView()->GetBoundsInScreen();
  EXPECT_LT(menu_bounds.ManhattanInternalDistance(scroll_bounds),
            kScrollToMenuBoundsBuffer);

  Shell::Get()->accessibility_controller()->SetAutoclickMenuPosition(
      FloatingMenuPosition::kTopRight);
  scroll_bounds = GetAutoclickScrollView()->GetBoundsInScreen();
  menu_bounds = GetAutoclickMenuView()->GetBoundsInScreen();
  EXPECT_LT(menu_bounds.ManhattanInternalDistance(scroll_bounds),
            kScrollToMenuBoundsBuffer);

  // However, if we dwell somewhere else, the autoclick scroll menu will now
  // move out of the corner and near that point when the display bounds are
  // found.
  gfx::Point scroll_point = gfx::Point(0, 0);
  GetEventGenerator()->MoveMouseTo(scroll_point);
  base::RunLoop().RunUntilIdle();
  GetAutoclickController()->HandleAutoclickScrollableBoundsFound(
      display_bounds);
  scroll_bounds = GetAutoclickScrollView()->GetBoundsInScreen();
  EXPECT_GT(menu_bounds.ManhattanInternalDistance(scroll_bounds),
            kScrollToMenuBoundsBuffer);

  // Moving the bubble menu now does not change the scroll bubble's position,
  // it remains near its point.
  Shell::Get()->accessibility_controller()->SetAutoclickMenuPosition(
      FloatingMenuPosition::kBottomRight);
  EXPECT_EQ(GetAutoclickScrollView()->GetBoundsInScreen(), scroll_bounds);
}

}  // namespace ash
