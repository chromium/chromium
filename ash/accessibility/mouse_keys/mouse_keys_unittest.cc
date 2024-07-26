// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/mouse_keys/mouse_keys_controller.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/events/test_event_capturer.h"
#include "ash/session/session_controller_impl.h"
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
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

// TODO(259372916): Add tests to verify interactions with other A11y features.
// TODO(259372916): Add tests for multiple screens.
// TODO(259372916): Add tests different DPIs.
// TODO(259372916): Add test to check holding down multiple movement keys.

namespace {

const int kMouseDeviceId = 42;
const gfx::Point kDefaultPosition(100, 100);
const double kMoveDeltaDIP = MouseKeysController::kBaseSpeedDIPPerSecond *
                             MouseKeysController::kUpdateFrequencyInSeconds;

class TestTextInputView : public views::WidgetDelegateView {
 public:
  TestTextInputView() : text_field_(new views::Textfield) {
    text_field_->SetTextInputType(ui::TEXT_INPUT_TYPE_TEXT);
    std::string name = "Hello, world";
    text_field_->GetViewAccessibility().SetName(base::UTF8ToUTF16(name));
    AddChildView(text_field_.get());
    SetLayoutManager(std::make_unique<views::FillLayout>());
  }
  TestTextInputView(const TestTextInputView&) = delete;
  TestTextInputView& operator=(const TestTextInputView&) = delete;
  ~TestTextInputView() override = default;

  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    return gfx::Size(50, 50);
  }

  void FocusOnTextInput() { text_field_->RequestFocus(); }
  const std::u16string& GetText() { return text_field_->GetText(); }

 private:
  raw_ptr<views::Textfield> text_field_;  // owned by views hierarchy.
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
    event_capturer_.set_capture_mouse_enter_exit(false);
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

  void SetEnabled(bool enabled) {
    Shell::Get()->accessibility_controller()->mouse_keys().SetEnabled(enabled);
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
      EXPECT_EQ(ui::EventType::kMouseMoved, mouse_events[i].type());
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

  void ExpectClick(const std::vector<ui::MouseEvent>& mouse_events,
                   int buttons,
                   const gfx::Point& position) {
    ASSERT_EQ(2u, mouse_events.size());
    ExpectClick(mouse_events[0], mouse_events[1], buttons, false, position);
  }

  void ExpectDoubleClick(const std::vector<ui::MouseEvent>& mouse_events,
                         const gfx::Point& position) {
    ASSERT_EQ(4u, mouse_events.size());
    ExpectClick(mouse_events[0], mouse_events[1], ui::EF_LEFT_MOUSE_BUTTON,
                false, position);
    ExpectClick(mouse_events[2], mouse_events[3], ui::EF_LEFT_MOUSE_BUTTON,
                true, position);
  }

  void ExpectClick(const ui::MouseEvent& mouse_event0,
                   const ui::MouseEvent& mouse_event1,
                   int buttons,
                   bool is_double_click,
                   const gfx::Point& position) {
    EXPECT_EQ(ui::EventType::kMousePressed, mouse_event0.type());
    EXPECT_TRUE(buttons & mouse_event0.flags());
    EXPECT_EQ(mouse_event0.location(), position);
    EXPECT_EQ(ui::EventType::kMouseReleased, mouse_event1.type());
    EXPECT_TRUE(buttons & mouse_event1.flags());
    EXPECT_EQ(mouse_event1.location(), position);
    if (is_double_click) {
      EXPECT_TRUE(ui::EF_IS_DOUBLE_CLICK & mouse_event0.flags());
      EXPECT_TRUE(ui::EF_IS_DOUBLE_CLICK & mouse_event1.flags());
    } else {
      EXPECT_FALSE(ui::EF_IS_DOUBLE_CLICK & mouse_event0.flags());
      EXPECT_FALSE(ui::EF_IS_DOUBLE_CLICK & mouse_event1.flags());
    }
  }

  MouseKeysController* GetMouseKeysController() {
    return Shell::Get()->mouse_keys_controller();
  }

  void SetUsePrimaryKeys(bool value) {
    PrefService* prefs =
        Shell::Get()->session_controller()->GetLastActiveUserPrefService();

    prefs->SetBoolean(prefs::kAccessibilityMouseKeysUsePrimaryKeys, value);
  }

  void SetLeftHanded(bool value) {
    PrefService* prefs =
        Shell::Get()->session_controller()->GetLastActiveUserPrefService();

    MouseKeysDominantHand dominant_hand =
        value ? MouseKeysDominantHand::kLeftHandDominant
              : MouseKeysDominantHand::kRightHandDominant;
    prefs->SetInteger(prefs::kAccessibilityMouseKeysDominantHand,
                      static_cast<int>(dominant_hand));
  }

  void ClearEvents() { event_capturer_.ClearEvents(); }

  void PressKey(ui::KeyboardCode key_code, int flags = 0) {
    GetEventGenerator()->PressKey(key_code, flags);
    base::RunLoop().RunUntilIdle();
  }

  void ReleaseKey(ui::KeyboardCode key_code, int flags = 0) {
    GetEventGenerator()->ReleaseKey(key_code, flags);
    base::RunLoop().RunUntilIdle();
  }

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
    ui::KeyEvent key_event(
        ColemakKeyEvent(ui::EventType::kKeyPressed, key_code));
    GetEventGenerator()->Dispatch(&key_event);
  }

  void ReleaseColemakKey(ui::KeyboardCode key_code) {
    ui::KeyEvent key_event(
        ColemakKeyEvent(ui::EventType::kKeyReleased, key_code));
    GetEventGenerator()->Dispatch(&key_event);
  }

  void PressAndReleaseColemakKey(ui::KeyboardCode key_code) {
    PressColemakKey(key_code);
    ReleaseColemakKey(key_code);
    base::RunLoop().RunUntilIdle();
  }

  void SetAcceleration(double acceleration) {
    PrefService* prefs =
        Shell::Get()->session_controller()->GetLastActiveUserPrefService();

    prefs->SetDouble(prefs::kAccessibilityMouseKeysAcceleration, acceleration);
  }

  void SetMaxSpeed(double factor) {
    PrefService* prefs =
        Shell::Get()->session_controller()->GetLastActiveUserPrefService();

    prefs->SetDouble(prefs::kAccessibilityMouseKeysMaxSpeed, factor);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  TestEventCapturer event_capturer_;
  EventRewriterWrapper rewriter_;
};

}  // namespace

TEST_F(MouseKeysTest, ToggleEnabled) {
  std::vector<ui::MouseEvent> events;

  // We should not see any events.
  ClearEvents();
  EXPECT_FALSE(GetMouseKeysController()->enabled());
  events = CheckForMouseEvents();
  EXPECT_EQ(0u, events.size());

  // Enable Mouse Keys.
  ClearEvents();
  SetEnabled(true);
  EXPECT_TRUE(GetMouseKeysController()->enabled());

  // We should still not get any more events.
  events = CheckForMouseEvents();
  EXPECT_EQ(0u, events.size());

  // Disable Mouse Keys.
  SetEnabled(false);
  EXPECT_FALSE(GetMouseKeysController()->enabled());
}

TEST_F(MouseKeysTest, Events) {
  // We should not see any mouse events initially, and key events should be
  // passed through.
  ClearEvents();
  EXPECT_FALSE(GetMouseKeysController()->enabled());
  PressAndReleaseKey(ui::VKEY_I);
  EXPECT_EQ(0u, CheckForMouseEvents().size());
  EXPECT_EQ(2u, CheckForKeyEvents().size());

  // Enable Mouse Keys, the key events should be absorbed.
  ClearEvents();
  SetEnabled(true);
  EXPECT_TRUE(GetMouseKeysController()->enabled());
  PressAndReleaseKey(ui::VKEY_I);
  EXPECT_EQ(2u, CheckForMouseEvents().size());
  EXPECT_EQ(0u, CheckForKeyEvents().size());

  // We should not get any more events.
  ClearEvents();
  EXPECT_EQ(0u, CheckForMouseEvents().size());
  EXPECT_EQ(0u, CheckForKeyEvents().size());

  // Disable Mouse Keys, and we should see the original behaviour.
  ClearEvents();
  SetEnabled(false);
  EXPECT_FALSE(GetMouseKeysController()->enabled());
  PressAndReleaseKey(ui::VKEY_I);
  EXPECT_EQ(0u, CheckForMouseEvents().size());
  EXPECT_EQ(2u, CheckForKeyEvents().size());
}

TEST_F(MouseKeysTest, Click) {
  GetEventGenerator()->MoveMouseToWithNative(kDefaultPosition,
                                             kDefaultPosition);

  // We should not see any mouse events initially.
  ClearEvents();
  EXPECT_FALSE(GetMouseKeysController()->enabled());
  PressAndReleaseKey(ui::VKEY_I);
  EXPECT_EQ(0u, CheckForMouseEvents().size());
  EXPECT_EQ(2u, CheckForKeyEvents().size());

  // Enable Mouse Keys, and we should be able to click by pressing i.
  ClearEvents();
  SetEnabled(true);
  EXPECT_TRUE(GetMouseKeysController()->enabled());
  PressAndReleaseKey(ui::VKEY_I);
  EXPECT_EQ(0u, CheckForKeyEvents().size());
  ExpectClick(CheckForMouseEvents(), ui::EF_LEFT_MOUSE_BUTTON,
              kDefaultPosition);

  // We should not get any more events.
  ClearEvents();
  EXPECT_EQ(0u, CheckForMouseEvents().size());

  // Disable Mouse Keys, and we should see the original behaviour.
  ClearEvents();
  SetEnabled(false);
  EXPECT_FALSE(GetMouseKeysController()->enabled());
  PressAndReleaseKey(ui::VKEY_I);
  EXPECT_EQ(0u, CheckForMouseEvents().size());
}

TEST_F(MouseKeysTest, DoubleClick) {
  SetEnabled(true);
  GetEventGenerator()->MoveMouseToWithNative(kDefaultPosition,
                                             kDefaultPosition);

  // Enable Mouse Keys, and we should be able to double click by pressing /.
  ClearEvents();
  PressAndReleaseKey(ui::VKEY_OEM_2);
  EXPECT_EQ(0u, CheckForKeyEvents().size());
  ExpectDoubleClick(CheckForMouseEvents(), kDefaultPosition);

  // Switch to right mouse button, we shouldn't get a double click.
  PressAndReleaseKey(ui::VKEY_OEM_COMMA);
  ClearEvents();
  PressAndReleaseKey(ui::VKEY_OEM_2);
  EXPECT_EQ(0u, CheckForKeyEvents().size());
  EXPECT_EQ(0u, CheckForMouseEvents().size());

  // Switch to both mouse buttons, we shouldn't get a double click.
  PressAndReleaseKey(ui::VKEY_OEM_COMMA);
  ClearEvents();
  PressAndReleaseKey(ui::VKEY_OEM_2);
  EXPECT_EQ(0u, CheckForKeyEvents().size());
  EXPECT_EQ(0u, CheckForMouseEvents().size());
}

TEST_F(MouseKeysTest, SelectButtonRightHand) {
  GetEventGenerator()->MoveMouseToWithNative(kDefaultPosition,
                                             kDefaultPosition);
  SetEnabled(true);
  SetLeftHanded(false);

  // Initial click should be the left button.
  ClearEvents();
  PressAndReleaseKey(ui::VKEY_I);
  EXPECT_EQ(0u, CheckForKeyEvents().size());
  ExpectClick(CheckForMouseEvents(), ui::EF_LEFT_MOUSE_BUTTON,
              kDefaultPosition);

  // Press , and the mouse action should be the right button.
  ClearEvents();
  PressAndReleaseKey(ui::VKEY_OEM_COMMA);
  PressAndReleaseKey(ui::VKEY_I);
  EXPECT_EQ(0u, CheckForKeyEvents().size());
  ExpectClick(CheckForMouseEvents(), ui::EF_RIGHT_MOUSE_BUTTON,
              kDefaultPosition);

  // Press , and the mouse action should be both buttons.
  ClearEvents();
  PressAndReleaseKey(ui::VKEY_OEM_COMMA);
  PressAndReleaseKey(ui::VKEY_I);
  EXPECT_EQ(0u, CheckForKeyEvents().size());
  ExpectClick(CheckForMouseEvents(),
              ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON,
              kDefaultPosition);

  // Press , and the mouse action should be the left button.
  ClearEvents();
  PressAndReleaseKey(ui::VKEY_OEM_COMMA);
  PressAndReleaseKey(ui::VKEY_I);
  EXPECT_EQ(0u, CheckForKeyEvents().size());
  ExpectClick(CheckForMouseEvents(), ui::EF_LEFT_MOUSE_BUTTON,
              kDefaultPosition);
}

TEST_F(MouseKeysTest, SelectButtonLeftHand) {
  GetEventGenerator()->MoveMouseToWithNative(kDefaultPosition,
                                             kDefaultPosition);
  SetEnabled(true);
  SetLeftHanded(true);

  // Initial click should be the left button.
  ClearEvents();
  PressAndReleaseKey(ui::VKEY_W);
  EXPECT_EQ(0u, CheckForKeyEvents().size());
  ExpectClick(CheckForMouseEvents(), ui::EF_LEFT_MOUSE_BUTTON,
              kDefaultPosition);

  // Press , and the mouse action should be the right button.
  ClearEvents();
  PressAndReleaseKey(ui::VKEY_X);
  PressAndReleaseKey(ui::VKEY_W);
  EXPECT_EQ(0u, CheckForKeyEvents().size());
  ExpectClick(CheckForMouseEvents(), ui::EF_RIGHT_MOUSE_BUTTON,
              kDefaultPosition);

  // Press , and the mouse action should be both buttons.
  ClearEvents();
  PressAndReleaseKey(ui::VKEY_W);
  PressAndReleaseKey(ui::VKEY_X);
  EXPECT_EQ(0u, CheckForKeyEvents().size());
  ExpectClick(CheckForMouseEvents(),
              ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON,
              kDefaultPosition);

  // Press , and the mouse action should be the left button.
  ClearEvents();
  PressAndReleaseKey(ui::VKEY_W);
  PressAndReleaseKey(ui::VKEY_X);
  EXPECT_EQ(0u, CheckForKeyEvents().size());
  ExpectClick(CheckForMouseEvents(), ui::EF_LEFT_MOUSE_BUTTON,
              kDefaultPosition);
}

TEST_F(MouseKeysTest, SelectButtonNumPad) {
  GetEventGenerator()->MoveMouseToWithNative(kDefaultPosition,
                                             kDefaultPosition);
  SetEnabled(true);

  SetUsePrimaryKeys(false);

  // Press - and the mouse action should be the right button.
  ClearEvents();
  PressAndReleaseKey(ui::VKEY_SUBTRACT);
  PressAndReleaseKey(ui::VKEY_NUMPAD5);
  EXPECT_EQ(0u, CheckForKeyEvents().size());
  ExpectClick(CheckForMouseEvents(), ui::EF_RIGHT_MOUSE_BUTTON,
              kDefaultPosition);

  // Press * and the mouse action should be both buttons.
  ClearEvents();
  PressAndReleaseKey(ui::VKEY_MULTIPLY);
  PressAndReleaseKey(ui::VKEY_NUMPAD5);
  EXPECT_EQ(0u, CheckForKeyEvents().size());
  ExpectClick(CheckForMouseEvents(),
              ui::EF_LEFT_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON,
              kDefaultPosition);

  // Press / and the mouse action should be the left button.
  ClearEvents();
  PressAndReleaseKey(ui::VKEY_DIVIDE);
  PressAndReleaseKey(ui::VKEY_NUMPAD5);
  EXPECT_EQ(0u, CheckForKeyEvents().size());
  ExpectClick(CheckForMouseEvents(), ui::EF_LEFT_MOUSE_BUTTON,
              kDefaultPosition);
}

TEST_F(MouseKeysTest, IgnoreKeyRepeat) {
  GetEventGenerator()->MoveMouseToWithNative(kDefaultPosition,
                                             kDefaultPosition);

  // Enable Mouse Keys, and we should be able to click by pressing i.
  ClearEvents();
  SetEnabled(true);
  EXPECT_TRUE(GetMouseKeysController()->enabled());
  PressKey(ui::VKEY_I);
  auto mouse_events = CheckForMouseEvents();
  ASSERT_EQ(1u, mouse_events.size());
  EXPECT_EQ(0u, CheckForKeyEvents().size());
  EXPECT_EQ(ui::EventType::kMousePressed, mouse_events[0].type());
  EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & mouse_events[0].flags());
  EXPECT_EQ(mouse_events[0].location(), kDefaultPosition);

  // A repeated key shouldn't cause another click.
  ClearEvents();
  PressKey(ui::VKEY_I, ui::EF_IS_REPEAT);
  ASSERT_EQ(0u, CheckForMouseEvents().size());
  EXPECT_EQ(0u, CheckForKeyEvents().size());

  // Releasing the key should release the mouse.
  ClearEvents();
  ReleaseKey(ui::VKEY_I);
  mouse_events = CheckForMouseEvents();
  EXPECT_EQ(0u, CheckForKeyEvents().size());
  ASSERT_EQ(1u, mouse_events.size());
  EXPECT_EQ(ui::EventType::kMouseReleased, mouse_events[0].type());
  EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & mouse_events[0].flags());
  EXPECT_EQ(mouse_events[0].location(), kDefaultPosition);
}

TEST_F(MouseKeysTest, Move) {
  GetEventGenerator()->MoveMouseToWithNative(kDefaultPosition,
                                             kDefaultPosition);

  // We should not see any mouse events initially.
  ClearEvents();
  EXPECT_FALSE(GetMouseKeysController()->enabled());
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
  SetEnabled(true);
  EXPECT_TRUE(GetMouseKeysController()->enabled());
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
                                    kMoveDeltaDIP);

  // We should not get any more events.
  ClearEvents();
  EXPECT_EQ(0u, CheckForMouseEvents().size());

  // Disable Mouse Keys, and we should see the original behaviour.
  ClearEvents();
  SetEnabled(false);
  EXPECT_FALSE(GetMouseKeysController()->enabled());
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
  SetEnabled(true);
  EXPECT_TRUE(GetMouseKeysController()->enabled());
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
                                    kMoveDeltaDIP);

  ClearEvents();
  // Click
  PressAndReleaseColemakKey(ui::VKEY_U);

  EXPECT_EQ(0u, CheckForKeyEvents().size());
  ExpectClick(CheckForMouseEvents(), ui::EF_LEFT_MOUSE_BUTTON,
              kDefaultPosition);

  ClearEvents();
  // Unmapped
  PressAndReleaseColemakKey(ui::VKEY_O);
  PressAndReleaseColemakKey(ui::VKEY_J);
  PressAndReleaseColemakKey(ui::VKEY_K);
  EXPECT_EQ(0u, CheckForMouseEvents().size());
  EXPECT_EQ(6u, CheckForKeyEvents().size());
}

TEST_F(MouseKeysTest, MaxSpeed) {
  // Enough time for the initial event and 9 updates.
  constexpr auto kTenEventsInSeconds =
      MouseKeysController::kUpdateFrequencyInSeconds * 9.5;
  GetEventGenerator()->MoveMouseToWithNative(kDefaultPosition,
                                             kDefaultPosition);
  SetEnabled(true);
  EXPECT_TRUE(GetMouseKeysController()->enabled());

  // No acceleration.
  constexpr int kMaxSpeed = 3;
  SetMaxSpeed(kMaxSpeed);
  SetAcceleration(0);

  // Move right.
  ClearEvents();
  PressKey(ui::VKEY_O);
  task_environment()->FastForwardBy(base::Seconds(kTenEventsInSeconds));
  ReleaseKey(ui::VKEY_O);
  auto mouse_events = CheckForMouseEvents();
  EXPECT_EQ(0u, CheckForKeyEvents().size());

  ASSERT_EQ(10u, mouse_events.size());
  gfx::Vector2d move_delta(kMoveDeltaDIP * kMaxSpeed, 0);
  auto position = kDefaultPosition;
  for (size_t i = 0; i < mouse_events.size(); ++i) {
    position += move_delta;
    EXPECT_EQ(ui::EventType::kMouseMoved, mouse_events[i].type());
    EXPECT_EQ(mouse_events[i].location(), position);
  }

  // Move down and left.
  ClearEvents();
  PressKey(ui::VKEY_J);
  task_environment()->FastForwardBy(base::Seconds(kTenEventsInSeconds));
  ReleaseKey(ui::VKEY_J);
  mouse_events = CheckForMouseEvents();
  EXPECT_EQ(0u, CheckForKeyEvents().size());

  EXPECT_EQ(10u, mouse_events.size());
  move_delta =
      gfx::Vector2d(-kMoveDeltaDIP * kMaxSpeed, kMoveDeltaDIP * kMaxSpeed);
  for (size_t i = 0; i < mouse_events.size(); ++i) {
    position += move_delta;
    EXPECT_EQ(ui::EventType::kMouseMoved, mouse_events[i].type());
    EXPECT_EQ(mouse_events[i].location(), position);
  }
}

TEST_F(MouseKeysTest, Acceleration) {
  // Enough time for the initial event and 9 updates.
  constexpr auto kTenEventsInSeconds =
      MouseKeysController::kUpdateFrequencyInSeconds * 9.5;
  GetEventGenerator()->MoveMouseToWithNative(kDefaultPosition,
                                             kDefaultPosition);
  SetEnabled(true);
  EXPECT_TRUE(GetMouseKeysController()->enabled());

  // Some acceleration.
  constexpr double kAcceleration = 0.2;
  const double kAccelerationDelta =
      kAcceleration *
      MouseKeysController::kBaseAccelerationDIPPerSecondSquared *
      MouseKeysController::kUpdateFrequencyInSeconds;
  SetMaxSpeed(10);
  SetAcceleration(kAcceleration);

  // Move down.
  ClearEvents();
  PressKey(ui::VKEY_K);
  task_environment()->FastForwardBy(base::Seconds(kTenEventsInSeconds));
  ReleaseKey(ui::VKEY_K);
  auto mouse_events = CheckForMouseEvents();
  EXPECT_EQ(0u, CheckForKeyEvents().size());

  EXPECT_EQ(10u, mouse_events.size());
  double move_delta = kMoveDeltaDIP;
  auto position = kDefaultPosition;
  for (size_t i = 0; i < mouse_events.size(); ++i) {
    position += gfx::Vector2d(0, move_delta);
    EXPECT_EQ(ui::EventType::kMouseMoved, mouse_events[i].type());
    EXPECT_EQ(mouse_events[i].location(), position);
    move_delta += kAccelerationDelta;
  }

  // Move up and right.
  ClearEvents();
  PressKey(ui::VKEY_9);
  task_environment()->FastForwardBy(base::Seconds(kTenEventsInSeconds));
  ReleaseKey(ui::VKEY_9);
  mouse_events = CheckForMouseEvents();
  EXPECT_EQ(0u, CheckForKeyEvents().size());

  EXPECT_EQ(10u, mouse_events.size());
  move_delta = kMoveDeltaDIP;
  for (size_t i = 0; i < mouse_events.size(); ++i) {
    position += gfx::Vector2d(move_delta, -move_delta);
    EXPECT_EQ(ui::EventType::kMouseMoved, mouse_events[i].type());
    EXPECT_EQ(mouse_events[i].location(), position);
    move_delta += kAccelerationDelta;
  }
}

TEST_F(MouseKeysTest, AccelerationAndMaxSpeed) {
  // Enough time for the initial event and 9 updates.
  constexpr auto kTenEventsInSeconds =
      MouseKeysController::kUpdateFrequencyInSeconds * 9.5;
  GetEventGenerator()->MoveMouseToWithNative(kDefaultPosition,
                                             kDefaultPosition);
  SetEnabled(true);
  EXPECT_TRUE(GetMouseKeysController()->enabled());

  // Some acceleration.
  constexpr double kAcceleration = 0.5;
  constexpr double kMaxSpeedFactor = 3;
  constexpr double kMaxSpeed = kMaxSpeedFactor *
                               MouseKeysController::kBaseSpeedDIPPerSecond *
                               MouseKeysController::kUpdateFrequencyInSeconds;
  const double kAccelerationDelta =
      kAcceleration *
      MouseKeysController::kBaseAccelerationDIPPerSecondSquared *
      MouseKeysController::kUpdateFrequencyInSeconds;
  SetMaxSpeed(kMaxSpeedFactor);
  SetAcceleration(kAcceleration);

  // Move right.
  ClearEvents();
  PressKey(ui::VKEY_O);
  task_environment()->FastForwardBy(base::Seconds(kTenEventsInSeconds));
  ReleaseKey(ui::VKEY_O);
  auto mouse_events = CheckForMouseEvents();
  EXPECT_EQ(0u, CheckForKeyEvents().size());

  EXPECT_EQ(10u, mouse_events.size());
  double move_delta = kMoveDeltaDIP;
  auto position = kDefaultPosition;
  for (size_t i = 0; i < mouse_events.size(); ++i) {
    position += gfx::Vector2d(move_delta, 0);
    EXPECT_EQ(ui::EventType::kMouseMoved, mouse_events[i].type());
    EXPECT_EQ(mouse_events[i].location(), position);
    move_delta += kAccelerationDelta;
    move_delta = std::clamp(move_delta, 0.0, kMaxSpeed);
  }
}

TEST_F(MouseKeysTest, LeftHanded) {
  GetEventGenerator()->MoveMouseToWithNative(kDefaultPosition,
                                             kDefaultPosition);

  ClearEvents();
  SetEnabled(true);
  EXPECT_TRUE(GetMouseKeysController()->enabled());

  // We should not see any mouse events initially from the left hand.
  PressAndReleaseKey(ui::VKEY_1);
  PressAndReleaseKey(ui::VKEY_2);
  PressAndReleaseKey(ui::VKEY_3);
  PressAndReleaseKey(ui::VKEY_Q);
  PressAndReleaseKey(ui::VKEY_E);
  PressAndReleaseKey(ui::VKEY_A);
  PressAndReleaseKey(ui::VKEY_S);
  PressAndReleaseKey(ui::VKEY_D);
  PressAndReleaseKey(ui::VKEY_W);
  EXPECT_EQ(0u, CheckForMouseEvents().size());
  EXPECT_EQ(18u, CheckForKeyEvents().size());

  // Switch to left handed.
  SetLeftHanded(true);

  ClearEvents();
  // We should not see any mouse events from the right hand.
  PressAndReleaseKey(ui::VKEY_7);
  PressAndReleaseKey(ui::VKEY_8);
  PressAndReleaseKey(ui::VKEY_9);
  PressAndReleaseKey(ui::VKEY_U);
  PressAndReleaseKey(ui::VKEY_O);
  PressAndReleaseKey(ui::VKEY_J);
  PressAndReleaseKey(ui::VKEY_K);
  PressAndReleaseKey(ui::VKEY_L);
  PressAndReleaseKey(ui::VKEY_I);
  EXPECT_EQ(0u, CheckForMouseEvents().size());
  EXPECT_EQ(18u, CheckForKeyEvents().size());

  // We should be able to click by pressing w.
  ClearEvents();
  PressAndReleaseKey(ui::VKEY_W);
  EXPECT_EQ(0u, CheckForKeyEvents().size());
  ExpectClick(CheckForMouseEvents(), ui::EF_LEFT_MOUSE_BUTTON,
              kDefaultPosition);

  // Enable Mouse Keys, and we should be able to move the mouse with 1, 2, 3, q,
  // e, a, s, d.
  ClearEvents();
  EXPECT_TRUE(GetMouseKeysController()->enabled());
  PressAndReleaseKey(ui::VKEY_1);
  PressAndReleaseKey(ui::VKEY_2);
  PressAndReleaseKey(ui::VKEY_3);
  PressAndReleaseKey(ui::VKEY_Q);
  PressAndReleaseKey(ui::VKEY_E);
  PressAndReleaseKey(ui::VKEY_A);
  PressAndReleaseKey(ui::VKEY_S);
  PressAndReleaseKey(ui::VKEY_D);
  EXPECT_EQ(0u, CheckForKeyEvents().size());
  ExpectMouseMovedInCircularPattern(CheckForMouseEvents(), kDefaultPosition,
                                    kMoveDeltaDIP);
}

TEST_F(MouseKeysTest, NumPad) {
  SetEnabled(true);
  GetEventGenerator()->MoveMouseToWithNative(kDefaultPosition,
                                             kDefaultPosition);

  // Switch to the num pad.
  SetUsePrimaryKeys(false);

  // We should be able to click with the num pad 5.
  ClearEvents();
  PressAndReleaseKey(ui::VKEY_NUMPAD5);
  EXPECT_EQ(0u, CheckForKeyEvents().size());
  ExpectClick(CheckForMouseEvents(), ui::EF_LEFT_MOUSE_BUTTON,
              kDefaultPosition);

  // We should be able to move the mouse with the num pad.
  ClearEvents();
  PressAndReleaseKey(ui::VKEY_NUMPAD7);
  PressAndReleaseKey(ui::VKEY_NUMPAD8);
  PressAndReleaseKey(ui::VKEY_NUMPAD9);
  PressAndReleaseKey(ui::VKEY_NUMPAD4);
  PressAndReleaseKey(ui::VKEY_NUMPAD6);
  PressAndReleaseKey(ui::VKEY_NUMPAD1);
  PressAndReleaseKey(ui::VKEY_NUMPAD2);
  PressAndReleaseKey(ui::VKEY_NUMPAD3);
  EXPECT_EQ(0u, CheckForKeyEvents().size());
  ExpectMouseMovedInCircularPattern(CheckForMouseEvents(), kDefaultPosition,
                                    kMoveDeltaDIP);
}

TEST_F(MouseKeysTest, UsePrimaryKeyboard) {
  SetEnabled(true);
  GetEventGenerator()->MoveMouseToWithNative(kDefaultPosition,
                                             kDefaultPosition);

  // Turn off the primary keyboard.
  SetUsePrimaryKeys(false);

  // Switch to left handed.
  SetLeftHanded(true);

  ClearEvents();
  // We should not see any mouse events from the left hand.
  PressAndReleaseKey(ui::VKEY_1);
  PressAndReleaseKey(ui::VKEY_2);
  PressAndReleaseKey(ui::VKEY_3);
  PressAndReleaseKey(ui::VKEY_Q);
  PressAndReleaseKey(ui::VKEY_E);
  PressAndReleaseKey(ui::VKEY_A);
  PressAndReleaseKey(ui::VKEY_S);
  PressAndReleaseKey(ui::VKEY_D);
  PressAndReleaseKey(ui::VKEY_W);
  EXPECT_EQ(0u, CheckForMouseEvents().size());
  EXPECT_EQ(18u, CheckForKeyEvents().size());

  // Switch to right handed.
  SetLeftHanded(false);

  ClearEvents();
  // We should not see any mouse events from the right hand.
  PressAndReleaseKey(ui::VKEY_7);
  PressAndReleaseKey(ui::VKEY_8);
  PressAndReleaseKey(ui::VKEY_9);
  PressAndReleaseKey(ui::VKEY_U);
  PressAndReleaseKey(ui::VKEY_O);
  PressAndReleaseKey(ui::VKEY_J);
  PressAndReleaseKey(ui::VKEY_K);
  PressAndReleaseKey(ui::VKEY_L);
  PressAndReleaseKey(ui::VKEY_I);
  EXPECT_EQ(0u, CheckForMouseEvents().size());
  EXPECT_EQ(18u, CheckForKeyEvents().size());
}

TEST_F(MouseKeysTest, Dragging) {
  // Enough time for the initial event and 9 updates.
  constexpr auto kTenEventsInSeconds =
      MouseKeysController::kUpdateFrequencyInSeconds * 9.5;
  GetEventGenerator()->MoveMouseToWithNative(kDefaultPosition,
                                             kDefaultPosition);
  SetEnabled(true);
  // No acceleration.
  constexpr int kMaxSpeed = 3;
  SetMaxSpeed(kMaxSpeed);
  SetAcceleration(0);

  // Start Drag.
  ClearEvents();
  PressAndReleaseKey(ui::VKEY_M);
  auto mouse_events = CheckForMouseEvents();
  EXPECT_EQ(0u, CheckForKeyEvents().size());
  ASSERT_EQ(1u, mouse_events.size());
  EXPECT_EQ(ui::EventType::kMousePressed, mouse_events[0].type());
  EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & mouse_events[0].flags());
  EXPECT_EQ(mouse_events[0].location(), kDefaultPosition);

  // Move right.
  ClearEvents();
  PressKey(ui::VKEY_O);
  task_environment()->FastForwardBy(base::Seconds(kTenEventsInSeconds));
  ReleaseKey(ui::VKEY_O);
  mouse_events = CheckForMouseEvents();
  EXPECT_EQ(0u, CheckForKeyEvents().size());
  ASSERT_EQ(10u, mouse_events.size());
  gfx::Vector2d move_delta(kMoveDeltaDIP * kMaxSpeed, 0);
  auto position = kDefaultPosition;
  for (size_t i = 0; i < mouse_events.size(); ++i) {
    position += move_delta;
    EXPECT_EQ(ui::EventType::kMouseDragged, mouse_events[i].type());
    EXPECT_EQ(mouse_events[i].location(), position);
  }

  // Stop Drag.
  ClearEvents();
  PressAndReleaseKey(ui::VKEY_OEM_PERIOD);
  mouse_events = CheckForMouseEvents();
  EXPECT_EQ(0u, CheckForKeyEvents().size());
  ASSERT_EQ(1u, mouse_events.size());
  EXPECT_EQ(ui::EventType::kMouseReleased, mouse_events[0].type());
  EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & mouse_events[0].flags());
  EXPECT_EQ(mouse_events[0].location(), position);
}

TEST_F(MouseKeysTest, DragWithClick) {
  // Enough time for the initial event and 9 updates.
  constexpr auto kTenEventsInSeconds =
      MouseKeysController::kUpdateFrequencyInSeconds * 9.5;
  GetEventGenerator()->MoveMouseToWithNative(kDefaultPosition,
                                             kDefaultPosition);
  SetEnabled(true);
  // No acceleration.
  constexpr int kMaxSpeed = 3;
  SetMaxSpeed(kMaxSpeed);
  SetAcceleration(0);

  // Start Drag.
  ClearEvents();
  PressKey(ui::VKEY_I);
  auto mouse_events = CheckForMouseEvents();
  EXPECT_EQ(0u, CheckForKeyEvents().size());
  ASSERT_EQ(1u, mouse_events.size());
  EXPECT_EQ(ui::EventType::kMousePressed, mouse_events[0].type());
  EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & mouse_events[0].flags());
  EXPECT_EQ(mouse_events[0].location(), kDefaultPosition);

  // Move right.
  ClearEvents();
  PressKey(ui::VKEY_O);
  task_environment()->FastForwardBy(base::Seconds(kTenEventsInSeconds));
  ReleaseKey(ui::VKEY_O);
  mouse_events = CheckForMouseEvents();
  EXPECT_EQ(0u, CheckForKeyEvents().size());
  ASSERT_EQ(10u, mouse_events.size());
  gfx::Vector2d move_delta(kMoveDeltaDIP * kMaxSpeed, 0);
  auto position = kDefaultPosition;
  for (size_t i = 0; i < mouse_events.size(); ++i) {
    position += move_delta;
    EXPECT_EQ(ui::EventType::kMouseDragged, mouse_events[i].type());
    EXPECT_EQ(mouse_events[i].location(), position);
  }

  // Stop Drag.
  ClearEvents();
  ReleaseKey(ui::VKEY_I);
  mouse_events = CheckForMouseEvents();
  EXPECT_EQ(0u, CheckForKeyEvents().size());
  ASSERT_EQ(1u, mouse_events.size());
  EXPECT_EQ(ui::EventType::kMouseReleased, mouse_events[0].type());
  EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & mouse_events[0].flags());
  EXPECT_EQ(mouse_events[0].location(), position);
}

TEST_F(MouseKeysTest, DragWithMixed) {
  // Enough time for the initial event and 9 updates.
  constexpr auto kTenEventsInSeconds =
      MouseKeysController::kUpdateFrequencyInSeconds * 9.5;
  GetEventGenerator()->MoveMouseToWithNative(kDefaultPosition,
                                             kDefaultPosition);
  SetEnabled(true);
  // No acceleration.
  constexpr int kMaxSpeed = 3;
  SetMaxSpeed(kMaxSpeed);
  SetAcceleration(0);

  // Start Drag.
  ClearEvents();
  PressAndReleaseKey(ui::VKEY_M);
  auto mouse_events = CheckForMouseEvents();
  EXPECT_EQ(0u, CheckForKeyEvents().size());
  ASSERT_EQ(1u, mouse_events.size());
  EXPECT_EQ(ui::EventType::kMousePressed, mouse_events[0].type());
  EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & mouse_events[0].flags());
  EXPECT_EQ(mouse_events[0].location(), kDefaultPosition);

  // Move right.
  ClearEvents();
  PressKey(ui::VKEY_O);
  task_environment()->FastForwardBy(base::Seconds(kTenEventsInSeconds));
  ReleaseKey(ui::VKEY_O);
  mouse_events = CheckForMouseEvents();
  EXPECT_EQ(0u, CheckForKeyEvents().size());
  ASSERT_EQ(10u, mouse_events.size());
  gfx::Vector2d move_delta(kMoveDeltaDIP * kMaxSpeed, 0);
  auto position = kDefaultPosition;
  for (size_t i = 0; i < mouse_events.size(); ++i) {
    position += move_delta;
    EXPECT_EQ(ui::EventType::kMouseDragged, mouse_events[i].type());
    EXPECT_EQ(mouse_events[i].location(), position);
  }

  // Stop Drag.
  ClearEvents();
  PressAndReleaseKey(ui::VKEY_I);
  mouse_events = CheckForMouseEvents();
  EXPECT_EQ(0u, CheckForKeyEvents().size());
  ASSERT_EQ(1u, mouse_events.size());
  EXPECT_EQ(ui::EventType::kMouseReleased, mouse_events[0].type());
  EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & mouse_events[0].flags());
  EXPECT_EQ(mouse_events[0].location(), position);
}

TEST_F(MouseKeysTest, Accelerator) {
  SetEnabled(true);
  auto* accelerator_controller = Shell::Get()->accelerator_controller();
  GetEventGenerator()->MoveMouseToWithNative(kDefaultPosition,
                                             kDefaultPosition);

  // Enable Mouse Keys, and we should be able to click by pressing i.
  ClearEvents();
  PressAndReleaseKey(ui::VKEY_I);
  EXPECT_EQ(0u, CheckForKeyEvents().size());
  ExpectClick(CheckForMouseEvents(), ui::EF_LEFT_MOUSE_BUTTON,
              kDefaultPosition);

  // Toggle Mouse Keys off, and we should see no mouse events.
  accelerator_controller->PerformActionIfEnabled(
      AcceleratorAction::kToggleMouseKeys, {});

  ClearEvents();
  PressAndReleaseKey(ui::VKEY_I);
  EXPECT_EQ(0u, CheckForMouseEvents().size());

  // Toggle Mouse Keys on, and we should see the original behaviour.
  accelerator_controller->PerformActionIfEnabled(
      AcceleratorAction::kToggleMouseKeys, {});

  ClearEvents();
  PressAndReleaseKey(ui::VKEY_I);
  EXPECT_EQ(0u, CheckForKeyEvents().size());
  ExpectClick(CheckForMouseEvents(), ui::EF_LEFT_MOUSE_BUTTON,
              kDefaultPosition);
}

}  // namespace ash
