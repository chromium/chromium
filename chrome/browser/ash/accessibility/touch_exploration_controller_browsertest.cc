// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/chromevox/touch_exploration_controller.h"

#include <memory>

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/test/draw_waiter_for_test.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/test/test_event_handler.h"

namespace ash {

class TouchExplorationTest : public InProcessBrowserTest {
 public:
  TouchExplorationTest() : simulated_clock_(new base::SimpleTestTickClock()) {
    // Tests fail if time is ever 0.
    simulated_clock_->Advance(base::Milliseconds(10));
  }

  TouchExplorationTest(const TouchExplorationTest&) = delete;
  TouchExplorationTest& operator=(const TouchExplorationTest&) = delete;

  ~TouchExplorationTest() override {}

 protected:
  void SetUpOnMainThread() override {
    // The RenderView for WebContents is created as a result of the
    // navigation to the New Tab page which is done as part of the test
    // SetUp. The creation involves sending a resize message to the renderer
    // process. Here we wait for the resize ack to be received, because
    // currently WindowEventDispatcher has code to hold touch and mouse
    // move events until resize is complete (crbug.com/384342) which
    // interferes with this test.
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::WaitForResizeComplete(web_contents);
    root_window_ = Shell::Get()->GetPrimaryRootWindow();
    event_handler_ = std::make_unique<ui::test::TestEventHandler>();
    root_window_->AddPreTargetHandler(event_handler_.get());
  }

  void TearDownOnMainThread() override {
    SwitchTouchExplorationMode(false);
    root_window_->RemovePreTargetHandler(event_handler_.get());
  }

  void SwitchTouchExplorationMode(bool on) {
    AccessibilityManager::Get()->EnableSpokenFeedback(on);
  }

  base::TimeTicks Now() { return simulated_clock_->NowTicks(); }

  ui::GestureDetector::Config gesture_detector_config_;
  raw_ptr<base::SimpleTestTickClock> simulated_clock_;
  raw_ptr<aura::Window> root_window_;
  std::unique_ptr<ui::test::TestEventHandler> event_handler_;
};

// This test turns the touch exploration mode off and confirms that events
// aren't modified.
// Disabled: crbug.com/422943
IN_PROC_BROWSER_TEST_F(TouchExplorationTest,
                       DISABLED_NoRewritingEventsWhenOff) {
  SwitchTouchExplorationMode(false);
  ui::test::EventGenerator generator(root_window_);

  base::TimeTicks initial_time = Now();
  ui::TouchEvent initial_press(
      ui::EventType::kTouchPressed, gfx::Point(99, 200), initial_time,
      ui::PointerDetails(ui::EventPointerType::kTouch, 1));
  generator.Dispatch(&initial_press);

  // Since the touch exploration controller doesn't know if the user is
  // double-tapping or not, touch exploration is only initiated if the
  // 300 ms has elapsed and the finger does not move fast enough to begin
  // gestures. Here, the touch move event is not important as a move, but
  // a way to create time advancement.
  ui::TouchEvent touch_time_advance(
      ui::EventType::kTouchMoved, gfx::Point(100, 200),
      initial_time + gesture_detector_config_.double_tap_timeout +
          base::Milliseconds(1),
      ui::PointerDetails(ui::EventPointerType::kTouch, 1));
  generator.Dispatch(&touch_time_advance);

  EXPECT_EQ(0, event_handler_->num_mouse_events());
  EXPECT_EQ(2, event_handler_->num_touch_events());
  event_handler_->Reset();

  generator.MoveTouchId(gfx::Point(11, 12), 1);
  EXPECT_EQ(0, event_handler_->num_mouse_events());
  EXPECT_EQ(1, event_handler_->num_touch_events());
  event_handler_->Reset();

  initial_time = Now();
  ui::TouchEvent second_initial_press(
      ui::EventType::kTouchPressed, gfx::Point(499, 600), initial_time,
      ui::PointerDetails(ui::EventPointerType::kTouch, 2));
  generator.Dispatch(&second_initial_press);
  ui::TouchEvent second_touch_time_advance(
      ui::EventType::kTouchMoved, gfx::Point(500, 600),
      initial_time + gesture_detector_config_.double_tap_timeout +
          base::Milliseconds(1),
      ui::PointerDetails(ui::EventPointerType::kTouch, 2));
  generator.Dispatch(&second_touch_time_advance);
  EXPECT_EQ(0, event_handler_->num_mouse_events());
  EXPECT_EQ(2, event_handler_->num_touch_events());
}

// This test turns the touch exploration mode on and confirms that events get
// rewritten.
// Disabling due to failing over 10% of the time. (crbug.com/469119)
IN_PROC_BROWSER_TEST_F(TouchExplorationTest, DISABLED_RewritesEventsWhenOn) {
  SwitchTouchExplorationMode(true);
  ui::test::EventGenerator generator(root_window_);

  base::TimeTicks initial_time = Now();
  ui::TouchEvent initial_press(
      ui::EventType::kTouchPressed, gfx::Point(100, 200), initial_time,
      ui::PointerDetails(ui::EventPointerType::kTouch, 1));
  generator.Dispatch(&initial_press);

  // Since the touch exploration controller doesn't know if the user is
  // double-tapping or not, touch exploration is only initiated if the
  // 300 ms has elapsed and the finger does not move fast enough to begin
  // gestures. Here, the touch move event is not important as a move, but
  // a way to create time advancement.
  ui::TouchEvent touch_time_advance(
      ui::EventType::kTouchMoved, gfx::Point(100, 200),
      initial_time + gesture_detector_config_.double_tap_timeout +
          base::Milliseconds(1),
      ui::PointerDetails(ui::EventPointerType::kTouch, 1));
  generator.Dispatch(&touch_time_advance);

  // Number of mouse events may be greater than 1 because of
  // EventType::kMouseEntered.
  EXPECT_GT(event_handler_->num_mouse_events(), 0);
  EXPECT_EQ(0, event_handler_->num_touch_events());
  event_handler_->Reset();

  initial_time = Now();
  ui::TouchEvent second_initial_press(
      ui::EventType::kTouchPressed, gfx::Point(500, 600), initial_time,
      ui::PointerDetails(ui::EventPointerType::kTouch, 2));
  generator.Dispatch(&second_initial_press);
  ui::TouchEvent second_touch_time_advance(
      ui::EventType::kTouchMoved, gfx::Point(500, 600),
      initial_time + gesture_detector_config_.double_tap_timeout +
          base::Milliseconds(1),
      ui::PointerDetails(ui::EventPointerType::kTouch, 2));
  generator.Dispatch(&second_touch_time_advance);
  EXPECT_GT(event_handler_->num_mouse_events(), 0);
  EXPECT_EQ(1, event_handler_->num_touch_events());
  event_handler_->Reset();

  // Stop the pending long press event. In some configurations, shutting down
  // the browser can take longer than the long press timeout, and a long press
  // event can come after the browser is already partly shut down, which causes
  // the test to crash.
  ui::TouchEvent release_second_touch(
      ui::EventType::kTouchReleased, gfx::Point(500, 600),
      initial_time + gesture_detector_config_.double_tap_timeout +
          base::Milliseconds(1),
      ui::PointerDetails(ui::EventPointerType::kTouch, 2));
  generator.Dispatch(&release_second_touch);
  EXPECT_GT(event_handler_->num_mouse_events(), 0);
  EXPECT_EQ(1, event_handler_->num_touch_events());
}

// This test makes sure that after the user clicks with split tap,
// they continue to touch exploration mode if the original touch exploration
// finger is still on the screen.
// Disabled due to failing upwards of 50% of the time (crbug.com/475923)
IN_PROC_BROWSER_TEST_F(TouchExplorationTest, DISABLED_SplitTapExplore) {
  SwitchTouchExplorationMode(true);
  ui::test::EventGenerator generator(root_window_);
  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(root_window_);

  // Mouse events should show the cursor.
  generator.MoveMouseTo(gfx::Point(30, 31));
  EXPECT_TRUE(cursor_client->IsMouseEventsEnabled());
  EXPECT_TRUE(cursor_client->IsCursorVisible());

  // The cursor should be shown immediately after the  press, and hidden
  // after the move.
  base::TimeTicks initial_time = Now();
  ui::TouchEvent initial_press(
      ui::EventType::kTouchPressed, gfx::Point(100, 200), initial_time,
      ui::PointerDetails(ui::EventPointerType::kTouch, 1));
  generator.Dispatch(&initial_press);
  EXPECT_TRUE(cursor_client->IsMouseEventsEnabled());
  EXPECT_TRUE(cursor_client->IsCursorVisible());

  // Initiate touch explore by waiting for the tap timer timeout. Time is
  // advanced by sending a move event after the timeout period.
  ui::TouchEvent touch_time_advance(
      ui::EventType::kTouchMoved, gfx::Point(100, 200),
      initial_time + gesture_detector_config_.double_tap_timeout +
          base::Milliseconds(1),
      ui::PointerDetails(ui::EventPointerType::kTouch, 1));
  generator.Dispatch(&touch_time_advance);
  EXPECT_TRUE(cursor_client->IsMouseEventsEnabled());
  EXPECT_FALSE(cursor_client->IsCursorVisible());
  event_handler_->Reset();

  // Press and release with a second finger for split tap. This should send
  // touch press and release events which should send a click press and release.
  // Once the press is passed through, mouse events should be disabled.
  // Mouse events are reenabled after the release.
  generator.set_current_screen_location(gfx::Point(102, 202));
  generator.PressTouchId(2);
  EXPECT_FALSE(cursor_client->IsMouseEventsEnabled());
  EXPECT_FALSE(cursor_client->IsCursorVisible());
  generator.ReleaseTouchId(2);
  EXPECT_TRUE(cursor_client->IsMouseEventsEnabled());
  EXPECT_FALSE(cursor_client->IsCursorVisible());
  EXPECT_EQ(2, event_handler_->num_touch_events());
  event_handler_->Reset();

  // Continuing to move the touch exploration finger should send more mouse
  // events.
  generator.MoveTouchId(gfx::Point(509, 609), 1);
  EXPECT_EQ(0, event_handler_->num_touch_events());
  EXPECT_TRUE(cursor_client->IsMouseEventsEnabled());
  EXPECT_FALSE(cursor_client->IsCursorVisible());
}

}  // namespace ash
