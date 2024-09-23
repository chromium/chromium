// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/events/select_to_speak_event_handler.h"

#include <memory>
#include <set>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/events/test_event_capturer.h"
#include "ash/public/cpp/select_to_speak_event_handler_delegate.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test/ash_test_views_delegate.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/window.h"
#include "ui/display/manager/display_manager.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/test/events_test_utils.h"
#include "ui/events/types/event_type.h"

namespace ash {
namespace {

class TestDelegate : public SelectToSpeakEventHandlerDelegate {
 public:
  TestDelegate() = default;

  TestDelegate(const TestDelegate&) = delete;
  TestDelegate& operator=(const TestDelegate&) = delete;

  virtual ~TestDelegate() = default;

  bool CapturedMouseEvent(ui::EventType event_type) {
    return base::Contains(mouse_events_captured_, event_type);
  }

  void Reset() {
    mouse_events_captured_.clear();
    last_mouse_location_.SetPoint(0, 0);
    last_mouse_root_location_.SetPoint(0, 0);
  }

  gfx::Point last_mouse_event_location() { return last_mouse_location_; }

  gfx::Point last_mouse_event_root_location() {
    return last_mouse_root_location_;
  }

 private:
  // SelectToSpeakEventHandlerDelegate:
  void DispatchMouseEvent(const ui::MouseEvent& event) override {
    mouse_events_captured_.insert(event.type());
    last_mouse_location_ = event.location();
    last_mouse_root_location_ = event.root_location();
  }
  void DispatchKeysCurrentlyDown(
      const std::set<ui::KeyboardCode>& pressed_keys) override {
    // Unused for now.
  }

  gfx::Point last_mouse_location_;
  gfx::Point last_mouse_root_location_;
  std::set<ui::EventType> mouse_events_captured_;
};

class SelectToSpeakEventHandlerTest : public AshTestBase {
 public:
  SelectToSpeakEventHandlerTest() = default;

  SelectToSpeakEventHandlerTest(const SelectToSpeakEventHandlerTest&) = delete;
  SelectToSpeakEventHandlerTest& operator=(
      const SelectToSpeakEventHandlerTest&) = delete;

  ~SelectToSpeakEventHandlerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    // This test triggers a resize of WindowTreeHost, which will end up
    // throttling events. set_throttle_input_on_resize_for_testing() disables
    // this.
    aura::Env::GetInstance()->set_throttle_input_on_resize_for_testing(false);

    // Make sure the display is initialized so we don't fail the test due to any
    // input events caused from creating the display.
    Shell::Get()->display_manager()->UpdateDisplays();
    base::RunLoop().RunUntilIdle();

    delegate_ = std::make_unique<TestDelegate>();
    generator_ = AshTestBase::GetEventGenerator();
    GetContext()->AddPreTargetHandler(&event_capturer_);

    controller_ = Shell::Get()->accessibility_controller();
    controller_->select_to_speak().SetEnabled(true);
    controller_->SetSelectToSpeakEventHandlerDelegate(delegate_.get());
  }

  void TearDown() override {
    GetContext()->RemovePreTargetHandler(&event_capturer_);
    generator_ = nullptr;
    controller_ = nullptr;
    AshTestBase::TearDown();
  }

 protected:
  raw_ptr<ui::test::EventGenerator> generator_ = nullptr;
  TestEventCapturer event_capturer_;
  raw_ptr<AccessibilityController> controller_ = nullptr;
  std::unique_ptr<TestDelegate> delegate_;
};

TEST_F(SelectToSpeakEventHandlerTest, PressAndReleaseSearchNotHandled) {
  // If the user presses and releases the Search key, with no mouse
  // presses, the key events won't be handled by the SelectToSpeakEventHandler
  // and the normal behavior will occur.

  EXPECT_FALSE(event_capturer_.LastKeyEvent());

  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(event_capturer_.LastKeyEvent());
  EXPECT_FALSE(event_capturer_.LastKeyEvent()->handled());

  event_capturer_.ClearEvents();
  generator_->ReleaseKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(event_capturer_.LastKeyEvent());
  EXPECT_FALSE(event_capturer_.LastKeyEvent()->handled());
}

// Note: when running these tests locally on desktop Linux, you may need
// to use xvfb-run, otherwise simulating the Search key press may not work.
TEST_F(SelectToSpeakEventHandlerTest, SearchPlusClick) {
  // If the user holds the Search key and then clicks the mouse button,
  // the mouse events and the key release event get handled by the
  // SelectToSpeakEventHandler, and mouse events are forwarded to the
  // extension.

  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(event_capturer_.LastKeyEvent());
  EXPECT_FALSE(event_capturer_.LastKeyEvent()->handled());

  gfx::Point click_location = gfx::Point(100, 12);
  generator_->set_current_screen_location(click_location);
  generator_->PressLeftButton();
  EXPECT_FALSE(event_capturer_.LastMouseEvent());

  EXPECT_TRUE(delegate_->CapturedMouseEvent(ui::EventType::kMousePressed));
  EXPECT_EQ(click_location, delegate_->last_mouse_event_location());

  generator_->ReleaseLeftButton();
  EXPECT_FALSE(event_capturer_.LastMouseEvent());

  EXPECT_TRUE(delegate_->CapturedMouseEvent(ui::EventType::kMouseReleased));
  EXPECT_EQ(click_location, delegate_->last_mouse_event_location());

  event_capturer_.ClearEvents();
  generator_->ReleaseKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(event_capturer_.LastKeyEvent());
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
  EXPECT_TRUE(delegate_->CapturedMouseEvent(ui::EventType::kMouseDragged));
  EXPECT_FALSE(event_capturer_.LastMouseEvent());
  event_capturer_.ClearEvents();

  generator_->ReleaseLeftButton();
  EXPECT_EQ(drag_location, delegate_->last_mouse_event_location());
  EXPECT_TRUE(delegate_->CapturedMouseEvent(ui::EventType::kMouseReleased));

  generator_->ReleaseKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
}

TEST_F(SelectToSpeakEventHandlerTest, SearchPlusMove) {
  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  gfx::Point initial_mouse_location = gfx::Point(100, 12);
  generator_->set_current_screen_location(initial_mouse_location);

  // Hovers are not passed through.
  gfx::Point move_location = gfx::Point(120, 32);
  generator_->MoveMouseTo(move_location);
  EXPECT_FALSE(event_capturer_.LastMouseEvent());
  event_capturer_.ClearEvents();

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
  EXPECT_TRUE(delegate_->CapturedMouseEvent(ui::EventType::kMouseDragged));
  EXPECT_TRUE(event_capturer_.LastMouseEvent());
  event_capturer_.ClearEvents();

  generator_->ReleaseLeftButton();
  EXPECT_EQ(gfx::Point(drag_location_px.x() / 2, drag_location_px.y() / 2),
            delegate_->last_mouse_event_location());
  EXPECT_TRUE(delegate_->CapturedMouseEvent(ui::EventType::kMouseReleased));

  generator_->ReleaseKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
}

TEST_F(SelectToSpeakEventHandlerTest, RepeatSearchKey) {
  // Holding the Search key may generate key repeat events. Make sure it's
  // still treated as if the search key is down.
  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);

  generator_->set_current_screen_location(gfx::Point(100, 12));
  generator_->PressLeftButton();
  EXPECT_FALSE(event_capturer_.LastMouseEvent());

  EXPECT_TRUE(delegate_->CapturedMouseEvent(ui::EventType::kMousePressed));

  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);

  generator_->ReleaseLeftButton();
  EXPECT_FALSE(event_capturer_.LastMouseEvent());

  EXPECT_TRUE(delegate_->CapturedMouseEvent(ui::EventType::kMouseReleased));

  event_capturer_.ClearEvents();
  generator_->ReleaseKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(event_capturer_.LastKeyEvent());
}

TEST_F(SelectToSpeakEventHandlerTest, TapSearchKey) {
  // Tapping the search key should not steal future events.

  event_capturer_.ClearEvents();
  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  generator_->ReleaseKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  generator_->PressLeftButton();
  EXPECT_FALSE(event_capturer_.LastMouseEvent()->handled());
  generator_->ReleaseLeftButton();
  EXPECT_FALSE(event_capturer_.LastMouseEvent()->handled());
}

TEST_F(SelectToSpeakEventHandlerTest, SearchPlusClickTwice) {
  // Same as SearchPlusClick, above, but test that the user can keep
  // holding down Search and click again.

  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(event_capturer_.LastKeyEvent());
  EXPECT_FALSE(event_capturer_.LastKeyEvent()->handled());

  generator_->set_current_screen_location(gfx::Point(100, 12));
  generator_->PressLeftButton();
  EXPECT_FALSE(event_capturer_.LastMouseEvent());
  EXPECT_TRUE(delegate_->CapturedMouseEvent(ui::EventType::kMousePressed));

  generator_->ReleaseLeftButton();
  EXPECT_FALSE(event_capturer_.LastMouseEvent());
  EXPECT_TRUE(delegate_->CapturedMouseEvent(ui::EventType::kMouseReleased));

  delegate_->Reset();
  EXPECT_FALSE(delegate_->CapturedMouseEvent(ui::EventType::kMousePressed));
  EXPECT_FALSE(delegate_->CapturedMouseEvent(ui::EventType::kMouseReleased));

  generator_->PressLeftButton();
  EXPECT_FALSE(event_capturer_.LastMouseEvent());
  EXPECT_TRUE(delegate_->CapturedMouseEvent(ui::EventType::kMousePressed));

  generator_->ReleaseLeftButton();
  EXPECT_FALSE(event_capturer_.LastMouseEvent());
  EXPECT_TRUE(delegate_->CapturedMouseEvent(ui::EventType::kMouseReleased));

  event_capturer_.ClearEvents();
  generator_->ReleaseKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(event_capturer_.LastKeyEvent());
}

TEST_F(SelectToSpeakEventHandlerTest, SearchPlusKeyIgnoresClicks) {
  // If the user presses the Search key and then some other key
  // besides 's', we should assume the user does not want select-to-speak,
  // and click events should be ignored.

  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(event_capturer_.LastKeyEvent());
  EXPECT_FALSE(event_capturer_.LastKeyEvent()->handled());

  generator_->PressKey(ui::VKEY_I, ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(event_capturer_.LastKeyEvent());
  EXPECT_FALSE(event_capturer_.LastKeyEvent()->handled());

  generator_->set_current_screen_location(gfx::Point(100, 12));
  generator_->PressLeftButton();
  ASSERT_TRUE(event_capturer_.LastMouseEvent());
  EXPECT_FALSE(event_capturer_.LastMouseEvent()->handled());

  EXPECT_FALSE(delegate_->CapturedMouseEvent(ui::EventType::kMousePressed));

  generator_->ReleaseLeftButton();
  ASSERT_TRUE(event_capturer_.LastMouseEvent());
  EXPECT_FALSE(event_capturer_.LastMouseEvent()->handled());

  EXPECT_FALSE(delegate_->CapturedMouseEvent(ui::EventType::kMouseReleased));

  event_capturer_.ClearEvents();
  generator_->ReleaseKey(ui::VKEY_I, ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(event_capturer_.LastKeyEvent());
  EXPECT_FALSE(event_capturer_.LastKeyEvent()->handled());

  event_capturer_.ClearEvents();
  generator_->ReleaseKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(event_capturer_.LastKeyEvent());
  EXPECT_FALSE(event_capturer_.LastKeyEvent()->handled());
}

TEST_F(SelectToSpeakEventHandlerTest, SearchPlusSIsCaptured) {
  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);

  // Press and release S, key presses should be captured.
  event_capturer_.ClearEvents();
  generator_->PressKey(ui::VKEY_S, ui::EF_COMMAND_DOWN);
  ASSERT_FALSE(event_capturer_.LastKeyEvent());

  generator_->ReleaseKey(ui::VKEY_S, ui::EF_COMMAND_DOWN);
  ASSERT_FALSE(event_capturer_.LastKeyEvent());

  // Press and release again while still holding down search.
  // The events should continue to be captured.
  generator_->PressKey(ui::VKEY_S, ui::EF_COMMAND_DOWN);
  ASSERT_FALSE(event_capturer_.LastKeyEvent());

  generator_->ReleaseKey(ui::VKEY_S, ui::EF_COMMAND_DOWN);
  ASSERT_FALSE(event_capturer_.LastKeyEvent());

  generator_->ReleaseKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  ASSERT_FALSE(event_capturer_.LastKeyEvent());

  // S alone is not captured.
  generator_->PressKey(ui::VKEY_S, ui::EF_NONE);
  ASSERT_TRUE(controller_->GetSelectToSpeakEventHandlerForTesting()
                  ->IsKeyDownForTesting(ui::VKEY_S));
  ASSERT_TRUE(event_capturer_.LastKeyEvent());
  ASSERT_FALSE(event_capturer_.LastKeyEvent()->handled());
}

TEST_F(SelectToSpeakEventHandlerTest, SearchPlusSIgnoresMouse) {
  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);

  // Press S
  event_capturer_.ClearEvents();
  generator_->PressKey(ui::VKEY_S, ui::EF_COMMAND_DOWN);
  ASSERT_FALSE(event_capturer_.LastKeyEvent());

  // Mouse events are passed through like normal.
  generator_->PressLeftButton();
  EXPECT_TRUE(event_capturer_.LastMouseEvent());
  event_capturer_.ClearEvents();
  generator_->ReleaseLeftButton();
  EXPECT_TRUE(event_capturer_.LastMouseEvent());

  generator_->ReleaseKey(ui::VKEY_S, ui::EF_COMMAND_DOWN);
  ASSERT_FALSE(event_capturer_.LastKeyEvent());

  generator_->ReleaseKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  ASSERT_FALSE(event_capturer_.LastKeyEvent());
}

TEST_F(SelectToSpeakEventHandlerTest, SearchPlusMouseIgnoresS) {
  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);

  // Press the mouse.
  event_capturer_.ClearEvents();
  generator_->PressLeftButton();
  EXPECT_FALSE(event_capturer_.LastMouseEvent());

  // S key events are passed through like normal.
  generator_->PressKey(ui::VKEY_S, ui::EF_NONE);
  ASSERT_TRUE(controller_->GetSelectToSpeakEventHandlerForTesting()
                  ->IsKeyDownForTesting(ui::VKEY_S));
  EXPECT_TRUE(event_capturer_.LastKeyEvent());
  event_capturer_.ClearEvents();
  generator_->ReleaseKey(ui::VKEY_S, ui::EF_NONE);
  ASSERT_FALSE(controller_->GetSelectToSpeakEventHandlerForTesting()
                   ->IsKeyDownForTesting(ui::VKEY_S));
  EXPECT_TRUE(event_capturer_.LastKeyEvent());

  generator_->ReleaseLeftButton();
  EXPECT_FALSE(event_capturer_.LastMouseEvent());

  event_capturer_.ClearEvents();
  generator_->ReleaseKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(event_capturer_.LastKeyEvent());
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
  EXPECT_FALSE(event_capturer_.LastMouseEvent());

  // This shouldn't cause any changes since the state is not inactive.
  controller_->SetSelectToSpeakState(
      SelectToSpeakState::kSelectToSpeakStateSelecting);

  generator_->ReleaseLeftButton();

  // Releasing the search key is still captured per the end of the search+click
  // mode.
  event_capturer_.ClearEvents();
  generator_->ReleaseKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(event_capturer_.LastKeyEvent());
}

TEST_F(SelectToSpeakEventHandlerTest,
       CancelSearchKeyUpAfterEarlyInactiveStateChange) {
  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  gfx::Point click_location = gfx::Point(100, 12);
  generator_->set_current_screen_location(click_location);
  generator_->PressLeftButton();
  EXPECT_FALSE(event_capturer_.LastMouseEvent());
  EXPECT_TRUE(delegate_->CapturedMouseEvent(ui::EventType::kMousePressed));
  generator_->ReleaseLeftButton();
  EXPECT_FALSE(event_capturer_.LastMouseEvent());
  EXPECT_TRUE(delegate_->CapturedMouseEvent(ui::EventType::kMouseReleased));

  // Set the state to inactive.
  // This is realistic because Select-to-Speak will set the state to inactive
  // after the hittest / search for the focused node callbacks, which may occur
  // before the user actually releases the search key.
  controller_->SetSelectToSpeakState(
      SelectToSpeakState::kSelectToSpeakStateInactive);

  // The search key release should still be captured.
  event_capturer_.ClearEvents();
  generator_->ReleaseKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(event_capturer_.LastKeyEvent());
}

TEST_F(SelectToSpeakEventHandlerTest, PassesCtrlKey) {
  generator_->PressKey(ui::VKEY_CONTROL, /*flags=*/0);
  ASSERT_TRUE(event_capturer_.LastKeyEvent());
  EXPECT_FALSE(event_capturer_.LastKeyEvent()->handled());
  event_capturer_.ClearEvents();
  generator_->ReleaseKey(ui::VKEY_CONTROL, /*flags=*/0);
  EXPECT_TRUE(event_capturer_.LastKeyEvent());
  EXPECT_FALSE(event_capturer_.LastKeyEvent()->handled());
}

TEST_F(SelectToSpeakEventHandlerTest, SelectionRequestedWorksWithMouse) {
  gfx::Point click_location = gfx::Point(100, 12);
  generator_->set_current_screen_location(click_location);

  // Mouse events are let through normally before entering selecting state.
  // Another mouse event is let through normally.
  controller_->SetSelectToSpeakState(
      SelectToSpeakState::kSelectToSpeakStateInactive);
  generator_->PressLeftButton();
  EXPECT_TRUE(event_capturer_.LastMouseEvent());
  event_capturer_.ClearEvents();
  generator_->ReleaseLeftButton();
  EXPECT_TRUE(event_capturer_.LastMouseEvent());
  event_capturer_.ClearEvents();

  // Start selection mode.
  controller_->SetSelectToSpeakState(
      SelectToSpeakState::kSelectToSpeakStateSelecting);

  generator_->PressLeftButton();
  EXPECT_FALSE(event_capturer_.LastMouseEvent());
  EXPECT_TRUE(delegate_->CapturedMouseEvent(ui::EventType::kMousePressed));
  event_capturer_.ClearEvents();

  gfx::Point drag_location = gfx::Point(120, 32);
  generator_->DragMouseTo(drag_location);
  EXPECT_EQ(drag_location, delegate_->last_mouse_event_location());
  EXPECT_TRUE(delegate_->CapturedMouseEvent(ui::EventType::kMouseDragged));
  EXPECT_FALSE(event_capturer_.LastMouseEvent());
  event_capturer_.ClearEvents();

  // Mouse up is the last event captured in the sequence
  generator_->ReleaseLeftButton();
  EXPECT_FALSE(event_capturer_.LastMouseEvent());
  event_capturer_.ClearEvents();

  // Another mouse event is let through normally.
  generator_->PressLeftButton();
  EXPECT_TRUE(event_capturer_.LastMouseEvent());
  event_capturer_.ClearEvents();
}

TEST_F(SelectToSpeakEventHandlerTest, SelectionRequestedWorksWithTouch) {
  gfx::Point touch_location = gfx::Point(100, 12);
  generator_->set_current_screen_location(touch_location);

  // Mouse events are let through normally before entering selecting state.
  // Another mouse event is let through normally.
  controller_->SetSelectToSpeakState(
      SelectToSpeakState::kSelectToSpeakStateInactive);
  generator_->PressTouch();
  EXPECT_TRUE(event_capturer_.LastTouchEvent());
  event_capturer_.ClearEvents();
  generator_->ReleaseTouch();
  EXPECT_TRUE(event_capturer_.LastTouchEvent());
  event_capturer_.ClearEvents();

  // Start selection mode.
  controller_->SetSelectToSpeakState(
      SelectToSpeakState::kSelectToSpeakStateSelecting);

  generator_->PressTouch();
  EXPECT_FALSE(event_capturer_.LastTouchEvent());
  // Touch events are converted to mouse events for the extension.
  EXPECT_TRUE(delegate_->CapturedMouseEvent(ui::EventType::kMousePressed));
  event_capturer_.ClearEvents();

  gfx::Point drag_location = gfx::Point(120, 32);
  generator_->MoveTouch(drag_location);
  EXPECT_EQ(drag_location, delegate_->last_mouse_event_location());
  EXPECT_TRUE(delegate_->CapturedMouseEvent(ui::EventType::kMouseDragged));
  EXPECT_TRUE(event_capturer_.LastTouchEvent());
  event_capturer_.ClearEvents();

  // Touch up is the last event captured in the sequence
  generator_->ReleaseTouch();
  EXPECT_FALSE(event_capturer_.LastTouchEvent());
  EXPECT_TRUE(delegate_->CapturedMouseEvent(ui::EventType::kMouseReleased));
  event_capturer_.ClearEvents();

  // Another touch event is let through normally.
  generator_->PressTouch();
  EXPECT_TRUE(event_capturer_.LastTouchEvent());
  event_capturer_.ClearEvents();
}

TEST_F(SelectToSpeakEventHandlerTest, SelectionRequestedIgnoresOtherInput) {
  // Start selection mode.
  controller_->SetSelectToSpeakState(
      SelectToSpeakState::kSelectToSpeakStateSelecting);

  // Search key events are not impacted.
  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  EXPECT_TRUE(event_capturer_.LastKeyEvent());
  event_capturer_.ClearEvents();
  generator_->ReleaseKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  EXPECT_TRUE(event_capturer_.LastKeyEvent());
  event_capturer_.ClearEvents();

  // Start a touch selection, it should get captured and forwarded.
  generator_->PressTouch();
  EXPECT_FALSE(event_capturer_.LastTouchEvent());
  EXPECT_TRUE(delegate_->CapturedMouseEvent(ui::EventType::kMousePressed));
  event_capturer_.ClearEvents();

  // Mouse event happening during the touch selection are not impacted;
  // we are locked into a touch selection mode.
  generator_->PressLeftButton();
  EXPECT_TRUE(event_capturer_.LastMouseEvent());
  event_capturer_.ClearEvents();
  generator_->ReleaseLeftButton();
  EXPECT_TRUE(event_capturer_.LastMouseEvent());
  event_capturer_.ClearEvents();

  // Complete the touch selection.
  generator_->ReleaseTouch();
  EXPECT_FALSE(event_capturer_.LastTouchEvent());
  event_capturer_.ClearEvents();
}

TEST_F(SelectToSpeakEventHandlerTest, SelectionRequestedPreventsHovers) {
  // Start selection mode.
  controller_->SetSelectToSpeakState(
      SelectToSpeakState::kSelectToSpeakStateSelecting);

  // Set the mouse.
  gfx::Point initial_mouse_location = gfx::Point(100, 12);
  generator_->set_current_screen_location(initial_mouse_location);

  // Hovers are not passed through.
  gfx::Point move_location = gfx::Point(120, 32);
  generator_->MoveMouseTo(move_location);
  EXPECT_FALSE(event_capturer_.LastMouseEvent());
  event_capturer_.ClearEvents();
}

TEST_F(SelectToSpeakEventHandlerTest, TrackingTouchIgnoresOtherTouchPointers) {
  gfx::Point touch_location = gfx::Point(100, 12);
  gfx::Point drag_location = gfx::Point(120, 32);
  generator_->set_current_screen_location(touch_location);
  controller_->SetSelectToSpeakState(
      SelectToSpeakState::kSelectToSpeakStateSelecting);

  // The first touch event is captured and sent to the extension.
  generator_->PressTouchId(1);
  EXPECT_FALSE(event_capturer_.LastTouchEvent());
  EXPECT_TRUE(delegate_->CapturedMouseEvent(ui::EventType::kMousePressed));
  event_capturer_.ClearEvents();
  delegate_->Reset();

  // A second touch event up and down is canceled but not sent to the extension.
  generator_->PressTouchId(2);
  EXPECT_FALSE(event_capturer_.LastTouchEvent());
  EXPECT_FALSE(delegate_->CapturedMouseEvent(ui::EventType::kMousePressed));
  generator_->MoveTouchId(drag_location, 2);
  EXPECT_FALSE(event_capturer_.LastTouchEvent());
  EXPECT_FALSE(delegate_->CapturedMouseEvent(ui::EventType::kMouseDragged));
  generator_->ReleaseTouchId(2);
  EXPECT_FALSE(event_capturer_.LastTouchEvent());
  EXPECT_FALSE(delegate_->CapturedMouseEvent(ui::EventType::kMouseReleased));

  // A pointer type event will not be sent either, as we are tracking touch,
  // even if the ID is the same.
  generator_->EnterPenPointerMode();
  generator_->PressTouchId(1);
  EXPECT_FALSE(event_capturer_.LastTouchEvent());
  EXPECT_FALSE(delegate_->CapturedMouseEvent(ui::EventType::kMousePressed));
  generator_->ExitPenPointerMode();

  // The first pointer is still tracked.
  generator_->MoveTouchId(drag_location, 1);
  EXPECT_EQ(drag_location, delegate_->last_mouse_event_location());
  EXPECT_TRUE(delegate_->CapturedMouseEvent(ui::EventType::kMouseDragged));
  EXPECT_TRUE(event_capturer_.LastTouchEvent());
  event_capturer_.ClearEvents();

  // Touch up is the last event captured in the sequence
  generator_->ReleaseTouchId(1);
  EXPECT_FALSE(event_capturer_.LastTouchEvent());
  event_capturer_.ClearEvents();

  // Another touch event is let through normally.
  generator_->PressTouchId(3);
  EXPECT_TRUE(event_capturer_.LastTouchEvent());
  event_capturer_.ClearEvents();
}

TEST_F(SelectToSpeakEventHandlerTest, TouchFirstOfMultipleDisplays) {
  UpdateDisplay("1+0-800x700,801+1-800x700");

  // On the first display.
  gfx::Point touch_location(200, 200);
  generator_->set_current_screen_location(touch_location);
  controller_->SetSelectToSpeakState(
      SelectToSpeakState::kSelectToSpeakStateSelecting);
  generator_->PressTouch();
  EXPECT_EQ(touch_location, delegate_->last_mouse_event_root_location());
}

TEST_F(SelectToSpeakEventHandlerTest, TouchSecondOfMultipleDisplays) {
  UpdateDisplay("1+0-800x700,801+1-800x700");

  // On the second display.
  gfx::Point touch_location(1000, 200);
  generator_->set_current_screen_location(touch_location);
  controller_->SetSelectToSpeakState(
      SelectToSpeakState::kSelectToSpeakStateSelecting);
  generator_->PressTouch();
  EXPECT_EQ(touch_location, delegate_->last_mouse_event_root_location());
}

}  // namespace
}  // namespace ash
