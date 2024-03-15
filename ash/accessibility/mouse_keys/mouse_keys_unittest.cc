// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/mouse_keys/mouse_keys_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/manager/display_manager.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/types/event_type.h"

namespace ash {

// TODO(259372916): Add tests to verify interactions with other A11y features.
// TODO(259372916): Add tests to toggle from Pref.
// TODO(259372916): Add tests for multiple screens.
// TODO(259372916): Add tests different DPIs.
// TODO(259372916): Add tests to verify cursor movement.

namespace {

const int kMouseDeviceId = 42;
const gfx::Point kDefaultPosition(100, 100);

class EventCapturer : public ui::EventHandler {
 public:
  EventCapturer() { Reset(); }

  EventCapturer(const EventCapturer&) = delete;
  EventCapturer& operator=(const EventCapturer&) = delete;

  ~EventCapturer() override = default;

  void Reset() {
    key_events_.clear();
    mouse_events_.clear();
  }

  void OnKeyEventRewrite(const ui::KeyEvent* event) {}

  void OnKeyEvent(ui::KeyEvent* event) override {
    key_events_.push_back(*event);

    // If there is a possibility that we're in an infinite loop, we should
    // exit early with a sensible error rather than letting the test time out.
    ASSERT_LT(key_events_.size(), 100u);
  }

  void OnMouseEvent(ui::MouseEvent* event) override {
    // Filter out extraneous mouse events like mouse entered, exited,
    // capture changed, etc.
    ui::EventType type = event->type();
    if (type == ui::ET_MOUSE_PRESSED || type == ui::ET_MOUSE_RELEASED ||
        type == ui::ET_MOUSE_MOVED) {
      mouse_events_.push_back(
          ui::MouseEvent(event->type(), event->location(),
                         event->root_location(), ui::EventTimeForNow(),
                         event->flags(), event->changed_button_flags()));
      event->StopPropagation();

      // If there is a possibility that we're in an infinite loop, we should
      // exit early with a sensible error rather than letting the test time out.
      ASSERT_LT(mouse_events_.size(), 100u);
    }
  }

  const std::vector<ui::KeyEvent>& key_events() const { return key_events_; }
  const std::vector<ui::MouseEvent>& mouse_events() const {
    return mouse_events_;
  }

 private:
  std::vector<ui::KeyEvent> key_events_;
  std::vector<ui::MouseEvent> mouse_events_;
};

class EventRewriterWrapper : public ui::EventRewriter {
 public:
  EventRewriterWrapper() = default;

  EventRewriterWrapper(const EventRewriterWrapper&) = delete;
  EventRewriterWrapper& operator=(const EventRewriterWrapper&) = delete;

  ~EventRewriterWrapper() override = default;
  ui::EventDispatchDetails RewriteEvent(
      const ui::Event& event,
      const Continuation continuation) override {
    bool captured = Shell::Get()->mouse_keys_controller()->RewriteEvent(event);
    return captured ? DiscardEvent(continuation)
                    : SendEvent(continuation, &event);
  }
};

class MouseKeysTest : public AshTestBase {
 protected:
  MouseKeysTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  MouseKeysTest(const MouseKeysTest&) = delete;
  MouseKeysTest& operator=(const MouseKeysTest&) = delete;
  ~MouseKeysTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        ::features::kAccessibilityMouseKeys);
    AshTestBase::SetUp();
    GetContext()->GetHost()->GetEventSource()->AddEventRewriter(&rewriter_);
    GetContext()->AddPreTargetHandler(&event_capturer_);

    // Set a device id so mouse events aren't ignored by the controller.
    GetEventGenerator()->set_mouse_source_device_id(kMouseDeviceId);

    // Make sure the display is initialized so we don't fail the test due to any
    // input events caused from creating the display.
    Shell::Get()->display_manager()->UpdateDisplays();
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    GetContext()->RemovePreTargetHandler(&event_capturer_);
    GetContext()->GetHost()->GetEventSource()->RemoveEventRewriter(&rewriter_);
    AshTestBase::TearDown();
  }

  const std::vector<ui::KeyEvent>& CheckForKeyEvents() {
    base::RunLoop().RunUntilIdle();
    return event_capturer_.key_events();
  }

  const std::vector<ui::MouseEvent>& CheckForMouseEvents() {
    base::RunLoop().RunUntilIdle();
    return event_capturer_.mouse_events();
  }

  void ExpectMouseMovedInCircularPattern(
      const std::vector<ui::MouseEvent>& mouse_events,
      const gfx::Point& starting_position,
      double delta) {
    EXPECT_EQ(8u, mouse_events.size());
    if (mouse_events.size() != 8u) {
      return;
    }

    // There should be 8 mouse movements.
    for (int i = 0; i < 8; ++i) {
      EXPECT_EQ(ui::ET_MOUSE_MOVED, mouse_events[i].type());
    }

    // The pointer should move in a circular pattern.
    auto position = starting_position + gfx::Vector2d(-delta, -delta);
    EXPECT_EQ(mouse_events[0].location(), position);
    position += gfx::Vector2d(0, -delta);
    EXPECT_EQ(mouse_events[1].location(), position);
    position += gfx::Vector2d(delta, -delta);
    EXPECT_EQ(mouse_events[2].location(), position);
    position += gfx::Vector2d(-delta, 0);
    EXPECT_EQ(mouse_events[3].location(), position);
    position += gfx::Vector2d(delta, 0);
    EXPECT_EQ(mouse_events[4].location(), position);
    position += gfx::Vector2d(-delta, delta);
    EXPECT_EQ(mouse_events[5].location(), position);
    position += gfx::Vector2d(0, delta);
    EXPECT_EQ(mouse_events[6].location(), position);
    position += gfx::Vector2d(delta, delta);
    EXPECT_EQ(mouse_events[7].location(), position);
  }

  MouseKeysController* GetMouseKeysController() {
    return Shell::Get()->mouse_keys_controller();
  }

  void ClearEvents() { event_capturer_.Reset(); }

  void PressAndReleaseKey(ui::KeyboardCode key_code) {
    GetEventGenerator()->PressAndReleaseKey(key_code);
    base::RunLoop().RunUntilIdle();
  }

  ui::KeyEvent ColemakKeyEvent(ui::EventType type, ui::KeyboardCode key_code) {
    switch (key_code) {
      case ui::VKEY_7:
        return ui::KeyEvent(type, key_code, ui::DomCode::DIGIT7, 0);
      case ui::VKEY_8:
        return ui::KeyEvent(type, key_code, ui::DomCode::DIGIT8, 0);
      case ui::VKEY_9:
        return ui::KeyEvent(type, key_code, ui::DomCode::DIGIT9, 0);
      case ui::VKEY_L:
        return ui::KeyEvent(type, key_code, ui::DomCode::US_U, 0);
      case ui::VKEY_Y:
        return ui::KeyEvent(type, key_code, ui::DomCode::US_O, 0);
      case ui::VKEY_N:
        return ui::KeyEvent(type, key_code, ui::DomCode::US_J, 0);
      case ui::VKEY_E:
        return ui::KeyEvent(type, key_code, ui::DomCode::US_K, 0);
      case ui::VKEY_I:
        return ui::KeyEvent(type, key_code, ui::DomCode::US_L, 0);
      case ui::VKEY_U:
        return ui::KeyEvent(type, key_code, ui::DomCode::US_I, 0);
      case ui::VKEY_O:
        return ui::KeyEvent(type, key_code, ui::DomCode::SEMICOLON, 0);
      case ui::VKEY_J:
        return ui::KeyEvent(type, key_code, ui::DomCode::US_Y, 0);
      case ui::VKEY_K:
        return ui::KeyEvent(type, key_code, ui::DomCode::US_N, 0);
      default:
        return ui::KeyEvent(type, ui::VKEY_UNKNOWN, 0, ui::EventTimeForNow());
    }
  }

  void PressColemakKey(ui::KeyboardCode key_code) {
    ui::KeyEvent key_event(ColemakKeyEvent(ui::ET_KEY_PRESSED, key_code));
    GetEventGenerator()->Dispatch(&key_event);
  }

  void ReleaseColemakKey(ui::KeyboardCode key_code) {
    ui::KeyEvent key_event(ColemakKeyEvent(ui::ET_KEY_RELEASED, key_code));
    GetEventGenerator()->Dispatch(&key_event);
  }

  void PressAndReleaseColemakKey(ui::KeyboardCode key_code) {
    PressColemakKey(key_code);
    ReleaseColemakKey(key_code);
    base::RunLoop().RunUntilIdle();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  EventCapturer event_capturer_;
  EventRewriterWrapper rewriter_;
};

}  // namespace

TEST_F(MouseKeysTest, ToggleEnabled) {
  std::vector<ui::MouseEvent> events;

  // We should not see any events.
  ClearEvents();
  EXPECT_FALSE(GetMouseKeysController()->IsEnabled());
  events = CheckForMouseEvents();
  EXPECT_EQ(0u, events.size());

  // Enable Mouse Keys.
  ClearEvents();
  GetMouseKeysController()->SetEnabled(true);
  EXPECT_TRUE(GetMouseKeysController()->IsEnabled());

  // We should still not get any more events.
  events = CheckForMouseEvents();
  EXPECT_EQ(0u, events.size());

  // Disable Mouse Keys.
  GetMouseKeysController()->SetEnabled(false);
  EXPECT_FALSE(GetMouseKeysController()->IsEnabled());
}

TEST_F(MouseKeysTest, Events) {
  // We should not see any mouse events initially, and key events should be
  // passed through.
  ClearEvents();
  EXPECT_FALSE(GetMouseKeysController()->IsEnabled());
  PressAndReleaseKey(ui::VKEY_I);
  EXPECT_EQ(0u, CheckForMouseEvents().size());
  EXPECT_EQ(2u, CheckForKeyEvents().size());

  // Enable Mouse Keys, the key events should be absorbed.
  ClearEvents();
  GetMouseKeysController()->SetEnabled(true);
  EXPECT_TRUE(GetMouseKeysController()->IsEnabled());
  PressAndReleaseKey(ui::VKEY_I);
  EXPECT_EQ(2u, CheckForMouseEvents().size());
  EXPECT_EQ(0u, CheckForKeyEvents().size());

  // We should not get any more events.
  ClearEvents();
  EXPECT_EQ(0u, CheckForMouseEvents().size());
  EXPECT_EQ(0u, CheckForKeyEvents().size());

  // Disable Mouse Keys, and we should see the original behaviour.
  ClearEvents();
  GetMouseKeysController()->SetEnabled(false);
  EXPECT_FALSE(GetMouseKeysController()->IsEnabled());
  PressAndReleaseKey(ui::VKEY_I);
  EXPECT_EQ(0u, CheckForMouseEvents().size());
  EXPECT_EQ(2u, CheckForKeyEvents().size());
}

TEST_F(MouseKeysTest, Click) {
  GetEventGenerator()->MoveMouseToWithNative(kDefaultPosition,
                                             kDefaultPosition);

  // We should not see any mouse events initially.
  ClearEvents();
  EXPECT_FALSE(GetMouseKeysController()->IsEnabled());
  PressAndReleaseKey(ui::VKEY_I);
  EXPECT_EQ(0u, CheckForMouseEvents().size());

  // Enable Mouse Keys, and we should be able to click by pressing i.
  ClearEvents();
  GetMouseKeysController()->SetEnabled(true);
  EXPECT_TRUE(GetMouseKeysController()->IsEnabled());
  PressAndReleaseKey(ui::VKEY_I);
  auto mouse_events = CheckForMouseEvents();
  ASSERT_EQ(2u, mouse_events.size());
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, mouse_events[0].type());
  EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & mouse_events[0].flags());
  EXPECT_EQ(mouse_events[0].location(), kDefaultPosition);
  EXPECT_EQ(ui::ET_MOUSE_RELEASED, mouse_events[1].type());
  EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & mouse_events[1].flags());
  EXPECT_EQ(mouse_events[1].location(), kDefaultPosition);

  // We should not get any more events.
  ClearEvents();
  EXPECT_EQ(0u, CheckForMouseEvents().size());

  // Disable Mouse Keys, and we should see the original behaviour.
  ClearEvents();
  GetMouseKeysController()->SetEnabled(false);
  EXPECT_FALSE(GetMouseKeysController()->IsEnabled());
  PressAndReleaseKey(ui::VKEY_I);
  EXPECT_EQ(0u, CheckForMouseEvents().size());
}

TEST_F(MouseKeysTest, Move) {
  GetEventGenerator()->MoveMouseToWithNative(kDefaultPosition,
                                             kDefaultPosition);

  // We should not see any mouse events initially.
  ClearEvents();
  EXPECT_FALSE(GetMouseKeysController()->IsEnabled());
  PressAndReleaseKey(ui::VKEY_7);
  PressAndReleaseKey(ui::VKEY_8);
  PressAndReleaseKey(ui::VKEY_9);
  PressAndReleaseKey(ui::VKEY_U);
  PressAndReleaseKey(ui::VKEY_O);
  PressAndReleaseKey(ui::VKEY_J);
  PressAndReleaseKey(ui::VKEY_K);
  PressAndReleaseKey(ui::VKEY_L);
  EXPECT_EQ(0u, CheckForMouseEvents().size());
  EXPECT_EQ(16u, CheckForKeyEvents().size());

  // Enable Mouse Keys, and we should be able to move the mouse with 7, 8, 9, u,
  // o, j, k, l.
  ClearEvents();
  GetMouseKeysController()->SetEnabled(true);
  EXPECT_TRUE(GetMouseKeysController()->IsEnabled());
  PressAndReleaseKey(ui::VKEY_7);
  PressAndReleaseKey(ui::VKEY_8);
  PressAndReleaseKey(ui::VKEY_9);
  PressAndReleaseKey(ui::VKEY_U);
  PressAndReleaseKey(ui::VKEY_O);
  PressAndReleaseKey(ui::VKEY_J);
  PressAndReleaseKey(ui::VKEY_K);
  PressAndReleaseKey(ui::VKEY_L);
  auto mouse_events = CheckForMouseEvents();
  EXPECT_EQ(0u, CheckForKeyEvents().size());

  ExpectMouseMovedInCircularPattern(mouse_events, kDefaultPosition,
                                    MouseKeysController::kMoveDeltaDIP);

  // We should not get any more events.
  ClearEvents();
  EXPECT_EQ(0u, CheckForMouseEvents().size());

  // Disable Mouse Keys, and we should see the original behaviour.
  ClearEvents();
  GetMouseKeysController()->SetEnabled(false);
  EXPECT_FALSE(GetMouseKeysController()->IsEnabled());
  PressAndReleaseKey(ui::VKEY_7);
  PressAndReleaseKey(ui::VKEY_8);
  PressAndReleaseKey(ui::VKEY_9);
  PressAndReleaseKey(ui::VKEY_U);
  PressAndReleaseKey(ui::VKEY_O);
  PressAndReleaseKey(ui::VKEY_J);
  PressAndReleaseKey(ui::VKEY_K);
  PressAndReleaseKey(ui::VKEY_L);
  EXPECT_EQ(0u, CheckForMouseEvents().size());
  EXPECT_EQ(16u, CheckForKeyEvents().size());
}

TEST_F(MouseKeysTest, KeyboardLayout) {
  GetEventGenerator()->MoveMouseToWithNative(kDefaultPosition,
                                             kDefaultPosition);

  // Enable Mouse Keys, and we should be able to move the mouse with 7, 8, 9, k,
  // y, n, e, i.
  ClearEvents();
  GetMouseKeysController()->SetEnabled(true);
  EXPECT_TRUE(GetMouseKeysController()->IsEnabled());
  PressAndReleaseColemakKey(ui::VKEY_7);
  PressAndReleaseColemakKey(ui::VKEY_8);
  PressAndReleaseColemakKey(ui::VKEY_9);
  PressAndReleaseColemakKey(ui::VKEY_L);
  PressAndReleaseColemakKey(ui::VKEY_Y);
  PressAndReleaseColemakKey(ui::VKEY_N);
  PressAndReleaseColemakKey(ui::VKEY_E);
  PressAndReleaseColemakKey(ui::VKEY_I);
  auto mouse_events = CheckForMouseEvents();
  EXPECT_EQ(0u, CheckForKeyEvents().size());

  ExpectMouseMovedInCircularPattern(mouse_events, kDefaultPosition,
                                    MouseKeysController::kMoveDeltaDIP);

  ClearEvents();
  // Click
  PressAndReleaseColemakKey(ui::VKEY_U);

  mouse_events = CheckForMouseEvents();
  EXPECT_EQ(0u, CheckForKeyEvents().size());
  ASSERT_EQ(2u, mouse_events.size());
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, mouse_events[0].type());
  EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & mouse_events[0].flags());
  EXPECT_EQ(mouse_events[0].location(), kDefaultPosition);
  EXPECT_EQ(ui::ET_MOUSE_RELEASED, mouse_events[1].type());
  EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & mouse_events[1].flags());
  EXPECT_EQ(mouse_events[1].location(), kDefaultPosition);

  ClearEvents();
  // Unmapped
  PressAndReleaseColemakKey(ui::VKEY_O);
  PressAndReleaseColemakKey(ui::VKEY_J);
  PressAndReleaseColemakKey(ui::VKEY_K);
  EXPECT_EQ(0u, CheckForMouseEvents().size());
  EXPECT_EQ(6u, CheckForKeyEvents().size());
}
}  // namespace ash
