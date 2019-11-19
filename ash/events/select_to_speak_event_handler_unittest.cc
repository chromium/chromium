// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/events/select_to_speak_event_handler.h"

#include <set>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/public/cpp/select_to_speak_event_handler_delegate.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test/ash_test_views_delegate.h"
#include "base/macros.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/test/events_test_utils.h"

namespace ash {
namespace {

// Records all key events for testing.
class EventCapturer : public ui::EventHandler {
 public:
  EventCapturer() {}
  ~EventCapturer() override {}

  void Reset() {
    last_key_event_.reset();
    last_mouse_event_.reset();
    last_touch_event_.reset();
  }

  ui::KeyEvent* last_key_event() { return last_key_event_.get(); }
  ui::MouseEvent* last_mouse_event() { return last_mouse_event_.get(); }
  ui::TouchEvent* last_touch_event() { return last_touch_event_.get(); }

 private:
  void OnMouseEvent(ui::MouseEvent* event) override {
    last_mouse_event_.reset(new ui::MouseEvent(*event));
  }
  void OnKeyEvent(ui::KeyEvent* event) override {
    last_key_event_.reset(new ui::KeyEvent(*event));
  }
  void OnTouchEvent(ui::TouchEvent* event) override {
    last_touch_event_.reset(new ui::TouchEvent(*event));
  }

  std::unique_ptr<ui::KeyEvent> last_key_event_;
  std::unique_ptr<ui::MouseEvent> last_mouse_event_;
  std::unique_ptr<ui::TouchEvent> last_touch_event_;

  DISALLOW_COPY_AND_ASSIGN(EventCapturer);
};

class TestDelegate : public SelectToSpeakEventHandlerDelegate {
 public:
  TestDelegate() = default;
  virtual ~TestDelegate() = default;

  bool CapturedMouseEvent(ui::EventType event_type) {
    return mouse_events_captured_.find(event_type) !=
           mouse_events_captured_.end();
  }

  void Reset() {
    mouse_events_captured_.clear();
    last_mouse_location_.SetPoint(0, 0);
  }

  gfx::Point last_mouse_event_location() { return last_mouse_location_; }

 private:
  // SelectToSpeakEventHandlerDelegate:
  void DispatchMouseEvent(const ui::MouseEvent& event) override {
    mouse_events_captured_.insert(event.type());
    last_mouse_location_ = event.location();
  }
  void DispatchKeyEvent(const ui::KeyEvent& event) override {
    // Unused for now.
  }

  gfx::Point last_mouse_location_;
  std::set<ui::EventType> mouse_events_captured_;

  DISALLOW_COPY_AND_ASSIGN(TestDelegate);
};

class SelectToSpeakEventHandlerTest : public AshTestBase {
 public:
  SelectToSpeakEventHandlerTest() = default;
  ~SelectToSpeakEventHandlerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    // This test triggers a resize of WindowTreeHost, which will end up
    // throttling events. set_throttle_input_on_resize_for_testing() disables
    // this.
    aura::Env::GetInstance()->set_throttle_input_on_resize_for_testing(false);
    delegate_ = std::make_unique<TestDelegate>();
    generator_ = AshTestBase::GetEventGenerator();
    CurrentContext()->AddPreTargetHandler(&event_capturer_);

    controller_ = Shell::Get()->accessibility_controller();
    controller_->SetSelectToSpeakEnabled(true);
    controller_->SetSelectToSpeakEventHandlerDelegate(delegate_.get());
  }

  void TearDown() override {
    CurrentContext()->RemovePreTargetHandler(&event_capturer_);
    generator_ = nullptr;
    controller_ = nullptr;
    AshTestBase::TearDown();
  }

 protected:
  ui::test::EventGenerator* generator_ = nullptr;
  EventCapturer event_capturer_;
  AccessibilityControllerImpl* controller_ = nullptr;
  std::unique_ptr<TestDelegate> delegate_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SelectToSpeakEventHandlerTest);
};

TEST_F(SelectToSpeakEventHandlerTest, PressAndReleaseSearchNotHandled) {
  // If the user presses and releases the Search key, with no mouse
  // presses, the key events won't be handled by the SelectToSpeakEventHandler
  // and the normal behavior will occur.

  EXPECT_FALSE(event_capturer_.last_key_event());

  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(event_capturer_.last_key_event());
  EXPECT_FALSE(event_capturer_.last_key_event()->handled());

  event_capturer_.Reset();
  generator_->ReleaseKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(event_capturer_.last_key_event());
  EXPECT_FALSE(event_capturer_.last_key_event()->handled());
}

// Note: when running these tests locally on desktop Linux, you may need
// to use xvfb-run, otherwise simulating the Search key press may not work.
TEST_F(SelectToSpeakEventHandlerTest, SearchPlusClick) {
  // If the user holds the Search key and then clicks the mouse button,
  // the mouse events and the key release event get handled by the
  // SelectToSpeakEventHandler, and mouse events are forwarded to the
  // extension.

  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(event_capturer_.last_key_event());
  EXPECT_FALSE(event_capturer_.last_key_event()->handled());

  gfx::Point click_location = gfx::Point(100, 12);
  generator_->set_current_screen_location(click_location);
  generator_->PressLeftButton();
  EXPECT_FALSE(event_capturer_.last_mouse_event());

  EXPECT_TRUE(delegate_->CapturedMouseEvent(ui::ET_MOUSE_PRESSED));
  EXPECT_EQ(click_location, delegate_->last_mouse_event_location());

  generator_->ReleaseLeftButton();
  EXPECT_FALSE(event_capturer_.last_mouse_event());

  EXPECT_TRUE(delegate_->CapturedMouseEvent(ui::ET_MOUSE_RELEASED));
  EXPECT_EQ(click_location, delegate_->last_mouse_event_location());

  event_capturer_.Reset();
  generator_->ReleaseKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(event_capturer_.last_key_event());
}

TEST_F(SelectToSpeakEventHandlerTest, SearchPlusDrag) {
  // Mouse move events should also be captured.

  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  gfx::Point click_location = gfx::Point(100, 12);
  generator_->set_current_screen_location(click_location);
  generator_->PressLeftButton();

  EXPECT_EQ(click_location, delegate_->last_mouse_event_location());

  // Drags are not blocked.
  gfx::Point drag_location = gfx::Point(120, 32);
  generator_->DragMouseTo(drag_location);
  EXPECT_EQ(drag_location, delegate_->last_mouse_event_location());
  EXPECT_TRUE(delegate_->CapturedMouseEvent(ui::ET_MOUSE_DRAGGED));
  EXPECT_TRUE(event_capturer_.last_mouse_event());
  event_capturer_.Reset();

  generator_->ReleaseLeftButton();
  EXPECT_EQ(drag_location, delegate_->last_mouse_event_location());
  EXPECT_TRUE(delegate_->CapturedMouseEvent(ui::ET_MOUSE_RELEASED));

  generator_->ReleaseKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
}

TEST_F(SelectToSpeakEventHandlerTest, SearchPlusDragOnLargeDisplay) {
  // This display has twice the number of pixels per DIP. This means that
  // each event coming in in px needs to be divided by two to be converted
  // to DIPs.
  UpdateDisplay("800x600*2");

  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  gfx::Point click_location_px = gfx::Point(100, 12);
  generator_->set_current_screen_location(click_location_px);
  generator_->PressLeftButton();
  EXPECT_EQ(gfx::Point(click_location_px.x() / 2, click_location_px.y() / 2),
            delegate_->last_mouse_event_location());

  gfx::Point drag_location_px = gfx::Point(120, 32);
  generator_->DragMouseTo(drag_location_px);
  EXPECT_EQ(gfx::Point(drag_location_px.x() / 2, drag_location_px.y() / 2),
            delegate_->last_mouse_event_location());
  EXPECT_TRUE(delegate_->CapturedMouseEvent(ui::ET_MOUSE_DRAGGED));
  EXPECT_TRUE(event_capturer_.last_mouse_event());
  event_capturer_.Reset();

  generator_->ReleaseLeftButton();
  EXPECT_EQ(gfx::Point(drag_location_px.x() / 2, drag_location_px.y() / 2),
            delegate_->last_mouse_event_location());
  EXPECT_TRUE(delegate_->CapturedMouseEvent(ui::ET_MOUSE_RELEASED));

  generator_->ReleaseKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
}

TEST_F(SelectToSpeakEventHandlerTest, RepeatSearchKey) {
  // Holding the Search key may generate key repeat events. Make sure it's
  // still treated as if the search key is down.
  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);

  generator_->set_current_screen_location(gfx::Point(100, 12));
  generator_->PressLeftButton();
  EXPECT_FALSE(event_capturer_.last_mouse_event());

  EXPECT_TRUE(delegate_->CapturedMouseEvent(ui::ET_MOUSE_PRESSED));

  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);

  generator_->ReleaseLeftButton();
  EXPECT_FALSE(event_capturer_.last_mouse_event());

  EXPECT_TRUE(delegate_->CapturedMouseEvent(ui::ET_MOUSE_RELEASED));

  event_capturer_.Reset();
  generator_->ReleaseKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(event_capturer_.last_key_event());
}

TEST_F(SelectToSpeakEventHandlerTest, TapSearchKey) {
  // Tapping the search key should not steal future events.

  event_capturer_.Reset();
  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  generator_->ReleaseKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  generator_->PressLeftButton();
  EXPECT_FALSE(event_capturer_.last_mouse_event()->handled());
  generator_->ReleaseLeftButton();
  EXPECT_FALSE(event_capturer_.last_mouse_event()->handled());
}

TEST_F(SelectToSpeakEventHandlerTest, SearchPlusClickTwice) {
  // Same as SearchPlusClick, above, but test that the user can keep
  // holding down Search and click again.

  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(event_capturer_.last_key_event());
  EXPECT_FALSE(event_capturer_.last_key_event()->handled());

  generator_->set_current_screen_location(gfx::Point(100, 12));
  generator_->PressLeftButton();
  EXPECT_FALSE(event_capturer_.last_mouse_event());
  EXPECT_TRUE(delegate_->CapturedMouseEvent(ui::ET_MOUSE_PRESSED));

  generator_->ReleaseLeftButton();
  EXPECT_FALSE(event_capturer_.last_mouse_event());
  EXPECT_TRUE(delegate_->CapturedMouseEvent(ui::ET_MOUSE_RELEASED));

  delegate_->Reset();
  EXPECT_FALSE(delegate_->CapturedMouseEvent(ui::ET_MOUSE_PRESSED));
  EXPECT_FALSE(delegate_->CapturedMouseEvent(ui::ET_MOUSE_RELEASED));

  generator_->PressLeftButton();
  EXPECT_FALSE(event_capturer_.last_mouse_event());
  EXPECT_TRUE(delegate_->CapturedMouseEvent(ui::ET_MOUSE_PRESSED));

  generator_->ReleaseLeftButton();
  EXPECT_FALSE(event_capturer_.last_mouse_event());
  EXPECT_TRUE(delegate_->CapturedMouseEvent(ui::ET_MOUSE_RELEASED));

  event_capturer_.Reset();
  generator_->ReleaseKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(event_capturer_.last_key_event());
}

TEST_F(SelectToSpeakEventHandlerTest, SearchPlusKeyIgnoresClicks) {
  // If the user presses the Search key and then some other key,
  // we should assume the user does not want select-to-speak, and
  // click events should be ignored.

  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(event_capturer_.last_key_event());
  EXPECT_FALSE(event_capturer_.last_key_event()->handled());

  generator_->PressKey(ui::VKEY_I, ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(event_capturer_.last_key_event());
  EXPECT_FALSE(event_capturer_.last_key_event()->handled());

  generator_->set_current_screen_location(gfx::Point(100, 12));
  generator_->PressLeftButton();
  ASSERT_TRUE(event_capturer_.last_mouse_event());
  EXPECT_FALSE(event_capturer_.last_mouse_event()->handled());

  EXPECT_FALSE(delegate_->CapturedMouseEvent(ui::ET_MOUSE_PRESSED));

  generator_->ReleaseLeftButton();
  ASSERT_TRUE(event_capturer_.last_mouse_event());
  EXPECT_FALSE(event_capturer_.last_mouse_event()->handled());

  EXPECT_FALSE(delegate_->CapturedMouseEvent(ui::ET_MOUSE_RELEASED));

  event_capturer_.Reset();
  generator_->ReleaseKey(ui::VKEY_I, ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(event_capturer_.last_key_event());
  EXPECT_FALSE(event_capturer_.last_key_event()->handled());

  event_capturer_.Reset();
  generator_->ReleaseKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(event_capturer_.last_key_event());
  EXPECT_FALSE(event_capturer_.last_key_event()->handled());
}

TEST_F(SelectToSpeakEventHandlerTest, SearchPlusSIsCaptured) {
  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);

  // Press and release S, key presses should be captured.
  event_capturer_.Reset();
  generator_->PressKey(ui::VKEY_S, ui::EF_COMMAND_DOWN);
  ASSERT_FALSE(event_capturer_.last_key_event());

  generator_->ReleaseKey(ui::VKEY_S, ui::EF_COMMAND_DOWN);
  ASSERT_FALSE(event_capturer_.last_key_event());

  // Press and release again while still holding down search.
  // The events should continue to be captured.
  generator_->PressKey(ui::VKEY_S, ui::EF_COMMAND_DOWN);
  ASSERT_FALSE(event_capturer_.last_key_event());

  generator_->ReleaseKey(ui::VKEY_S, ui::EF_COMMAND_DOWN);
  ASSERT_FALSE(event_capturer_.last_key_event());

  generator_->ReleaseKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  ASSERT_FALSE(event_capturer_.last_key_event());

  // S alone is not captured
  generator_->PressKey(ui::VKEY_S, ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(event_capturer_.last_key_event());
  ASSERT_FALSE(event_capturer_.last_key_event()->handled());
}

TEST_F(SelectToSpeakEventHandlerTest, SearchPlusSIgnoresMouse) {
  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);

  // Press S
  event_capturer_.Reset();
  generator_->PressKey(ui::VKEY_S, ui::EF_COMMAND_DOWN);
  ASSERT_FALSE(event_capturer_.last_key_event());

  // Mouse events are passed through like normal.
  generator_->PressLeftButton();
  EXPECT_TRUE(event_capturer_.last_mouse_event());
  event_capturer_.Reset();
  generator_->ReleaseLeftButton();
  EXPECT_TRUE(event_capturer_.last_mouse_event());

  generator_->ReleaseKey(ui::VKEY_S, ui::EF_COMMAND_DOWN);
  ASSERT_FALSE(event_capturer_.last_key_event());

  generator_->ReleaseKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  ASSERT_FALSE(event_capturer_.last_key_event());
}

TEST_F(SelectToSpeakEventHandlerTest, SearchPlusMouseIgnoresS) {
  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);

  // Press the mouse
  event_capturer_.Reset();
  generator_->PressLeftButton();
  EXPECT_FALSE(event_capturer_.last_mouse_event());

  // S key events are passed through like normal.
  generator_->PressKey(ui::VKEY_S, ui::EF_COMMAND_DOWN);
  EXPECT_TRUE(event_capturer_.last_key_event());
  event_capturer_.Reset();
  generator_->ReleaseKey(ui::VKEY_S, ui::EF_COMMAND_DOWN);
  EXPECT_TRUE(event_capturer_.last_key_event());

  generator_->ReleaseLeftButton();
  EXPECT_FALSE(event_capturer_.last_mouse_event());

  event_capturer_.Reset();
  generator_->ReleaseKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(event_capturer_.last_key_event());
}

TEST_F(SelectToSpeakEventHandlerTest, DoesntStartSelectionModeIfNotInactive) {
  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);

  // This shouldn't cause any changes since the state is not inactive.
  controller_->SetSelectToSpeakState(
      SelectToSpeakState::kSelectToSpeakStateSelecting);

  // Mouse event still captured.
  gfx::Point click_location = gfx::Point(100, 12);
  generator_->set_current_screen_location(click_location);
  generator_->PressLeftButton();
  EXPECT_FALSE(event_capturer_.last_mouse_event());

  // This shouldn't cause any changes since the state is not inactive.
  controller_->SetSelectToSpeakState(
      SelectToSpeakState::kSelectToSpeakStateSelecting);

  generator_->ReleaseLeftButton();

  // Releasing the search key is still captured per the end of the search+click
  // mode.
  event_capturer_.Reset();
  generator_->ReleaseKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(event_capturer_.last_key_event());
}

TEST_F(SelectToSpeakEventHandlerTest,
       CancelSearchKeyUpAfterEarlyInactiveStateChange) {
  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  gfx::Point click_location = gfx::Point(100, 12);
  generator_->set_current_screen_location(click_location);
  generator_->PressLeftButton();
  EXPECT_FALSE(event_capturer_.last_mouse_event());
  EXPECT_TRUE(delegate_->CapturedMouseEvent(ui::ET_MOUSE_PRESSED));
  generator_->ReleaseLeftButton();
  EXPECT_FALSE(event_capturer_.last_mouse_event());
  EXPECT_TRUE(delegate_->CapturedMouseEvent(ui::ET_MOUSE_RELEASED));

  // Set the state to inactive.
  // This is realistic because Select-to-Speak will set the state to inactive
  // after the hittest / search for the focused node callbacks, which may occur
  // before the user actually releases the search key.
  controller_->SetSelectToSpeakState(
      SelectToSpeakState::kSelectToSpeakStateInactive);

  // The search key release should still be captured.
  event_capturer_.Reset();
  generator_->ReleaseKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(event_capturer_.last_key_event());
}

TEST_F(SelectToSpeakEventHandlerTest, SelectionRequestedWorksWithMouse) {
  gfx::Point click_location = gfx::Point(100, 12);
  generator_->set_current_screen_location(click_location);

  // Mouse events are let through normally before entering selecting state.
  // Another mouse event is let through normally.
  controller_->SetSelectToSpeakState(
      SelectToSpeakState::kSelectToSpeakStateInactive);
  generator_->PressLeftButton();
  EXPECT_TRUE(event_capturer_.last_mouse_event());
  event_capturer_.Reset();
  generator_->ReleaseLeftButton();
  EXPECT_TRUE(event_capturer_.last_mouse_event());
  event_capturer_.Reset();

  // Start selection mode.
  controller_->SetSelectToSpeakState(
      SelectToSpeakState::kSelectToSpeakStateSelecting);

  generator_->PressLeftButton();
  EXPECT_FALSE(event_capturer_.last_mouse_event());
  EXPECT_TRUE(delegate_->CapturedMouseEvent(ui::ET_MOUSE_PRESSED));
  event_capturer_.Reset();

  gfx::Point drag_location = gfx::Point(120, 32);
  generator_->DragMouseTo(drag_location);
  EXPECT_EQ(drag_location, delegate_->last_mouse_event_location());
  EXPECT_TRUE(delegate_->CapturedMouseEvent(ui::ET_MOUSE_DRAGGED));
  EXPECT_TRUE(event_capturer_.last_mouse_event());
  event_capturer_.Reset();

  // Mouse up is the last event captured in the sequence
  generator_->ReleaseLeftButton();
  EXPECT_FALSE(event_capturer_.last_mouse_event());
  event_capturer_.Reset();

  // Another mouse event is let through normally.
  generator_->PressLeftButton();
  EXPECT_TRUE(event_capturer_.last_mouse_event());
  event_capturer_.Reset();
}

TEST_F(SelectToSpeakEventHandlerTest, SelectionRequestedWorksWithTouch) {
  gfx::Point touch_location = gfx::Point(100, 12);
  generator_->set_current_screen_location(touch_location);

  // Mouse events are let through normally before entering selecting state.
  // Another mouse event is let through normally.
  controller_->SetSelectToSpeakState(
      SelectToSpeakState::kSelectToSpeakStateInactive);
  generator_->PressTouch();
  EXPECT_TRUE(event_capturer_.last_touch_event());
  event_capturer_.Reset();
  generator_->ReleaseTouch();
  EXPECT_TRUE(event_capturer_.last_touch_event());
  event_capturer_.Reset();

  // Start selection mode.
  controller_->SetSelectToSpeakState(
      SelectToSpeakState::kSelectToSpeakStateSelecting);

  generator_->PressTouch();
  EXPECT_FALSE(event_capturer_.last_touch_event());
  // Touch events are converted to mouse events for the extension.
  EXPECT_TRUE(delegate_->CapturedMouseEvent(ui::ET_MOUSE_PRESSED));
  event_capturer_.Reset();

  gfx::Point drag_location = gfx::Point(120, 32);
  generator_->MoveTouch(drag_location);
  EXPECT_EQ(drag_location, delegate_->last_mouse_event_location());
  EXPECT_TRUE(delegate_->CapturedMouseEvent(ui::ET_MOUSE_DRAGGED));
  EXPECT_TRUE(event_capturer_.last_touch_event());
  event_capturer_.Reset();

  // Touch up is the last event captured in the sequence
  generator_->ReleaseTouch();
  EXPECT_FALSE(event_capturer_.last_touch_event());
  EXPECT_TRUE(delegate_->CapturedMouseEvent(ui::ET_MOUSE_RELEASED));
  event_capturer_.Reset();

  // Another touch event is let through normally.
  generator_->PressTouch();
  EXPECT_TRUE(event_capturer_.last_touch_event());
  event_capturer_.Reset();
}

TEST_F(SelectToSpeakEventHandlerTest, SelectionRequestedIgnoresOtherInput) {
  // Start selection mode.
  controller_->SetSelectToSpeakState(
      SelectToSpeakState::kSelectToSpeakStateSelecting);

  // Search key events are not impacted.
  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  EXPECT_TRUE(event_capturer_.last_key_event());
  event_capturer_.Reset();
  generator_->ReleaseKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  EXPECT_TRUE(event_capturer_.last_key_event());
  event_capturer_.Reset();

  // Start a touch selection, it should get captured and forwarded.
  generator_->PressTouch();
  EXPECT_FALSE(event_capturer_.last_touch_event());
  EXPECT_TRUE(delegate_->CapturedMouseEvent(ui::ET_MOUSE_PRESSED));
  event_capturer_.Reset();

  // Mouse event happening during the touch selection are not impacted;
  // we are locked into a touch selection mode.
  generator_->PressLeftButton();
  EXPECT_TRUE(event_capturer_.last_mouse_event());
  event_capturer_.Reset();
  generator_->ReleaseLeftButton();
  EXPECT_TRUE(event_capturer_.last_mouse_event());
  event_capturer_.Reset();

  // Complete the touch selection.
  generator_->ReleaseTouch();
  EXPECT_FALSE(event_capturer_.last_touch_event());
  event_capturer_.Reset();
}

TEST_F(SelectToSpeakEventHandlerTest, TrackingTouchIgnoresOtherTouchPointers) {
  gfx::Point touch_location = gfx::Point(100, 12);
  gfx::Point drag_location = gfx::Point(120, 32);
  generator_->set_current_screen_location(touch_location);
  controller_->SetSelectToSpeakState(
      SelectToSpeakState::kSelectToSpeakStateSelecting);

  // The first touch event is captured and sent to the extension.
  generator_->PressTouchId(1);
  EXPECT_FALSE(event_capturer_.last_touch_event());
  EXPECT_TRUE(delegate_->CapturedMouseEvent(ui::ET_MOUSE_PRESSED));
  event_capturer_.Reset();
  delegate_->Reset();

  // A second touch event up and down is canceled but not sent to the extension.
  generator_->PressTouchId(2);
  EXPECT_FALSE(event_capturer_.last_touch_event());
  EXPECT_FALSE(delegate_->CapturedMouseEvent(ui::ET_MOUSE_PRESSED));
  generator_->MoveTouchId(drag_location, 2);
  EXPECT_FALSE(event_capturer_.last_touch_event());
  EXPECT_FALSE(delegate_->CapturedMouseEvent(ui::ET_MOUSE_DRAGGED));
  generator_->ReleaseTouchId(2);
  EXPECT_FALSE(event_capturer_.last_touch_event());
  EXPECT_FALSE(delegate_->CapturedMouseEvent(ui::ET_MOUSE_RELEASED));

  // A pointer type event will not be sent either, as we are tracking touch,
  // even if the ID is the same.
  generator_->EnterPenPointerMode();
  generator_->PressTouchId(1);
  EXPECT_FALSE(event_capturer_.last_touch_event());
  EXPECT_FALSE(delegate_->CapturedMouseEvent(ui::ET_MOUSE_PRESSED));
  generator_->ExitPenPointerMode();

  // The first pointer is still tracked.
  generator_->MoveTouchId(drag_location, 1);
  EXPECT_EQ(drag_location, delegate_->last_mouse_event_location());
  EXPECT_TRUE(delegate_->CapturedMouseEvent(ui::ET_MOUSE_DRAGGED));
  EXPECT_TRUE(event_capturer_.last_touch_event());
  event_capturer_.Reset();

  // Touch up is the last event captured in the sequence
  generator_->ReleaseTouchId(1);
  EXPECT_FALSE(event_capturer_.last_touch_event());
  event_capturer_.Reset();

  // Another touch event is let through normally.
  generator_->PressTouchId(3);
  EXPECT_TRUE(event_capturer_.last_touch_event());
  event_capturer_.Reset();
}

}  // namespace
}  // namespace ash
