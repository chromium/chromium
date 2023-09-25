// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/events/accessibility_event_rewriter.h"

#include <memory>
#include <vector>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/constants/ash_constants.h"
#include "ash/public/cpp/accessibility_event_rewriter_delegate.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/ash/fake_ime_keyboard.h"
#include "ui/events/ash/event_rewriter_ash.h"
#include "ui/events/ash/keyboard_capability.h"
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
#include "ui/events/test/event_generator.h"
#include "ui/events/test/test_event_rewriter.h"
#include "ui/events/types/event_type.h"

namespace ash {
namespace {

// A test implementation of the spoken feedback delegate interface.
// TODO(crbug/1116205): Merge ChromeVox and Switch Access test infrastructure
// below.
class ChromeVoxTestDelegate : public AccessibilityEventRewriterDelegate {
 public:
  ChromeVoxTestDelegate() = default;
  ChromeVoxTestDelegate(const ChromeVoxTestDelegate&) = delete;
  ChromeVoxTestDelegate& operator=(const ChromeVoxTestDelegate&) = delete;
  ~ChromeVoxTestDelegate() override = default;

  // Count of events sent to the delegate.
  size_t chromevox_recorded_event_count_ = 0;

  // Count of captured events sent to the delegate.
  size_t chromevox_captured_event_count_ = 0;

  // Last key event sent to ChromeVox.
  ui::Event* GetLastChromeVoxKeyEvent() {
    return last_chromevox_key_event_.get();
  }

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
  void SendSwitchAccessCommand(SwitchAccessCommand command) override {}
  void SendPointScanPoint(const gfx::PointF& point) override {}
  void SendMagnifierCommand(MagnifierCommand command) override {}

  std::unique_ptr<ui::Event> last_chromevox_key_event_;
};

}  // namespace

class ChromeVoxAccessibilityEventRewriterTest
    : public ash::AshTestBase,
      public ui::EventRewriterAsh::Delegate {
 public:
  ChromeVoxAccessibilityEventRewriterTest() {
    keyboard_capability_ =
        ui::KeyboardCapability::CreateStubKeyboardCapability();
    event_rewriter_ash_ = std::make_unique<ui::EventRewriterAsh>(
        this, keyboard_capability_.get(), nullptr, false, &fake_ime_keyboard_);
  }
  ChromeVoxAccessibilityEventRewriterTest(
      const ChromeVoxAccessibilityEventRewriterTest&) = delete;
  ChromeVoxAccessibilityEventRewriterTest& operator=(
      const ChromeVoxAccessibilityEventRewriterTest&) = delete;

  void SetUp() override {
    ash::AshTestBase::SetUp();
    generator_ = AshTestBase::GetEventGenerator();
    accessibility_event_rewriter_ =
        std::make_unique<AccessibilityEventRewriter>(event_rewriter_ash_.get(),
                                                     &delegate_);
    GetContext()->GetHost()->GetEventSource()->AddEventRewriter(
        accessibility_event_rewriter_.get());
    GetContext()->GetHost()->GetEventSource()->AddEventRewriter(
        &event_recorder_);
  }

  void TearDown() override {
    GetContext()->GetHost()->GetEventSource()->RemoveEventRewriter(
        &event_recorder_);
    GetContext()->GetHost()->GetEventSource()->RemoveEventRewriter(
        accessibility_event_rewriter_.get());
    accessibility_event_rewriter_.reset();
    generator_ = nullptr;
    ash::AshTestBase::TearDown();
  }

  size_t delegate_chromevox_recorded_event_count() {
    return delegate_.chromevox_recorded_event_count_;
  }

  size_t delegate_chromevox_captured_event_count() {
    return delegate_.chromevox_captured_event_count_;
  }

  void SetDelegateChromeVoxCaptureAllKeys(bool value) {
    accessibility_event_rewriter_->set_chromevox_capture_all_keys(value);
  }

  void ExpectCounts(size_t expected_recorded_count,
                    size_t expected_delegate_count,
                    size_t expected_captured_count) {
    EXPECT_EQ(expected_recorded_count,
              static_cast<size_t>(event_recorder_.events_seen()));
    EXPECT_EQ(expected_delegate_count,
              delegate_chromevox_recorded_event_count());
    EXPECT_EQ(expected_captured_count,
              delegate_chromevox_captured_event_count());
  }

  void SetModifierRemapping(const std::string& pref_name,
                            ui::mojom::ModifierKey value) {
    DCHECK_NE(ui::mojom::ModifierKey::kIsoLevel5ShiftMod3, value);
    modifier_remapping_[pref_name] = value;
  }

  bool RewriteEventForChromeVox(
      const ui::Event& event,
      const AccessibilityEventRewriter::Continuation continuation) {
    return accessibility_event_rewriter_->RewriteEventForChromeVox(
        event, continuation);
  }

  ui::KeyEvent* GetLastChromeVoxKeyEvent() {
    if (delegate_.GetLastChromeVoxKeyEvent())
      return delegate_.GetLastChromeVoxKeyEvent()->AsKeyEvent();
    return nullptr;
  }

  void SetRemapPositionalKeys(bool value) {
    accessibility_event_rewriter_
        ->try_rewriting_positional_keys_for_chromevox_ = value;
  }

 protected:
  // A test accessibility event delegate; simulates ChromeVox and Switch Access.
  ChromeVoxTestDelegate delegate_;
  // Generates ui::Events from simulated user input.
  raw_ptr<ui::test::EventGenerator, ExperimentalAsh> generator_ = nullptr;
  // Records events delivered to the next event rewriter after spoken feedback.
  ui::test::TestEventRewriter event_recorder_;

  std::unique_ptr<AccessibilityEventRewriter> accessibility_event_rewriter_;

  input_method::FakeImeKeyboard fake_ime_keyboard_;
  std::unique_ptr<ui::KeyboardCapability> keyboard_capability_;
  std::unique_ptr<ui::EventRewriterAsh> event_rewriter_ash_;

 private:
  // ui::EventRewriterAsh::Delegate:
  bool RewriteModifierKeys() override { return true; }
  void SuppressModifierKeyRewrites(bool should_suppress) override {}
  bool RewriteMetaTopRowKeyComboEvents(int device_id) const override {
    return true;
  }
  void SuppressMetaTopRowKeyComboRewrites(bool should_suppress) override {}

  absl::optional<ui::mojom::ModifierKey> GetKeyboardRemappedModifierValue(
      int device_id,
      ui::mojom::ModifierKey modifier_key,
      const std::string& pref_name) const override {
    auto it = modifier_remapping_.find(pref_name);
    if (it == modifier_remapping_.end())
      return absl::nullopt;

    return it->second;
  }

  bool TopRowKeysAreFunctionKeys(int device_id) const override { return false; }

  bool IsExtensionCommandRegistered(ui::KeyboardCode key_code,
                                    int flags) const override {
    return false;
  }

  bool IsSearchKeyAcceleratorReserved() const override { return false; }

  bool NotifyDeprecatedRightClickRewrite() override { return false; }
  bool NotifyDeprecatedSixPackKeyRewrite(ui::KeyboardCode key_code) override {
    return false;
  }
  void RecordEventRemappedToRightClick(bool alt_based_right_click) override {}
  void RecordSixPackEventRewrite(ui::KeyboardCode key_code,
                                 bool alt_based) override {}
  absl::optional<ui::mojom::SimulateRightClickModifier>
  GetRemapRightClickModifier(int device_id) override {
    return absl::nullopt;
  }

  absl::optional<ui::mojom::SixPackShortcutModifier>
  GetShortcutModifierForSixPackKey(int device_id,
                                   ui::KeyboardCode key_code) override {
    return absl::nullopt;
  }

  void NotifyRightClickRewriteBlockedBySetting(
      ui::mojom::SimulateRightClickModifier blocked_modifier,
      ui::mojom::SimulateRightClickModifier active_modifier) override {}

  void NotifySixPackRewriteBlockedBySetting(
      ui::KeyboardCode key_code,
      ui::mojom::SixPackShortcutModifier blocked_modifier,
      ui::mojom::SixPackShortcutModifier active_modifier,
      int device_id) override {}

  absl::optional<ui::mojom::ExtendedFkeysModifier> GetExtendedFkeySetting(
      int device_id,
      ui::KeyboardCode key_code) override {
    return absl::nullopt;
  }

  std::map<std::string, ui::mojom::ModifierKey> modifier_remapping_;
};

// The delegate should not intercept events when spoken feedback is disabled.
TEST_F(ChromeVoxAccessibilityEventRewriterTest, EventsNotConsumedWhenDisabled) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  EXPECT_FALSE(controller->spoken_feedback().enabled());

  generator_->PressKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_EQ(1, event_recorder_.events_seen());
  EXPECT_EQ(0U, delegate_chromevox_recorded_event_count());
  generator_->ReleaseKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_EQ(2, event_recorder_.events_seen());
  EXPECT_EQ(0U, delegate_chromevox_recorded_event_count());

  generator_->ClickLeftButton();
  EXPECT_EQ(4, event_recorder_.events_seen());
  EXPECT_EQ(0U, delegate_chromevox_recorded_event_count());

  generator_->GestureTapAt(gfx::Point());
  EXPECT_EQ(6, event_recorder_.events_seen());
  EXPECT_EQ(0U, delegate_chromevox_recorded_event_count());
}

// The delegate should intercept key events when spoken feedback is enabled.
TEST_F(ChromeVoxAccessibilityEventRewriterTest, KeyEventsConsumedWhenEnabled) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  controller->SetSpokenFeedbackEnabled(true, A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(controller->spoken_feedback().enabled());

  generator_->PressKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_EQ(1, event_recorder_.events_seen());
  EXPECT_EQ(1U, delegate_chromevox_recorded_event_count());
  EXPECT_EQ(0U, delegate_chromevox_captured_event_count());
  generator_->ReleaseKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_EQ(2, event_recorder_.events_seen());
  EXPECT_EQ(2U, delegate_chromevox_recorded_event_count());
  EXPECT_EQ(0U, delegate_chromevox_captured_event_count());

  generator_->ClickLeftButton();
  EXPECT_EQ(4, event_recorder_.events_seen());
  EXPECT_EQ(2U, delegate_chromevox_recorded_event_count());
  EXPECT_EQ(0U, delegate_chromevox_captured_event_count());

  generator_->GestureTapAt(gfx::Point());
  EXPECT_EQ(6, event_recorder_.events_seen());
  EXPECT_EQ(2U, delegate_chromevox_recorded_event_count());
  EXPECT_EQ(0U, delegate_chromevox_captured_event_count());
}

// Asynchronously unhandled events should be sent to subsequent rewriters.
TEST_F(ChromeVoxAccessibilityEventRewriterTest,
       UnhandledEventsSentToOtherRewriters) {
  // Before it can forward unhandled events, AccessibilityEventRewriter
  // must have seen at least one event in the first place.
  generator_->PressKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_EQ(1, event_recorder_.events_seen());
  generator_->ReleaseKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_EQ(2, event_recorder_.events_seen());

  accessibility_event_rewriter_->OnUnhandledSpokenFeedbackEvent(
      std::make_unique<ui::KeyEvent>(ui::ET_KEY_PRESSED, ui::VKEY_A,
                                     ui::EF_NONE));
  EXPECT_EQ(3, event_recorder_.events_seen());
  accessibility_event_rewriter_->OnUnhandledSpokenFeedbackEvent(
      std::make_unique<ui::KeyEvent>(ui::ET_KEY_RELEASED, ui::VKEY_A,
                                     ui::EF_NONE));
  EXPECT_EQ(4, event_recorder_.events_seen());
}

TEST_F(ChromeVoxAccessibilityEventRewriterTest,
       KeysNotEatenWithChromeVoxDisabled) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  EXPECT_FALSE(controller->spoken_feedback().enabled());

  // Send Search+Shift+Right.
  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  EXPECT_EQ(1, event_recorder_.events_seen());
  generator_->PressKey(ui::VKEY_SHIFT, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN);
  EXPECT_EQ(2, event_recorder_.events_seen());

  // Mock successful commands lookup and dispatch; shouldn't matter either way.
  generator_->PressKey(ui::VKEY_RIGHT, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN);
  EXPECT_EQ(3, event_recorder_.events_seen());

  // Released keys shouldn't get eaten.
  generator_->ReleaseKey(ui::VKEY_RIGHT,
                         ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN);
  generator_->ReleaseKey(ui::VKEY_SHIFT, ui::EF_COMMAND_DOWN);
  generator_->ReleaseKey(ui::VKEY_LWIN, 0);
  EXPECT_EQ(6, event_recorder_.events_seen());

  // Try releasing more keys.
  generator_->ReleaseKey(ui::VKEY_RIGHT, 0);
  generator_->ReleaseKey(ui::VKEY_SHIFT, 0);
  generator_->ReleaseKey(ui::VKEY_LWIN, 0);
  EXPECT_EQ(9, event_recorder_.events_seen());

  EXPECT_EQ(0U, delegate_chromevox_recorded_event_count());
}

TEST_F(ChromeVoxAccessibilityEventRewriterTest, KeyEventsCaptured) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  controller->SetSpokenFeedbackEnabled(true, A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(controller->spoken_feedback().enabled());

  // Initialize expected counts as variables for easier maintaiblity.
  size_t recorded_count = 0;
  size_t delegate_count = 0;
  size_t captured_count = 0;

  // Anything with Search gets captured.
  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  ExpectCounts(recorded_count, ++delegate_count, ++captured_count);
  generator_->ReleaseKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  ExpectCounts(recorded_count, ++delegate_count, ++captured_count);

  // Tab never gets captured.
  generator_->PressKey(ui::VKEY_TAB, ui::EF_NONE);
  ExpectCounts(++recorded_count, ++delegate_count, captured_count);
  generator_->ReleaseKey(ui::VKEY_TAB, ui::EF_NONE);
  ExpectCounts(++recorded_count, ++delegate_count, captured_count);

  // A client requested capture of all keys.
  SetDelegateChromeVoxCaptureAllKeys(true);
  generator_->PressKey(ui::VKEY_A, ui::EF_NONE);
  ExpectCounts(recorded_count, ++delegate_count, ++captured_count);
  generator_->ReleaseKey(ui::VKEY_A, ui::EF_NONE);
  ExpectCounts(recorded_count, ++delegate_count, ++captured_count);

  // Tab never gets captured even with explicit client request for all keys.
  generator_->PressKey(ui::VKEY_TAB, ui::EF_NONE);
  ExpectCounts(++recorded_count, ++delegate_count, captured_count);
  generator_->ReleaseKey(ui::VKEY_TAB, ui::EF_NONE);
  ExpectCounts(++recorded_count, ++delegate_count, captured_count);

  // A client requested to not capture all keys.
  SetDelegateChromeVoxCaptureAllKeys(false);
  generator_->PressKey(ui::VKEY_A, ui::EF_NONE);
  ExpectCounts(++recorded_count, ++delegate_count, captured_count);
  generator_->ReleaseKey(ui::VKEY_A, ui::EF_NONE);
  ExpectCounts(++recorded_count, ++delegate_count, captured_count);
}

TEST_F(ChromeVoxAccessibilityEventRewriterTest,
       KeyEventsCapturedWithModifierRemapping) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
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
  generator_->PressKey(ui::VKEY_CONTROL, ui::EF_CONTROL_DOWN);
  ExpectCounts(recorded_count, ++delegate_count, ++captured_count);
  // EventRewriterAsh actually omits the modifier flag.
  generator_->ReleaseKey(ui::VKEY_CONTROL, 0);
  ExpectCounts(recorded_count, ++delegate_count, ++captured_count);

  // Search itself should also work.
  generator_->PressKey(ui::VKEY_LWIN, ui::EF_COMMAND_DOWN);
  ExpectCounts(recorded_count, ++delegate_count, ++captured_count);
  generator_->ReleaseKey(ui::VKEY_LWIN, 0);
  ExpectCounts(recorded_count, ++delegate_count, ++captured_count);

  // Remapping should have no effect on all other expectations.

  // Tab never gets captured.
  generator_->PressKey(ui::VKEY_TAB, ui::EF_NONE);
  ExpectCounts(++recorded_count, ++delegate_count, captured_count);
  generator_->ReleaseKey(ui::VKEY_TAB, ui::EF_NONE);
  ExpectCounts(++recorded_count, ++delegate_count, captured_count);

  // A client requested capture of all keys.
  SetDelegateChromeVoxCaptureAllKeys(true);
  generator_->PressKey(ui::VKEY_A, ui::EF_NONE);
  ExpectCounts(recorded_count, ++delegate_count, ++captured_count);
  generator_->ReleaseKey(ui::VKEY_A, ui::EF_NONE);
  ExpectCounts(recorded_count, ++delegate_count, ++captured_count);

  // Tab never gets captured even with explicit client request for all keys.
  generator_->PressKey(ui::VKEY_TAB, ui::EF_NONE);
  ExpectCounts(++recorded_count, ++delegate_count, captured_count);
  generator_->ReleaseKey(ui::VKEY_TAB, ui::EF_NONE);
  ExpectCounts(++recorded_count, ++delegate_count, captured_count);

  // A client requested to not capture all keys.
  SetDelegateChromeVoxCaptureAllKeys(false);
  generator_->PressKey(ui::VKEY_A, ui::EF_NONE);
  ExpectCounts(++recorded_count, ++delegate_count, captured_count);
  generator_->ReleaseKey(ui::VKEY_A, ui::EF_NONE);
  ExpectCounts(++recorded_count, ++delegate_count, captured_count);
}

TEST_F(ChromeVoxAccessibilityEventRewriterTest,
       PositionalInputMethodKeysMightBeRewritten) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  controller->SetSpokenFeedbackEnabled(true, A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(controller->spoken_feedback().enabled());

  // Ensure remapping of positional keys (back to their absolute key codes) is
  // in effect.
  SetRemapPositionalKeys(true);

  // Bracket right is a dom code which always causes an absolute key code to be
  // sent to ChromeVox. The key code below is irrelevant.
  ui::KeyEvent key_event(ui::ET_KEY_PRESSED, ui::VKEY_A,
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

// Records all key events for testing.
class EventCapturer : public ui::EventHandler {
 public:
  EventCapturer() = default;
  EventCapturer(const EventCapturer&) = delete;
  EventCapturer& operator=(const EventCapturer&) = delete;
  ~EventCapturer() override = default;

  void Reset() { last_key_event_.reset(); }

  ui::KeyEvent* last_key_event() { return last_key_event_.get(); }

 private:
  void OnKeyEvent(ui::KeyEvent* event) override {
    last_key_event_ = std::make_unique<ui::KeyEvent>(*event);
  }

  std::unique_ptr<ui::KeyEvent> last_key_event_;
};

class SwitchAccessTestDelegate : public AccessibilityEventRewriterDelegate {
 public:
  SwitchAccessTestDelegate() = default;
  SwitchAccessTestDelegate(const SwitchAccessTestDelegate&) = delete;
  SwitchAccessTestDelegate& operator=(const SwitchAccessTestDelegate&) = delete;
  ~SwitchAccessTestDelegate() override = default;

  SwitchAccessCommand last_command() { return commands_.back(); }
  int command_count() { return commands_.size(); }

  void ClearCommands() { commands_.clear(); }

  // AccessibilityEventRewriterDelegate:
  void SendSwitchAccessCommand(SwitchAccessCommand command) override {
    commands_.push_back(command);
  }
  void SendPointScanPoint(const gfx::PointF& point) override {}
  void SendMagnifierCommand(MagnifierCommand command) override {}
  void DispatchKeyEventToChromeVox(std::unique_ptr<ui::Event>, bool) override {}
  void DispatchMouseEvent(std::unique_ptr<ui::Event>) override {}

 private:
  std::vector<SwitchAccessCommand> commands_;
};

class SwitchAccessAccessibilityEventRewriterTest
    : public AshTestBase,
      public ui::EventRewriterAsh::Delegate {
 public:
  SwitchAccessAccessibilityEventRewriterTest() {
    keyboard_capability_ =
        ui::KeyboardCapability::CreateStubKeyboardCapability();
    event_rewriter_ash_ = std::make_unique<ui::EventRewriterAsh>(
        this, keyboard_capability_.get(), nullptr, false, &fake_ime_keyboard_);
  }
  ~SwitchAccessAccessibilityEventRewriterTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    // This test triggers a resize of WindowTreeHost, which will end up
    // throttling events. set_throttle_input_on_resize_for_testing() disables
    // this.
    aura::Env::GetInstance()->set_throttle_input_on_resize_for_testing(false);

    delegate_ = std::make_unique<SwitchAccessTestDelegate>();
    accessibility_event_rewriter_ =
        std::make_unique<AccessibilityEventRewriter>(event_rewriter_ash_.get(),
                                                     delegate_.get());
    generator_ = AshTestBase::GetEventGenerator();
    GetContext()->AddPreTargetHandler(&event_capturer_);

    GetContext()->GetHost()->GetEventSource()->AddEventRewriter(
        accessibility_event_rewriter_.get());

    controller_ = Shell::Get()->accessibility_controller();
    controller_->SetAccessibilityEventRewriter(
        accessibility_event_rewriter_.get());
    controller_->switch_access().SetEnabled(true);

    std::vector<ui::KeyboardDevice> keyboards;
    ui::DeviceDataManagerTestApi device_data_test_api;
    keyboards.emplace_back(1, ui::INPUT_DEVICE_INTERNAL, "");
    keyboards.emplace_back(2, ui::INPUT_DEVICE_USB, "");
    keyboards.emplace_back(3, ui::INPUT_DEVICE_BLUETOOTH, "");
    keyboards.emplace_back(4, ui::INPUT_DEVICE_UNKNOWN, "");
    device_data_test_api.SetKeyboardDevices(keyboards);
  }

  void TearDown() override {
    GetContext()->RemovePreTargetHandler(&event_capturer_);
    generator_ = nullptr;
    controller_ = nullptr;
    accessibility_event_rewriter_.reset();
    AshTestBase::TearDown();
  }

  void SetKeyCodesForSwitchAccessCommand(
      std::map<int, std::set<std::string>> key_codes,
      SwitchAccessCommand command) {
    AccessibilityEventRewriter* rewriter =
        controller_->GetAccessibilityEventRewriterForTest();
    rewriter->SetKeyCodesForSwitchAccessCommand(key_codes, command);
  }

  void SetModifierRemapping(const std::string& pref_name,
                            ui::mojom::ModifierKey value) {
    DCHECK_NE(ui::mojom::ModifierKey::kIsoLevel5ShiftMod3, value);
    modifier_remapping_[pref_name] = value;
  }

  const std::map<int, std::set<ui::InputDeviceType>> GetKeyCodesToCapture() {
    AccessibilityEventRewriter* rewriter =
        controller_->GetAccessibilityEventRewriterForTest();
    if (rewriter)
      return rewriter->switch_access_key_codes_to_capture_for_test();
    return std::map<int, std::set<ui::InputDeviceType>>();
  }

  const std::map<int, SwitchAccessCommand> GetCommandForKeyCodeMap() {
    AccessibilityEventRewriter* rewriter =
        controller_->GetAccessibilityEventRewriterForTest();
    if (rewriter)
      return rewriter->key_code_to_switch_access_command_map_for_test();
    return std::map<int, SwitchAccessCommand>();
  }

 private:
  // ui::EventRewriterAsh::Delegate:
  bool RewriteModifierKeys() override { return true; }
  void SuppressModifierKeyRewrites(bool should_suppress) override {}
  bool RewriteMetaTopRowKeyComboEvents(int device_id) const override {
    return true;
  }
  void SuppressMetaTopRowKeyComboRewrites(bool should_suppress) override {}

  absl::optional<ui::mojom::ModifierKey> GetKeyboardRemappedModifierValue(
      int device_id,
      ui::mojom::ModifierKey modifier_key,
      const std::string& pref_name) const override {
    auto it = modifier_remapping_.find(pref_name);
    if (it == modifier_remapping_.end())
      return absl::nullopt;

    return it->second;
  }

  bool TopRowKeysAreFunctionKeys(int device_id) const override { return false; }

  bool IsExtensionCommandRegistered(ui::KeyboardCode key_code,
                                    int flags) const override {
    return false;
  }

  bool IsSearchKeyAcceleratorReserved() const override { return false; }

  bool NotifyDeprecatedRightClickRewrite() override { return false; }
  bool NotifyDeprecatedSixPackKeyRewrite(ui::KeyboardCode key_code) override {
    return false;
  }

  void RecordEventRemappedToRightClick(bool alt_based_right_click) override {}
  void RecordSixPackEventRewrite(ui::KeyboardCode key_code,
                                 bool alt_based) override {}
  absl::optional<ui::mojom::SimulateRightClickModifier>
  GetRemapRightClickModifier(int device_id) override {
    return absl::nullopt;
  }

  absl::optional<ui::mojom::SixPackShortcutModifier>
  GetShortcutModifierForSixPackKey(int device_id,
                                   ui::KeyboardCode key_code) override {
    return absl::nullopt;
  }

  void NotifyRightClickRewriteBlockedBySetting(
      ui::mojom::SimulateRightClickModifier blocked_modifier,
      ui::mojom::SimulateRightClickModifier active_modifier) override {}

  void NotifySixPackRewriteBlockedBySetting(
      ui::KeyboardCode key_code,
      ui::mojom::SixPackShortcutModifier blocked_modifier,
      ui::mojom::SixPackShortcutModifier active_modifier,
      int device_id) override {}

  absl::optional<ui::mojom::ExtendedFkeysModifier> GetExtendedFkeySetting(
      int device_id,
      ui::KeyboardCode key_code) override {
    return absl::nullopt;
  }

  std::map<std::string, ui::mojom::ModifierKey> modifier_remapping_;

 protected:
  raw_ptr<ui::test::EventGenerator, ExperimentalAsh> generator_ = nullptr;
  EventCapturer event_capturer_;
  raw_ptr<AccessibilityControllerImpl, ExperimentalAsh> controller_ = nullptr;
  std::unique_ptr<SwitchAccessTestDelegate> delegate_;
  input_method::FakeImeKeyboard fake_ime_keyboard_;
  std::unique_ptr<AccessibilityEventRewriter> accessibility_event_rewriter_;
  std::unique_ptr<ui::KeyboardCapability> keyboard_capability_;
  std::unique_ptr<ui::EventRewriterAsh> event_rewriter_ash_;
};

TEST_F(SwitchAccessAccessibilityEventRewriterTest, CaptureSpecifiedKeys) {
  // Set keys for Switch Access to capture.
  SetKeyCodesForSwitchAccessCommand(
      {{ui::VKEY_1, {kSwitchAccessInternalDevice}},
       {ui::VKEY_2, {kSwitchAccessUsbDevice}}},
      SwitchAccessCommand::kSelect);

  EXPECT_FALSE(event_capturer_.last_key_event());

  // Press 1 from the internal keyboard.
  generator_->PressKey(ui::VKEY_1, ui::EF_NONE, 1 /* keyboard id */);
  generator_->ReleaseKey(ui::VKEY_1, ui::EF_NONE, 1 /* keyboard id */);

  // The event was captured by AccessibilityEventRewriter.
  EXPECT_FALSE(event_capturer_.last_key_event());
  EXPECT_EQ(SwitchAccessCommand::kSelect, delegate_->last_command());

  delegate_->ClearCommands();

  // Press 1 from the bluetooth keyboard.
  generator_->PressKey(ui::VKEY_1, ui::EF_NONE, 3 /* keyboard id */);
  generator_->ReleaseKey(ui::VKEY_1, ui::EF_NONE, 3 /* keyboard id */);

  // The event was not captured by AccessibilityEventRewriter.
  EXPECT_TRUE(event_capturer_.last_key_event());
  EXPECT_EQ(0, delegate_->command_count());

  // Press the "2" key.
  generator_->PressKey(ui::VKEY_2, ui::EF_NONE, 2 /* keyboard id */);
  generator_->ReleaseKey(ui::VKEY_2, ui::EF_NONE, 2 /* keyboard id */);

  // The event was captured by AccessibilityEventRewriter.
  EXPECT_TRUE(event_capturer_.last_key_event());
  EXPECT_EQ(SwitchAccessCommand::kSelect, delegate_->last_command());

  delegate_->ClearCommands();

  // Press the "3" key.
  generator_->PressKey(ui::VKEY_3, ui::EF_NONE, 1 /* keyboard id */);
  generator_->ReleaseKey(ui::VKEY_3, ui::EF_NONE, 1 /* keyboard id */);

  // The event was not captured by AccessibilityEventRewriter.
  EXPECT_TRUE(event_capturer_.last_key_event());
  EXPECT_EQ(0, delegate_->command_count());
}

TEST_F(SwitchAccessAccessibilityEventRewriterTest,
       KeysNoLongerCaptureAfterUpdate) {
  // Set Switch Access to capture the keys {1, 2, 3}.
  SetKeyCodesForSwitchAccessCommand(
      {{ui::VKEY_1, {kSwitchAccessInternalDevice}},
       {ui::VKEY_2, {kSwitchAccessInternalDevice}},
       {ui::VKEY_3, {kSwitchAccessInternalDevice}}},
      SwitchAccessCommand::kSelect);

  EXPECT_FALSE(event_capturer_.last_key_event());

  // Press the "1" key.
  generator_->PressKey(ui::VKEY_1, ui::EF_NONE, 1 /* keyboard id */);
  generator_->ReleaseKey(ui::VKEY_1, ui::EF_NONE, 1 /* keyboard id */);

  // The event was captured by AccessibilityEventRewriter.
  EXPECT_FALSE(event_capturer_.last_key_event());
  EXPECT_EQ(SwitchAccessCommand::kSelect, delegate_->last_command());

  // Update the Switch Access keys to capture {2, 3, 4}.
  SetKeyCodesForSwitchAccessCommand(
      {{ui::VKEY_2, {kSwitchAccessInternalDevice}},
       {ui::VKEY_3, {kSwitchAccessInternalDevice}},
       {ui::VKEY_4, {kSwitchAccessInternalDevice}}},
      SwitchAccessCommand::kSelect);

  // Press the "1" key.
  generator_->PressKey(ui::VKEY_1, ui::EF_NONE, 1 /* keyboard id */);
  generator_->ReleaseKey(ui::VKEY_1, ui::EF_NONE, 1 /* keyboard id */);

  // We received a new event.

  // The event was NOT captured by AccessibilityEventRewriter.
  EXPECT_TRUE(event_capturer_.last_key_event());
  EXPECT_FALSE(event_capturer_.last_key_event()->handled());

  // Press the "4" key.
  event_capturer_.Reset();
  generator_->PressKey(ui::VKEY_4, ui::EF_NONE, 1 /* keyboard id */);
  generator_->ReleaseKey(ui::VKEY_4, ui::EF_NONE, 1 /* keyboard id */);

  // The event was captured by AccessibilityEventRewriter.
  EXPECT_FALSE(event_capturer_.last_key_event());
  EXPECT_EQ(SwitchAccessCommand::kSelect, delegate_->last_command());
}

TEST_F(SwitchAccessAccessibilityEventRewriterTest,
       SetKeyCodesForSwitchAccessCommand) {
  AccessibilityEventRewriter* rewriter =
      controller_->GetAccessibilityEventRewriterForTest();
  EXPECT_NE(nullptr, rewriter);

  // Both the key codes to capture and the command map should be empty.
  EXPECT_EQ(0u, GetKeyCodesToCapture().size());
  EXPECT_EQ(0u, GetCommandForKeyCodeMap().size());

  // Set key codes for Select command.
  std::map<int, std::set<std::string>> new_key_codes;
  new_key_codes[48 /* '0' */] = {kSwitchAccessInternalDevice};
  new_key_codes[83 /* 's' */] = {kSwitchAccessInternalDevice};
  rewriter->SetKeyCodesForSwitchAccessCommand(new_key_codes,
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
  new_key_codes.clear();
  new_key_codes[49 /* '1' */] = {kSwitchAccessInternalDevice};
  new_key_codes[78 /* 'n' */] = {kSwitchAccessInternalDevice};
  rewriter->SetKeyCodesForSwitchAccessCommand(new_key_codes,
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
  new_key_codes.clear();
  new_key_codes[49 /* '1' */] = {kSwitchAccessInternalDevice};
  new_key_codes[80 /* 'p' */] = {kSwitchAccessInternalDevice};
  rewriter->SetKeyCodesForSwitchAccessCommand(new_key_codes,
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
  new_key_codes.clear();
  new_key_codes[51 /* '3' */] = {kSwitchAccessInternalDevice};
  new_key_codes[83 /* 's' */] = {kSwitchAccessInternalDevice};
  rewriter->SetKeyCodesForSwitchAccessCommand(new_key_codes,
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

TEST_F(SwitchAccessAccessibilityEventRewriterTest, RespectsModifierRemappings) {
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
  generator_->PressKey(ui::VKEY_CONTROL, ui::EF_CONTROL_DOWN,
                       1 /* keyboard id */);
  // EventRewriterAsh actually omits the modifier flag on release.
  generator_->ReleaseKey(ui::VKEY_CONTROL, ui::EF_NONE, 1 /* keyboard id */);

  // Verify Switch Access treated it like Alt.
  EXPECT_EQ(1, delegate_->command_count());
  EXPECT_EQ(SwitchAccessCommand::kSelect, delegate_->last_command());

  // Send a key event for Alt.
  generator_->PressKey(ui::VKEY_MENU, ui::EF_ALT_DOWN, 1 /* keyboard id */);
  // EventRewriterAsh actually omits the modifier flag on release.
  generator_->ReleaseKey(ui::VKEY_MENU, ui::EF_NONE, 1 /* keyboard id */);

  // Verify Switch Access also treats that like Alt.
  EXPECT_EQ(2, delegate_->command_count());
  EXPECT_EQ(SwitchAccessCommand::kSelect, delegate_->last_command());
}

TEST_F(SwitchAccessAccessibilityEventRewriterTest, UseFunctionKeyRemappings) {
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
  generator_->PressKey(ui::VKEY_F2, ui::EF_NONE, 1 /* keyboard id */);
  generator_->ReleaseKey(ui::VKEY_F2, ui::EF_NONE, 1 /* keyboard id */);

  // Verify Switch Access treated it like BrowserForward.
  EXPECT_EQ(1, delegate_->command_count());
  EXPECT_EQ(SwitchAccessCommand::kNext, delegate_->last_command());

  // Send a key event for BrowserForward.
  generator_->PressKey(ui::VKEY_BROWSER_FORWARD, ui::EF_NONE,
                       1 /* keyboard id */);
  generator_->ReleaseKey(ui::VKEY_BROWSER_FORWARD, ui::EF_NONE,
                         1 /* keyboard id */);

  // Verify Switch Access also treats that like BrowserForward.
  EXPECT_EQ(2, delegate_->command_count());
  EXPECT_EQ(SwitchAccessCommand::kNext, delegate_->last_command());
}

class MagnifierTestDelegate : public AccessibilityEventRewriterDelegate {
 public:
  MagnifierTestDelegate() = default;
  MagnifierTestDelegate(const MagnifierTestDelegate&) = delete;
  MagnifierTestDelegate& operator=(const MagnifierTestDelegate&) = delete;
  ~MagnifierTestDelegate() override = default;

  MagnifierCommand last_command() { return commands_.back(); }
  int command_count() { return commands_.size(); }

  // AccessibilityEventRewriterDelegate:
  void SendSwitchAccessCommand(SwitchAccessCommand command) override {}
  void SendPointScanPoint(const gfx::PointF& point) override {}
  void SendMagnifierCommand(MagnifierCommand command) override {
    commands_.push_back(command);
  }
  void DispatchKeyEventToChromeVox(std::unique_ptr<ui::Event>, bool) override {}
  void DispatchMouseEvent(std::unique_ptr<ui::Event>) override {}

 private:
  std::vector<MagnifierCommand> commands_;
};

class MagnifierAccessibilityEventRewriterTest : public AshTestBase {
 public:
  MagnifierAccessibilityEventRewriterTest() {
    keyboard_capability_ =
        ui::KeyboardCapability::CreateStubKeyboardCapability();
    event_rewriter_ash_ = std::make_unique<ui::EventRewriterAsh>(
        nullptr, keyboard_capability_.get(), nullptr, false,
        &fake_ime_keyboard_);
  }
  ~MagnifierAccessibilityEventRewriterTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    // This test triggers a resize of WindowTreeHost, which will end up
    // throttling events. set_throttle_input_on_resize_for_testing() disables
    // this.
    aura::Env::GetInstance()->set_throttle_input_on_resize_for_testing(false);

    delegate_ = std::make_unique<MagnifierTestDelegate>();
    accessibility_event_rewriter_ =
        std::make_unique<AccessibilityEventRewriter>(event_rewriter_ash_.get(),
                                                     delegate_.get());
    generator_ = AshTestBase::GetEventGenerator();
    GetContext()->AddPreTargetHandler(&event_capturer_);

    GetContext()->GetHost()->GetEventSource()->AddEventRewriter(
        accessibility_event_rewriter_.get());

    controller_ = Shell::Get()->accessibility_controller();
    controller_->SetAccessibilityEventRewriter(
        accessibility_event_rewriter_.get());
    controller_->fullscreen_magnifier().SetEnabled(true);
  }

  void TearDown() override {
    GetContext()->RemovePreTargetHandler(&event_capturer_);
    generator_ = nullptr;
    controller_ = nullptr;
    accessibility_event_rewriter_.reset();
    AshTestBase::TearDown();
  }

 protected:
  raw_ptr<ui::test::EventGenerator, ExperimentalAsh> generator_ = nullptr;
  EventCapturer event_capturer_;
  raw_ptr<AccessibilityControllerImpl, ExperimentalAsh> controller_ = nullptr;
  std::unique_ptr<MagnifierTestDelegate> delegate_;
  input_method::FakeImeKeyboard fake_ime_keyboard_;
  std::unique_ptr<AccessibilityEventRewriter> accessibility_event_rewriter_;
  std::unique_ptr<ui::KeyboardCapability> keyboard_capability_;
  std::unique_ptr<ui::EventRewriterAsh> event_rewriter_ash_;
};

TEST_F(MagnifierAccessibilityEventRewriterTest, CaptureKeys) {
  // Press and release Ctrl+Alt+Up.
  // Verify that the events are captured by AccessibilityEventRewriter.
  generator_->PressKey(ui::VKEY_UP, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN);
  EXPECT_FALSE(event_capturer_.last_key_event());
  EXPECT_EQ(MagnifierCommand::kMoveUp, delegate_->last_command());

  generator_->ReleaseKey(ui::VKEY_UP, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN);
  EXPECT_FALSE(event_capturer_.last_key_event());
  EXPECT_EQ(MagnifierCommand::kMoveStop, delegate_->last_command());

  // Press and release Ctrl+Alt+Down.
  // Verify that the events are captured by AccessibilityEventRewriter.
  generator_->PressKey(ui::VKEY_DOWN, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN);
  EXPECT_FALSE(event_capturer_.last_key_event());
  EXPECT_EQ(MagnifierCommand::kMoveDown, delegate_->last_command());

  generator_->ReleaseKey(ui::VKEY_DOWN, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN);
  EXPECT_FALSE(event_capturer_.last_key_event());
  EXPECT_EQ(MagnifierCommand::kMoveStop, delegate_->last_command());

  // Press and release the "3" key.
  // Verify that the events are not captured by AccessibilityEventRewriter.
  generator_->PressKey(ui::VKEY_3, ui::EF_NONE);
  EXPECT_TRUE(event_capturer_.last_key_event());

  generator_->ReleaseKey(ui::VKEY_3, ui::EF_NONE);
  EXPECT_TRUE(event_capturer_.last_key_event());
}

}  // namespace ash
