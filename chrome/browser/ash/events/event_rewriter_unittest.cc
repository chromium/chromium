// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/accessibility/sticky_keys/sticky_keys_controller.h"
#include "ash/accessibility/sticky_keys/sticky_keys_overlay.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/input_device_settings_controller.h"
#include "ash/public/cpp/test/mock_input_device_settings_controller.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/shell.h"
#include "ash/system/input_device_settings/input_device_settings_notification_controller.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/events/event_rewriter_delegate_impl.h"
#include "chrome/browser/ash/input_method/input_method_configuration.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/notifications/deprecation_notification_controller.h"
#include "chrome/browser/ash/preferences.h"
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
#include "ui/events/ash/event_rewriter_ash.h"
#include "ui/events/ash/event_rewriter_metrics.h"
#include "ui/events/ash/keyboard_capability.h"
#include "ui/events/ash/keyboard_device_id_event_rewriter.h"
#include "ui/events/ash/mojom/extended_fkeys_modifier.mojom-shared.h"
#include "ui/events/ash/mojom/modifier_key.mojom-shared.h"
#include "ui/events/ash/mojom/modifier_key.mojom.h"
#include "ui/events/ash/mojom/simulate_right_click_modifier.mojom-shared.h"
#include "ui/events/ash/mojom/six_pack_shortcut_modifier.mojom-shared.h"
#include "ui/events/ash/pref_names.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/keyboard_device.h"
#include "ui/events/devices/touchpad_device.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_rewriter.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/test/events_test_utils.h"
#include "ui/events/test/test_event_processor.h"
#include "ui/events/test/test_event_rewriter_continuation.h"
#include "ui/events/test/test_event_source.h"
#include "ui/events/types/event_type.h"
#include "ui/message_center/fake_message_center.h"
#include "ui/wm/core/window_util.h"

namespace {

constexpr int kKeyboardDeviceId = 123;
constexpr uint32_t kNoScanCode = 0;
constexpr char kKbdSysPath[] = "/devices/platform/i8042/serio2/input/input1";
constexpr char kKbdTopRowPropertyName[] = "CROS_KEYBOARD_TOP_ROW_LAYOUT";
constexpr char kKbdTopRowLayoutAttributeName[] = "function_row_physmap";

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
    events_.emplace_back(event->Clone());
    return ui::EventDispatchDetails();
  }

 private:
  std::vector<std::unique_ptr<ui::Event>> events_;
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

  std::string ToString() const;
};

// Factory methods of TestKeyEvent for reducing syntax noises in tests.
constexpr TestKeyEvent UnknownPressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::NONE, ui::DomKey::UNIDENTIFIED,
          ui::VKEY_UNKNOWN, flags};
}

constexpr TestKeyEvent APressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::US_A,
          ui::DomKey::Constant<'a'>::Character, ui::VKEY_A, flags};
}

constexpr TestKeyEvent AReleased(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_RELEASED, ui::DomCode::US_A,
          ui::DomKey::Constant<'a'>::Character, ui::VKEY_A, flags};
}

constexpr TestKeyEvent UnidentifiedAPressed(
    ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::US_A, ui::DomKey::UNIDENTIFIED,
          ui::VKEY_A, flags};
}

constexpr TestKeyEvent BPressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::US_B,
          ((flags & ui::EF_SHIFT_DOWN)
               ? ui::DomKey(ui::DomKey::Constant<'B'>::Character)
               : ui::DomKey(ui::DomKey::Constant<'b'>::Character)),
          ui::VKEY_B, flags};
}

constexpr TestKeyEvent LShiftPressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::SHIFT_LEFT, ui::DomKey::SHIFT,
          ui::VKEY_SHIFT, flags | ui::EF_SHIFT_DOWN};
}

constexpr TestKeyEvent RShiftPressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::SHIFT_RIGHT, ui::DomKey::SHIFT,
          ui::VKEY_SHIFT};
}

constexpr TestKeyEvent LWinPressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::META_LEFT, ui::DomKey::META,
          ui::VKEY_LWIN, flags | ui::EF_COMMAND_DOWN};
}

constexpr TestKeyEvent LWinReleased(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_RELEASED, ui::DomCode::META_LEFT, ui::DomKey::META,
          ui::VKEY_LWIN, flags};
}

constexpr TestKeyEvent RWinPressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::META_RIGHT, ui::DomKey::META,
          ui::VKEY_RWIN, flags | ui::EF_COMMAND_DOWN};
}

constexpr TestKeyEvent LControlPressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::CONTROL_LEFT, ui::DomKey::CONTROL,
          ui::VKEY_CONTROL, flags | ui::EF_CONTROL_DOWN};
}

constexpr TestKeyEvent LControlReleased(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_RELEASED, ui::DomCode::CONTROL_LEFT, ui::DomKey::CONTROL,
          ui::VKEY_CONTROL, flags};
}

constexpr TestKeyEvent RControlPressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::CONTROL_RIGHT, ui::DomKey::CONTROL,
          ui::VKEY_CONTROL, flags | ui::EF_CONTROL_DOWN};
}

constexpr TestKeyEvent LAltPressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::ALT_LEFT, ui::DomKey::ALT,
          ui::VKEY_MENU, flags | ui::EF_ALT_DOWN};
}

constexpr TestKeyEvent LAltReleased(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_RELEASED, ui::DomCode::ALT_LEFT, ui::DomKey::ALT,
          ui::VKEY_MENU, flags};
}

constexpr TestKeyEvent RAltPressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::ALT_RIGHT, ui::DomKey::ALT,
          ui::VKEY_MENU, flags | ui::EF_ALT_DOWN};
}

constexpr TestKeyEvent CapsLockPressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::CAPS_LOCK, ui::DomKey::CAPS_LOCK,
          ui::VKEY_CAPITAL, flags | ui::EF_MOD3_DOWN};
}

constexpr TestKeyEvent CapsLockReleased(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_RELEASED, ui::DomCode::CAPS_LOCK, ui::DomKey::CAPS_LOCK,
          ui::VKEY_CAPITAL, flags};
}

constexpr TestKeyEvent EscapePressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::ESCAPE, ui::DomKey::ESCAPE,
          ui::VKEY_ESCAPE, flags};
}

constexpr TestKeyEvent EscapeReleased(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_RELEASED, ui::DomCode::ESCAPE, ui::DomKey::ESCAPE,
          ui::VKEY_ESCAPE, flags};
}

constexpr TestKeyEvent CommaPressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::COMMA,
          ((flags & ui::EF_SHIFT_DOWN)
               ? ui::DomKey(ui::DomKey::Constant<'<'>::Character)
               : ui::DomKey(ui::DomKey::Constant<','>::Character)),
          ui::VKEY_OEM_COMMA, flags};
}

constexpr TestKeyEvent PeriodPressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::PERIOD,
          ui::DomKey::Constant<'.'>::Character, ui::VKEY_OEM_PERIOD, flags};
}

constexpr TestKeyEvent Digit1Pressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::DIGIT1,
          ui::DomKey::Constant<'1'>::Character, ui::VKEY_1, flags};
}

constexpr TestKeyEvent Digit2Pressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::DIGIT2,
          ui::DomKey::Constant<'2'>::Character, ui::VKEY_2, flags};
}

constexpr TestKeyEvent Digit3Pressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::DIGIT3,
          ui::DomKey::Constant<'3'>::Character, ui::VKEY_3, flags};
}

constexpr TestKeyEvent Digit4Pressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::DIGIT4,
          ui::DomKey::Constant<'4'>::Character, ui::VKEY_4, flags};
}

constexpr TestKeyEvent Digit5Pressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::DIGIT5,
          ui::DomKey::Constant<'5'>::Character, ui::VKEY_5, flags};
}

constexpr TestKeyEvent Digit6Pressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::DIGIT6,
          ui::DomKey::Constant<'6'>::Character, ui::VKEY_6, flags};
}

constexpr TestKeyEvent Digit7Pressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::DIGIT7,
          ui::DomKey::Constant<'7'>::Character, ui::VKEY_7, flags};
}

constexpr TestKeyEvent Digit8Pressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::DIGIT8,
          ui::DomKey::Constant<'8'>::Character, ui::VKEY_8, flags};
}

constexpr TestKeyEvent Digit9Pressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::DIGIT9,
          ((flags & ui::EF_SHIFT_DOWN)
               ? ui::DomKey(ui::DomKey::Constant<'('>::Character)
               : ui::DomKey(ui::DomKey::Constant<'9'>::Character)),
          ui::VKEY_9, flags};
}

constexpr TestKeyEvent Digit0Pressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::DIGIT0,
          ui::DomKey::Constant<'0'>::Character, ui::VKEY_0, flags};
}

constexpr TestKeyEvent MinusPressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::MINUS,
          ui::DomKey::Constant<'-'>::Character, ui::VKEY_OEM_MINUS, flags};
}

constexpr TestKeyEvent EqualPressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::EQUAL,
          ui::DomKey::Constant<'='>::Character, ui::VKEY_OEM_PLUS, flags};
}

constexpr TestKeyEvent F1Pressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::F1, ui::DomKey::F1, ui::VKEY_F1,
          flags};
}

constexpr TestKeyEvent F2Pressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::F2, ui::DomKey::F2, ui::VKEY_F2,
          flags};
}

constexpr TestKeyEvent F3Pressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::F3, ui::DomKey::F3, ui::VKEY_F3,
          flags};
}

constexpr TestKeyEvent F4Pressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::F4, ui::DomKey::F4, ui::VKEY_F4,
          flags};
}

constexpr TestKeyEvent F5Pressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::F5, ui::DomKey::F5, ui::VKEY_F5,
          flags};
}

constexpr TestKeyEvent F6Pressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::F6, ui::DomKey::F6, ui::VKEY_F6,
          flags};
}

constexpr TestKeyEvent F7Pressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::F7, ui::DomKey::F7, ui::VKEY_F7,
          flags};
}

constexpr TestKeyEvent F8Pressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::F8, ui::DomKey::F8, ui::VKEY_F8,
          flags};
}

constexpr TestKeyEvent F9Pressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::F9, ui::DomKey::F9, ui::VKEY_F9,
          flags};
}

constexpr TestKeyEvent F10Pressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::F10, ui::DomKey::F10, ui::VKEY_F10,
          flags};
}

constexpr TestKeyEvent F11Pressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::F11, ui::DomKey::F11, ui::VKEY_F11,
          flags};
}

constexpr TestKeyEvent F12Pressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::F12, ui::DomKey::F12, ui::VKEY_F12,
          flags};
}

constexpr TestKeyEvent F13Pressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::F13, ui::DomKey::F13, ui::VKEY_F13,
          flags};
}

constexpr TestKeyEvent F14Pressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::F14, ui::DomKey::F14, ui::VKEY_F14,
          flags};
}

constexpr TestKeyEvent F15Pressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::F15, ui::DomKey::F15, ui::VKEY_F15,
          flags};
}

constexpr TestKeyEvent BackspacePressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::BACKSPACE, ui::DomKey::BACKSPACE,
          ui::VKEY_BACK, flags};
}

constexpr TestKeyEvent InsertPressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::INSERT, ui::DomKey::INSERT,
          ui::VKEY_INSERT, flags};
}

constexpr TestKeyEvent DeletePressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::DEL, ui::DomKey::DEL,
          ui::VKEY_DELETE, flags};
}

constexpr TestKeyEvent HomePressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::HOME, ui::DomKey::HOME,
          ui::VKEY_HOME, flags};
}

constexpr TestKeyEvent EndPressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::END, ui::DomKey::END, ui::VKEY_END,
          flags};
}

constexpr TestKeyEvent PageUpPressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::PAGE_UP, ui::DomKey::PAGE_UP,
          ui::VKEY_PRIOR, flags};
}

constexpr TestKeyEvent PageDownPressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::PAGE_DOWN, ui::DomKey::PAGE_DOWN,
          ui::VKEY_NEXT, flags};
}

constexpr TestKeyEvent ArrowUpPressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::ARROW_UP, ui::DomKey::ARROW_UP,
          ui::VKEY_UP, flags};
}

constexpr TestKeyEvent ArrowDownPressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::ARROW_DOWN, ui::DomKey::ARROW_DOWN,
          ui::VKEY_DOWN, flags};
}

constexpr TestKeyEvent ArrowLeftPressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::ARROW_LEFT, ui::DomKey::ARROW_LEFT,
          ui::VKEY_LEFT, flags};
}

constexpr TestKeyEvent ArrowRightPressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::ARROW_RIGHT, ui::DomKey::ARROW_RIGHT,
          ui::VKEY_RIGHT, flags};
}

constexpr TestKeyEvent BrowserBackPressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::BROWSER_BACK,
          ui::DomKey::BROWSER_BACK, ui::VKEY_BROWSER_BACK, flags};
}

constexpr TestKeyEvent BrowserForwardPressed(
    ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::BROWSER_FORWARD,
          ui::DomKey::BROWSER_FORWARD, ui::VKEY_BROWSER_FORWARD, flags};
}

constexpr TestKeyEvent BrowserRefreshPressed(
    ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::BROWSER_REFRESH,
          ui::DomKey::BROWSER_REFRESH, ui::VKEY_BROWSER_REFRESH, flags};
}

constexpr TestKeyEvent ZoomTogglePressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::ZOOM_TOGGLE, ui::DomKey::ZOOM_TOGGLE,
          ui::VKEY_ZOOM, flags};
}

constexpr TestKeyEvent SelectTaskPressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::SELECT_TASK,
          ui::DomKey::LAUNCH_MY_COMPUTER, ui::VKEY_MEDIA_LAUNCH_APP1, flags};
}

constexpr TestKeyEvent BrightnessDownPressed(
    ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::BRIGHTNESS_DOWN,
          ui::DomKey::BRIGHTNESS_DOWN, ui::VKEY_BRIGHTNESS_DOWN, flags};
}

constexpr TestKeyEvent BrightnessUpPressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::BRIGHTNESS_UP,
          ui::DomKey::BRIGHTNESS_UP, ui::VKEY_BRIGHTNESS_UP, flags};
}

constexpr TestKeyEvent MediaPlayPausePressed(
    ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::MEDIA_PLAY_PAUSE,
          ui::DomKey::MEDIA_PLAY_PAUSE, ui::VKEY_MEDIA_PLAY_PAUSE, flags};
}

constexpr TestKeyEvent VolumeMutePressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::VOLUME_MUTE,
          ui::DomKey::AUDIO_VOLUME_MUTE, ui::VKEY_VOLUME_MUTE, flags};
}

constexpr TestKeyEvent VolumeDownPressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::VOLUME_DOWN,
          ui::DomKey::AUDIO_VOLUME_DOWN, ui::VKEY_VOLUME_DOWN, flags};
}

constexpr TestKeyEvent VolumeUpPressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::VOLUME_UP,
          ui::DomKey::AUDIO_VOLUME_UP, ui::VKEY_VOLUME_UP, flags};
}

constexpr TestKeyEvent PrivacyScreenTogglePressed(
    ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::PRIVACY_SCREEN_TOGGLE,
          ui::DomKey::F12,  // no dom-key for PRIVACY_SCREEN_TOGGLE.
          ui::VKEY_PRIVACY_SCREEN_TOGGLE, flags};
}

constexpr TestKeyEvent LaunchAssistantPressed(
    ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::LAUNCH_ASSISTANT,
          ui::DomKey::LAUNCH_ASSISTANT, ui::VKEY_ASSISTANT, flags};
}

// Hereafter, numpad key events.

constexpr TestKeyEvent Numpad0Pressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::NUMPAD0,
          ui::DomKey::Constant<'0'>::Character, ui::VKEY_NUMPAD0, flags};
}

constexpr TestKeyEvent Numpad1Pressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::NUMPAD1,
          ui::DomKey::Constant<'1'>::Character, ui::VKEY_NUMPAD1, flags};
}

constexpr TestKeyEvent Numpad2Pressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::NUMPAD2,
          ui::DomKey::Constant<'2'>::Character, ui::VKEY_NUMPAD2, flags};
}

constexpr TestKeyEvent Numpad3Pressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::NUMPAD3,
          ui::DomKey::Constant<'3'>::Character, ui::VKEY_NUMPAD3, flags};
}

constexpr TestKeyEvent Numpad4Pressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::NUMPAD4,
          ui::DomKey::Constant<'4'>::Character, ui::VKEY_NUMPAD4, flags};
}

constexpr TestKeyEvent Numpad5Pressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::NUMPAD5,
          ui::DomKey::Constant<'5'>::Character, ui::VKEY_NUMPAD5, flags};
}

constexpr TestKeyEvent Numpad6Pressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::NUMPAD6,
          ui::DomKey::Constant<'6'>::Character, ui::VKEY_NUMPAD6, flags};
}

constexpr TestKeyEvent Numpad7Pressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::NUMPAD7,
          ui::DomKey::Constant<'7'>::Character, ui::VKEY_NUMPAD7, flags};
}

constexpr TestKeyEvent Numpad8Pressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::NUMPAD8,
          ui::DomKey::Constant<'8'>::Character, ui::VKEY_NUMPAD8, flags};
}

constexpr TestKeyEvent Numpad9Pressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::NUMPAD9,
          ui::DomKey::Constant<'9'>::Character, ui::VKEY_NUMPAD9, flags};
}

constexpr TestKeyEvent NumpadDecimalPressed(
    ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::NUMPAD_DECIMAL,
          ui::DomKey::Constant<'.'>::Character, ui::VKEY_DECIMAL, flags};
}

constexpr TestKeyEvent NumpadInsertPressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::NUMPAD0, ui::DomKey::INSERT,
          ui::VKEY_INSERT, flags};
}

constexpr TestKeyEvent NumpadDeletePressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::NUMPAD_DECIMAL, ui::DomKey::DEL,
          ui::VKEY_DELETE, flags};
}

constexpr TestKeyEvent NumpadEndPressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::NUMPAD1, ui::DomKey::END,
          ui::VKEY_END, flags};
}

constexpr TestKeyEvent NumpadArrowDownPressed(
    ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::NUMPAD2, ui::DomKey::ARROW_DOWN,
          ui::VKEY_DOWN, flags};
}

constexpr TestKeyEvent NumpadPageDownPressed(
    ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::NUMPAD3, ui::DomKey::PAGE_DOWN,
          ui::VKEY_NEXT, flags};
}

constexpr TestKeyEvent NumpadArrowLeftPressed(
    ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::NUMPAD4, ui::DomKey::ARROW_LEFT,
          ui::VKEY_LEFT, flags};
}

constexpr TestKeyEvent NumpadClearPressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::NUMPAD5, ui::DomKey::CLEAR,
          ui::VKEY_CLEAR, flags};
}

constexpr TestKeyEvent NumpadArrowRightPressed(
    ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::NUMPAD6, ui::DomKey::ARROW_RIGHT,
          ui::VKEY_RIGHT, flags};
}

constexpr TestKeyEvent NumpadHomePressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::NUMPAD7, ui::DomKey::HOME,
          ui::VKEY_HOME, flags};
}

constexpr TestKeyEvent NumpadArrowUpPressed(
    ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::NUMPAD8, ui::DomKey::ARROW_UP,
          ui::VKEY_UP, flags};
}

constexpr TestKeyEvent NumpadPageUpPressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::NUMPAD9, ui::DomKey::PAGE_UP,
          ui::VKEY_PRIOR, flags};
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
constexpr TestKeyEvent HangulModePressed(ui::EventFlags flags = ui::EF_NONE) {
  return {ui::ET_KEY_PRESSED, ui::DomCode::ALT_RIGHT, ui::DomKey::HANGUL_MODE,
          ui::VKEY_HANGUL, flags};
}
#endif

std::string EventTypeToString(ui::EventType type) {
  switch (type) {
#define CASE(name) \
  case ui::name:   \
    return #name
    CASE(ET_UNKNOWN);
    CASE(ET_MOUSE_PRESSED);
    CASE(ET_MOUSE_DRAGGED);
    CASE(ET_MOUSE_RELEASED);
    CASE(ET_MOUSE_MOVED);
    CASE(ET_MOUSE_ENTERED);
    CASE(ET_MOUSE_EXITED);
    CASE(ET_KEY_PRESSED);
    CASE(ET_KEY_RELEASED);
    CASE(ET_MOUSEWHEEL);
    CASE(ET_MOUSE_CAPTURE_CHANGED);
    CASE(ET_TOUCH_RELEASED);
    CASE(ET_TOUCH_PRESSED);
    CASE(ET_TOUCH_MOVED);
    CASE(ET_TOUCH_CANCELLED);
    CASE(ET_DROP_TARGET_EVENT);
    CASE(ET_GESTURE_SCROLL_BEGIN);
    CASE(ET_GESTURE_SCROLL_END);
    CASE(ET_GESTURE_SCROLL_UPDATE);
    CASE(ET_GESTURE_TAP);
    CASE(ET_GESTURE_TAP_DOWN);
    CASE(ET_GESTURE_TAP_CANCEL);
    CASE(ET_GESTURE_TAP_UNCONFIRMED);
    CASE(ET_GESTURE_DOUBLE_TAP);
    CASE(ET_GESTURE_BEGIN);
    CASE(ET_GESTURE_END);
    CASE(ET_GESTURE_TWO_FINGER_TAP);
    CASE(ET_GESTURE_PINCH_BEGIN);
    CASE(ET_GESTURE_PINCH_END);
    CASE(ET_GESTURE_PINCH_UPDATE);
    CASE(ET_GESTURE_SHORT_PRESS);
    CASE(ET_GESTURE_LONG_PRESS);
    CASE(ET_GESTURE_LONG_TAP);
    CASE(ET_GESTURE_SWIPE);
    CASE(ET_GESTURE_SHOW_PRESS);
    CASE(ET_SCROLL);
    CASE(ET_SCROLL_FLING_START);
    CASE(ET_SCROLL_FLING_CANCEL);
    CASE(ET_CANCEL_MODE);
    CASE(ET_UMA_DATA);
    CASE(ET_LAST);
#undef CASE
  }
}

std::string KeyEventFlagsToString(ui::EventFlags flags) {
  if (flags == ui::EF_NONE) {
    return "EF_NONE";
  }

  static constexpr struct {
    ui::EventFlags flag;
    const char* name;
  } kFlags[] = {
#define FLAG(flag) {ui::flag, #flag}
      FLAG(EF_IS_SYNTHESIZED), FLAG(EF_SHIFT_DOWN),     FLAG(EF_CONTROL_DOWN),
      FLAG(EF_ALT_DOWN),       FLAG(EF_COMMAND_DOWN),   FLAG(EF_FUNCTION_DOWN),
      FLAG(EF_ALTGR_DOWN),     FLAG(EF_MOD3_DOWN),      FLAG(EF_NUM_LOCK_ON),
      FLAG(EF_CAPS_LOCK_ON),   FLAG(EF_SCROLL_LOCK_ON),
#undef FLAG
  };
  std::string result;
  for (auto [flag, name] : kFlags) {
    if (flags & flag) {
      if (!result.empty()) {
        result.push_back('|');
      }
      result += name;
    }
    flags &= ~flag;
  }
  if (flags) {
    if (!result.empty()) {
      result.push_back('|');
    }
    result += base::StringPrintf("unknown[0x%X]", flags);
  }
  return result;
}

std::string TestKeyEvent::ToString() const {
  return base::StringPrintf(
      "type=%s(%d) "
      "code=%s(0x%06X) "
      "key=%s(0x%08X) "
      "keycode=0x%02X "
      "flags=%s(0x%X) "
      "scan_code=0x%08X",
      EventTypeToString(type).c_str(), type,
      ui::KeycodeConverter::DomCodeToCodeString(code).c_str(),
      static_cast<uint32_t>(code),
      ui::KeycodeConverter::DomKeyToKeyString(key).c_str(),
      static_cast<uint32_t>(key), keycode, KeyEventFlagsToString(flags).c_str(),
      flags, scan_code);
}

inline std::ostream& operator<<(std::ostream& os, const TestKeyEvent& event) {
  return os << event.ToString();
}

inline bool operator==(const TestKeyEvent& e1, const TestKeyEvent& e2) {
  return e1.type == e2.type && e1.code == e2.code && e1.key == e2.key &&
         e1.keycode == e2.keycode && e1.flags == e2.flags &&
         e1.scan_code == e2.scan_code;
}

// Keyboard representation in tests.
struct TestKeyboard {
  const char* name;
  const char* layout;
  ui::InputDeviceType type;
  bool has_custom_top_row;
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
    kInternalChromeKeyboard,  kInternalChromeCustomLayoutKeyboard,
    kExternalChromeKeyboard,  kExternalChromeCustomLayoutKeyboard,
    kExternalGenericKeyboard, kExternalAppleKeyboard,
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

class EventRewriterTest : public ChromeAshTestBase {
 public:
  EventRewriterTest()
      : fake_user_manager_(new FakeChromeUserManager),
        user_manager_enabler_(base::WrapUnique(fake_user_manager_.get())) {}
  ~EventRewriterTest() override {}

  void SetUp() override {
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
    event_rewriter_ash_ = std::make_unique<ui::EventRewriterAsh>(
        delegate_.get(), keyboard_capability_.get(),
        Shell::Get()->sticky_keys_controller(), false, &fake_ime_keyboard_);

    source_.AddEventRewriter(keyboard_device_id_event_rewriter_.get());
    source_.AddEventRewriter(event_rewriter_ash_.get());
  }

  void TearDown() override {
    source_.RemoveEventRewriter(event_rewriter_ash_.get());
    source_.RemoveEventRewriter(keyboard_device_id_event_rewriter_.get());

    event_rewriter_ash_.reset();
    keyboard_device_id_event_rewriter_.reset();

    input_device_settings_controller_mock_.reset();
    input_device_settings_controller_resetter_.reset();
    ChromeAshTestBase::TearDown();
    // Shutdown() deletes the IME mock object.
    input_method::Shutdown();
  }

  ui::test::TestEventSource& source() { return source_; }

 protected:
  absl::optional<TestKeyEvent> RunRewriter(const TestKeyEvent& test_key_event,
                                           int device_id = kKeyboardDeviceId) {
    ui::KeyEvent event(test_key_event.type, test_key_event.keycode,
                       test_key_event.code, test_key_event.flags,
                       test_key_event.key, ui::EventTimeForNow());
    event.set_scan_code(test_key_event.scan_code);
    event.set_source_device_id(device_id);
    source().Send(&event);

    auto events =
        static_cast<TestEventSink*>(source().GetEventSink())->TakeEvents();
    if (events.empty()) {
      return absl::nullopt;
    }
    auto* key_event = events[0]->AsKeyEvent();
    return {{key_event->type(), key_event->code(), key_event->GetDomKey(),
             key_event->key_code(), key_event->flags(),
             key_event->scan_code()}};
  }

  // Parameterized version of test depending on feature flag values. The feature
  // kUseSearchClickForRightClick determines if this should test for alt-click
  // or search-click.
  void DontRewriteIfNotRewritten(int right_click_flags);

  ui::MouseEvent RewriteMouseButtonEvent(const ui::MouseEvent& event) {
    TestEventRewriterContinuation continuation;
    event_rewriter_ash_->RewriteMouseButtonEventForTesting(
        event, continuation.weak_ptr_factory_.GetWeakPtr());
    if (!continuation.rewritten_events.empty())
      return ui::MouseEvent(*continuation.rewritten_events[0]->AsMouseEvent());
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
    const ui::KeyboardDevice keyboard(kKeyboardDeviceId, test_keyboard.type,
                                      test_keyboard.name,
                                      /*phys=*/"", base::FilePath(kKbdSysPath),
                                      /*vendor=*/-1,
                                      /*product=*/-1, /*version=*/-1);

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
                             /*subsystem=*/"input", /*devnode=*/absl::nullopt,
                             /*devtype=*/absl::nullopt,
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

  void SendKeyEvent(ui::EventType type,
                    ui::KeyboardCode key_code,
                    ui::DomCode code,
                    ui::DomKey key,
                    int flags) {
    ui::KeyEvent press(type, key_code, code, flags, key, ui::EventTimeForNow());
    ui::EventDispatchDetails details = source_.Send(&press);
    CHECK(!details.dispatcher_destroyed);
  }

  void SendActivateStickyKeyPattern(ui::KeyboardCode key_code,
                                    ui::DomCode code,
                                    ui::DomKey key) {
    SendKeyEvent(ui::ET_KEY_PRESSED, key_code, code, key, ui::EF_NONE);
    SendKeyEvent(ui::ET_KEY_RELEASED, key_code, code, key, ui::EF_NONE);
  }

  void ClearNotifications() {
    message_center_.RemoveAllNotifications(
        false, message_center::FakeMessageCenter::RemoveType::ALL);
    deprecation_controller_->ResetStateForTesting();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<FakeChromeUserManager, DanglingUntriaged | ExperimentalAsh>
      fake_user_manager_;  // Not owned.
  user_manager::ScopedUserManager user_manager_enabler_;
  raw_ptr<input_method::MockInputMethodManagerImpl,
          DanglingUntriaged | ExperimentalAsh>
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
  std::unique_ptr<ui::KeyboardCapability> keyboard_capability_;
  input_method::FakeImeKeyboard fake_ime_keyboard_;
  std::unique_ptr<ui::KeyboardDeviceIdEventRewriter>
      keyboard_device_id_event_rewriter_;
  std::unique_ptr<ui::EventRewriterAsh> event_rewriter_ash_;
  TestEventSink sink_;
  ui::test::TestEventSource source_{&sink_};
  message_center::FakeMessageCenter message_center_;
  raw_ptr<DeprecationNotificationController, ExperimentalAsh>
      deprecation_controller_;  // Not owned.
  raw_ptr<InputDeviceSettingsNotificationController, ExperimentalAsh>
      input_device_settings_notification_controller_;  // Not owned.
};

// TestKeyRewriteLatency checks that the event rewriter
// publishes a latency metric every time a key is pressed.
TEST_F(EventRewriterTest, TestKeyRewriteLatency) {
  base::HistogramTester histogram_tester;
  EXPECT_EQ(BPressed(ui::EF_CONTROL_DOWN),
            RunRewriter(BPressed(ui::EF_CONTROL_DOWN)));
  EXPECT_EQ(BPressed(ui::EF_CONTROL_DOWN),
            RunRewriter(BPressed(ui::EF_CONTROL_DOWN)));
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Inputs.EventRewriter.KeyRewriteLatency", 2);
}

TEST_F(EventRewriterTest, TestRewriteCommandToControl) {
  // This test is not useful once device settings split is launched.
  scoped_feature_list_.InitAndDisableFeature(
      features::kInputDeviceSettingsSplit);

  // First, test non Apple keyboards, they should all behave the same.
  for (const auto& keyboard : kNonAppleKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    // VKEY_A, Alt modifier.
    EXPECT_EQ(UnidentifiedAPressed(ui::EF_ALT_DOWN),
              RunRewriter(UnidentifiedAPressed(ui::EF_ALT_DOWN)));

    // VKEY_A, Win modifier.
    EXPECT_EQ(UnidentifiedAPressed(ui::EF_COMMAND_DOWN),
              RunRewriter(UnidentifiedAPressed(ui::EF_COMMAND_DOWN)));

    // VKEY_A, Alt+Win modifier.
    EXPECT_EQ(UnidentifiedAPressed(ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN),
              RunRewriter(
                  UnidentifiedAPressed(ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN)));

    // VKEY_LWIN (left Windows key), Alt modifier.
    EXPECT_EQ(LWinPressed(ui::EF_ALT_DOWN),
              RunRewriter(LWinPressed(ui::EF_ALT_DOWN)));

    // VKEY_RWIN (right Windows key), Alt modifier.
    EXPECT_EQ(RWinPressed(ui::EF_ALT_DOWN),
              RunRewriter(RWinPressed(ui::EF_ALT_DOWN)));
  }

  // Simulate the default initialization of the Apple Command key remap pref to
  // Ctrl.
  Preferences::RegisterProfilePrefs(prefs()->registry());
  {
    SCOPED_TRACE(kExternalAppleKeyboard.name);
    SetUpKeyboard(kExternalAppleKeyboard);

    // VKEY_A, Alt modifier.
    EXPECT_EQ(UnidentifiedAPressed(ui::EF_ALT_DOWN),
              RunRewriter(UnidentifiedAPressed(ui::EF_ALT_DOWN)));

    // VKEY_A, Win modifier.
    EXPECT_EQ(APressed(ui::EF_CONTROL_DOWN),
              RunRewriter(UnidentifiedAPressed(ui::EF_COMMAND_DOWN)));

    // VKEY_A, Alt+Win modifier.
    EXPECT_EQ(APressed(ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN),
              RunRewriter(
                  UnidentifiedAPressed(ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN)));

    // VKEY_LWIN (left Windows key), Alt modifier.
    EXPECT_EQ(LControlPressed(ui::EF_ALT_DOWN),
              RunRewriter(LWinPressed(ui::EF_ALT_DOWN)));

    // VKEY_RWIN (right Windows key), Alt modifier.
    EXPECT_EQ(RControlPressed(ui::EF_ALT_DOWN),
              RunRewriter(RWinPressed(ui::EF_ALT_DOWN)));
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
    EXPECT_EQ(UnidentifiedAPressed(ui::EF_ALT_DOWN),
              RunRewriter(UnidentifiedAPressed(ui::EF_ALT_DOWN)));

    // VKEY_A, Win modifier.
    EXPECT_EQ(UnidentifiedAPressed(ui::EF_COMMAND_DOWN),
              RunRewriter(UnidentifiedAPressed(ui::EF_COMMAND_DOWN)));

    // VKEY_A, Alt+Win modifier.
    EXPECT_EQ(UnidentifiedAPressed(ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN),
              RunRewriter(
                  UnidentifiedAPressed(ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN)));

    // VKEY_LWIN (left Windows key), Alt modifier.
    EXPECT_EQ(LWinPressed(ui::EF_ALT_DOWN),
              RunRewriter(LWinPressed(ui::EF_ALT_DOWN)));

    // TODO(b/312578988): This should be an identity transformation with
    // RWinPressed as both the before and after event.
    // VKEY_RWIN (right Windows key), Alt modifier.
    EXPECT_EQ(TestKeyEvent(ui::ET_KEY_PRESSED, ui::DomCode::META_RIGHT,
                           ui::DomKey::META, ui::VKEY_LWIN,
                           ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN),
              RunRewriter(RWinPressed(ui::EF_ALT_DOWN)));
  }
}

TEST_F(EventRewriterTest, ModifiersNotRemappedWhenSuppressed) {
  // Remap Control -> Alt.
  Preferences::RegisterProfilePrefs(prefs()->registry());
  IntegerPrefMember control;
  InitModifierKeyPref(&control, ::prefs::kLanguageRemapControlKeyTo,
                      ui::mojom::ModifierKey::kControl,
                      ui::mojom::ModifierKey::kAlt);

  // Pressing Control + B should now be remapped to Alt + B.
  delegate_->SuppressModifierKeyRewrites(false);
  EXPECT_EQ(BPressed(ui::EF_ALT_DOWN),
            RunRewriter(BPressed(ui::EF_CONTROL_DOWN)));

  // Pressing Control + B should no longer be remapped.
  delegate_->SuppressModifierKeyRewrites(true);
  EXPECT_EQ(BPressed(ui::EF_CONTROL_DOWN),
            RunRewriter(BPressed(ui::EF_CONTROL_DOWN)));
}

TEST_F(EventRewriterTest, TestRewriteExternalMetaKey) {
  // This test is irrelevant once input device settings split launches.
  scoped_feature_list_.InitAndDisableFeature(
      features::kInputDeviceSettingsSplit);

  // Simulate the default initialization of the Meta key on external keyboards
  // remap pref to Search.
  Preferences::RegisterProfilePrefs(prefs()->registry());

  // By default, the Meta key on all keyboards, internal, external Chrome OS
  // branded keyboards, and Generic keyboards should produce Search.
  for (const auto& keyboard : kNonAppleKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    // VKEY_A, Win modifier.
    EXPECT_EQ(UnidentifiedAPressed(ui::EF_COMMAND_DOWN),
              RunRewriter(UnidentifiedAPressed(ui::EF_COMMAND_DOWN)));

    // VKEY_A, Alt+Win modifier.
    EXPECT_EQ(UnidentifiedAPressed(ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN),
              RunRewriter(
                  UnidentifiedAPressed(ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN)));

    // VKEY_LWIN (left Windows key), Alt modifier.
    EXPECT_EQ(LWinPressed(ui::EF_ALT_DOWN),
              RunRewriter(LWinPressed(ui::EF_ALT_DOWN)));

    // TODO(b/312578988): This should be an identity transformation with
    // RWinPressed as both the before and after event.
    // VKEY_RWIN (right Windows key), Alt modifier.
    EXPECT_EQ(TestKeyEvent(ui::ET_KEY_PRESSED, ui::DomCode::META_RIGHT,
                           ui::DomKey::META, ui::VKEY_LWIN,
                           ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN),
              RunRewriter(RWinPressed(ui::EF_ALT_DOWN)));
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
    EXPECT_EQ(APressed(ui::EF_CONTROL_DOWN),
              RunRewriter(UnidentifiedAPressed(ui::EF_COMMAND_DOWN)));

    // VKEY_A, Alt+Win modifier.
    EXPECT_EQ(APressed(ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN),
              RunRewriter(
                  UnidentifiedAPressed(ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN)));

    // VKEY_LWIN (left Windows key), Alt modifier.
    EXPECT_EQ(LControlPressed(ui::EF_ALT_DOWN),
              RunRewriter(LWinPressed(ui::EF_ALT_DOWN)));

    // VKEY_RWIN (right Windows key), Alt modifier.
    EXPECT_EQ(RControlPressed(ui::EF_ALT_DOWN),
              RunRewriter(RWinPressed(ui::EF_ALT_DOWN)));
  }

  SetUpKeyboard(kExternalGenericKeyboard);
  // VKEY_A, Win modifier.
  EXPECT_EQ(APressed(ui::EF_ALT_DOWN),
            RunRewriter(UnidentifiedAPressed(ui::EF_COMMAND_DOWN)));

  // VKEY_A, Alt+Win modifier.
  EXPECT_EQ(
      APressed(ui::EF_ALT_DOWN),
      RunRewriter(UnidentifiedAPressed(ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN)));

  // VKEY_LWIN (left Windows key), Alt modifier.
  EXPECT_EQ(LAltPressed(ui::EF_ALT_DOWN),
            RunRewriter(LWinPressed(ui::EF_ALT_DOWN)));

  // VKEY_RWIN (right Windows key), Alt modifier.
  EXPECT_EQ(RAltPressed(ui::EF_ALT_DOWN),
            RunRewriter(RWinPressed(ui::EF_ALT_DOWN)));
}

// For crbug.com/133896.
TEST_F(EventRewriterTest, TestRewriteCommandToControlWithControlRemapped) {
  // This test is irrelevant once input device settings split launches.
  scoped_feature_list_.InitAndDisableFeature(
      features::kInputDeviceSettingsSplit);

  // Remap Control to Alt.
  Preferences::RegisterProfilePrefs(prefs()->registry());
  IntegerPrefMember control;
  InitModifierKeyPref(&control, ::prefs::kLanguageRemapControlKeyTo,
                      ui::mojom::ModifierKey::kControl,
                      ui::mojom::ModifierKey::kAlt);

  for (const auto& keyboard : kNonAppleKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    EXPECT_EQ(LAltPressed(), RunRewriter(LControlPressed()));
  }

  // Now verify that remapping does not affect Apple keyboard.
  SetUpKeyboard(kExternalAppleKeyboard);

  // VKEY_LWIN (left Command key) with  Alt modifier. The remapped Command
  // key should never be re-remapped to Alt.
  EXPECT_EQ(LControlPressed(ui::EF_ALT_DOWN),
            RunRewriter(LWinPressed(ui::EF_ALT_DOWN)));

  // VKEY_RWIN (right Command key) with  Alt modifier. The remapped Command
  // key should never be re-remapped to Alt.
  EXPECT_EQ(RControlPressed(ui::EF_ALT_DOWN),
            RunRewriter(RWinPressed(ui::EF_ALT_DOWN)));
}

TEST_F(EventRewriterTest, TestRewriteNumPadKeys) {
  // Even if most Chrome OS keyboards do not have numpad, they should still
  // handle it the same way as generic PC keyboards.
  for (const auto& keyboard : kNonAppleKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    // XK_KP_Insert (= NumPad 0 without Num Lock), no modifier.
    EXPECT_EQ(Numpad0Pressed(), RunRewriter(NumpadInsertPressed()));

    // XK_KP_Insert (= NumPad 0 without Num Lock), Alt modifier.
    EXPECT_EQ(Numpad0Pressed(ui::EF_ALT_DOWN),
              RunRewriter(NumpadInsertPressed(ui::EF_ALT_DOWN)));

    // XK_KP_Delete (= NumPad . without Num Lock), Alt modifier.
    EXPECT_EQ(NumpadDecimalPressed(ui::EF_ALT_DOWN),
              RunRewriter(NumpadDeletePressed(ui::EF_ALT_DOWN)));

    // XK_KP_End (= NumPad 1 without Num Lock), Alt modifier.
    EXPECT_EQ(Numpad1Pressed(ui::EF_ALT_DOWN),
              RunRewriter(NumpadEndPressed(ui::EF_ALT_DOWN)));

    // XK_KP_Down (= NumPad 2 without Num Lock), Alt modifier.
    EXPECT_EQ(Numpad2Pressed(ui::EF_ALT_DOWN),
              RunRewriter(NumpadArrowDownPressed(ui::EF_ALT_DOWN)));

    // XK_KP_Next (= NumPad 3 without Num Lock), Alt modifier.
    EXPECT_EQ(Numpad3Pressed(ui::EF_ALT_DOWN),
              RunRewriter(NumpadPageDownPressed(ui::EF_ALT_DOWN)));

    // XK_KP_Left (= NumPad 4 without Num Lock), Alt modifier.
    EXPECT_EQ(Numpad4Pressed(ui::EF_ALT_DOWN),
              RunRewriter(NumpadArrowLeftPressed(ui::EF_ALT_DOWN)));

    // XK_KP_Begin (= NumPad 5 without Num Lock), Alt modifier.
    EXPECT_EQ(Numpad5Pressed(ui::EF_ALT_DOWN),
              RunRewriter(NumpadClearPressed(ui::EF_ALT_DOWN)));

    // XK_KP_Right (= NumPad 6 without Num Lock), Alt modifier.
    EXPECT_EQ(Numpad6Pressed(ui::EF_ALT_DOWN),
              RunRewriter(NumpadArrowRightPressed(ui::EF_ALT_DOWN)));

    // XK_KP_Home (= NumPad 7 without Num Lock), Alt modifier.
    EXPECT_EQ(Numpad7Pressed(ui::EF_ALT_DOWN),
              RunRewriter(NumpadHomePressed(ui::EF_ALT_DOWN)));

    // XK_KP_Up (= NumPad 8 without Num Lock), Alt modifier.
    EXPECT_EQ(Numpad8Pressed(ui::EF_ALT_DOWN),
              RunRewriter(NumpadArrowUpPressed(ui::EF_ALT_DOWN)));

    // XK_KP_Prior (= NumPad 9 without Num Lock), Alt modifier.
    EXPECT_EQ(Numpad9Pressed(ui::EF_ALT_DOWN),
              RunRewriter(NumpadPageUpPressed(ui::EF_ALT_DOWN)));

    // XK_KP_{N} (= NumPad {N} with Num Lock), Num Lock modifier.
    EXPECT_EQ(Numpad0Pressed(), RunRewriter(Numpad0Pressed()));
    EXPECT_EQ(Numpad1Pressed(), RunRewriter(Numpad1Pressed()));
    EXPECT_EQ(Numpad2Pressed(), RunRewriter(Numpad2Pressed()));
    EXPECT_EQ(Numpad3Pressed(), RunRewriter(Numpad3Pressed()));
    EXPECT_EQ(Numpad4Pressed(), RunRewriter(Numpad4Pressed()));
    EXPECT_EQ(Numpad5Pressed(), RunRewriter(Numpad5Pressed()));
    EXPECT_EQ(Numpad6Pressed(), RunRewriter(Numpad6Pressed()));
    EXPECT_EQ(Numpad7Pressed(), RunRewriter(Numpad7Pressed()));
    EXPECT_EQ(Numpad8Pressed(), RunRewriter(Numpad8Pressed()));
    EXPECT_EQ(Numpad9Pressed(), RunRewriter(Numpad9Pressed()));

    // XK_KP_DECIMAL (= NumPad . with Num Lock), Num Lock modifier.
    EXPECT_EQ(NumpadDecimalPressed(), RunRewriter(NumpadDecimalPressed()));
  }
}

// Tests if the rewriter can handle a Command + Num Pad event.
TEST_F(EventRewriterTest, TestRewriteNumPadKeysOnAppleKeyboard) {
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
  EXPECT_EQ(Numpad1Pressed(ui::EF_CONTROL_DOWN),
            RunRewriter(NumpadEndPressed(ui::EF_COMMAND_DOWN)));

  // XK_KP_1 (= NumPad 1 with Num Lock), Win modifier.
  // The result should also be "Num Pad 1 with Control + Num Lock
  // modifiers".
  EXPECT_EQ(Numpad1Pressed(ui::EF_CONTROL_DOWN),
            RunRewriter(Numpad1Pressed(ui::EF_COMMAND_DOWN)));
}

TEST_F(EventRewriterTest, TestRewriteModifiersNoRemap) {
  for (const auto& keyboard : kAllKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    // Press Search. Confirm the event is not rewritten.
    EXPECT_EQ(LWinPressed(), RunRewriter(LWinPressed()));

    // Press left Control. Confirm the event is not rewritten.
    EXPECT_EQ(LControlPressed(), RunRewriter(LControlPressed()));

    // Press right Control. Confirm the event is not rewritten.
    EXPECT_EQ(RControlPressed(), RunRewriter(RControlPressed()));

    // Press left Alt. Confirm the event is not rewritten.
    EXPECT_EQ(LAltPressed(), RunRewriter(LAltPressed()));

    // Press right Alt. Confirm the event is not rewritten.
    EXPECT_EQ(RAltPressed(), RunRewriter(RAltPressed()));

    // Test KeyRelease event, just in case.
    // Release Search. Confirm the release event is not rewritten.
    EXPECT_EQ(LWinReleased(), RunRewriter(LWinReleased()));
  }
}

TEST_F(EventRewriterTest, TestRewriteModifiersNoRemapMultipleKeys) {
  for (const auto& keyboard : kAllKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    // Press Alt with Shift. Confirm the event is not rewritten.
    EXPECT_EQ(LAltPressed(ui::EF_SHIFT_DOWN),
              RunRewriter(LAltPressed(ui::EF_SHIFT_DOWN)));

    // Press Escape with Alt and Shift. Confirm the event is not rewritten.
    EXPECT_EQ(EscapePressed(ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN),
              RunRewriter(EscapePressed(ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN)));

    // Press Search with Caps Lock mask. Confirm the event is not rewritten.
    EXPECT_EQ(LWinPressed(ui::EF_CAPS_LOCK_ON),
              RunRewriter(LWinPressed(ui::EF_CAPS_LOCK_ON)));

    // Release Search with Caps Lock mask. Confirm the event is not rewritten.
    EXPECT_EQ(LWinReleased(ui::EF_CAPS_LOCK_ON),
              RunRewriter(LWinReleased(ui::EF_CAPS_LOCK_ON)));

    // Press Shift+Ctrl+Alt+Search+Escape. Confirm the event is not rewritten.
    EXPECT_EQ(
        EscapePressed(ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN |
                      ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN),
        RunRewriter(EscapePressed(ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN |
                                  ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN)));

    // Press Shift+Ctrl+Alt+Search+B. Confirm the event is not rewritten.
    EXPECT_EQ(BPressed(ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN |
                       ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN),
              RunRewriter(BPressed(ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN |
                                   ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN)));
  }
}

TEST_F(EventRewriterTest, TestRewriteModifiersDisableSome) {
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
    EXPECT_EQ(LAltPressed(ui::EF_SHIFT_DOWN),
              RunRewriter(LAltPressed(ui::EF_SHIFT_DOWN)));

    // Press Search. Confirm the event is now VKEY_UNKNOWN.
    EXPECT_EQ(UnknownPressed(), RunRewriter(LWinPressed()));

    // Press Control. Confirm the event is now VKEY_UNKNOWN.
    EXPECT_EQ(UnknownPressed(), RunRewriter(LControlPressed()));

    // Press Escape. Confirm the event is now VKEY_UNKNOWN.
    EXPECT_EQ(UnknownPressed(), RunRewriter(EscapePressed()));

    // Press Control+Search. Confirm the event is now VKEY_UNKNOWN
    // without any modifiers.
    EXPECT_EQ(UnknownPressed(), RunRewriter(LWinPressed(ui::EF_CONTROL_DOWN)));

    // Press Control+Search+a. Confirm the event is now VKEY_A without any
    // modifiers.
    EXPECT_EQ(APressed(), RunRewriter(APressed(ui::EF_CONTROL_DOWN)));

    // Press Control+Search+Alt+a. Confirm the event is now VKEY_A only with
    // the Alt modifier.
    EXPECT_EQ(APressed(ui::EF_ALT_DOWN),
              RunRewriter(APressed(ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN)));
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
    EXPECT_EQ(LControlPressed(), RunRewriter(LAltPressed()));

    // Press Alt+a. Confirm the event is now Control+a even though the Control
    // key itself is disabled.
    EXPECT_EQ(APressed(ui::EF_CONTROL_DOWN),
              RunRewriter(APressed(ui::EF_ALT_DOWN)));
  }
}

TEST_F(EventRewriterTest, TestRewriteModifiersRemapToControl) {
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
    EXPECT_EQ(LControlPressed(), RunRewriter(LWinPressed()));
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
    EXPECT_EQ(LControlPressed(), RunRewriter(LAltPressed()));

    // Press Alt+Search. Confirm the event is now VKEY_CONTROL.
    EXPECT_EQ(LControlPressed(), RunRewriter(LWinPressed(ui::EF_ALT_DOWN)));

    // Press Control+Alt+Search. Confirm the event is now VKEY_CONTROL.
    EXPECT_EQ(LControlPressed(),
              RunRewriter(LWinPressed(ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN)));

    // Press Shift+Control+Alt+Search. Confirm the event is now Control with
    // Shift and Control modifiers.
    EXPECT_EQ(LControlPressed(ui::EF_SHIFT_DOWN),
              RunRewriter(LWinPressed(ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN |
                                      ui::EF_ALT_DOWN)));

    // Press Shift+Control+Alt+Search+B. Confirm the event is now B with Shift
    // and Control modifiers.
    EXPECT_EQ(BPressed(ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN),
              RunRewriter(BPressed(ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN |
                                   ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN)));
  }
}

TEST_F(EventRewriterTest, TestRewriteModifiersRemapToEscape) {
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
    EXPECT_EQ(EscapePressed(), RunRewriter(LWinPressed()));
  }
}

TEST_F(EventRewriterTest, TestRewriteModifiersRemapEscapeToAlt) {
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
    EXPECT_EQ(LAltPressed(), RunRewriter(EscapePressed()));
    // Release Escape to clear flags.
    EXPECT_EQ(LAltReleased(), RunRewriter(EscapeReleased()));
  }
}

TEST_F(EventRewriterTest, TestRewriteModifiersRemapAltToControl) {
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
    EXPECT_EQ(LControlPressed(), RunRewriter(LAltPressed()));

    // Press Shift+comma. Verify that only the flags are changed.
    EXPECT_EQ(CommaPressed(ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN),
              RunRewriter(CommaPressed(ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN)));

    // Press Shift+9. Verify that only the flags are changed.
    EXPECT_EQ(Digit9Pressed(ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN),
              RunRewriter(Digit9Pressed(ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN)));
  }
}

TEST_F(EventRewriterTest, TestRewriteModifiersRemapUnderEscapeControlAlt) {
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
    EXPECT_EQ(LWinPressed(), RunRewriter(LControlPressed()));

    // Then, press all of the three, Control+Alt+Escape.
    EXPECT_EQ(
        LAltPressed(ui::EF_CONTROL_DOWN | ui::EF_COMMAND_DOWN),
        RunRewriter(EscapePressed(ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN)));

    // Press Shift+Control+Alt+Escape.
    EXPECT_EQ(LAltPressed(ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN |
                          ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN),
              RunRewriter(EscapePressed(
                  ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN)));

    // Press Shift+Control+Alt+B
    EXPECT_EQ(BPressed(ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN |
                       ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN),
              RunRewriter(BPressed(ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN |
                                   ui::EF_ALT_DOWN)));
  }
}

TEST_F(EventRewriterTest,
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
    EXPECT_EQ(LWinPressed(), RunRewriter(LControlPressed()));
    EXPECT_EQ(LAltPressed(), RunRewriter(EscapePressed()));
    EXPECT_EQ(LWinReleased(ui::EF_ALT_DOWN), RunRewriter(LControlReleased()));
    EXPECT_EQ(LAltReleased(), RunRewriter(EscapeReleased()));

    // Press Search. Confirm the event is now VKEY_BACK.
    EXPECT_EQ(BackspacePressed(), RunRewriter(LWinPressed()));
  }
}

TEST_F(EventRewriterTest, TestRewriteModifiersRemapBackspaceToEscape) {
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
    EXPECT_EQ(EscapePressed(), RunRewriter(BackspacePressed()));
  }
}

TEST_F(EventRewriterTest,
       TestRewriteNonModifierToModifierWithRemapBetweenKeyEvents) {
  // Remap Escape to Alt.
  Preferences::RegisterProfilePrefs(prefs()->registry());
  IntegerPrefMember escape;
  InitModifierKeyPref(&escape, ::prefs::kLanguageRemapEscapeKeyTo,
                      ui::mojom::ModifierKey::kEscape,
                      ui::mojom::ModifierKey::kAlt);

  SetUpKeyboard(kInternalChromeKeyboard);

  // Press Escape.
  EXPECT_EQ(LAltPressed(), RunRewriter(EscapePressed()));

  // Remap Escape to Control before releasing Escape.
  InitModifierKeyPref(&escape, ::prefs::kLanguageRemapEscapeKeyTo,
                      ui::mojom::ModifierKey::kEscape,
                      ui::mojom::ModifierKey::kControl);

  // Release Escape.
  EXPECT_EQ(EscapeReleased(), RunRewriter(EscapeReleased()));

  // Press A, expect that Alt is not stickied.
  EXPECT_EQ(APressed(), RunRewriter(APressed()));

  // Release A.
  EXPECT_EQ(AReleased(), RunRewriter(AReleased()));
}

TEST_F(EventRewriterTest, TestRewriteModifiersRemapToCapsLock) {
  // Remap Search to Caps Lock.
  Preferences::RegisterProfilePrefs(prefs()->registry());
  IntegerPrefMember search;
  InitModifierKeyPref(&search, ::prefs::kLanguageRemapSearchKeyTo,
                      ui::mojom::ModifierKey::kMeta,
                      ui::mojom::ModifierKey::kCapsLock);

  SetUpKeyboard(kInternalChromeKeyboard);
  EXPECT_FALSE(fake_ime_keyboard_.IsCapsLockEnabled());

  // Press Search.
  EXPECT_EQ(CapsLockPressed(ui::EF_CAPS_LOCK_ON), RunRewriter(LWinPressed()));
  EXPECT_TRUE(fake_ime_keyboard_.IsCapsLockEnabled());

  // Release Search.
  EXPECT_EQ(CapsLockReleased(), RunRewriter(LWinReleased()));
  EXPECT_TRUE(fake_ime_keyboard_.IsCapsLockEnabled());

  // Press Search.
  EXPECT_EQ(CapsLockPressed(ui::EF_CAPS_LOCK_ON),
            RunRewriter(LWinPressed(ui::EF_CAPS_LOCK_ON)));
  EXPECT_FALSE(fake_ime_keyboard_.IsCapsLockEnabled());

  // Release Search.
  EXPECT_EQ(CapsLockReleased(), RunRewriter(LWinReleased()));
  EXPECT_FALSE(fake_ime_keyboard_.IsCapsLockEnabled());

  // Do the same on external Chrome OS keyboard.
  SetUpKeyboard(kExternalChromeKeyboard);

  // Press Search.
  EXPECT_EQ(CapsLockPressed(ui::EF_CAPS_LOCK_ON), RunRewriter(LWinPressed()));
  EXPECT_TRUE(fake_ime_keyboard_.IsCapsLockEnabled());

  // Release Search.
  EXPECT_EQ(CapsLockReleased(), RunRewriter(LWinReleased()));
  EXPECT_TRUE(fake_ime_keyboard_.IsCapsLockEnabled());

  // Press Search.
  EXPECT_EQ(CapsLockPressed(ui::EF_CAPS_LOCK_ON),
            RunRewriter(LWinPressed(ui::EF_CAPS_LOCK_ON)));
  EXPECT_FALSE(fake_ime_keyboard_.IsCapsLockEnabled());

  // Release Search.
  EXPECT_EQ(CapsLockReleased(), RunRewriter(LWinReleased()));
  EXPECT_FALSE(fake_ime_keyboard_.IsCapsLockEnabled());

  // Try external keyboard with Caps Lock.
  SetUpKeyboard(kExternalGenericKeyboard);

  // Press Caps Lock.
  EXPECT_EQ(CapsLockPressed(ui::EF_CAPS_LOCK_ON),
            RunRewriter(CapsLockPressed(ui::EF_CAPS_LOCK_ON)));
  EXPECT_TRUE(fake_ime_keyboard_.IsCapsLockEnabled());

  // Release Caps Lock.
  EXPECT_EQ(CapsLockReleased(),
            RunRewriter(CapsLockReleased(ui::EF_CAPS_LOCK_ON)));
  EXPECT_TRUE(fake_ime_keyboard_.IsCapsLockEnabled());
}

TEST_F(EventRewriterTest, TestRewriteCapsLock) {
  Preferences::RegisterProfilePrefs(prefs()->registry());

  SetUpKeyboard(kExternalGenericKeyboard);
  EXPECT_FALSE(fake_ime_keyboard_.IsCapsLockEnabled());

  // On Chrome OS, CapsLock is mapped to CapsLock with Mod3Mask.
  EXPECT_EQ(CapsLockPressed(ui::EF_CAPS_LOCK_ON),
            RunRewriter(CapsLockPressed()));
  EXPECT_TRUE(fake_ime_keyboard_.IsCapsLockEnabled());

  EXPECT_EQ(CapsLockReleased(),
            RunRewriter(CapsLockReleased(ui::EF_CAPS_LOCK_ON)));
  EXPECT_TRUE(fake_ime_keyboard_.IsCapsLockEnabled());

  // Remap Caps Lock to Control.
  IntegerPrefMember caps_lock;
  InitModifierKeyPref(&caps_lock, ::prefs::kLanguageRemapCapsLockKeyTo,
                      ui::mojom::ModifierKey::kCapsLock,
                      ui::mojom::ModifierKey::kControl);

  // Press Caps Lock. CapsLock is enabled but we have remapped the key to
  // now be Control. We want to ensure that the CapsLock modifier is still
  // active even after pressing the remapped Capslock key.
  EXPECT_EQ(LControlPressed(ui::EF_CAPS_LOCK_ON),
            RunRewriter(CapsLockPressed(ui::EF_CAPS_LOCK_ON)));
  EXPECT_TRUE(fake_ime_keyboard_.IsCapsLockEnabled());

  // Release Caps Lock.
  EXPECT_EQ(LControlReleased(ui::EF_CAPS_LOCK_ON),
            RunRewriter(CapsLockReleased(ui::EF_CAPS_LOCK_ON)));
  EXPECT_TRUE(fake_ime_keyboard_.IsCapsLockEnabled());
}

TEST_F(EventRewriterTest, TestRewriteExternalCapsLockWithDifferentScenarios) {
  Preferences::RegisterProfilePrefs(prefs()->registry());

  SetUpKeyboard(kExternalGenericKeyboard);
  EXPECT_FALSE(fake_ime_keyboard_.IsCapsLockEnabled());

  // Turn on CapsLock.
  EXPECT_EQ(CapsLockPressed(ui::EF_CAPS_LOCK_ON),
            RunRewriter(CapsLockPressed()));
  EXPECT_TRUE(fake_ime_keyboard_.IsCapsLockEnabled());

  EXPECT_EQ(CapsLockReleased(),
            RunRewriter(CapsLockReleased(ui::EF_CAPS_LOCK_ON)));
  EXPECT_TRUE(fake_ime_keyboard_.IsCapsLockEnabled());

  // Remap CapsLock to Search.
  IntegerPrefMember search;
  InitModifierKeyPref(&search, ::prefs::kLanguageRemapCapsLockKeyTo,
                      ui::mojom::ModifierKey::kCapsLock,
                      ui::mojom::ModifierKey::kMeta);

  // Now that CapsLock is enabled, press the remapped CapsLock button again
  // and expect to not disable CapsLock.
  EXPECT_EQ(LWinPressed(ui::EF_CAPS_LOCK_ON),
            RunRewriter(CapsLockPressed(ui::EF_CAPS_LOCK_ON)));
  EXPECT_TRUE(fake_ime_keyboard_.IsCapsLockEnabled());

  EXPECT_EQ(LWinReleased(ui::EF_CAPS_LOCK_ON),
            RunRewriter(CapsLockReleased(ui::EF_CAPS_LOCK_ON)));
  EXPECT_TRUE(fake_ime_keyboard_.IsCapsLockEnabled());

  // Remap CapsLock key back to CapsLock.
  IntegerPrefMember capslock;
  InitModifierKeyPref(&capslock, ::prefs::kLanguageRemapCapsLockKeyTo,
                      ui::mojom::ModifierKey::kCapsLock,
                      ui::mojom::ModifierKey::kCapsLock);

  // Now press CapsLock again and now expect that the CapsLock modifier is
  // removed and the key is disabled.
  EXPECT_EQ(CapsLockPressed(ui::EF_CAPS_LOCK_ON),
            RunRewriter(CapsLockPressed(ui::EF_CAPS_LOCK_ON)));
  EXPECT_FALSE(fake_ime_keyboard_.IsCapsLockEnabled());
}

TEST_F(EventRewriterTest, TestRewriteCapsLockToControl) {
  // Remap CapsLock to Control.
  Preferences::RegisterProfilePrefs(prefs()->registry());
  IntegerPrefMember control;
  InitModifierKeyPref(&control, ::prefs::kLanguageRemapCapsLockKeyTo,
                      ui::mojom::ModifierKey::kCapsLock,
                      ui::mojom::ModifierKey::kControl);

  SetUpKeyboard(kExternalGenericKeyboard);

  // Press CapsLock+a. Confirm that Mod3Mask is rewritten to ControlMask.
  // On Chrome OS, CapsLock works as a Mod3 modifier.
  EXPECT_EQ(APressed(ui::EF_CONTROL_DOWN),
            RunRewriter(APressed(ui::EF_MOD3_DOWN)));

  // Press Control+CapsLock+a. Confirm that Mod3Mask is rewritten to
  // ControlMask
  EXPECT_EQ(APressed(ui::EF_CONTROL_DOWN),
            RunRewriter(APressed(ui::EF_CONTROL_DOWN | ui::EF_MOD3_DOWN)));

  // Press Alt+CapsLock+a. Confirm that Mod3Mask is rewritten to
  // ControlMask.
  EXPECT_EQ(APressed(ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN),
            RunRewriter(APressed(ui::EF_ALT_DOWN | ui::EF_MOD3_DOWN)));
}

TEST_F(EventRewriterTest, TestRewriteCapsLockMod3InUse) {
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
  EXPECT_EQ(APressed(), RunRewriter(APressed()));

  input_method_manager_mock_->set_mod3_used(false);
}

// TODO(crbug.com/1179893): Remove once the feature is enabled permanently.
TEST_F(EventRewriterTest, TestRewriteExtendedKeysAltVariantsOld) {
  Preferences::RegisterProfilePrefs(prefs()->registry());
  scoped_feature_list_.InitWithFeatures(
      {}, {::features::kImprovedKeyboardShortcuts,
           features::kAltClickAndSixPackCustomization});

  for (const auto keyboard : kNonAppleKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    // Alt+Backspace -> Delete
    EXPECT_EQ(DeletePressed(), RunRewriter(BackspacePressed(ui::EF_ALT_DOWN)));

    // Control+Alt+Backspace -> Control+Delete
    EXPECT_EQ(
        DeletePressed(ui::EF_CONTROL_DOWN),
        RunRewriter(BackspacePressed(ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN)));

    // Search+Alt+Backspace -> Alt+Backspace
    EXPECT_EQ(
        BackspacePressed(ui::EF_ALT_DOWN),
        RunRewriter(BackspacePressed(ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN)));

    // Search+Control+Alt+Backspace -> Control+Alt+Backspace
    EXPECT_EQ(
        BackspacePressed(ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN),
        RunRewriter(BackspacePressed(ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN |
                                     ui::EF_CONTROL_DOWN)));

    // Alt+Up -> Prior
    EXPECT_EQ(PageUpPressed(), RunRewriter(ArrowUpPressed(ui::EF_ALT_DOWN)));

    // Alt+Down -> Next
    EXPECT_EQ(PageDownPressed(),
              RunRewriter(ArrowDownPressed(ui::EF_ALT_DOWN)));

    // Ctrl+Alt+Up -> Home
    EXPECT_EQ(HomePressed(), RunRewriter(ArrowUpPressed(ui::EF_ALT_DOWN |
                                                        ui::EF_CONTROL_DOWN)));

    // Ctrl+Alt+Down -> End
    EXPECT_EQ(EndPressed(), RunRewriter(ArrowDownPressed(ui::EF_ALT_DOWN |
                                                         ui::EF_CONTROL_DOWN)));

    // NOTE: The following are workarounds to avoid rewriting the
    // Alt variants by additionally pressing Search.
    // Search+Ctrl+Alt+Up -> Ctrl+Alt+Up
    EXPECT_EQ(ArrowUpPressed(ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN),
              RunRewriter(ArrowUpPressed(ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN |
                                         ui::EF_COMMAND_DOWN)));

    // Search+Ctrl+Alt+Down -> Ctrl+Alt+Down
    EXPECT_EQ(
        ArrowDownPressed(ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN),
        RunRewriter(ArrowDownPressed(ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN |
                                     ui::EF_COMMAND_DOWN)));
  }
}

TEST_F(EventRewriterTest, TestRewriteExtendedKeysAltVariants) {
  Preferences::RegisterProfilePrefs(prefs()->registry());
  scoped_feature_list_.InitAndDisableFeature(
      features::kAltClickAndSixPackCustomization);
  // All the previously supported Alt based rewrites no longer have any
  // effect. The Search workarounds no longer take effect and the Search+Key
  // portion is rewritten as expected.
  for (const auto keyboard : kNonAppleKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    // Alt+Backspace -> No Rewrite
    EXPECT_EQ(BackspacePressed(ui::EF_ALT_DOWN),
              RunRewriter(BackspacePressed(ui::EF_ALT_DOWN)));
    EXPECT_EQ(1u, message_center_.NotificationCount());
    ClearNotifications();

    // Control+Alt+Backspace -> No Rewrite
    EXPECT_EQ(
        BackspacePressed(ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN),
        RunRewriter(BackspacePressed(ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN)));
    EXPECT_EQ(1u, message_center_.NotificationCount());
    ClearNotifications();

    // Search+Alt+Backspace -> Alt+Delete
    EXPECT_EQ(
        DeletePressed(ui::EF_ALT_DOWN),
        RunRewriter(BackspacePressed(ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN)));

    // Search+Control+Alt+Backspace -> Control+Alt+Delete
    EXPECT_EQ(
        DeletePressed(ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN),
        RunRewriter(BackspacePressed(ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN |
                                     ui::EF_ALT_DOWN)));

    // Alt+Up -> No Rewrite
    EXPECT_EQ(ArrowUpPressed(ui::EF_ALT_DOWN),
              RunRewriter(ArrowUpPressed(ui::EF_ALT_DOWN)));
    EXPECT_EQ(1u, message_center_.NotificationCount());
    ClearNotifications();

    // Alt+Down -> No Rewrite
    EXPECT_EQ(ArrowDownPressed(ui::EF_ALT_DOWN),
              RunRewriter(ArrowDownPressed(ui::EF_ALT_DOWN)));
    EXPECT_EQ(1u, message_center_.NotificationCount());
    ClearNotifications();

    // Ctrl+Alt+Up -> No Rewrite
    EXPECT_EQ(
        ArrowUpPressed(ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN),
        RunRewriter(ArrowUpPressed(ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN)));
    EXPECT_EQ(1u, message_center_.NotificationCount());
    ClearNotifications();

    // Ctrl+Alt+Down -> No Rewrite
    EXPECT_EQ(
        ArrowDownPressed(ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN),
        RunRewriter(ArrowDownPressed(ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN)));
    EXPECT_EQ(1u, message_center_.NotificationCount());
    ClearNotifications();

    // NOTE: The following were workarounds to avoid rewriting the
    // Alt variants by additionally pressing Search.

    // Search+Ctrl+Alt+Up -> Ctrl+Alt+PageUp(aka Prior)
    EXPECT_EQ(
        PageUpPressed(ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN),
        RunRewriter(ArrowUpPressed(ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN |
                                   ui::EF_ALT_DOWN)));
    // Search+Ctrl+Alt+Down -> Ctrl+Alt+PageDown(aka Next)
    EXPECT_EQ(
        PageDownPressed(ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN),
        RunRewriter(ArrowDownPressed(ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN |
                                     ui::EF_ALT_DOWN)));
  }
}

// TODO(crbug.com/1179893): Remove once the feature is enabled permanently.
TEST_F(EventRewriterTest, TestRewriteExtendedKeyInsertOld) {
  Preferences::RegisterProfilePrefs(prefs()->registry());
  scoped_feature_list_.InitWithFeatures(
      {}, {::features::kImprovedKeyboardShortcuts,
           features::kAltClickAndSixPackCustomization});
  for (const auto keyboard : kNonAppleKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    // Period -> Period
    EXPECT_EQ(PeriodPressed(), RunRewriter(PeriodPressed()));

    // Search+Period -> Insert
    EXPECT_EQ(InsertPressed(), RunRewriter(PeriodPressed(ui::EF_COMMAND_DOWN)));

    // Control+Search+Period -> Control+Insert
    EXPECT_EQ(
        InsertPressed(ui::EF_CONTROL_DOWN),
        RunRewriter(PeriodPressed(ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN)));
  }
}

TEST_F(EventRewriterTest, TestRewriteExtendedKeyInsertDeprecatedNotification) {
  Preferences::RegisterProfilePrefs(prefs()->registry());
  scoped_feature_list_.InitWithFeatures(
      {::features::kImprovedKeyboardShortcuts},
      {features::kAltClickAndSixPackCustomization});

  for (const auto keyboard : kNonAppleKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    // Period -> Period
    EXPECT_EQ(PeriodPressed(), RunRewriter(PeriodPressed()));

    // Search+Period -> No rewrite (and shows notification)
    EXPECT_EQ(PeriodPressed(ui::EF_COMMAND_DOWN),
              RunRewriter(PeriodPressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(1u, message_center_.NotificationCount());
    ClearNotifications();

    // Control+Search+Period -> No rewrite (and shows notification)
    EXPECT_EQ(
        PeriodPressed(ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN),
        RunRewriter(PeriodPressed(ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN)));
    EXPECT_EQ(1u, message_center_.NotificationCount());
    ClearNotifications();
  }
}

// TODO(crbug.com/1179893): Rename once the feature is enabled permanently.
TEST_F(EventRewriterTest, TestRewriteExtendedKeyInsertNew) {
  Preferences::RegisterProfilePrefs(prefs()->registry());
  scoped_feature_list_.InitWithFeatures(
      {::features::kImprovedKeyboardShortcuts},
      {features::kAltClickAndSixPackCustomization});

  for (const auto keyboard : kNonAppleKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    // Search+Shift+Backspace -> Insert
    EXPECT_EQ(
        InsertPressed(),
        RunRewriter(BackspacePressed(ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN)));

    // Control+Search+Shift+Backspace -> Control+Insert
    EXPECT_EQ(
        InsertPressed(ui::EF_CONTROL_DOWN),
        RunRewriter(BackspacePressed(ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN |
                                     ui::EF_SHIFT_DOWN)));
  }
}

TEST_F(EventRewriterTest, TestRewriteExtendedKeysSearchVariants) {
  scoped_feature_list_.InitAndDisableFeature(
      features::kAltClickAndSixPackCustomization);
  Preferences::RegisterProfilePrefs(prefs()->registry());

  for (const auto keyboard : kNonAppleKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    // Search+Backspace -> Delete
    EXPECT_EQ(DeletePressed(),
              RunRewriter(BackspacePressed(ui::EF_COMMAND_DOWN)));

    // Search+Up -> Prior
    EXPECT_EQ(PageUpPressed(),
              RunRewriter(ArrowUpPressed(ui::EF_COMMAND_DOWN)));

    // Search+Down -> Next
    EXPECT_EQ(PageDownPressed(),
              RunRewriter(ArrowDownPressed(ui::EF_COMMAND_DOWN)));

    // Search+Left -> Home
    EXPECT_EQ(HomePressed(),
              RunRewriter(ArrowLeftPressed(ui::EF_COMMAND_DOWN)));

    // Control+Search+Left -> Control+Home
    EXPECT_EQ(HomePressed(ui::EF_CONTROL_DOWN),
              RunRewriter(
                  ArrowLeftPressed(ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN)));

    // Search+Right -> End
    EXPECT_EQ(EndPressed(),
              RunRewriter(ArrowRightPressed(ui::EF_COMMAND_DOWN)));

    // Control+Search+Right -> Control+End
    EXPECT_EQ(EndPressed(ui::EF_CONTROL_DOWN),
              RunRewriter(ArrowRightPressed(ui::EF_COMMAND_DOWN |
                                            ui::EF_CONTROL_DOWN)));
  }
}

TEST_F(EventRewriterTest, TestNumberRowIsNotRewritten) {
  Preferences::RegisterProfilePrefs(prefs()->registry());
  scoped_feature_list_.InitAndDisableFeature(
      features::kAltClickAndSixPackCustomization);
  for (const auto& keyboard : kNonAppleNonCustomLayoutKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    // The number row should not be rewritten without Search key.
    EXPECT_EQ(Digit1Pressed(), RunRewriter(Digit1Pressed()));
    EXPECT_EQ(Digit2Pressed(), RunRewriter(Digit2Pressed()));
    EXPECT_EQ(Digit3Pressed(), RunRewriter(Digit3Pressed()));
    EXPECT_EQ(Digit4Pressed(), RunRewriter(Digit4Pressed()));
    EXPECT_EQ(Digit5Pressed(), RunRewriter(Digit5Pressed()));
    EXPECT_EQ(Digit6Pressed(), RunRewriter(Digit6Pressed()));
    EXPECT_EQ(Digit7Pressed(), RunRewriter(Digit7Pressed()));
    EXPECT_EQ(Digit8Pressed(), RunRewriter(Digit8Pressed()));
    EXPECT_EQ(Digit9Pressed(), RunRewriter(Digit9Pressed()));
    EXPECT_EQ(Digit0Pressed(), RunRewriter(Digit0Pressed()));
    EXPECT_EQ(MinusPressed(), RunRewriter(MinusPressed()));
    EXPECT_EQ(EqualPressed(), RunRewriter(EqualPressed()));
  }
}

// TODO(crbug.com/1179893): Remove once the feature is enabled permanently.
TEST_F(EventRewriterTest, TestRewriteSearchNumberToFunctionKeyOld) {
  Preferences::RegisterProfilePrefs(prefs()->registry());
  scoped_feature_list_.InitAndDisableFeature(
      ::features::kImprovedKeyboardShortcuts);

  for (const auto& keyboard : kNonAppleNonCustomLayoutKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    // The number row should be rewritten as the F<number> row with Search
    // key.
    EXPECT_EQ(F1Pressed(), RunRewriter(Digit1Pressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(F2Pressed(), RunRewriter(Digit2Pressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(F3Pressed(), RunRewriter(Digit3Pressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(F4Pressed(), RunRewriter(Digit4Pressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(F5Pressed(), RunRewriter(Digit5Pressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(F6Pressed(), RunRewriter(Digit6Pressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(F7Pressed(), RunRewriter(Digit7Pressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(F8Pressed(), RunRewriter(Digit8Pressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(F9Pressed(), RunRewriter(Digit9Pressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(F10Pressed(), RunRewriter(Digit0Pressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(F11Pressed(), RunRewriter(MinusPressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(F12Pressed(), RunRewriter(EqualPressed(ui::EF_COMMAND_DOWN)));
  }
}

TEST_F(EventRewriterTest, TestRewriteSearchNumberToFunctionKeyNoAction) {
  Preferences::RegisterProfilePrefs(prefs()->registry());

  for (const auto& keyboard : kNonAppleNonCustomLayoutKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    // Search+Number should now have no effect but a notification will
    // be shown the first time F1 to F10 is pressed.
    EXPECT_EQ(Digit1Pressed(ui::EF_COMMAND_DOWN),
              RunRewriter(Digit1Pressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(Digit2Pressed(ui::EF_COMMAND_DOWN),
              RunRewriter(Digit2Pressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(Digit3Pressed(ui::EF_COMMAND_DOWN),
              RunRewriter(Digit3Pressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(Digit4Pressed(ui::EF_COMMAND_DOWN),
              RunRewriter(Digit4Pressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(Digit5Pressed(ui::EF_COMMAND_DOWN),
              RunRewriter(Digit5Pressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(Digit6Pressed(ui::EF_COMMAND_DOWN),
              RunRewriter(Digit6Pressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(Digit7Pressed(ui::EF_COMMAND_DOWN),
              RunRewriter(Digit7Pressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(Digit8Pressed(ui::EF_COMMAND_DOWN),
              RunRewriter(Digit8Pressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(Digit9Pressed(ui::EF_COMMAND_DOWN),
              RunRewriter(Digit9Pressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(Digit0Pressed(ui::EF_COMMAND_DOWN),
              RunRewriter(Digit0Pressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(MinusPressed(ui::EF_COMMAND_DOWN),
              RunRewriter(MinusPressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(EqualPressed(ui::EF_COMMAND_DOWN),
              RunRewriter(EqualPressed(ui::EF_COMMAND_DOWN)));
  }
}

TEST_F(EventRewriterTest, TestFunctionKeysNotRewrittenBySearch) {
  Preferences::RegisterProfilePrefs(prefs()->registry());

  for (const auto& keyboard : kNonAppleNonCustomLayoutKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    // The function keys should not be rewritten with Search key pressed.
    EXPECT_EQ(F1Pressed(), RunRewriter(F1Pressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(F2Pressed(), RunRewriter(F2Pressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(F3Pressed(), RunRewriter(F3Pressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(F4Pressed(), RunRewriter(F4Pressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(F5Pressed(), RunRewriter(F5Pressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(F6Pressed(), RunRewriter(F6Pressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(F7Pressed(), RunRewriter(F7Pressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(F8Pressed(), RunRewriter(F8Pressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(F9Pressed(), RunRewriter(F9Pressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(F10Pressed(), RunRewriter(F10Pressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(F11Pressed(), RunRewriter(F11Pressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(F12Pressed(), RunRewriter(F12Pressed(ui::EF_COMMAND_DOWN)));
  }
}

TEST_F(EventRewriterTest, TestRewriteFunctionKeysNonCustomLayouts) {
  Preferences::RegisterProfilePrefs(prefs()->registry());

  // Old CrOS keyboards that do not have custom layouts send F-Keys by default
  // and are translated by default to Actions based on hardcoded mappings.
  // New CrOS keyboards are not tested here because they do not remap F-Keys.
  for (const auto& keyboard : kNonAppleNonCustomLayoutKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    // F1 -> Back
    EXPECT_EQ(BrowserBackPressed(), RunRewriter(F1Pressed()));
    EXPECT_EQ(BrowserBackPressed(ui::EF_CONTROL_DOWN),
              RunRewriter(F1Pressed(ui::EF_CONTROL_DOWN)));
    EXPECT_EQ(BrowserBackPressed(ui::EF_ALT_DOWN),
              RunRewriter(F1Pressed(ui::EF_ALT_DOWN)));

    // F2 -> Forward
    EXPECT_EQ(BrowserForwardPressed(), RunRewriter(F2Pressed()));
    EXPECT_EQ(BrowserForwardPressed(ui::EF_CONTROL_DOWN),
              RunRewriter(F2Pressed(ui::EF_CONTROL_DOWN)));
    EXPECT_EQ(BrowserForwardPressed(ui::EF_ALT_DOWN),
              RunRewriter(F2Pressed(ui::EF_ALT_DOWN)));

    // F3 -> Refresh
    EXPECT_EQ(BrowserRefreshPressed(), RunRewriter(F3Pressed()));
    EXPECT_EQ(BrowserRefreshPressed(ui::EF_CONTROL_DOWN),
              RunRewriter(F3Pressed(ui::EF_CONTROL_DOWN)));
    EXPECT_EQ(BrowserRefreshPressed(ui::EF_ALT_DOWN),
              RunRewriter(F3Pressed(ui::EF_ALT_DOWN)));

    // F4 -> Zoom (aka Fullscreen)
    EXPECT_EQ(ZoomTogglePressed(), RunRewriter(F4Pressed()));
    EXPECT_EQ(ZoomTogglePressed(ui::EF_CONTROL_DOWN),
              RunRewriter(F4Pressed(ui::EF_CONTROL_DOWN)));
    EXPECT_EQ(ZoomTogglePressed(ui::EF_ALT_DOWN),
              RunRewriter(F4Pressed(ui::EF_ALT_DOWN)));

    // F5 -> Launch App 1
    EXPECT_EQ(SelectTaskPressed(), RunRewriter(F5Pressed()));
    EXPECT_EQ(SelectTaskPressed(ui::EF_CONTROL_DOWN),
              RunRewriter(F5Pressed(ui::EF_CONTROL_DOWN)));
    EXPECT_EQ(SelectTaskPressed(ui::EF_ALT_DOWN),
              RunRewriter(F5Pressed(ui::EF_ALT_DOWN)));

    // F6 -> Brightness down
    EXPECT_EQ(BrightnessDownPressed(), RunRewriter(F6Pressed()));
    EXPECT_EQ(BrightnessDownPressed(ui::EF_CONTROL_DOWN),
              RunRewriter(F6Pressed(ui::EF_CONTROL_DOWN)));
    EXPECT_EQ(BrightnessDownPressed(ui::EF_ALT_DOWN),
              RunRewriter(F6Pressed(ui::EF_ALT_DOWN)));

    // F7 -> Brightness up
    EXPECT_EQ(BrightnessUpPressed(), RunRewriter(F7Pressed()));
    EXPECT_EQ(BrightnessUpPressed(ui::EF_CONTROL_DOWN),
              RunRewriter(F7Pressed(ui::EF_CONTROL_DOWN)));
    EXPECT_EQ(BrightnessUpPressed(ui::EF_ALT_DOWN),
              RunRewriter(F7Pressed(ui::EF_ALT_DOWN)));

    // F8 -> Volume Mute
    EXPECT_EQ(VolumeMutePressed(), RunRewriter(F8Pressed()));
    EXPECT_EQ(VolumeMutePressed(ui::EF_CONTROL_DOWN),
              RunRewriter(F8Pressed(ui::EF_CONTROL_DOWN)));
    EXPECT_EQ(VolumeMutePressed(ui::EF_ALT_DOWN),
              RunRewriter(F8Pressed(ui::EF_ALT_DOWN)));

    // F9 -> Volume Down
    EXPECT_EQ(VolumeDownPressed(), RunRewriter(F9Pressed()));
    EXPECT_EQ(VolumeDownPressed(ui::EF_CONTROL_DOWN),
              RunRewriter(F9Pressed(ui::EF_CONTROL_DOWN)));
    EXPECT_EQ(VolumeDownPressed(ui::EF_ALT_DOWN),
              RunRewriter(F9Pressed(ui::EF_ALT_DOWN)));

    // F10 -> Volume Up
    EXPECT_EQ(VolumeUpPressed(), RunRewriter(F10Pressed()));
    EXPECT_EQ(VolumeUpPressed(ui::EF_CONTROL_DOWN),
              RunRewriter(F10Pressed(ui::EF_CONTROL_DOWN)));
    EXPECT_EQ(VolumeUpPressed(ui::EF_ALT_DOWN),
              RunRewriter(F10Pressed(ui::EF_ALT_DOWN)));

    // F11 -> F11
    EXPECT_EQ(F11Pressed(), RunRewriter(F11Pressed()));
    EXPECT_EQ(F11Pressed(ui::EF_CONTROL_DOWN),
              RunRewriter(F11Pressed(ui::EF_CONTROL_DOWN)));
    EXPECT_EQ(F11Pressed(ui::EF_ALT_DOWN),
              RunRewriter(F11Pressed(ui::EF_ALT_DOWN)));

    // F12 -> F12
    EXPECT_EQ(F12Pressed(), RunRewriter(F12Pressed()));
    EXPECT_EQ(F12Pressed(ui::EF_CONTROL_DOWN),
              RunRewriter(F12Pressed(ui::EF_CONTROL_DOWN)));
    EXPECT_EQ(F12Pressed(ui::EF_ALT_DOWN),
              RunRewriter(F12Pressed(ui::EF_ALT_DOWN)));
  }
}

TEST_F(EventRewriterTest, TestRewriteFunctionKeysCustomLayoutsFKeyUnchanged) {
  Preferences::RegisterProfilePrefs(prefs()->registry());
  // On devices with custom layouts, the F-Keys are never remapped.
  for (const auto& keyboard : kChromeCustomKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    for (const auto pressed :
         {F1Pressed, F2Pressed, F3Pressed, F4Pressed, F5Pressed, F6Pressed,
          F7Pressed, F8Pressed, F9Pressed, F10Pressed, F11Pressed, F12Pressed,
          F13Pressed, F14Pressed, F15Pressed}) {
      EXPECT_EQ(pressed(ui::EF_NONE), RunRewriter(pressed(ui::EF_NONE)));
      EXPECT_EQ(pressed(ui::EF_CONTROL_DOWN),
                RunRewriter(pressed(ui::EF_CONTROL_DOWN)));
      EXPECT_EQ(pressed(ui::EF_ALT_DOWN),
                RunRewriter(pressed(ui::EF_ALT_DOWN)));
      EXPECT_EQ(pressed(ui::EF_COMMAND_DOWN),
                RunRewriter(pressed(ui::EF_COMMAND_DOWN)));
    }
  }
}

TEST_F(EventRewriterTest, TestRewriteFunctionKeysCustomLayoutsActionUnchanged) {
  Preferences::RegisterProfilePrefs(prefs()->registry());

  // An action key on these devices is one where the scan code matches an entry
  // in the layout map. It doesn't matter what the action is, as long the
  // search key isn't pressed it will pass through unchanged.
  SetUpKeyboard({.name = "Internal Custom LayoutKeyboard",
                 .layout = "a1 a2 a3",
                 .type = ui::INPUT_DEVICE_INTERNAL,
                 .has_custom_top_row = true});
  auto browser_refresh = BrowserRefreshPressed();
  browser_refresh.scan_code = 0xa1;
  EXPECT_EQ(browser_refresh, RunRewriter(browser_refresh));

  auto volume_up = VolumeUpPressed();
  volume_up.scan_code = 0xa2;
  EXPECT_EQ(volume_up, RunRewriter(volume_up));

  auto volume_down = VolumeDownPressed();
  volume_down.scan_code = 0xa3;
  EXPECT_EQ(volume_down, RunRewriter(volume_down));
}

TEST_F(EventRewriterTest,
       TestRewriteFunctionKeysCustomLayoutsActionSuppressedUnchanged) {
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

  auto browser_refresh = BrowserRefreshPressed(ui::EF_COMMAND_DOWN);
  browser_refresh.scan_code = 0xa1;
  EXPECT_EQ(browser_refresh, RunRewriter(browser_refresh));

  auto volume_up = VolumeUpPressed(ui::EF_COMMAND_DOWN);
  volume_up.scan_code = 0xa2;
  EXPECT_EQ(volume_up, RunRewriter(volume_up));

  auto volume_down = VolumeDownPressed(ui::EF_COMMAND_DOWN);
  volume_down.scan_code = 0xa3;
  EXPECT_EQ(volume_down, RunRewriter(volume_down));
}

TEST_F(EventRewriterTest,
       TestRewriteFunctionKeysCustomLayoutsActionSuppressedWithTopRowAreFKeys) {
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

  auto browser_refresh = BrowserRefreshPressed(ui::EF_COMMAND_DOWN);
  browser_refresh.scan_code = 0xa1;
  auto f1 = F1Pressed(ui::EF_COMMAND_DOWN);
  f1.scan_code = 0xa1;
  EXPECT_EQ(f1, RunRewriter(browser_refresh));

  auto volume_up = VolumeUpPressed(ui::EF_COMMAND_DOWN);
  volume_up.scan_code = 0xa2;
  auto f2 = F2Pressed(ui::EF_COMMAND_DOWN);
  f2.scan_code = 0xa2;
  EXPECT_EQ(f2, RunRewriter(volume_up));

  auto volume_down = VolumeDownPressed(ui::EF_COMMAND_DOWN);
  volume_down.scan_code = 0xa3;
  auto f3 = F3Pressed(ui::EF_COMMAND_DOWN);
  f3.scan_code = 0xa3;
  EXPECT_EQ(f3, RunRewriter(volume_down));
}

TEST_F(EventRewriterTest, TestRewriteFunctionKeysCustomLayouts) {
  Preferences::RegisterProfilePrefs(prefs()->registry());

  // On devices with custom layouts, scan codes that match the layout
  // map get mapped to F-Keys based only on the scan code. The search
  // key also gets treated as unpressed in the remapped event.
  SetUpKeyboard({.name = "Internal Custom Layout Keyboard",
                 .layout = "a1 a2 a3 a4 a5 a6 a7 a8 a9 aa ab ac ad ae af",
                 .type = ui::INPUT_DEVICE_INTERNAL,
                 .has_custom_top_row = true});

  struct TestCase {
    TestKeyEvent (*pressed)(ui::EventFlags);
    uint32_t scan_code;
  };
  // Action -> F1..F15
  for (const auto& [pressed, scan_code] :
       std::initializer_list<TestCase>{{F1Pressed, 0xa1},
                                       {F2Pressed, 0xa2},
                                       {F3Pressed, 0xa3},
                                       {F4Pressed, 0xa4},
                                       {F5Pressed, 0xa5},
                                       {F6Pressed, 0xa6},
                                       {F7Pressed, 0xa7},
                                       {F8Pressed, 0xa8},
                                       {F9Pressed, 0xa9},
                                       {F10Pressed, 0xaa},
                                       {F11Pressed, 0xab},
                                       {F12Pressed, 0xac},
                                       {F13Pressed, 0xad},
                                       {F14Pressed, 0xae},
                                       {F15Pressed, 0xaf}}) {
    auto unknown = UnknownPressed(ui::EF_COMMAND_DOWN);
    unknown.scan_code = scan_code;
    auto func = pressed(ui::EF_NONE);
    func.scan_code = scan_code;
    EXPECT_EQ(func, RunRewriter(unknown));
  }
}

TEST_F(EventRewriterTest, TestRewriteFunctionKeysLayout2) {
  Preferences::RegisterProfilePrefs(prefs()->registry());
  SetUpKeyboard({.name = "Internal Keyboard",
                 .layout = kKbdTopRowLayout2Tag,
                 .type = ui::INPUT_DEVICE_INTERNAL,
                 .has_custom_top_row = false});

  // F1 -> Back
  EXPECT_EQ(BrowserBackPressed(), RunRewriter(F1Pressed()));
  EXPECT_EQ(BrowserBackPressed(ui::EF_CONTROL_DOWN),
            RunRewriter(F1Pressed(ui::EF_CONTROL_DOWN)));
  EXPECT_EQ(BrowserBackPressed(ui::EF_ALT_DOWN),
            RunRewriter(F1Pressed(ui::EF_ALT_DOWN)));

  // F2 -> Refresh
  EXPECT_EQ(BrowserRefreshPressed(), RunRewriter(F2Pressed()));
  EXPECT_EQ(BrowserRefreshPressed(ui::EF_CONTROL_DOWN),
            RunRewriter(F2Pressed(ui::EF_CONTROL_DOWN)));
  EXPECT_EQ(BrowserRefreshPressed(ui::EF_ALT_DOWN),
            RunRewriter(F2Pressed(ui::EF_ALT_DOWN)));

  // F3 -> Zoom (aka Fullscreen)
  EXPECT_EQ(ZoomTogglePressed(), RunRewriter(F3Pressed()));
  EXPECT_EQ(ZoomTogglePressed(ui::EF_CONTROL_DOWN),
            RunRewriter(F3Pressed(ui::EF_CONTROL_DOWN)));
  EXPECT_EQ(ZoomTogglePressed(ui::EF_ALT_DOWN),
            RunRewriter(F3Pressed(ui::EF_ALT_DOWN)));

  // F4 -> Launch App 1
  EXPECT_EQ(SelectTaskPressed(), RunRewriter(F4Pressed()));
  EXPECT_EQ(SelectTaskPressed(ui::EF_CONTROL_DOWN),
            RunRewriter(F4Pressed(ui::EF_CONTROL_DOWN)));
  EXPECT_EQ(SelectTaskPressed(ui::EF_ALT_DOWN),
            RunRewriter(F4Pressed(ui::EF_ALT_DOWN)));

  // F5 -> Brightness down
  EXPECT_EQ(BrightnessDownPressed(), RunRewriter(F5Pressed()));
  EXPECT_EQ(BrightnessDownPressed(ui::EF_CONTROL_DOWN),
            RunRewriter(F5Pressed(ui::EF_CONTROL_DOWN)));
  EXPECT_EQ(BrightnessDownPressed(ui::EF_ALT_DOWN),
            RunRewriter(F5Pressed(ui::EF_ALT_DOWN)));

  // F6 -> Brightness up
  EXPECT_EQ(BrightnessUpPressed(), RunRewriter(F6Pressed()));
  EXPECT_EQ(BrightnessUpPressed(ui::EF_CONTROL_DOWN),
            RunRewriter(F6Pressed(ui::EF_CONTROL_DOWN)));
  EXPECT_EQ(BrightnessUpPressed(ui::EF_ALT_DOWN),
            RunRewriter(F6Pressed(ui::EF_ALT_DOWN)));

  // F7 -> Media Play/Pause
  EXPECT_EQ(MediaPlayPausePressed(), RunRewriter(F7Pressed()));
  EXPECT_EQ(MediaPlayPausePressed(ui::EF_CONTROL_DOWN),
            RunRewriter(F7Pressed(ui::EF_CONTROL_DOWN)));
  EXPECT_EQ(MediaPlayPausePressed(ui::EF_ALT_DOWN),
            RunRewriter(F7Pressed(ui::EF_ALT_DOWN)));

  // F8 -> Volume Mute
  EXPECT_EQ(VolumeMutePressed(), RunRewriter(F8Pressed()));
  EXPECT_EQ(VolumeMutePressed(ui::EF_CONTROL_DOWN),
            RunRewriter(F8Pressed(ui::EF_CONTROL_DOWN)));
  EXPECT_EQ(VolumeMutePressed(ui::EF_ALT_DOWN),
            RunRewriter(F8Pressed(ui::EF_ALT_DOWN)));

  // F9 -> Volume Down
  EXPECT_EQ(VolumeDownPressed(), RunRewriter(F9Pressed()));
  EXPECT_EQ(VolumeDownPressed(ui::EF_CONTROL_DOWN),
            RunRewriter(F9Pressed(ui::EF_CONTROL_DOWN)));
  EXPECT_EQ(VolumeDownPressed(ui::EF_ALT_DOWN),
            RunRewriter(F9Pressed(ui::EF_ALT_DOWN)));

  // F10 -> Volume Up
  EXPECT_EQ(VolumeUpPressed(), RunRewriter(F10Pressed()));
  EXPECT_EQ(VolumeUpPressed(ui::EF_CONTROL_DOWN),
            RunRewriter(F10Pressed(ui::EF_CONTROL_DOWN)));
  EXPECT_EQ(VolumeUpPressed(ui::EF_ALT_DOWN),
            RunRewriter(F10Pressed(ui::EF_ALT_DOWN)));

  // F11 -> F11
  EXPECT_EQ(F11Pressed(), RunRewriter(F11Pressed()));
  EXPECT_EQ(F11Pressed(ui::EF_CONTROL_DOWN),
            RunRewriter(F11Pressed(ui::EF_CONTROL_DOWN)));
  EXPECT_EQ(F11Pressed(ui::EF_ALT_DOWN),
            RunRewriter(F11Pressed(ui::EF_ALT_DOWN)));

  // F12 -> F12
  EXPECT_EQ(F12Pressed(), RunRewriter(F12Pressed()));
  EXPECT_EQ(F12Pressed(ui::EF_CONTROL_DOWN),
            RunRewriter(F12Pressed(ui::EF_CONTROL_DOWN)));
  EXPECT_EQ(F12Pressed(ui::EF_ALT_DOWN),
            RunRewriter(F12Pressed(ui::EF_ALT_DOWN)));
}

TEST_F(EventRewriterTest,
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
  EXPECT_EQ(BrowserBackPressed(ui::EF_COMMAND_DOWN),
            RunRewriter(F1Pressed(ui::EF_COMMAND_DOWN)));

  // F2 -> Refresh
  EXPECT_EQ(BrowserRefreshPressed(ui::EF_COMMAND_DOWN),
            RunRewriter(F2Pressed(ui::EF_COMMAND_DOWN)));

  // F3 -> Zoom (aka Fullscreen)
  EXPECT_EQ(ZoomTogglePressed(ui::EF_COMMAND_DOWN),
            RunRewriter(F3Pressed(ui::EF_COMMAND_DOWN)));

  // F4 -> Launch App 1
  EXPECT_EQ(SelectTaskPressed(ui::EF_COMMAND_DOWN),
            RunRewriter(F4Pressed(ui::EF_COMMAND_DOWN)));

  // F5 -> Brightness down
  EXPECT_EQ(BrightnessDownPressed(ui::EF_COMMAND_DOWN),
            RunRewriter(F5Pressed(ui::EF_COMMAND_DOWN)));

  // F6 -> Brightness up
  EXPECT_EQ(BrightnessUpPressed(ui::EF_COMMAND_DOWN),
            RunRewriter(F6Pressed(ui::EF_COMMAND_DOWN)));

  // F7 -> Media Play/Pause
  EXPECT_EQ(MediaPlayPausePressed(ui::EF_COMMAND_DOWN),
            RunRewriter(F7Pressed(ui::EF_COMMAND_DOWN)));

  // F8 -> Volume Mute
  EXPECT_EQ(VolumeMutePressed(ui::EF_COMMAND_DOWN),
            RunRewriter(F8Pressed(ui::EF_COMMAND_DOWN)));

  // F9 -> Volume Down
  EXPECT_EQ(VolumeDownPressed(ui::EF_COMMAND_DOWN),
            RunRewriter(F9Pressed(ui::EF_COMMAND_DOWN)));

  // F10 -> Volume Up
  EXPECT_EQ(VolumeUpPressed(ui::EF_COMMAND_DOWN),
            RunRewriter(F10Pressed(ui::EF_COMMAND_DOWN)));

  // F11 -> F11
  EXPECT_EQ(F11Pressed(ui::EF_COMMAND_DOWN),
            RunRewriter(F11Pressed(ui::EF_COMMAND_DOWN)));

  // F12 -> F12
  EXPECT_EQ(F12Pressed(ui::EF_COMMAND_DOWN),
            RunRewriter(F12Pressed(ui::EF_COMMAND_DOWN)));
}

TEST_F(EventRewriterTest, RecordEventRemappedToRightClick) {
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

TEST_F(
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

  EXPECT_EQ(F1Pressed(ui::EF_COMMAND_DOWN),
            RunRewriter(F1Pressed(ui::EF_COMMAND_DOWN)));
  EXPECT_EQ(F2Pressed(ui::EF_COMMAND_DOWN),
            RunRewriter(F2Pressed(ui::EF_COMMAND_DOWN)));
  EXPECT_EQ(F3Pressed(ui::EF_COMMAND_DOWN),
            RunRewriter(F3Pressed(ui::EF_COMMAND_DOWN)));
  EXPECT_EQ(F4Pressed(ui::EF_COMMAND_DOWN),
            RunRewriter(F4Pressed(ui::EF_COMMAND_DOWN)));
  EXPECT_EQ(F5Pressed(ui::EF_COMMAND_DOWN),
            RunRewriter(F5Pressed(ui::EF_COMMAND_DOWN)));
  EXPECT_EQ(F6Pressed(ui::EF_COMMAND_DOWN),
            RunRewriter(F6Pressed(ui::EF_COMMAND_DOWN)));
  EXPECT_EQ(F7Pressed(ui::EF_COMMAND_DOWN),
            RunRewriter(F7Pressed(ui::EF_COMMAND_DOWN)));
  EXPECT_EQ(F8Pressed(ui::EF_COMMAND_DOWN),
            RunRewriter(F8Pressed(ui::EF_COMMAND_DOWN)));
  EXPECT_EQ(F9Pressed(ui::EF_COMMAND_DOWN),
            RunRewriter(F9Pressed(ui::EF_COMMAND_DOWN)));
  EXPECT_EQ(F10Pressed(ui::EF_COMMAND_DOWN),
            RunRewriter(F10Pressed(ui::EF_COMMAND_DOWN)));
  EXPECT_EQ(F11Pressed(ui::EF_COMMAND_DOWN),
            RunRewriter(F11Pressed(ui::EF_COMMAND_DOWN)));
  EXPECT_EQ(F12Pressed(ui::EF_COMMAND_DOWN),
            RunRewriter(F12Pressed(ui::EF_COMMAND_DOWN)));
}

TEST_F(EventRewriterTest, TestRewriteFunctionKeysWilcoLayouts) {
  Preferences::RegisterProfilePrefs(prefs()->registry());

  for (const auto& keyboard : kWilcoKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    // F1 -> F1, Search + F1 -> Back
    EXPECT_EQ(F1Pressed(), RunRewriter(F1Pressed()));
    EXPECT_EQ(BrowserBackPressed(),
              RunRewriter(F1Pressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(F1Pressed(ui::EF_CONTROL_DOWN),
              RunRewriter(F1Pressed(ui::EF_CONTROL_DOWN)));
    EXPECT_EQ(F1Pressed(ui::EF_ALT_DOWN),
              RunRewriter(F1Pressed(ui::EF_ALT_DOWN)));

    // F2 -> F2, Search + F2 -> Refresh
    EXPECT_EQ(F2Pressed(), RunRewriter(F2Pressed()));
    EXPECT_EQ(BrowserRefreshPressed(),
              RunRewriter(F2Pressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(F2Pressed(ui::EF_CONTROL_DOWN),
              RunRewriter(F2Pressed(ui::EF_CONTROL_DOWN)));
    EXPECT_EQ(F2Pressed(ui::EF_ALT_DOWN),
              RunRewriter(F2Pressed(ui::EF_ALT_DOWN)));

    // F3 -> F3, Search + F3 -> Zoom (aka Fullscreen)
    EXPECT_EQ(F3Pressed(), RunRewriter(F3Pressed()));
    EXPECT_EQ(ZoomTogglePressed(), RunRewriter(F3Pressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(F3Pressed(ui::EF_CONTROL_DOWN),
              RunRewriter(F3Pressed(ui::EF_CONTROL_DOWN)));
    EXPECT_EQ(F3Pressed(ui::EF_ALT_DOWN),
              RunRewriter(F3Pressed(ui::EF_ALT_DOWN)));

    // F4 -> F4, Search + F4 -> Launch App 1
    EXPECT_EQ(F4Pressed(), RunRewriter(F4Pressed()));
    EXPECT_EQ((TestKeyEvent{ui::ET_KEY_PRESSED, ui::DomCode::F4, ui::DomKey::F4,
                            ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_NONE}),
              RunRewriter(F4Pressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(F4Pressed(ui::EF_CONTROL_DOWN),
              RunRewriter(F4Pressed(ui::EF_CONTROL_DOWN)));
    EXPECT_EQ(F4Pressed(ui::EF_ALT_DOWN),
              RunRewriter(F4Pressed(ui::EF_ALT_DOWN)));

    // F5 -> F5, Search + F5 -> Brightness down
    EXPECT_EQ(F5Pressed(), RunRewriter(F5Pressed()));
    EXPECT_EQ(BrightnessDownPressed(),
              RunRewriter(F5Pressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(F5Pressed(ui::EF_CONTROL_DOWN),
              RunRewriter(F5Pressed(ui::EF_CONTROL_DOWN)));
    EXPECT_EQ(F5Pressed(ui::EF_ALT_DOWN),
              RunRewriter(F5Pressed(ui::EF_ALT_DOWN)));

    // F6 -> F6, Search + F6 -> Brightness up
    EXPECT_EQ(F6Pressed(), RunRewriter(F6Pressed()));
    EXPECT_EQ(BrightnessUpPressed(),
              RunRewriter(F6Pressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(F6Pressed(ui::EF_CONTROL_DOWN),
              RunRewriter(F6Pressed(ui::EF_CONTROL_DOWN)));
    EXPECT_EQ(F6Pressed(ui::EF_ALT_DOWN),
              RunRewriter(F6Pressed(ui::EF_ALT_DOWN)));

    // F7 -> F7, Search + F7 -> Volume mute
    EXPECT_EQ(F7Pressed(), RunRewriter(F7Pressed()));
    EXPECT_EQ(VolumeMutePressed(), RunRewriter(F7Pressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(F7Pressed(ui::EF_CONTROL_DOWN),
              RunRewriter(F7Pressed(ui::EF_CONTROL_DOWN)));
    EXPECT_EQ(F7Pressed(ui::EF_ALT_DOWN),
              RunRewriter(F7Pressed(ui::EF_ALT_DOWN)));

    // F8 -> F8, Search + F8 -> Volume Down
    EXPECT_EQ(F8Pressed(), RunRewriter(F8Pressed()));
    EXPECT_EQ(VolumeDownPressed(), RunRewriter(F8Pressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(F8Pressed(ui::EF_CONTROL_DOWN),
              RunRewriter(F8Pressed(ui::EF_CONTROL_DOWN)));
    EXPECT_EQ(F8Pressed(ui::EF_ALT_DOWN),
              RunRewriter(F8Pressed(ui::EF_ALT_DOWN)));

    // F9 -> F9, Search + F9 -> Volume Up
    EXPECT_EQ(F9Pressed(), RunRewriter(F9Pressed()));
    EXPECT_EQ(VolumeUpPressed(), RunRewriter(F9Pressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(F9Pressed(ui::EF_CONTROL_DOWN),
              RunRewriter(F9Pressed(ui::EF_CONTROL_DOWN)));
    EXPECT_EQ(F9Pressed(ui::EF_ALT_DOWN),
              RunRewriter(F9Pressed(ui::EF_ALT_DOWN)));

    // F10 -> F10, Search + F10 -> F10
    EXPECT_EQ(F10Pressed(), RunRewriter(F10Pressed()));
    EXPECT_EQ(F10Pressed(), RunRewriter(F10Pressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(F10Pressed(ui::EF_CONTROL_DOWN),
              RunRewriter(F10Pressed(ui::EF_CONTROL_DOWN)));
    EXPECT_EQ(F10Pressed(ui::EF_ALT_DOWN),
              RunRewriter(F10Pressed(ui::EF_ALT_DOWN)));

    // F11 -> F11, Search + F11 -> F11
    EXPECT_EQ(F11Pressed(), RunRewriter(F11Pressed()));
    EXPECT_EQ(F11Pressed(), RunRewriter(F11Pressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(F11Pressed(ui::EF_CONTROL_DOWN),
              RunRewriter(F11Pressed(ui::EF_CONTROL_DOWN)));
    EXPECT_EQ(F11Pressed(ui::EF_ALT_DOWN),
              RunRewriter(F11Pressed(ui::EF_ALT_DOWN)));

    // F12 -> F12
    // Search + F12 differs between Wilco devices so it is tested separately.
    EXPECT_EQ(F12Pressed(), RunRewriter(F12Pressed()));
    EXPECT_EQ(F12Pressed(ui::EF_CONTROL_DOWN),
              RunRewriter(F12Pressed(ui::EF_CONTROL_DOWN)));
    EXPECT_EQ(F12Pressed(ui::EF_ALT_DOWN),
              RunRewriter(F12Pressed(ui::EF_ALT_DOWN)));

    // The number row should not be rewritten without Search key.
    EXPECT_EQ(Digit1Pressed(), RunRewriter(Digit1Pressed()));
    EXPECT_EQ(Digit2Pressed(), RunRewriter(Digit2Pressed()));
    EXPECT_EQ(Digit3Pressed(), RunRewriter(Digit3Pressed()));
    EXPECT_EQ(Digit4Pressed(), RunRewriter(Digit4Pressed()));
    EXPECT_EQ(Digit5Pressed(), RunRewriter(Digit5Pressed()));
    EXPECT_EQ(Digit6Pressed(), RunRewriter(Digit6Pressed()));
    EXPECT_EQ(Digit7Pressed(), RunRewriter(Digit7Pressed()));
    EXPECT_EQ(Digit8Pressed(), RunRewriter(Digit8Pressed()));
    EXPECT_EQ(Digit9Pressed(), RunRewriter(Digit9Pressed()));
    EXPECT_EQ(Digit0Pressed(), RunRewriter(Digit0Pressed()));
    EXPECT_EQ(MinusPressed(), RunRewriter(MinusPressed()));
    EXPECT_EQ(EqualPressed(), RunRewriter(EqualPressed()));
  }

  SetUpKeyboard(kWilco1_0Keyboard);
  // Search + F12 -> Ctrl + Zoom (aka Fullscreen) (Display toggle)
  EXPECT_EQ((TestKeyEvent{ui::ET_KEY_PRESSED, ui::DomCode::F12, ui::DomKey::F12,
                          ui::VKEY_ZOOM, ui::EF_CONTROL_DOWN}),
            RunRewriter(F12Pressed(ui::EF_COMMAND_DOWN)));

  SetUpKeyboard(kWilco1_5Keyboard);
  // Search + F12 -> F12 (Privacy screen not supported)
  event_rewriter_ash_->set_privacy_screen_for_testing(false);
  EXPECT_EQ(F12Pressed(), RunRewriter(F12Pressed(ui::EF_COMMAND_DOWN)));

  // F12 -> F12, Search + F12 -> Privacy Screen Toggle
  event_rewriter_ash_->set_privacy_screen_for_testing(true);
  EXPECT_EQ(PrivacyScreenTogglePressed(),
            RunRewriter(F12Pressed(ui::EF_COMMAND_DOWN)));
}

TEST_F(EventRewriterTest, TestRewriteActionKeysWilcoLayouts) {
  Preferences::RegisterProfilePrefs(prefs()->registry());

  for (const auto& keyboard : kWilcoKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    // Back -> Back, Search + Back -> F1
    EXPECT_EQ(BrowserBackPressed(), RunRewriter(BrowserBackPressed()));
    EXPECT_EQ(F1Pressed(),
              RunRewriter(BrowserBackPressed(ui::EF_COMMAND_DOWN)));

    // Refresh -> Refresh, Search + Refresh -> F2
    EXPECT_EQ(BrowserRefreshPressed(), RunRewriter(BrowserRefreshPressed()));
    EXPECT_EQ(F2Pressed(),
              RunRewriter(BrowserRefreshPressed(ui::EF_COMMAND_DOWN)));

    // Full Screen -> Full Screen, Search + Full Screen -> F3
    EXPECT_EQ(ZoomTogglePressed(), RunRewriter(ZoomTogglePressed()));
    EXPECT_EQ(F3Pressed(), RunRewriter(ZoomTogglePressed(ui::EF_COMMAND_DOWN)));

    // Launch App 1 -> Launch App 1, Search + Launch App 1 -> F4
    EXPECT_EQ(SelectTaskPressed(), RunRewriter(SelectTaskPressed()));
    EXPECT_EQ(F4Pressed(), RunRewriter(SelectTaskPressed(ui::EF_COMMAND_DOWN)));

    // Brightness down -> Brightness Down, Search Brightness Down -> F5
    EXPECT_EQ(BrightnessDownPressed(), RunRewriter(BrightnessDownPressed()));
    EXPECT_EQ(F5Pressed(),
              RunRewriter(BrightnessDownPressed(ui::EF_COMMAND_DOWN)));

    // Brightness up -> Brightness Up, Search + Brightness Up -> F6
    EXPECT_EQ(BrightnessUpPressed(), RunRewriter(BrightnessUpPressed()));
    EXPECT_EQ(F6Pressed(),
              RunRewriter(BrightnessUpPressed(ui::EF_COMMAND_DOWN)));

    // Volume mute -> Volume Mute, Search + Volume Mute -> F7
    EXPECT_EQ(VolumeMutePressed(), RunRewriter(VolumeMutePressed()));
    EXPECT_EQ(F7Pressed(), RunRewriter(VolumeMutePressed(ui::EF_COMMAND_DOWN)));

    // Volume Down -> Volume Down, Search + Volume Down -> F8
    EXPECT_EQ(VolumeDownPressed(), RunRewriter(VolumeDownPressed()));
    EXPECT_EQ(F8Pressed(), RunRewriter(VolumeDownPressed(ui::EF_COMMAND_DOWN)));

    // Volume Up -> Volume Up, Search + Volume Up -> F9
    EXPECT_EQ(VolumeUpPressed(), RunRewriter(VolumeUpPressed()));
    EXPECT_EQ(F9Pressed(), RunRewriter(VolumeUpPressed(ui::EF_COMMAND_DOWN)));
  }

  SetUpKeyboard(kWilco1_0Keyboard);
  // Ctrl + Zoom (Display toggle) -> Unchanged
  // Search + Ctrl + Zoom (Display toggle) -> F12
  EXPECT_EQ(ZoomTogglePressed(ui::EF_CONTROL_DOWN),
            RunRewriter(ZoomTogglePressed(ui::EF_CONTROL_DOWN)));
  EXPECT_EQ(F12Pressed(), RunRewriter(ZoomTogglePressed(ui::EF_CONTROL_DOWN |
                                                        ui::EF_COMMAND_DOWN)));

  SetUpKeyboard(kWilco1_5Keyboard);
  {
    // Drallion specific key tests (no privacy screen)
    event_rewriter_ash_->set_privacy_screen_for_testing(false);

    // Privacy Screen Toggle -> F12 (Privacy Screen not supported),
    // Search + Privacy Screen Toggle -> F12
    EXPECT_EQ(F12Pressed(), RunRewriter(PrivacyScreenTogglePressed()));
    EXPECT_EQ(F12Pressed(),
              RunRewriter(PrivacyScreenTogglePressed(ui::EF_COMMAND_DOWN)));

    // Ctrl + Zoom (Display toggle) -> Unchanged
    // Search + Ctrl + Zoom (Display toggle) -> Unchanged
    EXPECT_EQ(ZoomTogglePressed(ui::EF_CONTROL_DOWN),
              RunRewriter(ZoomTogglePressed(ui::EF_CONTROL_DOWN)));
    EXPECT_EQ(ZoomTogglePressed(ui::EF_CONTROL_DOWN),
              RunRewriter(ZoomTogglePressed(ui::EF_CONTROL_DOWN |
                                            ui::EF_COMMAND_DOWN)));
  }

  {
    // Drallion specific key tests (privacy screen supported)
    event_rewriter_ash_->set_privacy_screen_for_testing(true);

    // Privacy Screen Toggle -> Privacy Screen Toggle,
    // Search + Privacy Screen Toggle -> F12
    EXPECT_EQ(PrivacyScreenTogglePressed(),
              RunRewriter(PrivacyScreenTogglePressed()));
    EXPECT_EQ(F12Pressed(),
              RunRewriter(PrivacyScreenTogglePressed(ui::EF_COMMAND_DOWN)));

    // Ctrl + Zoom (Display toggle) -> Unchanged
    // Search + Ctrl + Zoom (Display toggle) -> Unchanged
    EXPECT_EQ(ZoomTogglePressed(ui::EF_CONTROL_DOWN),
              RunRewriter(ZoomTogglePressed(ui::EF_CONTROL_DOWN)));
    EXPECT_EQ(ZoomTogglePressed(ui::EF_CONTROL_DOWN),
              RunRewriter(ZoomTogglePressed(ui::EF_CONTROL_DOWN |
                                            ui::EF_COMMAND_DOWN)));
  }
}

TEST_F(EventRewriterTest,
       TestRewriteActionKeysWilcoLayoutsSuppressMetaTopRowKeyRewrites) {
  Preferences::RegisterProfilePrefs(prefs()->registry());
  delegate_->SuppressMetaTopRowKeyComboRewrites(true);
  keyboard_settings->suppress_meta_fkey_rewrites = true;

  // With |SuppressMetaTopRowKeyComboRewrites|, all action keys should be
  // unchanged and keep the search modifier.

  for (const auto& keyboard : kWilcoKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    EXPECT_EQ(BrowserBackPressed(ui::EF_COMMAND_DOWN),
              RunRewriter(BrowserBackPressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(BrowserRefreshPressed(ui::EF_COMMAND_DOWN),
              RunRewriter(BrowserRefreshPressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(ZoomTogglePressed(ui::EF_COMMAND_DOWN),
              RunRewriter(ZoomTogglePressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(SelectTaskPressed(ui::EF_COMMAND_DOWN),
              RunRewriter(SelectTaskPressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(BrightnessDownPressed(ui::EF_COMMAND_DOWN),
              RunRewriter(BrightnessDownPressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(BrightnessUpPressed(ui::EF_COMMAND_DOWN),
              RunRewriter(BrightnessUpPressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(VolumeMutePressed(ui::EF_COMMAND_DOWN),
              RunRewriter(VolumeMutePressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(VolumeDownPressed(ui::EF_COMMAND_DOWN),
              RunRewriter(VolumeDownPressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(VolumeUpPressed(ui::EF_COMMAND_DOWN),
              RunRewriter(VolumeUpPressed(ui::EF_COMMAND_DOWN)));

    // F-Keys do not remove Search when pressed.
    EXPECT_EQ(F10Pressed(ui::EF_COMMAND_DOWN),
              RunRewriter(F10Pressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(F11Pressed(ui::EF_COMMAND_DOWN),
              RunRewriter(F11Pressed(ui::EF_COMMAND_DOWN)));
  }

  SetUpKeyboard(kWilco1_0Keyboard);
  EXPECT_EQ(ZoomTogglePressed(ui::EF_CONTROL_DOWN | ui::EF_COMMAND_DOWN),
            RunRewriter(
                ZoomTogglePressed(ui::EF_CONTROL_DOWN | ui::EF_COMMAND_DOWN)));

  SetUpKeyboard(kWilco1_5Keyboard);
  {
    // Drallion specific key tests (no privacy screen)
    event_rewriter_ash_->set_privacy_screen_for_testing(false);

    // Search + Privacy Screen Toggle -> Search + F12
    EXPECT_EQ(F12Pressed(ui::EF_COMMAND_DOWN),
              RunRewriter(PrivacyScreenTogglePressed(ui::EF_COMMAND_DOWN)));
    // Search + Ctrl + Zoom (Display toggle) -> Unchanged
    EXPECT_EQ(ZoomTogglePressed(ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN),
              RunRewriter(ZoomTogglePressed(ui::EF_COMMAND_DOWN |
                                            ui::EF_CONTROL_DOWN)));
  }

  {
    // Drallion specific key tests (privacy screen supported)
    event_rewriter_ash_->set_privacy_screen_for_testing(true);

    // Search + Privacy Screen Toggle -> F12  TODO
    EXPECT_EQ(PrivacyScreenTogglePressed(ui::EF_COMMAND_DOWN),
              RunRewriter(PrivacyScreenTogglePressed(ui::EF_COMMAND_DOWN)));
    // Ctrl + Zoom (Display toggle) -> Unchanged  TODO
    // Search + Ctrl + Zoom (Display toggle) -> Unchanged
    EXPECT_EQ(ZoomTogglePressed(ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN),
              RunRewriter(ZoomTogglePressed(ui::EF_COMMAND_DOWN |
                                            ui::EF_CONTROL_DOWN)));
  }
}

TEST_F(
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

    EXPECT_EQ(F1Pressed(ui::EF_COMMAND_DOWN),
              RunRewriter(BrowserBackPressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(F2Pressed(ui::EF_COMMAND_DOWN),
              RunRewriter(BrowserRefreshPressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(F3Pressed(ui::EF_COMMAND_DOWN),
              RunRewriter(ZoomTogglePressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(F4Pressed(ui::EF_COMMAND_DOWN),
              RunRewriter(SelectTaskPressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(F5Pressed(ui::EF_COMMAND_DOWN),
              RunRewriter(BrightnessDownPressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(F6Pressed(ui::EF_COMMAND_DOWN),
              RunRewriter(BrightnessUpPressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(F7Pressed(ui::EF_COMMAND_DOWN),
              RunRewriter(VolumeMutePressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(F8Pressed(ui::EF_COMMAND_DOWN),
              RunRewriter(VolumeDownPressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(F9Pressed(ui::EF_COMMAND_DOWN),
              RunRewriter(VolumeUpPressed(ui::EF_COMMAND_DOWN)));

    EXPECT_EQ(F10Pressed(ui::EF_COMMAND_DOWN),
              RunRewriter(F10Pressed(ui::EF_COMMAND_DOWN)));
    EXPECT_EQ(F11Pressed(ui::EF_COMMAND_DOWN),
              RunRewriter(F11Pressed(ui::EF_COMMAND_DOWN)));
  }

  SetUpKeyboard(kWilco1_0Keyboard);
  EXPECT_EQ(F12Pressed(ui::EF_COMMAND_DOWN),
            RunRewriter(
                ZoomTogglePressed(ui::EF_CONTROL_DOWN | ui::EF_COMMAND_DOWN)));

  SetUpKeyboard(kWilco1_5Keyboard);
  {
    // Drallion specific key tests (no privacy screen)
    event_rewriter_ash_->set_privacy_screen_for_testing(false);

    // Search + Privacy Screen Toggle -> Search + F12
    EXPECT_EQ(F12Pressed(ui::EF_COMMAND_DOWN),
              RunRewriter(PrivacyScreenTogglePressed(ui::EF_COMMAND_DOWN)));
    // Search + Ctrl + Zoom (Display toggle) -> Unchanged
    EXPECT_EQ(ZoomTogglePressed(ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN),
              RunRewriter(ZoomTogglePressed(ui::EF_COMMAND_DOWN |
                                            ui::EF_CONTROL_DOWN)));
  }

  {
    // Drallion specific key tests (privacy screen supported)
    event_rewriter_ash_->set_privacy_screen_for_testing(true);

    // Search + Privacy Screen Toggle -> F12
    EXPECT_EQ(F12Pressed(ui::EF_COMMAND_DOWN),
              RunRewriter(PrivacyScreenTogglePressed(ui::EF_COMMAND_DOWN)));
  }
}

TEST_F(EventRewriterTest, TestTopRowAsFnKeysForKeyboardWilcoLayouts) {
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
    EXPECT_EQ(F1Pressed(), RunRewriter(BrowserBackPressed()));
    EXPECT_EQ(BrowserBackPressed(),
              RunRewriter(BrowserBackPressed(ui::EF_COMMAND_DOWN)));

    // Refresh -> F2, Search + Refresh -> Refresh
    EXPECT_EQ(F2Pressed(), RunRewriter(BrowserRefreshPressed()));
    EXPECT_EQ(BrowserRefreshPressed(),
              RunRewriter(BrowserRefreshPressed(ui::EF_COMMAND_DOWN)));

    // Full Screen -> F3, Search + Full Screen -> Full Screen
    EXPECT_EQ(F3Pressed(), RunRewriter(ZoomTogglePressed()));
    EXPECT_EQ(ZoomTogglePressed(),
              RunRewriter(ZoomTogglePressed(ui::EF_COMMAND_DOWN)));

    // Launch App 1 -> F4, Search + Launch App 1 -> Launch App 1
    EXPECT_EQ(F4Pressed(), RunRewriter(SelectTaskPressed()));
    EXPECT_EQ(SelectTaskPressed(),
              RunRewriter(SelectTaskPressed(ui::EF_COMMAND_DOWN)));

    // Brightness down -> F5, Search Brightness Down -> Brightness Down
    EXPECT_EQ(F5Pressed(), RunRewriter(BrightnessDownPressed()));
    EXPECT_EQ(BrightnessDownPressed(),
              RunRewriter(BrightnessDownPressed(ui::EF_COMMAND_DOWN)));

    // Brightness up -> F6, Search + Brightness Up -> Brightness Up
    EXPECT_EQ(F6Pressed(), RunRewriter(BrightnessUpPressed()));
    EXPECT_EQ(BrightnessUpPressed(),
              RunRewriter(BrightnessUpPressed(ui::EF_COMMAND_DOWN)));

    // Volume mute -> F7, Search + Volume Mute -> Volume Mute
    EXPECT_EQ(F7Pressed(), RunRewriter(VolumeMutePressed()));
    EXPECT_EQ(VolumeMutePressed(),
              RunRewriter(VolumeMutePressed(ui::EF_COMMAND_DOWN)));

    // Volume Down -> F8, Search + Volume Down -> Volume Down
    EXPECT_EQ(F8Pressed(), RunRewriter(VolumeDownPressed()));
    EXPECT_EQ(VolumeDownPressed(),
              RunRewriter(VolumeDownPressed(ui::EF_COMMAND_DOWN)));

    // Volume Up -> F9, Search + Volume Up -> Volume Up
    EXPECT_EQ(F9Pressed(), RunRewriter(VolumeUpPressed()));
    EXPECT_EQ(VolumeUpPressed(),
              RunRewriter(VolumeUpPressed(ui::EF_COMMAND_DOWN)));

    // F10 -> F10
    EXPECT_EQ(F10Pressed(), RunRewriter(F10Pressed()));
    EXPECT_EQ(F10Pressed(), RunRewriter(F10Pressed(ui::EF_COMMAND_DOWN)));

    // F11 -> F11
    EXPECT_EQ(F11Pressed(), RunRewriter(F11Pressed()));
    EXPECT_EQ(F11Pressed(), RunRewriter(F11Pressed(ui::EF_COMMAND_DOWN)));
  }

  SetUpKeyboard(kWilco1_0Keyboard);
  // Ctrl + Zoom (Display toggle) -> F12
  // Search + Ctrl + Zoom (Display toggle) -> Search modifier should be removed
  EXPECT_EQ(F12Pressed(), RunRewriter(ZoomTogglePressed(ui::EF_CONTROL_DOWN)));
  EXPECT_EQ(ZoomTogglePressed(ui::EF_CONTROL_DOWN),
            RunRewriter(
                ZoomTogglePressed(ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN)));

  SetUpKeyboard(kWilco1_5Keyboard);
  {
    // Drallion specific key tests (no privacy screen)
    event_rewriter_ash_->set_privacy_screen_for_testing(false);

    // Privacy Screen Toggle -> F12,
    // Search + Privacy Screen Toggle -> F12 (Privacy screen not supported)
    EXPECT_EQ(F12Pressed(), RunRewriter(PrivacyScreenTogglePressed()));
    EXPECT_EQ(F12Pressed(),
              RunRewriter(PrivacyScreenTogglePressed(ui::EF_COMMAND_DOWN)));

    // Ctrl + Zoom (Display toggle) -> Unchanged
    // Search + Ctrl + Zoom (Display toggle) -> Unchanged
    EXPECT_EQ(ZoomTogglePressed(ui::EF_CONTROL_DOWN),
              RunRewriter(ZoomTogglePressed(ui::EF_CONTROL_DOWN)));
    EXPECT_EQ(ZoomTogglePressed(ui::EF_CONTROL_DOWN),
              RunRewriter(ZoomTogglePressed(ui::EF_COMMAND_DOWN |
                                            ui::EF_CONTROL_DOWN)));
  }

  {
    // Drallion specific key tests (privacy screen supported)
    event_rewriter_ash_->set_privacy_screen_for_testing(true);

    // Privacy Screen Toggle -> F12,
    // Search + Privacy Screen Toggle -> Unchanged
    EXPECT_EQ(F12Pressed(), RunRewriter(PrivacyScreenTogglePressed()));
    EXPECT_EQ(PrivacyScreenTogglePressed(),
              RunRewriter(PrivacyScreenTogglePressed(ui::EF_COMMAND_DOWN)));
  }
}

TEST_F(EventRewriterTest, TestRewriteFunctionKeysInvalidLayout) {
  Preferences::RegisterProfilePrefs(prefs()->registry());

  // Not adding a keyboard simulates a failure in getting top row layout, which
  // will fallback to Layout1 in which case keys are rewritten to their default
  // values.
  EXPECT_EQ(BrowserForwardPressed(), RunRewriter(F2Pressed()));
  EXPECT_EQ(BrowserRefreshPressed(), RunRewriter(F3Pressed()));
  EXPECT_EQ(ZoomTogglePressed(), RunRewriter(F4Pressed()));
  EXPECT_EQ(BrightnessUpPressed(), RunRewriter(F7Pressed()));

  // Adding a keyboard with a valid layout will take effect.
  SetUpKeyboard({.name = "Internal Keyboard",
                 .layout = kKbdTopRowLayout2Tag,
                 .type = ui::INPUT_DEVICE_INTERNAL,
                 .has_custom_top_row = false});
  EXPECT_EQ(BrowserRefreshPressed(), RunRewriter(F2Pressed()));
  EXPECT_EQ(ZoomTogglePressed(), RunRewriter(F3Pressed()));
  EXPECT_EQ(SelectTaskPressed(), RunRewriter(F4Pressed()));
  EXPECT_EQ(MediaPlayPausePressed(), RunRewriter(F7Pressed()));
}

// Tests that event rewrites still work even if modifiers are remapped.
TEST_F(EventRewriterTest, TestRewriteExtendedKeysWithControlRemapped) {
  // Remap Control to Search.
  Preferences::RegisterProfilePrefs(prefs()->registry());
  scoped_feature_list_.InitAndDisableFeature(
      features::kAltClickAndSixPackCustomization);
  IntegerPrefMember search;
  InitModifierKeyPref(&search, ::prefs::kLanguageRemapControlKeyTo,
                      ui::mojom::ModifierKey::kControl,
                      ui::mojom::ModifierKey::kMeta);

  for (const auto& keyboard : kChromeKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);

    EXPECT_EQ(EndPressed(),
              RunRewriter(ArrowRightPressed(ui::EF_CONTROL_DOWN)));
    EXPECT_EQ(EndPressed(ui::EF_SHIFT_DOWN),
              RunRewriter(
                  ArrowRightPressed(ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN)));
  }
}

TEST_F(EventRewriterTest, TestRewriteKeyEventSentByXSendEvent) {
  // Remap Control to Alt.
  Preferences::RegisterProfilePrefs(prefs()->registry());
  IntegerPrefMember control;
  InitModifierKeyPref(&control, ::prefs::kLanguageRemapControlKeyTo,
                      ui::mojom::ModifierKey::kControl,
                      ui::mojom::ModifierKey::kAlt);

  SetUpKeyboard(kInternalChromeKeyboard);

  // Send left control press.
  {
    ui::KeyEvent keyevent(ui::ET_KEY_PRESSED, ui::VKEY_CONTROL,
                          ui::DomCode::CONTROL_LEFT, ui::EF_FINAL,
                          ui::DomKey::CONTROL, ui::EventTimeForNow());
    source().Send(&keyevent);
    auto events = TakeEvents();
    // Control should NOT be remapped to Alt if EF_FINAL is set.
    ASSERT_EQ(1u, events.size());
    ASSERT_TRUE(events[0]->IsKeyEvent());
    EXPECT_EQ(ui::VKEY_CONTROL, events[0]->AsKeyEvent()->key_code());
  }
}

TEST_F(EventRewriterTest, TestRewriteNonNativeEvent) {
  // Remap Control to Alt.
  Preferences::RegisterProfilePrefs(prefs()->registry());
  IntegerPrefMember control;
  InitModifierKeyPref(&control, ::prefs::kLanguageRemapControlKeyTo,
                      ui::mojom::ModifierKey::kControl,
                      ui::mojom::ModifierKey::kAlt);

  SetUpKeyboard(kInternalChromeKeyboard);

  const int kTouchId = 2;
  gfx::Point location(0, 0);
  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED, location, base::TimeTicks(),
      ui::PointerDetails(ui::EventPointerType::kTouch, kTouchId));
  press.set_flags(ui::EF_CONTROL_DOWN);

  source().Send(&press);
  auto events = TakeEvents();
  ASSERT_EQ(1u, events.size());
  // Control should be remapped to Alt.
  EXPECT_EQ(ui::EF_ALT_DOWN,
            events[0]->flags() & (ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN));
}

TEST_F(EventRewriterTest, TopRowKeysAreFunctionKeys) {
  Preferences::RegisterProfilePrefs(prefs()->registry());
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(1));
  wm::ActivateWindow(window.get());

  // Create a simulated keypress of F1 targetted at the window.
  ui::KeyEvent press_f1(ui::ET_KEY_PRESSED, ui::VKEY_F1, ui::DomCode::F1,
                        ui::EF_NONE, ui::DomKey::F1, ui::EventTimeForNow());

  // The event should also not be rewritten if the send-function-keys pref is
  // additionally set, for both apps v2 and regular windows.
  BooleanPrefMember send_function_keys_pref;
  send_function_keys_pref.Init(prefs::kSendFunctionKeys, prefs());
  send_function_keys_pref.SetValue(true);
  keyboard_settings->top_row_are_fkeys = true;
  EXPECT_EQ(F1Pressed(), RunRewriter(F1Pressed()));

  // If the pref isn't set when an event is sent to a regular window, F1 is
  // rewritten to the back key.
  send_function_keys_pref.SetValue(false);
  keyboard_settings->top_row_are_fkeys = false;
  EXPECT_EQ(BrowserBackPressed(), RunRewriter(F1Pressed()));
}

// Parameterized version of test with the same name that accepts the
// event flags that correspond to a right-click. This will be either
// Alt+Click or Search+Click. After a transition period this will
// default to Search+Click and the Alt+Click logic will be removed.
void EventRewriterTest::DontRewriteIfNotRewritten(int right_click_flags) {
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
    ui::MouseEvent press(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                         ui::EventTimeForNow(), right_click_flags,
                         ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_press(&press);
    test_press.set_source_device_id(kTouchpadId1);
    // Sanity check.
    EXPECT_EQ(ui::ET_MOUSE_PRESSED, press.type());
    EXPECT_EQ(right_click_flags, press.flags());
    const ui::MouseEvent result = RewriteMouseButtonEvent(press);
    EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & result.flags());
    EXPECT_NE(right_click_flags, right_click_flags & result.flags());
    EXPECT_EQ(ui::EF_RIGHT_MOUSE_BUTTON, result.changed_button_flags());
  }
  {
    ui::MouseEvent release(ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(),
                           ui::EventTimeForNow(), right_click_flags,
                           ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_release(&release);
    test_release.set_source_device_id(kTouchpadId1);
    const ui::MouseEvent result = RewriteMouseButtonEvent(release);
    EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & result.flags());
    EXPECT_NE(right_click_flags, right_click_flags & result.flags());
    EXPECT_EQ(ui::EF_RIGHT_MOUSE_BUTTON, result.changed_button_flags());
  }

  // No (ALT|SEARCH) in first click.
  {
    ui::MouseEvent press(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                         ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                         ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_press(&press);
    test_press.set_source_device_id(kTouchpadId1);
    const ui::MouseEvent result = RewriteMouseButtonEvent(press);
    EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & result.flags());
    EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, result.changed_button_flags());
  }
  {
    ui::MouseEvent release(ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(),
                           ui::EventTimeForNow(), right_click_flags,
                           ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_release(&release);
    test_release.set_source_device_id(kTouchpadId1);
    const ui::MouseEvent result = RewriteMouseButtonEvent(release);
    EXPECT_EQ(right_click_flags, right_click_flags & result.flags());
    EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, result.changed_button_flags());
  }

  // ALT on different device.
  {
    ui::MouseEvent press(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                         ui::EventTimeForNow(), right_click_flags,
                         ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_press(&press);
    test_press.set_source_device_id(kTouchpadId2);
    const ui::MouseEvent result = RewriteMouseButtonEvent(press);
    EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & result.flags());
    EXPECT_NE(right_click_flags, right_click_flags & result.flags());
    EXPECT_EQ(ui::EF_RIGHT_MOUSE_BUTTON, result.changed_button_flags());
  }
  {
    ui::MouseEvent release(ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(),
                           ui::EventTimeForNow(), right_click_flags,
                           ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_release(&release);
    test_release.set_source_device_id(kTouchpadId1);
    const ui::MouseEvent result = RewriteMouseButtonEvent(release);
    EXPECT_EQ(right_click_flags, right_click_flags & result.flags());
    EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, result.changed_button_flags());
  }
  {
    ui::MouseEvent release(ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(),
                           ui::EventTimeForNow(), right_click_flags,
                           ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_release(&release);
    test_release.set_source_device_id(kTouchpadId2);
    const ui::MouseEvent result = RewriteMouseButtonEvent(release);
    EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & result.flags());
    EXPECT_NE(right_click_flags, right_click_flags & result.flags());
    EXPECT_EQ(ui::EF_RIGHT_MOUSE_BUTTON, result.changed_button_flags());
  }

  // No rewrite for non-touchpad devices.
  {
    ui::MouseEvent press(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                         ui::EventTimeForNow(), right_click_flags,
                         ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_press(&press);
    test_press.set_source_device_id(kMouseId);
    EXPECT_EQ(ui::ET_MOUSE_PRESSED, press.type());
    EXPECT_EQ(right_click_flags, press.flags());
    const ui::MouseEvent result = RewriteMouseButtonEvent(press);
    EXPECT_EQ(right_click_flags, right_click_flags & result.flags());
    EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, result.changed_button_flags());
  }
  {
    ui::MouseEvent release(ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(),
                           ui::EventTimeForNow(), right_click_flags,
                           ui::EF_LEFT_MOUSE_BUTTON);
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
    ui::MouseEvent press(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                         ui::EventTimeForNow(), right_click_flags,
                         ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_press(&press);
    test_press.set_source_device_id(kTouchpadId1);
    // Sanity check.
    EXPECT_EQ(ui::ET_MOUSE_PRESSED, press.type());
    EXPECT_EQ(right_click_flags, press.flags());
    const ui::MouseEvent result = RewriteMouseButtonEvent(press);
    EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & result.flags());
    EXPECT_NE(right_click_flags, right_click_flags & result.flags());
    EXPECT_EQ(ui::EF_RIGHT_MOUSE_BUTTON, result.changed_button_flags());
  }
  {
    ui::MouseEvent release(ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(),
                           ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                           ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_release(&release);
    test_release.set_source_device_id(kTouchpadId1);
    const ui::MouseEvent result = RewriteMouseButtonEvent(release);
    EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & result.flags());
    EXPECT_NE(right_click_flags, right_click_flags & result.flags());
    EXPECT_EQ(ui::EF_RIGHT_MOUSE_BUTTON, result.changed_button_flags());
  }
}

TEST_F(EventRewriterTest, DontRewriteIfNotRewritten_AltClickIsRightClick) {
  scoped_feature_list_.InitAndDisableFeature(
      features::kAltClickAndSixPackCustomization);
  DontRewriteIfNotRewritten(ui::EF_LEFT_MOUSE_BUTTON | ui::EF_ALT_DOWN);
  EXPECT_EQ(message_center_.NotificationCount(), 0u);
}

TEST_F(EventRewriterTest, DontRewriteIfNotRewritten_AltClickIsRightClick_New) {
  // Enabling the kImprovedKeyboardShortcuts feature does not change alt+click
  // behavior or create a notification.
  scoped_feature_list_.InitWithFeatures(
      {::features::kImprovedKeyboardShortcuts},
      {features::kAltClickAndSixPackCustomization});
  DontRewriteIfNotRewritten(ui::EF_LEFT_MOUSE_BUTTON | ui::EF_ALT_DOWN);
  EXPECT_EQ(message_center_.NotificationCount(), 0u);
}

TEST_F(EventRewriterTest, DontRewriteIfNotRewritten_SearchClickIsRightClick) {
  scoped_feature_list_.InitWithFeatures(
      {features::kUseSearchClickForRightClick},
      {features::kAltClickAndSixPackCustomization});
  DontRewriteIfNotRewritten(ui::EF_LEFT_MOUSE_BUTTON | ui::EF_COMMAND_DOWN);
  EXPECT_EQ(message_center_.NotificationCount(), 0u);
}

TEST_F(EventRewriterTest, DontRewriteIfNotRewritten_AltClickDeprecated) {
  // Pressing search+click with alt+click deprecated works, but does not
  // generate a notification.
  scoped_feature_list_.InitWithFeatures(
      {::features::kDeprecateAltClick},
      {features::kAltClickAndSixPackCustomization});
  DontRewriteIfNotRewritten(ui::EF_LEFT_MOUSE_BUTTON | ui::EF_COMMAND_DOWN);
  EXPECT_EQ(message_center_.NotificationCount(), 0u);
}

TEST_F(EventRewriterTest, DeprecatedAltClickGeneratesNotification) {
  scoped_feature_list_.InitWithFeatures(
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
    ui::MouseEvent press(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                         ui::EventTimeForNow(), deprecated_flags,
                         ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_press(&press);
    test_press.set_source_device_id(kTouchpadId1);
    // Sanity check.
    EXPECT_EQ(ui::ET_MOUSE_PRESSED, press.type());
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
    ui::MouseEvent release(ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(),
                           ui::EventTimeForNow(), deprecated_flags,
                           ui::EF_LEFT_MOUSE_BUTTON);
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
    ui::MouseEvent press(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                         ui::EventTimeForNow(), deprecated_flags,
                         ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_press(&press);
    test_press.set_source_device_id(kMouseId);
    EXPECT_EQ(ui::ET_MOUSE_PRESSED, press.type());
    EXPECT_EQ(deprecated_flags, press.flags());
    const ui::MouseEvent result = RewriteMouseButtonEvent(press);
    EXPECT_EQ(deprecated_flags, deprecated_flags & result.flags());
    EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, result.changed_button_flags());

    // No notification expected for this case.
    EXPECT_EQ(message_center_.NotificationCount(), 0u);
  }
  {
    ui::MouseEvent release(ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(),
                           ui::EventTimeForNow(), deprecated_flags,
                           ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_release(&release);
    test_release.set_source_device_id(kMouseId);
    const ui::MouseEvent result = RewriteMouseButtonEvent(release);
    EXPECT_EQ(deprecated_flags, deprecated_flags & result.flags());
    EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, result.changed_button_flags());

    // No notification expected for this case.
    EXPECT_EQ(message_center_.NotificationCount(), 0u);
  }
}

TEST_F(EventRewriterTest, StickyKeyEventDispatchImpl) {
  Shell::Get()->sticky_keys_controller()->Enable(true);
  // Test the actual key event dispatch implementation.
  SendActivateStickyKeyPattern(ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT,
                               ui::DomKey::CONTROL);
  {
    auto events = TakeEvents();
    ASSERT_EQ(1u, events.size());
    ASSERT_EQ(ui::ET_KEY_PRESSED, events[0]->type());
    EXPECT_EQ(ui::VKEY_CONTROL, events[0]->AsKeyEvent()->key_code());
  }

  // Test key press event is correctly modified and modifier release
  // event is sent.
  ui::KeyEvent press(ui::ET_KEY_PRESSED, ui::VKEY_C, ui::DomCode::US_C,
                     ui::EF_NONE, ui::DomKey::Constant<'c'>::Character,
                     ui::EventTimeForNow());
  ui::EventDispatchDetails details = source().Send(&press);
  {
    auto events = TakeEvents();
    ASSERT_EQ(2u, events.size());
    ASSERT_EQ(ui::ET_KEY_PRESSED, events[0]->type());
    EXPECT_EQ(ui::VKEY_C, events[0]->AsKeyEvent()->key_code());
    EXPECT_TRUE(events[0]->flags() & ui::EF_CONTROL_DOWN);

    ASSERT_EQ(ui::ET_KEY_RELEASED, events[1]->type());
    EXPECT_EQ(ui::VKEY_CONTROL, events[1]->AsKeyEvent()->key_code());
  }

  // Test key release event is not modified.
  ui::KeyEvent release(ui::ET_KEY_RELEASED, ui::VKEY_C, ui::DomCode::US_C,
                       ui::EF_NONE, ui::DomKey::Constant<'c'>::Character,
                       ui::EventTimeForNow());
  details = source().Send(&release);
  ASSERT_FALSE(details.dispatcher_destroyed);
  {
    auto events = TakeEvents();
    ASSERT_EQ(1u, events.size());
    ASSERT_EQ(ui::ET_KEY_RELEASED, events[0]->type());
    ASSERT_EQ(ui::VKEY_C, events[0]->AsKeyEvent()->key_code());
    EXPECT_FALSE(events[0]->flags() & ui::EF_CONTROL_DOWN);
  }
}

TEST_F(EventRewriterTest, MouseEventDispatchImpl) {
  Shell::Get()->sticky_keys_controller()->Enable(true);
  SendActivateStickyKeyPattern(ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT,
                               ui::DomKey::CONTROL);
  std::ignore = TakeEvents();

  // Test mouse press event is correctly modified.
  gfx::Point location(0, 0);
  ui::MouseEvent press(ui::ET_MOUSE_PRESSED, location, location,
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                       ui::EF_LEFT_MOUSE_BUTTON);
  ui::EventDispatchDetails details = source().Send(&press);
  ASSERT_FALSE(details.dispatcher_destroyed);
  {
    auto events = TakeEvents();
    ASSERT_EQ(1u, events.size());
    EXPECT_EQ(ui::ET_MOUSE_PRESSED, events[0]->type());
    EXPECT_TRUE(events[0]->flags() & ui::EF_CONTROL_DOWN);
  }

  // Test mouse release event is correctly modified and modifier release
  // event is sent. The mouse event should have the correct DIP location.
  ui::MouseEvent release(ui::ET_MOUSE_RELEASED, location, location,
                         ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                         ui::EF_LEFT_MOUSE_BUTTON);
  details = source().Send(&release);
  ASSERT_FALSE(details.dispatcher_destroyed);
  {
    auto events = TakeEvents();
    ASSERT_EQ(2u, events.size());
    EXPECT_EQ(ui::ET_MOUSE_RELEASED, events[0]->type());
    EXPECT_TRUE(events[0]->flags() & ui::EF_CONTROL_DOWN);

    ASSERT_EQ(ui::ET_KEY_RELEASED, events[1]->type());
    EXPECT_EQ(ui::VKEY_CONTROL, events[1]->AsKeyEvent()->key_code());
  }
}

TEST_F(EventRewriterTest, MouseWheelEventDispatchImpl) {
  Shell::Get()->sticky_keys_controller()->Enable(true);
  // Test positive mouse wheel event is correctly modified and modifier release
  // event is sent.
  SendActivateStickyKeyPattern(ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT,
                               ui::DomKey::CONTROL);
  std::ignore = TakeEvents();

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

    ASSERT_EQ(ui::ET_KEY_RELEASED, events[1]->type());
    EXPECT_EQ(ui::VKEY_CONTROL, events[1]->AsKeyEvent()->key_code());
  }

  // Test negative mouse wheel event is correctly modified and modifier release
  // event is sent.
  SendActivateStickyKeyPattern(ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT,
                               ui::DomKey::CONTROL);
  std::ignore = TakeEvents();

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

    ASSERT_EQ(ui::ET_KEY_RELEASED, events[1]->type());
    EXPECT_EQ(ui::VKEY_CONTROL, events[1]->AsKeyEvent()->key_code());
  }
}

// Tests that if modifier keys are remapped, the flags of a mouse wheel event
// will be rewritten properly.
TEST_F(EventRewriterTest, MouseWheelEventModifiersRewritten) {
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
  // ALT_DOWN.
  details = source().Send(&positive);
  ASSERT_FALSE(details.dispatcher_destroyed);
  {
    auto events = TakeEvents();
    ASSERT_EQ(1u, events.size());
    EXPECT_TRUE(events[0]->IsMouseWheelEvent());
    EXPECT_FALSE(events[0]->flags() & ui::EF_CONTROL_DOWN);
    EXPECT_TRUE(events[0]->flags() & ui::EF_ALT_DOWN);
  }
}

// Tests edge cases of key event rewriting (see https://crbug.com/913209).
TEST_F(EventRewriterTest, KeyEventRewritingEdgeCases) {
  Preferences::RegisterProfilePrefs(prefs()->registry());

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAltClickAndSixPackCustomization);

  // Edge case 1: Press the Launcher button first. Then press the Up Arrow
  // button.
  SendKeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_COMMAND, ui::DomCode::META_LEFT,
               ui::DomKey::META, ui::EF_NONE);
  SendKeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_UP, ui::DomCode::ARROW_UP,
               ui::DomKey::ARROW_UP, ui::EF_COMMAND_DOWN);
  {
    auto events = TakeEvents();
    EXPECT_EQ(2u, events.size());
  }

  // When releasing the Launcher button, the rewritten event should be released
  // as well.
  SendKeyEvent(ui::ET_KEY_RELEASED, ui::VKEY_COMMAND, ui::DomCode::META_LEFT,
               ui::DomKey::META, ui::EF_NONE);
  {
    auto events = TakeEvents();
    ASSERT_EQ(2u, events.size());
    EXPECT_EQ(ui::VKEY_COMMAND, events[0]->AsKeyEvent()->key_code());
    EXPECT_EQ(ui::VKEY_PRIOR, events[1]->AsKeyEvent()->key_code());
  }

  // Edge case 2: Press the Up Arrow button first. Then press the Launch button.
  SendKeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_UP, ui::DomCode::ARROW_UP,
               ui::DomKey::ARROW_UP, ui::EF_NONE);
  SendKeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_COMMAND, ui::DomCode::META_LEFT,
               ui::DomKey::META, ui::EF_NONE);
  {
    auto events = TakeEvents();
    EXPECT_EQ(2u, events.size());
  }

  // When releasing the Up Arrow button, the rewritten event should be blocked.
  SendKeyEvent(ui::ET_KEY_RELEASED, ui::VKEY_UP, ui::DomCode::ARROW_UP,
               ui::DomKey::ARROW_UP, ui::EF_COMMAND_DOWN);
  {
    auto events = TakeEvents();
    ASSERT_EQ(1u, events.size());
    EXPECT_EQ(ui::VKEY_UP, events[0]->AsKeyEvent()->key_code());
  }
}

TEST_F(EventRewriterTest, ScrollEventDispatchImpl) {
  Shell::Get()->sticky_keys_controller()->Enable(true);
  // Test scroll event is correctly modified.
  SendActivateStickyKeyPattern(ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT,
                               ui::DomKey::CONTROL);
  std::ignore = TakeEvents();

  gfx::PointF location(0, 0);
  ui::ScrollEvent scroll(ui::ET_SCROLL, location, location,
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
      ui::ET_SCROLL_FLING_START, location, location, ui::EventTimeForNow(),
      0 /* flag */, 0 /* x_offset */, 0 /* y_offset */,
      0 /* x_offset_ordinal */, 0 /* y_offset_ordinal */, 2 /* finger */);
  details = source().Send(&fling_start);
  {
    auto events = TakeEvents();
    ASSERT_EQ(2u, events.size());
    EXPECT_TRUE(events[0]->IsScrollEvent());
    EXPECT_TRUE(events[0]->flags() & ui::EF_CONTROL_DOWN);

    ASSERT_EQ(ui::ET_KEY_RELEASED, events[1]->type());
    EXPECT_EQ(ui::VKEY_CONTROL, events[1]->AsKeyEvent()->key_code());
  }

  // Test scroll direction change causes that modifier release event is sent.
  SendActivateStickyKeyPattern(ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT,
                               ui::DomKey::CONTROL);
  details = source().Send(&scroll);
  ASSERT_FALSE(details.dispatcher_destroyed);
  std::ignore = TakeEvents();

  ui::ScrollEvent scroll2(ui::ET_SCROLL, location, location,
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

    ASSERT_EQ(ui::ET_KEY_RELEASED, events[1]->type());
    EXPECT_EQ(ui::VKEY_CONTROL, events[1]->AsKeyEvent()->key_code());
  }
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
TEST_F(EventRewriterTest, RemapHangulOnCros1p) {
  scoped_refptr<input_method::MockInputMethodManagerImpl::State> state =
      base::MakeRefCounted<input_method::MockInputMethodManagerImpl::State>(
          input_method_manager_mock_);
  input_method_manager_mock_->SetState(state);

  for (const auto& keyboard : kAllKeyboardVariants) {
    SCOPED_TRACE(keyboard.name);
    SetUpKeyboard(keyboard);
    state->current_input_method_id =
        base::StrCat({kCros1pInputMethodIdPrefix, "ko-t-i0-und"});
    EXPECT_EQ(HangulModePressed(), RunRewriter(HangulModePressed()));
    EXPECT_EQ(LAltPressed(), RunRewriter(LAltPressed()));
    EXPECT_EQ(RAltPressed(), RunRewriter(RAltPressed()));

    state->current_input_method_id =
        base::StrCat({kCros1pInputMethodIdPrefix, "xkb:us::eng"});
    EXPECT_EQ(RAltPressed(), RunRewriter(HangulModePressed()));
    EXPECT_EQ(LAltPressed(), RunRewriter(LAltPressed()));
    EXPECT_EQ(RAltPressed(), RunRewriter(RAltPressed()));
  }
}
#endif

class StickyKeysOverlayTest : public EventRewriterTest {
 public:
  StickyKeysOverlayTest() : overlay_(nullptr) {}

  ~StickyKeysOverlayTest() override {}

  void SetUp() override {
    EventRewriterTest::SetUp();
    auto* sticky_keys_controller = Shell::Get()->sticky_keys_controller();
    sticky_keys_controller->Enable(true);
    overlay_ = sticky_keys_controller->GetOverlayForTest();
    ASSERT_TRUE(overlay_);
  }

  raw_ptr<StickyKeysOverlay, DanglingUntriaged | ExperimentalAsh> overlay_;
};

TEST_F(StickyKeysOverlayTest, OneModifierEnabled) {
  EXPECT_FALSE(overlay_->is_visible());
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));

  // Pressing modifier key should show overlay.
  SendActivateStickyKeyPattern(ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT,
                               ui::DomKey::CONTROL);
  EXPECT_TRUE(overlay_->is_visible());
  EXPECT_EQ(STICKY_KEY_STATE_ENABLED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));

  // Pressing a normal key should hide overlay.
  SendActivateStickyKeyPattern(ui::VKEY_T, ui::DomCode::US_T,
                               ui::DomKey::Constant<'t'>::Character);
  EXPECT_FALSE(overlay_->is_visible());
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));
}

TEST_F(StickyKeysOverlayTest, TwoModifiersEnabled) {
  EXPECT_FALSE(overlay_->is_visible());
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_SHIFT_DOWN));

  // Pressing two modifiers should show overlay.
  SendActivateStickyKeyPattern(ui::VKEY_SHIFT, ui::DomCode::SHIFT_LEFT,
                               ui::DomKey::SHIFT);
  SendActivateStickyKeyPattern(ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT,
                               ui::DomKey::CONTROL);
  EXPECT_TRUE(overlay_->is_visible());
  EXPECT_EQ(STICKY_KEY_STATE_ENABLED,
            overlay_->GetModifierKeyState(ui::EF_SHIFT_DOWN));
  EXPECT_EQ(STICKY_KEY_STATE_ENABLED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));

  // Pressing a normal key should hide overlay.
  SendActivateStickyKeyPattern(ui::VKEY_N, ui::DomCode::US_N,
                               ui::DomKey::Constant<'n'>::Character);
  EXPECT_FALSE(overlay_->is_visible());
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_SHIFT_DOWN));
}

TEST_F(StickyKeysOverlayTest, LockedModifier) {
  EXPECT_FALSE(overlay_->is_visible());
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_ALT_DOWN));

  // Pressing a modifier key twice should lock modifier and show overlay.
  SendActivateStickyKeyPattern(ui::VKEY_LMENU, ui::DomCode::ALT_LEFT,
                               ui::DomKey::ALT);
  SendActivateStickyKeyPattern(ui::VKEY_LMENU, ui::DomCode::ALT_LEFT,
                               ui::DomKey::ALT);
  EXPECT_TRUE(overlay_->is_visible());
  EXPECT_EQ(STICKY_KEY_STATE_LOCKED,
            overlay_->GetModifierKeyState(ui::EF_ALT_DOWN));

  // Pressing a normal key should not hide overlay.
  SendActivateStickyKeyPattern(ui::VKEY_D, ui::DomCode::US_D,
                               ui::DomKey::Constant<'d'>::Character);
  EXPECT_TRUE(overlay_->is_visible());
  EXPECT_EQ(STICKY_KEY_STATE_LOCKED,
            overlay_->GetModifierKeyState(ui::EF_ALT_DOWN));
}

TEST_F(StickyKeysOverlayTest, LockedAndNormalModifier) {
  EXPECT_FALSE(overlay_->is_visible());
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_SHIFT_DOWN));

  // Pressing a modifier key twice should lock modifier and show overlay.
  SendActivateStickyKeyPattern(ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT,
                               ui::DomKey::CONTROL);
  SendActivateStickyKeyPattern(ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT,
                               ui::DomKey::CONTROL);
  EXPECT_TRUE(overlay_->is_visible());
  EXPECT_EQ(STICKY_KEY_STATE_LOCKED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));

  // Pressing another modifier key should still show overlay.
  SendActivateStickyKeyPattern(ui::VKEY_SHIFT, ui::DomCode::SHIFT_LEFT,
                               ui::DomKey::SHIFT);
  EXPECT_TRUE(overlay_->is_visible());
  EXPECT_EQ(STICKY_KEY_STATE_LOCKED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));
  EXPECT_EQ(STICKY_KEY_STATE_ENABLED,
            overlay_->GetModifierKeyState(ui::EF_SHIFT_DOWN));

  // Pressing a normal key should not hide overlay but disable normal modifier.
  SendActivateStickyKeyPattern(ui::VKEY_D, ui::DomCode::US_D,
                               ui::DomKey::Constant<'d'>::Character);
  EXPECT_TRUE(overlay_->is_visible());
  EXPECT_EQ(STICKY_KEY_STATE_LOCKED,
            overlay_->GetModifierKeyState(ui::EF_CONTROL_DOWN));
  EXPECT_EQ(STICKY_KEY_STATE_DISABLED,
            overlay_->GetModifierKeyState(ui::EF_SHIFT_DOWN));
}

TEST_F(StickyKeysOverlayTest, ModifiersDisabled) {
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
  SendActivateStickyKeyPattern(ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT,
                               ui::DomKey::CONTROL);
  SendActivateStickyKeyPattern(ui::VKEY_SHIFT, ui::DomCode::SHIFT_LEFT,
                               ui::DomKey::SHIFT);
  SendActivateStickyKeyPattern(ui::VKEY_SHIFT, ui::DomCode::SHIFT_LEFT,
                               ui::DomKey::SHIFT);
  SendActivateStickyKeyPattern(ui::VKEY_LMENU, ui::DomCode::ALT_LEFT,
                               ui::DomKey::ALT);
  SendActivateStickyKeyPattern(ui::VKEY_COMMAND, ui::DomCode::META_LEFT,
                               ui::DomKey::META);
  SendActivateStickyKeyPattern(ui::VKEY_COMMAND, ui::DomCode::META_LEFT,
                               ui::DomKey::META);

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
  SendActivateStickyKeyPattern(ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT,
                               ui::DomKey::CONTROL);
  SendActivateStickyKeyPattern(ui::VKEY_CONTROL, ui::DomCode::CONTROL_LEFT,
                               ui::DomKey::CONTROL);
  SendActivateStickyKeyPattern(ui::VKEY_SHIFT, ui::DomCode::SHIFT_LEFT,
                               ui::DomKey::SHIFT);
  SendActivateStickyKeyPattern(ui::VKEY_LMENU, ui::DomCode::ALT_LEFT,
                               ui::DomKey::ALT);
  SendActivateStickyKeyPattern(ui::VKEY_LMENU, ui::DomCode::ALT_LEFT,
                               ui::DomKey::ALT);
  SendActivateStickyKeyPattern(ui::VKEY_COMMAND, ui::DomCode::META_LEFT,
                               ui::DomKey::META);

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

TEST_F(StickyKeysOverlayTest, ModifierVisibility) {
  // All but AltGr and Mod3 should initially be visible.
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_CONTROL_DOWN));
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_SHIFT_DOWN));
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_ALT_DOWN));
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_COMMAND_DOWN));
  EXPECT_FALSE(overlay_->GetModifierVisible(ui::EF_ALTGR_DOWN));
  EXPECT_FALSE(overlay_->GetModifierVisible(ui::EF_MOD3_DOWN));

  // Turn all modifiers on.
  auto* sticky_keys_controller = Shell::Get()->sticky_keys_controller();
  sticky_keys_controller->SetModifiersEnabled(true, true);
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_CONTROL_DOWN));
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_SHIFT_DOWN));
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_ALT_DOWN));
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_COMMAND_DOWN));
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_ALTGR_DOWN));
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_MOD3_DOWN));

  // Turn off Mod3.
  sticky_keys_controller->SetModifiersEnabled(false, true);
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_ALTGR_DOWN));
  EXPECT_FALSE(overlay_->GetModifierVisible(ui::EF_MOD3_DOWN));

  // Turn off AltGr.
  sticky_keys_controller->SetModifiersEnabled(true, false);
  EXPECT_FALSE(overlay_->GetModifierVisible(ui::EF_ALTGR_DOWN));
  EXPECT_TRUE(overlay_->GetModifierVisible(ui::EF_MOD3_DOWN));

  // Turn off AltGr and Mod3.
  sticky_keys_controller->SetModifiersEnabled(false, false);
  EXPECT_FALSE(overlay_->GetModifierVisible(ui::EF_ALTGR_DOWN));
  EXPECT_FALSE(overlay_->GetModifierVisible(ui::EF_MOD3_DOWN));
}

TEST_F(EventRewriterTest, RewrittenModifier) {
  // Register Control + B as an extension shortcut.
  SetExtensionCommands({{{ui::VKEY_B, ui::EF_CONTROL_DOWN}}});

  // Check that standard extension input has no rewritten modifiers.
  EXPECT_EQ(BPressed(ui::EF_CONTROL_DOWN),
            RunRewriter(BPressed(ui::EF_CONTROL_DOWN)));

  // Remap Control -> Alt.
  IntegerPrefMember control;
  InitModifierKeyPref(&control, ::prefs::kLanguageRemapControlKeyTo,
                      ui::mojom::ModifierKey::kControl,
                      ui::mojom::ModifierKey::kAlt);
  // Pressing Control + B should now be remapped to Alt + B.
  EXPECT_EQ(BPressed(ui::EF_ALT_DOWN),
            RunRewriter(BPressed(ui::EF_CONTROL_DOWN)));

  // Remap Alt -> Control.
  IntegerPrefMember alt;
  InitModifierKeyPref(&alt, ::prefs::kLanguageRemapAltKeyTo,
                      ui::mojom::ModifierKey::kAlt,
                      ui::mojom::ModifierKey::kControl);
  // Pressing Alt + B should now be remapped to Control + B.
  EXPECT_EQ(BPressed(ui::EF_CONTROL_DOWN),
            RunRewriter(BPressed(ui::EF_ALT_DOWN)));

  // Remove all extension shortcuts and still expect the remapping to work.
  SetExtensionCommands(std::nullopt);

  EXPECT_EQ(BPressed(ui::EF_ALT_DOWN),
            RunRewriter(BPressed(ui::EF_CONTROL_DOWN)));
  EXPECT_EQ(BPressed(ui::EF_CONTROL_DOWN),
            RunRewriter(BPressed(ui::EF_ALT_DOWN)));
}

TEST_F(EventRewriterTest, RewriteNumpadExtensionCommand) {
  // Register Control + NUMPAD1 as an extension shortcut.
  SetExtensionCommands({{{ui::VKEY_NUMPAD1, ui::EF_CONTROL_DOWN}}});
  // Check that extension shortcuts that involve numpads keys are properly
  // rewritten. Note that VKEY_END is associated with NUMPAD1 if Num Lock is
  // disabled. The result should be "NumPad 1 with Control".
  EXPECT_EQ(Numpad1Pressed(ui::EF_CONTROL_DOWN),
            RunRewriter(NumpadEndPressed(ui::EF_CONTROL_DOWN)));

  // Remove the extension shortcut and expect the numpad event to still be
  // rewritten.
  SetExtensionCommands(std::nullopt);
  EXPECT_EQ(Numpad1Pressed(ui::EF_CONTROL_DOWN),
            RunRewriter(NumpadEndPressed(ui::EF_CONTROL_DOWN)));
}

class ModifierPressedMetricsTest
    : public EventRewriterTest,
      public testing::WithParamInterface<std::tuple<TestKeyEvent,
                                                    ui::ModifierKeyUsageMetric,
                                                    std::vector<std::string>>> {
 public:
  void SetUp() override {
    std::tie(event_, modifier_key_usage_mapping_, key_pref_names_) = GetParam();
    scoped_feature_list_.InitAndDisableFeature(
        features::kInputDeviceSettingsSplit);
    EventRewriterTest::SetUp();
  }

 protected:
  TestKeyEvent event_;
  ui::ModifierKeyUsageMetric modifier_key_usage_mapping_;
  std::vector<std::string> key_pref_names_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ModifierPressedMetricsTest,
    testing::ValuesIn(std::vector<std::tuple<TestKeyEvent,
                                             ui::ModifierKeyUsageMetric,
                                             std::vector<std::string>>>{
        {LWinPressed(),
         ui::ModifierKeyUsageMetric::kMetaLeft,
         {::prefs::kLanguageRemapSearchKeyTo,
          ::prefs::kLanguageRemapExternalCommandKeyTo,
          ::prefs::kLanguageRemapExternalMetaKeyTo}},
        {RWinPressed(),
         ui::ModifierKeyUsageMetric::kMetaRight,
         {::prefs::kLanguageRemapSearchKeyTo,
          ::prefs::kLanguageRemapExternalCommandKeyTo,
          ::prefs::kLanguageRemapExternalMetaKeyTo}},
        {LControlPressed(),
         ui::ModifierKeyUsageMetric::kControlLeft,
         {::prefs::kLanguageRemapControlKeyTo}},
        {RControlPressed(),
         ui::ModifierKeyUsageMetric::kControlRight,
         {::prefs::kLanguageRemapControlKeyTo}},
        {LAltPressed(),
         ui::ModifierKeyUsageMetric::kAltLeft,
         {::prefs::kLanguageRemapAltKeyTo}},
        {RAltPressed(),
         ui::ModifierKeyUsageMetric::kAltRight,
         {::prefs::kLanguageRemapAltKeyTo}},
        {LShiftPressed(),
         ui::ModifierKeyUsageMetric::kShiftLeft,
         // Shift keys cannot be remapped and therefore do not have a real
         // "pref" path.
         {"fakePrefPath"}},
        {RShiftPressed(),
         ui::ModifierKeyUsageMetric::kShiftRight,
         // Shift keys cannot be remapped and therefore do not have a real
         // "pref" path.
         {"fakePrefPath"}},
        {CapsLockPressed(),
         ui::ModifierKeyUsageMetric::kCapsLock,
         {::prefs::kLanguageRemapCapsLockKeyTo}},
        {BackspacePressed(),
         ui::ModifierKeyUsageMetric::kBackspace,
         {::prefs::kLanguageRemapBackspaceKeyTo}},
        {EscapePressed(),
         ui::ModifierKeyUsageMetric::kEscape,
         {::prefs::kLanguageRemapEscapeKeyTo}},
        {LaunchAssistantPressed(),
         ui::ModifierKeyUsageMetric::kAssistant,
         {::prefs::kLanguageRemapAssistantKeyTo}}}));

TEST_P(ModifierPressedMetricsTest, KeyPressedTest) {
  auto expected = event_;
  if (expected.code == ui::DomCode::CAPS_LOCK) {
    expected.flags |= ui::EF_CAPS_LOCK_ON;
  }

  base::HistogramTester histogram_tester;
  SetUpKeyboard(kInternalChromeKeyboard);
  EXPECT_EQ(expected, RunRewriter(event_));
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.ModifierPressed.Internal",
      modifier_key_usage_mapping_, 1);
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.RemappedModifierPressed.Internal",
      modifier_key_usage_mapping_, 1);

  SetUpKeyboard(kExternalChromeKeyboard);
  EXPECT_EQ(expected, RunRewriter(event_));
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.ModifierPressed.CrOSExternal",
      modifier_key_usage_mapping_, 1);
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.RemappedModifierPressed.CrOSExternal",
      modifier_key_usage_mapping_, 1);

  SetUpKeyboard(kExternalAppleKeyboard);
  EXPECT_EQ(expected, RunRewriter(event_));
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.ModifierPressed.AppleExternal",
      modifier_key_usage_mapping_, 1);
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.RemappedModifierPressed.AppleExternal",
      modifier_key_usage_mapping_, 1);

  SetUpKeyboard(kExternalGenericKeyboard);
  EXPECT_EQ(expected, RunRewriter(event_));
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.ModifierPressed.External",
      modifier_key_usage_mapping_, 1);
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.RemappedModifierPressed.External",
      modifier_key_usage_mapping_, 1);
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
  EXPECT_EQ(BackspacePressed(), RunRewriter(event_));
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.ModifierPressed.Internal",
      modifier_key_usage_mapping_, 1);
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.RemappedModifierPressed.Internal",
      ui::ModifierKeyUsageMetric::kBackspace, 1);

  SetUpKeyboard(kExternalChromeKeyboard);
  EXPECT_EQ(BackspacePressed(), RunRewriter(event_));
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.ModifierPressed.CrOSExternal",
      modifier_key_usage_mapping_, 1);
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.RemappedModifierPressed.CrOSExternal",
      ui::ModifierKeyUsageMetric::kBackspace, 1);

  SetUpKeyboard(kExternalAppleKeyboard);
  EXPECT_EQ(BackspacePressed(), RunRewriter(event_));
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.ModifierPressed.AppleExternal",
      modifier_key_usage_mapping_, 1);
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.RemappedModifierPressed.AppleExternal",
      ui::ModifierKeyUsageMetric::kBackspace, 1);

  SetUpKeyboard(kExternalGenericKeyboard);
  EXPECT_EQ(BackspacePressed(), RunRewriter(event_));
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
  const auto control_event = right ? RControlPressed() : LControlPressed();

  for (const auto& pref_name : key_pref_names_) {
    IntegerPrefMember pref_member;
    InitModifierKeyPref(&pref_member, pref_name,
                        ui::mojom::ModifierKey::kControl,
                        ui::mojom::ModifierKey::kControl);
  }

  SetUpKeyboard(kInternalChromeKeyboard);
  EXPECT_EQ(control_event, RunRewriter(event_));
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.ModifierPressed.Internal",
      modifier_key_usage_mapping_, 1);
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.RemappedModifierPressed.Internal",
      remapped_modifier_key_usage_mapping, 1);

  SetUpKeyboard(kExternalChromeKeyboard);
  EXPECT_EQ(control_event, RunRewriter(event_));
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.ModifierPressed.CrOSExternal",
      modifier_key_usage_mapping_, 1);
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.RemappedModifierPressed.CrOSExternal",
      remapped_modifier_key_usage_mapping, 1);

  SetUpKeyboard(kExternalAppleKeyboard);
  EXPECT_EQ(control_event, RunRewriter(event_));
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.ModifierPressed.AppleExternal",
      modifier_key_usage_mapping_, 1);
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.RemappedModifierPressed.AppleExternal",
      remapped_modifier_key_usage_mapping, 1);

  SetUpKeyboard(kExternalGenericKeyboard);
  EXPECT_EQ(control_event, RunRewriter(event_));
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.ModifierPressed.External",
      modifier_key_usage_mapping_, 1);
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.RemappedModifierPressed.External",
      remapped_modifier_key_usage_mapping, 1);
}

TEST_P(ModifierPressedMetricsTest, KeyRepeatTest) {
  base::HistogramTester histogram_tester;
  // No metrics should be published if it is a repeated key.
  event_.flags |= ui::EF_IS_REPEAT;

  auto expected = event_;
  if (expected.code == ui::DomCode::CAPS_LOCK) {
    expected.flags |= ui::EF_CAPS_LOCK_ON;
  }

  SetUpKeyboard(kInternalChromeKeyboard);
  EXPECT_EQ(expected, RunRewriter(event_));
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.ModifierPressed.Internal",
      modifier_key_usage_mapping_, 0);
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.RemappedModifierPressed.Internal",
      modifier_key_usage_mapping_, 0);

  SetUpKeyboard(kExternalChromeKeyboard);
  EXPECT_EQ(expected, RunRewriter(event_));
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.ModifierPressed.CrOSExternal",
      modifier_key_usage_mapping_, 0);
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.RemappedModifierPressed.CrOSExternal",
      modifier_key_usage_mapping_, 0);

  SetUpKeyboard(kExternalAppleKeyboard);
  EXPECT_EQ(expected, RunRewriter(event_));
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.ModifierPressed.AppleExternal",
      modifier_key_usage_mapping_, 0);
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.RemappedModifierPressed.AppleExternal",
      modifier_key_usage_mapping_, 0);

  SetUpKeyboard(kExternalGenericKeyboard);
  EXPECT_EQ(expected, RunRewriter(event_));
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.ModifierPressed.External",
      modifier_key_usage_mapping_, 0);
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.RemappedModifierPressed.External",
      modifier_key_usage_mapping_, 0);
}

TEST_P(ModifierPressedMetricsTest, KeyReleasedTest) {
  base::HistogramTester histogram_tester;
  // No metrics should be published if it is a repeated key.
  event_.flags |= ui::EF_IS_REPEAT;

  auto expected = event_;
  if (expected.code == ui::DomCode::CAPS_LOCK) {
    expected.flags |= ui::EF_CAPS_LOCK_ON;
  }

  SetUpKeyboard(kInternalChromeKeyboard);
  EXPECT_EQ(expected, RunRewriter(event_));
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.ModifierPressed.Internal",
      modifier_key_usage_mapping_, 0);
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.RemappedModifierPressed.Internal",
      modifier_key_usage_mapping_, 0);

  SetUpKeyboard(kExternalChromeKeyboard);
  EXPECT_EQ(expected, RunRewriter(event_));
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.ModifierPressed.CrOSExternal",
      modifier_key_usage_mapping_, 0);
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.RemappedModifierPressed.CrOSExternal",
      modifier_key_usage_mapping_, 0);

  SetUpKeyboard(kExternalAppleKeyboard);
  EXPECT_EQ(expected, RunRewriter(event_));
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.ModifierPressed.AppleExternal",
      modifier_key_usage_mapping_, 0);
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.RemappedModifierPressed.AppleExternal",
      modifier_key_usage_mapping_, 0);

  SetUpKeyboard(kExternalGenericKeyboard);
  EXPECT_EQ(expected, RunRewriter(event_));
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.ModifierPressed.External",
      modifier_key_usage_mapping_, 0);
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Inputs.Keyboard.RemappedModifierPressed.External",
      modifier_key_usage_mapping_, 0);
}

class EventRewriterSixPackKeysTest : public EventRewriterTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kInputDeviceSettingsSplit,
         features::kAltClickAndSixPackCustomization},
        /*disabled_features=*/{});
    EventRewriterTest::SetUp();
  }
};

TEST_F(EventRewriterSixPackKeysTest, TestRewriteSixPackKeysSearchVariants) {
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
    EXPECT_EQ(
        InsertPressed(),
        RunRewriter(BackspacePressed(ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN)));
    // Search+Backspace -> Delete
    EXPECT_EQ(DeletePressed(),
              RunRewriter(BackspacePressed(ui::EF_COMMAND_DOWN)));
    // Search+Up -> Prior (aka PageUp)
    EXPECT_EQ(PageUpPressed(),
              RunRewriter(ArrowUpPressed(ui::EF_COMMAND_DOWN)));
    // Search+Down -> Next (aka PageDown)
    EXPECT_EQ(PageDownPressed(),
              RunRewriter(ArrowDownPressed(ui::EF_COMMAND_DOWN)));
    // Search+Left -> Home
    EXPECT_EQ(HomePressed(),
              RunRewriter(ArrowLeftPressed(ui::EF_COMMAND_DOWN)));
    // Search+Right -> End
    EXPECT_EQ(EndPressed(),
              RunRewriter(ArrowRightPressed(ui::EF_COMMAND_DOWN)));
    // Search+Shift+Down -> Shift+Next (aka PageDown)
    EXPECT_EQ(
        PageDownPressed(ui::EF_SHIFT_DOWN),
        RunRewriter(ArrowDownPressed(ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN)));
    // Search+Ctrl+Up -> Ctrl+Prior (aka PageUp)
    EXPECT_EQ(
        PageUpPressed(ui::EF_CONTROL_DOWN),
        RunRewriter(ArrowUpPressed(ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN)));
    // Search+Alt+Left -> Alt+Home
    EXPECT_EQ(
        HomePressed(ui::EF_ALT_DOWN),
        RunRewriter(ArrowLeftPressed(ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN)));
  }
}

TEST_F(EventRewriterSixPackKeysTest, TestRewriteSixPackKeysAltVariants) {
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
    EXPECT_EQ(DeletePressed(), RunRewriter(BackspacePressed(ui::EF_ALT_DOWN)));
    // Alt+Up -> Prior
    EXPECT_EQ(PageUpPressed(), RunRewriter(ArrowUpPressed(ui::EF_ALT_DOWN)));
    // Alt+Down -> Next
    EXPECT_EQ(PageDownPressed(),
              RunRewriter(ArrowDownPressed(ui::EF_ALT_DOWN)));
    // Ctrl+Alt+Up -> Home
    EXPECT_EQ(HomePressed(), RunRewriter(ArrowUpPressed(ui::EF_CONTROL_DOWN |
                                                        ui::EF_ALT_DOWN)));
    // Ctrl+Alt+Down -> End
    EXPECT_EQ(EndPressed(), RunRewriter(ArrowDownPressed(ui::EF_CONTROL_DOWN |
                                                         ui::EF_ALT_DOWN)));
    // Ctrl+Alt+Shift+Up -> Shift+Home
    EXPECT_EQ(HomePressed(ui::EF_SHIFT_DOWN),
              RunRewriter(ArrowUpPressed(ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN |
                                         ui::EF_SHIFT_DOWN)));
    // Ctrl+Alt+Search+Down -> Search+End
    EXPECT_EQ(
        EndPressed(ui::EF_COMMAND_DOWN),
        RunRewriter(ArrowDownPressed(ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN |
                                     ui::EF_COMMAND_DOWN)));
  }
}

TEST_F(EventRewriterSixPackKeysTest, TestRewriteSixPackKeysBlockedBySetting) {
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
  EXPECT_EQ(BackspacePressed(ui::EF_ALT_DOWN),
            RunRewriter(BackspacePressed(ui::EF_ALT_DOWN)));
  EXPECT_EQ(1u, message_center_.NotificationCount());
  ClearNotifications();

  settings.six_pack_key_remappings->del =
      ui::mojom::SixPackShortcutModifier::kAlt;
  // Rewrite should occur now that the alt rewrite is the current setting.
  // Alt+Backspace -> Delete
  EXPECT_EQ(DeletePressed(), RunRewriter(BackspacePressed(ui::EF_ALT_DOWN)));

  settings.six_pack_key_remappings->del =
      ui::mojom::SixPackShortcutModifier::kNone;
  // No rewrite should occur since remapping a key event to the "Delete"
  // 6-pack key is disabled.
  EXPECT_EQ(BackspacePressed(ui::EF_ALT_DOWN),
            RunRewriter(BackspacePressed(ui::EF_ALT_DOWN)));
  EXPECT_EQ(1u, message_center_.NotificationCount());
  ClearNotifications();
}

class EventRewriterExtendedFkeysTest : public EventRewriterTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {ash::features::kInputDeviceSettingsSplit,
         ::features::kSupportF11AndF12KeyShortcuts},
        {});
    EventRewriterTest::SetUp();
  }
};

TEST_F(EventRewriterExtendedFkeysTest, TestRewriteExtendedFkeys) {
  Preferences::RegisterProfilePrefs(prefs()->registry());
  mojom::KeyboardSettings settings;
  settings.f11 = ui::mojom::ExtendedFkeysModifier::kAlt;
  settings.f12 = ui::mojom::ExtendedFkeysModifier::kShift;
  settings.top_row_are_fkeys = true;

  EXPECT_CALL(*input_device_settings_controller_mock_,
              GetKeyboardSettings(kKeyboardDeviceId))
      .WillRepeatedly(testing::Return(&settings));

  SetUpKeyboard(kInternalChromeKeyboard);
  EXPECT_EQ(F11Pressed(), RunRewriter(F1Pressed(ui::EF_ALT_DOWN)));
  EXPECT_EQ(F12Pressed(), RunRewriter(F2Pressed(ui::EF_SHIFT_DOWN)));

  settings.f11 = ui::mojom::ExtendedFkeysModifier::kCtrlShift;
  settings.f12 = ui::mojom::ExtendedFkeysModifier::kAlt;

  EXPECT_EQ(F11Pressed(),
            RunRewriter(F1Pressed(ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN)));
  EXPECT_EQ(F12Pressed(), RunRewriter(F2Pressed(ui::EF_ALT_DOWN)));
}

TEST_F(EventRewriterExtendedFkeysTest,
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

  EXPECT_EQ(F1Pressed(ui::EF_ALT_DOWN),
            RunRewriter(F1Pressed(ui::EF_ALT_DOWN)));
}

TEST_F(EventRewriterExtendedFkeysTest, TestRewriteExtendedFkeysTopRowAreFkeys) {
  Preferences::RegisterProfilePrefs(prefs()->registry());
  mojom::KeyboardSettings settings;
  settings.f11 = ui::mojom::ExtendedFkeysModifier::kAlt;
  settings.f12 = ui::mojom::ExtendedFkeysModifier::kShift;
  settings.top_row_are_fkeys = true;

  EXPECT_CALL(*input_device_settings_controller_mock_,
              GetKeyboardSettings(kKeyboardDeviceId))
      .WillRepeatedly(testing::Return(&settings));
  SetUpKeyboard(kInternalChromeKeyboard);
  EXPECT_EQ(F11Pressed(), RunRewriter(F1Pressed(ui::EF_ALT_DOWN)));
  EXPECT_EQ(F11Pressed(ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN),
            RunRewriter(F1Pressed(ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN |
                                  ui::EF_ALT_DOWN)));
  EXPECT_EQ(F12Pressed(), RunRewriter(F2Pressed(ui::EF_SHIFT_DOWN)));

  settings.top_row_are_fkeys = false;
  EXPECT_EQ(F11Pressed(),
            RunRewriter(F1Pressed(ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN)));
  EXPECT_EQ(F12Pressed(),
            RunRewriter(F2Pressed(ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN)));
}

class EventRewriterSettingsSplitTest : public EventRewriterTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kInputDeviceSettingsSplit);
    EventRewriterTest::SetUp();
  }
};

TEST_F(EventRewriterSettingsSplitTest, TopRowAreFKeys) {
  mojom::KeyboardSettings settings;
  EXPECT_CALL(*input_device_settings_controller_mock_,
              GetKeyboardSettings(kKeyboardDeviceId))
      .WillRepeatedly(testing::Return(&settings));
  SetUpKeyboard(kExternalGenericKeyboard);

  settings.top_row_are_fkeys = false;
  settings.suppress_meta_fkey_rewrites = false;

  EXPECT_EQ(BrowserBackPressed(), RunRewriter(F1Pressed()));

  settings.top_row_are_fkeys = true;
  EXPECT_EQ(F1Pressed(), RunRewriter(F1Pressed()));
}

TEST_F(EventRewriterSettingsSplitTest, RewriteMetaTopRowKeyComboEvents) {
  mojom::KeyboardSettings settings;
  settings.top_row_are_fkeys = true;
  EXPECT_CALL(*input_device_settings_controller_mock_,
              GetKeyboardSettings(kKeyboardDeviceId))
      .WillRepeatedly(testing::Return(&settings));
  SetUpKeyboard(kExternalGenericKeyboard);

  settings.suppress_meta_fkey_rewrites = false;
  EXPECT_EQ(BrowserBackPressed(), RunRewriter(F1Pressed(ui::EF_COMMAND_DOWN)));

  settings.suppress_meta_fkey_rewrites = true;
  EXPECT_EQ(F1Pressed(ui::EF_COMMAND_DOWN),
            RunRewriter(F1Pressed(ui::EF_COMMAND_DOWN)));
}

TEST_F(EventRewriterSettingsSplitTest, ModifierRemapping) {
  mojom::KeyboardSettings settings;
  EXPECT_CALL(*input_device_settings_controller_mock_,
              GetKeyboardSettings(kKeyboardDeviceId))
      .WillRepeatedly(testing::Return(&settings));
  SetUpKeyboard(kExternalGenericKeyboard);

  settings.modifier_remappings = {
      {ui::mojom::ModifierKey::kAlt, ui::mojom::ModifierKey::kControl},
      {ui::mojom::ModifierKey::kMeta, ui::mojom::ModifierKey::kBackspace}};

  // Test remapping modifier keys.
  EXPECT_EQ(RControlPressed(), RunRewriter(RAltPressed()));
  EXPECT_EQ(BackspacePressed(), RunRewriter(LWinPressed()));
  EXPECT_EQ(LControlPressed(), RunRewriter(LControlPressed()));

  // Test remapping modifier flags.
  EXPECT_EQ(APressed(ui::EF_CONTROL_DOWN),
            RunRewriter(APressed(ui::EF_ALT_DOWN)));
  EXPECT_EQ(APressed(), RunRewriter(APressed(ui::EF_COMMAND_DOWN)));
  EXPECT_EQ(APressed(ui::EF_CONTROL_DOWN),
            RunRewriter(APressed(ui::EF_CONTROL_DOWN)));
}

class KeyEventRemappedToSixPackKeyTest
    : public EventRewriterTest,
      public testing::WithParamInterface<
          std::tuple<ui::KeyboardCode, bool, int, const char*>> {
 public:
  void SetUp() override {
    EventRewriterTest::SetUp();
    std::tie(key_code_, alt_based_, expected_pref_value_, pref_name_) =
        GetParam();
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
    testing::ValuesIn(std::vector<
                      std::tuple<ui::KeyboardCode, bool, int, const char*>>{
        {ui::VKEY_DELETE, false, -1, prefs::kKeyEventRemappedToSixPackDelete},
        {ui::VKEY_HOME, true, 1, prefs::kKeyEventRemappedToSixPackHome},
        {ui::VKEY_PRIOR, false, -1, prefs::kKeyEventRemappedToSixPackPageDown},
        {ui::VKEY_END, true, 1, prefs::kKeyEventRemappedToSixPackEnd},
        {ui::VKEY_NEXT, false, -1, prefs::kKeyEventRemappedToSixPackPageUp}}));

TEST_P(KeyEventRemappedToSixPackKeyTest, KeyEventRemappedTest) {
  Preferences::RegisterProfilePrefs(prefs()->registry());
  IntegerPrefMember int_pref;
  int_pref.Init(pref_name_, prefs());
  int_pref.SetValue(0);
  delegate_->RecordSixPackEventRewrite(key_code_, alt_based_);
  EXPECT_EQ(expected_pref_value_, prefs()->GetInteger(pref_name_));
}

class EventRewriterRemapToRightClickTest
    : public EventRewriterTest,
      public message_center::MessageCenterObserver {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kInputDeviceSettingsSplit,
         features::kAltClickAndSixPackCustomization},
        /*disabled_features=*/{});
    EventRewriterTest::SetUp();

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
    EventRewriterTest::TearDown();
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

TEST_F(EventRewriterRemapToRightClickTest, AltClickRemappedToRightClick) {
  SetSimulateRightClickSetting(ui::mojom::SimulateRightClickModifier::kAlt);
  int flag_masks = ui::EF_ALT_DOWN | ui::EF_LEFT_MOUSE_BUTTON;

  ui::MouseEvent press(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), flag_masks,
                       ui::EF_LEFT_MOUSE_BUTTON);
  ui::EventTestApi test_press(&press);
  test_press.set_source_device_id(kTouchpadId1);
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, press.type());
  EXPECT_EQ(flag_masks, press.flags());
  const ui::MouseEvent result = RewriteMouseButtonEvent(press);
  EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & result.flags());
  EXPECT_NE(flag_masks, flag_masks & result.flags());
  EXPECT_EQ(ui::EF_RIGHT_MOUSE_BUTTON, result.changed_button_flags());
}

TEST_F(EventRewriterRemapToRightClickTest, SearchClickRemappedToRightClick) {
  SetSimulateRightClickSetting(ui::mojom::SimulateRightClickModifier::kSearch);
  int flag_masks = ui::EF_COMMAND_DOWN | ui::EF_LEFT_MOUSE_BUTTON;

  ui::MouseEvent press(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), flag_masks,
                       ui::EF_LEFT_MOUSE_BUTTON);
  ui::EventTestApi test_press(&press);
  test_press.set_source_device_id(kTouchpadId1);
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, press.type());
  EXPECT_EQ(flag_masks, press.flags());
  const ui::MouseEvent result = RewriteMouseButtonEvent(press);
  EXPECT_TRUE(ui::EF_RIGHT_MOUSE_BUTTON & result.flags());
  EXPECT_NE(flag_masks, flag_masks & result.flags());
  EXPECT_EQ(ui::EF_RIGHT_MOUSE_BUTTON, result.changed_button_flags());
}

TEST_F(EventRewriterRemapToRightClickTest, RemapToRightClickBlockedBySetting) {
  ui::DeviceDataManager* device_data_manager =
      ui::DeviceDataManager::GetInstance();
  std::vector<ui::TouchpadDevice> touchpad_devices(1);
  touchpad_devices[0].id = kTouchpadId1;
  static_cast<ui::DeviceHotplugEventObserver*>(device_data_manager)
      ->OnTouchpadDevicesUpdated(touchpad_devices);
  SetSimulateRightClickSetting(ui::mojom::SimulateRightClickModifier::kAlt);

  {
    ui::MouseEvent press(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
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
  {
    SetSimulateRightClickSetting(
        ui::mojom::SimulateRightClickModifier::kSearch);
    ui::MouseEvent press(
        ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(), ui::EventTimeForNow(),
        ui::EF_ALT_DOWN | ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
    ui::EventTestApi test_press(&press);
    test_press.set_source_device_id(kTouchpadId1);
    const ui::MouseEvent result = RewriteMouseButtonEvent(press);
    EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & result.flags());
    EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, result.changed_button_flags());
    EXPECT_EQ(notification_count(), 2);
  }
}

TEST_F(EventRewriterRemapToRightClickTest, RemapToRightClickIsDisabled) {
  ui::DeviceDataManager* device_data_manager =
      ui::DeviceDataManager::GetInstance();
  std::vector<ui::TouchpadDevice> touchpad_devices(1);
  touchpad_devices[0].id = kTouchpadId1;
  static_cast<ui::DeviceHotplugEventObserver*>(device_data_manager)
      ->OnTouchpadDevicesUpdated(touchpad_devices);
  SetSimulateRightClickSetting(ui::mojom::SimulateRightClickModifier::kNone);

  ui::MouseEvent press(
      ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(), ui::EventTimeForNow(),
      ui::EF_COMMAND_DOWN | ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  ui::EventTestApi test_press(&press);
  test_press.set_source_device_id(kTouchpadId1);
  const ui::MouseEvent result = RewriteMouseButtonEvent(press);
  EXPECT_TRUE(ui::EF_LEFT_MOUSE_BUTTON & result.flags());
  EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, result.changed_button_flags());
  EXPECT_EQ(notification_count(), 1);
}

class FKeysRewritingPeripheralCustomizationTest : public EventRewriterTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures({features::kInputDeviceSettingsSplit,
                                           features::kPeripheralCustomization},
                                          {});
    EventRewriterTest::SetUp();
  }

 protected:
  mojom::MouseSettings mouse_settings_;
  mojom::KeyboardSettings keyboard_settings_;
};

TEST_F(FKeysRewritingPeripheralCustomizationTest, FKeysNotRewritten) {
  EXPECT_CALL(*input_device_settings_controller_mock_,
              GetKeyboardSettings(kMouseDeviceId))
      .WillRepeatedly(testing::Return(nullptr));
  EXPECT_CALL(*input_device_settings_controller_mock_,
              GetMouseSettings(kMouseDeviceId))
      .WillRepeatedly(testing::Return(&mouse_settings_));

  SetUpKeyboard(kExternalGenericKeyboard);

  // Mice that press F-Keys do not get rewritten to actions.
  EXPECT_EQ(F1Pressed(), RunRewriter(F1Pressed(), kMouseDeviceId));
  EXPECT_EQ(F2Pressed(), RunRewriter(F2Pressed(), kMouseDeviceId));
  EXPECT_EQ(F1Pressed(ui::EF_COMMAND_DOWN),
            RunRewriter(F1Pressed(ui::EF_COMMAND_DOWN), kMouseDeviceId));
  EXPECT_EQ(F2Pressed(ui::EF_COMMAND_DOWN),
            RunRewriter(F2Pressed(ui::EF_COMMAND_DOWN), kMouseDeviceId));

  // Keyboards that press F-Keys do get rewritten to actions.
  EXPECT_EQ(BrowserBackPressed(), RunRewriter(F1Pressed()));
  EXPECT_EQ(BrowserForwardPressed(), RunRewriter(F2Pressed()));
  EXPECT_EQ(F1Pressed(), RunRewriter(F1Pressed(ui::EF_COMMAND_DOWN)));
  EXPECT_EQ(F2Pressed(), RunRewriter(F2Pressed(ui::EF_COMMAND_DOWN)));
}

}  // namespace ash
