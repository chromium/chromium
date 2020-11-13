// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_EVENTS_ACCESSIBILITY_EVENT_REWRITER_H_
#define ASH_EVENTS_ACCESSIBILITY_EVENT_REWRITER_H_

#include <map>
#include <memory>
#include <set>

#include "ash/ash_export.h"
#include "base/scoped_observer.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/input_device_event_observer.h"
#include "ui/events/event_rewriter.h"

namespace ui {
class EventRewriterChromeOS;
enum InputDeviceType;
}

namespace ash {

class AccessibilityEventRewriterDelegate;
enum class SwitchAccessCommand;
enum class MagnifierCommand;

// AccessibilityEventRewriter sends key events to Accessibility extensions (such
// as ChromeVox and Switch Access) via the delegate when the corresponding
// extension is enabled. Continues dispatch of unhandled key events.
class ASH_EXPORT AccessibilityEventRewriter
    : public ui::EventRewriter,
      public ui::InputDeviceEventObserver {
 public:
  AccessibilityEventRewriter(ui::EventRewriterChromeOS* event_rewriter_chromeos,
                             AccessibilityEventRewriterDelegate* delegate);
  AccessibilityEventRewriter(const AccessibilityEventRewriter&) = delete;
  AccessibilityEventRewriter& operator=(const AccessibilityEventRewriter&) =
      delete;
  ~AccessibilityEventRewriter() override;

  // Continue dispatch of events that were unhandled by the ChromeVox extension.
  // NOTE: These events may be delivered out-of-order from non-ChromeVox events.
  void OnUnhandledSpokenFeedbackEvent(std::unique_ptr<ui::Event> event) const;

  // Sets what |key_codes| are captured for a given Switch Access command;
  // returns true if any mapping changed.
  bool SetKeyCodesForSwitchAccessCommand(std::set<int> key_codes,
                                         SwitchAccessCommand command);

  // Set the types of keyboard input types processed by this rewriter.
  void SetKeyboardInputDeviceTypes(
      const std::set<ui::InputDeviceType>& keyboard_input_device_types);

  void set_chromevox_capture_all_keys(bool value) {
    chromevox_capture_all_keys_ = value;
  }
  void set_chromevox_send_mouse_events(bool value) {
    chromevox_send_mouse_events_ = value;
  }

  // For testing use only.
  std::set<int> switch_access_key_codes_to_capture_for_test() {
    return switch_access_key_codes_to_capture_;
  }
  std::map<int, SwitchAccessCommand>
  key_code_to_switch_access_command_map_for_test() {
    return key_code_to_switch_access_command_;
  }

 private:
  // Internal helpers to rewrite an event for a given accessibility feature.
  // Returns true if the event is captured.
  bool RewriteEventForChromeVox(const ui::Event& event,
                                const Continuation continuation);
  bool RewriteEventForSwitchAccess(const ui::Event& event,
                                   const Continuation continuation);
  bool RewriteEventForMagnifier(const ui::Event& event,
                                const Continuation continuation);
  void OnMagnifierKeyPressed(const ui::KeyEvent* event);
  void OnMagnifierKeyReleased(const ui::KeyEvent* event);

  // Updates the list of allowed keyboard device ids based on the current set of
  // keyboard input types.
  void UpdateKeyboardDeviceIds();

  // ui::EventRewriter:
  ui::EventDispatchDetails RewriteEvent(
      const ui::Event& event,
      const Continuation continuation) override;

  // ui::InputDeviceObserver:
  void OnInputDeviceConfigurationChanged(uint8_t input_device_types) override;

  // Continuation saved for OnUnhandledSpokenFeedbackEvent().
  Continuation chromevox_continuation_;

  // The delegate used to send events to the ChromeVox and Switch Access
  // extensions.
  AccessibilityEventRewriterDelegate* delegate_ = nullptr;

  // Whether to send mouse events to the ChromeVox extension.
  bool chromevox_send_mouse_events_ = false;

  // Whether to capture all keys for ChromeVox.
  bool chromevox_capture_all_keys_ = false;

  // Set of keys to capture for Switch Access.
  std::set<int> switch_access_key_codes_to_capture_;

  // Maps a captured key from above to a Switch Access command.
  std::map<int, SwitchAccessCommand> key_code_to_switch_access_command_;

  // Used to rewrite events in special cases such as function keys for ChromeVox
  // taylored behavior.
  ui::EventRewriterChromeOS* const event_rewriter_chromeos_;

  // A set of keyboard device ids who's key events we want to process.
  std::set<int> keyboard_device_ids_;

  // A set of input device types used to filter the list of keyboard devices
  // above.
  std::set<ui::InputDeviceType> keyboard_input_device_types_;

  // Used to refresh state when keyboard devices change.
  ScopedObserver<ui::DeviceDataManager, ui::InputDeviceEventObserver> observer_{
      this};
};

}  // namespace ash

#endif  // ASH_EVENTS_ACCESSIBILITY_EVENT_REWRITER_H_
