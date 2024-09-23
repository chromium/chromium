// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/events/accessibility_event_rewriter.h"

#include <memory>
#include <optional>
#include <vector>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/mouse_keys/mouse_keys_controller.h"
#include "ash/constants/ash_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/events/test_event_capturer.h"
#include "ash/public/cpp/accessibility_event_rewriter_delegate.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/ash/fake_ime_keyboard.h"
#include "ui/events/ash/event_rewriter_ash.h"
#include "ui/events/ash/fake_event_rewriter_ash_delegate.h"
#include "ui/events/ash/keyboard_capability.h"
#include "ui/events/ash/keyboard_device_id_event_rewriter.h"
#include "ui/events/ash/keyboard_modifier_event_rewriter.h"
#include "ui/events/ash/mojom/extended_fkeys_modifier.mojom-shared.h"
#include "ui/events/ash/mojom/modifier_key.mojom-shared.h"
#include "ui/events/ash/mojom/six_pack_shortcut_modifier.mojom-shared.h"
#include "ui/events/ash/pref_names.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_handler.h"
#include "ui/events/event_rewriter.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/test/test_event_rewriter.h"
#include "ui/events/types/event_type.h"

namespace ash {
namespace {

// A test implementation of the spoken feedback delegate interface.
class TestAccessibilityEventRewriterDelegate
    : public AccessibilityEventRewriterDelegate {
 public:
  TestAccessibilityEventRewriterDelegate() = default;
  TestAccessibilityEventRewriterDelegate(
      const TestAccessibilityEventRewriterDelegate&) = delete;
  TestAccessibilityEventRewriterDelegate& operator=(
      const TestAccessibilityEventRewriterDelegate&) = delete;
  ~TestAccessibilityEventRewriterDelegate() override = default;

  // Count of events sent to the delegate.
  size_t chromevox_recorded_event_count_ = 0;

  // Count of captured events sent to the delegate.
  size_t chromevox_captured_event_count_ = 0;

  // Last key event sent to ChromeVox.
  ui::Event* GetLastChromeVoxKeyEvent() {
    return last_chromevox_key_event_.get();
  }

  const std::vector<SwitchAccessCommand>& switch_access_commands() const {
    return switch_access_commands_;
  }
  void ClearSwitchAccessCommands() { switch_access_commands_.clear(); }

  const std::vector<MagnifierCommand>& magnifier_commands() const {
    return magnifier_commands_;
  }
  void ClearMagnifierCommands() { magnifier_commands_.clear(); }

 private:
  // AccessibilityEventRewriterDelegate:
  void DispatchKeyEventToChromeVox(std::unique_ptr<ui::Event> event,
                                   bool capture) override {
    chromevox_recorded_event_count_++;
    if (capture)
      chromevox_captured_event_count_++;
    last_chromevox_key_event_ = std::move(event);
  }
  void DispatchMouseEvent(std::unique_ptr<ui::Event> event) override {
    chromevox_recorded_event_count_++;
  }
  void SendSwitchAccessCommand(SwitchAccessCommand command) override {
    switch_access_commands_.push_back(command);
  }
  void SendPointScanPoint(const gfx::PointF& point) override {}
  void SendMagnifierCommand(MagnifierCommand command) override {
    magnifier_commands_.push_back(command);
  }

  std::unique_ptr<ui::Event> last_chromevox_key_event_;
  std::vector<SwitchAccessCommand> switch_access_commands_;
  std::vector<MagnifierCommand> magnifier_commands_;
};

class KeyboardModifierEventRewriterDelegate
    : public ui::KeyboardModifierEventRewriter::Delegate {
 public:
  explicit KeyboardModifierEventRewriterDelegate(
      ui::EventRewriterAsh::Delegate* delegate)
      : delegate_(delegate) {}

  std::optional<ui::mojom::ModifierKey> GetKeyboardRemappedModifierValue(
      int device_id,
      ui::mojom::ModifierKey modifier_key,
      const std::string& pref_name) const override {
    return delegate_->GetKeyboardRemappedModifierValue(device_id, modifier_key,
                                                       pref_name);
  }

  bool RewriteModifierKeys() override {
    return delegate_->RewriteModifierKeys();
  }

 private:
  raw_ptr<ui::EventRewriterAsh::Delegate> delegate_;
};

// Common set up for AccessibilityEventRewriter related unit tests.
class AccessibilityEventRewriterTestBase : public ash::AshTestBase {
 protected:
  AccessibilityEventRewriterTestBase() = default;
  AccessibilityEventRewriterTestBase(
      const AccessibilityEventRewriterTestBase&) = delete;
  AccessibilityEventRewriterTestBase& operator=(
      const AccessibilityEventRewriterTestBase&) = delete;
  ~AccessibilityEventRewriterTestBase() override = default;

  void SetUp() override {
    ash::AshTestBase::SetUp();
    event_capturer_.set_capture_mouse_enter_exit(false);
    generator_ = AshTestBase::GetEventGenerator();

    accessibility_event_rewriter_ =
        std::make_unique<AccessibilityEventRewriter>(
            &event_rewriter_ash_, &accessibility_event_rewriter_delegate_);

    GetContext()->AddPreTargetHandler(&event_capturer_);

    GetAccessibilityController()->SetAccessibilityEventRewriter(
        accessibility_event_rewriter_.get());

    auto* event_source = GetContext()->GetHost()->GetEventSource();
    if (ash::features::IsKeyboardRewriterFixEnabled()) {
      event_source->AddEventRewriter(&keyboard_device_id_event_rewriter_);
      event_source->AddEventRewriter(&keyboard_modifier_event_rewriter_);
    }
    event_source->AddEventRewriter(accessibility_event_rewriter_.get());
  }

  void TearDown() override {
    auto* event_source = GetContext()->GetHost()->GetEventSource();
    event_source->RemoveEventRewriter(accessibility_event_rewriter_.get());
    if (ash::features::IsKeyboardRewriterFixEnabled()) {
      event_source->RemoveEventRewriter(&keyboard_modifier_event_rewriter_);
      event_source->RemoveEventRewriter(&keyboard_device_id_event_rewriter_);
    }

    GetAccessibilityController()->SetAccessibilityEventRewriter(nullptr);

    GetContext()->RemovePreTargetHandler(&event_capturer_);
    accessibility_event_rewriter_.reset();

    generator_ = nullptr;
    ash::AshTestBase::TearDown();
  }

 protected:
  TestEventCapturer& event_capturer() { return event_capturer_; }

  TestAccessibilityEventRewriterDelegate&
  accessibility_event_rewriter_delegate() {
    return accessibility_event_rewriter_delegate_;
  }

  AccessibilityEventRewriter& accessibility_event_rewriter() {
    return *accessibility_event_rewriter_;
  }

  ui::test::EventGenerator& generator() { return *generator_; }

  AccessibilityController* GetAccessibilityController() {
    return Shell::Get()->accessibility_controller();
  }

  void SetModifierRemapping(const std::string& pref_name,
                            ui::mojom::ModifierKey value) {
    event_rewriter_ash_delegate_.SetModifierRemapping(pref_name, value);
  }

 private:
  TestEventCapturer event_capturer_;

  ui::test::FakeEventRewriterAshDelegate event_rewriter_ash_delegate_;
  ui::StubKeyboardLayoutEngine keyboard_layout_engine_;
  std::unique_ptr<ui::KeyboardCapability> keyboard_capability_{
      ui::KeyboardCapability::CreateStubKeyboardCapability()};
  input_method::FakeImeKeyboard fake_ime_keyboard_;

  ui::KeyboardDeviceIdEventRewriter keyboard_device_id_event_rewriter_{
      keyboard_capability_.get()};
  ui::KeyboardModifierEventRewriter keyboard_modifier_event_rewriter_{
      std::make_unique<KeyboardModifierEventRewriterDelegate>(
          &event_rewriter_ash_delegate_),
      &keyboard_layout_engine_, keyboard_capability_.get(),
      &fake_ime_keyboard_};
  ui::EventRewriterAsh event_rewriter_ash_{&event_rewriter_ash_delegate_,
                                           keyboard_capability_.get(), nullptr,
                                           false, &fake_ime_keyboard_};

  TestAccessibilityEventRewriterDelegate accessibility_event_rewriter_delegate_;
  std::unique_ptr<AccessibilityEventRewriter> accessibility_event_rewriter_;

  // Generates ui::Events from simulated user input.
  raw_ptr<ui::test::EventGenerator> generator_ = nullptr;
};

}  // namespace

class ChromeVoxAccessibilityEventRewriterTest
    : public AccessibilityEventRewriterTestBase,
      public testing::WithParamInterface<bool> {
 public:
  ChromeVoxAccessibilityEventRewriterTest() {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          ash::features::kEnableKeyboardRewriterFix);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          ash::features::kEnableKeyboardRewriterFix);
    }
  }

  void SetUp() override {
    AccessibilityEventRewriterTestBase::SetUp();

    GetContext()->GetHost()->GetEventSource()->AddEventRewriter(
        &event_recorder_);
  }

  void TearDown() override {
    GetContext()->GetHost()->GetEventSource()->RemoveEventRewriter(
        &event_recorder_);
    AccessibilityEventRewriterTestBase::TearDown();
  }

  ui::test::TestEventRewriter& event_recorder() { return event_recorder_; }

  size_t delegate_chromevox_recorded_event_count() {
    return accessibility_event_rewriter_delegate()
        .chromevox_recorded_event_count_;
  }

  size_t delegate_chromevox_captured_event_count() {
    return accessibility_event_rewriter_delegate()
        .chromevox_captured_event_count_;
  }

  void SetDelegateChromeVoxCaptureAllKeys(bool value) {
    accessibility_event_rewriter().set_chromevox_capture_all_keys(value);
  }

  void ExpectCounts(size_t expected_recorded_count,
                    size_t expected_delegate_count,
                    size_t expected_captured_count) {
    EXPECT_EQ(expected_recorded_count,
              static_cast<size_t>(event_recorder().events_seen()));
    EXPECT_EQ(expected_delegate_count,
              delegate_chromevox_recorded_event_count());
    EXPECT_EQ(expected_captured_count,
              delegate_chromevox_captured_event_count());
  }

  bool RewriteEventForChromeVox(
      const ui::Event& event,
      const AccessibilityEventRewriter::Continuation continuation) {
    return accessibility_event_rewriter().RewriteEventForChromeVox(
        event, continuation);
  }

  ui::KeyEvent* GetLastChromeVoxKeyEvent() {
    if (accessibility_event_rewriter_delegate().GetLastChromeVoxKeyEvent()) {
      return accessibility_event_rewriter_delegate()
          .GetLastChromeVoxKeyEvent()
          ->AsKeyEvent();
    }
    return nullptr;
  }

  void SetRemapPositionalKeys(bool value) {
    accessibility_event_rewriter()
        .try_rewriting_positional_keys_for_chromevox_ = value;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  // Records events delivered to the next event rewriter after spoken feedback.
  ui::test::TestEventRewriter event_recorder_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         ChromeVoxAccessibilityEventRewriterTest,
                         testing::Bool());

// The delegate should not intercept events when spoken feedback is disabled.
TEST_P(ChromeVoxAccessibilityEventRewriterTest, EventsNotConsumedWhenDisabled) {
  AccessibilityController* controller = GetAccessibilityController();
  EXPECT_FALSE(controller->spoken_feedback().enabled());

  generator().PressKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_EQ(1, event_recorder().events_seen());
  EXPECT_EQ(0U, delegate_chromevox_recorded_event_count());
  generator().ReleaseKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_EQ(2, event_recorder().events_seen());
  EXPECT_EQ(0U, delegate_chromevox_recorded_event_count());

  generator().ClickLeftButton();
  EXPECT_EQ(4, event_recorder().events_seen());
  EXPECT_EQ(0U, delegate_chromevox_recorded_event_count());

  generator().GestureTapAt(gfx::Point());
  EXPECT_EQ(6, event_recorder().events_seen());
  EXPECT_EQ(0U, delegate_chromevox_recorded_event_count());
}

// The delegate should intercept key events when spoken feedback is enabled.
TEST_P(ChromeVoxAccessibilityEventRewriterTest, KeyEventsConsumedWhenEnabled) {
  AccessibilityController* controller = GetAccessibilityController();
  controller->SetSpokenFeedbackEnabled(true, A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(controller->spoken_feedback().enabled());

  generator().PressKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_EQ(1, event_recorder().events_seen());
  EXPECT_EQ(1U, delegate_chromevox_recorded_event_count());
  EXPECT_EQ(0U, delegate_chromevox_captured_event_count());
  generator().ReleaseKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_EQ(2, event_recorder().events_seen());
  EXPECT_EQ(2U, delegate_chromevox_recorded_event_count());
  EXPECT_EQ(0U, delegate_chromevox_captured_event_count());

  generator().ClickLeftButton();
  EXPECT_EQ(4, event_recorder().events_seen());
  EXPECT_EQ(2U, delegate_chromevox_recorded_event_count());
  EXPECT_EQ(0U, delegate_chromevox_captured_event_count());

  generator().GestureTapAt(gfx::Point());
  EXPECT_EQ(6, event_recorder().events_seen());
  EXPECT_EQ(2U, delegate_chromevox_recorded_event_count());
  EXPECT_EQ(0U, delegate_chromevox_captured_event_count());
}

// Asynchronously unhandled events should be sent to subsequent rewriters.
TEST_P(ChromeVoxAccessibilityEventRewriterTest,
       UnhandledEventsSentToOtherRewriters) {
  // Before it can forward unhandled events, AccessibilityEventRewriter
  // must have seen at least one event in the first place.
  generator().PressKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_EQ(1, event_recorder().events_seen());
  generator().ReleaseKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_EQ(2, event_recorder().events_seen());

  accessibility_event_rewriter().OnUnhandledSpokenFeedbackEvent(
      std::make_unique<ui::KeyEvent>(ui::EventType::kKeyPressed, ui::VKEY_A,
                                     ui::EF_NONE));
  EXPECT_EQ(3, event_recorder().events_seen());
  accessibility_event_rewriter().OnUnhandledSpokenFeedbackEvent(
      std::make_unique<ui::KeyEvent>(ui::EventType::kKeyReleased, ui::VKEY_A,
                                     ui::EF_NONE));
  EXPECT_EQ(4, event_recorder().events_seen());
}

TEST_P(ChromeVoxAccessibilityEventRewriterTest,
       KeysNotEatenWithChromeVoxDisabled) {
  AccessibilityController* controller = GetAccessibilityController();
  EXPECT_FALSE(controller->spoken_feedback().enabled());

  // Send Search+Shift+Right.
  generator().PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  EXPECT_EQ(1, event_recorder().events_seen());
  generator().PressKey(ui::VKEY_SHIFT, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN);
  EXPECT_EQ(2, event_recorder().events_seen());

  // Mock successful commands lookup and dispatch; shouldn't matter either way.
  generator().PressKey(ui::VKEY_RIGHT, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN);
  EXPECT_EQ(3, event_recorder().events_seen());

  // Released keys shouldn't get eaten.
  generator().ReleaseKey(ui::VKEY_RIGHT,
                         ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN);
  generator().ReleaseKey(ui::VKEY_SHIFT, ui::EF_COMMAND_DOWN);
  generator().ReleaseKey(ui::VKEY_LWIN, 0);
  EXPECT_EQ(6, event_recorder().events_seen());

  // Try releasing more keys.
  generator().ReleaseKey(ui::VKEY_RIGHT, 0);
  generator().ReleaseKey(ui::VKEY_SHIFT, 0);
  generator().ReleaseKey(ui::VKEY_LWIN, 0);
  EXPECT_EQ(9, event_recorder().events_seen());

  EXPECT_EQ(0U, delegate_chromevox_recorded_event_count());
}

TEST_P(ChromeVoxAccessibilityEventRewriterTest, KeyEventsCaptured) {
  AccessibilityController* controller = GetAccessibilityController();
  controller->SetSpokenFeedbackEnabled(true, A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(controller->spoken_feedback().enabled());

  // Initialize expected counts as variables for easier maintaiblity.
  size_t recorded_count = 0;
  size_t delegate_count = 0;
  size_t captured_count = 0;

  // Anything with Search gets captured.
  generator().PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  ExpectCounts(recorded_count, ++delegate_count, ++captured_count);
  generator().ReleaseKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  ExpectCounts(recorded_count, ++delegate_count, ++captured_count);

  // Tab never gets captured.
  generator().PressKey(ui::VKEY_TAB, ui::EF_NONE);
  ExpectCounts(++recorded_count, ++delegate_count, captured_count);
  generator().ReleaseKey(ui::VKEY_TAB, ui::EF_NONE);
  ExpectCounts(++recorded_count, ++delegate_count, captured_count);

  // A client requested capture of all keys.
  SetDelegateChromeVoxCaptureAllKeys(true);
  generator().PressKey(ui::VKEY_A, ui::EF_NONE);
  ExpectCounts(recorded_count, ++delegate_count, ++captured_count);
  generator().ReleaseKey(ui::VKEY_A, ui::EF_NONE);
  ExpectCounts(recorded_count, ++delegate_count, ++captured_count);

  // Tab never gets captured even with explicit client request for all keys.
  generator().PressKey(ui::VKEY_TAB, ui::EF_NONE);
  ExpectCounts(++recorded_count, ++delegate_count, captured_count);
  generator().ReleaseKey(ui::VKEY_TAB, ui::EF_NONE);
  ExpectCounts(++recorded_count, ++delegate_count, captured_count);

  // A client requested to not capture all keys.
  SetDelegateChromeVoxCaptureAllKeys(false);
  generator().PressKey(ui::VKEY_A, ui::EF_NONE);
  ExpectCounts(++recorded_count, ++delegate_count, captured_count);
  generator().ReleaseKey(ui::VKEY_A, ui::EF_NONE);
  ExpectCounts(++recorded_count, ++delegate_count, captured_count);
}

TEST_P(ChromeVoxAccessibilityEventRewriterTest,
       KeyEventsCapturedWithModifierRemapping) {
  AccessibilityController* controller = GetAccessibilityController();
  controller->SetSpokenFeedbackEnabled(true, A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(controller->spoken_feedback().enabled());

  // Initialize expected counts as variables for easier maintaiblity.
  size_t recorded_count = 0;
  size_t delegate_count = 0;
  size_t captured_count = 0;

  // Map Control key to Search.
  SetModifierRemapping(prefs::kLanguageRemapControlKeyTo,
                       ui::mojom::ModifierKey::kMeta);

  // Anything with Search gets captured.
  generator().PressKey(ui::VKEY_CONTROL, ui::EF_CONTROL_DOWN);
  ExpectCounts(recorded_count, ++delegate_count, ++captured_count);
  // EventRewriterAsh actually omits the modifier flag.
  generator().ReleaseKey(ui::VKEY_CONTROL, 0);
  ExpectCounts(recorded_count, ++delegate_count, ++captured_count);

  // Search itself should also work.
  generator().PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  ExpectCounts(recorded_count, ++delegate_count, ++captured_count);
  generator().ReleaseKey(ui::VKEY_LWIN, 0);
  ExpectCounts(recorded_count, ++delegate_count, ++captured_count);

  // Remapping should have no effect on all other expectations.

  // Tab never gets captured.
  generator().PressKey(ui::VKEY_TAB, ui::EF_NONE);
  ExpectCounts(++recorded_count, ++delegate_count, captured_count);
  generator().ReleaseKey(ui::VKEY_TAB, ui::EF_NONE);
  ExpectCounts(++recorded_count, ++delegate_count, captured_count);

  // A client requested capture of all keys.
  SetDelegateChromeVoxCaptureAllKeys(true);
  generator().PressKey(ui::VKEY_A, ui::EF_NONE);
  ExpectCounts(recorded_count, ++delegate_count, ++captured_count);
  generator().ReleaseKey(ui::VKEY_A, ui::EF_NONE);
  ExpectCounts(recorded_count, ++delegate_count, ++captured_count);

  // Tab never gets captured even with explicit client request for all keys.
  generator().PressKey(ui::VKEY_TAB, ui::EF_NONE);
  ExpectCounts(++recorded_count, ++delegate_count, captured_count);
  generator().ReleaseKey(ui::VKEY_TAB, ui::EF_NONE);
  ExpectCounts(++recorded_count, ++delegate_count, captured_count);

  // A client requested to not capture all keys.
  SetDelegateChromeVoxCaptureAllKeys(false);
  generator().PressKey(ui::VKEY_A, ui::EF_NONE);
  ExpectCounts(++recorded_count, ++delegate_count, captured_count);
  generator().ReleaseKey(ui::VKEY_A, ui::EF_NONE);
  ExpectCounts(++recorded_count, ++delegate_count, captured_count);
}

TEST_P(ChromeVoxAccessibilityEventRewriterTest,
       PositionalInputMethodKeysMightBeRewritten) {
  AccessibilityController* controller = GetAccessibilityController();
  controller->SetSpokenFeedbackEnabled(true, A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(controller->spoken_feedback().enabled());

  // Ensure remapping of positional keys (back to their absolute key codes) is
  // in effect.
  SetRemapPositionalKeys(true);

  // Bracket right is a dom code which always causes an absolute key code to be
  // sent to ChromeVox. The key code below is irrelevant.
  ui::KeyEvent key_event(ui::EventType::kKeyPressed, ui::VKEY_A,
                         ui::DomCode::BRACKET_RIGHT, ui::EF_NONE);
  AccessibilityEventRewriter::Continuation continuation;
  RewriteEventForChromeVox(key_event, continuation);
  auto* last_key_event = GetLastChromeVoxKeyEvent();
  ASSERT_TRUE(last_key_event);

  // This is key cap label "]" in a English input method.
  EXPECT_EQ(ui::VKEY_OEM_6, last_key_event->key_code());

  // Sanity check the flag, when off, using the same key event.
  SetRemapPositionalKeys(false);
  RewriteEventForChromeVox(key_event, continuation);
  last_key_event = GetLastChromeVoxKeyEvent();
  ASSERT_TRUE(last_key_event);

  // Unmodified.
  EXPECT_EQ(ui::VKEY_A, last_key_event->key_code());
}

class MouseKeysAccessibilityEventRewriterTest
    : public AccessibilityEventRewriterTestBase {
 public:
  MouseKeysAccessibilityEventRewriterTest() {
    scoped_feature_list_.InitAndEnableFeature(
        ::features::kAccessibilityMouseKeys);
  }

  void SetUp() override {
    AccessibilityEventRewriterTestBase::SetUp();
    GetAccessibilityController()->mouse_keys().SetEnabled(true);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(MouseKeysAccessibilityEventRewriterTest, CapturesCorrectInput) {
  // Mouse Keys should treat 'i' as a click.
  GetEventGenerator()->PressAndReleaseKey(ui::VKEY_I);
  EXPECT_FALSE(event_capturer().LastKeyEvent());

  // Mouse Keys should not capture 'g'.
  GetEventGenerator()->PressAndReleaseKey(ui::VKEY_G);
  ASSERT_TRUE(event_capturer().LastKeyEvent());
  EXPECT_EQ(ui::VKEY_G, event_capturer().LastKeyEvent()->key_code());
}

class SwitchAccessAccessibilityEventRewriterTest
    : public AccessibilityEventRewriterTestBase,
      public testing::WithParamInterface<bool> {
 public:
  SwitchAccessAccessibilityEventRewriterTest() {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          ash::features::kEnableKeyboardRewriterFix);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          ash::features::kEnableKeyboardRewriterFix);
    }
  }

  void SetUp() override {
    AccessibilityEventRewriterTestBase::SetUp();

    // This test triggers a resize of WindowTreeHost, which will end up
    // throttling events. set_throttle_input_on_resize_for_testing() disables
    // this.
    aura::Env::GetInstance()->set_throttle_input_on_resize_for_testing(false);

    GetAccessibilityController()->switch_access().SetEnabled(true);

    std::vector<ui::KeyboardDevice> keyboards;
    ui::DeviceDataManagerTestApi device_data_test_api;
    keyboards.emplace_back(1, ui::INPUT_DEVICE_INTERNAL, "");
    keyboards.emplace_back(2, ui::INPUT_DEVICE_USB, "");
    keyboards.emplace_back(3, ui::INPUT_DEVICE_BLUETOOTH, "");
    keyboards.emplace_back(4, ui::INPUT_DEVICE_UNKNOWN, "");
    device_data_test_api.SetKeyboardDevices(keyboards);
  }

  void SetKeyCodesForSwitchAccessCommand(
      std::map<int, std::set<std::string>> key_codes,
      SwitchAccessCommand command) {
    accessibility_event_rewriter().SetKeyCodesForSwitchAccessCommand(key_codes,
                                                                     command);
  }

  const std::map<int, std::set<ui::InputDeviceType>> GetKeyCodesToCapture() {
    return accessibility_event_rewriter()
        .switch_access_key_codes_to_capture_for_test();
  }

  const std::map<int, SwitchAccessCommand> GetCommandForKeyCodeMap() {
    return accessibility_event_rewriter()
        .key_code_to_switch_access_command_map_for_test();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         SwitchAccessAccessibilityEventRewriterTest,
                         testing::Bool());

TEST_P(SwitchAccessAccessibilityEventRewriterTest, CaptureSpecifiedKeys) {
  // Set keys for Switch Access to capture.
  SetKeyCodesForSwitchAccessCommand(
      {{ui::VKEY_1, {kSwitchAccessInternalDevice}},
       {ui::VKEY_2, {kSwitchAccessUsbDevice}}},
      SwitchAccessCommand::kSelect);

  EXPECT_FALSE(event_capturer().LastKeyEvent());

  // Press 1 from the internal keyboard.
  generator().PressKey(ui::VKEY_1, ui::EF_NONE, 1 /* keyboard id */);
  generator().ReleaseKey(ui::VKEY_1, ui::EF_NONE, 1 /* keyboard id */);

  // The event was captured by AccessibilityEventRewriter.
  EXPECT_FALSE(event_capturer().LastKeyEvent());
  EXPECT_EQ(
      SwitchAccessCommand::kSelect,
      accessibility_event_rewriter_delegate().switch_access_commands().back());

  accessibility_event_rewriter_delegate().ClearSwitchAccessCommands();

  // Press 1 from the bluetooth keyboard.
  generator().PressKey(ui::VKEY_1, ui::EF_NONE, 3 /* keyboard id */);
  generator().ReleaseKey(ui::VKEY_1, ui::EF_NONE, 3 /* keyboard id */);

  // The event was not captured by AccessibilityEventRewriter.
  EXPECT_TRUE(event_capturer().LastKeyEvent());
  EXPECT_EQ(
      0u,
      accessibility_event_rewriter_delegate().switch_access_commands().size());

  // Press the "2" key.
  generator().PressKey(ui::VKEY_2, ui::EF_NONE, 2 /* keyboard id */);
  generator().ReleaseKey(ui::VKEY_2, ui::EF_NONE, 2 /* keyboard id */);

  // The event was captured by AccessibilityEventRewriter.
  EXPECT_TRUE(event_capturer().LastKeyEvent());
  EXPECT_EQ(
      SwitchAccessCommand::kSelect,
      accessibility_event_rewriter_delegate().switch_access_commands().back());

  accessibility_event_rewriter_delegate().ClearSwitchAccessCommands();

  // Press the "3" key.
  generator().PressKey(ui::VKEY_3, ui::EF_NONE, 1 /* keyboard id */);
  generator().ReleaseKey(ui::VKEY_3, ui::EF_NONE, 1 /* keyboard id */);

  // The event was not captured by AccessibilityEventRewriter.
  EXPECT_TRUE(event_capturer().LastKeyEvent());
  EXPECT_EQ(
      0u,
      accessibility_event_rewriter_delegate().switch_access_commands().size());
}

TEST_P(SwitchAccessAccessibilityEventRewriterTest,
       KeysNoLongerCaptureAfterUpdate) {
  // Set Switch Access to capture the keys {1, 2, 3}.
  SetKeyCodesForSwitchAccessCommand(
      {{ui::VKEY_1, {kSwitchAccessInternalDevice}},
       {ui::VKEY_2, {kSwitchAccessInternalDevice}},
       {ui::VKEY_3, {kSwitchAccessInternalDevice}}},
      SwitchAccessCommand::kSelect);

  EXPECT_FALSE(event_capturer().LastKeyEvent());

  // Press the "1" key.
  generator().PressKey(ui::VKEY_1, ui::EF_NONE, 1 /* keyboard id */);
  generator().ReleaseKey(ui::VKEY_1, ui::EF_NONE, 1 /* keyboard id */);

  // The event was captured by AccessibilityEventRewriter.
  EXPECT_FALSE(event_capturer().LastKeyEvent());
  EXPECT_EQ(
      SwitchAccessCommand::kSelect,
      accessibility_event_rewriter_delegate().switch_access_commands().back());

  // Update the Switch Access keys to capture {2, 3, 4}.
  SetKeyCodesForSwitchAccessCommand(
      {{ui::VKEY_2, {kSwitchAccessInternalDevice}},
       {ui::VKEY_3, {kSwitchAccessInternalDevice}},
       {ui::VKEY_4, {kSwitchAccessInternalDevice}}},
      SwitchAccessCommand::kSelect);

  // Press the "1" key.
  generator().PressKey(ui::VKEY_1, ui::EF_NONE, 1 /* keyboard id */);
  generator().ReleaseKey(ui::VKEY_1, ui::EF_NONE, 1 /* keyboard id */);

  // We received a new event.

  // The event was NOT captured by AccessibilityEventRewriter.
  EXPECT_TRUE(event_capturer().LastKeyEvent());
  EXPECT_FALSE(event_capturer().LastKeyEvent()->handled());

  // Press the "4" key.
  event_capturer().ClearEvents();
  generator().PressKey(ui::VKEY_4, ui::EF_NONE, 1 /* keyboard id */);
  generator().ReleaseKey(ui::VKEY_4, ui::EF_NONE, 1 /* keyboard id */);

  // The event was captured by AccessibilityEventRewriter.
  EXPECT_FALSE(event_capturer().LastKeyEvent());
  EXPECT_EQ(
      SwitchAccessCommand::kSelect,
      accessibility_event_rewriter_delegate().switch_access_commands().back());
}

TEST_P(SwitchAccessAccessibilityEventRewriterTest,
       SetKeyCodesForSwitchAccessCommand) {
  // Both the key codes to capture and the command map should be empty.
  EXPECT_EQ(0u, GetKeyCodesToCapture().size());
  EXPECT_EQ(0u, GetCommandForKeyCodeMap().size());

  // Set key codes for Select command.
  SetKeyCodesForSwitchAccessCommand(
      {{ui::VKEY_0, {kSwitchAccessInternalDevice}},
       {ui::VKEY_S, {kSwitchAccessInternalDevice}}},
      SwitchAccessCommand::kSelect);

  // Check that values are added to both data structures.
  std::map<int, std::set<ui::InputDeviceType>> kc_to_capture =
      GetKeyCodesToCapture();
  EXPECT_EQ(2u, kc_to_capture.size());
  EXPECT_EQ(1u, kc_to_capture.count(48));
  EXPECT_EQ(1u, kc_to_capture.count(83));

  std::map<int, SwitchAccessCommand> command_map = GetCommandForKeyCodeMap();
  EXPECT_EQ(2u, command_map.size());
  EXPECT_EQ(SwitchAccessCommand::kSelect, command_map.at(48));
  EXPECT_EQ(SwitchAccessCommand::kSelect, command_map.at(83));

  // Set key codes for the Next command.
  SetKeyCodesForSwitchAccessCommand(
      {{ui::VKEY_1, {kSwitchAccessInternalDevice}},
       {ui::VKEY_N, {kSwitchAccessInternalDevice}}},
      SwitchAccessCommand::kNext);

  // Check that the new values are added and old values are not changed.
  kc_to_capture = GetKeyCodesToCapture();
  EXPECT_EQ(4u, kc_to_capture.size());
  EXPECT_EQ(1u, kc_to_capture.count(49));
  EXPECT_EQ(1u, kc_to_capture.count(78));

  command_map = GetCommandForKeyCodeMap();
  EXPECT_EQ(4u, command_map.size());
  EXPECT_EQ(SwitchAccessCommand::kNext, command_map.at(49));
  EXPECT_EQ(SwitchAccessCommand::kNext, command_map.at(78));

  // Set key codes for the Previous command. Re-use a key code from above.
  SetKeyCodesForSwitchAccessCommand(
      {{ui::VKEY_1, {kSwitchAccessInternalDevice}},
       {ui::VKEY_P, {kSwitchAccessInternalDevice}}},
      SwitchAccessCommand::kPrevious);

  // Check that '1' has been remapped to Previous.
  kc_to_capture = GetKeyCodesToCapture();
  EXPECT_EQ(5u, kc_to_capture.size());
  EXPECT_EQ(1u, kc_to_capture.count(49));
  EXPECT_EQ(1u, kc_to_capture.count(80));

  command_map = GetCommandForKeyCodeMap();
  EXPECT_EQ(5u, command_map.size());
  EXPECT_EQ(SwitchAccessCommand::kPrevious, command_map.at(49));
  EXPECT_EQ(SwitchAccessCommand::kPrevious, command_map.at(80));
  EXPECT_EQ(SwitchAccessCommand::kNext, command_map.at(78));

  // Set a new key code for the Select command.
  SetKeyCodesForSwitchAccessCommand(
      {{ui::VKEY_3, {kSwitchAccessInternalDevice}},
       {ui::VKEY_S, {kSwitchAccessInternalDevice}}},
      SwitchAccessCommand::kSelect);

  // Check that the previously set values for Select have been cleared.
  kc_to_capture = GetKeyCodesToCapture();
  EXPECT_EQ(5u, kc_to_capture.size());
  EXPECT_EQ(0u, kc_to_capture.count(48));
  EXPECT_EQ(1u, kc_to_capture.count(51));
  EXPECT_EQ(1u, kc_to_capture.count(83));

  command_map = GetCommandForKeyCodeMap();
  EXPECT_EQ(5u, command_map.size());
  EXPECT_EQ(SwitchAccessCommand::kSelect, command_map.at(51));
  EXPECT_EQ(SwitchAccessCommand::kSelect, command_map.at(83));
  EXPECT_EQ(command_map.end(), command_map.find(48));
}

TEST_P(SwitchAccessAccessibilityEventRewriterTest, RespectsModifierRemappings) {
  // Set Control to be Switch Access' next button.
  SetKeyCodesForSwitchAccessCommand(
      {{ui::VKEY_CONTROL, {kSwitchAccessInternalDevice}}},
      SwitchAccessCommand::kNext);

  // Set Alt to be Switch Access' select button.
  SetKeyCodesForSwitchAccessCommand(
      {{ui::VKEY_MENU /* Alt key */, {kSwitchAccessInternalDevice}}},
      SwitchAccessCommand::kSelect);

  // Map Control key to Alt.
  SetModifierRemapping(prefs::kLanguageRemapControlKeyTo,
                       ui::mojom::ModifierKey::kAlt);

  // Send a key event for Control.
  generator().PressKey(ui::VKEY_CONTROL, ui::EF_CONTROL_DOWN,
                       1 /* keyboard id */);
  // EventRewriterAsh actually omits the modifier flag on release.
  generator().ReleaseKey(ui::VKEY_CONTROL, ui::EF_NONE, 1 /* keyboard id */);

  // Verify Switch Access treated it like Alt.
  EXPECT_EQ(
      1u,
      accessibility_event_rewriter_delegate().switch_access_commands().size());
  EXPECT_EQ(
      SwitchAccessCommand::kSelect,
      accessibility_event_rewriter_delegate().switch_access_commands().back());

  // Send a key event for Alt.
  generator().PressKey(ui::VKEY_MENU, ui::EF_ALT_DOWN, 1 /* keyboard id */);
  // EventRewriterAsh actually omits the modifier flag on release.
  generator().ReleaseKey(ui::VKEY_MENU, ui::EF_NONE, 1 /* keyboard id */);

  // Verify Switch Access also treats that like Alt.
  EXPECT_EQ(
      2u,
      accessibility_event_rewriter_delegate().switch_access_commands().size());
  EXPECT_EQ(
      SwitchAccessCommand::kSelect,
      accessibility_event_rewriter_delegate().switch_access_commands().back());
}

TEST_P(SwitchAccessAccessibilityEventRewriterTest, UseFunctionKeyRemappings) {
  // Set BrowserForward to be Switch Access' next button.
  SetKeyCodesForSwitchAccessCommand(
      {{ui::VKEY_BROWSER_FORWARD, {kSwitchAccessInternalDevice}}},
      SwitchAccessCommand::kNext);

  // Set F2 (the underlying value for BrowserForward) to be Switch Access'
  // select button.
  SetKeyCodesForSwitchAccessCommand(
      {{ui::VKEY_F2, {kSwitchAccessInternalDevice}}},
      SwitchAccessCommand::kSelect);

  // Send a key event for F2.
  generator().PressKey(ui::VKEY_F2, ui::EF_NONE, 1 /* keyboard id */);
  generator().ReleaseKey(ui::VKEY_F2, ui::EF_NONE, 1 /* keyboard id */);

  // Verify Switch Access treated it like BrowserForward.
  EXPECT_EQ(
      1u,
      accessibility_event_rewriter_delegate().switch_access_commands().size());
  EXPECT_EQ(
      SwitchAccessCommand::kNext,
      accessibility_event_rewriter_delegate().switch_access_commands().back());

  // Send a key event for BrowserForward.
  generator().PressKey(ui::VKEY_BROWSER_FORWARD, ui::EF_NONE,
                       1 /* keyboard id */);
  generator().ReleaseKey(ui::VKEY_BROWSER_FORWARD, ui::EF_NONE,
                         1 /* keyboard id */);

  // Verify Switch Access also treats that like BrowserForward.
  EXPECT_EQ(
      2u,
      accessibility_event_rewriter_delegate().switch_access_commands().size());
  EXPECT_EQ(
      SwitchAccessCommand::kNext,
      accessibility_event_rewriter_delegate().switch_access_commands().back());
}

class MagnifierAccessibilityEventRewriterTest
    : public AccessibilityEventRewriterTestBase,
      public testing::WithParamInterface<bool> {
 public:
  MagnifierAccessibilityEventRewriterTest() {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          ash::features::kEnableKeyboardRewriterFix);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          ash::features::kEnableKeyboardRewriterFix);
    }
  }

  void SetUp() override {
    AccessibilityEventRewriterTestBase::SetUp();

    // This test triggers a resize of WindowTreeHost, which will end up
    // throttling events. set_throttle_input_on_resize_for_testing() disables
    // this.
    aura::Env::GetInstance()->set_throttle_input_on_resize_for_testing(false);

    GetAccessibilityController()->fullscreen_magnifier().SetEnabled(true);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         MagnifierAccessibilityEventRewriterTest,
                         testing::Bool());

TEST_P(MagnifierAccessibilityEventRewriterTest, CaptureKeys) {
  // Press and release Ctrl+Alt+Up.
  // Verify that the events are captured by AccessibilityEventRewriter.
  generator().PressModifierKeys(ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN);
  event_capturer().ClearEvents();

  generator().PressKey(ui::VKEY_UP, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN);
  EXPECT_FALSE(event_capturer().LastKeyEvent());
  EXPECT_EQ(
      MagnifierCommand::kMoveUp,
      accessibility_event_rewriter_delegate().magnifier_commands().back());

  generator().ReleaseKey(ui::VKEY_UP, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN);
  EXPECT_FALSE(event_capturer().LastKeyEvent());
  EXPECT_EQ(
      MagnifierCommand::kMoveStop,
      accessibility_event_rewriter_delegate().magnifier_commands().back());

  // Press and release Ctrl+Alt+Down.
  // Verify that the events are captured by AccessibilityEventRewriter.
  generator().PressKey(ui::VKEY_DOWN, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN);
  EXPECT_FALSE(event_capturer().LastKeyEvent());
  EXPECT_EQ(
      MagnifierCommand::kMoveDown,
      accessibility_event_rewriter_delegate().magnifier_commands().back());

  generator().ReleaseKey(ui::VKEY_DOWN, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN);
  EXPECT_FALSE(event_capturer().LastKeyEvent());
  EXPECT_EQ(
      MagnifierCommand::kMoveStop,
      accessibility_event_rewriter_delegate().magnifier_commands().back());

  generator().ReleaseModifierKeys(ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN);
  event_capturer().ClearEvents();

  // Press and release the "3" key.
  // Verify that the events are not captured by AccessibilityEventRewriter.
  generator().PressKey(ui::VKEY_3, ui::EF_NONE);
  EXPECT_TRUE(event_capturer().LastKeyEvent());

  generator().ReleaseKey(ui::VKEY_3, ui::EF_NONE);
  EXPECT_TRUE(event_capturer().LastKeyEvent());
}

}  // namespace ash
