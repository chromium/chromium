// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/accessibility/chromevox/touch_exploration_controller.h"

#include <math.h>
#include <stddef.h>

#include <memory>
#include <vector>

#include "ash/accessibility/chromevox/mock_touch_exploration_controller_delegate.h"
#include "base/memory/ptr_util.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_cursor_client.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/gestures/gesture_provider_aura.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/test/events_test_utils.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/test/gl_surface_test_support.h"

using EventList = std::vector<std::unique_ptr<ui::Event>>;

namespace ash {

namespace {

// Records all mouse, touch, gesture, and key events.
class EventCapturer : public ui::EventHandler {
 public:
  EventCapturer() {}

  EventCapturer(const EventCapturer&) = delete;
  EventCapturer& operator=(const EventCapturer&) = delete;

  ~EventCapturer() override {}

  void Reset() { events_.clear(); }

  void OnEvent(ui::Event* event) override {
    if (event->IsMouseEvent() || event->IsTouchEvent() ||
        event->IsGestureEvent() || event->IsKeyEvent()) {
      events_.push_back(event->Clone());
    } else {
      return;
    }
    // Stop event propagation so we don't click on random stuff that
    // might break test assumptions.
    event->StopPropagation();
    // If there is a possibility that we're in an infinite loop, we should
    // exit early with a sensible error rather than letting the test time out.
    ASSERT_LT(events_.size(), 100u);
  }

  const EventList& captured_events() const { return events_; }

 private:
  EventList events_;
};

int Factorial(int n) {
  if (n <= 0)
    return 0;
  if (n == 1)
    return 1;
  return n * Factorial(n - 1);
}

}  // namespace

class TouchExplorationControllerTestApi {
 public:
  TouchExplorationControllerTestApi(
      TouchExplorationController* touch_exploration_controller) {
    touch_exploration_controller_.reset(touch_exploration_controller);
  }

  TouchExplorationControllerTestApi(const TouchExplorationControllerTestApi&) =
      delete;
  TouchExplorationControllerTestApi& operator=(
      const TouchExplorationControllerTestApi&) = delete;

  void CallTapTimerNowForTesting() {
    DCHECK(touch_exploration_controller_->tap_timer_.IsRunning());
    touch_exploration_controller_->tap_timer_.Stop();
    touch_exploration_controller_->OnTapTimerFired();
  }

  void CallTapTimerNowIfRunningForTesting() {
    if (touch_exploration_controller_->tap_timer_.IsRunning()) {
      touch_exploration_controller_->tap_timer_.Stop();
      touch_exploration_controller_->OnTapTimerFired();
    }
  }

  void CallLongPressTimerNowIfRunningForTesting() {
    if (touch_exploration_controller_->long_press_timer_.IsRunning()) {
      touch_exploration_controller_->long_press_timer_.Stop();
      touch_exploration_controller_->OnLiftActivationLongPressTimerFired();
    }
  }

  bool IsInNoFingersDownStateForTesting() const {
    return touch_exploration_controller_->state_ ==
           touch_exploration_controller_->NO_FINGERS_DOWN;
  }

  bool IsInGestureInProgressStateForTesting() const {
    return touch_exploration_controller_->state_ ==
           touch_exploration_controller_->GESTURE_IN_PROGRESS;
  }

  bool IsInSlideGestureStateForTesting() const {
    return touch_exploration_controller_->state_ ==
           touch_exploration_controller_->SLIDE_GESTURE;
  }

  bool IsInTwoFingerTapStateForTesting() const {
    return touch_exploration_controller_->state_ ==
           touch_exploration_controller_->TWO_FINGER_TAP;
  }

  gfx::Rect BoundsOfRootWindowInDIPForTesting() const {
    return touch_exploration_controller_->root_window_->GetBoundsInScreen();
  }

  // VLOGs should be suppressed in tests that generate a lot of logs,
  // for example permutations of nine touch events.
  void SuppressVLOGsForTesting(bool suppress) {
    touch_exploration_controller_->VLOG_on_ = !suppress;
  }

  float GetMaxDistanceFromEdge() const {
    return touch_exploration_controller_->kMaxDistanceFromEdge;
  }

  float GetSlopDistanceFromEdge() const {
    return touch_exploration_controller_->kSlopDistanceFromEdge;
  }

  void SetTouchAccessibilityAnchorPoint(const gfx::Point& location) {
    touch_exploration_controller_->SetTouchAccessibilityAnchorPoint(location);
  }

  void SetExcludeBounds(const gfx::Rect& bounds) {
    touch_exploration_controller_->SetExcludeBounds(bounds);
  }

  void SetLiftActivationBounds(const gfx::Rect& bounds) {
    touch_exploration_controller_->SetLiftActivationBounds(bounds);
  }

 private:
  std::unique_ptr<TouchExplorationController> touch_exploration_controller_;
};

class TouchExplorationTest : public aura::test::AuraTestBase {
 public:
  TouchExplorationTest() {}

  TouchExplorationTest(const TouchExplorationTest&) = delete;
  TouchExplorationTest& operator=(const TouchExplorationTest&) = delete;

  ~TouchExplorationTest() override {}

  void SetUp() override {
    if (gl::GetGLImplementation() == gl::kGLImplementationNone)
      gl::GLSurfaceTestSupport::InitializeOneOff();
    aura::test::AuraTestBase::SetUp();
    cursor_client_ =
        std::make_unique<aura::test::TestCursorClient>(root_window());
    root_window()->AddPreTargetHandler(&event_capturer_);
    generator_ = std::make_unique<ui::test::EventGenerator>(root_window());

    // Tests fail if time is ever 0.
    simulated_clock_.Advance(base::Milliseconds(10));
    // ui takes ownership of the tick clock.
    ui::SetEventTickClockForTesting(&simulated_clock_);

    cursor_client()->ShowCursor();
    cursor_client()->DisableMouseEvents();
  }

  void TearDown() override {
    ui::SetEventTickClockForTesting(nullptr);
    root_window()->RemovePreTargetHandler(&event_capturer_);
    SwitchTouchExplorationMode(false);
    cursor_client_.reset();
    aura::test::AuraTestBase::TearDown();
  }

 protected:
  aura::client::CursorClient* cursor_client() { return cursor_client_.get(); }

  const EventList& GetCapturedEvents() {
    return event_capturer_.captured_events();
  }

  std::vector<ui::LocatedEvent*> GetCapturedLocatedEvents() {
    const EventList& all_events = GetCapturedEvents();
    std::vector<ui::LocatedEvent*> located_events;
    for (size_t i = 0; i < all_events.size(); ++i) {
      if (all_events[i]->IsMouseEvent() || all_events[i]->IsTouchEvent() ||
          all_events[i]->IsGestureEvent()) {
        located_events.push_back(
            static_cast<ui::LocatedEvent*>(all_events[i].get()));
      }
    }
    return located_events;
  }

  std::vector<ui::LocatedEvent*> GetCapturedLocatedEventsOfType(
      ui::EventType type) {
    std::vector<ui::LocatedEvent*> located_events = GetCapturedLocatedEvents();
    std::vector<ui::LocatedEvent*> events;
    for (size_t i = 0; i < located_events.size(); ++i) {
      if (type == located_events[i]->type())
        events.push_back(located_events[i]);
    }
    return events;
  }

  std::vector<gfx::Point>& GetTouchExplorePoints() {
    return delegate_.GetTouchExplorePoints();
  }

  void ClearCapturedAndGestureEvents() {
    event_capturer_.Reset();
    GetTouchExplorePoints().clear();
  }

  void AdvanceSimulatedTimePastTapDelay() {
    simulated_clock_.Advance(gesture_detector_config_.double_tap_timeout);
    simulated_clock_.Advance(base::Milliseconds(1));
    touch_exploration_controller_->CallTapTimerNowForTesting();
  }

  void AdvanceSimulatedTimePastPotentialTapDelay() {
    simulated_clock_.Advance(base::Milliseconds(1000));
    touch_exploration_controller_->CallTapTimerNowIfRunningForTesting();
  }

  void AdvanceSimulatedTimePastLongPressDelay() {
    simulated_clock_.Advance(base::Milliseconds(5000));
    touch_exploration_controller_->CallLongPressTimerNowIfRunningForTesting();
  }

  void SuppressVLOGs(bool suppress) {
    touch_exploration_controller_->SuppressVLOGsForTesting(suppress);
  }

  void SwitchTouchExplorationMode(bool on) {
    if (!on && touch_exploration_controller_.get()) {
      touch_exploration_controller_.reset();
    } else if (on && !touch_exploration_controller_.get()) {
      touch_exploration_controller_ =
          std::make_unique<TouchExplorationControllerTestApi>(
              new TouchExplorationController(root_window(), &delegate_,
                                             nullptr));
      cursor_client()->ShowCursor();
      cursor_client()->DisableMouseEvents();
    }
  }

  void EnterTouchExplorationModeAtLocation(gfx::Point tap_location) {
    ui::TouchEvent touch_press(
        ui::EventType::kTouchPressed, tap_location, Now(),
        ui::PointerDetails(ui::EventPointerType::kTouch, 0));
    generator_->Dispatch(&touch_press);
    AdvanceSimulatedTimePastTapDelay();
    EXPECT_TRUE(IsInTouchToMouseMode());
  }

  bool IsInTouchToMouseMode() {
    aura::client::CursorClient* cursor_client =
        aura::client::GetCursorClient(root_window());
    return cursor_client && cursor_client->IsMouseEventsEnabled() &&
           !cursor_client->IsCursorVisible();
  }

  bool IsInNoFingersDownState() {
    return touch_exploration_controller_->IsInNoFingersDownStateForTesting();
  }

  bool IsInGestureInProgressState() {
    return touch_exploration_controller_
        ->IsInGestureInProgressStateForTesting();
  }

  bool IsInSlideGestureState() {
    return touch_exploration_controller_->IsInSlideGestureStateForTesting();
  }

  bool IsInTwoFingerTapState() {
    return touch_exploration_controller_->IsInTwoFingerTapStateForTesting();
  }

  gfx::Rect BoundsOfRootWindowInDIP() {
    return touch_exploration_controller_->BoundsOfRootWindowInDIPForTesting();
  }

  float GetMaxDistanceFromEdge() const {
    return touch_exploration_controller_->GetMaxDistanceFromEdge();
  }

  float GetSlopDistanceFromEdge() const {
    return touch_exploration_controller_->GetSlopDistanceFromEdge();
  }

  base::TimeTicks Now() { return ui::EventTimeForNow(); }

  void SetTouchAccessibilityAnchorPoint(const gfx::Point& location) {
    touch_exploration_controller_->SetTouchAccessibilityAnchorPoint(location);
  }

  void SetExcludeBounds(const gfx::Rect& bounds) {
    touch_exploration_controller_->SetExcludeBounds(bounds);
  }

  void SetLiftActivationBounds(const gfx::Rect& bounds) {
    touch_exploration_controller_->SetLiftActivationBounds(bounds);
  }

  // Taps at |tap_location|, waiting past tap delay to enter touch
  // exploration. Pass true to |set_anchor_point| to ensure any subsequent
  // gestures like a double tap go to |tap_location|. Usually, ChromeVox sets
  // the anchor, which is the center of the focused node and can differ from
  // |tap_location|.
  void TapAndVerifyTouchExplore(gfx::Point tap_location,
                                bool set_anchor_point = false) {
    generator_->set_current_screen_location(tap_location);
    generator_->PressTouchId(1);
    generator_->ReleaseTouchId(1);
    AdvanceSimulatedTimePastTapDelay();
    if (set_anchor_point)
      SetTouchAccessibilityAnchorPoint(tap_location);

    std::vector<ui::LocatedEvent*> events =
        GetCapturedLocatedEventsOfType(ui::EventType::kMouseMoved);
    ASSERT_TRUE(events.empty());

    ASSERT_EQ(1U, GetTouchExplorePoints().size());
    EXPECT_EQ(tap_location, GetTouchExplorePoints()[0]);
    ClearCapturedAndGestureEvents();
  }

  std::unique_ptr<ui::test::EventGenerator> generator_;
  ui::GestureDetector::Config gesture_detector_config_;
  base::SimpleTestTickClock simulated_clock_;
  MockTouchExplorationControllerDelegate delegate_;

 private:
  EventCapturer event_capturer_;
  std::unique_ptr<TouchExplorationControllerTestApi>
      touch_exploration_controller_;
  std::unique_ptr<aura::test::TestCursorClient> cursor_client_;
};

// Executes a number of assertions to confirm that |e1| and |e2| are touch
// events and are equal to each other.
void ConfirmEventsAreTouchAndEqual(ui::Event* e1, ui::Event* e2) {
  ASSERT_TRUE(e1->IsTouchEvent());
  ASSERT_TRUE(e2->IsTouchEvent());
  ui::TouchEvent* touch_event1 = e1->AsTouchEvent();
  ui::TouchEvent* touch_event2 = e2->AsTouchEvent();
  EXPECT_EQ(touch_event1->type(), touch_event2->type());
  EXPECT_EQ(touch_event1->location(), touch_event2->location());
  EXPECT_EQ(touch_event1->pointer_details().id,
            touch_event2->pointer_details().id);
  EXPECT_EQ(touch_event1->flags(), touch_event2->flags());
  EXPECT_EQ(touch_event1->time_stamp(), touch_event2->time_stamp());
}

// Executes a number of assertions to confirm that |e1| and |e2| are mouse
// events and are equal to each other.
void ConfirmEventsAreMouseAndEqual(ui::Event* e1, ui::Event* e2) {
  ASSERT_TRUE(e1->IsMouseEvent());
  ASSERT_TRUE(e2->IsMouseEvent());
  ui::MouseEvent* mouse_event1 = e1->AsMouseEvent();
  ui::MouseEvent* mouse_event2 = e2->AsMouseEvent();
  EXPECT_EQ(mouse_event1->type(), mouse_event2->type());
  EXPECT_EQ(mouse_event1->location(), mouse_event2->location());
  EXPECT_EQ(mouse_event1->root_location(), mouse_event2->root_location());
  EXPECT_EQ(mouse_event1->flags(), mouse_event2->flags());
}

// Executes a number of assertions to confirm that |e1| and |e2| are key events
// and are equal to each other.
void ConfirmEventsAreKeyAndEqual(ui::Event* e1, ui::Event* e2) {
  ASSERT_TRUE(e1->IsKeyEvent());
  ASSERT_TRUE(e2->IsKeyEvent());
  ui::KeyEvent* key_event1 = e1->AsKeyEvent();
  ui::KeyEvent* key_event2 = e2->AsKeyEvent();
  EXPECT_EQ(key_event1->type(), key_event2->type());
  EXPECT_EQ(key_event1->key_code(), key_event2->key_code());
  EXPECT_EQ(key_event1->code(), key_event2->code());
  EXPECT_EQ(key_event1->flags(), key_event2->flags());
}

#define CONFIRM_EVENTS_ARE_TOUCH_AND_EQUAL(e1, e2) \
  ASSERT_NO_FATAL_FAILURE(ConfirmEventsAreTouchAndEqual(e1, e2))

#define CONFIRM_EVENTS_ARE_MOUSE_AND_EQUAL(e1, e2) \
  ASSERT_NO_FATAL_FAILURE(ConfirmEventsAreMouseAndEqual(e1, e2))

#define CONFIRM_EVENTS_ARE_KEY_AND_EQUAL(e1, e2) \
  ASSERT_NO_FATAL_FAILURE(ConfirmEventsAreKeyAndEqual(e1, e2))

// TODO(mfomitchev): Need to investigate why we don't get mouse enter/exit
// events when running these tests as part of ui_base_unittests. We do get them
// when the tests are run as part of ash unit tests.

TEST_F(TouchExplorationTest, EntersTouchToMouseModeAfterPressAndDelay) {
  SwitchTouchExplorationMode(true);
  EXPECT_FALSE(IsInTouchToMouseMode());
  generator_->PressTouch();
  AdvanceSimulatedTimePastTapDelay();
  EXPECT_TRUE(IsInTouchToMouseMode());
}

TEST_F(TouchExplorationTest, EntersTouchToMouseModeAfterMoveOutsideSlop) {
  int slop = gesture_detector_config_.touch_slop;
  int half_slop = slop / 2;

  SwitchTouchExplorationMode(true);
  EXPECT_FALSE(IsInTouchToMouseMode());
  generator_->set_current_screen_location(gfx::Point(11, 12));
  generator_->PressTouch();
  generator_->MoveTouch(gfx::Point(11 + half_slop, 12));
  EXPECT_FALSE(IsInTouchToMouseMode());
  generator_->MoveTouch(gfx::Point(11, 12 + half_slop));
  EXPECT_FALSE(IsInTouchToMouseMode());
  AdvanceSimulatedTimePastTapDelay();
  generator_->MoveTouch(gfx::Point(11 + slop + 1, 12));
  EXPECT_TRUE(IsInTouchToMouseMode());
}

TEST_F(TouchExplorationTest, OneFingerTap) {
  SwitchTouchExplorationMode(true);
  gfx::Point location(11, 12);
  generator_->set_current_screen_location(location);
  generator_->PressTouch();
  generator_->ReleaseTouch();
  AdvanceSimulatedTimePastTapDelay();

  std::vector<ui::LocatedEvent*> events =
      GetCapturedLocatedEventsOfType(ui::EventType::kMouseMoved);
  ASSERT_EQ(0U, events.size());

  ASSERT_EQ(1U, GetTouchExplorePoints().size());
  EXPECT_EQ(location, GetTouchExplorePoints()[0]);
  EXPECT_TRUE(IsInNoFingersDownState());
}

TEST_F(TouchExplorationTest, ActualMouseMovesUnaffected) {
  SwitchTouchExplorationMode(true);

  gfx::Point location_start(11, 12);
  gfx::Point location_end(13, 14);
  generator_->set_current_screen_location(location_start);
  generator_->PressTouch();
  AdvanceSimulatedTimePastTapDelay();
  generator_->MoveTouch(location_end);

  gfx::Point location_real_mouse_move(15, 16);
  ui::MouseEvent mouse_move(ui::EventType::kMouseMoved,
                            location_real_mouse_move, location_real_mouse_move,
                            ui::EventTimeForNow(), 0, 0);
  generator_->Dispatch(&mouse_move);
  generator_->ReleaseTouch();
  AdvanceSimulatedTimePastTapDelay();

  std::vector<ui::LocatedEvent*> events =
      GetCapturedLocatedEventsOfType(ui::EventType::kMouseMoved);
  std::vector<gfx::Point> touch_explore_points = GetTouchExplorePoints();
  ASSERT_EQ(1U, events.size());
  ASSERT_EQ(3U, touch_explore_points.size());

  EXPECT_EQ(location_start, touch_explore_points[0]);
  EXPECT_EQ(location_end, touch_explore_points[1]);
  EXPECT_EQ(location_end, touch_explore_points[2]);

  // The real mouse move goes through.
  EXPECT_EQ(location_real_mouse_move, events[0]->location());
  CONFIRM_EVENTS_ARE_MOUSE_AND_EQUAL(events[0], &mouse_move);
  EXPECT_FALSE(events[0]->flags() & ui::EF_IS_SYNTHESIZED);
  EXPECT_FALSE(events[0]->flags() & ui::EF_TOUCH_ACCESSIBILITY);

  EXPECT_TRUE(IsInNoFingersDownState());
}

// Turn the touch exploration mode on in the middle of the touch gesture.
// Confirm that events from the finger which was touching when the mode was
// turned on don't get rewritten.
TEST_F(TouchExplorationTest, TurnOnMidTouch) {
  SwitchTouchExplorationMode(false);
  generator_->PressTouchId(1);
  EXPECT_TRUE(cursor_client()->IsCursorVisible());
  ClearCapturedAndGestureEvents();

  // Enable touch exploration mode while the first finger is touching the
  // screen. Ensure that subsequent events from that first finger are not
  // affected by the touch exploration mode, while the touch events from another
  // finger get rewritten.
  SwitchTouchExplorationMode(true);
  ui::TouchEvent touch_move(
      ui::EventType::kTouchMoved, gfx::Point(11, 12), Now(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 1));
  generator_->Dispatch(&touch_move);
  EXPECT_TRUE(cursor_client()->IsCursorVisible());
  EXPECT_FALSE(cursor_client()->IsMouseEventsEnabled());
  std::vector<ui::LocatedEvent*> captured_events = GetCapturedLocatedEvents();
  ASSERT_EQ(1u, captured_events.size());
  CONFIRM_EVENTS_ARE_TOUCH_AND_EQUAL(captured_events[0], &touch_move);
  ClearCapturedAndGestureEvents();

  // The press from the second finger should get rewritten.
  generator_->PressTouchId(2);
  AdvanceSimulatedTimePastTapDelay();
  EXPECT_TRUE(IsInTouchToMouseMode());

  ASSERT_FALSE(GetTouchExplorePoints().empty());

  // The release of the first finger shouldn't be affected.
  ui::TouchEvent touch_release(
      ui::EventType::kTouchReleased, gfx::Point(11, 12), Now(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 1));
  generator_->Dispatch(&touch_release);
  captured_events = GetCapturedLocatedEvents();
  ASSERT_EQ(1u, captured_events.size());
  CONFIRM_EVENTS_ARE_TOUCH_AND_EQUAL(captured_events[0], &touch_release);
  ClearCapturedAndGestureEvents();

  // The move and release from the second finger should get rewritten.
  generator_->MoveTouchId(gfx::Point(13, 14), 2);
  generator_->ReleaseTouchId(2);
  AdvanceSimulatedTimePastTapDelay();
  captured_events = GetCapturedLocatedEvents();
  ASSERT_TRUE(captured_events.empty());
  ASSERT_EQ(2U, GetTouchExplorePoints().size());
  EXPECT_TRUE(IsInNoFingersDownState());
}

// If an event is received after the double-tap timeout has elapsed, but
// before the timer has fired, a mouse move should still be generated.
TEST_F(TouchExplorationTest, TimerFiresLateDuringTouchExploration) {
  SwitchTouchExplorationMode(true);

  // Make sure the touch is not in a corner of the screen.
  generator_->MoveTouch(gfx::Point(100, 200));

  // Send a press, then add another finger after the double-tap timeout.
  generator_->PressTouchId(1);
  simulated_clock_.Advance(base::Milliseconds(1000));
  generator_->PressTouchId(2);
  std::vector<ui::LocatedEvent*> events =
      GetCapturedLocatedEventsOfType(ui::EventType::kMouseMoved);
  ASSERT_TRUE(events.empty());

  ASSERT_EQ(1U, GetTouchExplorePoints().size());

  generator_->ReleaseTouchId(2);
  generator_->ReleaseTouchId(1);
  AdvanceSimulatedTimePastTapDelay();
  EXPECT_TRUE(IsInNoFingersDownState());
}

// If a new tap is received after the double-tap timeout has elapsed from
// a previous tap, but before the timer has fired, a mouse move should
// still be generated from the old tap.
TEST_F(TouchExplorationTest, TimerFiresLateAfterTap) {
  SwitchTouchExplorationMode(true);

  // Send a tap at location1.
  gfx::Point location0(11, 12);
  generator_->set_current_screen_location(location0);
  generator_->PressTouch();
  generator_->ReleaseTouch();

  // Send a tap at location2, after the double-tap timeout, but before the
  // timer fires.
  gfx::Point location1(33, 34);
  generator_->set_current_screen_location(location1);
  simulated_clock_.Advance(base::Milliseconds(301));
  generator_->PressTouch();
  generator_->ReleaseTouch();
  AdvanceSimulatedTimePastTapDelay();

  std::vector<ui::LocatedEvent*> events =
      GetCapturedLocatedEventsOfType(ui::EventType::kMouseMoved);
  ASSERT_TRUE(events.empty());

  ASSERT_EQ(2U, GetTouchExplorePoints().size());
  EXPECT_EQ(location0, GetTouchExplorePoints()[0]);
  EXPECT_EQ(location1, GetTouchExplorePoints()[1]);
  EXPECT_TRUE(IsInNoFingersDownState());
}

// Double-tapping should send a touch press and release through to the location
// of the last successful touch exploration.
TEST_F(TouchExplorationTest, DoubleTap) {
  SwitchTouchExplorationMode(true);
  gfx::Point tap_location(51, 52);
  TapAndVerifyTouchExplore(tap_location);
  // Now double-tap at a different location. This should result in
  // no touches at all, but a click gesture to ChromeVox.
  gfx::Point double_tap_location(33, 34);
  generator_->set_current_screen_location(double_tap_location);
  generator_->PressTouch();
  generator_->ReleaseTouch();
  generator_->PressTouch();
  generator_->ReleaseTouch();

  std::vector<ui::LocatedEvent*> captured_events = GetCapturedLocatedEvents();
  ASSERT_TRUE(captured_events.empty());
  EXPECT_EQ(ax::mojom::Gesture::kClick, delegate_.GetLastGesture());
  EXPECT_TRUE(IsInNoFingersDownState());
}

// The press of the second tap in a double-tap must come within the double-tap
// timeout, but the release of the second tap can come later.
TEST_F(TouchExplorationTest, DoubleTapTiming) {
  SwitchTouchExplorationMode(true);
  gfx::Point tap_location(51, 52);
  TapAndVerifyTouchExplore(tap_location, true);

  // The press of the second tap happens in time, but the release does not.
  gfx::Point double_tap_location(33, 34);
  generator_->set_current_screen_location(double_tap_location);
  generator_->PressTouch();
  generator_->ReleaseTouch();
  simulated_clock_.Advance(gesture_detector_config_.double_tap_timeout -
                           base::Milliseconds(25));
  generator_->PressTouch();
  simulated_clock_.Advance(base::Milliseconds(50));
  generator_->ReleaseTouch();

  std::vector<ui::LocatedEvent*> captured_events = GetCapturedLocatedEvents();
  ASSERT_EQ(0U, captured_events.size());
  EXPECT_EQ(ax::mojom::Gesture::kClick, delegate_.GetLastGesture());
}

// If an explicit anchor point is set during touch exploration, double-tapping
// should send a 'click' gesture rather than a simulated touch press and
// release.
TEST_F(TouchExplorationTest, DoubleTapWithExplicitAnchorPoint) {
  SwitchTouchExplorationMode(true);

  gfx::Point tap_location(51, 52);
  TapAndVerifyTouchExplore(tap_location, true);

  // Now double-tap at a different location. This should result in
  // a click gesture.
  gfx::Point double_tap_location(33, 34);
  generator_->set_current_screen_location(double_tap_location);
  generator_->PressTouch();
  generator_->ReleaseTouch();
  generator_->PressTouch();
  generator_->ReleaseTouch();

  std::vector<ui::LocatedEvent*> captured_events = GetCapturedLocatedEvents();
  ASSERT_EQ(0U, captured_events.size());
  EXPECT_TRUE(IsInNoFingersDownState());
  EXPECT_EQ(ax::mojom::Gesture::kClick, delegate_.GetLastGesture());
}

// Double-tapping where the user holds their finger down for the second time
// for a longer press should send a touch press and passthrough all further
// events from that finger. Other finger presses should be ignored.
TEST_F(TouchExplorationTest, DoubleTapPassthrough) {
  SwitchTouchExplorationMode(true);
  gfx::Point tap_location(11, 12);
  TapAndVerifyTouchExplore(tap_location);

  // Now double-tap and hold at a different location.
  // This should result in a single touch press at the location of the tap,
  // not at the location of the double-tap.
  gfx::Point first_tap_location(13, 14);
  generator_->set_current_screen_location(first_tap_location);
  generator_->PressTouchId(1);
  generator_->ReleaseTouchId(1);
  ASSERT_EQ(0U, delegate_.NumPassthroughSounds());
  gfx::Point second_tap_location(15, 16);
  generator_->set_current_screen_location(second_tap_location);
  generator_->PressTouchId(1);
  // Advance to the finger passing through.
  AdvanceSimulatedTimePastTapDelay();
  ASSERT_EQ(1U, delegate_.NumPassthroughSounds());

  gfx::Vector2d passthrough_offset = second_tap_location - tap_location;

  std::vector<ui::LocatedEvent*> captured_events = GetCapturedLocatedEvents();
  ASSERT_EQ(1U, captured_events.size());
  EXPECT_EQ(ui::EventType::kTouchPressed, captured_events[0]->type());
  EXPECT_EQ(second_tap_location - passthrough_offset,
            captured_events[0]->location());
  EXPECT_TRUE(captured_events[0]->flags() & ui::EF_TOUCH_ACCESSIBILITY);
  ClearCapturedAndGestureEvents();

  // All events for the first finger should pass through now, displaced
  // relative to the last touch exploration location.
  gfx::Point first_move_location(17, 18);
  generator_->MoveTouchId(first_move_location, 1);
  gfx::Point second_move_location(12, 13);
  generator_->MoveTouchId(second_move_location, 1);

  captured_events = GetCapturedLocatedEvents();
  ASSERT_EQ(2U, captured_events.size());
  EXPECT_EQ(ui::EventType::kTouchMoved, captured_events[0]->type());
  EXPECT_EQ(first_move_location - passthrough_offset,
            captured_events[0]->location());
  EXPECT_EQ(ui::EventType::kTouchMoved, captured_events[1]->type());
  EXPECT_EQ(second_move_location - passthrough_offset,
            captured_events[1]->location());
  EXPECT_TRUE(captured_events[1]->flags() & ui::EF_TOUCH_ACCESSIBILITY);
  ClearCapturedAndGestureEvents();

  // Events for other fingers should do nothing.
  generator_->PressTouchId(2);
  generator_->PressTouchId(3);
  generator_->MoveTouchId(gfx::Point(34, 36), 2);
  generator_->ReleaseTouchId(2);
  captured_events = GetCapturedLocatedEvents();
  ASSERT_EQ(0U, captured_events.size());

  // Even with finger 3 still down, events for the first finger should still
  // pass through.
  gfx::Point third_move_location(14, 15);
  generator_->MoveTouchId(third_move_location, 1);
  captured_events = GetCapturedLocatedEvents();
  ASSERT_EQ(1U, captured_events.size());
  EXPECT_EQ(ui::EventType::kTouchMoved, captured_events[0]->type());
  EXPECT_EQ(third_move_location - passthrough_offset,
            captured_events[0]->location());
  EXPECT_TRUE(captured_events[0]->flags() & ui::EF_TOUCH_ACCESSIBILITY);

  // No fingers down state is only reached when every finger is lifted.
  generator_->ReleaseTouchId(1);
  EXPECT_FALSE(IsInNoFingersDownState());
  generator_->ReleaseTouchId(3);
  EXPECT_TRUE(IsInNoFingersDownState());

  // There should have only ever been one pass through earcon played.
  ASSERT_EQ(1U, delegate_.NumPassthroughSounds());
}

// Double-tapping, going into passthrough, and holding for the longpress
// time should send a touch press and released (right click)
// to the location of the last successful touch exploration.
TEST_F(TouchExplorationTest, DoubleTapLongPress) {
  SwitchTouchExplorationMode(true);
  gfx::Point tap_location(11, 12);
  TapAndVerifyTouchExplore(tap_location);

  // Now double-tap and hold at a different location.
  // This should result in a single touch long press and release
  // at the location of the tap, not at the location of the double-tap.
  // There should be a time delay between the touch press and release.
  gfx::Point first_tap_location(33, 34);
  generator_->set_current_screen_location(first_tap_location);
  generator_->PressTouch();
  generator_->ReleaseTouch();
  gfx::Point second_tap_location(23, 24);
  generator_->set_current_screen_location(second_tap_location);
  generator_->PressTouch();
  // Advance to the finger passing through, and then to the longpress timeout.
  AdvanceSimulatedTimePastTapDelay();
  simulated_clock_.Advance(gesture_detector_config_.longpress_timeout);
  generator_->ReleaseTouch();

  std::vector<ui::LocatedEvent*> captured_events = GetCapturedLocatedEvents();
  ASSERT_EQ(2U, captured_events.size());
  EXPECT_EQ(ui::EventType::kTouchPressed, captured_events[0]->type());
  EXPECT_EQ(tap_location, captured_events[0]->location());
  EXPECT_TRUE(captured_events[0]->flags() & ui::EF_TOUCH_ACCESSIBILITY);
  base::TimeTicks pressed_time = captured_events[0]->time_stamp();
  EXPECT_EQ(ui::EventType::kTouchReleased, captured_events[1]->type());
  EXPECT_EQ(tap_location, captured_events[1]->location());
  EXPECT_TRUE(captured_events[1]->flags() & ui::EF_TOUCH_ACCESSIBILITY);
  base::TimeTicks released_time = captured_events[1]->time_stamp();
  EXPECT_EQ(released_time - pressed_time,
            gesture_detector_config_.longpress_timeout);
}

// Single-tapping should send a touch press and release through to the location
// of the last successful touch exploration if the grace period has not
// elapsed.
TEST_F(TouchExplorationTest, SingleTap) {
  SwitchTouchExplorationMode(true);

  // Tap once to simulate a touch explore point.
  gfx::Point initial_location(11, 12);
  generator_->set_current_screen_location(initial_location);
  generator_->PressTouch();
  AdvanceSimulatedTimePastTapDelay();
  ASSERT_EQ(1U, GetTouchExplorePoints().size());

  // Move to another location for single tap
  gfx::Point tap_location(22, 23);
  generator_->MoveTouch(tap_location);
  generator_->ReleaseTouch();

  // Allow time to pass within the grace period of releasing before
  // tapping again.
  gfx::Point final_location(33, 34);
  generator_->set_current_screen_location(final_location);
  simulated_clock_.Advance(base::Milliseconds(250));
  generator_->PressTouch();
  generator_->ReleaseTouch();

  ASSERT_EQ(3U, GetTouchExplorePoints().size());

  std::vector<ui::LocatedEvent*> captured_events = GetCapturedLocatedEvents();
  ASSERT_TRUE(captured_events.empty());
  EXPECT_EQ(ax::mojom::Gesture::kClick, delegate_.GetLastGesture());
}

// Double-tapping without coming from touch exploration (no previous touch
// exploration event) should not generate any events.
TEST_F(TouchExplorationTest, DoubleTapNoTouchExplore) {
  SwitchTouchExplorationMode(true);

  // Double-tap without any previous touch.
  // Touch exploration mode has not been entered, so there is no previous
  // touch exploration event. The double-tap should be discarded, and no events
  // should be generated at all.
  gfx::Point double_tap_location(33, 34);
  generator_->set_current_screen_location(double_tap_location);
  generator_->PressTouch();
  generator_->ReleaseTouch();
  generator_->PressTouch();
  // Since the state stays in single_tap_released, we need to make sure the
  // tap timer doesn't fire and set the state to no fingers down (since there
  // is still a finger down).
  AdvanceSimulatedTimePastPotentialTapDelay();
  EXPECT_FALSE(IsInNoFingersDownState());
  generator_->ReleaseTouch();
  EXPECT_TRUE(IsInNoFingersDownState());

  std::vector<ui::LocatedEvent*> captured_events = GetCapturedLocatedEvents();
  ASSERT_EQ(0U, captured_events.size());
}

// Tapping and releasing with a second finger when in touch exploration mode
// should send a touch press and released to the location of the last
// successful touch exploration and return to touch explore.
TEST_F(TouchExplorationTest, SplitTap) {
  SwitchTouchExplorationMode(true);
  gfx::Point initial_touch_location(11, 12);
  gfx::Point second_touch_location(33, 34);

  // Tap and hold at one location, and get a mouse move event in touch explore.
  EnterTouchExplorationModeAtLocation(initial_touch_location);
  std::vector<ui::LocatedEvent*> events =
      GetCapturedLocatedEventsOfType(ui::EventType::kMouseMoved);
  ASSERT_TRUE(events.empty());

  ASSERT_EQ(1U, GetTouchExplorePoints().size());
  EXPECT_EQ(initial_touch_location, GetTouchExplorePoints()[0]);
  ClearCapturedAndGestureEvents();
  EXPECT_TRUE(IsInTouchToMouseMode());

  // Now tap and release at a different location. This should result in a
  // single touch and release at the location of the first (held) tap,
  // not at the location of the second tap and release.
  // After the release, there is still a finger in touch explore mode.
  ui::TouchEvent split_tap_press(
      ui::EventType::kTouchPressed, second_touch_location, Now(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 1));
  generator_->Dispatch(&split_tap_press);
  // To simulate the behavior of the real device, we manually disable
  // mouse events. To not rely on manually setting the state, this is also
  // tested in touch_exploration_controller_browsertest.
  cursor_client()->DisableMouseEvents();
  EXPECT_FALSE(cursor_client()->IsMouseEventsEnabled());
  EXPECT_FALSE(cursor_client()->IsCursorVisible());
  EXPECT_FALSE(IsInGestureInProgressState());
  ui::TouchEvent split_tap_release(
      ui::EventType::kTouchReleased, second_touch_location, Now(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 1));
  generator_->Dispatch(&split_tap_release);
  // Releasing the second finger should re-enable mouse events putting us
  // back into the touch exploration mode.
  EXPECT_TRUE(IsInTouchToMouseMode());
  EXPECT_FALSE(IsInNoFingersDownState());

  std::vector<ui::LocatedEvent*> captured_events = GetCapturedLocatedEvents();
  ASSERT_TRUE(captured_events.empty());
  EXPECT_EQ(ax::mojom::Gesture::kClick, delegate_.GetLastGesture());
  ClearCapturedAndGestureEvents();

  ui::TouchEvent touch_explore_release(
      ui::EventType::kTouchReleased, initial_touch_location, Now(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  generator_->Dispatch(&touch_explore_release);
  AdvanceSimulatedTimePastTapDelay();
  EXPECT_TRUE(IsInNoFingersDownState());
}

// If split tap is started but the touch explore finger is released first,
// there should still be a touch press and release sent to the location of
// the last successful touch exploration.
// Both fingers should be released after the click goes through.
TEST_F(TouchExplorationTest, SplitTapRelease) {
  SwitchTouchExplorationMode(true);

  gfx::Point initial_touch_location(11, 12);
  gfx::Point second_touch_location(33, 34);

  // Tap and hold at one location, and get a mouse move event in touch explore.
  EnterTouchExplorationModeAtLocation(initial_touch_location);

  std::vector<ui::LocatedEvent*> events =
      GetCapturedLocatedEventsOfType(ui::EventType::kMouseMoved);
  ASSERT_TRUE(events.empty());
  ASSERT_EQ(1U, GetTouchExplorePoints().size());

  // Now tap at a different location. Release at the first location,
  // then release at the second. This should result in a
  // click gesture to ChromeVox.
  ui::TouchEvent split_tap_press(
      ui::EventType::kTouchPressed, second_touch_location, Now(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 1));
  generator_->Dispatch(&split_tap_press);
  ui::TouchEvent touch_explore_release(
      ui::EventType::kTouchReleased, initial_touch_location, Now(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  generator_->Dispatch(&touch_explore_release);
  ui::TouchEvent split_tap_release(
      ui::EventType::kTouchReleased, second_touch_location, Now(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 1));
  generator_->Dispatch(&split_tap_release);
  EXPECT_TRUE(IsInNoFingersDownState());

  std::vector<ui::LocatedEvent*> captured_events = GetCapturedLocatedEvents();
  EXPECT_TRUE(captured_events.empty());
  EXPECT_EQ(ax::mojom::Gesture::kClick, delegate_.GetLastGesture());
}

TEST_F(TouchExplorationTest, SplitTapMultiFinger) {
  SwitchTouchExplorationMode(true);
  gfx::Point initial_touch_location(11, 12);
  gfx::Point second_touch_location(33, 34);
  gfx::Point third_touch_location(16, 17);

  // Tap and hold at one location, and get a mouse move event in touch explore.
  EnterTouchExplorationModeAtLocation(initial_touch_location);

  std::vector<ui::LocatedEvent*> events =
      GetCapturedLocatedEventsOfType(ui::EventType::kMouseMoved);
  ASSERT_TRUE(events.empty());

  ASSERT_EQ(1U, GetTouchExplorePoints().size());
  EXPECT_EQ(initial_touch_location, GetTouchExplorePoints()[0]);
  ClearCapturedAndGestureEvents();

  // Now tap at a different location
  ui::TouchEvent split_tap_press(
      ui::EventType::kTouchPressed, second_touch_location, Now(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 1));
  generator_->Dispatch(&split_tap_press);
  simulated_clock_.Advance(gesture_detector_config_.longpress_timeout);

  // Placing a third finger on the screen should cancel the split tap and
  // enter the wait state.
  ui::TouchEvent third_press(
      ui::EventType::kTouchPressed, third_touch_location, Now(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 2));
  generator_->Dispatch(&third_press);

  // When all three fingers are released, no events should be captured.
  // All fingers should then be up.
  ui::TouchEvent touch_explore_release(
      ui::EventType::kTouchReleased, initial_touch_location, Now(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  generator_->Dispatch(&touch_explore_release);
  ui::TouchEvent split_tap_release(
      ui::EventType::kTouchReleased, second_touch_location, Now(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 1));
  generator_->Dispatch(&split_tap_release);
  ui::TouchEvent third_tap_release(
      ui::EventType::kTouchReleased, third_touch_location, Now(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 2));
  generator_->Dispatch(&third_tap_release);

  std::vector<ui::LocatedEvent*> captured_events = GetCapturedLocatedEvents();
  ASSERT_EQ(0U, captured_events.size());
  EXPECT_TRUE(IsInNoFingersDownState());
}

TEST_F(TouchExplorationTest, SplitTapLeaveSlop) {
  SwitchTouchExplorationMode(true);
  gfx::Point first_touch_location(11, 12);
  gfx::Point second_touch_location(33, 34);
  gfx::Point first_move_location(
      first_touch_location.x() + gesture_detector_config_.touch_slop * 3 + 1,
      first_touch_location.y());
  gfx::Point second_move_location(
      second_touch_location.x() + gesture_detector_config_.touch_slop * 3 + 1,
      second_touch_location.y());

  // Tap and hold at one location, and get a mouse move event in touch explore.
  EnterTouchExplorationModeAtLocation(first_touch_location);
  ClearCapturedAndGestureEvents();

  // Now tap at a different location for split tap.
  ui::TouchEvent split_tap_press(
      ui::EventType::kTouchPressed, second_touch_location, Now(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 1));
  generator_->Dispatch(&split_tap_press);

  // Move the first finger out of slop and release both fingers. The split
  // tap should have been cancelled.
  ui::TouchEvent first_touch_move(
      ui::EventType::kTouchMoved, first_move_location, Now(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  generator_->Dispatch(&first_touch_move);
  ui::TouchEvent first_touch_release(
      ui::EventType::kTouchReleased, first_move_location, Now(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  generator_->Dispatch(&first_touch_release);
  ui::TouchEvent second_touch_release(
      ui::EventType::kTouchReleased, second_touch_location, Now(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 1));
  generator_->Dispatch(&second_touch_release);

  std::vector<ui::LocatedEvent*> captured_events = GetCapturedLocatedEvents();
  ASSERT_EQ(0U, captured_events.size());
  EXPECT_TRUE(IsInNoFingersDownState());

  // Now do the same, but moving the split tap finger out of slop
  EnterTouchExplorationModeAtLocation(first_touch_location);
  ClearCapturedAndGestureEvents();
  ui::TouchEvent split_tap_press2(
      ui::EventType::kTouchPressed, second_touch_location, Now(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 1));
  generator_->Dispatch(&split_tap_press2);

  // Move the second finger out of slop and release both fingers. The split
  // tap should have been cancelled.
  ui::TouchEvent second_touch_move2(
      ui::EventType::kTouchMoved, second_move_location, Now(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 1));
  generator_->Dispatch(&second_touch_move2);
  ui::TouchEvent first_touch_release2(
      ui::EventType::kTouchReleased, first_touch_location, Now(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  generator_->Dispatch(&first_touch_release2);
  ui::TouchEvent second_touch_release2(
      ui::EventType::kTouchReleased, second_move_location, Now(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 1));
  generator_->Dispatch(&second_touch_release2);

  captured_events = GetCapturedLocatedEvents();
  ASSERT_EQ(0U, captured_events.size());
  EXPECT_TRUE(IsInNoFingersDownState());
}

// Finger must have moved more than slop, faster than the minimum swipe
// velocity, and before the tap timer fires in order to enter
// GestureInProgress state. Otherwise, if the tap timer fires before the a
// gesture is completed, enter touch exploration.
TEST_F(TouchExplorationTest, EnterGestureInProgressState) {
  SwitchTouchExplorationMode(true);
  EXPECT_FALSE(IsInTouchToMouseMode());
  EXPECT_FALSE(IsInGestureInProgressState());

  float distance = gesture_detector_config_.touch_slop + 1;
  ui::TouchEvent first_press(
      ui::EventType::kTouchPressed, gfx::Point(0, 1), Now(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  gfx::Point second_location(distance / 2, 1);
  gfx::Point third_location(distance, 1);
  gfx::Point touch_exploration_location(20, 21);

  generator_->Dispatch(&first_press);
  simulated_clock_.Advance(base::Milliseconds(10));
  // Since we are not out of the touch slop yet, we should not be in gesture in
  // progress.
  generator_->MoveTouch(second_location);
  EXPECT_FALSE(IsInTouchToMouseMode());
  EXPECT_FALSE(IsInGestureInProgressState());
  simulated_clock_.Advance(base::Milliseconds(10));

  // Once we are out of slop, we should be in GestureInProgress.
  generator_->MoveTouch(third_location);
  EXPECT_TRUE(IsInGestureInProgressState());
  EXPECT_FALSE(IsInTouchToMouseMode());
  const EventList& captured_events = GetCapturedEvents();
  ASSERT_EQ(0U, captured_events.size());

  // Exit out of gesture mode once grace period is over and enter touch
  // exploration. There should be a touch explore gesture when entering touch
  // exploration and also for the touch move.
  AdvanceSimulatedTimePastTapDelay();
  generator_->MoveTouch(touch_exploration_location);
  ASSERT_TRUE(captured_events.empty());
  EXPECT_EQ(2U, GetTouchExplorePoints().size());

  EXPECT_TRUE(IsInTouchToMouseMode());
  EXPECT_FALSE(IsInGestureInProgressState());
}

// A swipe+direction gesture should trigger a Shift+Search+Direction
// keyboard event.
TEST_F(TouchExplorationTest, GestureSwipe) {
  SwitchTouchExplorationMode(true);

  // Test all four swipe directions with 1 to 4 fingers.
  struct GestureInfo {
    int move_x;
    int move_y;
    int num_fingers;
    ax::mojom::Gesture expected_gesture;
  } gestures_to_test[] = {
      {-1, 0, 1, ax::mojom::Gesture::kSwipeLeft1},
      {0, -1, 1, ax::mojom::Gesture::kSwipeUp1},
      {1, 0, 1, ax::mojom::Gesture::kSwipeRight1},
      {0, 1, 1, ax::mojom::Gesture::kSwipeDown1},
      {-1, 0, 2, ax::mojom::Gesture::kSwipeLeft2},
      {0, -1, 2, ax::mojom::Gesture::kSwipeUp2},
      {1, 0, 2, ax::mojom::Gesture::kSwipeRight2},
      {0, 1, 2, ax::mojom::Gesture::kSwipeDown2},
      {-1, 0, 3, ax::mojom::Gesture::kSwipeLeft3},
      {0, -1, 3, ax::mojom::Gesture::kSwipeUp3},
      {1, 0, 3, ax::mojom::Gesture::kSwipeRight3},
      {0, 1, 3, ax::mojom::Gesture::kSwipeDown3},
      {-1, 0, 4, ax::mojom::Gesture::kSwipeLeft4},
      {0, -1, 4, ax::mojom::Gesture::kSwipeUp4},
      {1, 0, 4, ax::mojom::Gesture::kSwipeRight4},
      {0, 1, 4, ax::mojom::Gesture::kSwipeDown4},
  };

  // This value was taken from gesture_recognizer_unittest.cc in a swipe
  // detector test, since it seems to be about the right amount to get a swipe.
  const int kSteps = 15;

  for (size_t i = 0; i < std::size(gestures_to_test); ++i) {
    const float distance = 2 * gesture_detector_config_.touch_slop + 1;
    int move_x = gestures_to_test[i].move_x * distance;
    int move_y = gestures_to_test[i].move_y * distance;
    int num_fingers = gestures_to_test[i].num_fingers;
    ax::mojom::Gesture expected_gesture = gestures_to_test[i].expected_gesture;

    std::vector<gfx::Point> start_points;
    for (int j = 0; j < num_fingers; j++) {
      start_points.push_back(gfx::Point(j * 10 + 100, j * 10 + 200));
    }
    gfx::Point* start_points_array = &start_points[0];

    // A swipe is made when a fling starts
    float delta_time =
        distance / gesture_detector_config_.maximum_fling_velocity;
    // delta_time is in seconds, so we convert to ms.
    int delta_time_ms = floor(delta_time * 1000);
    generator_->GestureMultiFingerScroll(num_fingers, start_points_array,
                                         delta_time_ms, kSteps, move_x, move_y);
    EXPECT_EQ(expected_gesture, delegate_.GetLastGesture());
    EXPECT_TRUE(IsInNoFingersDownState());
    EXPECT_FALSE(IsInTouchToMouseMode());
    EXPECT_FALSE(IsInGestureInProgressState());
    ClearCapturedAndGestureEvents();
  }
}

TEST_F(TouchExplorationTest, GestureSwipePortrit) {
  // Rotate the window 90-degrees counter-clockwise.
  root_window()->GetHost()->SetRootTransform(gfx::Transform::RowMajor(
      0, 1, 0, 0, -1, 0, 0, root_window()->bounds().height(), 0, 0, 0, 0, 0, 0,
      0, 0));

  SwitchTouchExplorationMode(true);

  // Test 2-4 finger gestures.
  struct GestureInfo {
    int move_x;
    int move_y;
    int num_fingers;
    ax::mojom::Gesture expected_gesture;
  } gestures_to_test[] = {
      {-1, 0, 2, ax::mojom::Gesture::kSwipeDown2},
      {0, -1, 2, ax::mojom::Gesture::kSwipeLeft2},
      {1, 0, 2, ax::mojom::Gesture::kSwipeUp2},
      {0, 1, 2, ax::mojom::Gesture::kSwipeRight2},
      {-1, 0, 3, ax::mojom::Gesture::kSwipeDown3},
      {0, -1, 3, ax::mojom::Gesture::kSwipeLeft3},
      {1, 0, 3, ax::mojom::Gesture::kSwipeUp3},
      {0, 1, 3, ax::mojom::Gesture::kSwipeRight3},
      {-1, 0, 4, ax::mojom::Gesture::kSwipeDown4},
      {0, -1, 4, ax::mojom::Gesture::kSwipeLeft4},
      {1, 0, 4, ax::mojom::Gesture::kSwipeUp4},
      {0, 1, 4, ax::mojom::Gesture::kSwipeRight4},
  };

  // This value was taken from gesture_recognizer_unittest.cc in a swipe
  // detector test, since it seems to be about the right amount to get a swipe.
  const int kSteps = 15;

  for (size_t i = 0; i < std::size(gestures_to_test); ++i) {
    const float distance = 2 * gesture_detector_config_.touch_slop + 1;
    int move_x = gestures_to_test[i].move_x * distance;
    int move_y = gestures_to_test[i].move_y * distance;
    int num_fingers = gestures_to_test[i].num_fingers;
    ax::mojom::Gesture expected_gesture = gestures_to_test[i].expected_gesture;

    std::vector<gfx::Point> start_points;
    for (int j = 0; j < num_fingers; j++) {
      start_points.push_back(gfx::Point(j * 10 + 100, j * 10 + 200));
    }
    gfx::Point* start_points_array = &start_points[0];

    // A swipe is made when a fling starts
    float delta_time =
        distance / gesture_detector_config_.maximum_fling_velocity;
    // delta_time is in seconds, so we convert to ms.
    int delta_time_ms = floor(delta_time * 1000);
    generator_->GestureMultiFingerScroll(num_fingers, start_points_array,
                                         delta_time_ms, kSteps, move_x, move_y);
    EXPECT_EQ(expected_gesture, delegate_.GetLastGesture());
    EXPECT_TRUE(IsInNoFingersDownState());
    EXPECT_FALSE(IsInTouchToMouseMode());
    EXPECT_FALSE(IsInGestureInProgressState());
    ClearCapturedAndGestureEvents();
  }
}

TEST_F(TouchExplorationTest, AllFingerPermutations) {
  SwitchTouchExplorationMode(true);
  SuppressVLOGs(true);
  // We will test all permutations of events from one finger
  // to ensure that we return to NO_FINGERS_DOWN when fingers have been
  // released.
  std::vector<std::unique_ptr<ui::TouchEvent>> all_events;

  // A copy of all events list which can be modified without destrying events.
  std::vector<ui::TouchEvent*> queued_events;

  for (int touch_id = 0; touch_id < 3; ++touch_id) {
    all_events.clear();
    int x = 10 * touch_id + 1;
    int y = 10 * touch_id + 2;
    all_events.push_back(std::make_unique<ui::TouchEvent>(
        ui::EventType::kTouchPressed, gfx::Point(x++, y++), Now(),
        ui::PointerDetails(ui::EventPointerType::kTouch, touch_id)));
    all_events.push_back(std::make_unique<ui::TouchEvent>(
        ui::EventType::kTouchMoved, gfx::Point(x++, y++), Now(),
        ui::PointerDetails(ui::EventPointerType::kTouch, touch_id)));
    all_events.push_back(std::make_unique<ui::TouchEvent>(
        ui::EventType::kTouchReleased, gfx::Point(x, y), Now(),
        ui::PointerDetails(ui::EventPointerType::kTouch, touch_id)));

    // I'm going to explain this algorithm, and use an example in parentheses.
    // The example will be all permutations of a b c d.
    // There are four letters and 4! = 24 permutations.
    const int num_events = all_events.size();
    const int num_permutations = Factorial(num_events);

    for (int p = 0; p < num_permutations; p++) {
      std::vector<bool> fingers_pressed(3, false);

      // Initialize queued_events.
      for (size_t i = 0; i < all_events.size(); ++i) {
        queued_events.push_back(all_events[i].get());
      }

      int current_num_permutations = num_permutations;
      for (int events_left = num_events; events_left > 0; events_left--) {
        // |p| indexes to each permutation when there are num_permutations
        // permutations. (e.g. 0 is abcd, 1 is abdc, 2 is acbd, 3 is acdb...)
        // But how do we find the index for the current number of permutations?
        // To find the permutation within the part of the sequence we're
        // currently looking at, we need a number between 0 and
        // |current_num_permutations| - 1.
        // (e.g. if we already chose the first letter, there are 3! = 6
        // options left, so we do p % 6. So |current_permutation| would go
        // from 0 to 5 and then reset to 0 again, for all combinations of
        // whichever three letters are remaining, as we loop through the
        // permutations)
        int current_permutation = p % current_num_permutations;

        // Since this is is the total number of permutations starting with
        // this event and including future events, there could be multiple
        // values of current_permutation that will generate the same event
        // in this iteration.
        // (e.g. If we chose 'a' but have b c d to choose from, we choose b when
        // |current_permutation| = 0, 1 and c when |current_permutation| = 2, 3.
        // Note that each letter gets two numbers, which is the next
        // current_num_permutations, 2! for the two letters left.)

        // Branching out from the first event, there are num_permutations
        // permutations, and each value of |p| is associated with one of these
        // permutations. However, once the first event is chosen, there
        // are now |num_events| - 1 events left, so the number of permutations
        // for the rest of the events changes, and will always be equal to
        // the factorial of the events_left.
        // (e.g. There are 3! = 6 permutations that start with 'a', so if we
        // start with 'a' there will be 6 ways to then choose from b c d.)
        // So we now set-up for the next iteration by setting
        // current_num_permutations to the factorial of the next number of
        // events left.
        current_num_permutations /= events_left;

        // To figure out what current event we want to choose, we integer
        // divide the current permutation by the next current_num_permutations.
        // (e.g. If there are 4 letters a b c d and 24 permutations, we divide
        // by 24/4 = 6. Values 0 to 5 when divided by 6 equals 0, so the first
        // 6 permutations start with 'a', and the last 6 will start with 'd'.
        // Note that there are 6 that start with 'a' because there are 6
        // permutations for the next three letters that follow 'a'.)
        int index = current_permutation / current_num_permutations;

        ui::TouchEvent* next_dispatch = queued_events[index];
        ASSERT_TRUE(next_dispatch != NULL);

        // |next_dispatch| has to be put in this container so that its time
        // stamp can be changed to this point in the test, when it is being
        // dispatched..
        ui::EventTestApi test_dispatch(next_dispatch);
        test_dispatch.set_time_stamp(Now());
        generator_->Dispatch(next_dispatch);
        queued_events.erase(queued_events.begin() + index);

        // Keep track of what fingers have been pressed, to release
        // only those fingers at the end, so the check for being in
        // no fingers down can be accurate.
        if (next_dispatch->type() == ui::EventType::kTouchPressed) {
          fingers_pressed[next_dispatch->pointer_details().id] = true;
        } else if (next_dispatch->type() == ui::EventType::kTouchReleased) {
          fingers_pressed[next_dispatch->pointer_details().id] = false;
        }
      }
      ASSERT_EQ(queued_events.size(), 0u);

      // Release fingers recorded as pressed.
      for (int j = 0; j < int(fingers_pressed.size()); j++) {
        if (fingers_pressed[j] == true) {
          generator_->ReleaseTouchId(j);
          fingers_pressed[j] = false;
        }
      }
      AdvanceSimulatedTimePastPotentialTapDelay();
      EXPECT_TRUE(IsInNoFingersDownState());
      ClearCapturedAndGestureEvents();
    }
  }
}

// With the simple swipe gestures, if additional fingers are added and the tap
// timer times out, then the state should change to the wait for one finger
// state.
TEST_F(TouchExplorationTest, GestureAddedFinger) {
  SwitchTouchExplorationMode(true);
  EXPECT_FALSE(IsInTouchToMouseMode());
  EXPECT_FALSE(IsInGestureInProgressState());

  float distance = gesture_detector_config_.touch_slop + 1;
  ui::TouchEvent first_press(
      ui::EventType::kTouchPressed, gfx::Point(100, 200), Now(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  generator_->Dispatch(&first_press);
  simulated_clock_.Advance(base::Milliseconds(10));
  gfx::Point second_location(100 + distance, 200);
  generator_->MoveTouch(second_location);
  EXPECT_TRUE(IsInGestureInProgressState());
  EXPECT_FALSE(IsInTouchToMouseMode());
  const EventList& captured_events = GetCapturedEvents();
  ASSERT_EQ(0U, captured_events.size());

  // Generate a second press, but time out past the gesture period so that
  // gestures are prevented from continuing to go through.
  ui::TouchEvent second_press(
      ui::EventType::kTouchPressed, gfx::Point(20, 21), Now(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 1));
  generator_->Dispatch(&second_press);
  AdvanceSimulatedTimePastTapDelay();
  EXPECT_FALSE(IsInGestureInProgressState());
  EXPECT_FALSE(IsInTouchToMouseMode());
  ASSERT_EQ(0U, captured_events.size());
}

TEST_F(TouchExplorationTest, EnterSlideGestureState) {
  SwitchTouchExplorationMode(true);
  EXPECT_FALSE(IsInTouchToMouseMode());
  EXPECT_FALSE(IsInGestureInProgressState());

  int window_right = BoundsOfRootWindowInDIP().right();
  float distance = gesture_detector_config_.touch_slop + 1;
  ui::TouchEvent first_press(
      ui::EventType::kTouchPressed, gfx::Point(window_right, 1), Now(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  gfx::Point second_location(window_right, 1 + distance / 2);
  gfx::Point third_location(window_right, 1 + distance);
  gfx::Point fourth_location(window_right, 35);

  generator_->Dispatch(&first_press);
  simulated_clock_.Advance(base::Milliseconds(10));

  // Since we haven't moved past slop yet, we should not be in slide gesture.
  generator_->MoveTouch(second_location);
  EXPECT_FALSE(IsInTouchToMouseMode());
  EXPECT_FALSE(IsInGestureInProgressState());
  EXPECT_FALSE(IsInSlideGestureState());
  simulated_clock_.Advance(base::Milliseconds(10));

  // Once we are out of slop, we should be in slide gesture since we are along
  // the edge of the screen.
  generator_->MoveTouch(third_location);
  EXPECT_FALSE(IsInGestureInProgressState());
  EXPECT_TRUE(IsInSlideGestureState());
  EXPECT_FALSE(IsInTouchToMouseMode());

  // Now that we are in slide gesture, we can adjust the volume.
  generator_->MoveTouch(fourth_location);
  const EventList& captured_events = GetCapturedEvents();
  ASSERT_EQ(0U, captured_events.size());

  // Since we are at the right edge of the screen, but the sound timer has not
  // elapsed, there should have been a sound that fired and a volume
  // change.
  size_t num_adjust_sounds = delegate_.NumAdjustSounds();
  ASSERT_EQ(1U, num_adjust_sounds);
  ASSERT_EQ(1U, delegate_.VolumeChanges().size());

  // Exit out of slide gesture once touch is lifted, but not before even if the
  // grace period is over.
  AdvanceSimulatedTimePastPotentialTapDelay();
  ASSERT_EQ(0U, captured_events.size());
  EXPECT_FALSE(IsInTouchToMouseMode());
  EXPECT_FALSE(IsInGestureInProgressState());
  EXPECT_TRUE(IsInSlideGestureState());

  generator_->ReleaseTouch();
  ASSERT_EQ(0U, captured_events.size());
  EXPECT_FALSE(IsInTouchToMouseMode());
  EXPECT_FALSE(IsInGestureInProgressState());
  EXPECT_FALSE(IsInSlideGestureState());
}

// If a press + move occurred outside the boundaries, but within the slop
// boundaries and then moved into the boundaries of an edge, there still should
// not be a slide gesture.
TEST_F(TouchExplorationTest, AvoidEnteringSlideGesture) {
  SwitchTouchExplorationMode(true);

  gfx::Rect window = BoundsOfRootWindowInDIP();
  float distance = gesture_detector_config_.touch_slop + 1;
  ui::TouchEvent first_press(
      ui::EventType::kTouchPressed,
      gfx::Point(window.right() - GetSlopDistanceFromEdge(), 1), Now(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  gfx::Point out_of_slop(window.right() - GetSlopDistanceFromEdge() + distance,
                         1);
  gfx::Point into_boundaries(window.right() - GetMaxDistanceFromEdge() / 2, 1);

  generator_->Dispatch(&first_press);
  simulated_clock_.Advance(base::Milliseconds(10));

  generator_->MoveTouch(out_of_slop);
  EXPECT_FALSE(IsInTouchToMouseMode());
  EXPECT_TRUE(IsInGestureInProgressState());
  EXPECT_FALSE(IsInSlideGestureState());
  simulated_clock_.Advance(base::Milliseconds(10));

  // Since we did not start moving while in the boundaries, we should not be in
  // slide gestures.
  generator_->MoveTouch(into_boundaries);
  EXPECT_TRUE(IsInGestureInProgressState());
  EXPECT_FALSE(IsInSlideGestureState());
  EXPECT_FALSE(IsInTouchToMouseMode());
  const EventList& captured_events = GetCapturedEvents();
  ASSERT_EQ(0U, captured_events.size());

  generator_->ReleaseTouch();
}

// If the slide gesture begins within the boundaries and then moves
// SlopDistanceFromEdge there should still be a sound change. If the finger
// moves into the center screen, there should no longer be a sound change but it
// should still be in slide gesture. If the finger moves back into the edges
// without lifting, it should start changing sound again.
TEST_F(TouchExplorationTest, TestingBoundaries) {
  SwitchTouchExplorationMode(true);

  gfx::Rect window = BoundsOfRootWindowInDIP();
  gfx::Point initial_press(window.right() - GetMaxDistanceFromEdge() / 2, 1);

  gfx::Point center_screen(window.right() / 2, window.bottom() / 2);

  ui::TouchEvent first_press(
      ui::EventType::kTouchPressed, initial_press, Now(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  generator_->Dispatch(&first_press);
  simulated_clock_.Advance(base::Milliseconds(10));
  EXPECT_FALSE(IsInGestureInProgressState());
  EXPECT_FALSE(IsInSlideGestureState());
  EXPECT_FALSE(IsInTouchToMouseMode());

  // Move past the touch slop to begin slide gestures.
  // + slop + 1 to actually leave slop.
  gfx::Point touch_move(
      initial_press.x(),
      initial_press.y() + gesture_detector_config_.touch_slop + 1);
  generator_->MoveTouch(touch_move);
  EXPECT_FALSE(IsInGestureInProgressState());
  EXPECT_TRUE(IsInSlideGestureState());
  EXPECT_FALSE(IsInTouchToMouseMode());
  simulated_clock_.Advance(base::Milliseconds(10));

  // Move the touch into slop boundaries. It should still be in slide gestures
  // and adjust the volume.
  gfx::Point into_slop_boundaries(
      window.right() - GetSlopDistanceFromEdge() / 2, 1);
  generator_->MoveTouch(into_slop_boundaries);
  EXPECT_FALSE(IsInGestureInProgressState());
  EXPECT_TRUE(IsInSlideGestureState());
  EXPECT_FALSE(IsInTouchToMouseMode());

  // The sound is rate limiting so it only activates every 150ms.
  simulated_clock_.Advance(base::Milliseconds(200));

  size_t num_adjust_sounds = delegate_.NumAdjustSounds();
  ASSERT_EQ(1U, num_adjust_sounds);
  ASSERT_EQ(1U, delegate_.VolumeChanges().size());

  // Move the touch into the center of the window. It should still be in slide
  // gestures, but there should not be anymore volume adjustments.
  generator_->MoveTouch(center_screen);
  EXPECT_FALSE(IsInGestureInProgressState());
  EXPECT_TRUE(IsInSlideGestureState());
  EXPECT_FALSE(IsInTouchToMouseMode());

  simulated_clock_.Advance(base::Milliseconds(200));
  num_adjust_sounds = delegate_.NumAdjustSounds();
  ASSERT_EQ(1U, num_adjust_sounds);
  ASSERT_EQ(1U, delegate_.VolumeChanges().size());

  // Move the touch back into slop edge distance and volume should be changing
  // again, one volume change for each new move.
  generator_->MoveTouch(into_slop_boundaries);
  EXPECT_FALSE(IsInGestureInProgressState());
  EXPECT_TRUE(IsInSlideGestureState());
  EXPECT_FALSE(IsInTouchToMouseMode());

  generator_->MoveTouch(
      gfx::Point(into_slop_boundaries.x() + gesture_detector_config_.touch_slop,
                 into_slop_boundaries.y()));
  simulated_clock_.Advance(base::Milliseconds(200));

  num_adjust_sounds = delegate_.NumAdjustSounds();
  ASSERT_EQ(2U, num_adjust_sounds);
  ASSERT_EQ(3U, delegate_.VolumeChanges().size());

  const EventList& captured_events = GetCapturedEvents();
  ASSERT_EQ(0U, captured_events.size());

  generator_->ReleaseTouch();
}

// Even if the gesture starts within bounds, if it has not moved past slop
// within the grace period, it should go to touch exploration.
TEST_F(TouchExplorationTest, InBoundariesTouchExploration) {
  SwitchTouchExplorationMode(true);

  gfx::Rect window = BoundsOfRootWindowInDIP();
  gfx::Point initial_press(window.right() - GetMaxDistanceFromEdge() / 2, 1);
  ui::TouchEvent first_press(
      ui::EventType::kTouchPressed, initial_press, Now(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  generator_->Dispatch(&first_press);
  EXPECT_FALSE(IsInGestureInProgressState());
  EXPECT_FALSE(IsInSlideGestureState());
  EXPECT_FALSE(IsInTouchToMouseMode());

  AdvanceSimulatedTimePastTapDelay();
  EXPECT_FALSE(IsInGestureInProgressState());
  EXPECT_FALSE(IsInSlideGestureState());
  EXPECT_TRUE(IsInTouchToMouseMode());
}

// If two fingers tap the screen at the same time and release before the tap
// timer runs out, a control key event should be sent to silence chromevox.
TEST_F(TouchExplorationTest, TwoFingerTap) {
  SwitchTouchExplorationMode(true);

  generator_->set_current_screen_location(gfx::Point(101, 102));
  generator_->PressTouchId(1);
  EXPECT_FALSE(IsInTwoFingerTapState());

  generator_->PressTouchId(2);
  EXPECT_TRUE(IsInTwoFingerTapState());

  const EventList& captured_events = GetCapturedEvents();
  ASSERT_EQ(0U, captured_events.size());

  generator_->ReleaseTouchId(1);
  EXPECT_TRUE(IsInTwoFingerTapState());
  generator_->ReleaseTouchId(2);

  EXPECT_EQ(0U, captured_events.size());
  ASSERT_EQ(ax::mojom::Gesture::kTap2, delegate_.GetLastGesture());
}

// If the fingers are not released before the tap timer runs out, a control
// keyevent is not sent and the state will no longer be in two finger tap.
TEST_F(TouchExplorationTest, TwoFingerTapAndHold) {
  SwitchTouchExplorationMode(true);

  generator_->PressTouchId(1);
  EXPECT_FALSE(IsInTwoFingerTapState());

  generator_->PressTouchId(2);
  EXPECT_TRUE(IsInTwoFingerTapState());

  const EventList& captured_events = GetCapturedEvents();
  ASSERT_EQ(0U, captured_events.size());

  AdvanceSimulatedTimePastTapDelay();
  // Since the tap delay has elapsed, it should no longer be in two finger tap.
  EXPECT_FALSE(IsInTwoFingerTapState());
}

// The next two tests set up two finger swipes to happen. If one of the fingers
// moves out of slop before the tap timer fires, a two finger tap is not made.
// In this first test, the first finger placed will move out of slop.
TEST_F(TouchExplorationTest, TwoFingerTapAndMoveFirstFinger) {
  SwitchTouchExplorationMode(true);

  // Once one of the fingers leaves slop, it should no longer be in two finger
  // tap.
  ui::TouchEvent first_press_id_1(
      ui::EventType::kTouchPressed, gfx::Point(100, 200), Now(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 1));
  ui::TouchEvent first_press_id_2(
      ui::EventType::kTouchPressed, gfx::Point(110, 200), Now(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 2));

  ui::TouchEvent slop_move_id_1(
      ui::EventType::kTouchMoved,
      gfx::Point(100 + gesture_detector_config_.touch_slop, 200), Now(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 1));
  ui::TouchEvent slop_move_id_2(
      ui::EventType::kTouchMoved,
      gfx::Point(110 + gesture_detector_config_.touch_slop, 200), Now(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 2));

  ui::TouchEvent out_slop_id_1(
      ui::EventType::kTouchMoved,
      gfx::Point(100 + gesture_detector_config_.touch_slop + 1, 200), Now(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 1));

  // Dispatch the inital presses.
  generator_->Dispatch(&first_press_id_1);
  EXPECT_FALSE(IsInTwoFingerTapState());
  generator_->Dispatch(&first_press_id_2);
  EXPECT_TRUE(IsInTwoFingerTapState());

  const EventList& captured_events = GetCapturedEvents();
  ASSERT_EQ(0U, captured_events.size());

  // The presses have not moved out of slop yet so it should still be in
  // TwoFingerTap.
  generator_->Dispatch(&slop_move_id_1);
  EXPECT_TRUE(IsInTwoFingerTapState());
  generator_->Dispatch(&slop_move_id_2);
  EXPECT_TRUE(IsInTwoFingerTapState());

  // Once one of the fingers moves out of slop, we are no longer in
  // TwoFingerTap.
  generator_->Dispatch(&out_slop_id_1);
  EXPECT_FALSE(IsInTwoFingerTapState());
}

// Similar test to the previous test except the second finger placed will be the
// one to move out of slop.
TEST_F(TouchExplorationTest, TwoFingerTapAndMoveSecondFinger) {
  SwitchTouchExplorationMode(true);

  // Once one of the fingers leaves slop, it should no longer be in two finger
  // tap.
  ui::TouchEvent first_press_id_1(
      ui::EventType::kTouchPressed, gfx::Point(100, 200), Now(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 1));
  ui::TouchEvent first_press_id_2(
      ui::EventType::kTouchPressed, gfx::Point(110, 200), Now(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 2));

  ui::TouchEvent out_slop_id_2(
      ui::EventType::kTouchMoved,
      gfx::Point(100 + gesture_detector_config_.touch_slop + 1, 200), Now(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 1));

  generator_->Dispatch(&first_press_id_1);
  EXPECT_FALSE(IsInTwoFingerTapState());

  generator_->Dispatch(&first_press_id_2);
  EXPECT_TRUE(IsInTwoFingerTapState());

  const EventList& captured_events = GetCapturedEvents();
  ASSERT_EQ(0U, captured_events.size());

  generator_->Dispatch(&out_slop_id_2);
  EXPECT_FALSE(IsInTwoFingerTapState());
}

TEST_F(TouchExplorationTest, ExclusionArea) {
  SwitchTouchExplorationMode(true);

  gfx::Rect window = BoundsOfRootWindowInDIP();
  gfx::Rect exclude = window;
  exclude.Inset(gfx::Insets::TLBR(0, 0, 30, 0));
  SetExcludeBounds(exclude);

  gfx::Point in_pt = exclude.CenterPoint();
  gfx::Point in_mv_pt(in_pt.x(), (in_pt.y() + exclude.bottom()) / 2);
  gfx::Point out_pt(in_pt.x(), exclude.bottom() + 20);
  gfx::Point out_mv_pt(in_pt.x(), exclude.bottom() + 10);

  // Motion starting in exclusion bounds is passed-through unchanged.
  {
    generator_->set_current_screen_location(in_pt);
    generator_->PressTouchId(0);
    AdvanceSimulatedTimePastPotentialTapDelay();
    generator_->MoveTouchId(out_mv_pt, 0);
    generator_->ReleaseTouchId(0);
    EXPECT_TRUE(IsInNoFingersDownState());
    const EventList& captured_events = GetCapturedEvents();
    ASSERT_EQ(3U, captured_events.size());
    EXPECT_EQ(ui::EventType::kTouchPressed, captured_events[0]->type());
    EXPECT_EQ(ui::EventType::kTouchMoved, captured_events[1]->type());
    EXPECT_EQ(ui::EventType::kTouchReleased, captured_events[2]->type());
    ClearCapturedAndGestureEvents();
  }

  // Complete motion outside exclusion is rewritten.
  {
    generator_->set_current_screen_location(out_pt);
    generator_->PressTouchId(0);
    AdvanceSimulatedTimePastTapDelay();
    generator_->MoveTouchId(out_mv_pt, 0);
    generator_->ReleaseTouchId(0);
    AdvanceSimulatedTimePastTapDelay();
    EXPECT_TRUE(IsInNoFingersDownState());
    const EventList& captured_events = GetCapturedEvents();
    ASSERT_TRUE(captured_events.empty());
    ASSERT_EQ(3U, GetTouchExplorePoints().size());
    ClearCapturedAndGestureEvents();
  }

  // For a motion starting outside: outside events are rewritten, inside
  // events are discarded unless they end the motion.
  {
    // finger 0 down outside, moves inside.
    generator_->set_current_screen_location(out_pt);
    generator_->PressTouchId(0);
    AdvanceSimulatedTimePastTapDelay();
    generator_->MoveTouchId(out_mv_pt, 0);
    generator_->MoveTouchId(in_mv_pt, 0);
    ASSERT_TRUE(GetCapturedEvents().empty());
    ASSERT_EQ(2U, GetTouchExplorePoints().size());
    ClearCapturedAndGestureEvents();

    // finger 1 down inside, moves outside
    generator_->set_current_screen_location(in_pt);
    generator_->PressTouchId(1);
    generator_->MoveTouchId(out_mv_pt, 1);
    generator_->ReleaseTouchId(1);
    ASSERT_TRUE(GetCapturedEvents().empty());
    ASSERT_TRUE(GetTouchExplorePoints().empty());
    EXPECT_FALSE(IsInNoFingersDownState());

    generator_->ReleaseTouchId(0);
    AdvanceSimulatedTimePastTapDelay();
    EXPECT_TRUE(IsInNoFingersDownState());

    ASSERT_TRUE(GetCapturedEvents().empty());
    ASSERT_EQ(1U, GetTouchExplorePoints().size());
  }
}

TEST_F(TouchExplorationTest, SingleTapInLiftActivationArea) {
  SwitchTouchExplorationMode(true);

  gfx::Rect lift_activation = BoundsOfRootWindowInDIP();
  lift_activation.Inset(gfx::Insets::TLBR(0, 0, 30, 0));
  SetLiftActivationBounds(lift_activation);

  // Tap at one location, and get tap and mouse move events.
  gfx::Point tap_location = lift_activation.CenterPoint();

  // The user has to have previously selected something.
  SetTouchAccessibilityAnchorPoint(tap_location);

  generator_->set_current_screen_location(tap_location);
  generator_->PressTouchId(1);
  generator_->ReleaseTouchId(1);
  AdvanceSimulatedTimePastTapDelay();

  const EventList& captured_events = GetCapturedEvents();
  ASSERT_EQ(2U, captured_events.size());
  EXPECT_EQ(ui::EventType::kTouchPressed, captured_events[0]->type());
  EXPECT_EQ(ui::EventType::kTouchReleased, captured_events[1]->type());
  ASSERT_EQ(1U, GetTouchExplorePoints().size());
  ClearCapturedAndGestureEvents();

  gfx::Point out_tap_location(tap_location.x(), lift_activation.bottom() + 20);
  SetTouchAccessibilityAnchorPoint(out_tap_location);
  generator_->set_current_screen_location(out_tap_location);
  generator_->PressTouchId(1);
  generator_->ReleaseTouchId(1);
  AdvanceSimulatedTimePastTapDelay();

  const EventList& out_captured_events = GetCapturedEvents();
  ASSERT_TRUE(out_captured_events.empty());
  ASSERT_EQ(1U, GetTouchExplorePoints().size());
}

TEST_F(TouchExplorationTest, TouchExploreLiftInLiftActivationArea) {
  SwitchTouchExplorationMode(true);

  gfx::Rect lift_activation = BoundsOfRootWindowInDIP();
  lift_activation.Inset(gfx::Insets::TLBR(0, 0, 30, 0));
  SetLiftActivationBounds(lift_activation);

  // Explore at one location, and get tap and touch explore events.
  gfx::Point tap_location = lift_activation.CenterPoint();
  EnterTouchExplorationModeAtLocation(tap_location);
  ClearCapturedAndGestureEvents();
  ASSERT_EQ(0U, delegate_.NumTouchTypeSounds());

  // A touch release should trigger a tap.
  ui::TouchEvent touch_explore_release(
      ui::EventType::kTouchReleased, tap_location, Now(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  generator_->Dispatch(&touch_explore_release);
  AdvanceSimulatedTimePastTapDelay();

  const EventList& captured_events = GetCapturedEvents();
  ASSERT_EQ(2U, captured_events.size());
  EXPECT_EQ(ui::EventType::kTouchPressed, captured_events[0]->type());
  EXPECT_EQ(ui::EventType::kTouchReleased, captured_events[1]->type());
  ASSERT_EQ(1U, delegate_.NumTouchTypeSounds());
  ASSERT_EQ(1U, GetTouchExplorePoints().size());
  ClearCapturedAndGestureEvents();
  delegate_.ResetCountersToZero();

  // Touch explore inside the activation bounds, but lift outside.
  gfx::Point out_tap_location(tap_location.x(), lift_activation.bottom() + 20);
  SetTouchAccessibilityAnchorPoint(out_tap_location);
  EnterTouchExplorationModeAtLocation(tap_location);
  ClearCapturedAndGestureEvents();
  ui::TouchEvent out_touch_explore_release(
      ui::EventType::kTouchReleased, out_tap_location, Now(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  generator_->Dispatch(&out_touch_explore_release);
  AdvanceSimulatedTimePastTapDelay();

  const EventList& out_captured_events = GetCapturedEvents();
  ASSERT_TRUE(out_captured_events.empty());
  ASSERT_EQ(1U, GetTouchExplorePoints().size());
  ASSERT_EQ(0U, delegate_.NumTouchTypeSounds());
}

// Ensure that any touch release events received after
// TouchExplorationController starts up are canceled, if we haven't
// seen any touch press events yet. http://crbug.com/751348
TEST_F(TouchExplorationTest, AlreadyHeldFingersGetCanceled) {
  generator_->PressTouch();
  SwitchTouchExplorationMode(true);
  generator_->ReleaseTouch();

  std::vector<ui::LocatedEvent*> events =
      GetCapturedLocatedEventsOfType(ui::EventType::kTouchCancelled);
  ASSERT_EQ(1U, events.size());
}

// Ensure 3 or 4 finger tap gets recognized correctly.
TEST_F(TouchExplorationTest, ThreeOrFourFingerTap) {
  SwitchTouchExplorationMode(true);

  ui::TouchEvent press_id_1(
      ui::EventType::kTouchPressed, gfx::Point(100, 200), Now(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 1));
  ui::TouchEvent release_id_1(
      ui::EventType::kTouchReleased, gfx::Point(100, 200), Now(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 1));
  ui::TouchEvent press_id_2(
      ui::EventType::kTouchPressed, gfx::Point(110, 200), Now(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 2));
  ui::TouchEvent release_id_2(
      ui::EventType::kTouchReleased, gfx::Point(110, 200), Now(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 2));
  ui::TouchEvent press_id_3(
      ui::EventType::kTouchPressed, gfx::Point(120, 200), Now(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 3));
  ui::TouchEvent release_id_3(
      ui::EventType::kTouchReleased, gfx::Point(120, 200), Now(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 3));
  ui::TouchEvent press_id_4(
      ui::EventType::kTouchPressed, gfx::Point(130, 200), Now(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 4));
  ui::TouchEvent release_id_4(
      ui::EventType::kTouchReleased, gfx::Point(120, 200), Now(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 4));

  // Three fingers down.
  generator_->Dispatch(&press_id_1);
  EXPECT_FALSE(IsInTwoFingerTapState());
  generator_->Dispatch(&press_id_2);
  EXPECT_TRUE(IsInTwoFingerTapState());
  generator_->Dispatch(&press_id_3);
  EXPECT_TRUE(IsInGestureInProgressState());

  // Three fingers up.
  generator_->Dispatch(&release_id_1);
  EXPECT_TRUE(IsInGestureInProgressState());
  generator_->Dispatch(&release_id_2);
  EXPECT_TRUE(IsInGestureInProgressState());

  ASSERT_EQ(ax::mojom::Gesture::kNone, delegate_.GetLastGesture());

  generator_->Dispatch(&release_id_3);
  EXPECT_TRUE(IsInNoFingersDownState());

  ASSERT_EQ(ax::mojom::Gesture::kTap3, delegate_.GetLastGesture());
  delegate_.ResetLastGesture();

  // Four fingers down.
  generator_->Dispatch(&press_id_1);
  EXPECT_FALSE(IsInTwoFingerTapState());
  generator_->Dispatch(&press_id_2);
  EXPECT_TRUE(IsInTwoFingerTapState());
  generator_->Dispatch(&press_id_3);
  EXPECT_TRUE(IsInGestureInProgressState());
  generator_->Dispatch(&press_id_4);
  EXPECT_TRUE(IsInGestureInProgressState());

  // Four fingers up.
  generator_->Dispatch(&release_id_1);
  EXPECT_TRUE(IsInGestureInProgressState());
  generator_->Dispatch(&release_id_2);
  EXPECT_TRUE(IsInGestureInProgressState());
  generator_->Dispatch(&release_id_3);
  EXPECT_TRUE(IsInGestureInProgressState());

  ASSERT_EQ(ax::mojom::Gesture::kNone, delegate_.GetLastGesture());

  generator_->Dispatch(&release_id_4);
  EXPECT_TRUE(IsInNoFingersDownState());

  ASSERT_EQ(ax::mojom::Gesture::kTap4, delegate_.GetLastGesture());
}

// Triggers right-click when anchor point remains in position after delay.
TEST_F(TouchExplorationTest, TriggersRightClickAfterDelay) {
  SwitchTouchExplorationMode(true);

  gfx::Rect lift_activation = BoundsOfRootWindowInDIP();
  SetLiftActivationBounds(lift_activation);

  // Explore at one location.
  gfx::Point tap_location = lift_activation.CenterPoint();
  EnterTouchExplorationModeAtLocation(tap_location);
  ClearCapturedAndGestureEvents();
  ASSERT_EQ(0U, delegate_.NumTouchTypeSounds());

  // Stay in the same anchor point after delay.
  AdvanceSimulatedTimePastLongPressDelay();

  // Any event should be rewritten as a right-mouse click.
  generator_->set_current_screen_location(tap_location);
  generator_->MoveTouch(tap_location);

  const EventList& captured_events = GetCapturedEvents();
  ASSERT_EQ(2U, captured_events.size());
  EXPECT_EQ(ui::EventType::kMousePressed, captured_events[0]->type());
  EXPECT_EQ(ui::EventType::kMouseReleased, captured_events[1]->type());
  // We immediately go back to touch exploration so there will be a touch
  // explore event from the touch exploration.
  ASSERT_EQ(1U, GetTouchExplorePoints().size());
  EXPECT_EQ(1U, delegate_.NumLongPressRightClickSounds());
  ClearCapturedAndGestureEvents();
  delegate_.ResetCountersToZero();
}

// Does not trigger right-click when anchor point is not in lift activation
// bounds.
TEST_F(TouchExplorationTest,
       DoesNotTriggerRightClickInNonLiftActivationBounds) {
  SwitchTouchExplorationMode(true);

  // Explore at one location.
  gfx::Point tap_location = BoundsOfRootWindowInDIP().CenterPoint();
  EnterTouchExplorationModeAtLocation(tap_location);
  ClearCapturedAndGestureEvents();
  ASSERT_EQ(0U, delegate_.NumTouchTypeSounds());

  // Stay in the same anchor point after delay.
  AdvanceSimulatedTimePastLongPressDelay();

  generator_->set_current_screen_location(tap_location);
  generator_->MoveTouch(tap_location);

  const EventList& captured_events = GetCapturedEvents();
  ASSERT_TRUE(captured_events.empty());
  ASSERT_EQ(1U, GetTouchExplorePoints().size());
  EXPECT_EQ(0U, delegate_.NumLongPressRightClickSounds());
  ClearCapturedAndGestureEvents();
  delegate_.ResetCountersToZero();
}

}  // namespace ash
