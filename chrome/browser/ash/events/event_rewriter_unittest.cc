// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/event_rewriter.h"

#include <memory>
#include <optional>
#include <vector>

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/accessibility/sticky_keys/sticky_keys_controller.h"
#include "ash/accessibility/sticky_keys/sticky_keys_overlay.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/input_device_settings_controller.h"
#include "ash/public/cpp/test/mock_input_device_settings_controller.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/shell.h"
#include "ash/system/input_device_settings/input_device_settings_notification_controller.h"
#include "ash/system/input_device_settings/input_device_settings_pref_names.h"
#include "ash/system/toast/anchored_nudge_manager_impl.h"
#include "base/command_line.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/events/event_rewriter_delegate_impl.h"
#include "chrome/browser/ash/input_method/input_method_configuration.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/notifications/deprecation_notification_controller.h"
#include "chrome/browser/ash/preferences/preferences.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "components/prefs/pref_member.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "device/udev_linux/fake_udev_loader.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/ime/ash/fake_ime_keyboard.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/ime/ash/mock_input_method_manager.h"
#include "ui/base/ime/ash/mock_input_method_manager_impl.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/ash/caps_lock_event_rewriter.h"
#include "ui/events/ash/discard_key_event_rewriter.h"
#include "ui/events/ash/event_rewriter_ash.h"
#include "ui/events/ash/event_rewriter_metrics.h"
#include "ui/events/ash/keyboard_capability.h"
#include "ui/events/ash/keyboard_device_id_event_rewriter.h"
#include "ui/events/ash/keyboard_modifier_event_rewriter.h"
#include "ui/events/ash/mojom/extended_fkeys_modifier.mojom-shared.h"
#include "ui/events/ash/mojom/modifier_key.mojom-shared.h"
#include "ui/events/ash/mojom/modifier_key.mojom.h"
#include "ui/events/ash/mojom/simulate_right_click_modifier.mojom-shared.h"
#include "ui/events/ash/mojom/six_pack_shortcut_modifier.mojom-shared.h"
#include "ui/events/ash/pref_names.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/keyboard_device.h"
#include "ui/events/devices/touchpad_device.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"
#include "ui/events/test/events_test_utils.h"
#include "ui/events/test/test_event_processor.h"
#include "ui/events/test/test_event_rewriter_continuation.h"
#include "ui/events/test/test_event_source.h"
#include "ui/events/types/event_type.h"
#include "ui/lottie/resource.h"
#include "ui/message_center/fake_message_center.h"
#include "ui/wm/core/window_util.h"

namespace {

constexpr int kKeyboardDeviceId = 123;
constexpr uint32_t kNoScanCode = 0;
constexpr char kKbdSysPath[] = "/devices/platform/i8042/serio2/input/input1";
constexpr char kKbdTopRowPropertyName[] = "CROS_KEYBOARD_TOP_ROW_LAYOUT";
constexpr char kKbdTopRowLayoutAttributeName[] = "function_row_physmap";
constexpr char kSixPackKeyNoMatchNudgeId[] = "six-patch-key-no-match-nudge-id";
constexpr char kTopRowKeyNoMatchNudgeId[] = "top-row-key-no-match-nudge-id";

constexpr char kKbdTopRowLayoutUnspecified[] = "";
constexpr char kKbdTopRowLayout1Tag[] = "1";
constexpr char kKbdTopRowLayout2Tag[] = "2";
constexpr char kKbdTopRowLayoutWilcoTag[] = "3";
constexpr char kKbdTopRowLayoutDrallionTag[] = "4";

constexpr int kTouchpadId1 = 10;
constexpr int kTouchpadId2 = 11;

constexpr int kMouseDeviceId = 456;

// A default example of the layout string read from the function_row_physmap
// sysfs attribute. The values represent the scan codes for each position
// in the top row, which maps to F-Keys.
constexpr char kKbdDefaultCustomTopRowLayout[] =
    "01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f";

// Tag used to mark events as being for right alt.
constexpr std::pair<std::string, std::vector<uint8_t>> kPropertyRightAlt = {
    "right_alt_event",
    {}};

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
constexpr char kCros1pInputMethodIdPrefix[] =
    "_comp_ime_jkghodnilhceideoidjikpgommlajknk";
#endif

class TestEventSink : public ui::EventSink {
 public:
  TestEventSink() = default;
  TestEventSink(const TestEventSink&) = delete;
  TestEventSink& operator=(const TestEventSink&) = delete;
  ~TestEventSink() override = default;

  // Returns the recorded events.
  std::vector<std::unique_ptr<ui::Event>> TakeEvents() {
    return std::move(events_);
  }

  // ui::EventSink:
  ui::EventDispatchDetails OnEventFromSource(ui::Event* event) override {
    auto cloned_event = event->Clone();
    ui::EventTestApi(cloned_event.get())
        .set_native_event(event->native_event());
    events_.emplace_back(std::move(cloned_event));
    return ui::EventDispatchDetails();
  }

 private:
  std::vector<std::unique_ptr<ui::Event>> events_;
};

class TestKeyboardModifierEventRewriterDelegate
    : public ui::KeyboardModifierEventRewriter::Delegate {
 public:
  explicit TestKeyboardModifierEventRewriterDelegate(
      ui::EventRewriterAsh::Delegate* ash_delegate)
      : ash_delegate_(ash_delegate) {}

  std::optional<ui::mojom::ModifierKey> GetKeyboardRemappedModifierValue(
      int device_id,
      ui::mojom::ModifierKey modifier_key,
      const std::string& pref_name) const override {
    return ash_delegate_->GetKeyboardRemappedModifierValue(
        device_id, modifier_key, pref_name);
  }

  bool RewriteModifierKeys() override {
    return ash_delegate_->RewriteModifierKeys();
  }

 private:
  raw_ptr<ui::EventRewriterAsh::Delegate> ash_delegate_;
};

class TestEventRewriterContinuation
    : public ui::test::TestEventRewriterContinuation {
 public:
  TestEventRewriterContinuation() = default;
  ~TestEventRewriterContinuation() override = default;
  TestEventRewriterContinuation(const TestEventRewriterContinuation&) = delete;
  TestEventRewriterContinuation& operator=(
      const TestEventRewriterContinuation&) = delete;

  ui::EventDispatchDetails SendEvent(const ui::Event* event) override {
    passthrough_events.push_back(event->Clone());
    return ui::EventDispatchDetails();
  }

  ui::EventDispatchDetails SendEventFinally(const ui::Event* event) override {
    rewritten_events.push_back(event->Clone());
    return ui::EventDispatchDetails();
  }

  ui::EventDispatchDetails DiscardEvent() override {
    return ui::EventDispatchDetails();
  }

  std::vector<std::unique_ptr<ui::Event>> rewritten_events;
  std::vector<std::unique_ptr<ui::Event>> passthrough_events;

  base::WeakPtrFactory<TestEventRewriterContinuation> weak_ptr_factory_{this};
};

// Key representation in test cases.
struct TestKeyEvent {
  ui::EventType type;
  ui::DomCode code;
  ui::DomKey key;
  ui::KeyboardCode keycode;
  ui::EventFlags flags = ui::EF_NONE;
  uint32_t scan_code = kNoScanCode;
  std::vector<std::pair<std::string, std::vector<uint8_t>>> properties;

  std::string ToString() const;
};

std::string TestKeyEvent::ToString() const {
  std::string type_name(ui::EventTypeName(type));
  std::string flags_name = base::JoinString(ui::EventFlagsNames(flags), "|");

  std::string property_dump;
  for (const auto& property : properties) {
    property_dump += (property_dump.empty() ? "" : "|") + property.first;
    property_dump += "=" + base::HexEncode(property.second);
  }

  return base::StringPrintf(
      "type=%s(%d) "
      "code=%s(0x%06X) "
      "key=%s(0x%08X) "
      "keycode=0x%02X "
      "flags=%s(0x%X) "
      "scan_code=0x%08X "
      "properties=%s",
      type_name.c_str(), type,
      ui::KeycodeConverter::DomCodeToCodeString(code).c_str(),
      static_cast<uint32_t>(code),
      ui::KeycodeConverter::DomKeyToKeyString(key).c_str(),
      static_cast<uint32_t>(key), keycode, flags_name.c_str(), flags, scan_code,
      property_dump.data());
}

inline std::ostream& operator<<(std::ostream& os, const TestKeyEvent& event) {
  return os << event.ToString();
}

inline bool operator==(const TestKeyEvent& e1, const TestKeyEvent& e2) {
  return e1.type == e2.type && e1.code == e2.code && e1.key == e2.key &&
         e1.keycode == e2.keycode && e1.flags == e2.flags &&
         e1.scan_code == e2.scan_code && e1.properties == e2.properties;
}

// Factory template of TestKeyEvents just to reduce a lot of code/data
// duplication.
template <ui::DomCode code,
          ui::DomKey::Base key,
          ui::KeyboardCode keycode,
          ui::EventFlags modifier_flag = ui::EF_NONE,
          ui::DomKey::Base shifted_key = key>
struct TestKey {
  // Returns press key event.
  static constexpr TestKeyEvent Pressed(
      ui::EventFlags flags = ui::EF_NONE,
      std::vector<std::pair<std::string, std::vector<uint8_t>>> properties =
          {}) {
    return {ui::EventType::kKeyPressed,
            code,
            (flags & ui::EF_SHIFT_DOWN) ? shifted_key : key,
            keycode,
            flags | modifier_flag,
            kNoScanCode,
            std::move(properties)};
  }

  // Returns release key event.
  static constexpr TestKeyEvent Released(
      ui::EventFlags flags = ui::EF_NONE,
      std::vector<std::pair<std::string, std::vector<uint8_t>>> properties =
          {}) {
    // Note: modifier flag should not be present on release events.
    return {ui::EventType::kKeyReleased,
            code,
            (flags & ui::EF_SHIFT_DOWN) ? shifted_key : key,
            keycode,
            flags,
            kNoScanCode,
            std::move(properties)};
  }

  // Returns press then release key events.
  static std::vector<TestKeyEvent> Typed(
      ui::EventFlags flags = ui::EF_NONE,
      std::vector<std::pair<std::string, std::vector<uint8_t>>> properties =
          {}) {
    return {Pressed(flags, properties), Released(flags, std::move(properties))};
  }
};

// Short cut of TestKey construction for Character keys.
template <ui::DomCode code,
          char key,
          ui::KeyboardCode keycode,
          char shifted_key = key>
using TestCharKey = TestKey<code,
                            ui::DomKey::FromCharacter(key),
                            keycode,
                            ui::EF_NONE,
                            ui::DomKey::FromCharacter(shifted_key)>;

using KeyUnknown =
    TestKey<ui::DomCode::NONE, ui::DomKey::UNIDENTIFIED, ui::VKEY_UNKNOWN>;

// Character keys. Shift chars are based on US layout.
using KeyA = TestCharKey<ui::DomCode::US_A, 'a', ui::VKEY_A, 'A'>;
using KeyB = TestCharKey<ui::DomCode::US_B, 'b', ui::VKEY_B, 'B'>;
using KeyC = TestCharKey<ui::DomCode::US_C, 'c', ui::VKEY_C, 'C'>;
using KeyD = TestCharKey<ui::DomCode::US_D, 'd', ui::VKEY_D, 'D'>;
using KeyN = TestCharKey<ui::DomCode::US_N, 'n', ui::VKEY_N, 'N'>;
using KeyT = TestCharKey<ui::DomCode::US_T, 't', ui::VKEY_T, 'T'>;
using KeyComma = TestCharKey<ui::DomCode::COMMA, ',', ui::VKEY_OEM_COMMA, '<'>;
using KeyPeriod =
    TestCharKey<ui::DomCode::PERIOD, '.', ui::VKEY_OEM_PERIOD, '>'>;
using KeyDigit1 = TestCharKey<ui::DomCode::DIGIT1, '1', ui::VKEY_1, '!'>;
using KeyDigit2 = TestCharKey<ui::DomCode::DIGIT2, '2', ui::VKEY_2, '@'>;
using KeyDigit3 = TestCharKey<ui::DomCode::DIGIT3, '3', ui::VKEY_3, '#'>;
using KeyDigit4 = TestCharKey<ui::DomCode::DIGIT4, '4', ui::VKEY_4, '$'>;
using KeyDigit5 = TestCharKey<ui::DomCode::DIGIT5, '5', ui::VKEY_5, '%'>;
using KeyDigit6 = TestCharKey<ui::DomCode::DIGIT6, '6', ui::VKEY_6, '^'>;
using KeyDigit7 = TestCharKey<ui::DomCode::DIGIT7, '7', ui::VKEY_7, '&'>;
using KeyDigit8 = TestCharKey<ui::DomCode::DIGIT8, '8', ui::VKEY_8, '*'>;
using KeyDigit9 = TestCharKey<ui::DomCode::DIGIT9, '9', ui::VKEY_9, '('>;
using KeyDigit0 = TestCharKey<ui::DomCode::DIGIT0, '0', ui::VKEY_0, ')'>;
using KeyMinus = TestCharKey<ui::DomCode::MINUS, '-', ui::VKEY_OEM_MINUS, '_'>;
using KeyEqual = TestCharKey<ui::DomCode::EQUAL, '=', ui::VKEY_OEM_PLUS, '+'>;

// Modifier keys.
using KeyLShift = TestKey<ui::DomCode::SHIFT_LEFT,
                          ui::DomKey::SHIFT,
                          ui::VKEY_SHIFT,
                          ui::EF_SHIFT_DOWN>;
using KeyRShift = TestKey<ui::DomCode::SHIFT_RIGHT,
                          ui::DomKey::SHIFT,
                          ui::VKEY_SHIFT,
                          ui::EF_SHIFT_DOWN>;
using KeyLMeta = TestKey<ui::DomCode::META_LEFT,
                         ui::DomKey::META,
                         ui::VKEY_LWIN,
                         ui::EF_COMMAND_DOWN>;
using KeyRMeta = TestKey<ui::DomCode::META_RIGHT,
                         ui::DomKey::META,
                         ui::VKEY_RWIN,
                         ui::EF_COMMAND_DOWN>;
using KeyLControl = TestKey<ui::DomCode::CONTROL_LEFT,
                            ui::DomKey::CONTROL,
                            ui::VKEY_CONTROL,
                            ui::EF_CONTROL_DOWN>;
using KeyRControl = TestKey<ui::DomCode::CONTROL_RIGHT,
                            ui::DomKey::CONTROL,
                            ui::VKEY_CONTROL,
                            ui::EF_CONTROL_DOWN>;
using KeyLAlt = TestKey<ui::DomCode::ALT_LEFT,
                        ui::DomKey::ALT,
                        ui::VKEY_MENU,
                        ui::EF_ALT_DOWN>;
using KeyRAlt = TestKey<ui::DomCode::ALT_RIGHT,
                        ui::DomKey::ALT,
                        ui::VKEY_MENU,
                        ui::EF_ALT_DOWN>;
using KeyCapsLock = TestKey<ui::DomCode::CAPS_LOCK,
                            ui::DomKey::CAPS_LOCK,
                            ui::VKEY_CAPITAL,
                            ui::EF_MOD3_DOWN>;
using KeyFunction = TestKey<ui::DomCode::FN,
                            ui::DomKey::FN,
                            ui::VKEY_FUNCTION,
                            ui::EF_FUNCTION_DOWN>;

// Function keys.
using KeyEscape =
    TestKey<ui::DomCode::ESCAPE, ui::DomKey::ESCAPE, ui::VKEY_ESCAPE>;
using KeyF1 = TestKey<ui::DomCode::F1, ui::DomKey::F1, ui::VKEY_F1>;
using KeyF2 = TestKey<ui::DomCode::F2, ui::DomKey::F2, ui::VKEY_F2>;
using KeyF3 = TestKey<ui::DomCode::F3, ui::DomKey::F3, ui::VKEY_F3>;
using KeyF4 = TestKey<ui::DomCode::F4, ui::DomKey::F4, ui::VKEY_F4>;
using KeyF5 = TestKey<ui::DomCode::F5, ui::DomKey::F5, ui::VKEY_F5>;
using KeyF6 = TestKey<ui::DomCode::F6, ui::DomKey::F6, ui::VKEY_F6>;
using KeyF7 = TestKey<ui::DomCode::F7, ui::DomKey::F7, ui::VKEY_F7>;
using KeyF8 = TestKey<ui::DomCode::F8, ui::DomKey::F8, ui::VKEY_F8>;
using KeyF9 = TestKey<ui::DomCode::F9, ui::DomKey::F9, ui::VKEY_F9>;
using KeyF10 = TestKey<ui::DomCode::F10, ui::DomKey::F10, ui::VKEY_F10>;
using KeyF11 = TestKey<ui::DomCode::F11, ui::DomKey::F11, ui::VKEY_F11>;
using KeyF12 = TestKey<ui::DomCode::F12, ui::DomKey::F12, ui::VKEY_F12>;
using KeyF13 = TestKey<ui::DomCode::F13, ui::DomKey::F13, ui::VKEY_F13>;
using KeyF14 = TestKey<ui::DomCode::F14, ui::DomKey::F14, ui::VKEY_F14>;
using KeyF15 = TestKey<ui::DomCode::F15, ui::DomKey::F15, ui::VKEY_F15>;
using KeyBackspace =
    TestKey<ui::DomCode::BACKSPACE, ui::DomKey::BACKSPACE, ui::VKEY_BACK>;

// Chrome OS Special keys.
using KeyBrowserBack = TestKey<ui::DomCode::BROWSER_BACK,
                               ui::DomKey::BROWSER_BACK,
                               ui::VKEY_BROWSER_BACK>;
using KeyBrowserForward = TestKey<ui::DomCode::BROWSER_FORWARD,
                                  ui::DomKey::BROWSER_FORWARD,
                                  ui::VKEY_BROWSER_FORWARD>;
using KeyBrowserRefresh = TestKey<ui::DomCode::BROWSER_REFRESH,
                                  ui::DomKey::BROWSER_REFRESH,
                                  ui::VKEY_BROWSER_REFRESH>;
using KeyZoomToggle =
    TestKey<ui::DomCode::ZOOM_TOGGLE, ui::DomKey::ZOOM_TOGGLE, ui::VKEY_ZOOM>;
using KeySelectTask = TestKey<ui::DomCode::SELECT_TASK,
                              ui::DomKey::LAUNCH_MY_COMPUTER,
                              ui::VKEY_MEDIA_LAUNCH_APP1>;
using KeyBrightnessDown = TestKey<ui::DomCode::BRIGHTNESS_DOWN,
                                  ui::DomKey::BRIGHTNESS_DOWN,
                                  ui::VKEY_BRIGHTNESS_DOWN>;
using KeyBrightnessUp = TestKey<ui::DomCode::BRIGHTNESS_UP,
                                ui::DomKey::BRIGHTNESS_UP,
                                ui::VKEY_BRIGHTNESS_UP>;
using KeyMediaPlayPause = TestKey<ui::DomCode::MEDIA_PLAY_PAUSE,
                                  ui::DomKey::MEDIA_PLAY_PAUSE,
                                  ui::VKEY_MEDIA_PLAY_PAUSE>;
using KeyVolumeMute = TestKey<ui::DomCode::VOLUME_MUTE,
                              ui::DomKey::AUDIO_VOLUME_MUTE,
                              ui::VKEY_VOLUME_MUTE>;
using KeyVolumeDown = TestKey<ui::DomCode::VOLUME_DOWN,
                              ui::DomKey::AUDIO_VOLUME_DOWN,
                              ui::VKEY_VOLUME_DOWN>;
using KeyVolumeUp = TestKey<ui::DomCode::VOLUME_UP,
                            ui::DomKey::AUDIO_VOLUME_UP,
                            ui::VKEY_VOLUME_UP>;
using KeyPrivacyScreenToggle =
    TestKey<ui::DomCode::PRIVACY_SCREEN_TOGGLE,
            ui::DomKey::F12,  // no DomKey for PRIVACY_SCREEN_TOGGLE>
            ui::VKEY_PRIVACY_SCREEN_TOGGLE>;
using KeyLaunchAssistant = TestKey<ui::DomCode::LAUNCH_ASSISTANT,
                                   ui::DomKey::LAUNCH_ASSISTANT,
                                   ui::VKEY_ASSISTANT>;
using KeyRightAlt = TestKey<ui::DomCode::LAUNCH_ASSISTANT,
                            ui::DomKey::LAUNCH_ASSISTANT,
                            ui::VKEY_ASSISTANT,
                            ui::EF_NONE,
                            ui::DomKey::LAUNCH_ASSISTANT>;

using KeyHangulMode =
    TestKey<ui::DomCode::ALT_RIGHT, ui::DomKey::HANGUL_MODE, ui::VKEY_HANGUL>;

// 6-pack keys.
using KeyInsert =
    TestKey<ui::DomCode::INSERT, ui::DomKey::INSERT, ui::VKEY_INSERT>;
using KeyDelete = TestKey<ui::DomCode::DEL, ui::DomKey::DEL, ui::VKEY_DELETE>;
using KeyHome = TestKey<ui::DomCode::HOME, ui::DomKey::HOME, ui::VKEY_HOME>;
using KeyEnd = TestKey<ui::DomCode::END, ui::DomKey::END, ui::VKEY_END>;
using KeyPageUp =
    TestKey<ui::DomCode::PAGE_UP, ui::DomKey::PAGE_UP, ui::VKEY_PRIOR>;
using KeyPageDown =
    TestKey<ui::DomCode::PAGE_DOWN, ui::DomKey::PAGE_DOWN, ui::VKEY_NEXT>;

// Arrow keys.
using KeyArrowUp =
    TestKey<ui::DomCode::ARROW_UP, ui::DomKey::ARROW_UP, ui::VKEY_UP>;
using KeyArrowDown =
    TestKey<ui::DomCode::ARROW_DOWN, ui::DomKey::ARROW_DOWN, ui::VKEY_DOWN>;
using KeyArrowLeft =
    TestKey<ui::DomCode::ARROW_LEFT, ui::DomKey::ARROW_LEFT, ui::VKEY_LEFT>;
using KeyArrowRight =
    TestKey<ui::DomCode::ARROW_RIGHT, ui::DomKey::ARROW_RIGHT, ui::VKEY_RIGHT>;

// Numpad keys.
using KeyNumpad0 = TestCharKey<ui::DomCode::NUMPAD0, '0', ui::VKEY_NUMPAD0>;
using KeyNumpadDecimal =
    TestCharKey<ui::DomCode::NUMPAD_DECIMAL, '.', ui::VKEY_DECIMAL>;
using KeyNumpad1 = TestCharKey<ui::DomCode::NUMPAD1, '1', ui::VKEY_NUMPAD1>;
using KeyNumpad2 = TestCharKey<ui::DomCode::NUMPAD2, '2', ui::VKEY_NUMPAD2>;
using KeyNumpad3 = TestCharKey<ui::DomCode::NUMPAD3, '3', ui::VKEY_NUMPAD3>;
using KeyNumpad4 = TestCharKey<ui::DomCode::NUMPAD4, '4', ui::VKEY_NUMPAD4>;
using KeyNumpad5 = TestCharKey<ui::DomCode::NUMPAD5, '5', ui::VKEY_NUMPAD5>;
using KeyNumpad6 = TestCharKey<ui::DomCode::NUMPAD6, '6', ui::VKEY_NUMPAD6>;
using KeyNumpad7 = TestCharKey<ui::DomCode::NUMPAD7, '7', ui::VKEY_NUMPAD7>;
using KeyNumpad8 = TestCharKey<ui::DomCode::NUMPAD8, '8', ui::VKEY_NUMPAD8>;
using KeyNumpad9 = TestCharKey<ui::DomCode::NUMPAD9, '9', ui::VKEY_NUMPAD9>;

// Numpad keys without NumLock key.
using KeyNumpadInsert =
    TestKey<ui::DomCode::NUMPAD0, ui::DomKey::INSERT, ui::VKEY_INSERT>;
using KeyNumpadDelete =
    TestKey<ui::DomCode::NUMPAD_DECIMAL, ui::DomKey::DEL, ui::VKEY_DELETE>;
using KeyNumpadEnd =
    TestKey<ui::DomCode::NUMPAD1, ui::DomKey::END, ui::VKEY_END>;
using KeyNumpadArrowDown =
    TestKey<ui::DomCode::NUMPAD2, ui::DomKey::ARROW_DOWN, ui::VKEY_DOWN>;
using KeyNumpadPageDown =
    TestKey<ui::DomCode::NUMPAD3, ui::DomKey::PAGE_DOWN, ui::VKEY_NEXT>;
using KeyNumpadArrowLeft =
    TestKey<ui::DomCode::NUMPAD4, ui::DomKey::ARROW_LEFT, ui::VKEY_LEFT>;
using KeyNumpadClear =
    TestKey<ui::DomCode::NUMPAD5, ui::DomKey::CLEAR, ui::VKEY_CLEAR>;
using KeyNumpadArrowRight =
    TestKey<ui::DomCode::NUMPAD6, ui::DomKey::ARROW_RIGHT, ui::VKEY_RIGHT>;
using KeyNumpadHome =
    TestKey<ui::DomCode::NUMPAD7, ui::DomKey::HOME, ui::VKEY_HOME>;
using KeyNumpadArrowUp =
    TestKey<ui::DomCode::NUMPAD8, ui::DomKey::ARROW_UP, ui::VKEY_UP>;
using KeyNumpadPageUp =
    TestKey<ui::DomCode::NUMPAD9, ui::DomKey::PAGE_UP, ui::VKEY_PRIOR>;

// Keyboard representation in tests.
struct TestKeyboard {
  const char* name;
  const char* layout;
  ui::InputDeviceType type;
  bool has_custom_top_row;
  bool has_assistant_key = false;
  bool has_function_key = false;
};
constexpr TestKeyboard kInternalChromeKeyboard = {
    "Internal Keyboard",
    kKbdTopRowLayoutUnspecified,
    ui::INPUT_DEVICE_INTERNAL,
    /*has_custom_top_row=*/false,
};
constexpr TestKeyboard kInternalChromeCustomLayoutKeyboard = {
    "Internal Custom Layout Keyboard",
    kKbdDefaultCustomTopRowLayout,
    ui::INPUT_DEVICE_INTERNAL,
    /*has_custom_top_row=*/true,
};
constexpr TestKeyboard kInternalChromeSplitModifierLayoutKeyboard = {
    "Internal Custom Layout Keyboard", kKbdDefaultCustomTopRowLayout,
    ui::INPUT_DEVICE_INTERNAL,
    /*has_custom_top_row=*/true,
    /*has_assistant_key=*/true,
    /*has_function_key=*/true,
};
constexpr TestKeyboard kExternalChromeKeyboard = {
    "External Chrome Keyboard",
    kKbdTopRowLayout1Tag,
    ui::INPUT_DEVICE_UNKNOWN,
    /*has_custom_top_row=*/false,
};
constexpr TestKeyboard kExternalChromeCustomLayoutKeyboard = {
    "External Chrome Custom Layout Keyboard",
    kKbdDefaultCustomTopRowLayout,
    ui::INPUT_DEVICE_UNKNOWN,
    /*has_custom_top_row=*/true,
};
constexpr TestKeyboard kExternalGenericKeyboard = {
    "PC Keyboard",
    kKbdTopRowLayoutUnspecified,
    ui::INPUT_DEVICE_UNKNOWN,
    /*has_custom_top_row=*/false,
};
constexpr TestKeyboard kExternalAppleKeyboard = {
    "Apple Keyboard",
    kKbdTopRowLayoutUnspecified,
    ui::INPUT_DEVICE_UNKNOWN,
    /*has_custom_top_row=*/false,
};

constexpr TestKeyboard kChromeKeyboardVariants[] = {
    kInternalChromeKeyboard,
    kExternalChromeKeyboard,
};
constexpr TestKeyboard kChromeCustomKeyboardVariants[] = {
    kInternalChromeCustomLayoutKeyboard,
    kExternalChromeCustomLayoutKeyboard,
};
constexpr TestKeyboard kNonAppleKeyboardVariants[] = {
    kInternalChromeKeyboard,  kInternalChromeCustomLayoutKeyboard,
    kExternalChromeKeyboard,  kExternalChromeCustomLayoutKeyboard,
    kExternalGenericKeyboard,
};
constexpr TestKeyboard kNonAppleNonCustomLayoutKeyboardVariants[] = {
    kInternalChromeKeyboard,
    kExternalChromeKeyboard,
    kExternalGenericKeyboard,
};
constexpr TestKeyboard kAllKeyboardVariants[] = {
    kInternalChromeKeyboard,
    kInternalChromeCustomLayoutKeyboard,
    kInternalChromeSplitModifierLayoutKeyboard,
    kExternalChromeKeyboard,
    kExternalChromeCustomLayoutKeyboard,
    kExternalGenericKeyboard,
    kExternalAppleKeyboard,
};

// Wilco keyboard configs

constexpr TestKeyboard kWilco1_0Keyboard{
    "Wilco Keyboard",
    kKbdTopRowLayoutWilcoTag,
    ui::INPUT_DEVICE_INTERNAL,
    /*has_custom_top_row=*/false,
};

constexpr TestKeyboard kWilco1_5Keyboard{
    "Drallion Keyboard",
    kKbdTopRowLayoutDrallionTag,
    ui::INPUT_DEVICE_INTERNAL,
    /*has_custom_top_row=*/false,
};

constexpr TestKeyboard kWilcoKeyboardVariants[] = {
    kWilco1_0Keyboard,
    kWilco1_5Keyboard,
};

}  // namespace

namespace ash {

class EventRewriterTestBase : public ChromeAshTestBase {
 public:
  EventRewriterTestBase()
      : fake_user_manager_(new FakeChromeUserManager),
        user_manager_enabler_(base::WrapUnique(fake_user_manager_.get())) {}
  ~EventRewriterTestBase() override {}

  void SetUp() override {
    ui::ResourceBundle::SetLottieParsingFunctions(
        &lottie::ParseLottieAsStillImage,
        &lottie::ParseLottieAsThemedStillImage);
    keyboard_layout_engine_ = std::make_unique<ui::StubKeyboardLayoutEngine>();
    // Inject custom table to make this closer to en-US behavior.
    keyboard_layout_engine_->SetCustomLookupTableForTesting({
        // Keep MetaRight as MetaRight.
        {ui::DomCode::META_RIGHT, ui::DomKey::META, ui::DomKey::META,
         ui::VKEY_RWIN},

        // Inject select_task key.
        {ui::DomCode::SELECT_TASK, ui::DomKey::LAUNCH_MY_COMPUTER,
         ui::DomKey::LAUNCH_MY_COMPUTER, ui::VKEY_MEDIA_LAUNCH_APP1},

        // Map numpad keys.
        {ui::DomCode::NUMPAD0, ui::DomKey::FromCharacter('0'),
         ui::DomKey::INSERT, ui::VKEY_NUMPAD0},
        {ui::DomCode::NUMPAD1, ui::DomKey::FromCharacter('1'), ui::DomKey::END,
         ui::VKEY_NUMPAD1},
        {ui::DomCode::NUMPAD2, ui::DomKey::FromCharacter('2'),
         ui::DomKey::ARROW_DOWN, ui::VKEY_NUMPAD2},
        {ui::DomCode::NUMPAD3, ui::DomKey::FromCharacter('3'),
         ui::DomKey::PAGE_DOWN, ui::VKEY_NUMPAD3},
        {ui::DomCode::NUMPAD4, ui::DomKey::FromCharacter('4'),
         ui::DomKey::ARROW_RIGHT, ui::VKEY_NUMPAD4},
        {ui::DomCode::NUMPAD5, ui::DomKey::FromCharacter('5'),
         ui::DomKey::CLEAR, ui::VKEY_NUMPAD5},
        {ui::DomCode::NUMPAD6, ui::DomKey::FromCharacter('6'),
         ui::DomKey::ARROW_LEFT, ui::VKEY_NUMPAD6},
        {ui::DomCode::NUMPAD7, ui::DomKey::FromCharacter('7'), ui::DomKey::HOME,
         ui::VKEY_NUMPAD7},
        {ui::DomCode::NUMPAD8, ui::DomKey::FromCharacter('8'),
         ui::DomKey::ARROW_UP, ui::VKEY_NUMPAD8},
        {ui::DomCode::NUMPAD9, ui::DomKey::FromCharacter('9'),
         ui::DomKey::PAGE_UP, ui::VKEY_NUMPAD9},
    });
    keyboard_capability_ =
        ui::KeyboardCapability::CreateStubKeyboardCapability();
    input_method_manager_mock_ = new input_method::MockInputMethodManagerImpl;
    input_method::InitializeForTesting(
        input_method_manager_mock_);  // pass ownership
    auto deprecation_controller =
        std::make_unique<DeprecationNotificationController>(&message_center_);
    deprecation_controller_ = deprecation_controller.get();
    auto input_device_settings_notification_controller =
        std::make_unique<InputDeviceSettingsNotificationController>(
            &message_center_);
    input_device_settings_notification_controller_ =
        input_device_settings_notification_controller.get();
    ChromeAshTestBase::SetUp();

    input_device_settings_controller_resetter_ = std::make_unique<
        InputDeviceSettingsController::ScopedResetterForTest>();
    input_device_settings_controller_mock_ =
        std::make_unique<MockInputDeviceSettingsController>();
    keyboard_settings = mojom::KeyboardSettings::New();
    // Disable F11/F12 settings by default.
    keyboard_settings->f11 = ui::mojom::ExtendedFkeysModifier::kDisabled;
    keyboard_settings->f12 = ui::mojom::ExtendedFkeysModifier::kDisabled;
    EXPECT_CALL(*input_device_settings_controller_mock_,
                GetKeyboardSettings(testing::_))
        .WillRepeatedly(testing::Return(keyboard_settings.get()));

    delegate_ = std::make_unique<EventRewriterDelegateImpl>(
        nullptr, std::move(deprecation_controller),
        std::move(input_device_settings_notification_controller),
        input_device_settings_controller_mock_.get());
    delegate_->set_pref_service_for_testing(prefs());
    device_data_manager_test_api_.SetKeyboardDevices({});
    keyboard_device_id_event_rewriter_ =
        std::make_unique<ui::KeyboardDeviceIdEventRewriter>(
            keyboard_capability_.get());
    keyboard_modifier_event_rewriter_ =
        std::make_unique<ui::KeyboardModifierEventRewriter>(
            std::make_unique<TestKeyboardModifierEventRewriterDelegate>(
                delegate_.get()),
            keyboard_layout_engine_.get(), keyboard_capability_.get(),
            &fake_ime_keyboard_);
    caps_lock_event_rewriter_ = std::make_unique<ui::CapsLockEventRewriter>(
        keyboard_layout_engine_.get(), keyboard_capability_.get(),
        &fake_ime_keyboard_);
    event_rewriter_ash_ = std::make_unique<ui::EventRewriterAsh>(
        delegate_.get(), keyboard_capability_.get(),
        Shell::Get()->sticky_keys_controller(), false, &fake_ime_keyboard_);
    discard_key_event_rewriter_ =
        std::make_unique<ui::DiscardKeyEventRewriter>();

    source_.AddEventRewriter(keyboard_device_id_event_rewriter_.get());
    if (ash::features::IsKeyboardRewriterFixEnabled()) {
      source_.AddEventRewriter(keyboard_modifier_event_rewriter_.get());
    }
    if (ash::features::IsKeyboardRewriterFixEnabled() &&
        features::IsModifierSplitEnabled()) {
      source_.AddEventRewriter(caps_lock_event_rewriter_.get());
    }
    source_.AddEventRewriter(event_rewriter_ash_.get());
    if (!ash::features::IsKeyboardRewriterFixEnabled() &&
        features::IsModifierSplitEnabled()) {
      source_.AddEventRewriter(caps_lock_event_rewriter_.get());
    }
    if (features::IsModifierSplitEnabled()) {
      source_.AddEventRewriter(discard_key_event_rewriter_.get());
    }
  }

  void TearDown() override {
    if (features::IsModifierSplitEnabled()) {
      source_.RemoveEventRewriter(discard_key_event_rewriter_.get());
    }
    if (!ash::features::IsKeyboardRewriterFixEnabled() &&
        features::IsModifierSplitEnabled()) {
      source_.RemoveEventRewriter(caps_lock_event_rewriter_.get());
    }
    source_.RemoveEventRewriter(event_rewriter_ash_.get());
    if (ash::features::IsKeyboardRewriterFixEnabled() &&
        features::IsModifierSplitEnabled()) {
      source_.RemoveEventRewriter(caps_lock_event_rewriter_.get());
    }
    if (ash::features::IsKeyboardRewriterFixEnabled()) {
      source_.RemoveEventRewriter(keyboard_modifier_event_rewriter_.get());
    }
    source_.RemoveEventRewriter(keyboard_device_id_event_rewriter_.get());

    event_rewriter_ash_.reset();
    caps_lock_event_rewriter_.reset();
    keyboard_modifier_event_rewriter_.reset();
    keyboard_device_id_event_rewriter_.reset();

    input_device_settings_controller_mock_.reset();
    input_device_settings_controller_resetter_.reset();
    ChromeAshTestBase::TearDown();
    // Shutdown() deletes the IME mock object.
    input_method::Shutdown();
    keyboard_capability_.reset();
    keyboard_layout_engine_.reset();
  }

  ui::test::TestEventSource& source() { return source_; }

 protected:
  std::vector<TestKeyEvent> RunRewriter(
      const std::vector<TestKeyEvent>& events,
      ui::EventFlags extra_flags = ui::EF_NONE,
      int device_id = kKeyboardDeviceId) {
    struct ModifierInfo {
      ui::EventFlags flag;
      ui::DomCode code;
      ui::DomKey key;
      ui::KeyboardCode keycode;
    };
    // We'll use modifier keys at left side heuristically.
    static constexpr ModifierInfo kModifierList[] = {
        {ui::EF_SHIFT_DOWN, ui::DomCode::SHIFT_LEFT, ui::DomKey::SHIFT,
         ui::VKEY_SHIFT},
        {ui::EF_CONTROL_DOWN, ui::DomCode::CONTROL_LEFT, ui::DomKey::CONTROL,
         ui::VKEY_CONTROL},
        {ui::EF_ALT_DOWN, ui::DomCode::ALT_LEFT, ui::DomKey::ALT,
         ui::VKEY_MENU},
        {ui::EF_COMMAND_DOWN, ui::DomCode::META_LEFT, ui::DomKey::META,
         ui::VKEY_LWIN},
        {ui::EF_MOD3_DOWN, ui::DomCode::CAPS_LOCK, ui::DomKey::CAPS_LOCK,
         ui::VKEY_CAPITAL},
        {ui::EF_FUNCTION_DOWN, ui::DomCode::FN, ui::DomKey::FN,
         ui::VKEY_FUNCTION},
    };

    // Send modifier key press events to update rewriter's modifier flag state.
    ui::EventFlags current_flags = 0;
    for (const auto& modifier : kModifierList) {
      if (!(extra_flags & modifier.flag)) {
        continue;
      }
      current_flags |= modifier.flag;
      SendKeyEvent(TestKeyEvent{ui::EventType::kKeyPressed, modifier.code,
                                modifier.key, modifier.keycode, current_flags},
                   device_id);
    }
    CHECK_EQ(current_flags, extra_flags);

    // Add extra_flags to each TestkeyEvent.
    std::vector<TestKeyEvent> key_events;
    for (const auto& event : events) {
      key_events.push_back(TestKeyEvent{
          event.type, event.code, event.key, event.keycode,
          event.flags | current_flags, event.scan_code, event.properties});
    }
    auto result = SendKeyEvents(key_events, device_id);

    // Send modifier key release events to unset rewriter'.s modifier flag
    // state.
    for (const auto& modifier : base::Reversed(kModifierList)) {
      if (!(extra_flags & modifier.flag)) {
        continue;
      }
      current_flags &= ~modifier.flag;
      SendKeyEvent(TestKeyEvent{ui::EventType::kKeyReleased, modifier.code,
                                modifier.key, modifier.keycode, current_flags},
                   device_id);
    }
    CHECK_EQ(current_flags, 0);

    return result;
  }

  // Sends a KeyEvent to the rewriter, returns the rewritten events.
  // Note: one event may be rewritten into multiple events.
  std::vector<TestKeyEvent> SendKeyEvent(const TestKeyEvent& event,
                                         int device_id = kKeyboardDeviceId) {
    return SendKeyEvents({event}, device_id);
  }

  std::vector<TestKeyEvent> SendKeyEvents(
      const std::vector<TestKeyEvent>& events,
      int device_id = kKeyboardDeviceId) {
    // Just in case some events may be there.
    if (!TakeEvents().empty()) {
      ADD_FAILURE() << "Rewritten events were left";
    }

    // Convert TestKeyEvent into ui::KeyEvent, then dispatch it to the
    // rewriter.
    for (const TestKeyEvent& event : events) {
      ui::KeyEvent key_event(event.type, event.keycode, event.code, event.flags,
                             event.key, ui::EventTimeForNow());
      key_event.set_scan_code(event.scan_code);
      key_event.set_source_device_id(device_id);
      ui::EventDispatchDetails details = source_.Send(&key_event);
      CHECK(!details.dispatcher_destroyed);
    }

    // Convert the rewritten ui::Events back to TestKeyEvent.
    auto rewritten_events = TakeEvents();
    std::vector<TestKeyEvent> result;
    for (const auto& rewritten_event : rewritten_events) {
      auto* rewritten_key_event = rewritten_event->AsKeyEvent();
      if (!rewritten_key_event) {
        ADD_FAILURE() << "Unexpected rewritten key event: "
                      << rewritten_event->ToString();
        continue;
      }
      std::vector<std::pair<std::string, std::vector<uint8_t>>> properties;
      if (rewritten_key_event->properties()) {
        for (const auto& property : *rewritten_key_event->properties()) {
          properties.push_back(property);
        }
      }
      result.push_back(
          {rewritten_key_event->type(), rewritten_key_event->code(),
           rewritten_key_event->GetDomKey(), rewritten_key_event->key_code(),
           rewritten_key_event->flags(), rewritten_key_event->scan_code(),
           std::move(properties)});
    }
    return result;
  }

  // Parameterized version of test depending on feature flag values. The feature
  // kUseSearchClickForRightClick determines if this should test for alt-click
  // or search-click.
  void DontRewriteIfNotRewritten(int right_click_flags);

  ui::MouseEvent RewriteMouseButtonEvent(const ui::MouseEvent& event) {
    TestEventRewriterContinuation continuation;
    event_rewriter_ash_->RewriteMouseButtonEventForTesting(
        event, continuation.weak_ptr_factory_.GetWeakPtr());
    if (!continuation.rewritten_events.empty()) {
      return ui::MouseEvent(*continuation.rewritten_events[0]->AsMouseEvent());
    }
    return ui::MouseEvent(event);
  }

  sync_preferences::TestingPrefServiceSyncable* prefs() { return &prefs_; }

  void InitModifierKeyPref(IntegerPrefMember* int_pref,
                           const std::string& pref_name,
                           ui::mojom::ModifierKey remap_from,
                           ui::mojom::ModifierKey remap_to) {
    if (!features::IsInputDeviceSettingsSplitEnabled()) {
      if (int_pref->GetPrefName() !=
          pref_name) {  // skip if already initialized.
        int_pref->Init(pref_name, prefs());
      }
      int_pref->SetValue(static_cast<int>(remap_to));
      return;
    }
    if (remap_from == remap_to) {
      keyboard_settings->modifier_remappings.erase(remap_from);
      return;
    }

    keyboard_settings->modifier_remappings[remap_from] = remap_to;
  }

  void SetUpKeyboard(const TestKeyboard& test_keyboard) {
    // Add a fake device to udev.
    const ui::KeyboardDevice keyboard(
        kKeyboardDeviceId, test_keyboard.type, test_keyboard.name,
        /*phys=*/"", base::FilePath(kKbdSysPath),
        /*vendor=*/-1,
        /*product=*/-1, /*version=*/-1,
        /*has_assistant_key=*/test_keyboard.has_assistant_key,
        /*has_function_key=*/test_keyboard.has_function_key);

    // Old CrOS keyboards supply an integer/enum as a sysfs property to identify
    // their layout type. New keyboards provide the mapping of scan codes to
    // F-Key position via an attribute.
    std::map<std::string, std::string> sysfs_properties;
    std::map<std::string, std::string> sysfs_attributes;
    if (!std::string_view(test_keyboard.layout).empty()) {
      (test_keyboard.has_custom_top_row
           ? sysfs_attributes[kKbdTopRowLayoutAttributeName]
           : sysfs_properties[kKbdTopRowPropertyName]) = test_keyboard.layout;
    }

    fake_udev_.Reset();
    fake_udev_.AddFakeDevice(keyboard.name, keyboard.sys_path.value(),
                             /*subsystem=*/"input", /*devnode=*/std::nullopt,
                             /*devtype=*/std::nullopt,
                             std::move(sysfs_attributes),
                             std::move(sysfs_properties));

    // Reset the state of the device manager.
    device_data_manager_test_api_.SetKeyboardDevices({});
    device_data_manager_test_api_.SetKeyboardDevices({keyboard});

    // Reset the state of the EventRewriter.
    event_rewriter_ash_->ResetStateForTesting();
    event_rewriter_ash_->set_last_keyboard_device_id_for_testing(
        kKeyboardDeviceId);
  }

  void SetExtensionCommands(
      std::optional<base::flat_set<std::pair<ui::KeyboardCode, int>>>
          commands) {
    delegate_->SetExtensionCommandsOverrideForTesting(std::move(commands));
  }

  std::vector<std::unique_ptr<ui::Event>> TakeEvents() {
    return sink_.TakeEvents();
  }

  void ClearNotifications() {
    message_center_.RemoveAllNotifications(
        false, message_center::FakeMessageCenter::RemoveType::ALL);
    deprecation_controller_->ResetStateForTesting();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<FakeChromeUserManager, DanglingUntriaged>
      fake_user_manager_;  // Not owned.
  user_manager::ScopedUserManager user_manager_enabler_;
  raw_ptr<input_method::MockInputMethodManagerImpl, DanglingUntriaged>
      input_method_manager_mock_;
  testing::FakeUdevLoader fake_udev_;
  ui::DeviceDataManagerTestApi device_data_manager_test_api_;
  std::unique_ptr<InputDeviceSettingsController::ScopedResetterForTest>
      input_device_settings_controller_resetter_;
  std::unique_ptr<MockInputDeviceSettingsController>
      input_device_settings_controller_mock_;
  mojom::KeyboardSettingsPtr keyboard_settings;

  sync_preferences::TestingPrefServiceSyncable prefs_;
  std::unique_ptr<EventRewriterDelegateImpl> delegate_;
  std::unique_ptr<ui::StubKeyboardLayoutEngine> keyboard_layout_engine_;
  std::unique_ptr<ui::KeyboardCapability> keyboard_capability_;
  input_method::FakeImeKeyboard fake_ime_keyboard_;
  std::unique_ptr<ui::KeyboardDeviceIdEventRewriter>
      keyboard_device_id_event_rewriter_;
  std::unique_ptr<ui::KeyboardModifierEventRewriter>
      keyboard_modifier_event_rewriter_;
  std::unique_ptr<ui::CapsLockEventRewriter> caps_lock_event_rewriter_;
  std::unique_ptr<ui::EventRewriterAsh> event_rewriter_ash_;
  std::unique_ptr<ui::DiscardKeyEventRewriter> discard_key_event_rewriter_;
  TestEventSink sink_;
  ui::test::TestEventSource source_{&sink_};
  message_center::FakeMessageCenter message_center_;
  base::AutoReset<bool> ignore_modifier_split_secret_key_ =
      switches::SetIgnoreModifierSplitSecretKeyForTest();
  raw_ptr<DeprecationNotificationController>
      deprecation_controller_;  // Not owned.
  raw_ptr<InputDeviceSettingsNotificationController>
      input_device_settings_notification_controller_;  // Not owned.
};

class EventRewriterTest
    : public EventRewriterTestBase,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  void SetUp() override {
    auto [enable_keyboard_rewriter_fix, enable_modifier_split] = GetParam();
    if (enable_keyboard_rewriter_fix) {
      fix_feature_list_.InitAndEnableFeature(
          ash::features::kEnableKeyboardRewriterFix);
    } else {
      fix_feature_list_.InitAndDisableFeature(
          ash::features::kEnableKeyboardRewriterFix);
    }

    if (enable_modifier_split) {
      modifier_split_feature_list_.InitAndEnableFeature(
          ash::features::kModifierSplit);
    } else {
      modifier_split_feature_list_.InitAndDisableFeature(
          ash::features::kModifierSplit);
    }

    EventRewriterTestBase::SetUp();
  }

  void TearDown() override {
    EventRewriterTestBase::TearDown();
    modifier_split_feature_list_.Reset();
    fix_feature_list_.Reset();
  }

 private:
  base::test::ScopedFeatureList fix_feature_list_;
  base::test::ScopedFeatureList modifier_split_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         EventRewriterTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

// TestKeyRewriteLatency checks that the event rewriter
// publishes a latency metric every time a key is pressed.
TEST_P(EventRewriterTest, TestKeyRewriteLatency) {
  SendKeyEvent(KeyLControl::Pressed());

  base::HistogramTester histogram_tester;
  EXPECT_EQ(std::vector({KeyB::Pressed(ui::EF_CONTROL_DOWN),
                         KeyB::Pressed(ui::EF_CONTROL_DOWN)}),
            SendKeyEvents({KeyB::Pressed(ui::EF_CONTROL_DOWN),
                           KeyB::Pressed(ui::EF_CONTROL_DOWN)}));
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Inputs.EventRewriter.KeyRewriteLatency", 2);
}

TEST_P(EventRewriterTest, ModifiersNotRemappedWhenSuppressed) {
  // Remap Control -> Alt.
  Preferences::RegisterProfilePrefs(prefs()->registry());
  IntegerPrefMember control;
  InitModifierKeyPref(&control, ::prefs::kLanguageRemapControlKeyTo,
                      ui::mojom::ModifierKey::kControl,
                      ui::mojom::ModifierKey::kAlt);

  // Pressing Control + B should now be remapped to Alt + B.
  delegate_->SuppressModifierKeyRewrites(false);
  EXPECT_EQ(KeyB::Typed(ui::EF_ALT_DOWN),
            RunRewriter(KeyB::Typed(), ui::EF_CONTROL_DOWN));

  // Pressing Control + B should no longer be remapped.
  delegate_->SuppressModifierKeyRewrites(true);
  EXPECT_EQ(KeyB::Typed(ui::EF_CONTROL_DOWN),
            RunRewriter(KeyB::Typed(), ui::EF_CONTROL_DOWN));
}

TEST_P(EventRewriterTest, TestRewriteNumPadKeys) {
  // Even if most Chrome OS keyboards do not have numpad, they should still
  // handle it the same way as generic PC keyboards.
  for (const auto& keyboard : kNonAppleKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    // XK_KP_Insert (= NumPad 0 without Num Lock), no modifier.
    EXPECT_EQ(KeyNumpad0::Typed(), RunRewriter(KeyNumpadInsert::Typed()));

    // XK_KP_Insert (= NumPad 0 without Num Lock), Alt modifier.
    EXPECT_EQ(KeyNumpad0::Typed(ui::EF_ALT_DOWN),
              RunRewriter(KeyNumpadInsert::Typed(), ui::EF_ALT_DOWN));

    // XK_KP_Delete (= NumPad . without Num Lock), Alt modifier.
    EXPECT_EQ(KeyNumpadDecimal::Typed(ui::EF_ALT_DOWN),
              RunRewriter(KeyNumpadDelete::Typed(), ui::EF_ALT_DOWN));

    // XK_KP_End (= NumPad 1 without Num Lock), Alt modifier.
    EXPECT_EQ(KeyNumpad1::Typed(ui::EF_ALT_DOWN),
              RunRewriter(KeyNumpadEnd::Typed(), ui::EF_ALT_DOWN));

    // XK_KP_Down (= NumPad 2 without Num Lock), Alt modifier.
    EXPECT_EQ(KeyNumpad2::Typed(ui::EF_ALT_DOWN),
              RunRewriter(KeyNumpadArrowDown::Typed(), ui::EF_ALT_DOWN));

    // XK_KP_Next (= NumPad 3 without Num Lock), Alt modifier.
    EXPECT_EQ(KeyNumpad3::Typed(ui::EF_ALT_DOWN),
              RunRewriter(KeyNumpadPageDown::Typed(), ui::EF_ALT_DOWN));

    // XK_KP_Left (= NumPad 4 without Num Lock), Alt modifier.
    EXPECT_EQ(KeyNumpad4::Typed(ui::EF_ALT_DOWN),
              RunRewriter(KeyNumpadArrowLeft::Typed(), ui::EF_ALT_DOWN));

    // XK_KP_Begin (= NumPad 5 without Num Lock), Alt modifier.
    EXPECT_EQ(KeyNumpad5::Typed(ui::EF_ALT_DOWN),
              RunRewriter(KeyNumpadClear::Typed(), ui::EF_ALT_DOWN));

    // XK_KP_Right (= NumPad 6 without Num Lock), Alt modifier.
    EXPECT_EQ(KeyNumpad6::Typed(ui::EF_ALT_DOWN),
              RunRewriter(KeyNumpadArrowRight::Typed(), ui::EF_ALT_DOWN));

    // XK_KP_Home (= NumPad 7 without Num Lock), Alt modifier.
    EXPECT_EQ(KeyNumpad7::Typed(ui::EF_ALT_DOWN),
              RunRewriter(KeyNumpadHome::Typed(), ui::EF_ALT_DOWN));

    // XK_KP_Up (= NumPad 8 without Num Lock), Alt modifier.
    EXPECT_EQ(KeyNumpad8::Typed(ui::EF_ALT_DOWN),
              RunRewriter(KeyNumpadArrowUp::Typed(), ui::EF_ALT_DOWN));

    // XK_KP_Prior (= NumPad 9 without Num Lock), Alt modifier.
    EXPECT_EQ(KeyNumpad9::Typed(ui::EF_ALT_DOWN),
              RunRewriter(KeyNumpadPageUp::Typed(), ui::EF_ALT_DOWN));

    // XK_KP_{N} (= NumPad {N} with Num Lock), Num Lock modifier.
    EXPECT_EQ(KeyNumpad0::Typed(), RunRewriter(KeyNumpad0::Typed()));
    EXPECT_EQ(KeyNumpad1::Typed(), RunRewriter(KeyNumpad1::Typed()));
    EXPECT_EQ(KeyNumpad2::Typed(), RunRewriter(KeyNumpad2::Typed()));
    EXPECT_EQ(KeyNumpad3::Typed(), RunRewriter(KeyNumpad3::Typed()));
    EXPECT_EQ(KeyNumpad4::Typed(), RunRewriter(KeyNumpad4::Typed()));
    EXPECT_EQ(KeyNumpad5::Typed(), RunRewriter(KeyNumpad5::Typed()));
    EXPECT_EQ(KeyNumpad6::Typed(), RunRewriter(KeyNumpad6::Typed()));
    EXPECT_EQ(KeyNumpad7::Typed(), RunRewriter(KeyNumpad7::Typed()));
    EXPECT_EQ(KeyNumpad8::Typed(), RunRewriter(KeyNumpad8::Typed()));
    EXPECT_EQ(KeyNumpad9::Typed(), RunRewriter(KeyNumpad9::Typed()));

    // XK_KP_DECIMAL (= NumPad . with Num Lock), Num Lock modifier.
    EXPECT_EQ(KeyNumpadDecimal::Typed(),
              RunRewriter(KeyNumpadDecimal::Typed()));
  }
}

// Tests if the rewriter can handle a Command + Num Pad event.
TEST_P(EventRewriterTest, TestRewriteNumPadKeysOnAppleKeyboard) {
  // Simulate the default initialization of the Apple Command key remap pref to
  // Ctrl.
  Preferences::RegisterProfilePrefs(prefs()->registry());

  if (features::IsInputDeviceSettingsSplitEnabled()) {
    keyboard_settings->modifier_remappings[ui::mojom::ModifierKey::kMeta] =
        ui::mojom::ModifierKey::kControl;
  }

  SetUpKeyboard(kExternalAppleKeyboard);

  // XK_KP_End (= NumPad 1 without Num Lock), Win modifier.
  // The result should be "Num Pad 1 with Control + Num Lock modifiers".
  EXPECT_EQ(KeyNumpad1::Typed(ui::EF_CONTROL_DOWN),
            RunRewriter(KeyNumpadEnd::Typed(), ui::EF_COMMAND_DOWN));

  // XK_KP_1 (= NumPad 1 with Num Lock), Win modifier.
  // The result should also be "Num Pad 1 with Control + Num Lock
  // modifiers".
  EXPECT_EQ(KeyNumpad1::Typed(ui::EF_CONTROL_DOWN),
            RunRewriter(KeyNumpad1::Typed(), ui::EF_COMMAND_DOWN));
}

TEST_P(EventRewriterTest, TestRewriteModifiersNoRemap) {
  for (const auto& keyboard : kAllKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    // Press Search. Confirm the event is not rewritten.
    EXPECT_EQ(KeyLMeta::Typed(), RunRewriter(KeyLMeta::Typed()));

    // Press left Control. Confirm the event is not rewritten.
    EXPECT_EQ(KeyLControl::Typed(), RunRewriter(KeyLControl::Typed()));

    // Press right Control. Confirm the event is not rewritten.
    EXPECT_EQ(KeyRControl::Typed(), RunRewriter(KeyRControl::Typed()));

    // Press left Alt. Confirm the event is not rewritten.
    EXPECT_EQ(KeyLAlt::Typed(), RunRewriter(KeyLAlt::Typed()));

    // Press right Alt. Confirm the event is not rewritten.
    EXPECT_EQ(KeyRAlt::Typed(), RunRewriter(KeyRAlt::Typed()));
  }
}

TEST_P(EventRewriterTest, TestRewriteModifiersNoRemapMultipleKeys) {
  for (const auto& keyboard : kAllKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    // Press Alt with Shift. Confirm the event is not rewritten.
    EXPECT_EQ(KeyLAlt::Typed(ui::EF_SHIFT_DOWN),
              RunRewriter(KeyLAlt::Typed(), ui::EF_SHIFT_DOWN));

    // Press Escape with Alt and Shift. Confirm the event is not rewritten.
    EXPECT_EQ(
        KeyEscape::Typed(ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN),
        RunRewriter(KeyEscape::Typed(), ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN));

    // Toggling on CapsLock.
    EXPECT_EQ(KeyCapsLock::Typed(ui::EF_CAPS_LOCK_ON),
              RunRewriter(KeyCapsLock::Typed(ui::EF_CAPS_LOCK_ON)));

    // Press Search with Caps Lock mask. Confirm the event is not rewritten.
    EXPECT_EQ(KeyLMeta::Typed(ui::EF_CAPS_LOCK_ON),
              RunRewriter(KeyLMeta::Typed(ui::EF_CAPS_LOCK_ON)));

    // Toggling off CapsLock.
    EXPECT_EQ(KeyCapsLock::Typed(), RunRewriter(KeyCapsLock::Typed()));

    // Press Shift+Ctrl+Alt+Search+Escape. Confirm the event is not rewritten.
    EXPECT_EQ(KeyEscape::Typed(ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN |
                               ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN),
              RunRewriter(KeyEscape::Typed(),
                          ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN |
                              ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN));

    // Press Shift+Ctrl+Alt+Search+B. Confirm the event is not rewritten.
    EXPECT_EQ(KeyB::Typed(ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN |
                          ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN),
              RunRewriter(
                  // In this case, SHIFT modifier will be set on pressing B,
                  // thus we should use capital 'B' as DomKey, which the current
                  // factory does not support.
                  // Modifier flags will be annotated to TestKeyEvents inside
                  // RunRewriter.
                  {TestKeyEvent{ui::EventType::kKeyPressed, ui::DomCode::US_B,
                                ui::DomKey::FromCharacter('B'), ui::VKEY_B},
                   TestKeyEvent{ui::EventType::kKeyReleased, ui::DomCode::US_B,
                                ui::DomKey::FromCharacter('B'), ui::VKEY_B}},
                  ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN |
                      ui::EF_COMMAND_DOWN));
  }
}

TEST_P(EventRewriterTest, TestRewriteModifiersDisableSome) {
  // Disable Search, Control and Escape keys.
  Preferences::RegisterProfilePrefs(prefs()->registry());
  IntegerPrefMember search;
  InitModifierKeyPref(&search, ::prefs::kLanguageRemapSearchKeyTo,
                      ui::mojom::ModifierKey::kMeta,
                      ui::mojom::ModifierKey::kVoid);
  IntegerPrefMember control;
  InitModifierKeyPref(&control, ::prefs::kLanguageRemapControlKeyTo,
                      ui::mojom::ModifierKey::kControl,
                      ui::mojom::ModifierKey::kVoid);
  IntegerPrefMember escape;
  InitModifierKeyPref(&escape, ::prefs::kLanguageRemapEscapeKeyTo,
                      ui::mojom::ModifierKey::kEscape,
                      ui::mojom::ModifierKey::kVoid);

  for (const auto& keyboard : kChromeKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    // Press Alt with Shift. This key press shouldn't be affected by the
    // pref. Confirm the event is not rewritten.
    EXPECT_EQ(KeyLAlt::Typed(ui::EF_SHIFT_DOWN),
              RunRewriter(KeyLAlt::Typed(ui::EF_SHIFT_DOWN)));

    // Press Search. Confirm the event is now VKEY_UNKNOWN.
    EXPECT_EQ(KeyUnknown::Typed(), RunRewriter(KeyLMeta::Typed()));

    // Press Control. Confirm the event is now VKEY_UNKNOWN.
    EXPECT_EQ(KeyUnknown::Typed(), RunRewriter(KeyLControl::Typed()));

    // Press Escape. Confirm the event is now VKEY_UNKNOWN.
    EXPECT_EQ(KeyUnknown::Typed(), RunRewriter(KeyEscape::Typed()));

    // Press Control+Search. Confirm the event is now VKEY_UNKNOWN
    // without any modifiers.
    if (ash::features::IsKeyboardRewriterFixEnabled()) {
      EXPECT_EQ(KeyUnknown::Typed(),
                RunRewriter(KeyLMeta::Typed(), ui::EF_CONTROL_DOWN));
    } else {
      // TODO(crbug.com/40265877): Release key event is not dispatched in old
      // rewriter. Remove this once the old rewriter is no longer used.
      EXPECT_EQ(std::vector({KeyUnknown::Pressed()}),
                RunRewriter(KeyLMeta::Typed(), ui::EF_CONTROL_DOWN));
    }

    // Press Control+Search+a. Confirm the event is now VKEY_A without any
    // modifiers.
    EXPECT_EQ(KeyA::Typed(), RunRewriter(KeyA::Typed(), ui::EF_CONTROL_DOWN));

    // Press Control+Search+Alt+a. Confirm the event is now VKEY_A only with
    // the Alt modifier.
    EXPECT_EQ(
        KeyA::Typed(ui::EF_ALT_DOWN),
        RunRewriter(KeyA::Typed(), ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN));
  }

  // Remap Alt to Control.
  IntegerPrefMember alt;
  InitModifierKeyPref(&alt, ::prefs::kLanguageRemapAltKeyTo,
                      ui::mojom::ModifierKey::kAlt,
                      ui::mojom::ModifierKey::kControl);

  for (const auto& keyboard : kChromeKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    // Press left Alt. Confirm the event is now VKEY_CONTROL
    // even though the Control key itself is disabled.
    EXPECT_EQ(KeyLControl::Typed(), RunRewriter(KeyLAlt::Typed()));

    // Press Alt+a. Confirm the event is now Control+a even though the Control
    // key itself is disabled.
    EXPECT_EQ(KeyA::Typed(ui::EF_CONTROL_DOWN),
              RunRewriter(KeyA::Typed(), ui::EF_ALT_DOWN));
  }
}

TEST_P(EventRewriterTest, TestRewriteModifiersRemapToControl) {
  // Remap Search to Control.
  Preferences::RegisterProfilePrefs(prefs()->registry());
  IntegerPrefMember search;
  InitModifierKeyPref(&search, ::prefs::kLanguageRemapSearchKeyTo,
                      ui::mojom::ModifierKey::kMeta,
                      ui::mojom::ModifierKey::kControl);

  for (const auto& keyboard : kChromeKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    // Press Search. Confirm the event is now VKEY_CONTROL.
    EXPECT_EQ(KeyLControl::Typed(), RunRewriter(KeyLMeta::Typed()));
  }

  // Remap Alt to Control too.
  IntegerPrefMember alt;
  InitModifierKeyPref(&alt, ::prefs::kLanguageRemapAltKeyTo,
                      ui::mojom::ModifierKey::kAlt,
                      ui::mojom::ModifierKey::kControl);

  for (const auto& keyboard : kChromeKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    // Press Alt. Confirm the event is now VKEY_CONTROL.
    EXPECT_EQ(KeyLControl::Typed(), RunRewriter(KeyLAlt::Typed()));

    // Press Alt+Search. Confirm the event is now VKEY_CONTROL.
    if (ash::features::IsKeyboardRewriterFixEnabled()) {
      // In this case, both pressed/released events have EF_CONTROL_DOWN,
      // because ALT key mapped to CONTROL is held.
      EXPECT_EQ(KeyLControl::Typed(ui::EF_CONTROL_DOWN),
                RunRewriter(KeyLMeta::Typed(), ui::EF_ALT_DOWN));
    } else {
      // TODO(crbug.com/40265877): Release key event is not dispatched in old
      // rewriter. Remove this once the old rewriter is no longer used.
      EXPECT_EQ(std::vector({KeyLControl::Pressed()}),
                RunRewriter(KeyLMeta::Typed(), ui::EF_ALT_DOWN));
    }

    // Press Control+Alt+Search. Confirm the event is now VKEY_CONTROL.
    if (ash::features::IsKeyboardRewriterFixEnabled()) {
      // In this case, both pressed/released events have EF_CONTROL_DOWN,
      // because ALT key mapped to CONTROL is held.
      EXPECT_EQ(KeyLControl::Typed(ui::EF_CONTROL_DOWN),
                RunRewriter(KeyLMeta::Typed(),
                            ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN));
    } else {
      // TODO(crbug.com/40265877): Release key event is not dispatched in old
      // rewriter. Remove this once the old rewriter is no longer used.
      EXPECT_EQ(std::vector({KeyLControl::Pressed()}),
                RunRewriter(KeyLMeta::Typed(),
                            ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN));
    }

    // Press Shift+Control+Alt+Search. Confirm the event is now Control with
    // Shift and Control modifiers.
    if (ash::features::IsKeyboardRewriterFixEnabled()) {
      // In this case, both pressed/released events have EF_CONTROL_DOWN,
      // because ALT key mapped to CONTROL is held.
      EXPECT_EQ(KeyLControl::Typed(ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN),
                RunRewriter(KeyLMeta::Typed(), ui::EF_SHIFT_DOWN |
                                                   ui::EF_CONTROL_DOWN |
                                                   ui::EF_ALT_DOWN));
    } else {
      // TODO(crbug.com/40265877): Release key event is not dispatched in old
      // rewriter. Remove this once the old rewriter is no longer used.
      EXPECT_EQ(std::vector({KeyLControl::Pressed(ui::EF_SHIFT_DOWN)}),
                RunRewriter(KeyLMeta::Typed(), ui::EF_SHIFT_DOWN |
                                                   ui::EF_CONTROL_DOWN |
                                                   ui::EF_ALT_DOWN));
    }

    // Press Shift+Control+Alt+Search+B. Confirm the event is now B with Shift
    // and Control modifiers.
    EXPECT_EQ(
        KeyB::Typed(ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN),
        RunRewriter(KeyB::Typed(), ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN |
                                       ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN));
  }
}

TEST_P(EventRewriterTest, TestRewriteModifiersRemapToEscape) {
  // Remap Search to Escape.
  Preferences::RegisterProfilePrefs(prefs()->registry());
  IntegerPrefMember search;
  InitModifierKeyPref(&search, ::prefs::kLanguageRemapSearchKeyTo,
                      ui::mojom::ModifierKey::kMeta,
                      ui::mojom::ModifierKey::kEscape);

  for (const auto& keyboard : kChromeKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    // Press Search. Confirm the event is now VKEY_ESCAPE.
    EXPECT_EQ(KeyEscape::Typed(), RunRewriter(KeyLMeta::Typed()));
  }
}

TEST_P(EventRewriterTest, TestRewriteModifiersRemapEscapeToAlt) {
  // Remap Escape to Alt.
  Preferences::RegisterProfilePrefs(prefs()->registry());
  IntegerPrefMember escape;
  InitModifierKeyPref(&escape, ::prefs::kLanguageRemapEscapeKeyTo,
                      ui::mojom::ModifierKey::kEscape,
                      ui::mojom::ModifierKey::kAlt);

  for (const auto& keyboard : kAllKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    // Press Escape. Confirm the event is now VKEY_MENU.
    EXPECT_EQ(KeyLAlt::Typed(), RunRewriter(KeyEscape::Typed()));
  }
}

TEST_P(EventRewriterTest, TestRewriteModifiersRemapAltToControl) {
  // Remap Alt to Control.
  Preferences::RegisterProfilePrefs(prefs()->registry());
  IntegerPrefMember alt;
  InitModifierKeyPref(&alt, ::prefs::kLanguageRemapAltKeyTo,
                      ui::mojom::ModifierKey::kAlt,
                      ui::mojom::ModifierKey::kControl);

  for (const auto& keyboard : kAllKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    // Press left Alt. Confirm the event is now VKEY_CONTROL.
    EXPECT_EQ(KeyLControl::Typed(), RunRewriter(KeyLAlt::Typed()));

    // Press Shift+comma. Verify that only the flags are changed.
    EXPECT_EQ(
        KeyComma::Typed(ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN),
        RunRewriter(KeyComma::Typed(), ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN));

    // Press Shift+9. Verify that only the flags are changed.
    EXPECT_EQ(
        KeyDigit9::Typed(ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN),
        RunRewriter(KeyDigit9::Typed(), ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN));
  }
}

TEST_P(EventRewriterTest, TestRewriteModifiersRemapUnderEscapeControlAlt) {
  Preferences::RegisterProfilePrefs(prefs()->registry());

  // Remap Escape to Alt.
  IntegerPrefMember escape;
  InitModifierKeyPref(&escape, ::prefs::kLanguageRemapEscapeKeyTo,
                      ui::mojom::ModifierKey::kEscape,
                      ui::mojom::ModifierKey::kAlt);

  // Remap Alt to Control.
  IntegerPrefMember alt;
  InitModifierKeyPref(&alt, ::prefs::kLanguageRemapAltKeyTo,
                      ui::mojom::ModifierKey::kAlt,
                      ui::mojom::ModifierKey::kControl);

  // Remap Control to Search.
  IntegerPrefMember control;
  InitModifierKeyPref(&control, ::prefs::kLanguageRemapControlKeyTo,
                      ui::mojom::ModifierKey::kControl,
                      ui::mojom::ModifierKey::kMeta);

  for (const auto& keyboard : kAllKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    // Press left Control. Confirm the event is now VKEY_LWIN.
    EXPECT_EQ(KeyLMeta::Typed(), RunRewriter(KeyLControl::Typed()));

    // Then, press all of the three, Control+Alt+Escape.
    EXPECT_EQ(
        KeyLAlt::Typed(ui::EF_CONTROL_DOWN | ui::EF_COMMAND_DOWN),
        RunRewriter(KeyEscape::Typed(), ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN));

    // Press Shift+Control+Alt+Escape.
    EXPECT_EQ(
        KeyLAlt::Typed(ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN |
                       ui::EF_COMMAND_DOWN),
        RunRewriter(KeyEscape::Typed(),
                    ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN));

    // Press Shift+Control+Alt+B
    EXPECT_EQ(
        KeyB::Typed(ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN |
                    ui::EF_COMMAND_DOWN),
        RunRewriter(KeyB::Typed(),
                    ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN));
  }
}

TEST_P(EventRewriterTest,
       TestRewriteModifiersRemapUnderEscapeControlAltSearch) {
  Preferences::RegisterProfilePrefs(prefs()->registry());

  // Remap Escape to Alt.
  IntegerPrefMember escape;
  InitModifierKeyPref(&escape, ::prefs::kLanguageRemapEscapeKeyTo,
                      ui::mojom::ModifierKey::kEscape,
                      ui::mojom::ModifierKey::kAlt);

  // Remap Alt to Control.
  IntegerPrefMember alt;
  InitModifierKeyPref(&alt, ::prefs::kLanguageRemapAltKeyTo,
                      ui::mojom::ModifierKey::kAlt,
                      ui::mojom::ModifierKey::kControl);

  // Remap Control to Search.
  IntegerPrefMember control;
  InitModifierKeyPref(&control, ::prefs::kLanguageRemapControlKeyTo,
                      ui::mojom::ModifierKey::kControl,
                      ui::mojom::ModifierKey::kMeta);

  // Remap Search to Backspace.
  IntegerPrefMember search;
  InitModifierKeyPref(&search, ::prefs::kLanguageRemapSearchKeyTo,
                      ui::mojom::ModifierKey::kMeta,
                      ui::mojom::ModifierKey::kBackspace);

  for (const auto& keyboard : kChromeKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    // Release Control and Escape, as Search and Alt would transform Backspace
    // to Delete.
    EXPECT_EQ(std::vector({KeyLMeta::Pressed()}),
              SendKeyEvent(KeyLControl::Pressed()));
    EXPECT_EQ(std::vector({KeyLAlt::Pressed(ui::EF_COMMAND_DOWN)}),
              SendKeyEvent(KeyEscape::Pressed(ui::EF_CONTROL_DOWN)));

    if (ash::features::IsKeyboardRewriterFixEnabled()) {
      EXPECT_EQ(std::vector({KeyLMeta::Released(ui::EF_ALT_DOWN)}),
                SendKeyEvent(KeyLControl::Released()));
      EXPECT_EQ(std::vector({KeyLAlt::Released()}),
                SendKeyEvent(KeyEscape::Released()));
    } else {
      // TODO(crbug.com/40265877): Due to old rewriter implementation,
      // unexpected key release events are dispatched, followed by wrongly
      // un-rewritten event is dispatched. Fix them.
      EXPECT_EQ(std::vector({KeyLMeta::Released(ui::EF_ALT_DOWN),
                             KeyLAlt::Released(ui::EF_ALT_DOWN)}),
                SendKeyEvent(KeyLControl::Released()));
      EXPECT_EQ(std::vector({KeyEscape::Released()}),
                SendKeyEvent(KeyEscape::Released()));
    }

    // Press Search. Confirm the event is now VKEY_BACK.
    EXPECT_EQ(KeyBackspace::Typed(), RunRewriter(KeyLMeta::Typed()));
  }
}

TEST_P(EventRewriterTest, TestRewriteModifiersRemapBackspaceToEscape) {
  // Remap Backspace to Escape.
  Preferences::RegisterProfilePrefs(prefs()->registry());
  IntegerPrefMember backspace;
  InitModifierKeyPref(&backspace, ::prefs::kLanguageRemapBackspaceKeyTo,
                      ui::mojom::ModifierKey::kBackspace,
                      ui::mojom::ModifierKey::kEscape);

  for (const auto& keyboard : kAllKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    // Press Backspace. Confirm the event is now VKEY_ESCAPE.
    EXPECT_EQ(KeyEscape::Typed(), RunRewriter(KeyBackspace::Typed()));
  }
}

TEST_P(EventRewriterTest,
       TestRewriteNonModifierToModifierWithRemapBetweenKeyEvents) {
  // Remap Escape to Alt.
  Preferences::RegisterProfilePrefs(prefs()->registry());
  IntegerPrefMember escape;
  InitModifierKeyPref(&escape, ::prefs::kLanguageRemapEscapeKeyTo,
                      ui::mojom::ModifierKey::kEscape,
                      ui::mojom::ModifierKey::kAlt);

  SetUpKeyboard(kInternalChromeKeyboard);

  // Press Escape.
  EXPECT_EQ(std::vector({KeyLAlt::Pressed()}),
            SendKeyEvent(KeyEscape::Pressed()));

  // Remap Escape to Control before releasing Escape.
  InitModifierKeyPref(&escape, ::prefs::kLanguageRemapEscapeKeyTo,
                      ui::mojom::ModifierKey::kEscape,
                      ui::mojom::ModifierKey::kControl);

  // Release Escape.
  if (ash::features::IsKeyboardRewriterFixEnabled()) {
    EXPECT_EQ(std::vector({KeyLAlt::Released()}),
              SendKeyEvent(KeyEscape::Released()));
  } else {
    EXPECT_EQ(std::vector({KeyEscape::Released()}),
              SendKeyEvent(KeyEscape::Released()));
  }

  // Type A, expect that Alt is not stickied.
  EXPECT_EQ(KeyA::Typed(), RunRewriter(KeyA::Typed()));
}

TEST_P(EventRewriterTest, TestRewriteModifiersRemapToCapsLock) {
  // Remap Search to Caps Lock.
  Preferences::RegisterProfilePrefs(prefs()->registry());
  IntegerPrefMember search;
  InitModifierKeyPref(&search, ::prefs::kLanguageRemapSearchKeyTo,
                      ui::mojom::ModifierKey::kMeta,
                      ui::mojom::ModifierKey::kCapsLock);

  SetUpKeyboard(kInternalChromeKeyboard);
  EXPECT_FALSE(fake_ime_keyboard_.IsCapsLockEnabled());

  // Press Search.
  EXPECT_EQ(std::vector({KeyCapsLock::Pressed(ui::EF_CAPS_LOCK_ON)}),
            SendKeyEvent(KeyLMeta::Pressed()));
  EXPECT_TRUE(fake_ime_keyboard_.IsCapsLockEnabled());

  // Release Search.
  EXPECT_EQ(std::vector({KeyCapsLock::Released(ui::EF_CAPS_LOCK_ON)}),
            SendKeyEvent(KeyLMeta::Released()));
  EXPECT_TRUE(fake_ime_keyboard_.IsCapsLockEnabled());

  // Press Search.
  EXPECT_EQ(std::vector({KeyCapsLock::Pressed()}),
            SendKeyEvent(KeyLMeta::Pressed()));
  EXPECT_FALSE(fake_ime_keyboard_.IsCapsLockEnabled());

  // Release Search.
  EXPECT_EQ(std::vector({KeyCapsLock::Released()}),
            SendKeyEvent(KeyLMeta::Released()));
  EXPECT_FALSE(fake_ime_keyboard_.IsCapsLockEnabled());

  // Do the same on external Chrome OS keyboard.
  SetUpKeyboard(kExternalChromeKeyboard);

  // Press Search.
  EXPECT_EQ(std::vector({KeyCapsLock::Pressed(ui::EF_CAPS_LOCK_ON)}),
            SendKeyEvent(KeyLMeta::Pressed()));
  EXPECT_TRUE(fake_ime_keyboard_.IsCapsLockEnabled());

  // Release Search.
  EXPECT_EQ(std::vector({KeyCapsLock::Released(ui::EF_CAPS_LOCK_ON)}),
            SendKeyEvent(KeyLMeta::Released()));
  EXPECT_TRUE(fake_ime_keyboard_.IsCapsLockEnabled());

  // Press Search.
  EXPECT_EQ(std::vector({KeyCapsLock::Pressed()}),
            SendKeyEvent(KeyLMeta::Pressed()));
  EXPECT_FALSE(fake_ime_keyboard_.IsCapsLockEnabled());

  // Release Search.
  EXPECT_EQ(std::vector({KeyCapsLock::Released()}),
            SendKeyEvent(KeyLMeta::Released()));
  EXPECT_FALSE(fake_ime_keyboard_.IsCapsLockEnabled());

  // Try external keyboard with Caps Lock.
  SetUpKeyboard(kExternalGenericKeyboard);

  // Press Caps Lock.
  EXPECT_EQ(std::vector({KeyCapsLock::Pressed(ui::EF_CAPS_LOCK_ON)}),
            SendKeyEvent(KeyCapsLock::Pressed(ui::EF_CAPS_LOCK_ON)));
  EXPECT_TRUE(fake_ime_keyboard_.IsCapsLockEnabled());

  // Release Caps Lock.
  EXPECT_EQ(std::vector({KeyCapsLock::Released(ui::EF_CAPS_LOCK_ON)}),
            SendKeyEvent(KeyCapsLock::Released(ui::EF_CAPS_LOCK_ON)));
  EXPECT_TRUE(fake_ime_keyboard_.IsCapsLockEnabled());
}

TEST_P(EventRewriterTest, TestRewriteCapsLock) {
  Preferences::RegisterProfilePrefs(prefs()->registry());

  SetUpKeyboard(kExternalGenericKeyboard);
  EXPECT_FALSE(fake_ime_keyboard_.IsCapsLockEnabled());

  // On Chrome OS, CapsLock is mapped to CapsLock with Mod3Mask.
  EXPECT_EQ(std::vector({KeyCapsLock::Pressed(ui::EF_CAPS_LOCK_ON)}),
            SendKeyEvent(KeyCapsLock::Pressed()));
  EXPECT_TRUE(fake_ime_keyboard_.IsCapsLockEnabled());

  // Key repeating should not toggle CapsLock state.
  EXPECT_EQ(std::vector({KeyCapsLock::Pressed(ui::EF_CAPS_LOCK_ON)}),
            SendKeyEvent(KeyCapsLock::Pressed()));
  EXPECT_TRUE(fake_ime_keyboard_.IsCapsLockEnabled());

  EXPECT_EQ(std::vector({KeyCapsLock::Released(ui::EF_CAPS_LOCK_ON)}),
            SendKeyEvent(KeyCapsLock::Released(ui::EF_CAPS_LOCK_ON)));
  EXPECT_TRUE(fake_ime_keyboard_.IsCapsLockEnabled());

  // Remap Caps Lock to Control.
  IntegerPrefMember caps_lock;
  InitModifierKeyPref(&caps_lock, ::prefs::kLanguageRemapCapsLockKeyTo,
                      ui::mojom::ModifierKey::kCapsLock,
                      ui::mojom::ModifierKey::kControl);

  // Press Caps Lock. CapsLock is enabled but we have remapped the key to
  // now be Control. We want to ensure that the CapsLock modifier is still
  // active even after pressing the remapped Capslock key.
  EXPECT_EQ(std::vector({KeyLControl::Pressed(ui::EF_CAPS_LOCK_ON)}),
            SendKeyEvent(KeyCapsLock::Pressed(ui::EF_CAPS_LOCK_ON)));
  EXPECT_TRUE(fake_ime_keyboard_.IsCapsLockEnabled());

  // Release Caps Lock.
  EXPECT_EQ(std::vector({KeyLControl::Released(ui::EF_CAPS_LOCK_ON)}),
            SendKeyEvent(KeyCapsLock::Released(ui::EF_CAPS_LOCK_ON)));
  EXPECT_TRUE(fake_ime_keyboard_.IsCapsLockEnabled());
}

TEST_P(EventRewriterTest, TestRewriteExternalCapsLockWithDifferentScenarios) {
  Preferences::RegisterProfilePrefs(prefs()->registry());

  SetUpKeyboard(kExternalGenericKeyboard);
  EXPECT_FALSE(fake_ime_keyboard_.IsCapsLockEnabled());

  // Turn on CapsLock.
  EXPECT_EQ(std::vector({KeyCapsLock::Pressed(ui::EF_CAPS_LOCK_ON)}),
            SendKeyEvent(KeyCapsLock::Pressed()));
  EXPECT_TRUE(fake_ime_keyboard_.IsCapsLockEnabled());

  EXPECT_EQ(std::vector({KeyCapsLock::Released(ui::EF_CAPS_LOCK_ON)}),
            SendKeyEvent(KeyCapsLock::Released(ui::EF_CAPS_LOCK_ON)));
  EXPECT_TRUE(fake_ime_keyboard_.IsCapsLockEnabled());

  // Remap CapsLock to Search.
  IntegerPrefMember search;
  InitModifierKeyPref(&search, ::prefs::kLanguageRemapCapsLockKeyTo,
                      ui::mojom::ModifierKey::kCapsLock,
                      ui::mojom::ModifierKey::kMeta);

  // Now that CapsLock is enabled, press the remapped CapsLock button again
  // and expect to not disable CapsLock.
  EXPECT_EQ(std::vector({KeyLMeta::Pressed(ui::EF_CAPS_LOCK_ON)}),
            SendKeyEvent(KeyCapsLock::Pressed(ui::EF_CAPS_LOCK_ON)));
  EXPECT_TRUE(fake_ime_keyboard_.IsCapsLockEnabled());

  EXPECT_EQ(std::vector({KeyLMeta::Released(ui::EF_CAPS_LOCK_ON)}),
            SendKeyEvent(KeyCapsLock::Released(ui::EF_CAPS_LOCK_ON)));
  EXPECT_TRUE(fake_ime_keyboard_.IsCapsLockEnabled());

  // Remap CapsLock key back to CapsLock.
  IntegerPrefMember capslock;
  InitModifierKeyPref(&capslock, ::prefs::kLanguageRemapCapsLockKeyTo,
                      ui::mojom::ModifierKey::kCapsLock,
                      ui::mojom::ModifierKey::kCapsLock);

  // Now press CapsLock again and now expect that the CapsLock modifier is
  // removed and the key is disabled.
  EXPECT_EQ(std::vector({KeyCapsLock::Pressed()}),
            SendKeyEvent(KeyCapsLock::Pressed(ui::EF_CAPS_LOCK_ON)));
  EXPECT_FALSE(fake_ime_keyboard_.IsCapsLockEnabled());
}

TEST_P(EventRewriterTest, TestRewriteCapsLockToControl) {
  // Remap CapsLock to Control.
  Preferences::RegisterProfilePrefs(prefs()->registry());
  IntegerPrefMember control;
  InitModifierKeyPref(&control, ::prefs::kLanguageRemapCapsLockKeyTo,
                      ui::mojom::ModifierKey::kCapsLock,
                      ui::mojom::ModifierKey::kControl);

  SetUpKeyboard(kExternalGenericKeyboard);

  // Press CapsLock+a. Confirm that Mod3Mask is rewritten to ControlMask.
  // On Chrome OS, CapsLock works as a Mod3 modifier.
  EXPECT_EQ(KeyA::Typed(ui::EF_CONTROL_DOWN),
            RunRewriter(KeyA::Typed(), ui::EF_MOD3_DOWN));

  // Press Control+CapsLock+a. Confirm that Mod3Mask is rewritten to
  // ControlMask
  EXPECT_EQ(KeyA::Typed(ui::EF_CONTROL_DOWN),
            RunRewriter(KeyA::Typed(), ui::EF_CONTROL_DOWN | ui::EF_MOD3_DOWN));

  // Press Alt+CapsLock+a. Confirm that Mod3Mask is rewritten to
  // ControlMask.
  EXPECT_EQ(KeyA::Typed(ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN),
            RunRewriter(KeyA::Typed(), ui::EF_ALT_DOWN | ui::EF_MOD3_DOWN));
}

TEST_P(EventRewriterTest, TestRewriteCapsLockMod3InUse) {
  // Remap CapsLock to Control.
  Preferences::RegisterProfilePrefs(prefs()->registry());
  IntegerPrefMember control;
  InitModifierKeyPref(&control, ::prefs::kLanguageRemapCapsLockKeyTo,
                      ui::mojom::ModifierKey::kCapsLock,
                      ui::mojom::ModifierKey::kControl);

  SetUpKeyboard(kExternalGenericKeyboard);
  input_method_manager_mock_->set_mod3_used(true);

  // Press CapsLock+a. Confirm that Mod3Mask is NOT rewritten to ControlMask
  // when Mod3Mask is already in use by the current XKB layout.
  EXPECT_EQ(KeyA::Typed(), RunRewriter(KeyA::Typed()));

  input_method_manager_mock_->set_mod3_used(false);
}

TEST_P(EventRewriterTest, TestRewriteToRightAlt) {
  // Remap RightAlt to Control
  Preferences::RegisterProfilePrefs(prefs()->registry());
  IntegerPrefMember control;
  InitModifierKeyPref(&control, ::prefs::kLanguageRemapControlKeyTo,
                      ui::mojom::ModifierKey::kControl,
                      ui::mojom::ModifierKey::kRightAlt);

  IntegerPrefMember search;
  InitModifierKeyPref(&search, ::prefs::kLanguageRemapSearchKeyTo,
                      ui::mojom::ModifierKey::kMeta,
                      ui::mojom::ModifierKey::kRightAlt);

  for (const auto& keyboard : kChromeKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    EXPECT_EQ(KeyRightAlt::Typed(ui::EF_NONE, {kPropertyRightAlt}),
              RunRewriter(KeyLControl::Typed()));
    EXPECT_EQ(KeyRightAlt::Typed(ui::EF_NONE, {kPropertyRightAlt}),
              RunRewriter(KeyLMeta::Typed()));
  }
}

TEST_P(EventRewriterTest, FnAndRightAltKeyPressedMetrics) {
  if (!features::IsModifierSplitEnabled()) {
    GTEST_SKIP() << "Test is only valid with the modifier split flag enabled";
  }
  base::HistogramTester histogram_tester;
  scoped_feature_list_.InitAndEnableFeature(
      features::kInputDeviceSettingsSplit);
  SetUpKeyboard(kInternalChromeSplitModifierLayoutKeyboard);
  SendKeyEvent(KeyFunction::Pressed());
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Inputs.Keyboard.ModifierPressed.Internal",
      ui::ModifierKeyUsageMetric::kFunction, 1);
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Inputs.Keyboard.RemappedModifierPressed.Internal",
      ui::ModifierKeyUsageMetric::kFunction, 1);

  SendKeyEvent(KeyRightAlt::Pressed());
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Inputs.Keyboard.ModifierPressed.Internal",
      ui::ModifierKeyUsageMetric::kRightAlt, 1);
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Inputs.Keyboard.RemappedModifierPressed.Internal",
      ui::ModifierKeyUsageMetric::kRightAlt, 1);

  // Remap RightAlt to Assistant
  InitModifierKeyPref(nullptr, "", ui::mojom::ModifierKey::kRightAlt,
                      ui::mojom::ModifierKey::kAssistant);

  RunRewriter(KeyRightAlt::Typed());
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Inputs.Keyboard.ModifierPressed.Internal",
      ui::ModifierKeyUsageMetric::kRightAlt, 2);
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Inputs.Keyboard.RemappedModifierPressed.Internal",
      ui::ModifierKeyUsageMetric::kRightAlt, 1);
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Inputs.Keyboard.RemappedModifierPressed.Internal",
      ui::ModifierKeyUsageMetric::kAssistant, 1);
  scoped_feature_list_.Reset();
}

TEST_P(EventRewriterTest, TestRewriteToFunction) {
  if (!features::IsModifierSplitEnabled()) {
    GTEST_SKIP() << "Test is only valid with the modifier split flag enabled";
  }

  SetUpKeyboard(kInternalChromeSplitModifierLayoutKeyboard);

  // Remap RightAlt to Control
  Preferences::RegisterProfilePrefs(prefs()->registry());
  IntegerPrefMember control;
  InitModifierKeyPref(&control, ::prefs::kLanguageRemapControlKeyTo,
                      ui::mojom::ModifierKey::kControl,
                      ui::mojom::ModifierKey::kFunction);

  IntegerPrefMember search;
  InitModifierKeyPref(&search, ::prefs::kLanguageRemapSearchKeyTo,
                      ui::mojom::ModifierKey::kMeta,
                      ui::mojom::ModifierKey::kFunction);

  // Keys + rewritten modifiers produce rewritten six-pack keys.
  EXPECT_EQ(KeyPageUp::Typed(),
            RunRewriter(KeyArrowUp::Typed(), ui::EF_CONTROL_DOWN));
  EXPECT_EQ(KeyPageDown::Typed(),
            RunRewriter(KeyArrowDown::Typed(), ui::EF_COMMAND_DOWN));

  // After command + control are released, events are not affected.
  EXPECT_EQ(KeyA::Typed(), RunRewriter(KeyA::Typed()));
}

TEST_P(EventRewriterTest, TestRewriteFromFunction) {
  // Function is only available when InputDeviceSettingsSplit is enabled.
  scoped_feature_list_.InitAndEnableFeature(
      features::kInputDeviceSettingsSplit);

  // Remap Function to Control
  InitModifierKeyPref(nullptr, "", ui::mojom::ModifierKey::kFunction,
                      ui::mojom::ModifierKey::kControl);

  for (const auto& keyboard : kChromeKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    EXPECT_EQ(KeyLControl::Typed(), RunRewriter(KeyFunction::Typed()));

    // A + rewritten modifiers produce events with function flag down.
    EXPECT_EQ(KeyA::Typed(ui::EF_CONTROL_DOWN),
              RunRewriter(KeyA::Typed(), ui::EF_FUNCTION_DOWN));
    // After command + control are released, events are not affected.
    EXPECT_EQ(KeyA::Typed(), RunRewriter(KeyA::Typed()));
  }

  // Remap Function to CapsLock
  InitModifierKeyPref(nullptr, "", ui::mojom::ModifierKey::kFunction,
                      ui::mojom::ModifierKey::kCapsLock);

  for (const auto& keyboard : kChromeKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    // Toggle CapsLock on
    EXPECT_EQ(KeyCapsLock::Typed(ui::EF_CAPS_LOCK_ON),
              RunRewriter(KeyFunction::Typed(ui::EF_CAPS_LOCK_ON)));

    // Toggle CapsLock off
    EXPECT_EQ(KeyCapsLock::Typed(), RunRewriter(KeyFunction::Typed()));
  }

  // Remap Function to Void
  InitModifierKeyPref(nullptr, "", ui::mojom::ModifierKey::kFunction,
                      ui::mojom::ModifierKey::kVoid);

  for (const auto& keyboard : kChromeKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    EXPECT_EQ(KeyUnknown::Typed(), RunRewriter(KeyFunction::Typed()));
  }

  scoped_feature_list_.Reset();
}

TEST_P(EventRewriterTest, TestRewriteFromRightAlt) {
  if (!features::IsModifierSplitEnabled()) {
    GTEST_SKIP() << "Test is only valid with the modifier split flag enabled";
  }

  // RightAlt is only available when InputDeviceSettingsSplit is enabled.
  scoped_feature_list_.InitAndEnableFeature(
      features::kInputDeviceSettingsSplit);
  SetUpKeyboard(kInternalChromeSplitModifierLayoutKeyboard);

  // Test that identity is working as expected.
  EXPECT_EQ(KeyRightAlt::Typed(ui::EF_NONE, {kPropertyRightAlt}),
            RunRewriter(KeyLaunchAssistant::Typed()));

  // Remap RightAlt to Control
  InitModifierKeyPref(nullptr, "", ui::mojom::ModifierKey::kRightAlt,
                      ui::mojom::ModifierKey::kControl);

  EXPECT_EQ(KeyLControl::Typed(), RunRewriter(KeyRightAlt::Typed()));

  // Test RightAlt remapped to Control properly applies the flag to other
  // events.
  EXPECT_EQ((std::vector<TestKeyEvent>{KeyLControl::Pressed()}),
            (RunRewriter(std::vector<TestKeyEvent>{KeyRightAlt::Pressed()})));
  EXPECT_EQ(KeyA::Typed(ui::EF_CONTROL_DOWN), RunRewriter(KeyA::Typed()));
  EXPECT_EQ((std::vector<TestKeyEvent>{KeyLControl::Released()}),
            (RunRewriter(std::vector<TestKeyEvent>{KeyRightAlt::Released()})));

  // Remap RightAlt to CapsLock
  InitModifierKeyPref(nullptr, "", ui::mojom::ModifierKey::kRightAlt,
                      ui::mojom::ModifierKey::kCapsLock);
  // Toggle CapsLock on/off
  EXPECT_EQ(KeyCapsLock::Typed(ui::EF_CAPS_LOCK_ON),
            RunRewriter(KeyRightAlt::Typed(ui::EF_CAPS_LOCK_ON)));
  EXPECT_EQ(KeyCapsLock::Typed(), RunRewriter(KeyRightAlt::Typed()));

  // Remap RightAlt to Void
  InitModifierKeyPref(nullptr, "", ui::mojom::ModifierKey::kRightAlt,
                      ui::mojom::ModifierKey::kVoid);
  EXPECT_EQ(KeyUnknown::Typed(), RunRewriter(KeyRightAlt::Typed()));

  scoped_feature_list_.Reset();
}

// TODO(crbug.com/1179893): Remove once the feature is enabled permanently.
TEST_P(EventRewriterTest, TestRewriteExtendedKeysAltVariantsOld) {
  Preferences::RegisterProfilePrefs(prefs()->registry());
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {}, {::features::kImprovedKeyboardShortcuts,
           features::kAltClickAndSixPackCustomization});

  for (const auto keyboard : kNonAppleKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    // Alt+Backspace -> Delete
    EXPECT_EQ(KeyDelete::Typed(),
              RunRewriter(KeyBackspace::Typed(), ui::EF_ALT_DOWN));

    // Control+Alt+Backspace -> Control+Delete
    EXPECT_EQ(KeyDelete::Typed(ui::EF_CONTROL_DOWN),
              RunRewriter(KeyBackspace::Typed(),
                          ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN));

    // Search+Alt+Backspace -> Alt+Backspace
    EXPECT_EQ(KeyBackspace::Typed(ui::EF_ALT_DOWN),
              RunRewriter(KeyBackspace::Typed(),
                          ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN));

    // Search+Control+Alt+Backspace -> Control+Alt+Backspace
    EXPECT_EQ(KeyBackspace::Typed(ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN),
              RunRewriter(KeyBackspace::Typed(), ui::EF_ALT_DOWN |
                                                     ui::EF_COMMAND_DOWN |
                                                     ui::EF_CONTROL_DOWN));

    // Alt+Up -> Prior
    EXPECT_EQ(KeyPageUp::Typed(),
              RunRewriter(KeyArrowUp::Typed(), ui::EF_ALT_DOWN));

    // Alt+Down -> Next
    EXPECT_EQ(KeyPageDown::Typed(),
              RunRewriter(KeyArrowDown::Typed(), ui::EF_ALT_DOWN));

    // Ctrl+Alt+Up -> Home
    EXPECT_EQ(KeyHome::Typed(),
              RunRewriter(KeyArrowUp::Typed(),
                          ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN));

    // Ctrl+Alt+Down -> End
    EXPECT_EQ(KeyEnd::Typed(),
              RunRewriter(KeyArrowDown::Typed(),
                          ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN));

    // NOTE: The following are workarounds to avoid rewriting the
    // Alt variants by additionally pressing Search.
    // Search+Ctrl+Alt+Up -> Ctrl+Alt+Up
    EXPECT_EQ(
        KeyArrowUp::Typed(ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN),
        RunRewriter(KeyArrowUp::Typed(), ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN |
                                             ui::EF_COMMAND_DOWN));

    // Search+Ctrl+Alt+Down -> Ctrl+Alt+Down
    EXPECT_EQ(KeyArrowDown::Typed(ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN),
              RunRewriter(KeyArrowDown::Typed(), ui::EF_ALT_DOWN |
                                                     ui::EF_CONTROL_DOWN |
                                                     ui::EF_COMMAND_DOWN));
  }
}

TEST_P(EventRewriterTest, TestRewriteExtendedKeysAltVariants) {
  Preferences::RegisterProfilePrefs(prefs()->registry());
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAltClickAndSixPackCustomization);
  // All the previously supported Alt based rewrites no longer have any
  // effect. The Search workarounds no longer take effect and the Search+Key
  // portion is rewritten as expected.
  for (const auto keyboard : kNonAppleKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    // Alt+Backspace -> No Rewrite
    EXPECT_EQ(KeyBackspace::Typed(ui::EF_ALT_DOWN),
              RunRewriter(KeyBackspace::Typed(), ui::EF_ALT_DOWN));
    EXPECT_EQ(1u, message_center_.NotificationCount());
    ClearNotifications();

    // Control+Alt+Backspace -> No Rewrite
    EXPECT_EQ(KeyBackspace::Typed(ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN),
              RunRewriter(KeyBackspace::Typed(),
                          ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN));
    EXPECT_EQ(1u, message_center_.NotificationCount());
    ClearNotifications();

    // Search+Alt+Backspace -> Alt+Delete
    EXPECT_EQ(KeyDelete::Typed(ui::EF_ALT_DOWN),
              RunRewriter(KeyBackspace::Typed(),
                          ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN));

    // Search+Control+Alt+Backspace -> Control+Alt+Delete
    EXPECT_EQ(KeyDelete::Typed(ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN),
              RunRewriter(KeyBackspace::Typed(), ui::EF_COMMAND_DOWN |
                                                     ui::EF_CONTROL_DOWN |
                                                     ui::EF_ALT_DOWN));

    // Alt+Up -> No Rewrite
    EXPECT_EQ(KeyArrowUp::Typed(ui::EF_ALT_DOWN),
              RunRewriter(KeyArrowUp::Typed(), ui::EF_ALT_DOWN));
    EXPECT_EQ(1u, message_center_.NotificationCount());
    ClearNotifications();

    // Alt+Down -> No Rewrite
    EXPECT_EQ(KeyArrowDown::Typed(ui::EF_ALT_DOWN),
              RunRewriter(KeyArrowDown::Typed(), ui::EF_ALT_DOWN));
    EXPECT_EQ(1u, message_center_.NotificationCount());
    ClearNotifications();

    // Ctrl+Alt+Up -> No Rewrite
    EXPECT_EQ(KeyArrowUp::Typed(ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN),
              RunRewriter(KeyArrowUp::Typed(),
                          ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN));
    EXPECT_EQ(1u, message_center_.NotificationCount());
    ClearNotifications();

    // Ctrl+Alt+Down -> No Rewrite
    EXPECT_EQ(KeyArrowDown::Typed(ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN),
              RunRewriter(KeyArrowDown::Typed(),
                          ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN));
    EXPECT_EQ(1u, message_center_.NotificationCount());
    ClearNotifications();

    // NOTE: The following were workarounds to avoid rewriting the
    // Alt variants by additionally pressing Search.

    // Search+Ctrl+Alt+Up -> Ctrl+Alt+PageUp(aka Prior)
    EXPECT_EQ(KeyPageUp::Typed(ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN),
              RunRewriter(KeyArrowUp::Typed(), ui::EF_COMMAND_DOWN |
                                                   ui::EF_CONTROL_DOWN |
                                                   ui::EF_ALT_DOWN));
    // Search+Ctrl+Alt+Down -> Ctrl+Alt+PageDown(aka Next)
    EXPECT_EQ(KeyPageDown::Typed(ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN),
              RunRewriter(KeyArrowDown::Typed(), ui::EF_COMMAND_DOWN |
                                                     ui::EF_CONTROL_DOWN |
                                                     ui::EF_ALT_DOWN));
  }
}

// TODO(crbug.com/1179893): Remove once the feature is enabled permanently.
TEST_P(EventRewriterTest, TestRewriteExtendedKeyInsertOld) {
  Preferences::RegisterProfilePrefs(prefs()->registry());
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {}, {::features::kImprovedKeyboardShortcuts,
           features::kAltClickAndSixPackCustomization});
  for (const auto keyboard : kNonAppleKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    // Period -> Period
    EXPECT_EQ(KeyPeriod::Typed(), RunRewriter(KeyPeriod::Typed()));

    // Search+Period -> Insert
    EXPECT_EQ(KeyInsert::Typed(),
              RunRewriter(KeyPeriod::Typed(), ui::EF_COMMAND_DOWN));

    // Control+Search+Period -> Control+Insert
    EXPECT_EQ(KeyInsert::Typed(ui::EF_CONTROL_DOWN),
              RunRewriter(KeyPeriod::Typed(),
                          ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN));
  }
}

TEST_P(EventRewriterTest, TestRewriteExtendedKeyInsertDeprecatedNotification) {
  Preferences::RegisterProfilePrefs(prefs()->registry());
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {::features::kImprovedKeyboardShortcuts},
      {features::kAltClickAndSixPackCustomization});

  for (const auto keyboard : kNonAppleKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    // Period -> Period
    EXPECT_EQ(KeyPeriod::Typed(), RunRewriter(KeyPeriod::Typed()));

    // Search+Period -> No rewrite (and shows notification)
    EXPECT_EQ(KeyPeriod::Typed(ui::EF_COMMAND_DOWN),
              RunRewriter(KeyPeriod::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(1u, message_center_.NotificationCount());
    ClearNotifications();

    // Control+Search+Period -> No rewrite (and shows notification)
    EXPECT_EQ(KeyPeriod::Typed(ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN),
              RunRewriter(KeyPeriod::Typed(),
                          ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN));
    EXPECT_EQ(1u, message_center_.NotificationCount());
    ClearNotifications();
  }
}

// TODO(crbug.com/1179893): Rename once the feature is enabled permanently.
TEST_P(EventRewriterTest, TestRewriteExtendedKeyInsertNew) {
  Preferences::RegisterProfilePrefs(prefs()->registry());
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {::features::kImprovedKeyboardShortcuts},
      {features::kAltClickAndSixPackCustomization});

  for (const auto keyboard : kNonAppleKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    // Search+Shift+Backspace -> Insert
    EXPECT_EQ(KeyInsert::Typed(),
              RunRewriter(KeyBackspace::Typed(),
                          ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN));

    // Control+Search+Shift+Backspace -> Control+Insert
    EXPECT_EQ(KeyInsert::Typed(ui::EF_CONTROL_DOWN),
              RunRewriter(KeyBackspace::Typed(), ui::EF_COMMAND_DOWN |
                                                     ui::EF_CONTROL_DOWN |
                                                     ui::EF_SHIFT_DOWN));
  }
}

TEST_P(EventRewriterTest, TestRewriteExtendedKeysSearchVariants) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAltClickAndSixPackCustomization);
  Preferences::RegisterProfilePrefs(prefs()->registry());

  for (const auto keyboard : kNonAppleKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    // Search+Backspace -> Delete
    EXPECT_EQ(KeyDelete::Typed(),
              RunRewriter(KeyBackspace::Typed(), ui::EF_COMMAND_DOWN));

    // Search+Up -> Prior
    EXPECT_EQ(KeyPageUp::Typed(),
              RunRewriter(KeyArrowUp::Typed(), ui::EF_COMMAND_DOWN));

    // Search+Down -> Next
    EXPECT_EQ(KeyPageDown::Typed(),
              RunRewriter(KeyArrowDown::Typed(), ui::EF_COMMAND_DOWN));

    // Search+Left -> Home
    EXPECT_EQ(KeyHome::Typed(),
              RunRewriter(KeyArrowLeft::Typed(), ui::EF_COMMAND_DOWN));

    // Control+Search+Left -> Control+Home
    EXPECT_EQ(KeyHome::Typed(ui::EF_CONTROL_DOWN),
              RunRewriter(KeyArrowLeft::Typed(),
                          ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN));

    // Search+Right -> End
    EXPECT_EQ(KeyEnd::Typed(),
              RunRewriter(KeyArrowRight::Typed(), ui::EF_COMMAND_DOWN));

    // Control+Search+Right -> Control+End
    EXPECT_EQ(KeyEnd::Typed(ui::EF_CONTROL_DOWN),
              RunRewriter(KeyArrowRight::Typed(),
                          ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN));
  }
}

TEST_P(EventRewriterTest, TestNumberRowIsNotRewritten) {
  Preferences::RegisterProfilePrefs(prefs()->registry());
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAltClickAndSixPackCustomization);
  for (const auto& keyboard : kNonAppleNonCustomLayoutKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    // The number row should not be rewritten without Search key.
    EXPECT_EQ(KeyDigit1::Typed(), RunRewriter(KeyDigit1::Typed()));
    EXPECT_EQ(KeyDigit2::Typed(), RunRewriter(KeyDigit2::Typed()));
    EXPECT_EQ(KeyDigit3::Typed(), RunRewriter(KeyDigit3::Typed()));
    EXPECT_EQ(KeyDigit4::Typed(), RunRewriter(KeyDigit4::Typed()));
    EXPECT_EQ(KeyDigit5::Typed(), RunRewriter(KeyDigit5::Typed()));
    EXPECT_EQ(KeyDigit6::Typed(), RunRewriter(KeyDigit6::Typed()));
    EXPECT_EQ(KeyDigit7::Typed(), RunRewriter(KeyDigit7::Typed()));
    EXPECT_EQ(KeyDigit8::Typed(), RunRewriter(KeyDigit8::Typed()));
    EXPECT_EQ(KeyDigit9::Typed(), RunRewriter(KeyDigit9::Typed()));
    EXPECT_EQ(KeyDigit0::Typed(), RunRewriter(KeyDigit0::Typed()));
    EXPECT_EQ(KeyMinus::Typed(), RunRewriter(KeyMinus::Typed()));
    EXPECT_EQ(KeyEqual::Typed(), RunRewriter(KeyEqual::Typed()));
  }
}

// TODO(crbug.com/1179893): Remove once the feature is enabled permanently.
TEST_P(EventRewriterTest, TestRewriteSearchNumberToFunctionKeyOld) {
  Preferences::RegisterProfilePrefs(prefs()->registry());
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      ::features::kImprovedKeyboardShortcuts);

  for (const auto& keyboard : kNonAppleNonCustomLayoutKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    // The number row should be rewritten as the F<number> row with Search
    // key.
    EXPECT_EQ(KeyF1::Typed(),
              RunRewriter(KeyDigit1::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyF2::Typed(),
              RunRewriter(KeyDigit2::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyF3::Typed(),
              RunRewriter(KeyDigit3::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyF4::Typed(),
              RunRewriter(KeyDigit4::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyF5::Typed(),
              RunRewriter(KeyDigit5::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyF6::Typed(),
              RunRewriter(KeyDigit6::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyF7::Typed(),
              RunRewriter(KeyDigit7::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyF8::Typed(),
              RunRewriter(KeyDigit8::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyF9::Typed(),
              RunRewriter(KeyDigit9::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyF10::Typed(),
              RunRewriter(KeyDigit0::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyF11::Typed(),
              RunRewriter(KeyMinus::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyF12::Typed(),
              RunRewriter(KeyEqual::Typed(), ui::EF_COMMAND_DOWN));
  }
}

TEST_P(EventRewriterTest, TestRewriteSearchNumberToFunctionKeyNoAction) {
  Preferences::RegisterProfilePrefs(prefs()->registry());

  for (const auto& keyboard : kNonAppleNonCustomLayoutKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    // Search+Number should now have no effect but a notification will
    // be shown the first time F1 to F10 is pressed.
    EXPECT_EQ(KeyDigit1::Typed(ui::EF_COMMAND_DOWN),
              RunRewriter(KeyDigit1::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyDigit2::Typed(ui::EF_COMMAND_DOWN),
              RunRewriter(KeyDigit2::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyDigit3::Typed(ui::EF_COMMAND_DOWN),
              RunRewriter(KeyDigit3::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyDigit4::Typed(ui::EF_COMMAND_DOWN),
              RunRewriter(KeyDigit4::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyDigit5::Typed(ui::EF_COMMAND_DOWN),
              RunRewriter(KeyDigit5::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyDigit6::Typed(ui::EF_COMMAND_DOWN),
              RunRewriter(KeyDigit6::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyDigit7::Typed(ui::EF_COMMAND_DOWN),
              RunRewriter(KeyDigit7::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyDigit8::Typed(ui::EF_COMMAND_DOWN),
              RunRewriter(KeyDigit8::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyDigit9::Typed(ui::EF_COMMAND_DOWN),
              RunRewriter(KeyDigit9::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyDigit0::Typed(ui::EF_COMMAND_DOWN),
              RunRewriter(KeyDigit0::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyMinus::Typed(ui::EF_COMMAND_DOWN),
              RunRewriter(KeyMinus::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyEqual::Typed(ui::EF_COMMAND_DOWN),
              RunRewriter(KeyEqual::Typed(), ui::EF_COMMAND_DOWN));
  }
}

TEST_P(EventRewriterTest, TestFunctionKeysNotRewrittenBySearch) {
  Preferences::RegisterProfilePrefs(prefs()->registry());

  for (const auto& keyboard : kNonAppleNonCustomLayoutKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    // The function keys should not be rewritten with Search key pressed.
    EXPECT_EQ(KeyF1::Typed(), RunRewriter(KeyF1::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyF2::Typed(), RunRewriter(KeyF2::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyF3::Typed(), RunRewriter(KeyF3::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyF4::Typed(), RunRewriter(KeyF4::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyF5::Typed(), RunRewriter(KeyF5::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyF6::Typed(), RunRewriter(KeyF6::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyF7::Typed(), RunRewriter(KeyF7::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyF8::Typed(), RunRewriter(KeyF8::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyF9::Typed(), RunRewriter(KeyF9::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyF10::Typed(),
              RunRewriter(KeyF10::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyF11::Typed(),
              RunRewriter(KeyF11::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyF12::Typed(),
              RunRewriter(KeyF12::Typed(), ui::EF_COMMAND_DOWN));
  }
}

TEST_P(EventRewriterTest, TestRewriteFunctionKeysNonCustomLayouts) {
  Preferences::RegisterProfilePrefs(prefs()->registry());

  // Old CrOS keyboards that do not have custom layouts send F-Keys by default
  // and are translated by default to Actions based on hardcoded mappings.
  // New CrOS keyboards are not tested here because they do not remap F-Keys.
  for (const auto& keyboard : kNonAppleNonCustomLayoutKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    // F1 -> Back
    EXPECT_EQ(KeyBrowserBack::Typed(), RunRewriter(KeyF1::Typed()));
    EXPECT_EQ(KeyBrowserBack::Typed(ui::EF_CONTROL_DOWN),
              RunRewriter(KeyF1::Typed(), ui::EF_CONTROL_DOWN));
    EXPECT_EQ(KeyBrowserBack::Typed(ui::EF_ALT_DOWN),
              RunRewriter(KeyF1::Typed(), ui::EF_ALT_DOWN));

    // F2 -> Forward
    EXPECT_EQ(KeyBrowserForward::Typed(), RunRewriter(KeyF2::Typed()));
    EXPECT_EQ(KeyBrowserForward::Typed(ui::EF_CONTROL_DOWN),
              RunRewriter(KeyF2::Typed(), ui::EF_CONTROL_DOWN));
    EXPECT_EQ(KeyBrowserForward::Typed(ui::EF_ALT_DOWN),
              RunRewriter(KeyF2::Typed(), ui::EF_ALT_DOWN));

    // F3 -> Refresh
    EXPECT_EQ(KeyBrowserRefresh::Typed(), RunRewriter(KeyF3::Typed()));
    EXPECT_EQ(KeyBrowserRefresh::Typed(ui::EF_CONTROL_DOWN),
              RunRewriter(KeyF3::Typed(), ui::EF_CONTROL_DOWN));
    EXPECT_EQ(KeyBrowserRefresh::Typed(ui::EF_ALT_DOWN),
              RunRewriter(KeyF3::Typed(), ui::EF_ALT_DOWN));

    // F4 -> Zoom (aka Fullscreen)
    EXPECT_EQ(KeyZoomToggle::Typed(), RunRewriter(KeyF4::Typed()));
    EXPECT_EQ(KeyZoomToggle::Typed(ui::EF_CONTROL_DOWN),
              RunRewriter(KeyF4::Typed(), ui::EF_CONTROL_DOWN));
    EXPECT_EQ(KeyZoomToggle::Typed(ui::EF_ALT_DOWN),
              RunRewriter(KeyF4::Typed(), ui::EF_ALT_DOWN));

    // F5 -> Launch App 1
    EXPECT_EQ(KeySelectTask::Typed(), RunRewriter(KeyF5::Typed()));
    EXPECT_EQ(KeySelectTask::Typed(ui::EF_CONTROL_DOWN),
              RunRewriter(KeyF5::Typed(), ui::EF_CONTROL_DOWN));
    EXPECT_EQ(KeySelectTask::Typed(ui::EF_ALT_DOWN),
              RunRewriter(KeyF5::Typed(), ui::EF_ALT_DOWN));

    // F6 -> Brightness down
    EXPECT_EQ(KeyBrightnessDown::Typed(), RunRewriter(KeyF6::Typed()));
    EXPECT_EQ(KeyBrightnessDown::Typed(ui::EF_CONTROL_DOWN),
              RunRewriter(KeyF6::Typed(), ui::EF_CONTROL_DOWN));
    EXPECT_EQ(KeyBrightnessDown::Typed(ui::EF_ALT_DOWN),
              RunRewriter(KeyF6::Typed(), ui::EF_ALT_DOWN));

    // F7 -> Brightness up
    EXPECT_EQ(KeyBrightnessUp::Typed(), RunRewriter(KeyF7::Typed()));
    EXPECT_EQ(KeyBrightnessUp::Typed(ui::EF_CONTROL_DOWN),
              RunRewriter(KeyF7::Typed(), ui::EF_CONTROL_DOWN));
    EXPECT_EQ(KeyBrightnessUp::Typed(ui::EF_ALT_DOWN),
              RunRewriter(KeyF7::Typed(), ui::EF_ALT_DOWN));

    // F8 -> Volume Mute
    EXPECT_EQ(KeyVolumeMute::Typed(), RunRewriter(KeyF8::Typed()));
    EXPECT_EQ(KeyVolumeMute::Typed(ui::EF_CONTROL_DOWN),
              RunRewriter(KeyF8::Typed(), ui::EF_CONTROL_DOWN));
    EXPECT_EQ(KeyVolumeMute::Typed(ui::EF_ALT_DOWN),
              RunRewriter(KeyF8::Typed(), ui::EF_ALT_DOWN));

    // F9 -> Volume Down
    EXPECT_EQ(KeyVolumeDown::Typed(), RunRewriter(KeyF9::Typed()));
    EXPECT_EQ(KeyVolumeDown::Typed(ui::EF_CONTROL_DOWN),
              RunRewriter(KeyF9::Typed(), ui::EF_CONTROL_DOWN));
    EXPECT_EQ(KeyVolumeDown::Typed(ui::EF_ALT_DOWN),
              RunRewriter(KeyF9::Typed(), ui::EF_ALT_DOWN));

    // F10 -> Volume Up
    EXPECT_EQ(KeyVolumeUp::Typed(), RunRewriter(KeyF10::Typed()));
    EXPECT_EQ(KeyVolumeUp::Typed(ui::EF_CONTROL_DOWN),
              RunRewriter(KeyF10::Typed(), ui::EF_CONTROL_DOWN));
    EXPECT_EQ(KeyVolumeUp::Typed(ui::EF_ALT_DOWN),
              RunRewriter(KeyF10::Typed(), ui::EF_ALT_DOWN));

    // F11 -> F11
    EXPECT_EQ(KeyF11::Typed(), RunRewriter(KeyF11::Typed()));
    EXPECT_EQ(KeyF11::Typed(ui::EF_CONTROL_DOWN),
              RunRewriter(KeyF11::Typed(), ui::EF_CONTROL_DOWN));
    EXPECT_EQ(KeyF11::Typed(ui::EF_ALT_DOWN),
              RunRewriter(KeyF11::Typed(), ui::EF_ALT_DOWN));

    // F12 -> F12
    EXPECT_EQ(KeyF12::Typed(), RunRewriter(KeyF12::Typed()));
    EXPECT_EQ(KeyF12::Typed(ui::EF_CONTROL_DOWN),
              RunRewriter(KeyF12::Typed(), ui::EF_CONTROL_DOWN));
    EXPECT_EQ(KeyF12::Typed(ui::EF_ALT_DOWN),
              RunRewriter(KeyF12::Typed(), ui::EF_ALT_DOWN));
  }
}

TEST_P(EventRewriterTest, TestRewriteFunctionKeysCustomLayoutsFKeyUnchanged) {
  Preferences::RegisterProfilePrefs(prefs()->registry());
  // On devices with custom layouts, the F-Keys are never remapped.
  for (const auto& keyboard : kChromeCustomKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    for (const auto typed :
         {KeyF1::Typed, KeyF2::Typed, KeyF3::Typed, KeyF4::Typed, KeyF5::Typed,
          KeyF6::Typed, KeyF7::Typed, KeyF8::Typed, KeyF9::Typed, KeyF10::Typed,
          KeyF11::Typed, KeyF12::Typed, KeyF13::Typed, KeyF14::Typed,
          KeyF15::Typed}) {
      EXPECT_EQ(typed(ui::EF_NONE, {}), RunRewriter(typed(ui::EF_NONE, {})));
      EXPECT_EQ(typed(ui::EF_CONTROL_DOWN, {}),
                RunRewriter(typed(ui::EF_NONE, {}), ui::EF_CONTROL_DOWN));
      EXPECT_EQ(typed(ui::EF_ALT_DOWN, {}),
                RunRewriter(typed(ui::EF_NONE, {}), ui::EF_ALT_DOWN));
      EXPECT_EQ(typed(ui::EF_COMMAND_DOWN, {}),
                RunRewriter(typed(ui::EF_NONE, {}), ui::EF_COMMAND_DOWN));
    }
  }
}

TEST_P(EventRewriterTest, TestRewriteFunctionKeysCustomLayoutsActionUnchanged) {
  Preferences::RegisterProfilePrefs(prefs()->registry());

  // An action key on these devices is one where the scan code matches an entry
  // in the layout map. It doesn't matter what the action is, as long the
  // search key isn't pressed it will pass through unchanged.
  SetUpKeyboard({.name = "Internal Custom LayoutKeyboard",
                 .layout = "a1 a2 a3",
                 .type = ui::INPUT_DEVICE_INTERNAL,
                 .has_custom_top_row = true});
  auto browser_refresh = KeyBrowserRefresh::Pressed();
  browser_refresh.scan_code = 0xa1;
  EXPECT_EQ(std::vector({browser_refresh}), SendKeyEvent(browser_refresh));

  auto volume_up = KeyVolumeUp::Pressed();
  volume_up.scan_code = 0xa2;
  EXPECT_EQ(std::vector({volume_up}), SendKeyEvent(volume_up));

  auto volume_down = KeyVolumeDown::Pressed();
  volume_down.scan_code = 0xa3;
  EXPECT_EQ(std::vector({volume_down}), SendKeyEvent(volume_down));
}

TEST_P(EventRewriterTest,
       TestRewriteFunctionKeysCustomLayoutsActionSuppressedUnchanged) {
  // For EF_COMMAND_DOWN modifier.
  SendKeyEvent(KeyLMeta::Pressed());

  Preferences::RegisterProfilePrefs(prefs()->registry());
  delegate_->SuppressMetaTopRowKeyComboRewrites(true);
  keyboard_settings->suppress_meta_fkey_rewrites = true;

  // An action key on these devices is one where the scan code matches an entry
  // in the layout map. With Meta + Top Row Key rewrites being suppressed, the
  // input should be equivalent to the output for all tested keys.
  SetUpKeyboard({.name = "Internal Custom Layout Keyboard",
                 .layout = "a1 a2 a3",
                 .type = ui::INPUT_DEVICE_INTERNAL,
                 .has_custom_top_row = true});

  auto browser_refresh = KeyBrowserRefresh::Pressed(ui::EF_COMMAND_DOWN);
  browser_refresh.scan_code = 0xa1;
  EXPECT_EQ(std::vector({browser_refresh}), SendKeyEvent(browser_refresh));

  auto volume_up = KeyVolumeUp::Pressed(ui::EF_COMMAND_DOWN);
  volume_up.scan_code = 0xa2;
  EXPECT_EQ(std::vector({volume_up}), SendKeyEvent(volume_up));

  auto volume_down = KeyVolumeDown::Pressed(ui::EF_COMMAND_DOWN);
  volume_down.scan_code = 0xa3;
  EXPECT_EQ(std::vector({volume_down}), SendKeyEvent(volume_down));
}

TEST_P(EventRewriterTest,
       TestRewriteFunctionKeysCustomLayoutsActionSuppressedWithTopRowAreFKeys) {
  // For EF_COMMAND_DOWN.
  SendKeyEvent(KeyLMeta::Pressed());

  Preferences::RegisterProfilePrefs(prefs()->registry());
  delegate_->SuppressMetaTopRowKeyComboRewrites(true);
  keyboard_settings->suppress_meta_fkey_rewrites = true;

  BooleanPrefMember send_function_keys_pref;
  send_function_keys_pref.Init(prefs::kSendFunctionKeys, prefs());
  send_function_keys_pref.SetValue(true);
  keyboard_settings->top_row_are_fkeys = true;

  // An action key on these devices is one where the scan code matches an entry
  // in the layout map. With Meta + Top Row Key rewrites being suppressed, the
  // input should be remapped to F-Keys and the Search modifier should not be
  // removed.
  SetUpKeyboard({.name = "Internal Custom Layout Keyboard",
                 .layout = "a1 a2 a3",
                 .type = ui::INPUT_DEVICE_INTERNAL,
                 .has_custom_top_row = true});

  auto browser_refresh = KeyBrowserRefresh::Pressed(ui::EF_COMMAND_DOWN);
  browser_refresh.scan_code = 0xa1;
  auto f1 = KeyF1::Pressed(ui::EF_COMMAND_DOWN);
  f1.scan_code = 0xa1;
  EXPECT_EQ(std::vector({f1}), SendKeyEvent(browser_refresh));

  auto volume_up = KeyVolumeUp::Pressed(ui::EF_COMMAND_DOWN);
  volume_up.scan_code = 0xa2;
  auto f2 = KeyF2::Pressed(ui::EF_COMMAND_DOWN);
  f2.scan_code = 0xa2;
  EXPECT_EQ(std::vector({f2}), SendKeyEvent(volume_up));

  auto volume_down = KeyVolumeDown::Pressed(ui::EF_COMMAND_DOWN);
  volume_down.scan_code = 0xa3;
  auto f3 = KeyF3::Pressed(ui::EF_COMMAND_DOWN);
  f3.scan_code = 0xa3;
  EXPECT_EQ(std::vector({f3}), SendKeyEvent(volume_down));
}

TEST_P(EventRewriterTest, TestRewriteFunctionKeysCustomLayouts) {
  Preferences::RegisterProfilePrefs(prefs()->registry());

  // On devices with custom layouts, scan codes that match the layout
  // map get mapped to F-Keys based only on the scan code. The search
  // key also gets treated as unpressed in the remapped event.
  SetUpKeyboard({.name = "Internal Custom Layout Keyboard",
                 .layout = "a1 a2 a3 a4 a5 a6 a7 a8 a9 aa ab ac ad ae af",
                 .type = ui::INPUT_DEVICE_INTERNAL,
                 .has_custom_top_row = true});

  struct TestCase {
    std::vector<TestKeyEvent> (*pressed)(
        ui::EventFlags,
        std::vector<std::pair<std::string, std::vector<uint8_t>>>);
    uint32_t scan_code;
  };
  // Action -> F1..F15
  for (const auto& [typed, scan_code] :
       std::initializer_list<TestCase>{{KeyF1::Typed, 0xa1},
                                       {KeyF2::Typed, 0xa2},
                                       {KeyF3::Typed, 0xa3},
                                       {KeyF4::Typed, 0xa4},
                                       {KeyF5::Typed, 0xa5},
                                       {KeyF6::Typed, 0xa6},
                                       {KeyF7::Typed, 0xa7},
                                       {KeyF8::Typed, 0xa8},
                                       {KeyF9::Typed, 0xa9},
                                       {KeyF10::Typed, 0xaa},
                                       {KeyF11::Typed, 0xab},
                                       {KeyF12::Typed, 0xac},
                                       {KeyF13::Typed, 0xad},
                                       {KeyF14::Typed, 0xae},
                                       {KeyF15::Typed, 0xaf}}) {
    auto unknowns = KeyUnknown::Typed();
    for (auto& unknown : unknowns) {
      unknown.scan_code = scan_code;
    }
    auto expected_events = typed(ui::EF_NONE, {});
    for (auto& event : expected_events) {
      event.scan_code = scan_code;
    }
    EXPECT_EQ(expected_events, RunRewriter(unknowns, ui::EF_COMMAND_DOWN));

    if (features::IsModifierSplitEnabled()) {
      // With fn down, nothing should change since this keyboard uses Search
      // based rewriting.
      EXPECT_EQ(unknowns, RunRewriter(unknowns, ui::EF_FUNCTION_DOWN));
    }
  }
}

TEST_P(EventRewriterTest, TestRewriteFunctionKeysCustomLayoutsWithFunction) {
  if (!features::IsModifierSplitEnabled()) {
    GTEST_SKIP() << "Test is only valid with the modifier split flag enabled";
  }

  // On devices with custom layouts, scan codes that match the layout
  // map get mapped to F-Keys based only on the scan code. The search
  // key also gets treated as unpressed in the remapped event.
  SetUpKeyboard({.name = "Internal Custom Layout Keyboard",
                 .layout = "a1 a2 a3 a4 a5 a6 a7 a8 a9 aa ab ac ad ae af",
                 .type = ui::INPUT_DEVICE_INTERNAL,
                 .has_custom_top_row = true,
                 .has_assistant_key = true,
                 .has_function_key = true});

  struct TestCase {
    std::vector<TestKeyEvent> (*pressed)(
        ui::EventFlags,
        std::vector<std::pair<std::string, std::vector<uint8_t>>>);
    uint32_t scan_code;
  };
  // Action -> F1..F15
  for (const auto& [typed, scan_code] :
       std::initializer_list<TestCase>{{KeyF1::Typed, 0xa1},
                                       {KeyF2::Typed, 0xa2},
                                       {KeyF3::Typed, 0xa3},
                                       {KeyF4::Typed, 0xa4},
                                       {KeyF5::Typed, 0xa5},
                                       {KeyF6::Typed, 0xa6},
                                       {KeyF7::Typed, 0xa7},
                                       {KeyF8::Typed, 0xa8},
                                       {KeyF9::Typed, 0xa9},
                                       {KeyF10::Typed, 0xaa},
                                       {KeyF11::Typed, 0xab},
                                       {KeyF12::Typed, 0xac},
                                       {KeyF13::Typed, 0xad},
                                       {KeyF14::Typed, 0xae},
                                       {KeyF15::Typed, 0xaf}}) {
    auto unknowns = KeyUnknown::Typed();
    for (auto& unknown : unknowns) {
      unknown.scan_code = scan_code;
    }
    auto unknowns_with_search = KeyUnknown::Typed(ui::EF_COMMAND_DOWN);
    for (auto& unknown : unknowns_with_search) {
      unknown.scan_code = scan_code;
    }
    auto expected_events = typed(ui::EF_NONE, {});
    for (auto& event : expected_events) {
      event.scan_code = scan_code;
    }
    // Do not rewrite when search key is down since the keyboard should use the
    // fn key.
    EXPECT_EQ(unknowns_with_search, RunRewriter(unknowns, ui::EF_COMMAND_DOWN));

    // Rewrite correctly with the fn key down.
    EXPECT_EQ(expected_events, RunRewriter(unknowns, ui::EF_FUNCTION_DOWN));
  }
  scoped_feature_list_.Reset();
}

TEST_P(EventRewriterTest, TestRewriteFunctionKeysLayout2) {
  Preferences::RegisterProfilePrefs(prefs()->registry());
  SetUpKeyboard({.name = "Internal Keyboard",
                 .layout = kKbdTopRowLayout2Tag,
                 .type = ui::INPUT_DEVICE_INTERNAL,
                 .has_custom_top_row = false});

  // F1 -> Back
  EXPECT_EQ(KeyBrowserBack::Typed(), RunRewriter(KeyF1::Typed()));
  EXPECT_EQ(KeyBrowserBack::Typed(ui::EF_CONTROL_DOWN),
            RunRewriter(KeyF1::Typed(), ui::EF_CONTROL_DOWN));
  EXPECT_EQ(KeyBrowserBack::Typed(ui::EF_ALT_DOWN),
            RunRewriter(KeyF1::Typed(), ui::EF_ALT_DOWN));

  // F2 -> Refresh
  EXPECT_EQ(KeyBrowserRefresh::Typed(), RunRewriter(KeyF2::Typed()));
  EXPECT_EQ(KeyBrowserRefresh::Typed(ui::EF_CONTROL_DOWN),
            RunRewriter(KeyF2::Typed(), ui::EF_CONTROL_DOWN));
  EXPECT_EQ(KeyBrowserRefresh::Typed(ui::EF_ALT_DOWN),
            RunRewriter(KeyF2::Typed(), ui::EF_ALT_DOWN));

  // F3 -> Zoom (aka Fullscreen)
  EXPECT_EQ(KeyZoomToggle::Typed(), RunRewriter(KeyF3::Typed()));
  EXPECT_EQ(KeyZoomToggle::Typed(ui::EF_CONTROL_DOWN),
            RunRewriter(KeyF3::Typed(), ui::EF_CONTROL_DOWN));
  EXPECT_EQ(KeyZoomToggle::Typed(ui::EF_ALT_DOWN),
            RunRewriter(KeyF3::Typed(), ui::EF_ALT_DOWN));

  // F4 -> Launch App 1
  EXPECT_EQ(KeySelectTask::Typed(), RunRewriter(KeyF4::Typed()));
  EXPECT_EQ(KeySelectTask::Typed(ui::EF_CONTROL_DOWN),
            RunRewriter(KeyF4::Typed(), ui::EF_CONTROL_DOWN));
  EXPECT_EQ(KeySelectTask::Typed(ui::EF_ALT_DOWN),
            RunRewriter(KeyF4::Typed(), ui::EF_ALT_DOWN));

  // F5 -> Brightness down
  EXPECT_EQ(KeyBrightnessDown::Typed(), RunRewriter(KeyF5::Typed()));
  EXPECT_EQ(KeyBrightnessDown::Typed(ui::EF_CONTROL_DOWN),
            RunRewriter(KeyF5::Typed(), ui::EF_CONTROL_DOWN));
  EXPECT_EQ(KeyBrightnessDown::Typed(ui::EF_ALT_DOWN),
            RunRewriter(KeyF5::Typed(), ui::EF_ALT_DOWN));

  // F6 -> Brightness up
  EXPECT_EQ(KeyBrightnessUp::Typed(), RunRewriter(KeyF6::Typed()));
  EXPECT_EQ(KeyBrightnessUp::Typed(ui::EF_CONTROL_DOWN),
            RunRewriter(KeyF6::Typed(), ui::EF_CONTROL_DOWN));
  EXPECT_EQ(KeyBrightnessUp::Typed(ui::EF_ALT_DOWN),
            RunRewriter(KeyF6::Typed(), ui::EF_ALT_DOWN));

  // F7 -> Media Play/Pause
  EXPECT_EQ(KeyMediaPlayPause::Typed(), RunRewriter(KeyF7::Typed()));
  EXPECT_EQ(KeyMediaPlayPause::Typed(ui::EF_CONTROL_DOWN),
            RunRewriter(KeyF7::Typed(), ui::EF_CONTROL_DOWN));
  EXPECT_EQ(KeyMediaPlayPause::Typed(ui::EF_ALT_DOWN),
            RunRewriter(KeyF7::Typed(), ui::EF_ALT_DOWN));

  // F8 -> Volume Mute
  EXPECT_EQ(KeyVolumeMute::Typed(), RunRewriter(KeyF8::Typed()));
  EXPECT_EQ(KeyVolumeMute::Typed(ui::EF_CONTROL_DOWN),
            RunRewriter(KeyF8::Typed(), ui::EF_CONTROL_DOWN));
  EXPECT_EQ(KeyVolumeMute::Typed(ui::EF_ALT_DOWN),
            RunRewriter(KeyF8::Typed(), ui::EF_ALT_DOWN));

  // F9 -> Volume Down
  EXPECT_EQ(KeyVolumeDown::Typed(), RunRewriter(KeyF9::Typed()));
  EXPECT_EQ(KeyVolumeDown::Typed(ui::EF_CONTROL_DOWN),
            RunRewriter(KeyF9::Typed(), ui::EF_CONTROL_DOWN));
  EXPECT_EQ(KeyVolumeDown::Typed(ui::EF_ALT_DOWN),
            RunRewriter(KeyF9::Typed(), ui::EF_ALT_DOWN));

  // F10 -> Volume Up
  EXPECT_EQ(KeyVolumeUp::Typed(), RunRewriter(KeyF10::Typed()));
  EXPECT_EQ(KeyVolumeUp::Typed(ui::EF_CONTROL_DOWN),
            RunRewriter(KeyF10::Typed(), ui::EF_CONTROL_DOWN));
  EXPECT_EQ(KeyVolumeUp::Typed(ui::EF_ALT_DOWN),
            RunRewriter(KeyF10::Typed(), ui::EF_ALT_DOWN));

  // F11 -> F11
  EXPECT_EQ(KeyF11::Typed(), RunRewriter(KeyF11::Typed()));
  EXPECT_EQ(KeyF11::Typed(ui::EF_CONTROL_DOWN),
            RunRewriter(KeyF11::Typed(), ui::EF_CONTROL_DOWN));
  EXPECT_EQ(KeyF11::Typed(ui::EF_ALT_DOWN),
            RunRewriter(KeyF11::Typed(), ui::EF_ALT_DOWN));

  // F12 -> F12
  EXPECT_EQ(KeyF12::Typed(), RunRewriter(KeyF12::Typed()));
  EXPECT_EQ(KeyF12::Typed(ui::EF_CONTROL_DOWN),
            RunRewriter(KeyF12::Typed(), ui::EF_CONTROL_DOWN));
  EXPECT_EQ(KeyF12::Typed(ui::EF_ALT_DOWN),
            RunRewriter(KeyF12::Typed(), ui::EF_ALT_DOWN));
}

TEST_P(EventRewriterTest,
       TestFunctionKeysLayout2SuppressMetaTopRowKeyRewrites) {
  Preferences::RegisterProfilePrefs(prefs()->registry());
  delegate_->SuppressMetaTopRowKeyComboRewrites(true);
  keyboard_settings->suppress_meta_fkey_rewrites = true;

  // With Meta + Top Row Key rewrites suppressed, F-Keys should be translated to
  // the equivalent action key and not lose the Search modifier.
  SetUpKeyboard({.name = "Internal Keyboard",
                 .layout = kKbdTopRowLayout2Tag,
                 .type = ui::INPUT_DEVICE_INTERNAL,
                 .has_custom_top_row = false});

  // F1 -> Back
  EXPECT_EQ(KeyBrowserBack::Typed(ui::EF_COMMAND_DOWN),
            RunRewriter(KeyF1::Typed(), ui::EF_COMMAND_DOWN));

  // F2 -> Refresh
  EXPECT_EQ(KeyBrowserRefresh::Typed(ui::EF_COMMAND_DOWN),
            RunRewriter(KeyF2::Typed(), ui::EF_COMMAND_DOWN));

  // F3 -> Zoom (aka Fullscreen)
  EXPECT_EQ(KeyZoomToggle::Typed(ui::EF_COMMAND_DOWN),
            RunRewriter(KeyF3::Typed(), ui::EF_COMMAND_DOWN));

  // F4 -> Launch App 1
  EXPECT_EQ(KeySelectTask::Typed(ui::EF_COMMAND_DOWN),
            RunRewriter(KeyF4::Typed(), ui::EF_COMMAND_DOWN));

  // F5 -> Brightness down
  EXPECT_EQ(KeyBrightnessDown::Typed(ui::EF_COMMAND_DOWN),
            RunRewriter(KeyF5::Typed(), ui::EF_COMMAND_DOWN));

  // F6 -> Brightness up
  EXPECT_EQ(KeyBrightnessUp::Typed(ui::EF_COMMAND_DOWN),
            RunRewriter(KeyF6::Typed(), ui::EF_COMMAND_DOWN));

  // F7 -> Media Play/Pause
  EXPECT_EQ(KeyMediaPlayPause::Typed(ui::EF_COMMAND_DOWN),
            RunRewriter(KeyF7::Typed(), ui::EF_COMMAND_DOWN));

  // F8 -> Volume Mute
  EXPECT_EQ(KeyVolumeMute::Typed(ui::EF_COMMAND_DOWN),
            RunRewriter(KeyF8::Typed(), ui::EF_COMMAND_DOWN));

  // F9 -> Volume Down
  EXPECT_EQ(KeyVolumeDown::Typed(ui::EF_COMMAND_DOWN),
            RunRewriter(KeyF9::Typed(), ui::EF_COMMAND_DOWN));

  // F10 -> Volume Up
  EXPECT_EQ(KeyVolumeUp::Typed(ui::EF_COMMAND_DOWN),
            RunRewriter(KeyF10::Typed(), ui::EF_COMMAND_DOWN));

  // F11 -> F11
  EXPECT_EQ(KeyF11::Typed(ui::EF_COMMAND_DOWN),
            RunRewriter(KeyF11::Typed(), ui::EF_COMMAND_DOWN));

  // F12 -> F12
  EXPECT_EQ(KeyF12::Typed(ui::EF_COMMAND_DOWN),
            RunRewriter(KeyF12::Typed(), ui::EF_COMMAND_DOWN));
}

TEST_P(EventRewriterTest, RecordEventRemappedToRightClick) {
  Preferences::RegisterProfilePrefs(prefs()->registry());
  IntegerPrefMember alt_remap_to_right_click;
  IntegerPrefMember search_remap_to_right_click;
  alt_remap_to_right_click.Init(prefs::kAltEventRemappedToRightClick, prefs());
  alt_remap_to_right_click.SetValue(0);
  search_remap_to_right_click.Init(prefs::kSearchEventRemappedToRightClick,
                                   prefs());
  search_remap_to_right_click.SetValue(0);
  delegate_->RecordEventRemappedToRightClick(/*alt_based_right_click=*/false);
  EXPECT_EQ(1, prefs()->GetInteger(prefs::kSearchEventRemappedToRightClick));
  delegate_->RecordEventRemappedToRightClick(/*alt_based_right_click=*/true);
  EXPECT_EQ(1, prefs()->GetInteger(prefs::kAltEventRemappedToRightClick));
}

TEST_P(
    EventRewriterTest,
    TestFunctionKeysLayout2SuppressMetaTopRowKeyRewritesWithTreatTopRowAsFKeys) {
  Preferences::RegisterProfilePrefs(prefs()->registry());
  delegate_->SuppressMetaTopRowKeyComboRewrites(true);
  keyboard_settings->suppress_meta_fkey_rewrites = true;

  // Enable preference treat-top-row-as-function-keys.
  // That causes action keys to be mapped back to Fn keys.
  BooleanPrefMember top_row_as_fn_key;
  top_row_as_fn_key.Init(prefs::kSendFunctionKeys, prefs());
  top_row_as_fn_key.SetValue(true);
  keyboard_settings->top_row_are_fkeys = true;

  // With Meta + Top Row Key rewrites suppressed and TopRowAsFKeys enabled,
  // F-Keys should not be translated and search modifier should be kept.
  SetUpKeyboard({.name = "Internal Keyboard",
                 .layout = kKbdTopRowLayout2Tag,
                 .type = ui::INPUT_DEVICE_INTERNAL,
                 .has_custom_top_row = false});

  EXPECT_EQ(KeyF1::Typed(ui::EF_COMMAND_DOWN),
            RunRewriter(KeyF1::Typed(), ui::EF_COMMAND_DOWN));
  EXPECT_EQ(KeyF2::Typed(ui::EF_COMMAND_DOWN),
            RunRewriter(KeyF2::Typed(), ui::EF_COMMAND_DOWN));
  EXPECT_EQ(KeyF3::Typed(ui::EF_COMMAND_DOWN),
            RunRewriter(KeyF3::Typed(), ui::EF_COMMAND_DOWN));
  EXPECT_EQ(KeyF4::Typed(ui::EF_COMMAND_DOWN),
            RunRewriter(KeyF4::Typed(), ui::EF_COMMAND_DOWN));
  EXPECT_EQ(KeyF5::Typed(ui::EF_COMMAND_DOWN),
            RunRewriter(KeyF5::Typed(), ui::EF_COMMAND_DOWN));
  EXPECT_EQ(KeyF6::Typed(ui::EF_COMMAND_DOWN),
            RunRewriter(KeyF6::Typed(), ui::EF_COMMAND_DOWN));
  EXPECT_EQ(KeyF7::Typed(ui::EF_COMMAND_DOWN),
            RunRewriter(KeyF7::Typed(), ui::EF_COMMAND_DOWN));
  EXPECT_EQ(KeyF8::Typed(ui::EF_COMMAND_DOWN),
            RunRewriter(KeyF8::Typed(), ui::EF_COMMAND_DOWN));
  EXPECT_EQ(KeyF9::Typed(ui::EF_COMMAND_DOWN),
            RunRewriter(KeyF9::Typed(), ui::EF_COMMAND_DOWN));
  EXPECT_EQ(KeyF10::Typed(ui::EF_COMMAND_DOWN),
            RunRewriter(KeyF10::Typed(), ui::EF_COMMAND_DOWN));
  EXPECT_EQ(KeyF11::Typed(ui::EF_COMMAND_DOWN),
            RunRewriter(KeyF11::Typed(), ui::EF_COMMAND_DOWN));
  EXPECT_EQ(KeyF12::Typed(ui::EF_COMMAND_DOWN),
            RunRewriter(KeyF12::Typed(), ui::EF_COMMAND_DOWN));
}

TEST_P(EventRewriterTest, TestRewriteFunctionKeysWilcoLayouts) {
  Preferences::RegisterProfilePrefs(prefs()->registry());

  for (const auto& keyboard : kWilcoKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    // F1 -> F1, Search + F1 -> Back
    EXPECT_EQ(KeyF1::Typed(), RunRewriter(KeyF1::Typed()));
    EXPECT_EQ(KeyBrowserBack::Typed(),
              RunRewriter(KeyF1::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyF1::Typed(ui::EF_CONTROL_DOWN),
              RunRewriter(KeyF1::Typed(), ui::EF_CONTROL_DOWN));
    EXPECT_EQ(KeyF1::Typed(ui::EF_ALT_DOWN),
              RunRewriter(KeyF1::Typed(), ui::EF_ALT_DOWN));

    // F2 -> F2, Search + F2 -> Refresh
    EXPECT_EQ(KeyF2::Typed(), RunRewriter(KeyF2::Typed()));
    EXPECT_EQ(KeyBrowserRefresh::Typed(),
              RunRewriter(KeyF2::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyF2::Typed(ui::EF_CONTROL_DOWN),
              RunRewriter(KeyF2::Typed(), ui::EF_CONTROL_DOWN));
    EXPECT_EQ(KeyF2::Typed(ui::EF_ALT_DOWN),
              RunRewriter(KeyF2::Typed(), ui::EF_ALT_DOWN));

    // F3 -> F3, Search + F3 -> Zoom (aka Fullscreen)
    EXPECT_EQ(KeyF3::Typed(), RunRewriter(KeyF3::Typed()));
    EXPECT_EQ(KeyZoomToggle::Typed(),
              RunRewriter(KeyF3::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyF3::Typed(ui::EF_CONTROL_DOWN),
              RunRewriter(KeyF3::Typed(), ui::EF_CONTROL_DOWN));
    EXPECT_EQ(KeyF3::Typed(ui::EF_ALT_DOWN),
              RunRewriter(KeyF3::Typed(), ui::EF_ALT_DOWN));

    // F4 -> F4, Search + F4 -> Launch App 1
    EXPECT_EQ(KeyF4::Typed(), RunRewriter(KeyF4::Typed()));
    EXPECT_EQ(
        std::vector({TestKeyEvent{ui::EventType::kKeyPressed, ui::DomCode::F4,
                                  ui::DomKey::F4, ui::VKEY_MEDIA_LAUNCH_APP1,
                                  ui::EF_NONE},
                     TestKeyEvent{ui::EventType::kKeyReleased, ui::DomCode::F4,
                                  ui::DomKey::F4, ui::VKEY_MEDIA_LAUNCH_APP1,
                                  ui::EF_NONE}}),
        RunRewriter(KeyF4::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyF4::Typed(ui::EF_CONTROL_DOWN),
              RunRewriter(KeyF4::Typed(), ui::EF_CONTROL_DOWN));
    EXPECT_EQ(KeyF4::Typed(ui::EF_ALT_DOWN),
              RunRewriter(KeyF4::Typed(), ui::EF_ALT_DOWN));

    // F5 -> F5, Search + F5 -> Brightness down
    EXPECT_EQ(KeyF5::Typed(), RunRewriter(KeyF5::Typed()));
    EXPECT_EQ(KeyBrightnessDown::Typed(),
              RunRewriter(KeyF5::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyF5::Typed(ui::EF_CONTROL_DOWN),
              RunRewriter(KeyF5::Typed(), ui::EF_CONTROL_DOWN));
    EXPECT_EQ(KeyF5::Typed(ui::EF_ALT_DOWN),
              RunRewriter(KeyF5::Typed(), ui::EF_ALT_DOWN));

    // F6 -> F6, Search + F6 -> Brightness up
    EXPECT_EQ(KeyF6::Typed(), RunRewriter(KeyF6::Typed()));
    EXPECT_EQ(KeyBrightnessUp::Typed(),
              RunRewriter(KeyF6::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyF6::Typed(ui::EF_CONTROL_DOWN),
              RunRewriter(KeyF6::Typed(), ui::EF_CONTROL_DOWN));
    EXPECT_EQ(KeyF6::Typed(ui::EF_ALT_DOWN),
              RunRewriter(KeyF6::Typed(), ui::EF_ALT_DOWN));

    // F7 -> F7, Search + F7 -> Volume mute
    EXPECT_EQ(KeyF7::Typed(), RunRewriter(KeyF7::Typed()));
    EXPECT_EQ(KeyVolumeMute::Typed(),
              RunRewriter(KeyF7::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyF7::Typed(ui::EF_CONTROL_DOWN),
              RunRewriter(KeyF7::Typed(), ui::EF_CONTROL_DOWN));
    EXPECT_EQ(KeyF7::Typed(ui::EF_ALT_DOWN),
              RunRewriter(KeyF7::Typed(), ui::EF_ALT_DOWN));

    // F8 -> F8, Search + F8 -> Volume Down
    EXPECT_EQ(KeyF8::Typed(), RunRewriter(KeyF8::Typed()));
    EXPECT_EQ(KeyVolumeDown::Typed(),
              RunRewriter(KeyF8::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyF8::Typed(ui::EF_CONTROL_DOWN),
              RunRewriter(KeyF8::Typed(), ui::EF_CONTROL_DOWN));
    EXPECT_EQ(KeyF8::Typed(ui::EF_ALT_DOWN),
              RunRewriter(KeyF8::Typed(), ui::EF_ALT_DOWN));

    // F9 -> F9, Search + F9 -> Volume Up
    EXPECT_EQ(KeyF9::Typed(), RunRewriter(KeyF9::Typed()));
    EXPECT_EQ(KeyVolumeUp::Typed(),
              RunRewriter(KeyF9::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyF9::Typed(ui::EF_CONTROL_DOWN),
              RunRewriter(KeyF9::Typed(), ui::EF_CONTROL_DOWN));
    EXPECT_EQ(KeyF9::Typed(ui::EF_ALT_DOWN),
              RunRewriter(KeyF9::Typed(), ui::EF_ALT_DOWN));

    // F10 -> F10, Search + F10 -> F10
    EXPECT_EQ(KeyF10::Typed(), RunRewriter(KeyF10::Typed()));
    EXPECT_EQ(KeyF10::Typed(),
              RunRewriter(KeyF10::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyF10::Typed(ui::EF_CONTROL_DOWN),
              RunRewriter(KeyF10::Typed(), ui::EF_CONTROL_DOWN));
    EXPECT_EQ(KeyF10::Typed(ui::EF_ALT_DOWN),
              RunRewriter(KeyF10::Typed(), ui::EF_ALT_DOWN));

    // F11 -> F11, Search + F11 -> F11
    EXPECT_EQ(KeyF11::Typed(), RunRewriter(KeyF11::Typed()));
    EXPECT_EQ(KeyF11::Typed(),
              RunRewriter(KeyF11::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyF11::Typed(ui::EF_CONTROL_DOWN),
              RunRewriter(KeyF11::Typed(), ui::EF_CONTROL_DOWN));
    EXPECT_EQ(KeyF11::Typed(ui::EF_ALT_DOWN),
              RunRewriter(KeyF11::Typed(), ui::EF_ALT_DOWN));

    // F12 -> F12
    // Search + F12 differs between Wilco devices so it is tested separately.
    EXPECT_EQ(KeyF12::Typed(), RunRewriter(KeyF12::Typed()));
    EXPECT_EQ(KeyF12::Typed(ui::EF_CONTROL_DOWN),
              RunRewriter(KeyF12::Typed(), ui::EF_CONTROL_DOWN));
    EXPECT_EQ(KeyF12::Typed(ui::EF_ALT_DOWN),
              RunRewriter(KeyF12::Typed(), ui::EF_ALT_DOWN));

    // The number row should not be rewritten without Search key.
    EXPECT_EQ(KeyDigit1::Typed(), RunRewriter(KeyDigit1::Typed()));
    EXPECT_EQ(KeyDigit2::Typed(), RunRewriter(KeyDigit2::Typed()));
    EXPECT_EQ(KeyDigit3::Typed(), RunRewriter(KeyDigit3::Typed()));
    EXPECT_EQ(KeyDigit4::Typed(), RunRewriter(KeyDigit4::Typed()));
    EXPECT_EQ(KeyDigit5::Typed(), RunRewriter(KeyDigit5::Typed()));
    EXPECT_EQ(KeyDigit6::Typed(), RunRewriter(KeyDigit6::Typed()));
    EXPECT_EQ(KeyDigit7::Typed(), RunRewriter(KeyDigit7::Typed()));
    EXPECT_EQ(KeyDigit8::Typed(), RunRewriter(KeyDigit8::Typed()));
    EXPECT_EQ(KeyDigit9::Typed(), RunRewriter(KeyDigit9::Typed()));
    EXPECT_EQ(KeyDigit0::Typed(), RunRewriter(KeyDigit0::Typed()));
    EXPECT_EQ(KeyMinus::Typed(), RunRewriter(KeyMinus::Typed()));
    EXPECT_EQ(KeyEqual::Typed(), RunRewriter(KeyEqual::Typed()));
  }

  SetUpKeyboard(kWilco1_0Keyboard);
  // Search + F12 -> Ctrl + Zoom (aka Fullscreen) (Display toggle)
  EXPECT_EQ(
      std::vector(
          {TestKeyEvent{ui::EventType::kKeyPressed, ui::DomCode::F12,
                        ui::DomKey::F12, ui::VKEY_ZOOM, ui::EF_CONTROL_DOWN},
           TestKeyEvent{ui::EventType::kKeyReleased, ui::DomCode::F12,
                        ui::DomKey::F12, ui::VKEY_ZOOM, ui::EF_CONTROL_DOWN}}),
      RunRewriter(KeyF12::Typed(), ui::EF_COMMAND_DOWN));

  SetUpKeyboard(kWilco1_5Keyboard);
  // Search + F12 -> F12 (Privacy screen not supported)
  event_rewriter_ash_->set_privacy_screen_for_testing(false);
  EXPECT_EQ(KeyF12::Typed(), RunRewriter(KeyF12::Typed(), ui::EF_COMMAND_DOWN));

  // F12 -> F12, Search + F12 -> Privacy Screen Toggle
  event_rewriter_ash_->set_privacy_screen_for_testing(true);
  EXPECT_EQ(KeyPrivacyScreenToggle::Typed(),
            RunRewriter(KeyF12::Typed(), ui::EF_COMMAND_DOWN));
}

TEST_P(EventRewriterTest, TestRewriteActionKeysWilcoLayouts) {
  Preferences::RegisterProfilePrefs(prefs()->registry());

  for (const auto& keyboard : kWilcoKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    // Back -> Back, Search + Back -> F1
    EXPECT_EQ(KeyBrowserBack::Typed(), RunRewriter(KeyBrowserBack::Typed()));
    EXPECT_EQ(KeyF1::Typed(),
              RunRewriter(KeyBrowserBack::Typed(), ui::EF_COMMAND_DOWN));

    // Refresh -> Refresh, Search + Refresh -> F2
    EXPECT_EQ(KeyBrowserRefresh::Typed(),
              RunRewriter(KeyBrowserRefresh::Typed()));
    EXPECT_EQ(KeyF2::Typed(),
              RunRewriter(KeyBrowserRefresh::Typed(), ui::EF_COMMAND_DOWN));

    // Full Screen -> Full Screen, Search + Full Screen -> F3
    EXPECT_EQ(KeyZoomToggle::Typed(), RunRewriter(KeyZoomToggle::Typed()));
    EXPECT_EQ(KeyF3::Typed(),
              RunRewriter(KeyZoomToggle::Typed(), ui::EF_COMMAND_DOWN));

    // Launch App 1 -> Launch App 1, Search + Launch App 1 -> F4
    EXPECT_EQ(KeySelectTask::Typed(), RunRewriter(KeySelectTask::Typed()));
    EXPECT_EQ(KeyF4::Typed(),
              RunRewriter(KeySelectTask::Typed(), ui::EF_COMMAND_DOWN));

    // Brightness down -> Brightness Down, Search Brightness Down -> F5
    EXPECT_EQ(KeyBrightnessDown::Typed(),
              RunRewriter(KeyBrightnessDown::Typed()));
    EXPECT_EQ(KeyF5::Typed(),
              RunRewriter(KeyBrightnessDown::Typed(), ui::EF_COMMAND_DOWN));

    // Brightness up -> Brightness Up, Search + Brightness Up -> F6
    EXPECT_EQ(KeyBrightnessUp::Typed(), RunRewriter(KeyBrightnessUp::Typed()));
    EXPECT_EQ(KeyF6::Typed(),
              RunRewriter(KeyBrightnessUp::Typed(), ui::EF_COMMAND_DOWN));

    // Volume mute -> Volume Mute, Search + Volume Mute -> F7
    EXPECT_EQ(KeyVolumeMute::Typed(), RunRewriter(KeyVolumeMute::Typed()));
    EXPECT_EQ(KeyF7::Typed(),
              RunRewriter(KeyVolumeMute::Typed(), ui::EF_COMMAND_DOWN));

    // Volume Down -> Volume Down, Search + Volume Down -> F8
    EXPECT_EQ(KeyVolumeDown::Typed(), RunRewriter(KeyVolumeDown::Typed()));
    EXPECT_EQ(KeyF8::Typed(),
              RunRewriter(KeyVolumeDown::Typed(), ui::EF_COMMAND_DOWN));

    // Volume Up -> Volume Up, Search + Volume Up -> F9
    EXPECT_EQ(KeyVolumeUp::Typed(), RunRewriter(KeyVolumeUp::Typed()));
    EXPECT_EQ(KeyF9::Typed(),
              RunRewriter(KeyVolumeUp::Typed(), ui::EF_COMMAND_DOWN));
  }

  SetUpKeyboard(kWilco1_0Keyboard);
  // Ctrl + Zoom (Display toggle) -> Unchanged
  // Search + Ctrl + Zoom (Display toggle) -> F12
  EXPECT_EQ(KeyZoomToggle::Typed(ui::EF_CONTROL_DOWN),
            RunRewriter(KeyZoomToggle::Typed(), ui::EF_CONTROL_DOWN));
  EXPECT_EQ(KeyF12::Typed(),
            RunRewriter(KeyZoomToggle::Typed(),
                        ui::EF_CONTROL_DOWN | ui::EF_COMMAND_DOWN));

  SetUpKeyboard(kWilco1_5Keyboard);
  {
    // Drallion specific key tests (no privacy screen)
    event_rewriter_ash_->set_privacy_screen_for_testing(false);

    // Privacy Screen Toggle -> F12 (Privacy Screen not supported),
    // Search + Privacy Screen Toggle -> F12
    EXPECT_EQ(KeyF12::Typed(), RunRewriter(KeyPrivacyScreenToggle::Typed()));
    EXPECT_EQ(KeyF12::Typed(), RunRewriter(KeyPrivacyScreenToggle::Typed(),
                                           ui::EF_COMMAND_DOWN));

    // Ctrl + Zoom (Display toggle) -> Unchanged
    // Search + Ctrl + Zoom (Display toggle) -> Unchanged
    EXPECT_EQ(KeyZoomToggle::Typed(ui::EF_CONTROL_DOWN),
              RunRewriter(KeyZoomToggle::Typed(), ui::EF_CONTROL_DOWN));
    EXPECT_EQ(KeyZoomToggle::Typed(ui::EF_CONTROL_DOWN),
              RunRewriter(KeyZoomToggle::Typed(),
                          ui::EF_CONTROL_DOWN | ui::EF_COMMAND_DOWN));
  }

  {
    // Drallion specific key tests (privacy screen supported)
    event_rewriter_ash_->set_privacy_screen_for_testing(true);

    // Privacy Screen Toggle -> Privacy Screen Toggle,
    // Search + Privacy Screen Toggle -> F12
    EXPECT_EQ(KeyPrivacyScreenToggle::Typed(),
              RunRewriter(KeyPrivacyScreenToggle::Typed()));
    EXPECT_EQ(KeyF12::Typed(), RunRewriter(KeyPrivacyScreenToggle::Typed(),
                                           ui::EF_COMMAND_DOWN));

    // Ctrl + Zoom (Display toggle) -> Unchanged
    // Search + Ctrl + Zoom (Display toggle) -> Unchanged
    EXPECT_EQ(KeyZoomToggle::Typed(ui::EF_CONTROL_DOWN),
              RunRewriter(KeyZoomToggle::Typed(), ui::EF_CONTROL_DOWN));
    EXPECT_EQ(KeyZoomToggle::Typed(ui::EF_CONTROL_DOWN),
              RunRewriter(KeyZoomToggle::Typed(),
                          ui::EF_CONTROL_DOWN | ui::EF_COMMAND_DOWN));
  }
}

TEST_P(EventRewriterTest,
       TestRewriteActionKeysWilcoLayoutsSuppressMetaTopRowKeyRewrites) {
  Preferences::RegisterProfilePrefs(prefs()->registry());
  delegate_->SuppressMetaTopRowKeyComboRewrites(true);
  keyboard_settings->suppress_meta_fkey_rewrites = true;

  // With |SuppressMetaTopRowKeyComboRewrites|, all action keys should be
  // unchanged and keep the search modifier.

  for (const auto& keyboard : kWilcoKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    EXPECT_EQ(KeyBrowserBack::Typed(ui::EF_COMMAND_DOWN),
              RunRewriter(KeyBrowserBack::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyBrowserRefresh::Typed(ui::EF_COMMAND_DOWN),
              RunRewriter(KeyBrowserRefresh::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyZoomToggle::Typed(ui::EF_COMMAND_DOWN),
              RunRewriter(KeyZoomToggle::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeySelectTask::Typed(ui::EF_COMMAND_DOWN),
              RunRewriter(KeySelectTask::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyBrightnessDown::Typed(ui::EF_COMMAND_DOWN),
              RunRewriter(KeyBrightnessDown::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyBrightnessUp::Typed(ui::EF_COMMAND_DOWN),
              RunRewriter(KeyBrightnessUp::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyVolumeMute::Typed(ui::EF_COMMAND_DOWN),
              RunRewriter(KeyVolumeMute::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyVolumeDown::Typed(ui::EF_COMMAND_DOWN),
              RunRewriter(KeyVolumeDown::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyVolumeUp::Typed(ui::EF_COMMAND_DOWN),
              RunRewriter(KeyVolumeUp::Typed(), ui::EF_COMMAND_DOWN));

    // F-Keys do not remove Search when pressed.
    EXPECT_EQ(KeyF10::Typed(ui::EF_COMMAND_DOWN),
              RunRewriter(KeyF10::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyF11::Typed(ui::EF_COMMAND_DOWN),
              RunRewriter(KeyF11::Typed(), ui::EF_COMMAND_DOWN));
  }

  SetUpKeyboard(kWilco1_0Keyboard);
  EXPECT_EQ(KeyZoomToggle::Typed(ui::EF_CONTROL_DOWN | ui::EF_COMMAND_DOWN),
            RunRewriter(KeyZoomToggle::Typed(),
                        ui::EF_CONTROL_DOWN | ui::EF_COMMAND_DOWN));

  SetUpKeyboard(kWilco1_5Keyboard);
  {
    // Drallion specific key tests (no privacy screen)
    event_rewriter_ash_->set_privacy_screen_for_testing(false);

    // Search + Privacy Screen Toggle -> Search + F12
    EXPECT_EQ(
        KeyF12::Typed(ui::EF_COMMAND_DOWN),
        RunRewriter(KeyPrivacyScreenToggle::Typed(), ui::EF_COMMAND_DOWN));
    // Search + Ctrl + Zoom (Display toggle) -> Unchanged
    EXPECT_EQ(KeyZoomToggle::Typed(ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN),
              RunRewriter(KeyZoomToggle::Typed(),
                          ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN));
  }

  {
    // Drallion specific key tests (privacy screen supported)
    event_rewriter_ash_->set_privacy_screen_for_testing(true);

    // Search + Privacy Screen Toggle -> F12  TODO
    EXPECT_EQ(
        KeyPrivacyScreenToggle::Typed(ui::EF_COMMAND_DOWN),
        RunRewriter(KeyPrivacyScreenToggle::Typed(), ui::EF_COMMAND_DOWN));
    // Ctrl + Zoom (Display toggle) -> Unchanged  TODO
    // Search + Ctrl + Zoom (Display toggle) -> Unchanged
    EXPECT_EQ(KeyZoomToggle::Typed(ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN),
              RunRewriter(KeyZoomToggle::Typed(),
                          ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN));
  }
}

TEST_P(
    EventRewriterTest,
    TestRewriteActionKeysWilcoLayoutsSuppressMetaTopRowKeyRewritesWithTopRowAreFkeys) {
  Preferences::RegisterProfilePrefs(prefs()->registry());
  delegate_->SuppressMetaTopRowKeyComboRewrites(true);
  keyboard_settings->suppress_meta_fkey_rewrites = true;

  // Enable preference treat-top-row-as-function-keys.
  // That causes action keys to be mapped back to Fn keys.
  BooleanPrefMember top_row_as_fn_key;
  top_row_as_fn_key.Init(prefs::kSendFunctionKeys, prefs());
  top_row_as_fn_key.SetValue(true);
  keyboard_settings->top_row_are_fkeys = true;

  // With |SuppressMetaTopRowKeyComboRewrites| and TopRowAreFKeys, all action
  // keys should be remapped to F-Keys and keep the Search modifier.
  for (const auto& keyboard : kWilcoKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    EXPECT_EQ(KeyF1::Typed(ui::EF_COMMAND_DOWN),
              RunRewriter(KeyBrowserBack::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyF2::Typed(ui::EF_COMMAND_DOWN),
              RunRewriter(KeyBrowserRefresh::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyF3::Typed(ui::EF_COMMAND_DOWN),
              RunRewriter(KeyZoomToggle::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyF4::Typed(ui::EF_COMMAND_DOWN),
              RunRewriter(KeySelectTask::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyF5::Typed(ui::EF_COMMAND_DOWN),
              RunRewriter(KeyBrightnessDown::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyF6::Typed(ui::EF_COMMAND_DOWN),
              RunRewriter(KeyBrightnessUp::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyF7::Typed(ui::EF_COMMAND_DOWN),
              RunRewriter(KeyVolumeMute::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyF8::Typed(ui::EF_COMMAND_DOWN),
              RunRewriter(KeyVolumeDown::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyF9::Typed(ui::EF_COMMAND_DOWN),
              RunRewriter(KeyVolumeUp::Typed(), ui::EF_COMMAND_DOWN));

    EXPECT_EQ(KeyF10::Typed(ui::EF_COMMAND_DOWN),
              RunRewriter(KeyF10::Typed(), ui::EF_COMMAND_DOWN));
    EXPECT_EQ(KeyF11::Typed(ui::EF_COMMAND_DOWN),
              RunRewriter(KeyF11::Typed(), ui::EF_COMMAND_DOWN));
  }

  SetUpKeyboard(kWilco1_0Keyboard);
  EXPECT_EQ(KeyF12::Typed(ui::EF_COMMAND_DOWN),
            RunRewriter(KeyZoomToggle::Typed(),
                        ui::EF_CONTROL_DOWN | ui::EF_COMMAND_DOWN));

  SetUpKeyboard(kWilco1_5Keyboard);
  {
    // Drallion specific key tests (no privacy screen)
    event_rewriter_ash_->set_privacy_screen_for_testing(false);

    // Search + Privacy Screen Toggle -> Search + F12
    EXPECT_EQ(
        KeyF12::Typed(ui::EF_COMMAND_DOWN),
        RunRewriter(KeyPrivacyScreenToggle::Typed(), ui::EF_COMMAND_DOWN));
    // Search + Ctrl + Zoom (Display toggle) -> Unchanged
    EXPECT_EQ(KeyZoomToggle::Typed(ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN),
              RunRewriter(KeyZoomToggle::Typed(),
                          ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN));
  }

  {
    // Drallion specific key tests (privacy screen supported)
    event_rewriter_ash_->set_privacy_screen_for_testing(true);

    // Search + Privacy Screen Toggle -> F12
    EXPECT_EQ(
        KeyF12::Typed(ui::EF_COMMAND_DOWN),
        RunRewriter(KeyPrivacyScreenToggle::Typed(), ui::EF_COMMAND_DOWN));
  }
}

TEST_P(EventRewriterTest, TestTopRowAsFnKeysForKeyboardWilcoLayouts) {
  Preferences::RegisterProfilePrefs(prefs()->registry());

  // Enable preference treat-top-row-as-function-keys.
  // That causes action keys to be mapped back to Fn keys, unless the search
  // key is pressed.
  BooleanPrefMember top_row_as_fn_key;
  top_row_as_fn_key.Init(prefs::kSendFunctionKeys, prefs());
  top_row_as_fn_key.SetValue(true);
  keyboard_settings->top_row_are_fkeys = true;

  for (const auto& keyboard : kWilcoKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    // Back -> F1, Search + Back -> Back
    EXPECT_EQ(KeyF1::Typed(), RunRewriter(KeyBrowserBack::Typed()));
    EXPECT_EQ(KeyBrowserBack::Typed(),
              RunRewriter(KeyBrowserBack::Typed(), ui::EF_COMMAND_DOWN));

    // Refresh -> F2, Search + Refresh -> Refresh
    EXPECT_EQ(KeyF2::Typed(), RunRewriter(KeyBrowserRefresh::Typed()));
    EXPECT_EQ(KeyBrowserRefresh::Typed(),
              RunRewriter(KeyBrowserRefresh::Typed(), ui::EF_COMMAND_DOWN));

    // Full Screen -> F3, Search + Full Screen -> Full Screen
    EXPECT_EQ(KeyF3::Typed(), RunRewriter(KeyZoomToggle::Typed()));
    EXPECT_EQ(KeyZoomToggle::Typed(),
              RunRewriter(KeyZoomToggle::Typed(), ui::EF_COMMAND_DOWN));

    // Launch App 1 -> F4, Search + Launch App 1 -> Launch App 1
    EXPECT_EQ(KeyF4::Typed(), RunRewriter(KeySelectTask::Typed()));
    EXPECT_EQ(KeySelectTask::Typed(),
              RunRewriter(KeySelectTask::Typed(), ui::EF_COMMAND_DOWN));

    // Brightness down -> F5, Search Brightness Down -> Brightness Down
    EXPECT_EQ(KeyF5::Typed(), RunRewriter(KeyBrightnessDown::Typed()));
    EXPECT_EQ(KeyBrightnessDown::Typed(),
              RunRewriter(KeyBrightnessDown::Typed(), ui::EF_COMMAND_DOWN));

    // Brightness up -> F6, Search + Brightness Up -> Brightness Up
    EXPECT_EQ(KeyF6::Typed(), RunRewriter(KeyBrightnessUp::Typed()));
    EXPECT_EQ(KeyBrightnessUp::Typed(),
              RunRewriter(KeyBrightnessUp::Typed(), ui::EF_COMMAND_DOWN));

    // Volume mute -> F7, Search + Volume Mute -> Volume Mute
    EXPECT_EQ(KeyF7::Typed(), RunRewriter(KeyVolumeMute::Typed()));
    EXPECT_EQ(KeyVolumeMute::Typed(),
              RunRewriter(KeyVolumeMute::Typed(), ui::EF_COMMAND_DOWN));

    // Volume Down -> F8, Search + Volume Down -> Volume Down
    EXPECT_EQ(KeyF8::Typed(), RunRewriter(KeyVolumeDown::Typed()));
    EXPECT_EQ(KeyVolumeDown::Typed(),
              RunRewriter(KeyVolumeDown::Typed(), ui::EF_COMMAND_DOWN));

    // Volume Up -> F9, Search + Volume Up -> Volume Up
    EXPECT_EQ(KeyF9::Typed(), RunRewriter(KeyVolumeUp::Typed()));
    EXPECT_EQ(KeyVolumeUp::Typed(),
              RunRewriter(KeyVolumeUp::Typed(), ui::EF_COMMAND_DOWN));

    // F10 -> F10
    EXPECT_EQ(KeyF10::Typed(), RunRewriter(KeyF10::Typed()));
    EXPECT_EQ(KeyF10::Typed(),
              RunRewriter(KeyF10::Typed(), ui::EF_COMMAND_DOWN));

    // F11 -> F11
    EXPECT_EQ(KeyF11::Typed(), RunRewriter(KeyF11::Typed()));
    EXPECT_EQ(KeyF11::Typed(),
              RunRewriter(KeyF11::Typed(), ui::EF_COMMAND_DOWN));
  }

  SetUpKeyboard(kWilco1_0Keyboard);
  // Ctrl + Zoom (Display toggle) -> F12
  // Search + Ctrl + Zoom (Display toggle) -> Search modifier should be removed
  EXPECT_EQ(KeyF12::Typed(),
            RunRewriter(KeyZoomToggle::Typed(), ui::EF_CONTROL_DOWN));
  EXPECT_EQ(KeyZoomToggle::Typed(ui::EF_CONTROL_DOWN),
            RunRewriter(KeyZoomToggle::Typed(),
                        ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN));

  SetUpKeyboard(kWilco1_5Keyboard);
  {
    // Drallion specific key tests (no privacy screen)
    event_rewriter_ash_->set_privacy_screen_for_testing(false);

    // Privacy Screen Toggle -> F12,
    // Search + Privacy Screen Toggle -> F12 (Privacy screen not supported)
    EXPECT_EQ(KeyF12::Typed(), RunRewriter(KeyPrivacyScreenToggle::Typed()));
    EXPECT_EQ(KeyF12::Typed(), RunRewriter(KeyPrivacyScreenToggle::Typed(),
                                           ui::EF_COMMAND_DOWN));

    // Ctrl + Zoom (Display toggle) -> Unchanged
    // Search + Ctrl + Zoom (Display toggle) -> Unchanged
    EXPECT_EQ(KeyZoomToggle::Typed(ui::EF_CONTROL_DOWN),
              RunRewriter(KeyZoomToggle::Typed(), ui::EF_CONTROL_DOWN));
    EXPECT_EQ(KeyZoomToggle::Typed(ui::EF_CONTROL_DOWN),
              RunRewriter(KeyZoomToggle::Typed(),
                          ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN));
  }

  {
    // Drallion specific key tests (privacy screen supported)
    event_rewriter_ash_->set_privacy_screen_for_testing(true);

    // Privacy Screen Toggle -> F12,
    // Search + Privacy Screen Toggle -> Unchanged
    EXPECT_EQ(KeyF12::Typed(), RunRewriter(KeyPrivacyScreenToggle::Typed()));
    EXPECT_EQ(
        KeyPrivacyScreenToggle::Typed(),
        RunRewriter(KeyPrivacyScreenToggle::Typed(), ui::EF_COMMAND_DOWN));
  }
}

TEST_P(EventRewriterTest, TestRewriteFunctionKeysInvalidLayout) {
  Preferences::RegisterProfilePrefs(prefs()->registry());

  // Not adding a keyboard simulates a failure in getting top row layout, which
  // will fallback to Layout1 in which case keys are rewritten to their default
  // values.
  EXPECT_EQ(KeyBrowserForward::Typed(), RunRewriter(KeyF2::Typed()));
  EXPECT_EQ(KeyBrowserRefresh::Typed(), RunRewriter(KeyF3::Typed()));
  EXPECT_EQ(KeyZoomToggle::Typed(), RunRewriter(KeyF4::Typed()));
  EXPECT_EQ(KeyBrightnessUp::Typed(), RunRewriter(KeyF7::Typed()));

  // Adding a keyboard with a valid layout will take effect.
  SetUpKeyboard({.name = "Internal Keyboard",
                 .layout = kKbdTopRowLayout2Tag,
                 .type = ui::INPUT_DEVICE_INTERNAL,
                 .has_custom_top_row = false});
  EXPECT_EQ(KeyBrowserRefresh::Typed(), RunRewriter(KeyF2::Typed()));
  EXPECT_EQ(KeyZoomToggle::Typed(), RunRewriter(KeyF3::Typed()));
  EXPECT_EQ(KeySelectTask::Typed(), RunRewriter(KeyF4::Typed()));
  EXPECT_EQ(KeyMediaPlayPause::Typed(), RunRewriter(KeyF7::Typed()));
}

// Tests that event rewrites still work even if modifiers are remapped.
TEST_P(EventRewriterTest, TestRewriteExtendedKeysWithControlRemapped) {
  // Remap Control to Search.
  Preferences::RegisterProfilePrefs(prefs()->registry());
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAltClickAndSixPackCustomization);
  IntegerPrefMember search;
  InitModifierKeyPref(&search, ::prefs::kLanguageRemapControlKeyTo,
                      ui::mojom::ModifierKey::kControl,
                      ui::mojom::ModifierKey::kMeta);

  for (const auto& keyboard : kChromeKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    EXPECT_EQ(KeyEnd::Typed(),
              RunRewriter(KeyArrowRight::Typed(), ui::EF_CONTROL_DOWN));
    EXPECT_EQ(KeyEnd::Typed(ui::EF_SHIFT_DOWN),
              RunRewriter(KeyArrowRight::Typed(),
                          ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN));
  }
}

TEST_P(EventRewriterTest, TestRewriteKeyEventSentByXSendEvent) {
  // Remap Control to Alt.
  Preferences::RegisterProfilePrefs(prefs()->registry());
  IntegerPrefMember control;
  InitModifierKeyPref(&control, ::prefs::kLanguageRemapControlKeyTo,
                      ui::mojom::ModifierKey::kControl,
                      ui::mojom::ModifierKey::kAlt);

  SetUpKeyboard(kInternalChromeKeyboard);

  // Send left control press.
  {
    // Control should NOT be remapped to Alt if EF_FINAL is set.
    EXPECT_EQ(KeyLControl::Typed(ui::EF_FINAL),
              SendKeyEvents(KeyLControl::Typed(ui::EF_FINAL)));
  }
}

TEST_P(EventRewriterTest, TestRewriteNonNativeEvent) {
  // Remap Control to Alt.
  Preferences::RegisterProfilePrefs(prefs()->registry());
  IntegerPrefMember control;
  InitModifierKeyPref(&control, ::prefs::kLanguageRemapControlKeyTo,
                      ui::mojom::ModifierKey::kControl,
                      ui::mojom::ModifierKey::kAlt);

  SetUpKeyboard(kInternalChromeKeyboard);

  // For EF_CONTROL_DOWN modifier.
  SendKeyEvent(KeyLControl::Pressed());

  const int kTouchId = 2;
  gfx::Point location(0, 0);
  ui::TouchEvent press(
      ui::EventType::kTouchPressed, location, base::TimeTicks(),
      ui::PointerDetails(ui::EventPointerType::kTouch, kTouchId));
  press.SetFlags(ui::EF_CONTROL_DOWN);

  source().Send(&press);
  auto events = TakeEvents();
  ASSERT_EQ(1u, events.size());
  // Control should be remapped to Alt.
  EXPECT_EQ(ui::EF_ALT_DOWN,
            events[0]->flags() & (ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN));
}

TEST_P(EventRewriterTest, TopRowKeysAreFunctionKeys) {
  Preferences::RegisterProfilePrefs(prefs()->registry());
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(1));
  wm::ActivateWindow(window.get());

  // Create a simulated keypress of F1 targetted at the window.
  ui::KeyEvent press_f1(ui::EventType::kKeyPressed, ui::VKEY_F1,
                        ui::DomCode::F1, ui::EF_NONE, ui::DomKey::F1,
                        ui::EventTimeForNow());

  // The event should also not be rewritten if the send-function-keys pref is
  // additionally set, for both apps v2 and regular windows.
  BooleanPrefMember send_function_keys_pref;
  send_function_keys_pref.Init(prefs::kSendFunctionKeys, prefs());
  send_function_keys_pref.SetValue(true);
  keyboard_settings->top_row_are_fkeys = true;
  EXPECT_EQ(KeyF1::Typed(), RunRewriter(KeyF1::Typed()));

  // If the pref isn't set when an event is sent to a regular window, F1 is
  // rewritten to the back key.
  send_function_keys_pref.SetValue(false);
  keyboard_settings->top_row_are_fkeys = false;
  EXPECT_EQ(KeyBrowserBack::Typed(), RunRewriter(KeyF1::Typed()));
}

// Parameterized version of test with the same name that accepts the
// event flags that correspond to a right-click. This will be either
// Alt+Click or Search+Click. After a transition period this will
// default to Search+Click and the Alt+Click logic will be removed.
void EventRewriterTestBase::DontRewriteIfNotRewritten(int right_click_flags) {
  Preferences::RegisterProfilePrefs(prefs()->registry());
  ui::DeviceDataManager* device_data_manager =
      ui::DeviceDataManager::GetInstance();
  std::vector<ui::TouchpadDevice> touchpad_devices(2);
  touchpad_devices[0].id = kTouchpadId1;
  touchpad_devices[1].id = kTouchpadId2;
  static_cast<ui::DeviceHotplugEventObserver*>(device_data_manager)
      ->OnTouchpadDevicesUpdated(touchpad_devices);
  std::vector<ui::InputDevice> mouse_devices(1);
  constexpr int kMouseId = 12;
  touchpad_devices[0].id = kMouseId;
  static_cast<ui::DeviceHotplugEventObserver*>(device_data_manager)
      ->OnMouseDevicesUpdated(mouse_devices);

  // Test (Alt|Search) + Left click.
  {
    ui::MouseEvent press(ui::EventType::kMousePressed, gfx::Point(),
                         gfx::Point(), ui::EventTimeForNow(), right_click_flags,
                         ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_press(&press);
    test_press.set_source_device_id(kTouchpadId1);
    // Sanity check.
    EXPECT_EQ(ui::EventType::kMousePressed, press.type());
    EXPECT_EQ(right_click_flags, press.flags());
    const ui::MouseEvent result = RewriteMouseButtonEvent(press);
    EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & result.flags());
    EXPECT_NE(right_click_flags, right_click_flags & result.flags());
    EXPECT_EQ(ui::EF_RIGHT_MOUSE_BUTTON, result.changed_button_flags());
  }
  {
    ui::MouseEvent release(ui::EventType::kMouseReleased, gfx::Point(),
                           gfx::Point(), ui::EventTimeForNow(),
                           right_click_flags, ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_release(&release);
    test_release.set_source_device_id(kTouchpadId1);
    const ui::MouseEvent result = RewriteMouseButtonEvent(release);
    EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & result.flags());
    EXPECT_NE(right_click_flags, right_click_flags & result.flags());
    EXPECT_EQ(ui::EF_RIGHT_MOUSE_BUTTON, result.changed_button_flags());
  }

  // No (ALT|SEARCH) in first click.
  {
    ui::MouseEvent press(ui::EventType::kMousePressed, gfx::Point(),
                         gfx::Point(), ui::EventTimeForNow(),
                         ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_press(&press);
    test_press.set_source_device_id(kTouchpadId1);
    const ui::MouseEvent result = RewriteMouseButtonEvent(press);
    EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & result.flags());
    EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, result.changed_button_flags());
  }
  {
    ui::MouseEvent release(ui::EventType::kMouseReleased, gfx::Point(),
                           gfx::Point(), ui::EventTimeForNow(),
                           right_click_flags, ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_release(&release);
    test_release.set_source_device_id(kTouchpadId1);
    const ui::MouseEvent result = RewriteMouseButtonEvent(release);
    EXPECT_EQ(right_click_flags, right_click_flags & result.flags());
    EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, result.changed_button_flags());
  }

  // ALT on different device.
  {
    ui::MouseEvent press(ui::EventType::kMousePressed, gfx::Point(),
                         gfx::Point(), ui::EventTimeForNow(), right_click_flags,
                         ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_press(&press);
    test_press.set_source_device_id(kTouchpadId2);
    const ui::MouseEvent result = RewriteMouseButtonEvent(press);
    EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & result.flags());
    EXPECT_NE(right_click_flags, right_click_flags & result.flags());
    EXPECT_EQ(ui::EF_RIGHT_MOUSE_BUTTON, result.changed_button_flags());
  }
  {
    ui::MouseEvent release(ui::EventType::kMouseReleased, gfx::Point(),
                           gfx::Point(), ui::EventTimeForNow(),
                           right_click_flags, ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_release(&release);
    test_release.set_source_device_id(kTouchpadId1);
    const ui::MouseEvent result = RewriteMouseButtonEvent(release);
    EXPECT_EQ(right_click_flags, right_click_flags & result.flags());
    EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, result.changed_button_flags());
  }
  {
    ui::MouseEvent release(ui::EventType::kMouseReleased, gfx::Point(),
                           gfx::Point(), ui::EventTimeForNow(),
                           right_click_flags, ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_release(&release);
    test_release.set_source_device_id(kTouchpadId2);
    const ui::MouseEvent result = RewriteMouseButtonEvent(release);
    EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & result.flags());
    EXPECT_NE(right_click_flags, right_click_flags & result.flags());
    EXPECT_EQ(ui::EF_RIGHT_MOUSE_BUTTON, result.changed_button_flags());
  }

  // No rewrite for non-touchpad devices.
  {
    ui::MouseEvent press(ui::EventType::kMousePressed, gfx::Point(),
                         gfx::Point(), ui::EventTimeForNow(), right_click_flags,
                         ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_press(&press);
    test_press.set_source_device_id(kMouseId);
    EXPECT_EQ(ui::EventType::kMousePressed, press.type());
    EXPECT_EQ(right_click_flags, press.flags());
    const ui::MouseEvent result = RewriteMouseButtonEvent(press);
    EXPECT_EQ(right_click_flags, right_click_flags & result.flags());
    EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, result.changed_button_flags());
  }
  {
    ui::MouseEvent release(ui::EventType::kMouseReleased, gfx::Point(),
                           gfx::Point(), ui::EventTimeForNow(),
                           right_click_flags, ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_release(&release);
    test_release.set_source_device_id(kMouseId);
    const ui::MouseEvent result = RewriteMouseButtonEvent(release);
    EXPECT_EQ(right_click_flags, right_click_flags & result.flags());
    EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, result.changed_button_flags());
  }

  // Still rewrite to right button, even if the modifier key is already
  // released when the mouse release event happens
  // This is for regressions such as:
  // https://crbug.com/1399284
  // https://crbug.com/1417079
  {
    ui::MouseEvent press(ui::EventType::kMousePressed, gfx::Point(),
                         gfx::Point(), ui::EventTimeForNow(), right_click_flags,
                         ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_press(&press);
    test_press.set_source_device_id(kTouchpadId1);
    // Sanity check.
    EXPECT_EQ(ui::EventType::kMousePressed, press.type());
    EXPECT_EQ(right_click_flags, press.flags());
    const ui::MouseEvent result = RewriteMouseButtonEvent(press);
    EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & result.flags());
    EXPECT_NE(right_click_flags, right_click_flags & result.flags());
    EXPECT_EQ(ui::EF_RIGHT_MOUSE_BUTTON, result.changed_button_flags());
  }
  {
    ui::MouseEvent release(ui::EventType::kMouseReleased, gfx::Point(),
                           gfx::Point(), ui::EventTimeForNow(),
                           ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_release(&release);
    test_release.set_source_device_id(kTouchpadId1);
    const ui::MouseEvent result = RewriteMouseButtonEvent(release);
    EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & result.flags());
    EXPECT_NE(right_click_flags, right_click_flags & result.flags());
    EXPECT_EQ(ui::EF_RIGHT_MOUSE_BUTTON, result.changed_button_flags());
  }
}

TEST_P(EventRewriterTest, DontRewriteIfNotRewritten_AltClickIsRightClick) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAltClickAndSixPackCustomization);
  DontRewriteIfNotRewritten(ui::EF_LEFT_MOUSE_BUTTON | ui::EF_ALT_DOWN);
  EXPECT_EQ(message_center_.NotificationCount(), 0u);
}

TEST_P(EventRewriterTest, DontRewriteIfNotRewritten_AltClickIsRightClick_New) {
  // Enabling the kImprovedKeyboardShortcuts feature does not change alt+click
  // behavior or create a notification.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {::features::kImprovedKeyboardShortcuts},
      {features::kAltClickAndSixPackCustomization});
  DontRewriteIfNotRewritten(ui::EF_LEFT_MOUSE_BUTTON | ui::EF_ALT_DOWN);
  EXPECT_EQ(message_center_.NotificationCount(), 0u);
}

TEST_P(EventRewriterTest, DontRewriteIfNotRewritten_SearchClickIsRightClick) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kUseSearchClickForRightClick},
      {features::kAltClickAndSixPackCustomization});
  DontRewriteIfNotRewritten(ui::EF_LEFT_MOUSE_BUTTON | ui::EF_COMMAND_DOWN);
  EXPECT_EQ(message_center_.NotificationCount(), 0u);
}

TEST_P(EventRewriterTest, DontRewriteIfNotRewritten_AltClickDeprecated) {
  // Pressing search+click with alt+click deprecated works, but does not
  // generate a notification.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {::features::kDeprecateAltClick},
      {features::kAltClickAndSixPackCustomization});
  DontRewriteIfNotRewritten(ui::EF_LEFT_MOUSE_BUTTON | ui::EF_COMMAND_DOWN);
  EXPECT_EQ(message_center_.NotificationCount(), 0u);
}

TEST_P(EventRewriterTest, DeprecatedAltClickGeneratesNotification) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {::features::kDeprecateAltClick},
      {features::kAltClickAndSixPackCustomization});
  ui::DeviceDataManager* device_data_manager =
      ui::DeviceDataManager::GetInstance();
  std::vector<ui::TouchpadDevice> touchpad_devices(1);
  constexpr int kTouchpadId1 = 10;
  touchpad_devices[0].id = kTouchpadId1;
  static_cast<ui::DeviceHotplugEventObserver*>(device_data_manager)
      ->OnTouchpadDevicesUpdated(touchpad_devices);
  std::vector<ui::InputDevice> mouse_devices(1);
  constexpr int kMouseId = 12;
  touchpad_devices[0].id = kMouseId;
  static_cast<ui::DeviceHotplugEventObserver*>(device_data_manager)
      ->OnMouseDevicesUpdated(mouse_devices);

  const int deprecated_flags = ui::EF_LEFT_MOUSE_BUTTON | ui::EF_ALT_DOWN;

  // Alt + Left click => No rewrite.
  {
    ui::MouseEvent press(ui::EventType::kMousePressed, gfx::Point(),
                         gfx::Point(), ui::EventTimeForNow(), deprecated_flags,
                         ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_press(&press);
    test_press.set_source_device_id(kTouchpadId1);
    // Sanity check.
    EXPECT_EQ(ui::EventType::kMousePressed, press.type());
    EXPECT_EQ(deprecated_flags, press.flags());
    const ui::MouseEvent result = RewriteMouseButtonEvent(press);

    // No rewrite occurred.
    EXPECT_EQ(deprecated_flags, deprecated_flags & result.flags());
    EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, result.changed_button_flags());

    // Expect a deprecation notification.
    EXPECT_EQ(message_center_.NotificationCount(), 1u);
    ClearNotifications();
  }
  {
    ui::MouseEvent release(ui::EventType::kMouseReleased, gfx::Point(),
                           gfx::Point(), ui::EventTimeForNow(),
                           deprecated_flags, ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_release(&release);
    test_release.set_source_device_id(kTouchpadId1);
    const ui::MouseEvent result = RewriteMouseButtonEvent(release);

    // No rewrite occurred.
    EXPECT_EQ(deprecated_flags, deprecated_flags & result.flags());
    EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, result.changed_button_flags());

    // Don't expect a new notification on release.
    EXPECT_EQ(message_center_.NotificationCount(), 0u);
  }

  // No rewrite or notification for non-touchpad devices.
  {
    ui::MouseEvent press(ui::EventType::kMousePressed, gfx::Point(),
                         gfx::Point(), ui::EventTimeForNow(), deprecated_flags,
                         ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_press(&press);
    test_press.set_source_device_id(kMouseId);
    EXPECT_EQ(ui::EventType::kMousePressed, press.type());
    EXPECT_EQ(deprecated_flags, press.flags());
    const ui::MouseEvent result = RewriteMouseButtonEvent(press);
    EXPECT_EQ(deprecated_flags, deprecated_flags & result.flags());
    EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, result.changed_button_flags());

    // No notification expected for this case.
    EXPECT_EQ(message_center_.NotificationCount(), 0u);
  }
  {
    ui::MouseEvent release(ui::EventType::kMouseReleased, gfx::Point(),
                           gfx::Point(), ui::EventTimeForNow(),
                           deprecated_flags, ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_release(&release);
    test_release.set_source_device_id(kMouseId);
    const ui::MouseEvent result = RewriteMouseButtonEvent(release);
    EXPECT_EQ(deprecated_flags, deprecated_flags & result.flags());
    EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, result.changed_button_flags());

    // No notification expected for this case.
    EXPECT_EQ(message_center_.NotificationCount(), 0u);
  }
}

TEST_P(EventRewriterTest, StickyKeyEventDispatchImpl) {
  Shell::Get()->sticky_keys_controller()->Enable(true);
  // Test the actual key event dispatch implementation.
  {
    auto events = SendKeyEvents(KeyLControl::Typed());
    ASSERT_EQ(1u, events.size());
    EXPECT_EQ(KeyLControl::Pressed(), events[0]);
  }

  // Test key press event is correctly modified and modifier release
  // event is sent.
  {
    auto events = SendKeyEvent(KeyC::Pressed());
    ASSERT_EQ(2u, events.size());
    EXPECT_EQ(KeyC::Pressed(ui::EF_CONTROL_DOWN), events[0]);
    EXPECT_EQ(KeyLControl::Released(), events[1]);
  }

  // Test key release event is not modified.
  {
    auto events = SendKeyEvent(KeyC::Released());
    ASSERT_EQ(1u, events.size());
    EXPECT_EQ(KeyC::Released(), events[0]);
  }
}

TEST_P(EventRewriterTest, MouseEventDispatchImpl) {
  Shell::Get()->sticky_keys_controller()->Enable(true);
  SendKeyEvents(KeyLControl::Typed());

  // Test mouse press event is correctly modified.
  gfx::Point location(0, 0);
  ui::MouseEvent press(ui::EventType::kMousePressed, location, location,
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                       ui::EF_LEFT_MOUSE_BUTTON);
  ui::EventDispatchDetails details = source().Send(&press);
  ASSERT_FALSE(details.dispatcher_destroyed);
  {
    auto events = TakeEvents();
    ASSERT_EQ(1u, events.size());
    EXPECT_EQ(ui::EventType::kMousePressed, events[0]->type());
    EXPECT_TRUE(events[0]->flags() & ui::EF_CONTROL_DOWN);
  }

  // Test mouse release event is correctly modified and modifier release
  // event is sent. The mouse event should have the correct DIP location.
  ui::MouseEvent release(ui::EventType::kMouseReleased, location, location,
                         ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                         ui::EF_LEFT_MOUSE_BUTTON);
  details = source().Send(&release);
  ASSERT_FALSE(details.dispatcher_destroyed);
  {
    auto events = TakeEvents();
    ASSERT_EQ(2u, events.size());
    EXPECT_EQ(ui::EventType::kMouseReleased, events[0]->type());
    EXPECT_TRUE(events[0]->flags() & ui::EF_CONTROL_DOWN);

    ASSERT_EQ(ui::EventType::kKeyReleased, events[1]->type());
    EXPECT_EQ(ui::VKEY_CONTROL, events[1]->AsKeyEvent()->key_code());
  }
}

TEST_P(EventRewriterTest, MouseWheelEventDispatchImpl) {
  Shell::Get()->sticky_keys_controller()->Enable(true);
  // Test positive mouse wheel event is correctly modified and modifier release
  // event is sent.
  SendKeyEvents(KeyLControl::Typed());

  gfx::Point location(0, 0);
  ui::MouseWheelEvent positive(
      gfx::Vector2d(0, ui::MouseWheelEvent::kWheelDelta), location, location,
      ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
      ui::EF_LEFT_MOUSE_BUTTON);
  ui::EventDispatchDetails details = source().Send(&positive);
  ASSERT_FALSE(details.dispatcher_destroyed);
  {
    auto events = TakeEvents();
    ASSERT_EQ(2u, events.size());
    EXPECT_TRUE(events[0]->IsMouseWheelEvent());
    EXPECT_TRUE(events[0]->flags() & ui::EF_CONTROL_DOWN);

    ASSERT_EQ(ui::EventType::kKeyReleased, events[1]->type());
    EXPECT_EQ(ui::VKEY_CONTROL, events[1]->AsKeyEvent()->key_code());
  }

  // Test negative mouse wheel event is correctly modified and modifier release
  // event is sent.
  SendKeyEvents({KeyLControl::Pressed(), KeyLControl::Released()});

  ui::MouseWheelEvent negative(
      gfx::Vector2d(0, -ui::MouseWheelEvent::kWheelDelta), location, location,
      ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
      ui::EF_LEFT_MOUSE_BUTTON);
  details = source().Send(&negative);
  ASSERT_FALSE(details.dispatcher_destroyed);
  {
    auto events = TakeEvents();
    ASSERT_EQ(2u, events.size());
    EXPECT_TRUE(events[0]->IsMouseWheelEvent());
    EXPECT_TRUE(events[0]->flags() & ui::EF_CONTROL_DOWN);

    ASSERT_EQ(ui::EventType::kKeyReleased, events[1]->type());
    EXPECT_EQ(ui::VKEY_CONTROL, events[1]->AsKeyEvent()->key_code());
  }
}

// Tests that if modifier keys are remapped, the flags of a mouse wheel event
// will be rewritten properly.
TEST_P(EventRewriterTest, MouseWheelEventModifiersRewritten) {
  Preferences::RegisterProfilePrefs(prefs()->registry());

  // For EF_CONTROL_DOWN modifier.
  SendKeyEvent(KeyLControl::Pressed());

  // Generate a mouse wheel event that has a CONTROL_DOWN modifier flag and
  // expect that no rewriting happens as no modifier remapping is active.
  gfx::Point location(0, 0);
  ui::MouseWheelEvent positive(
      gfx::Vector2d(0, ui::MouseWheelEvent::kWheelDelta), location, location,
      ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON | ui::EF_CONTROL_DOWN,
      ui::EF_LEFT_MOUSE_BUTTON);
  ui::EventDispatchDetails details = source().Send(&positive);
  ASSERT_FALSE(details.dispatcher_destroyed);
  {
    auto events = TakeEvents();
    ASSERT_EQ(1u, events.size());
    EXPECT_TRUE(events[0]->IsMouseWheelEvent());
    EXPECT_TRUE(events[0]->flags() & ui::EF_CONTROL_DOWN);
  }

  // Remap Control to Alt.
  IntegerPrefMember control;
  InitModifierKeyPref(&control, ::prefs::kLanguageRemapControlKeyTo,
                      ui::mojom::ModifierKey::kControl,
                      ui::mojom::ModifierKey::kAlt);

  // Sends the same events once again and expect that it will be rewritten to
  // ALT_DOWN in older implementation, or not rewritten (as Control is held)
  // in the new implementation.
  details = source().Send(&positive);
  ASSERT_FALSE(details.dispatcher_destroyed);
  {
    auto events = TakeEvents();
    ASSERT_EQ(1u, events.size());
    EXPECT_TRUE(events[0]->IsMouseWheelEvent());
    if (ash::features::IsKeyboardRewriterFixEnabled()) {
      EXPECT_TRUE(events[0]->flags() & ui::EF_CONTROL_DOWN);
      EXPECT_FALSE(events[0]->flags() & ui::EF_ALT_DOWN);
    } else {
      EXPECT_FALSE(events[0]->flags() & ui::EF_CONTROL_DOWN);
      EXPECT_TRUE(events[0]->flags() & ui::EF_ALT_DOWN);
    }
  }
}

TEST_P(EventRewriterTest, MouseEventMaintainNativeEvent) {
  if (!features::IsKeyboardRewriterFixEnabled()) {
    GTEST_SKIP() << "Test is only valid with keyboard rewriter fix enabled";
  }

  Preferences::RegisterProfilePrefs(prefs()->registry());

  gfx::Point location(0, 0);
  ui::MouseEvent native_event(ui::EventType::kMouseMoved, location, location,
                              /*time_stamp=*/{}, ui::EF_CONTROL_DOWN,
                              ui::EF_NONE);
  ui::MouseEvent mouse_event(&native_event);
  // Remap Control to Alt.
  IntegerPrefMember control;
  InitModifierKeyPref(&control, ::prefs::kLanguageRemapControlKeyTo,
                      ui::mojom::ModifierKey::kControl,
                      ui::mojom::ModifierKey::kAlt);

  SendKeyEvent(KeyLControl::Pressed());

  // Sends the same events once again and expect that it will be rewritten to
  // ALT_DOWN in older implementation, or not rewritten (as Control is held)
  // in the new implementation.
  ui::EventDispatchDetails details = source().Send(&mouse_event);
  ASSERT_FALSE(details.dispatcher_destroyed);
  {
    auto events = TakeEvents();
    ASSERT_EQ(1u, events.size());
    EXPECT_TRUE(events[0]->IsMouseEvent());
    EXPECT_FALSE(events[0]->flags() & ui::EF_CONTROL_DOWN);
    EXPECT_TRUE(events[0]->flags() & ui::EF_ALT_DOWN);
    EXPECT_TRUE(events[0]->HasNativeEvent());
  }
}

// Tests edge cases of key event rewriting (see https://crbug.com/913209).
TEST_P(EventRewriterTest, KeyEventRewritingEdgeCases) {
  Preferences::RegisterProfilePrefs(prefs()->registry());

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAltClickAndSixPackCustomization);

  // Edge case 1: Press the Launcher button first. Then press the Up Arrow
  // button.
  {
    auto events = SendKeyEvents(
        {KeyLMeta::Pressed(), KeyArrowUp::Pressed(ui::EF_COMMAND_DOWN)});
    EXPECT_EQ(2u, events.size());
  }

  // When releasing the Launcher button, the rewritten event should be released
  // as well.
  if (features::IsKeyboardRewriterFixEnabled()) {
    EXPECT_EQ(std::vector({KeyLMeta::Released()}),
              SendKeyEvent(KeyLMeta::Released()));
    EXPECT_EQ(std::vector({KeyPageUp::Released(), KeyArrowUp::Pressed()}),
              SendKeyEvent(KeyArrowUp::Pressed()));
  } else {
    auto events = SendKeyEvent(KeyLMeta::Released());
    ASSERT_EQ(2u, events.size());
    EXPECT_EQ(KeyLMeta::Released(), events[0]);
    EXPECT_EQ(KeyPageUp::Released(), events[1]);
  }

  // Edge case 2: Press the Up Arrow button first. Then press the Launch button.
  {
    auto events = SendKeyEvents({KeyArrowUp::Pressed(), KeyLMeta::Pressed()});
    EXPECT_EQ(2u, events.size());
  }

  // When releasing the Up Arrow button, the rewritten event should be blocked.
  {
    auto events = SendKeyEvent(KeyArrowUp::Released(ui::EF_COMMAND_DOWN));
    ASSERT_EQ(1u, events.size());
    EXPECT_EQ(KeyArrowUp::Released(ui::EF_COMMAND_DOWN), events[0]);
  }
}

TEST_P(EventRewriterTest, ScrollEventDispatchImpl) {
  Shell::Get()->sticky_keys_controller()->Enable(true);
  // Test scroll event is correctly modified.
  SendKeyEvents(KeyLControl::Typed());

  gfx::PointF location(0, 0);
  ui::ScrollEvent scroll(ui::EventType::kScroll, location, location,
                         ui::EventTimeForNow(), 0 /* flag */, 0 /* x_offset */,
                         1 /* y_offset */, 0 /* x_offset_ordinal */,
                         1 /* y_offset_ordinal */, 2 /* finger */);
  ui::EventDispatchDetails details = source().Send(&scroll);
  ASSERT_FALSE(details.dispatcher_destroyed);
  {
    auto events = TakeEvents();
    ASSERT_EQ(1u, events.size());
    EXPECT_TRUE(events[0]->IsScrollEvent());
    EXPECT_TRUE(events[0]->flags() & ui::EF_CONTROL_DOWN);
  }

  // Test FLING_START event deactivates the sticky key, but is modified.
  ui::ScrollEvent fling_start(
      ui::EventType::kScrollFlingStart, location, location,
      ui::EventTimeForNow(), 0 /* flag */, 0 /* x_offset */, 0 /* y_offset */,
      0 /* x_offset_ordinal */, 0 /* y_offset_ordinal */, 2 /* finger */);
  details = source().Send(&fling_start);
  {
    auto events = TakeEvents();
    ASSERT_EQ(2u, events.size());
    EXPECT_TRUE(events[0]->IsScrollEvent());
    EXPECT_TRUE(events[0]->flags() & ui::EF_CONTROL_DOWN);

    ASSERT_EQ(ui::EventType::kKeyReleased, events[1]->type());
    EXPECT_EQ(ui::VKEY_CONTROL, events[1]->AsKeyEvent()->key_code());
  }

  // Test scroll direction change causes that modifier release event is sent.
  SendKeyEvents(KeyLControl::Typed());

  details = source().Send(&scroll);
  ASSERT_FALSE(details.dispatcher_destroyed);
  std::ignore = TakeEvents();

  ui::ScrollEvent scroll2(ui::EventType::kScroll, location, location,
                          ui::EventTimeForNow(), 0 /* flag */, 0 /* x_offset */,
                          -1 /* y_offset */, 0 /* x_offset_ordinal */,
                          -1 /* y_offset_ordinal */, 2 /* finger */);
  details = source().Send(&scroll2);
  ASSERT_FALSE(details.dispatcher_destroyed);
  {
    auto events = TakeEvents();
    ASSERT_EQ(2u, events.size());
    EXPECT_TRUE(events[0]->IsScrollEvent());
    EXPECT_FALSE(events[0]->flags() & ui::EF_CONTROL_DOWN);

    ASSERT_EQ(ui::EventType::kKeyReleased, events[1]->type());
    EXPECT_EQ(ui::VKEY_CONTROL, events[1]->AsKeyEvent()->key_code());
  }
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
TEST_P(EventRewriterTest, RemapHangulOnCros1p) {
  Preferences::RegisterProfilePrefs(prefs()->registry());
  IntegerPrefMember alt;
  InitModifierKeyPref(&alt, ::prefs::kLanguageRemapAltKeyTo,
                      ui::mojom::ModifierKey::kAlt,
                      ui::mojom::ModifierKey::kAlt);

  scoped_refptr<input_method::MockInputMethodManagerImpl::State> state =
      base::MakeRefCounted<input_method::MockInputMethodManagerImpl::State>(
          input_method_manager_mock_);
  input_method_manager_mock_->SetState(state);

  for (const auto& keyboard : kAllKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);
    state->current_input_method_id =
        base::StrCat({kCros1pInputMethodIdPrefix, "ko-t-i0-und"});
    EXPECT_EQ(KeyHangulMode::Typed(), RunRewriter(KeyHangulMode::Typed()));
    EXPECT_EQ(KeyLAlt::Typed(), RunRewriter(KeyLAlt::Typed()));
    EXPECT_EQ(KeyRAlt::Typed(), RunRewriter(KeyRAlt::Typed()));

    state->current_input_method_id =
        base::StrCat({kCros1pInputMethodIdPrefix, "xkb:us::eng"});
    EXPECT_EQ(KeyRAlt::Typed(), RunRewriter(KeyHangulMode::Typed()));
    EXPECT_EQ(KeyLAlt::Typed(), RunRewriter(KeyLAlt::Typed()));
    EXPECT_EQ(KeyRAlt::Typed(), RunRewriter(KeyRAlt::Typed()));
  }
}
#endif

class EventRewriterInputSettingsSplitDisabledTest : public EventRewriterTest {
 public:
  void SetUp() override {
    settings_split_disable_feature_list_.InitAndDisableFeature(
        features::kInputDeviceSettingsSplit);
    EventRewriterTest::SetUp();
  }

  void TearDown() override {
    EventRewriterTest::TearDown();
    settings_split_disable_feature_list_.Reset();
  }

 private:
  base::test::ScopedFeatureList settings_split_disable_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         EventRewriterInputSettingsSplitDisabledTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

TEST_P(EventRewriterInputSettingsSplitDisabledTest,
       TestRewriteCommandToControl) {
  // First, test non Apple keyboards, they should all behave the same.
  for (const auto& keyboard : kNonAppleKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    // VKEY_A, Alt modifier.
    EXPECT_EQ(KeyA::Typed(ui::EF_ALT_DOWN),
              RunRewriter(KeyA::Typed(), ui::EF_ALT_DOWN));

    // VKEY_A, Win modifier.
    EXPECT_EQ(KeyA::Typed(ui::EF_COMMAND_DOWN),
              RunRewriter(KeyA::Typed(), ui::EF_COMMAND_DOWN));

    // VKEY_A, Alt+Win modifier.
    EXPECT_EQ(
        KeyA::Typed(ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN),
        RunRewriter(KeyA::Typed(), ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN));

    // VKEY_LWIN (left Windows key), Alt modifier.
    EXPECT_EQ(KeyLMeta::Typed(ui::EF_ALT_DOWN),
              RunRewriter(KeyLMeta::Typed(), ui::EF_ALT_DOWN));

    // VKEY_RWIN (right Windows key), Alt modifier.
    EXPECT_EQ(KeyRMeta::Typed(ui::EF_ALT_DOWN),
              RunRewriter(KeyRMeta::Typed(), ui::EF_ALT_DOWN));
  }

  // Simulate the default initialization of the Apple Command key remap pref to
  // Ctrl.
  Preferences::RegisterProfilePrefs(prefs()->registry());
  {
    SCOPED_TRACE(kExternalAppleKeyboard.name);
    SetUpKeyboard(kExternalAppleKeyboard);

    // VKEY_A, Alt modifier.
    EXPECT_EQ(KeyA::Typed(ui::EF_ALT_DOWN),
              RunRewriter(KeyA::Typed(), ui::EF_ALT_DOWN));

    // VKEY_A, Win modifier.
    EXPECT_EQ(KeyA::Typed(ui::EF_CONTROL_DOWN),
              RunRewriter(KeyA::Typed(), ui::EF_COMMAND_DOWN));

    // VKEY_A, Alt+Win modifier.
    EXPECT_EQ(
        KeyA::Typed(ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN),
        RunRewriter(KeyA::Typed(), ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN));

    // VKEY_LWIN (left Windows key), Alt modifier.
    EXPECT_EQ(KeyLControl::Typed(ui::EF_ALT_DOWN),
              RunRewriter(KeyLMeta::Typed(), ui::EF_ALT_DOWN));

    // VKEY_RWIN (right Windows key), Alt modifier.
    EXPECT_EQ(KeyRControl::Typed(ui::EF_ALT_DOWN),
              RunRewriter(KeyRMeta::Typed(), ui::EF_ALT_DOWN));
  }

  // Now simulate the user remapped the Command key back to Search.
  IntegerPrefMember command;
  InitModifierKeyPref(&command, ::prefs::kLanguageRemapExternalCommandKeyTo,
                      ui::mojom::ModifierKey::kMeta,
                      ui::mojom::ModifierKey::kMeta);
  {
    SCOPED_TRACE(kExternalAppleKeyboard.name);
    SetUpKeyboard(kExternalAppleKeyboard);

    // VKEY_A, Alt modifier.
    EXPECT_EQ(KeyA::Typed(ui::EF_ALT_DOWN),
              RunRewriter(KeyA::Typed(), ui::EF_ALT_DOWN));

    // VKEY_A, Win modifier.
    EXPECT_EQ(KeyA::Typed(ui::EF_COMMAND_DOWN),
              RunRewriter(KeyA::Typed(), ui::EF_COMMAND_DOWN));

    // VKEY_A, Alt+Win modifier.
    EXPECT_EQ(
        KeyA::Typed(ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN),
        RunRewriter(KeyA::Typed(), ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN));

    // VKEY_LWIN (left Windows key), Alt modifier.
    EXPECT_EQ(KeyLMeta::Typed(ui::EF_ALT_DOWN),
              RunRewriter(KeyLMeta::Typed(), ui::EF_ALT_DOWN));

    // VKEY_RWIN (right Windows key), Alt modifier.
    EXPECT_EQ(KeyRMeta::Typed(ui::EF_ALT_DOWN),
              RunRewriter(KeyRMeta::Typed(), ui::EF_ALT_DOWN));
  }
}

TEST_P(EventRewriterInputSettingsSplitDisabledTest,
       TestRewriteExternalMetaKey) {
  // Simulate the default initialization of the Meta key on external keyboards
  // remap pref to Search.
  Preferences::RegisterProfilePrefs(prefs()->registry());

  // By default, the Meta key on all keyboards, internal, external Chrome OS
  // branded keyboards, and Generic keyboards should produce Search.
  for (const auto& keyboard : kNonAppleKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    // VKEY_A, Win modifier.
    EXPECT_EQ(KeyA::Typed(ui::EF_COMMAND_DOWN),
              RunRewriter(KeyA::Typed(), ui::EF_COMMAND_DOWN));

    // VKEY_A, Alt+Win modifier.
    EXPECT_EQ(
        KeyA::Typed(ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN),
        RunRewriter(KeyA::Typed(), ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN));

    // VKEY_LWIN (left Windows key), Alt modifier.
    EXPECT_EQ(KeyLMeta::Typed(ui::EF_ALT_DOWN),
              RunRewriter(KeyLMeta::Typed(), ui::EF_ALT_DOWN));

    // VKEY_RWIN (right Windows key), Alt modifier.
    EXPECT_EQ(KeyRMeta::Typed(ui::EF_ALT_DOWN),
              RunRewriter(KeyRMeta::Typed(), ui::EF_ALT_DOWN));
  }

  // Both preferences for Search on Chrome keyboards, and external Meta on
  // generic external keyboards are independent, even if one or both are
  // modified.

  // Remap Chrome OS Search to Ctrl.
  IntegerPrefMember internal_search;
  InitModifierKeyPref(&internal_search, ::prefs::kLanguageRemapSearchKeyTo,
                      ui::mojom::ModifierKey::kMeta,
                      ui::mojom::ModifierKey::kControl);

  // Remap external Meta to Alt.
  IntegerPrefMember meta;
  InitModifierKeyPref(&meta, ::prefs::kLanguageRemapExternalMetaKeyTo,
                      ui::mojom::ModifierKey::kMeta,
                      ui::mojom::ModifierKey::kAlt);
  for (const auto& keyboard : kChromeKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    // VKEY_A, Win modifier.
    EXPECT_EQ(KeyA::Typed(ui::EF_CONTROL_DOWN),
              RunRewriter(KeyA::Typed(), ui::EF_COMMAND_DOWN));

    // VKEY_A, Alt+Win modifier.
    EXPECT_EQ(
        KeyA::Typed(ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN),
        RunRewriter(KeyA::Typed(), ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN));

    // VKEY_LWIN (left Windows key), Alt modifier.
    EXPECT_EQ(KeyLControl::Typed(ui::EF_ALT_DOWN),
              RunRewriter(KeyLMeta::Typed(), ui::EF_ALT_DOWN));

    // VKEY_RWIN (right Windows key), Alt modifier.
    EXPECT_EQ(KeyRControl::Typed(ui::EF_ALT_DOWN),
              RunRewriter(KeyRMeta::Typed(), ui::EF_ALT_DOWN));
  }

  SetUpKeyboard(kExternalGenericKeyboard);

  // VKEY_A, Win modifier.
  EXPECT_EQ(KeyA::Typed(ui::EF_ALT_DOWN),
            RunRewriter(KeyA::Typed(), ui::EF_COMMAND_DOWN));

  // VKEY_A, Alt+Win modifier.
  EXPECT_EQ(KeyA::Typed(ui::EF_ALT_DOWN),
            RunRewriter(KeyA::Typed(), ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN));

  if (ash::features::IsKeyboardRewriterFixEnabled()) {
    // VKEY_LWIN (left Windows key), Alt modifier.
    EXPECT_EQ(KeyLAlt::Typed(ui::EF_ALT_DOWN),
              RunRewriter(KeyLMeta::Typed(), ui::EF_ALT_DOWN));
    // VKEY_RWIN (right Windows key), Alt modifier.
    EXPECT_EQ(KeyRAlt::Typed(ui::EF_ALT_DOWN),
              RunRewriter(KeyRMeta::Typed(), ui::EF_ALT_DOWN));
  } else {
    // VKEY_LWIN (left Windows key), Alt modifier.
    // Older implementation has an issue that release event is not dispatched.
    EXPECT_EQ(std::vector({KeyLAlt::Pressed()}),
              RunRewriter(KeyLMeta::Typed(), ui::EF_ALT_DOWN));
    // VKEY_RWIN (right Windows key), Alt modifier.
    EXPECT_EQ(KeyRAlt::Typed(),
              RunRewriter(KeyRMeta::Typed(), ui::EF_ALT_DOWN));
  }
}

// For crbug.com/133896.
TEST_P(EventRewriterInputSettingsSplitDisabledTest,
       TestRewriteCommandToControlWithControlRemapped) {
  // Remap Control to Alt.
  Preferences::RegisterProfilePrefs(prefs()->registry());
  IntegerPrefMember control;
  InitModifierKeyPref(&control, ::prefs::kLanguageRemapControlKeyTo,
                      ui::mojom::ModifierKey::kControl,
                      ui::mojom::ModifierKey::kAlt);

  for (const auto& keyboard : kNonAppleKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    EXPECT_EQ(KeyLAlt::Typed(), RunRewriter(KeyLControl::Typed()));
  }

  // Now verify that remapping does not affect Apple keyboard.
  SetUpKeyboard(kExternalAppleKeyboard);

  // VKEY_LWIN (left Command key) with  Alt modifier. The remapped Command
  // key should never be re-remapped to Alt.
  EXPECT_EQ(KeyLControl::Typed(ui::EF_ALT_DOWN),
            RunRewriter(KeyLMeta::Typed(), ui::EF_ALT_DOWN));

  // VKEY_RWIN (right Command key) with  Alt modifier. The remapped Command
  // key should never be re-remapped to Alt.
  EXPECT_EQ(KeyRControl::Typed(ui::EF_ALT_DOWN),
            RunRewriter(KeyRMeta::Typed(), ui::EF_ALT_DOWN));
}

class StickyKeysOverlayTest
    : public EventRewriterTestBase,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  StickyKeysOverlayTest() : overlay_(nullptr) {}

  ~StickyKeysOverlayTest() override {}

  void SetUp() override {
    auto [enable_keyboard_rewriter_fix, enable_modifier_split] = GetParam();
    if (enable_keyboard_rewriter_fix) {
      fix_feature_list_.InitAndEnableFeature(
          ash::features::kEnableKeyboardRewriterFix);
    } else {
      fix_feature_list_.InitAndDisableFeature(
          ash::features::kEnableKeyboardRewriterFix);
    }

    if (enable_modifier_split) {
      modifier_split_feature_list_.InitAndEnableFeature(
          ash::features::kModifierSplit);
    } else {
      modifier_split_feature_list_.InitAndDisableFeature(
          ash::features::kModifierSplit);
    }

    EventRewriterTestBase::SetUp();
    auto* sticky_keys_controller = Shell::Get()->sticky_keys_controller();
    sticky_keys_controller->Enable(true);
    overlay_ = sticky_keys_controller->GetOverlayForTest();
    ASSERT_TRUE(overlay_);
  }

  raw_ptr<StickyKeysOverlay, DanglingUntriaged> overlay_;

 private:
  base::test::ScopedFeatureList fix_feature_list_;
  base::test::ScopedFeatureList modifier_split_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         StickyKeysOverlayTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

TEST_P(StickyKeysOverlayTest, OneModifierEnabled) {
  EXPECT_FALSE(overlay_->is_visible());
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));

  // Pressing modifier key should show overlay.
  SendKeyEvents(KeyLControl::Typed());
  EXPECT_TRUE(overlay_->is_visible());
  EXPECT_EQ(STICKY_KEY_STATE_ENABLED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));

  // Pressing a normal key should hide overlay.
  SendKeyEvents(KeyT::Typed());
  EXPECT_FALSE(overlay_->is_visible());
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));
}

TEST_P(StickyKeysOverlayTest, TwoModifiersEnabled) {
  EXPECT_FALSE(overlay_->is_visible());
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_SHIFT_DOWN));

  // Pressing two modifiers should show overlay.
  SendKeyEvents(KeyLShift::Typed());
  SendKeyEvents(KeyLControl::Typed());
  EXPECT_TRUE(overlay_->is_visible());
  EXPECT_EQ(STICKY_KEY_STATE_ENABLED,
            overlay_->GetModifierKeyState(ui::EF_SHIFT_DOWN));
  EXPECT_EQ(STICKY_KEY_STATE_ENABLED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));

  // Pressing a normal key should hide overlay.
  SendKeyEvents(KeyN::Typed());
  EXPECT_FALSE(overlay_->is_visible());
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_SHIFT_DOWN));
}

TEST_P(StickyKeysOverlayTest, LockedModifier) {
  EXPECT_FALSE(overlay_->is_visible());
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_ALT_DOWN));

  // Pressing a modifier key twice should lock modifier and show overlay.
  SendKeyEvents(KeyLAlt::Typed());
  SendKeyEvents(KeyLAlt::Typed());
  EXPECT_TRUE(overlay_->is_visible());
  EXPECT_EQ(STICKY_KEY_STATE_LOCKED,
            overlay_->GetModifierKeyState(ui::EF_ALT_DOWN));

  // Pressing a normal key should not hide overlay.
  SendKeyEvents(KeyD::Typed());
  EXPECT_TRUE(overlay_->is_visible());
  EXPECT_EQ(STICKY_KEY_STATE_LOCKED,
            overlay_->GetModifierKeyState(ui::EF_ALT_DOWN));
}

TEST_P(StickyKeysOverlayTest, LockedAndNormalModifier) {
  EXPECT_FALSE(overlay_->is_visible());
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_SHIFT_DOWN));

  // Pressing a modifier key twice should lock modifier and show overlay.
  SendKeyEvents(KeyLControl::Typed());
  SendKeyEvents(KeyLControl::Typed());
  EXPECT_TRUE(overlay_->is_visible());
  EXPECT_EQ(STICKY_KEY_STATE_LOCKED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));

  // Pressing another modifier key should still show overlay.
  SendKeyEvents(KeyLShift::Typed());
  EXPECT_TRUE(overlay_->is_visible());
  EXPECT_EQ(STICKY_KEY_STATE_LOCKED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));
  EXPECT_EQ(STICKY_KEY_STATE_ENABLED,
            overlay_->GetModifierKeyState(ui::EF_SHIFT_DOWN));

  // Pressing a normal key should not hide overlay but disable normal modifier.
  SendKeyEvents(KeyD::Typed());
  EXPECT_TRUE(overlay_->is_visible());
  EXPECT_EQ(STICKY_KEY_STATE_LOCKED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_SHIFT_DOWN));
}

TEST_P(StickyKeysOverlayTest, ModifiersDisabled) {
  EXPECT_FALSE(overlay_->is_visible());
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_SHIFT_DOWN));
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_ALT_DOWN));
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_COMMAND_DOWN));

  // Enable modifiers.
  SendKeyEvents(KeyLControl::Typed());
  SendKeyEvents(KeyLShift::Typed());
  SendKeyEvents(KeyLShift::Typed());
  SendKeyEvents(KeyLAlt::Typed());
  SendKeyEvents(KeyLMeta::Typed());
  SendKeyEvents(KeyLMeta::Typed());

  EXPECT_TRUE(overlay_->is_visible());
  EXPECT_EQ(STICKY_KEY_STATE_ENABLED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));
  EXPECT_EQ(STICKY_KEY_STATE_LOCKED,
            overlay_->GetModifierKeyState(ui::EF_SHIFT_DOWN));
  EXPECT_EQ(STICKY_KEY_STATE_ENABLED,
            overlay_->GetModifierKeyState(ui::EF_ALT_DOWN));
  EXPECT_EQ(STICKY_KEY_STATE_LOCKED,
            overlay_->GetModifierKeyState(ui::EF_COMMAND_DOWN));

  // Disable modifiers and overlay should be hidden.
  SendKeyEvents(KeyLControl::Typed());
  SendKeyEvents(KeyLControl::Typed());
  SendKeyEvents(KeyLShift::Typed());
  SendKeyEvents(KeyLAlt::Typed());
  SendKeyEvents(KeyLAlt::Typed());
  SendKeyEvents(KeyLMeta::Typed());

  EXPECT_FALSE(overlay_->is_visible());
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_SHIFT_DOWN));
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_ALT_DOWN));
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_COMMAND_DOWN));
}

TEST_P(StickyKeysOverlayTest, ModifierVisibility) {
  // All but AltGr and Mod3 should initially be visible.
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_CONTROL_DOWN));
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_SHIFT_DOWN));
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_ALT_DOWN));
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_COMMAND_DOWN));
  EXPECT_FALSE(overlay_->GetModifierVisible(ui::EF_ALTGR_DOWN));
  EXPECT_FALSE(overlay_->GetModifierVisible(ui::EF_MOD3_DOWN));
  EXPECT_FALSE(overlay_->GetModifierVisible(ui::EF_FUNCTION_DOWN));

  // Turn all modifiers on.
  auto* sticky_keys_controller = Shell::Get()->sticky_keys_controller();
  sticky_keys_controller->SetMod3AndAltGrModifiersEnabled(true, true);
  sticky_keys_controller->SetFnModifierEnabled(true);
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_CONTROL_DOWN));
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_SHIFT_DOWN));
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_ALT_DOWN));
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_COMMAND_DOWN));
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_ALTGR_DOWN));
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_MOD3_DOWN));
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_FUNCTION_DOWN));

  // Turn off Fn.
  sticky_keys_controller->SetFnModifierEnabled(false);
  EXPECT_FALSE(overlay_->GetModifierVisible(ui::EF_FUNCTION_DOWN));

  // Turn off Mod3.
  sticky_keys_controller->SetMod3AndAltGrModifiersEnabled(false, true);
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_ALTGR_DOWN));
  EXPECT_FALSE(overlay_->GetModifierVisible(ui::EF_MOD3_DOWN));

  // Turn off AltGr.
  sticky_keys_controller->SetMod3AndAltGrModifiersEnabled(true, false);
  EXPECT_FALSE(overlay_->GetModifierVisible(ui::EF_ALTGR_DOWN));
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_MOD3_DOWN));

  // Turn off AltGr and Mod3.
  sticky_keys_controller->SetMod3AndAltGrModifiersEnabled(false, false);
  EXPECT_FALSE(overlay_->GetModifierVisible(ui::EF_ALTGR_DOWN));
  EXPECT_FALSE(overlay_->GetModifierVisible(ui::EF_MOD3_DOWN));
}

TEST_P(EventRewriterTest, RewrittenModifier) {
  Preferences::RegisterProfilePrefs(prefs()->registry());

  // Register Control + B as an extension shortcut.
  SetExtensionCommands({{{ui::VKEY_B, ui::EF_CONTROL_DOWN}}});

  // Check that standard extension input has no rewritten modifiers.
  EXPECT_EQ(KeyB::Typed(ui::EF_CONTROL_DOWN),
            RunRewriter(KeyB::Typed(), ui::EF_CONTROL_DOWN));

  // Remap Control -> Alt.
  IntegerPrefMember control;
  InitModifierKeyPref(&control, ::prefs::kLanguageRemapControlKeyTo,
                      ui::mojom::ModifierKey::kControl,
                      ui::mojom::ModifierKey::kAlt);
  // Pressing Control + B should now be remapped to Alt + B.
  EXPECT_EQ(KeyB::Typed(ui::EF_ALT_DOWN),
            RunRewriter(KeyB::Typed(), ui::EF_CONTROL_DOWN));

  // Remap Alt -> Control.
  IntegerPrefMember alt;
  InitModifierKeyPref(&alt, ::prefs::kLanguageRemapAltKeyTo,
                      ui::mojom::ModifierKey::kAlt,
                      ui::mojom::ModifierKey::kControl);
  // Pressing Alt + B should now be remapped to Control + B.
  EXPECT_EQ(KeyB::Typed(ui::EF_CONTROL_DOWN),
            RunRewriter(KeyB::Typed(), ui::EF_ALT_DOWN));

  // Remove all extension shortcuts and still expect the remapping to work.
  SetExtensionCommands(std::nullopt);

  EXPECT_EQ(KeyB::Typed(ui::EF_ALT_DOWN),
            RunRewriter(KeyB::Typed(), ui::EF_CONTROL_DOWN));
  EXPECT_EQ(KeyB::Typed(ui::EF_CONTROL_DOWN),
            RunRewriter(KeyB::Typed(), ui::EF_ALT_DOWN));
}

TEST_P(EventRewriterTest, RewriteNumpadExtensionCommand) {
  // Register Control + NUMPAD1 as an extension shortcut.
  SetExtensionCommands({{{ui::VKEY_NUMPAD1, ui::EF_CONTROL_DOWN}}});
  // Check that extension shortcuts that involve numpads keys are properly
  // rewritten. Note that VKEY_END is associated with NUMPAD1 if Num Lock is
  // disabled. The result should be "NumPad 1 with Control".
  EXPECT_EQ(KeyNumpad1::Typed(ui::EF_CONTROL_DOWN),
            RunRewriter(KeyNumpadEnd::Typed(), ui::EF_CONTROL_DOWN));

  // Remove the extension shortcut and expect the numpad event to still be
  // rewritten.
  SetExtensionCommands(std::nullopt);
  EXPECT_EQ(KeyNumpad1::Typed(ui::EF_CONTROL_DOWN),
            RunRewriter(KeyNumpadEnd::Typed(), ui::EF_CONTROL_DOWN));
}

TEST_P(EventRewriterTest, RecordRewritingToFunctionKeys) {
  Preferences::RegisterProfilePrefs(prefs()->registry());
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kInputDeviceSettingsSplit},
      {::features::kImprovedKeyboardShortcuts});

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount("ChromeOS.Inputs.Keyboard.F1Pressed", 0);

  // Search + back -> F1.
  SetUpKeyboard(kWilco1_0Keyboard);
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Inputs.Keyboard.F1Pressed",
      /*SearchTopRowTranslated*/
      static_cast<int>(ui::InputKeyEventToFunctionKey::kSearchTopRowTranslated),
      0);
  EXPECT_EQ(KeyF1::Typed(),
            RunRewriter(KeyBrowserBack::Typed(), ui::EF_COMMAND_DOWN));
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Inputs.Keyboard.F1Pressed",
      /*SearchTopRowTranslated*/
      static_cast<int>(ui::InputKeyEventToFunctionKey::kSearchTopRowTranslated),
      1u);
  histogram_tester.ExpectTotalCount("ChromeOS.Inputs.Keyboard.F1Pressed", 1u);

  mojom::KeyboardSettings settings;
  EXPECT_CALL(*input_device_settings_controller_mock_,
              GetKeyboardSettings(kKeyboardDeviceId))
      .WillRepeatedly(testing::Return(&settings));

  // Back + Search -> F1 + Search. Back was automatically translated to F1,
  // with search key unaffected so it should be mapped to
  // kTopRowAutoTranslated.
  settings.top_row_are_fkeys = true;
  settings.suppress_meta_fkey_rewrites = true;
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Inputs.Keyboard.F1Pressed",
      /*TopRowAutoTranslated*/
      static_cast<int>(ui::InputKeyEventToFunctionKey::kTopRowAutoTranslated),
      0);
  EXPECT_EQ(KeyF1::Typed(ui::EF_COMMAND_DOWN),
            RunRewriter(KeyBrowserBack::Typed(), ui::EF_COMMAND_DOWN));
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Inputs.Keyboard.F1Pressed",
      /*TopRowAutoTranslated*/
      static_cast<int>(ui::InputKeyEventToFunctionKey::kTopRowAutoTranslated),
      1u);
  histogram_tester.ExpectTotalCount("ChromeOS.Inputs.Keyboard.F1Pressed", 2u);

  SetUpKeyboard(kExternalGenericKeyboard);

  // F1 + search -> F1.
  settings.top_row_are_fkeys = false;
  settings.suppress_meta_fkey_rewrites = false;

  histogram_tester.ExpectBucketCount(
      "ChromeOS.Inputs.Keyboard.F1Pressed",
      /*DirectlyWithSearch*/
      static_cast<int>(ui::InputKeyEventToFunctionKey::kDirectlyWithSearch), 0);
  // The keyboard sends F1 + search and the result is F1 only without search so
  // it should be mapped to kDirectlyWithSearch.
  EXPECT_EQ(KeyF1::Typed(), RunRewriter(KeyF1::Typed(), ui::EF_COMMAND_DOWN));
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Inputs.Keyboard.F1Pressed",
      /*DirectlyWithSearch*/
      static_cast<int>(ui::InputKeyEventToFunctionKey::kDirectlyWithSearch),
      1u);
  histogram_tester.ExpectTotalCount("ChromeOS.Inputs.Keyboard.F1Pressed", 3u);

  // No change.
  settings.top_row_are_fkeys = true;
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Inputs.Keyboard.F1Pressed",
      /*DirectlyFromKeyboard*/
      static_cast<int>(ui::InputKeyEventToFunctionKey::kDirectlyFromKeyboard),
      0);
  EXPECT_EQ(KeyF1::Typed(), RunRewriter(KeyF1::Typed()));
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Inputs.Keyboard.F1Pressed",
      /*DirectlyFromKeyboard*/
      static_cast<int>(ui::InputKeyEventToFunctionKey::kDirectlyFromKeyboard),
      1u);
  histogram_tester.ExpectTotalCount("ChromeOS.Inputs.Keyboard.F1Pressed", 4u);

  // Search + number.
  SetUpKeyboard(kInternalChromeKeyboard);
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Inputs.Keyboard.F1Pressed",
      /*SearchDigitTranslated*/
      static_cast<int>(ui::InputKeyEventToFunctionKey::kSearchDigitTranslated),
      0);
  EXPECT_EQ(KeyF1::Typed(),
            RunRewriter(KeyDigit1::Typed(), ui::EF_COMMAND_DOWN));
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Inputs.Keyboard.F1Pressed",
      /*SearchDigitTranslated*/
      static_cast<int>(ui::InputKeyEventToFunctionKey::kSearchDigitTranslated),
      1u);
  histogram_tester.ExpectTotalCount("ChromeOS.Inputs.Keyboard.F1Pressed", 5u);

  // F1 + search to F1 + search
  settings.suppress_meta_fkey_rewrites = true;
  EXPECT_EQ(KeyF1::Typed(ui::EF_COMMAND_DOWN),
            RunRewriter(KeyF1::Typed(), ui::EF_COMMAND_DOWN));
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Inputs.Keyboard.F1Pressed",
      /*DirectlyFromKeyboard*/
      static_cast<int>(ui::InputKeyEventToFunctionKey::kDirectlyFromKeyboard),
      2u);
  histogram_tester.ExpectTotalCount("ChromeOS.Inputs.Keyboard.F1Pressed", 6u);
}

TEST_P(EventRewriterTest, AltgrLatch) {
  // TODO(b/331906341): Consider to use real latvian layout.
  keyboard_layout_engine_->SetCustomLookupTableForTesting({
      {ui::DomCode::QUOTE, ui::DomKey::ALT_GRAPH_LATCH,
       ui::DomKey::FromCharacter(u'"'), ui::VKEY_ALTGR},
  });

  // Use fake latvian quote key.
  using KeyLatvianQuote =
      TestKey<ui::DomCode::QUOTE, ui::DomKey::ALT_GRAPH_LATCH, ui::VKEY_ALTGR,
              ui::EF_ALTGR_DOWN>;
  EXPECT_EQ(std::vector<TestKeyEvent>(
                {{ui::EventType::kKeyPressed, ui::DomCode::QUOTE,
                  ui::DomKey::ALT_GRAPH, ui::VKEY_ALTGR, ui::EF_ALTGR_DOWN},
                 // EF_ALTGR_DOWN is still here, because it's latched.
                 {ui::EventType::kKeyReleased, ui::DomCode::QUOTE,
                  ui::DomKey::ALT_GRAPH, ui::VKEY_ALTGR, ui::EF_ALTGR_DOWN}}),
            SendKeyEvents(KeyLatvianQuote::Typed()));
  EXPECT_EQ(std::vector({KeyA::Pressed(ui::EF_ALTGR_DOWN), KeyA::Released()}),
            SendKeyEvents(KeyA::Typed()));

  // Hold the quote.
  EXPECT_EQ(std::vector<TestKeyEvent>(
                {{ui::EventType::kKeyPressed, ui::DomCode::QUOTE,
                  ui::DomKey::ALT_GRAPH, ui::VKEY_ALTGR, ui::EF_ALTGR_DOWN}}),
            SendKeyEvent(KeyLatvianQuote::Pressed()));

  // Type A followed by C.
  EXPECT_EQ(KeyA::Typed(ui::EF_ALTGR_DOWN),
            SendKeyEvents(KeyA::Typed(ui::EF_ALTGR_DOWN)));
  EXPECT_EQ(KeyC::Typed(ui::EF_ALTGR_DOWN),
            SendKeyEvents(KeyC::Typed(ui::EF_ALTGR_DOWN)));

  // Release the quote, where EF_ALTGR_DOWN should not be set.
  EXPECT_EQ(std::vector<TestKeyEvent>(
                {{ui::EventType::kKeyReleased, ui::DomCode::QUOTE,
                  ui::DomKey::ALT_GRAPH, ui::VKEY_ALTGR}}),
            SendKeyEvent(KeyLatvianQuote::Released()));
}

TEST_P(EventRewriterTest, SixPackRemappingsFnBased) {
  if (!features::IsModifierSplitEnabled()) {
    GTEST_SKIP() << "Test is only valid with the modifier split flag enabled";
  }

  SetUpKeyboard(kInternalChromeSplitModifierLayoutKeyboard);

  // Test each case while applying additional flags to confirm flags get
  // properly applied to rewritten events.
  for (const auto flag : {ui::EF_NONE, ui::EF_COMMAND_DOWN, ui::EF_CONTROL_DOWN,
                          ui::EF_ALT_DOWN, ui::EF_SHIFT_DOWN}) {
    EXPECT_EQ(KeyDelete::Typed(flag),
              RunRewriter(KeyBackspace::Typed(), ui::EF_FUNCTION_DOWN | flag));
    EXPECT_EQ(KeyHome::Typed(flag),
              RunRewriter(KeyArrowLeft::Typed(), ui::EF_FUNCTION_DOWN | flag));
    EXPECT_EQ(KeyEnd::Typed(flag),
              RunRewriter(KeyArrowRight::Typed(), ui::EF_FUNCTION_DOWN | flag));
    EXPECT_EQ(KeyPageUp::Typed(flag),
              RunRewriter(KeyArrowUp::Typed(), ui::EF_FUNCTION_DOWN | flag));
    EXPECT_EQ(KeyPageDown::Typed(flag),
              RunRewriter(KeyArrowDown::Typed(), ui::EF_FUNCTION_DOWN | flag));
  }
}

TEST_P(EventRewriterTest, NotifyShortcutEventRewriteBlockedByFnKey) {
  if (!features::IsModifierSplitEnabled()) {
    GTEST_SKIP() << "Test is only valid with the modifier split flag enabled";
  }

  AnchoredNudgeManagerImpl* nudge_manager =
      Shell::Get()->anchored_nudge_manager();
  ASSERT_TRUE(nudge_manager);
  SetUpKeyboard(kInternalChromeSplitModifierLayoutKeyboard);
  EXPECT_EQ(KeyArrowLeft::Typed(ui::EF_COMMAND_DOWN),
            RunRewriter(KeyArrowLeft::Typed(), ui::EF_COMMAND_DOWN));

  EXPECT_TRUE(nudge_manager->GetNudgeIfShown(kSixPackKeyNoMatchNudgeId));
  nudge_manager->Cancel(kSixPackKeyNoMatchNudgeId);

  // Set the scan code so the key event is recognized as top row key.
  std::vector<TestKeyEvent> key_events;
  for (auto event : KeyF1::Typed()) {
    event.scan_code = 1;
    key_events.push_back(std::move(event));
  }

  std::vector<TestKeyEvent> expected_events;
  for (auto event : KeyF1::Typed(ui::EF_COMMAND_DOWN)) {
    event.scan_code = 1;
    expected_events.push_back(std::move(event));
  }

  EXPECT_EQ(expected_events, RunRewriter(key_events, ui::EF_COMMAND_DOWN));
  EXPECT_TRUE(nudge_manager->GetNudgeIfShown(kTopRowKeyNoMatchNudgeId));
  nudge_manager->Cancel(kTopRowKeyNoMatchNudgeId);
}

TEST_P(EventRewriterTest, CapsLockRemappingFnBased) {
  if (!features::IsModifierSplitEnabled()) {
    GTEST_SKIP() << "Test is only valid with the modifier split flag enabled";
  }

  SetUpKeyboard(kInternalChromeSplitModifierLayoutKeyboard);

  for (const auto flag :
       {ui::EF_NONE, ui::EF_CONTROL_DOWN, ui::EF_SHIFT_DOWN, ui::EF_ALT_DOWN}) {
    EXPECT_EQ(KeyCapsLock::Typed(flag | ui::EF_CAPS_LOCK_ON),
              RunRewriter(KeyRightAlt::Typed(ui::EF_NONE, {kPropertyRightAlt}),
                          ui::EF_FUNCTION_DOWN | flag));
    EXPECT_TRUE(fake_ime_keyboard_.IsCapsLockEnabled());

    EXPECT_EQ(KeyCapsLock::Typed(flag),
              RunRewriter(
                  KeyRightAlt::Typed(ui::EF_CAPS_LOCK_ON, {kPropertyRightAlt}),
                  ui::EF_FUNCTION_DOWN | flag));
    EXPECT_FALSE(fake_ime_keyboard_.IsCapsLockEnabled());
  }
}

TEST_P(EventRewriterTest, FnDiscarded) {
  if (!features::IsModifierSplitEnabled()) {
    GTEST_SKIP() << "Test is only valid with the modifier split flag enabled";
  }

  SetUpKeyboard(kInternalChromeSplitModifierLayoutKeyboard);

  EXPECT_EQ(KeyA::Typed(), RunRewriter(KeyA::Typed(), ui::EF_FUNCTION_DOWN));
  EXPECT_EQ(
      KeyA::Typed(ui::EF_CONTROL_DOWN),
      RunRewriter(KeyA::Typed(), ui::EF_FUNCTION_DOWN | ui::EF_CONTROL_DOWN));

  EXPECT_EQ(std::vector<TestKeyEvent>(), RunRewriter(KeyFunction::Typed()));
}

// Tests that when you press Fn -> Right Alt -> Release Fn -> Release Right Alt
// that the release of right alt is remapped to CapsLock to match the remapped
// press.
TEST_P(EventRewriterTest, CapsLockRemappingFnBasedReleaseOrdering) {
  if (!features::IsModifierSplitEnabled()) {
    GTEST_SKIP() << "Test is only valid with the modifier split flag enabled";
  }

  SetUpKeyboard(kInternalChromeSplitModifierLayoutKeyboard);

  EXPECT_EQ(std::vector<TestKeyEvent>(),
            RunRewriter(std::vector<TestKeyEvent>{KeyFunction::Pressed()}));
  EXPECT_EQ(
      std::vector<TestKeyEvent>({KeyCapsLock::Pressed(ui::EF_CAPS_LOCK_ON)}),
      RunRewriter(std::vector<TestKeyEvent>{
          KeyRightAlt::Pressed(ui::EF_FUNCTION_DOWN, {kPropertyRightAlt})}));
  EXPECT_EQ(std::vector<TestKeyEvent>(),
            RunRewriter(std::vector<TestKeyEvent>{
                KeyFunction::Released(ui::EF_CAPS_LOCK_ON)}));
  EXPECT_EQ(
      std::vector<TestKeyEvent>({KeyCapsLock::Released(ui::EF_CAPS_LOCK_ON)}),
      RunRewriter(std::vector<TestKeyEvent>{
          KeyRightAlt::Released(ui::EF_CAPS_LOCK_ON, {kPropertyRightAlt})}));
}

class ModifierPressedMetricsTest
    : public EventRewriterTestBase,
      public testing::WithParamInterface<
          std::tuple<bool,
                     std::tuple<TestKeyEvent,
                                ui::ModifierKeyUsageMetric,
                                std::vector<std::string>>>> {
 public:
  void SetUp() override {
    bool fix_enabled;
    auto tuple = std::tie(event_, modifier_key_usage_mapping_, key_pref_names_);
    std::tie(fix_enabled, tuple) = GetParam();
    std::vector<base::test::FeatureRef> enabled_features, disabled_features;
    (fix_enabled ? enabled_features : disabled_features)
        .push_back(ash::features::kEnableKeyboardRewriterFix);
    disabled_features.push_back(features::kInputDeviceSettingsSplit);
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
    EventRewriterTestBase::SetUp();
  }

 protected:
  TestKeyEvent event_;
  ui::ModifierKeyUsageMetric modifier_key_usage_mapping_;
  std::vector<std::string> key_pref_names_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ModifierPressedMetricsTest,
    testing::Combine(
        testing::Bool(),
        testing::ValuesIn(std::vector<std::tuple<TestKeyEvent,
                                                 ui::ModifierKeyUsageMetric,
                                                 std::vector<std::string>>>{
            {KeyLMeta::Pressed(),
             ui::ModifierKeyUsageMetric::kMetaLeft,
             {::prefs::kLanguageRemapSearchKeyTo,
              ::prefs::kLanguageRemapExternalCommandKeyTo,
              ::prefs::kLanguageRemapExternalMetaKeyTo}},
            {KeyRMeta::Pressed(),
             ui::ModifierKeyUsageMetric::kMetaRight,
             {::prefs::kLanguageRemapSearchKeyTo,
              ::prefs::kLanguageRemapExternalCommandKeyTo,
              ::prefs::kLanguageRemapExternalMetaKeyTo}},
            {KeyLControl::Pressed(),
             ui::ModifierKeyUsageMetric::kControlLeft,
             {::prefs::kLanguageRemapControlKeyTo}},
            {KeyRControl::Pressed(),
             ui::ModifierKeyUsageMetric::kControlRight,
             {::prefs::kLanguageRemapControlKeyTo}},
            {KeyLAlt::Pressed(),
             ui::ModifierKeyUsageMetric::kAltLeft,
             {::prefs::kLanguageRemapAltKeyTo}},
            {KeyRAlt::Pressed(),
             ui::ModifierKeyUsageMetric::kAltRight,
             {::prefs::kLanguageRemapAltKeyTo}},
            {KeyLShift::Pressed(),
             ui::ModifierKeyUsageMetric::kShiftLeft,
             // Shift keys cannot be remapped and therefore do not have a real
             // "pref" path.
             {"fakePrefPath"}},
            {KeyRShift::Pressed(),
             ui::ModifierKeyUsageMetric::kShiftRight,
             // Shift keys cannot be remapped and therefore do not have a real
             // "pref" path.
             {"fakePrefPath"}},
            {KeyCapsLock::Pressed(),
             ui::ModifierKeyUsageMetric::kCapsLock,
             {::prefs::kLanguageRemapCapsLockKeyTo}},
            {KeyBackspace::Pressed(),
             ui::ModifierKeyUsageMetric::kBackspace,
             {::prefs::kLanguageRemapBackspaceKeyTo}},
            {KeyEscape::Pressed(),
             ui::ModifierKeyUsageMetric::kEscape,
             {::prefs::kLanguageRemapEscapeKeyTo}},
            {KeyLaunchAssistant::Pressed(),
             ui::ModifierKeyUsageMetric::kAssistant,
             {::prefs::kLanguageRemapAssistantKeyTo}}})));

TEST_P(ModifierPressedMetricsTest, KeyPressedTest) {
  auto expected = event_;
  if (expected.code == ui::DomCode::CAPS_LOCK) {
    expected.flags |= ui::EF_CAPS_LOCK_ON;
  }

  base::HistogramTester histogram_tester;
  SetUpKeyboard(kInternalChromeKeyboard);
  EXPECT_EQ(std::vector({expected}), SendKeyEvent(event_));
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.ModifierPressed.Internal",
      modifier_key_usage_mapping_, 1);
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.RemappedModifierPressed.Internal",
      modifier_key_usage_mapping_, 1);
  // Unset CapsLock for each press.
  SendKeyEvent(TestKeyEvent{ui::EventType::kKeyReleased, event_.code,
                            event_.key, event_.keycode, event_.flags,
                            event_.scan_code});
  fake_ime_keyboard_.SetCapsLockEnabled(false);

  SetUpKeyboard(kExternalChromeKeyboard);
  EXPECT_EQ(std::vector({expected}), SendKeyEvent(event_));
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.ModifierPressed.CrOSExternal",
      modifier_key_usage_mapping_, 1);
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.RemappedModifierPressed.CrOSExternal",
      modifier_key_usage_mapping_, 1);
  SendKeyEvent(TestKeyEvent{ui::EventType::kKeyReleased, event_.code,
                            event_.key, event_.keycode, event_.flags,
                            event_.scan_code});
  fake_ime_keyboard_.SetCapsLockEnabled(false);

  SetUpKeyboard(kExternalAppleKeyboard);
  EXPECT_EQ(std::vector({expected}), SendKeyEvent(event_));
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.ModifierPressed.AppleExternal",
      modifier_key_usage_mapping_, 1);
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.RemappedModifierPressed.AppleExternal",
      modifier_key_usage_mapping_, 1);
  SendKeyEvent(TestKeyEvent{ui::EventType::kKeyReleased, event_.code,
                            event_.key, event_.keycode, event_.flags,
                            event_.scan_code});
  fake_ime_keyboard_.SetCapsLockEnabled(false);

  SetUpKeyboard(kExternalGenericKeyboard);
  EXPECT_EQ(std::vector({expected}), SendKeyEvent(event_));
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.ModifierPressed.External",
      modifier_key_usage_mapping_, 1);
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.RemappedModifierPressed.External",
      modifier_key_usage_mapping_, 1);
  SendKeyEvent(TestKeyEvent{ui::EventType::kKeyReleased, event_.code,
                            event_.key, event_.keycode, event_.flags,
                            event_.scan_code});
  fake_ime_keyboard_.SetCapsLockEnabled(false);
}

TEST_P(ModifierPressedMetricsTest, KeyPressedWithRemappingToBackspaceTest) {
  if (event_.keycode == ui::VKEY_SHIFT) {
    GTEST_SKIP() << "Shift cannot be remapped";
  }

  Preferences::RegisterProfilePrefs(prefs()->registry());
  base::HistogramTester histogram_tester;
  for (const auto& pref_name : key_pref_names_) {
    IntegerPrefMember pref_member;
    InitModifierKeyPref(&pref_member, pref_name,
                        ui::mojom::ModifierKey::kControl,
                        ui::mojom::ModifierKey::kBackspace);
  }

  SetUpKeyboard(kInternalChromeKeyboard);
  EXPECT_EQ(std::vector({KeyBackspace::Pressed()}), SendKeyEvent(event_));
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.ModifierPressed.Internal",
      modifier_key_usage_mapping_, 1);
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.RemappedModifierPressed.Internal",
      ui::ModifierKeyUsageMetric::kBackspace, 1);

  SetUpKeyboard(kExternalChromeKeyboard);
  EXPECT_EQ(std::vector({KeyBackspace::Pressed()}), SendKeyEvent(event_));
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.ModifierPressed.CrOSExternal",
      modifier_key_usage_mapping_, 1);
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.RemappedModifierPressed.CrOSExternal",
      ui::ModifierKeyUsageMetric::kBackspace, 1);

  SetUpKeyboard(kExternalAppleKeyboard);
  EXPECT_EQ(std::vector({KeyBackspace::Pressed()}), SendKeyEvent(event_));
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.ModifierPressed.AppleExternal",
      modifier_key_usage_mapping_, 1);
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.RemappedModifierPressed.AppleExternal",
      ui::ModifierKeyUsageMetric::kBackspace, 1);

  SetUpKeyboard(kExternalGenericKeyboard);
  EXPECT_EQ(std::vector({KeyBackspace::Pressed()}), SendKeyEvent(event_));
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.ModifierPressed.External",
      modifier_key_usage_mapping_, 1);
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.RemappedModifierPressed.External",
      ui::ModifierKeyUsageMetric::kBackspace, 1);
}

TEST_P(ModifierPressedMetricsTest, KeyPressedWithRemappingToControlTest) {
  if (event_.keycode == ui::VKEY_SHIFT) {
    GTEST_SKIP() << "Shift cannot be remapped";
  }

  Preferences::RegisterProfilePrefs(prefs()->registry());
  base::HistogramTester histogram_tester;

  const bool right = ui::KeycodeConverter::DomCodeToLocation(event_.code) ==
                     ui::DomKeyLocation::RIGHT;
  const ui::ModifierKeyUsageMetric remapped_modifier_key_usage_mapping =
      right ? ui::ModifierKeyUsageMetric::kControlRight
            : ui::ModifierKeyUsageMetric::kControlLeft;
  const auto control_event =
      right ? KeyRControl::Pressed() : KeyLControl::Pressed();

  for (const auto& pref_name : key_pref_names_) {
    IntegerPrefMember pref_member;
    InitModifierKeyPref(&pref_member, pref_name,
                        ui::mojom::ModifierKey::kControl,
                        ui::mojom::ModifierKey::kControl);
  }

  SetUpKeyboard(kInternalChromeKeyboard);
  EXPECT_EQ(std::vector({control_event}), SendKeyEvent(event_));
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.ModifierPressed.Internal",
      modifier_key_usage_mapping_, 1);
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.RemappedModifierPressed.Internal",
      remapped_modifier_key_usage_mapping, 1);

  SetUpKeyboard(kExternalChromeKeyboard);
  EXPECT_EQ(std::vector({control_event}), SendKeyEvent(event_));
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.ModifierPressed.CrOSExternal",
      modifier_key_usage_mapping_, 1);
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.RemappedModifierPressed.CrOSExternal",
      remapped_modifier_key_usage_mapping, 1);

  SetUpKeyboard(kExternalAppleKeyboard);
  EXPECT_EQ(std::vector({control_event}), SendKeyEvent(event_));
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.ModifierPressed.AppleExternal",
      modifier_key_usage_mapping_, 1);
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.RemappedModifierPressed.AppleExternal",
      remapped_modifier_key_usage_mapping, 1);

  SetUpKeyboard(kExternalGenericKeyboard);
  EXPECT_EQ(std::vector({control_event}), SendKeyEvent(event_));
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.ModifierPressed.External",
      modifier_key_usage_mapping_, 1);
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.RemappedModifierPressed.External",
      remapped_modifier_key_usage_mapping, 1);
}

TEST_P(ModifierPressedMetricsTest, KeyRepeatTest) {
  if (event_.code == ui::DomCode::CAPS_LOCK) {
    GTEST_SKIP() << "CapsLock Key will not be marked as EF_IS_REPEAT";
  }

  base::HistogramTester histogram_tester;
  // No metrics should be published if it is a repeated key.
  event_.flags |= ui::EF_IS_REPEAT;

  SetUpKeyboard(kInternalChromeKeyboard);
  EXPECT_EQ(std::vector({event_}), SendKeyEvent(event_));
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.ModifierPressed.Internal",
      modifier_key_usage_mapping_, 0);
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.RemappedModifierPressed.Internal",
      modifier_key_usage_mapping_, 0);

  SetUpKeyboard(kExternalChromeKeyboard);
  EXPECT_EQ(std::vector({event_}), SendKeyEvent(event_));
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.ModifierPressed.CrOSExternal",
      modifier_key_usage_mapping_, 0);
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.RemappedModifierPressed.CrOSExternal",
      modifier_key_usage_mapping_, 0);

  SetUpKeyboard(kExternalAppleKeyboard);
  EXPECT_EQ(std::vector({event_}), SendKeyEvent(event_));
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.ModifierPressed.AppleExternal",
      modifier_key_usage_mapping_, 0);
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.RemappedModifierPressed.AppleExternal",
      modifier_key_usage_mapping_, 0);

  SetUpKeyboard(kExternalGenericKeyboard);
  EXPECT_EQ(std::vector({event_}), SendKeyEvent(event_));
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.ModifierPressed.External",
      modifier_key_usage_mapping_, 0);
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.RemappedModifierPressed.External",
      modifier_key_usage_mapping_, 0);
}

TEST_P(ModifierPressedMetricsTest, KeyReleasedTest) {
  if (event_.code == ui::DomCode::CAPS_LOCK) {
    GTEST_SKIP() << "CapsLock Key will not be marked as EF_IS_REPEAT";
  }

  base::HistogramTester histogram_tester;
  // No metrics should be published if it is a repeated key.
  event_.flags |= ui::EF_IS_REPEAT;

  auto expected = event_;
  if (expected.code == ui::DomCode::CAPS_LOCK) {
    expected.flags |= ui::EF_CAPS_LOCK_ON;
  }

  SetUpKeyboard(kInternalChromeKeyboard);
  EXPECT_EQ(std::vector({expected}), SendKeyEvent(event_));
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.ModifierPressed.Internal",
      modifier_key_usage_mapping_, 0);
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.RemappedModifierPressed.Internal",
      modifier_key_usage_mapping_, 0);

  SetUpKeyboard(kExternalChromeKeyboard);
  EXPECT_EQ(std::vector({expected}), SendKeyEvent(event_));
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.ModifierPressed.CrOSExternal",
      modifier_key_usage_mapping_, 0);
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.RemappedModifierPressed.CrOSExternal",
      modifier_key_usage_mapping_, 0);

  SetUpKeyboard(kExternalAppleKeyboard);
  EXPECT_EQ(std::vector({expected}), SendKeyEvent(event_));
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.ModifierPressed.AppleExternal",
      modifier_key_usage_mapping_, 0);
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.RemappedModifierPressed.AppleExternal",
      modifier_key_usage_mapping_, 0);

  SetUpKeyboard(kExternalGenericKeyboard);
  EXPECT_EQ(std::vector({expected}), SendKeyEvent(event_));
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.ModifierPressed.External",
      modifier_key_usage_mapping_, 0);
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.RemappedModifierPressed.External",
      modifier_key_usage_mapping_, 0);
}

class EventRewriterSixPackKeysTest
    : public EventRewriterTestBase,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  void SetUp() override {
    auto [enable_keyboard_rewriter_fix, enable_modifier_split] = GetParam();

    std::vector<base::test::FeatureRef> enabled_features, disabled_features;
    enabled_features.push_back(features::kInputDeviceSettingsSplit);
    enabled_features.push_back(features::kAltClickAndSixPackCustomization);
    (enable_keyboard_rewriter_fix ? enabled_features : disabled_features)
        .push_back(ash::features::kEnableKeyboardRewriterFix);
    (enable_modifier_split ? enabled_features : disabled_features)
        .push_back(ash::features::kModifierSplit);
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);

    EventRewriterTestBase::SetUp();
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         EventRewriterSixPackKeysTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

TEST_P(EventRewriterSixPackKeysTest, TestRewriteSixPackKeysSearchVariants) {
  Preferences::RegisterProfilePrefs(prefs()->registry());
  mojom::KeyboardSettings settings;
  settings.six_pack_key_remappings = ash::mojom::SixPackKeyInfo::New();
  EXPECT_CALL(*input_device_settings_controller_mock_,
              GetKeyboardSettings(kKeyboardDeviceId))
      .WillRepeatedly(testing::Return(&settings));
  for (const auto& keyboard : kNonAppleKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    // Search+Shift+Backspace -> Insert
    EXPECT_EQ(KeyInsert::Typed(),
              RunRewriter(KeyBackspace::Typed(),
                          ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN));
    // Search+Backspace -> Delete
    EXPECT_EQ(KeyDelete::Typed(),
              RunRewriter(KeyBackspace::Typed(), ui::EF_COMMAND_DOWN));
    // Search+Up -> Prior (aka PageUp)
    EXPECT_EQ(KeyPageUp::Typed(),
              RunRewriter(KeyArrowUp::Typed(), ui::EF_COMMAND_DOWN));
    // Search+Down -> Next (aka PageDown)
    EXPECT_EQ(KeyPageDown::Typed(),
              RunRewriter(KeyArrowDown::Typed(), ui::EF_COMMAND_DOWN));
    // Search+Left -> Home
    EXPECT_EQ(KeyHome::Typed(),
              RunRewriter(KeyArrowLeft::Typed(), ui::EF_COMMAND_DOWN));
    // Search+Right -> End
    EXPECT_EQ(KeyEnd::Typed(),
              RunRewriter(KeyArrowRight::Typed(), ui::EF_COMMAND_DOWN));
    // Search+Shift+Down -> Shift+Next (aka PageDown)
    EXPECT_EQ(KeyPageDown::Typed(ui::EF_SHIFT_DOWN),
              RunRewriter(KeyArrowDown::Typed(),
                          ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN));
    // Search+Ctrl+Up -> Ctrl+Prior (aka PageUp)
    EXPECT_EQ(KeyPageUp::Typed(ui::EF_CONTROL_DOWN),
              RunRewriter(KeyArrowUp::Typed(),
                          ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN));
    // Search+Alt+Left -> Alt+Home
    EXPECT_EQ(KeyHome::Typed(ui::EF_ALT_DOWN),
              RunRewriter(KeyArrowLeft::Typed(),
                          ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN));
  }
}

TEST_P(EventRewriterSixPackKeysTest, TestRewriteSixPackKeysAltVariants) {
  Preferences::RegisterProfilePrefs(prefs()->registry());
  mojom::KeyboardSettings settings;
  settings.six_pack_key_remappings = ash::mojom::SixPackKeyInfo::New();
  settings.six_pack_key_remappings->del =
      ui::mojom::SixPackShortcutModifier::kAlt;
  settings.six_pack_key_remappings->end =
      ui::mojom::SixPackShortcutModifier::kAlt;
  settings.six_pack_key_remappings->home =
      ui::mojom::SixPackShortcutModifier::kAlt;
  settings.six_pack_key_remappings->page_down =
      ui::mojom::SixPackShortcutModifier::kAlt;
  settings.six_pack_key_remappings->page_up =
      ui::mojom::SixPackShortcutModifier::kAlt;

  EXPECT_CALL(*input_device_settings_controller_mock_,
              GetKeyboardSettings(kKeyboardDeviceId))
      .WillRepeatedly(testing::Return(&settings));
  for (const auto& keyboard : kNonAppleKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    // Alt+Backspace -> Delete
    EXPECT_EQ(KeyDelete::Typed(),
              RunRewriter(KeyBackspace::Typed(), ui::EF_ALT_DOWN));
    // Alt+Up -> Prior
    EXPECT_EQ(KeyPageUp::Typed(),
              RunRewriter(KeyArrowUp::Typed(), ui::EF_ALT_DOWN));
    // Alt+Down -> Next
    EXPECT_EQ(KeyPageDown::Typed(),
              RunRewriter(KeyArrowDown::Typed(), ui::EF_ALT_DOWN));
    // Ctrl+Alt+Up -> Home
    EXPECT_EQ(KeyHome::Typed(),
              RunRewriter(KeyArrowUp::Typed(),
                          ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN));
    // Ctrl+Alt+Down -> End
    EXPECT_EQ(KeyEnd::Typed(),
              RunRewriter(KeyArrowDown::Typed(),
                          ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN));
    // Ctrl+Alt+Shift+Up -> Shift+Home
    EXPECT_EQ(
        KeyHome::Typed(ui::EF_SHIFT_DOWN),
        RunRewriter(KeyArrowUp::Typed(),
                    ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN));
    // Ctrl+Alt+Search+Down -> Search+End
    EXPECT_EQ(KeyEnd::Typed(ui::EF_COMMAND_DOWN),
              RunRewriter(KeyArrowDown::Typed(), ui::EF_CONTROL_DOWN |
                                                     ui::EF_ALT_DOWN |
                                                     ui::EF_COMMAND_DOWN));
  }
}

TEST_P(EventRewriterSixPackKeysTest, TestRewriteSixPackKeysBlockedBySetting) {
  Preferences::RegisterProfilePrefs(prefs()->registry());
  mojom::KeyboardSettings settings;
  // "six pack" key settings use the search modifier by default.
  settings.six_pack_key_remappings = ash::mojom::SixPackKeyInfo::New();
  EXPECT_CALL(*input_device_settings_controller_mock_,
              GetKeyboardSettings(kKeyboardDeviceId))
      .WillRepeatedly(testing::Return(&settings));
  // No rewrite should occur since the search-based rewrite is the setting for
  // the "Delete" 6-pack key.
  SetUpKeyboard(kInternalChromeKeyboard);
  EXPECT_EQ(KeyBackspace::Typed(ui::EF_ALT_DOWN),
            RunRewriter(KeyBackspace::Typed(), ui::EF_ALT_DOWN));
  EXPECT_EQ(1u, message_center_.NotificationCount());
  ClearNotifications();

  settings.six_pack_key_remappings->del =
      ui::mojom::SixPackShortcutModifier::kAlt;
  // Rewrite should occur now that the alt rewrite is the current setting.
  // Alt+Backspace -> Delete
  EXPECT_EQ(KeyDelete::Typed(),
            RunRewriter(KeyBackspace::Typed(), ui::EF_ALT_DOWN));

  settings.six_pack_key_remappings->del =
      ui::mojom::SixPackShortcutModifier::kNone;
  // No rewrite should occur since remapping a key event to the "Delete"
  // 6-pack key is disabled.
  EXPECT_EQ(KeyBackspace::Typed(ui::EF_ALT_DOWN),
            RunRewriter(KeyBackspace::Typed(), ui::EF_ALT_DOWN));
  EXPECT_EQ(1u, message_center_.NotificationCount());
  ClearNotifications();
}

class EventRewriterExtendedFkeysTest
    : public EventRewriterTestBase,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  void SetUp() override {
    auto [enable_keyboard_rewriter_fix, enable_modifier_split] = GetParam();
    std::vector<base::test::FeatureRef> enabled_features, disabled_features;
    enabled_features.push_back(ash::features::kInputDeviceSettingsSplit);
    enabled_features.push_back(::features::kSupportF11AndF12KeyShortcuts);
    (enable_keyboard_rewriter_fix ? enabled_features : disabled_features)
        .push_back(ash::features::kEnableKeyboardRewriterFix);
    (enable_modifier_split ? enabled_features : disabled_features)
        .push_back(ash::features::kModifierSplit);
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);

    EventRewriterTestBase::SetUp();
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         EventRewriterExtendedFkeysTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

TEST_P(EventRewriterExtendedFkeysTest, TestRewriteExtendedFkeys) {
  Preferences::RegisterProfilePrefs(prefs()->registry());
  mojom::KeyboardSettings settings;
  settings.f11 = ui::mojom::ExtendedFkeysModifier::kAlt;
  settings.f12 = ui::mojom::ExtendedFkeysModifier::kShift;
  settings.top_row_are_fkeys = true;

  EXPECT_CALL(*input_device_settings_controller_mock_,
              GetKeyboardSettings(kKeyboardDeviceId))
      .WillRepeatedly(testing::Return(&settings));

  SetUpKeyboard(kInternalChromeKeyboard);
  EXPECT_EQ(KeyF11::Typed(), RunRewriter(KeyF1::Typed(), ui::EF_ALT_DOWN));
  EXPECT_EQ(KeyF12::Typed(), RunRewriter(KeyF2::Typed(), ui::EF_SHIFT_DOWN));

  settings.f11 = ui::mojom::ExtendedFkeysModifier::kCtrlShift;
  settings.f12 = ui::mojom::ExtendedFkeysModifier::kAlt;

  EXPECT_EQ(
      KeyF11::Typed(),
      RunRewriter(KeyF1::Typed(), ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN));
  EXPECT_EQ(KeyF12::Typed(), RunRewriter(KeyF2::Typed(), ui::EF_ALT_DOWN));
}

TEST_P(EventRewriterExtendedFkeysTest,
       TestRewriteExtendedFkeysBlockedBySetting) {
  Preferences::RegisterProfilePrefs(prefs()->registry());
  mojom::KeyboardSettings settings;
  settings.f11 = ui::mojom::ExtendedFkeysModifier::kDisabled;
  settings.f12 = ui::mojom::ExtendedFkeysModifier::kDisabled;
  settings.top_row_are_fkeys = true;

  EXPECT_CALL(*input_device_settings_controller_mock_,
              GetKeyboardSettings(kKeyboardDeviceId))
      .WillRepeatedly(testing::Return(&settings));
  SetUpKeyboard(kInternalChromeKeyboard);

  EXPECT_EQ(KeyF1::Typed(ui::EF_ALT_DOWN),
            RunRewriter(KeyF1::Typed(), ui::EF_ALT_DOWN));
}

TEST_P(EventRewriterExtendedFkeysTest, TestRewriteExtendedFkeysTopRowAreFkeys) {
  Preferences::RegisterProfilePrefs(prefs()->registry());
  mojom::KeyboardSettings settings;
  settings.f11 = ui::mojom::ExtendedFkeysModifier::kAlt;
  settings.f12 = ui::mojom::ExtendedFkeysModifier::kShift;
  settings.top_row_are_fkeys = true;

  EXPECT_CALL(*input_device_settings_controller_mock_,
              GetKeyboardSettings(kKeyboardDeviceId))
      .WillRepeatedly(testing::Return(&settings));
  SetUpKeyboard(kInternalChromeKeyboard);
  EXPECT_EQ(KeyF11::Typed(), RunRewriter(KeyF1::Typed(), ui::EF_ALT_DOWN));
  EXPECT_EQ(
      KeyF11::Typed(ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN),
      RunRewriter(KeyF1::Typed(),
                  ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN));
  EXPECT_EQ(KeyF12::Typed(), RunRewriter(KeyF2::Typed(), ui::EF_SHIFT_DOWN));

  settings.top_row_are_fkeys = false;
  EXPECT_EQ(KeyF11::Typed(),
            RunRewriter(KeyF1::Typed(), ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN));
  EXPECT_EQ(
      KeyF12::Typed(),
      RunRewriter(KeyF2::Typed(), ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN));
}

class EventRewriterSettingsSplitTest
    : public EventRewriterTestBase,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  void SetUp() override {
    auto [enable_keyboard_rewriter_fix, enable_modifier_split] = GetParam();

    std::vector<base::test::FeatureRef> enabled_features, disabled_features;
    enabled_features.push_back(ash::features::kInputDeviceSettingsSplit);
    (enable_keyboard_rewriter_fix ? enabled_features : disabled_features)
        .push_back(ash::features::kEnableKeyboardRewriterFix);
    (enable_modifier_split ? enabled_features : disabled_features)
        .push_back(ash::features::kModifierSplit);
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);

    EventRewriterTestBase::SetUp();
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         EventRewriterSettingsSplitTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

TEST_P(EventRewriterSettingsSplitTest, TopRowAreFKeys) {
  mojom::KeyboardSettings settings;
  EXPECT_CALL(*input_device_settings_controller_mock_,
              GetKeyboardSettings(kKeyboardDeviceId))
      .WillRepeatedly(testing::Return(&settings));
  SetUpKeyboard(kExternalGenericKeyboard);

  settings.top_row_are_fkeys = false;
  settings.suppress_meta_fkey_rewrites = false;

  EXPECT_EQ(KeyBrowserBack::Typed(), RunRewriter(KeyF1::Typed()));

  settings.top_row_are_fkeys = true;
  EXPECT_EQ(KeyF1::Typed(), RunRewriter(KeyF1::Typed()));
}

TEST_P(EventRewriterSettingsSplitTest, RewriteMetaTopRowKeyComboEvents) {
  mojom::KeyboardSettings settings;
  settings.top_row_are_fkeys = true;
  EXPECT_CALL(*input_device_settings_controller_mock_,
              GetKeyboardSettings(kKeyboardDeviceId))
      .WillRepeatedly(testing::Return(&settings));
  SetUpKeyboard(kExternalGenericKeyboard);

  settings.suppress_meta_fkey_rewrites = false;
  EXPECT_EQ(KeyBrowserBack::Typed(),
            RunRewriter(KeyF1::Typed(), ui::EF_COMMAND_DOWN));

  settings.suppress_meta_fkey_rewrites = true;
  EXPECT_EQ(KeyF1::Typed(ui::EF_COMMAND_DOWN),
            RunRewriter(KeyF1::Typed(), ui::EF_COMMAND_DOWN));
}

TEST_P(EventRewriterSettingsSplitTest, ModifierRemapping) {
  mojom::KeyboardSettings settings;
  EXPECT_CALL(*input_device_settings_controller_mock_,
              GetKeyboardSettings(kKeyboardDeviceId))
      .WillRepeatedly(testing::Return(&settings));
  SetUpKeyboard(kExternalGenericKeyboard);

  settings.modifier_remappings = {
      {ui::mojom::ModifierKey::kAlt, ui::mojom::ModifierKey::kControl},
      {ui::mojom::ModifierKey::kMeta, ui::mojom::ModifierKey::kBackspace}};

  // Test remapping modifier keys.
  EXPECT_EQ(KeyRControl::Typed(), RunRewriter(KeyRAlt::Typed()));
  EXPECT_EQ(KeyBackspace::Typed(), RunRewriter(KeyLMeta::Typed()));
  EXPECT_EQ(KeyLControl::Typed(), RunRewriter(KeyLControl::Typed()));

  // Test remapping modifier flags.
  EXPECT_EQ(KeyA::Typed(ui::EF_CONTROL_DOWN),
            RunRewriter(KeyA::Typed(), ui::EF_ALT_DOWN));
  EXPECT_EQ(KeyA::Typed(), RunRewriter(KeyA::Typed(), ui::EF_COMMAND_DOWN));
  EXPECT_EQ(KeyA::Typed(ui::EF_CONTROL_DOWN),
            RunRewriter(KeyA::Typed(), ui::EF_CONTROL_DOWN));
}

class KeyEventRemappedToSixPackKeyTest
    : public EventRewriterTestBase,
      public testing::WithParamInterface<
          std::tuple<bool,
                     std::tuple<ui::KeyboardCode, bool, int, const char*>>> {
 public:
  void SetUp() override {
    bool fix_enabled;
    auto tuple =
        std::tie(key_code_, alt_based_, expected_pref_value_, pref_name_);
    std::tie(fix_enabled, tuple) = GetParam();
    if (fix_enabled) {
      scoped_feature_list_.InitAndEnableFeature(
          ash::features::kEnableKeyboardRewriterFix);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          ash::features::kEnableKeyboardRewriterFix);
    }
    EventRewriterTestBase::SetUp();
  }

 protected:
  ui::KeyboardCode key_code_;
  bool alt_based_;
  int expected_pref_value_;
  const char* pref_name_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    KeyEventRemappedToSixPackKeyTest,
    testing::Combine(
        testing::Bool(),
        testing::ValuesIn(
            std::vector<std::tuple<ui::KeyboardCode, bool, int, const char*>>{
                {ui::VKEY_DELETE, false, -1,
                 prefs::kKeyEventRemappedToSixPackDelete},
                {ui::VKEY_HOME, true, 1, prefs::kKeyEventRemappedToSixPackHome},
                {ui::VKEY_PRIOR, false, -1,
                 prefs::kKeyEventRemappedToSixPackPageDown},
                {ui::VKEY_END, true, 1, prefs::kKeyEventRemappedToSixPackEnd},
                {ui::VKEY_NEXT, false, -1,
                 prefs::kKeyEventRemappedToSixPackPageUp}})));

TEST_P(KeyEventRemappedToSixPackKeyTest, KeyEventRemappedTest) {
  Preferences::RegisterProfilePrefs(prefs()->registry());
  IntegerPrefMember int_pref;
  int_pref.Init(pref_name_, prefs());
  int_pref.SetValue(0);
  delegate_->RecordSixPackEventRewrite(key_code_, alt_based_);
  EXPECT_EQ(expected_pref_value_, prefs()->GetInteger(pref_name_));
}

class EventRewriterRemapToRightClickTest
    : public EventRewriterTestBase,
      public message_center::MessageCenterObserver,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  void SetUp() override {
    auto [enable_keyboard_rewriter_fix, enable_modifier_split] = GetParam();
    std::vector<base::test::FeatureRef> enabled_features, disabled_features;
    enabled_features.push_back(features::kInputDeviceSettingsSplit);
    enabled_features.push_back(features::kAltClickAndSixPackCustomization);
    (enable_keyboard_rewriter_fix ? enabled_features : disabled_features)
        .push_back(ash::features::kEnableKeyboardRewriterFix);
    (enable_modifier_split ? enabled_features : disabled_features)
        .push_back(ash::features::kModifierSplit);
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);

    EventRewriterTestBase::SetUp();

    Preferences::RegisterProfilePrefs(prefs()->registry());
    ui::DeviceDataManager* device_data_manager =
        ui::DeviceDataManager::GetInstance();
    std::vector<ui::TouchpadDevice> touchpad_devices(1);
    touchpad_devices[0].id = kTouchpadId1;
    static_cast<ui::DeviceHotplugEventObserver*>(device_data_manager)
        ->OnTouchpadDevicesUpdated(touchpad_devices);

    EXPECT_CALL(*input_device_settings_controller_mock_,
                GetTouchpadSettings(kTouchpadId1))
        .WillRepeatedly(testing::Return(&settings_));

    observation_.Observe(&message_center_);
  }

  void TearDown() override {
    observation_.Reset();
    EventRewriterTestBase::TearDown();
  }

  // message_center::MessageCenterObserver:
  void OnNotificationAdded(const std::string& notification_id) override {
    ++notification_count_;
  }

  void SetSimulateRightClickSetting(
      ui::mojom::SimulateRightClickModifier modifier) {
    settings_.simulate_right_click = modifier;
  }

  int notification_count() { return notification_count_; }

 private:
  mojom::TouchpadSettings settings_;
  int notification_count_ = 0;
  base::ScopedObservation<message_center::MessageCenter,
                          message_center::MessageCenterObserver>
      observation_{this};
};

INSTANTIATE_TEST_SUITE_P(All,
                         EventRewriterRemapToRightClickTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

TEST_P(EventRewriterRemapToRightClickTest, AltClickRemappedToRightClick) {
  SetSimulateRightClickSetting(ui::mojom::SimulateRightClickModifier::kAlt);
  int flag_masks = ui::EF_ALT_DOWN | ui::EF_LEFT_MOUSE_BUTTON;

  ui::MouseEvent press(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), flag_masks,
                       ui::EF_LEFT_MOUSE_BUTTON);
  ui::EventTestApi test_press(&press);
  test_press.set_source_device_id(kTouchpadId1);
  EXPECT_EQ(ui::EventType::kMousePressed, press.type());
  EXPECT_EQ(flag_masks, press.flags());
  const ui::MouseEvent result = RewriteMouseButtonEvent(press);
  EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & result.flags());
  EXPECT_NE(flag_masks, flag_masks & result.flags());
  EXPECT_EQ(ui::EF_RIGHT_MOUSE_BUTTON, result.changed_button_flags());
}

TEST_P(EventRewriterRemapToRightClickTest, SearchClickRemappedToRightClick) {
  SetSimulateRightClickSetting(ui::mojom::SimulateRightClickModifier::kSearch);
  int flag_masks = ui::EF_COMMAND_DOWN | ui::EF_LEFT_MOUSE_BUTTON;

  ui::MouseEvent press(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), flag_masks,
                       ui::EF_LEFT_MOUSE_BUTTON);
  ui::EventTestApi test_press(&press);
  test_press.set_source_device_id(kTouchpadId1);
  EXPECT_EQ(ui::EventType::kMousePressed, press.type());
  EXPECT_EQ(flag_masks, press.flags());
  const ui::MouseEvent result = RewriteMouseButtonEvent(press);
  EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & result.flags());
  EXPECT_NE(flag_masks, flag_masks & result.flags());
  EXPECT_EQ(ui::EF_RIGHT_MOUSE_BUTTON, result.changed_button_flags());
}

TEST_P(EventRewriterRemapToRightClickTest, RemapToRightClickBlockedBySetting) {
  ui::DeviceDataManager* device_data_manager =
      ui::DeviceDataManager::GetInstance();
  std::vector<ui::TouchpadDevice> touchpad_devices(1);
  touchpad_devices[0].id = kTouchpadId1;
  static_cast<ui::DeviceHotplugEventObserver*>(device_data_manager)
      ->OnTouchpadDevicesUpdated(touchpad_devices);
  SetSimulateRightClickSetting(ui::mojom::SimulateRightClickModifier::kAlt);

  {
    ui::MouseEvent press(ui::EventType::kMousePressed, gfx::Point(),
                         gfx::Point(), ui::EventTimeForNow(),
                         ui::EF_COMMAND_DOWN | ui::EF_LEFT_MOUSE_BUTTON,
                         ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_press(&press);
    test_press.set_source_device_id(kTouchpadId1);
    const ui::MouseEvent result = RewriteMouseButtonEvent(press);
    EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & result.flags());
    EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, result.changed_button_flags());
    EXPECT_EQ(notification_count(), 1);
  }
  {
    SetSimulateRightClickSetting(
        ui::mojom::SimulateRightClickModifier::kSearch);
    ui::MouseEvent press(ui::EventType::kMousePressed, gfx::Point(),
                         gfx::Point(), ui::EventTimeForNow(),
                         ui::EF_ALT_DOWN | ui::EF_LEFT_MOUSE_BUTTON,
                         ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_press(&press);
    test_press.set_source_device_id(kTouchpadId1);
    const ui::MouseEvent result = RewriteMouseButtonEvent(press);
    EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & result.flags());
    EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, result.changed_button_flags());
    EXPECT_EQ(notification_count(), 2);
  }
}

TEST_P(EventRewriterRemapToRightClickTest, RemapToRightClickIsDisabled) {
  ui::DeviceDataManager* device_data_manager =
      ui::DeviceDataManager::GetInstance();
  std::vector<ui::TouchpadDevice> touchpad_devices(1);
  touchpad_devices[0].id = kTouchpadId1;
  static_cast<ui::DeviceHotplugEventObserver*>(device_data_manager)
      ->OnTouchpadDevicesUpdated(touchpad_devices);
  SetSimulateRightClickSetting(ui::mojom::SimulateRightClickModifier::kNone);

  ui::MouseEvent press(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(),
                       ui::EF_COMMAND_DOWN | ui::EF_LEFT_MOUSE_BUTTON,
                       ui::EF_LEFT_MOUSE_BUTTON);
  ui::EventTestApi test_press(&press);
  test_press.set_source_device_id(kTouchpadId1);
  const ui::MouseEvent result = RewriteMouseButtonEvent(press);
  EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & result.flags());
  EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, result.changed_button_flags());
  EXPECT_EQ(notification_count(), 1);
}

class FKeysRewritingPeripheralCustomizationTest
    : public EventRewriterTestBase,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  void SetUp() override {
    auto [enable_keyboard_rewriter_fix, enable_modifier_split] = GetParam();

    std::vector<base::test::FeatureRef> enabled_features, disabled_features;
    enabled_features.push_back(features::kInputDeviceSettingsSplit);
    enabled_features.push_back(features::kPeripheralCustomization);
    (enable_keyboard_rewriter_fix ? enabled_features : disabled_features)
        .push_back(ash::features::kEnableKeyboardRewriterFix);
    (enable_modifier_split ? enabled_features : disabled_features)
        .push_back(ash::features::kModifierSplit);
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);

    EventRewriterTestBase::SetUp();
  }

 protected:
  mojom::MouseSettings mouse_settings_;
  mojom::KeyboardSettings keyboard_settings_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         FKeysRewritingPeripheralCustomizationTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

TEST_P(FKeysRewritingPeripheralCustomizationTest, FKeysNotRewritten) {
  EXPECT_CALL(*input_device_settings_controller_mock_,
              GetKeyboardSettings(kMouseDeviceId))
      .WillRepeatedly(testing::Return(nullptr));
  EXPECT_CALL(*input_device_settings_controller_mock_,
              GetMouseSettings(kMouseDeviceId))
      .WillRepeatedly(testing::Return(&mouse_settings_));

  SetUpKeyboard(kExternalGenericKeyboard);

  // Mice that press F-Keys do not get rewritten to actions.
  EXPECT_EQ(KeyF1::Typed(),
            RunRewriter(KeyF1::Typed(), ui::EF_NONE, kMouseDeviceId));
  EXPECT_EQ(KeyF2::Typed(),
            RunRewriter(KeyF2::Typed(), ui::EF_NONE, kMouseDeviceId));
  EXPECT_EQ(KeyF1::Typed(ui::EF_COMMAND_DOWN),
            RunRewriter(KeyF1::Typed(), ui::EF_COMMAND_DOWN, kMouseDeviceId));
  EXPECT_EQ(KeyF2::Typed(ui::EF_COMMAND_DOWN),
            RunRewriter(KeyF2::Typed(), ui::EF_COMMAND_DOWN, kMouseDeviceId));

  // Keyboards that press F-Keys do get rewritten to actions.
  EXPECT_EQ(KeyBrowserBack::Typed(), RunRewriter(KeyF1::Typed()));
  EXPECT_EQ(KeyBrowserForward::Typed(), RunRewriter(KeyF2::Typed()));
  EXPECT_EQ(KeyF1::Typed(), RunRewriter(KeyF1::Typed(), ui::EF_COMMAND_DOWN));
  EXPECT_EQ(KeyF2::Typed(), RunRewriter(KeyF2::Typed(), ui::EF_COMMAND_DOWN));
}

}  // namespace ash
