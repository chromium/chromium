// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/hover_text_event_handler.h"

#include <memory>
#include <optional>
#include <string>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/test_accessibility_controller_client.h"
#include "ash/public/cpp/accessibility_controller_enums.h"
#include "ash/shell.h"
#include "ash/system/accessibility/hover_text_bubble_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/display/display.h"
#include "ui/display/display_layout.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_handler.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Post-target handler on the primary root window that records how many raw
// touch events and gesture events escaped pre-target interception.
class TouchAndGestureRecorder : public ui::EventHandler {
 public:
  TouchAndGestureRecorder() {
    Shell::GetPrimaryRootWindow()->AddPostTargetHandler(this);
  }
  TouchAndGestureRecorder(const TouchAndGestureRecorder&) = delete;
  TouchAndGestureRecorder& operator=(const TouchAndGestureRecorder&) = delete;
  ~TouchAndGestureRecorder() override {
    Shell::GetPrimaryRootWindow()->RemovePostTargetHandler(this);
  }

  void OnTouchEvent(ui::TouchEvent* event) override { ++touch_count_; }
  void OnGestureEvent(ui::GestureEvent* event) override { ++gesture_count_; }

  void Reset() {
    touch_count_ = 0;
    gesture_count_ = 0;
  }

  void ExpectNoEvents() const {
    EXPECT_EQ(0, touch_count_);
    EXPECT_EQ(0, gesture_count_);
  }

  int touch_count() const { return touch_count_; }
  int gesture_count() const { return gesture_count_; }

 private:
  int touch_count_ = 0;
  int gesture_count_ = 0;
};

}  // namespace

class HoverTextEventHandlerTest : public AshTestBase {
 public:
  HoverTextEventHandlerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  HoverTextEventHandlerTest(const HoverTextEventHandlerTest&) = delete;
  HoverTextEventHandlerTest& operator=(const HoverTextEventHandlerTest&) =
      delete;
  ~HoverTextEventHandlerTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        ::features::kAccessibilityHoverText);
    AshTestBase::SetUp();
    client_.emplace();
    controller()->hover_text().SetEnabled(true);
  }

  void TearDown() override {
    client_.reset();
    AshTestBase::TearDown();
  }

  AccessibilityController* controller() {
    return Shell::Get()->accessibility_controller();
  }

  HoverTextEventHandler* GetEventHandler() {
    return controller()->GetHoverTextEventHandlerForTest();
  }

  bool IsHoverTextActive() { return controller()->IsHoverTextActive(); }

  void OnHoverTextTrayToggled() { controller()->ToggleHoverTextActive(); }

  void FastForwardPastRateLimit() {
    task_environment()->FastForwardBy(base::Milliseconds(35));
  }

  void ShowHoverTextBubble(const std::u16string& text = u"Downloads") {
    HoverTextBubbleProps props;
    props.visible = true;
    props.text = text;
    props.anchor_rect = gfx::Rect(100, 100, 80, 32);
    controller()->UpdateHoverTextBubble(props);
  }

  void HideHoverTextBubble() {
    HoverTextBubbleProps props;
    props.visible = false;
    controller()->UpdateHoverTextBubble(props);
  }

  ui::KeyEvent SendKeyEvent(ui::EventType type,
                            ui::KeyboardCode key_code,
                            int flags = ui::EF_NONE) {
    ui::KeyEvent event(type, key_code, flags);
    GetEventHandler()->OnKeyEvent(&event);
    return event;
  }

  void ExpectLastPoint(int expected_count,
                       const gfx::Point& expected_point,
                       HoverTextPointerType expected_type) {
    EXPECT_EQ(expected_count, client_->hover_text_point_update_count());
    EXPECT_EQ(expected_point, client_->last_hover_text_point());
    EXPECT_EQ(expected_type, client_->last_hover_text_pointer_type());
  }

  void ExpectLastKeyEvent(int expected_count,
                          bool expected_key_held,
                          bool expected_stop_speech) {
    EXPECT_EQ(expected_count, client_->hover_text_key_event_count());
    EXPECT_EQ(expected_key_held,
              client_->last_hover_text_activation_key_held());
    EXPECT_EQ(expected_stop_speech, client_->last_hover_text_stop_speech());
  }

 protected:
  std::optional<TestAccessibilityControllerClient> client_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(HoverTextEventHandlerTest, MouseEventsDispatchRateLimitedPointUpdates) {
  ASSERT_TRUE(GetEventHandler());

  // Moving the mouse while Hover Text is inactive (and holding Search down at
  // a static cursor before moving) must not dispatch any point updates.
  GetEventGenerator()->MoveMouseTo(gfx::Point(100, 200));
  EXPECT_EQ(0, client_->hover_text_point_update_count());
  GetEventGenerator()->PressKey(ui::VKEY_LWIN, ui::EF_NONE);
  EXPECT_TRUE(IsHoverTextActive());
  EXPECT_EQ(0, client_->hover_text_point_update_count());

  // Moving the mouse *after* Search is held down dispatches the point update.
  GetEventGenerator()->MoveMouseTo(gfx::Point(120, 240));
  ExpectLastPoint(1, gfx::Point(120, 240), HoverTextPointerType::kMouse);

  // Sub-interval mouse moves are rate-limited and reset the delayed trailing
  // dispatch timer so the final resting mouse position at the end of a fast
  // movement is sent once the 30ms interval elapses.
  task_environment()->FastForwardBy(base::Milliseconds(10));
  GetEventGenerator()->MoveMouseTo(gfx::Point(130, 250));
  EXPECT_EQ(1, client_->hover_text_point_update_count());

  task_environment()->FastForwardBy(base::Milliseconds(10));
  GetEventGenerator()->MoveMouseTo(gfx::Point(145, 255));
  EXPECT_EQ(1, client_->hover_text_point_update_count());

  task_environment()->FastForwardBy(base::Milliseconds(20));
  EXPECT_EQ(1, client_->hover_text_point_update_count());

  task_environment()->FastForwardBy(base::Milliseconds(10));
  ExpectLastPoint(2, gfx::Point(145, 255), HoverTextPointerType::kMouse);

  // Advance clock past the rate-limit interval and verify the next move
  // dispatches immediately.
  FastForwardPastRateLimit();
  GetEventGenerator()->MoveMouseTo(gfx::Point(180, 260));
  ExpectLastPoint(3, gfx::Point(180, 260), HoverTextPointerType::kMouse);

  // Deactivating and re-activating within the 30ms rate-limit interval (via
  // Search release/press or shelf tray toggle) resets
  // `last_point_dispatch_time_` so the first mouse move after re-activation
  // dispatches immediately.
  GetEventGenerator()->ReleaseKey(ui::VKEY_LWIN, ui::EF_NONE);
  GetEventGenerator()->PressKey(ui::VKEY_LWIN, ui::EF_NONE);
  GetEventGenerator()->MoveMouseTo(gfx::Point(190, 270));
  ExpectLastPoint(4, gfx::Point(190, 270), HoverTextPointerType::kMouse);
  GetEventGenerator()->ReleaseKey(ui::VKEY_LWIN, ui::EF_NONE);

  OnHoverTextTrayToggled();
  GetEventGenerator()->MoveMouseTo(gfx::Point(200, 280));
  ExpectLastPoint(5, gfx::Point(200, 280), HoverTextPointerType::kMouse);
  OnHoverTextTrayToggled();

  // Synthetic mouse events generated from touch (`ui::EF_FROM_TOUCH`) are
  // ignored so they never overwrite or dismiss a touch-triggered bubble.
  GetEventGenerator()->PressKey(ui::VKEY_LWIN, ui::EF_NONE);
  FastForwardPastRateLimit();
  ui::MouseEvent synthetic_touch_mouse(
      ui::EventType::kMouseMoved, gfx::Point(220, 300), gfx::Point(220, 300),
      base::TimeTicks::Now(), ui::EF_FROM_TOUCH, ui::EF_NONE);
  GetEventHandler()->OnMouseEvent(&synthetic_touch_mouse);
  EXPECT_EQ(5, client_->hover_text_point_update_count());

  GetEventGenerator()->ReleaseKey(ui::VKEY_LWIN, ui::EF_NONE);
}

// Verifies that mouse and touch points on every display, including external
// displays whose origin is not (0, 0), are dispatched in screen coordinates
// rather than in the coordinates of the display's root window.
TEST_F(HoverTextEventHandlerTest, DispatchesScreenPointsOnEveryDisplay) {
  UpdateDisplay("800x600,1024x768");
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressKey(ui::VKEY_LWIN, ui::EF_NONE);

  // Each placement gives the external display a different non-zero origin:
  // (800, 0) to the right of the primary display, and (-1024, 100) to its left
  // and offset downward, so both axes and negative coordinates are covered.
  const struct {
    display::DisplayPlacement::Position position;
    int offset;
    gfx::Point expected_external_origin;
    gfx::Point primary_point;
  } kPlacements[] = {
      {display::DisplayPlacement::RIGHT, 0, gfx::Point(800, 0),
       gfx::Point(400, 300)},
      {display::DisplayPlacement::LEFT, 100, gfx::Point(-1024, 100),
       gfx::Point(500, 350)},
  };

  for (const auto& placement : kPlacements) {
    display_manager()->SetLayoutForCurrentDisplays(
        display::test::CreateDisplayLayout(
            display_manager(), placement.position, placement.offset));
    const gfx::Rect external_bounds = GetSecondaryDisplay().bounds();
    ASSERT_EQ(placement.expected_external_origin, external_bounds.origin());

    const gfx::Point external_point =
        external_bounds.origin() + gfx::Vector2d(300, 200);
    for (const gfx::Point& point : {placement.primary_point, external_point}) {
      SCOPED_TRACE(point.ToString());

      // Display changes can post synthesized mouse moves that rate-limit this
      // move; advancing past the 30ms interval flushes the trailing point.
      generator->MoveMouseTo(point);
      FastForwardPastRateLimit();
      EXPECT_EQ(point, client_->last_hover_text_point());
      EXPECT_EQ(HoverTextPointerType::kMouse,
                client_->last_hover_text_pointer_type());

      generator->PressTouch(point);
      EXPECT_EQ(point, client_->last_hover_text_point());
      EXPECT_EQ(HoverTextPointerType::kTouchPress,
                client_->last_hover_text_pointer_type());
      generator->ReleaseTouch();
      EXPECT_EQ(point, client_->last_hover_text_point());
      EXPECT_EQ(HoverTextPointerType::kTouchRelease,
                client_->last_hover_text_pointer_type());
    }
  }

  generator->ReleaseKey(ui::VKEY_LWIN, ui::EF_NONE);
}

TEST_F(HoverTextEventHandlerTest,
       TouchExplorationConsumesEventsAndDispatchesPointsWhenActive) {
  TouchAndGestureRecorder recorder;
  ui::test::EventGenerator* generator = GetEventGenerator();
  OnHoverTextTrayToggled();
  ASSERT_TRUE(GetEventHandler()->is_tray_toggled_active_for_testing());

  generator->PressTouchId(1, gfx::Point(310, 410));
  recorder.ExpectNoEvents();
  ExpectLastPoint(1, gfx::Point(310, 410), HoverTextPointerType::kTouchPress);

  // A move within the touch slop (5 DIP in Ash tests, see
  // `AshTestHelper::SetUp`) synthesizes no scroll gesture, so no hover point
  // is sent until the finger moves farther than that from where it pressed.
  FastForwardPastRateLimit();
  generator->MoveTouchId(gfx::Point(312, 412), 1);
  recorder.ExpectNoEvents();
  EXPECT_EQ(1, client_->hover_text_point_update_count());

  FastForwardPastRateLimit();
  generator->MoveTouchId(gfx::Point(325, 430), 1);
  recorder.ExpectNoEvents();
  ExpectLastPoint(2, gfx::Point(325, 430), HoverTextPointerType::kTouchPress);

  generator->ReleaseTouchId(1);
  recorder.ExpectNoEvents();
  ExpectLastPoint(3, gfx::Point(325, 430), HoverTextPointerType::kTouchRelease);

  generator->PressTouchId(1, gfx::Point(325, 430));
  recorder.ExpectNoEvents();
  ExpectLastPoint(4, gfx::Point(325, 430), HoverTextPointerType::kTouchPress);

  generator->CancelTouchId(1);
  recorder.ExpectNoEvents();
  ExpectLastPoint(5, gfx::Point(325, 430), HoverTextPointerType::kTouchRelease);

  // Toggling the tray item off pauses touch forwarding and passes touch events
  // through unhandled.
  OnHoverTextTrayToggled();
  EXPECT_FALSE(IsHoverTextActive());
  generator->PressTouchId(1, gfx::Point(310, 410));
  generator->ReleaseTouchId(1);
  EXPECT_EQ(2, recorder.touch_count());
  EXPECT_GT(recorder.gesture_count(), 0);
  EXPECT_EQ(5, client_->hover_text_point_update_count());
}

TEST_F(HoverTextEventHandlerTest,
       TracksSingleTouchPointerAndCancelsTouchesStartedBeforeActivation) {
  TouchAndGestureRecorder recorder;
  ui::test::EventGenerator* generator = GetEventGenerator();
  ASSERT_TRUE(GetEventHandler());

  // 1. A touch sequence whose `kTouchPressed` occurred before Hover Text became
  // active is cancelled via `CancelActiveTouchesExcept(nullptr)` when Search is
  // pressed mid-touch, so the window that received the press receives a
  // synthetic `kTouchCancelled` and subsequent moves/release are ignored
  // without dispatching Hover Text points.
  generator->PressTouchId(1, gfx::Point(100, 100));
  EXPECT_EQ(1, recorder.touch_count());
  EXPECT_EQ(0, client_->hover_text_point_update_count());

  generator->PressKey(ui::VKEY_LWIN, ui::EF_NONE);
  ASSERT_TRUE(IsHoverTextActive());
  EXPECT_EQ(2, recorder.touch_count());
  EXPECT_EQ(0, client_->hover_text_point_update_count());
  recorder.Reset();

  generator->MoveTouchId(gfx::Point(120, 120), 1);
  generator->ReleaseTouchId(1);
  recorder.ExpectNoEvents();
  EXPECT_EQ(0, client_->hover_text_point_update_count());

  // 2. Two-finger touch while active: the first finger (`pointer_id = 1`) is
  // tracked and dispatched; a second finger (`pointer_id = 2`) pressed, moved,
  // and released while the first finger is still down is consumed without
  // dispatching a fresh `kTouchPress`, alternating move positions, or emitting
  // an early `kTouchRelease`.
  generator->PressTouchId(1, gfx::Point(200, 200));
  recorder.ExpectNoEvents();
  ExpectLastPoint(1, gfx::Point(200, 200), HoverTextPointerType::kTouchPress);

  generator->PressTouchId(2, gfx::Point(400, 400));
  FastForwardPastRateLimit();
  generator->MoveTouchId(gfx::Point(450, 450), 2);
  generator->ReleaseTouchId(2);
  recorder.ExpectNoEvents();
  ExpectLastPoint(1, gfx::Point(200, 200), HoverTextPointerType::kTouchPress);

  // Moving and releasing the tracked first finger (`pointer_id = 1`) continues
  // to dispatch normally, and a subsequent touch with `pointer_id = 2` is
  // tracked once `pointer_id = 1` is released.
  generator->MoveTouchId(gfx::Point(220, 220), 1);
  recorder.ExpectNoEvents();
  ExpectLastPoint(2, gfx::Point(220, 220), HoverTextPointerType::kTouchPress);

  generator->ReleaseTouchId(1);
  recorder.ExpectNoEvents();
  ExpectLastPoint(3, gfx::Point(220, 220), HoverTextPointerType::kTouchRelease);

  generator->PressTouchId(2, gfx::Point(500, 500));
  recorder.ExpectNoEvents();
  ExpectLastPoint(4, gfx::Point(500, 500), HoverTextPointerType::kTouchPress);

  // 3. If a secondary finger (`pointer_id = 3`) is pressed while the tracked
  // finger (`pointer_id = 2`) is down, and the tracked finger lifts first,
  // subsequent moves and release of `pointer_id = 3` are still consumed so a
  // window that never saw its press does not receive its moves or release.
  generator->PressTouchId(3, gfx::Point(300, 300));
  recorder.ExpectNoEvents();
  EXPECT_EQ(4, client_->hover_text_point_update_count());

  generator->set_current_screen_location(gfx::Point(500, 500));
  generator->ReleaseTouchId(2);
  recorder.ExpectNoEvents();
  ExpectLastPoint(5, gfx::Point(500, 500), HoverTextPointerType::kTouchRelease);

  generator->MoveTouchId(gfx::Point(310, 310), 3);
  generator->ReleaseTouchId(3);
  recorder.ExpectNoEvents();
  EXPECT_EQ(5, client_->hover_text_point_update_count());

  // 4. If Hover Text deactivates mid-touch (for example, Search is released
  // before lifting the finger), the finger whose press was consumed continues
  // to have its remaining moves and release consumed without dispatching
  // points, so the underlying window never receives an orphaned move or
  // release.
  generator->PressTouchId(4, gfx::Point(350, 350));
  recorder.ExpectNoEvents();
  ExpectLastPoint(6, gfx::Point(350, 350), HoverTextPointerType::kTouchPress);

  generator->ReleaseKey(ui::VKEY_LWIN, ui::EF_NONE);
  ASSERT_FALSE(IsHoverTextActive());

  FastForwardPastRateLimit();
  generator->MoveTouchId(gfx::Point(360, 360), 4);
  generator->ReleaseTouchId(4);
  recorder.ExpectNoEvents();
  EXPECT_EQ(6, client_->hover_text_point_update_count());

  // Once `pointer_id = 4` has been released, a subsequent inactive touch press
  // with the same pointer ID passes through unhandled.
  generator->PressTouchId(4, gfx::Point(365, 365));
  generator->ReleaseTouchId(4);
  EXPECT_EQ(2, recorder.touch_count());
  EXPECT_GT(recorder.gesture_count(), 0);
  EXPECT_EQ(6, client_->hover_text_point_update_count());
}

// Verifies that touch moves share the mouse rate limit, that touch presses and
// releases are sent at once and a release first sends any touch move still
// waiting (but not a waiting mouse point), that a mouse move right after a
// touch is rate-limited by the same timer, and that turning Hover Text off
// drops a waiting touch move.
TEST_F(HoverTextEventHandlerTest,
       TouchMovesShareRateLimitAndReleaseFlushesPendingMove) {
  OnHoverTextTrayToggled();
  ASSERT_TRUE(IsHoverTextActive());
  ui::test::EventGenerator* generator = GetEventGenerator();

  generator->PressTouch(gfx::Point(100, 100));
  ExpectLastPoint(1, gfx::Point(100, 100), HoverTextPointerType::kTouchPress);

  // Touch moves within 30ms of the last dispatch wait, and only the most
  // recent one is sent when the interval ends.
  task_environment()->FastForwardBy(base::Milliseconds(10));
  generator->MoveTouch(gfx::Point(110, 110));
  task_environment()->FastForwardBy(base::Milliseconds(10));
  generator->MoveTouch(gfx::Point(120, 120));
  EXPECT_EQ(1, client_->hover_text_point_update_count());
  task_environment()->FastForwardBy(base::Milliseconds(30));
  ExpectLastPoint(2, gfx::Point(120, 120), HoverTextPointerType::kTouchPress);

  // Lifting the finger first sends the waiting move, so the extension sees the
  // final finger position, then sends the release: the count rises by two.
  // Nothing follows later.
  task_environment()->FastForwardBy(base::Milliseconds(5));
  generator->MoveTouch(gfx::Point(130, 130));
  EXPECT_EQ(2, client_->hover_text_point_update_count());
  generator->ReleaseTouch();
  ExpectLastPoint(4, gfx::Point(130, 130), HoverTextPointerType::kTouchRelease);
  task_environment()->FastForwardBy(base::Milliseconds(50));
  ExpectLastPoint(4, gfx::Point(130, 130), HoverTextPointerType::kTouchRelease);

  // A mouse move right after a touch waits for the same interval.
  generator->PressTouch(gfx::Point(300, 300));
  generator->ReleaseTouch();
  EXPECT_EQ(6, client_->hover_text_point_update_count());
  generator->MoveMouseTo(gfx::Point(210, 210));
  EXPECT_EQ(6, client_->hover_text_point_update_count());
  task_environment()->FastForwardBy(base::Milliseconds(30));
  ExpectLastPoint(7, gfx::Point(210, 210), HoverTextPointerType::kMouse);

  // A waiting mouse point is not sent ahead of a touch release; only touch
  // moves are sent early. Start the touch press first so `touch_id_` is
  // tracked, then queue a throttled mouse move before lifting the finger.
  task_environment()->FastForwardBy(base::Milliseconds(50));
  generator->PressTouch(gfx::Point(500, 500));
  EXPECT_EQ(8, client_->hover_text_point_update_count());
  task_environment()->FastForwardBy(base::Milliseconds(50));
  generator->MoveMouseTo(gfx::Point(220, 220));
  EXPECT_EQ(9, client_->hover_text_point_update_count());
  generator->MoveMouseTo(gfx::Point(230, 230));
  EXPECT_EQ(9, client_->hover_text_point_update_count());
  generator->set_current_screen_location(gfx::Point(500, 500));
  generator->ReleaseTouch();
  ExpectLastPoint(10, gfx::Point(500, 500),
                  HoverTextPointerType::kTouchRelease);
  task_environment()->FastForwardBy(base::Milliseconds(50));
  EXPECT_EQ(10, client_->hover_text_point_update_count());

  // A touch move still waiting when Hover Text turns off is never sent.
  generator->PressTouch(gfx::Point(400, 400));
  EXPECT_EQ(11, client_->hover_text_point_update_count());
  generator->MoveTouch(gfx::Point(410, 410));
  OnHoverTextTrayToggled();
  ASSERT_FALSE(IsHoverTextActive());
  task_environment()->FastForwardBy(base::Milliseconds(50));
  ExpectLastPoint(11, gfx::Point(400, 400), HoverTextPointerType::kTouchPress);
}

TEST_F(HoverTextEventHandlerTest,
       SearchAndControlKeysManageActivationAndSpeech) {
  EXPECT_FALSE(IsHoverTextActive());

  int expected_key_events = 0;
  for (ui::KeyboardCode key_code : {ui::VKEY_LWIN, ui::VKEY_RWIN}) {
    // Pressing the Search activation key activates Hover Text and dispatches
    // `stop_speech = true`; auto-repeated keydowns are ignored; releasing
    // Search deactivates Hover Text and dispatches `stop_speech = true`.
    GetEventGenerator()->PressKey(key_code, ui::EF_NONE);
    ExpectLastKeyEvent(++expected_key_events, /*expected_key_held=*/true,
                       /*expected_stop_speech=*/true);
    EXPECT_TRUE(IsHoverTextActive());

    GetEventGenerator()->PressKey(key_code, ui::EF_IS_REPEAT);
    EXPECT_EQ(expected_key_events, client_->hover_text_key_event_count());

    GetEventGenerator()->ReleaseKey(key_code, ui::EF_NONE);
    ExpectLastKeyEvent(++expected_key_events, /*expected_key_held=*/false,
                       /*expected_stop_speech=*/true);
    EXPECT_FALSE(IsHoverTextActive());
  }

  // Pressing Control while inactive dispatches `stop_speech = true` without
  // activating Hover Text.
  GetEventGenerator()->PressKey(ui::VKEY_CONTROL, ui::EF_NONE);
  ExpectLastKeyEvent(++expected_key_events, /*expected_key_held=*/false,
                     /*expected_stop_speech=*/true);
  GetEventGenerator()->ReleaseKey(ui::VKEY_CONTROL, ui::EF_NONE);

  // Pressing any Control key while Search is held dispatches
  // `stop_speech = true` while keeping Hover Text active, and ignores
  // auto-repeated Control keydowns.
  GetEventGenerator()->PressKey(ui::VKEY_LWIN, ui::EF_NONE);
  ExpectLastKeyEvent(++expected_key_events, /*expected_key_held=*/true,
                     /*expected_stop_speech=*/true);
  for (ui::KeyboardCode ctrl_key :
       {ui::VKEY_CONTROL, ui::VKEY_LCONTROL, ui::VKEY_RCONTROL}) {
    GetEventGenerator()->PressKey(ctrl_key, ui::EF_NONE);
    ExpectLastKeyEvent(++expected_key_events, /*expected_key_held=*/true,
                       /*expected_stop_speech=*/true);
    GetEventGenerator()->ReleaseKey(ctrl_key, ui::EF_NONE);
  }
  GetEventGenerator()->PressKey(ui::VKEY_CONTROL, ui::EF_IS_REPEAT);
  EXPECT_EQ(expected_key_events, client_->hover_text_key_event_count());
  GetEventGenerator()->ReleaseKey(ui::VKEY_LWIN, ui::EF_NONE);
  ++expected_key_events;

  // Non-Search modifiers (Alt, Shift, Caps Lock) never trigger activation.
  for (ui::KeyboardCode non_activation_key :
       {ui::VKEY_MENU, ui::VKEY_SHIFT, ui::VKEY_CAPITAL}) {
    GetEventGenerator()->PressKey(non_activation_key, ui::EF_NONE);
    GetEventGenerator()->ReleaseKey(non_activation_key, ui::EF_NONE);
    EXPECT_EQ(expected_key_events, client_->hover_text_key_event_count());
    EXPECT_FALSE(client_->last_hover_text_activation_key_held());
  }
}

TEST_F(HoverTextEventHandlerTest,
       SearchReleaseConsumedOnlyWhenBubbleShownDuringHold) {
  // 1. A bare Search tap (or pointer move during a Search hold without showing
  // a bubble) passes the Search key-up through unhandled so the launcher opens.
  ui::KeyEvent press = SendKeyEvent(ui::EventType::kKeyPressed, ui::VKEY_LWIN);
  EXPECT_FALSE(press.handled());
  EXPECT_FALSE(press.stopped_propagation());
  ui::KeyEvent release =
      SendKeyEvent(ui::EventType::kKeyReleased, ui::VKEY_LWIN);
  EXPECT_FALSE(release.handled());
  EXPECT_FALSE(release.stopped_propagation());

  SendKeyEvent(ui::EventType::kKeyPressed, ui::VKEY_LWIN);
  FastForwardPastRateLimit();
  GetEventGenerator()->MoveMouseTo(gfx::Point(200, 200));
  release = SendKeyEvent(ui::EventType::kKeyReleased, ui::VKEY_LWIN);
  EXPECT_FALSE(release.handled());
  EXPECT_FALSE(release.stopped_propagation());

  // 2. When Hover Text displays a bubble during a Search hold, the Search
  // key-up event is consumed so the launcher does not open after hovering.
  SendKeyEvent(ui::EventType::kKeyPressed, ui::VKEY_LWIN);
  ShowHoverTextBubble();
  release = SendKeyEvent(ui::EventType::kKeyReleased, ui::VKEY_LWIN);
  EXPECT_TRUE(release.handled());
  EXPECT_TRUE(release.stopped_propagation());
  HideHoverTextBubble();

  // 3. A subsequent bare Search tap without a bubble passes through again.
  SendKeyEvent(ui::EventType::kKeyPressed, ui::VKEY_LWIN);
  release = SendKeyEvent(ui::EventType::kKeyReleased, ui::VKEY_LWIN);
  EXPECT_FALSE(release.handled());
  EXPECT_FALSE(release.stopped_propagation());

  // 4. When Hover Text is already locked active via the shelf tray button,
  // pressing Search stops speech without arming Search-hold cancellation, so
  // releasing Search is not consumed even if a bubble update arrives while
  // Search is depressed.
  OnHoverTextTrayToggled();
  ASSERT_TRUE(GetEventHandler()->is_tray_toggled_active_for_testing());
  const int count_before = client_->hover_text_key_event_count();
  SendKeyEvent(ui::EventType::kKeyPressed, ui::VKEY_LWIN);
  ExpectLastKeyEvent(count_before + 1, /*expected_key_held=*/true,
                     /*expected_stop_speech=*/true);
  EXPECT_FALSE(GetEventHandler()->activation_key_held_for_testing());
  ShowHoverTextBubble();
  release = SendKeyEvent(ui::EventType::kKeyReleased, ui::VKEY_LWIN);
  EXPECT_FALSE(release.handled());
  EXPECT_FALSE(release.stopped_propagation());
  EXPECT_TRUE(GetEventHandler()->IsActive());
  OnHoverTextTrayToggled();
}

TEST_F(HoverTextEventHandlerTest,
       SearchHoldCancelledByOtherKeyOrMouseClickOrDrag) {
  int expected_key_events = 0;

  // 1. Pressing another key while Search is held cancels the Search activation
  // chord (`OnHoverTextKeyEvent(false, false)`), prevents repeated Search
  // keydowns from re-activating before Search is released, and does not consume
  // the Search release.
  GetEventGenerator()->PressKey(ui::VKEY_LWIN, ui::EF_NONE);
  ExpectLastKeyEvent(++expected_key_events, /*expected_key_held=*/true,
                     /*expected_stop_speech=*/true);

  GetEventGenerator()->PressKey(ui::VKEY_H, ui::EF_COMMAND_DOWN);
  ExpectLastKeyEvent(++expected_key_events, /*expected_key_held=*/false,
                     /*expected_stop_speech=*/false);
  EXPECT_FALSE(GetEventHandler()->IsActive());

  GetEventGenerator()->PressKey(ui::VKEY_LWIN, ui::EF_NONE);
  EXPECT_EQ(expected_key_events, client_->hover_text_key_event_count());
  EXPECT_FALSE(GetEventHandler()->IsActive());

  GetEventGenerator()->ReleaseKey(ui::VKEY_H, ui::EF_COMMAND_DOWN);
  GetEventGenerator()->ReleaseKey(ui::VKEY_LWIN, ui::EF_NONE);
  EXPECT_EQ(expected_key_events, client_->hover_text_key_event_count());

  // 2. Clicking the mouse while Search is held cancels the Search activation
  // chord and prevents a bubble shown before the click from consuming Search
  // release.
  GetEventGenerator()->PressKey(ui::VKEY_LWIN, ui::EF_NONE);
  ++expected_key_events;
  ShowHoverTextBubble();

  GetEventGenerator()->ClickLeftButton();
  ExpectLastKeyEvent(++expected_key_events, /*expected_key_held=*/false,
                     /*expected_stop_speech=*/false);
  ui::KeyEvent release =
      SendKeyEvent(ui::EventType::kKeyReleased, ui::VKEY_LWIN);
  EXPECT_FALSE(release.handled());
  EXPECT_FALSE(release.stopped_propagation());
  HideHoverTextBubble();

  // 3. Dragging the mouse while Search is held also cancels the Search-hold
  // activation.
  GetEventGenerator()->PressKey(ui::VKEY_LWIN, ui::EF_NONE);
  ++expected_key_events;
  EXPECT_TRUE(GetEventHandler()->IsActive());
  ui::MouseEvent mouse_drag(ui::EventType::kMouseDragged, gfx::Point(120, 120),
                            gfx::Point(120, 120), base::TimeTicks::Now(),
                            ui::EF_LEFT_MOUSE_BUTTON, 0);
  GetEventHandler()->OnMouseEvent(&mouse_drag);
  ExpectLastKeyEvent(++expected_key_events, /*expected_key_held=*/false,
                     /*expected_stop_speech=*/false);
  EXPECT_FALSE(GetEventHandler()->IsActive());
  GetEventGenerator()->ReleaseKey(ui::VKEY_LWIN, ui::EF_NONE);
  EXPECT_EQ(expected_key_events, client_->hover_text_key_event_count());
}

TEST_F(HoverTextEventHandlerTest, TypingIntoFocusedTextfieldIsNeverConsumed) {
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetBounds(gfx::Rect(100, 100, 300, 200));
  views::Textfield* textfield = widget->GetContentsView()->AddChildView(
      std::make_unique<views::Textfield>());
  textfield->SetBoundsRect(gfx::Rect(10, 10, 200, 30));
  widget->Activate();
  textfield->RequestFocus();
  ASSERT_EQ(textfield, widget->GetFocusManager()->GetFocusedView());

  // 1. While inactive: typing delivers all characters to the focused textfield.
  GetEventGenerator()->PressAndReleaseKey(ui::VKEY_H, ui::EF_SHIFT_DOWN);
  GetEventGenerator()->PressAndReleaseKey(ui::VKEY_I);
  GetEventGenerator()->PressAndReleaseKey(ui::VKEY_SPACE);
  EXPECT_EQ(u"Hi ", textfield->GetText());
  EXPECT_EQ(0, client_->hover_text_key_event_count());

  // 2. While Search is held: pressing another key cancels the Search activation
  // chord without consuming the key event.
  SendKeyEvent(ui::EventType::kKeyPressed, ui::VKEY_LWIN);
  EXPECT_TRUE(IsHoverTextActive());
  ui::KeyEvent key_while_search_held =
      SendKeyEvent(ui::EventType::kKeyPressed, ui::VKEY_A, ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(key_while_search_held.handled());
  EXPECT_FALSE(key_while_search_held.stopped_propagation());
  EXPECT_FALSE(IsHoverTextActive());
  SendKeyEvent(ui::EventType::kKeyReleased, ui::VKEY_LWIN);

  // 3. While tray-toggled active: typing still delivers all characters to the
  // focused textfield without consuming any key event.
  OnHoverTextTrayToggled();
  EXPECT_TRUE(IsHoverTextActive());
  GetEventGenerator()->PressAndReleaseKey(ui::VKEY_T, ui::EF_SHIFT_DOWN);
  GetEventGenerator()->PressAndReleaseKey(ui::VKEY_E);
  GetEventGenerator()->PressAndReleaseKey(ui::VKEY_S);
  GetEventGenerator()->PressAndReleaseKey(ui::VKEY_T);
  EXPECT_EQ(u"Hi Test", textfield->GetText());
  EXPECT_EQ(textfield, widget->GetFocusManager()->GetFocusedView());
}

}  // namespace ash
