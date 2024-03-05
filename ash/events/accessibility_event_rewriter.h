// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_EVENTS_ACCESSIBILITY_EVENT_REWRITER_H_
#define ASH_EVENTS_ACCESSIBILITY_EVENT_REWRITER_H_

#include <map>
#include <memory>
#include <set>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/event_rewriter.h"

namespace ui {
class EventRewriterAsh;
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
      public input_method::InputMethodManager::Observer {
 public:
  AccessibilityEventRewriter(ui::EventRewriterAsh* event_rewriter_ash,
                             AccessibilityEventRewriterDelegate* delegate);
  AccessibilityEventRewriter(const AccessibilityEventRewriter&) = delete;
  AccessibilityEventRewriter& operator=(const AccessibilityEventRewriter&) =
      delete;
  ~AccessibilityEventRewriter() override;

  // Continue dispatch of events that were unhandled by the ChromeVox extension.
  // NOTE: These events may be delivered out-of-order from non-ChromeVox events.
  void OnUnhandledSpokenFeedbackEvent(std::unique_ptr<ui::Event> event) const;

  // Sets what |key_codes| are captured for a given Switch Access command.
  void SetKeyCodesForSwitchAccessCommand(
      const std::map<int, std::set<std::string>>& key_codes,
      SwitchAccessCommand command);

  void set_chromevox_capture_all_keys(bool value) {
    chromevox_capture_all_keys_ = value;
  }

  void set_send_mouse_events(bool value) { send_mouse_events_ = value; }

  void set_suspend_switch_access_key_handling(bool suspend) {
    suspend_switch_access_key_handling_ = suspend;
  }

  // For testing use only.
  std::map<int, std::set<ui::InputDeviceType>>
  switch_access_key_codes_to_capture_for_test() {
    return switch_access_key_codes_to_capture_;
  }
  std::map<int, SwitchAccessCommand>
  key_code_to_switch_access_command_map_for_test() {
    return key_code_to_switch_access_command_;
  }

 private:
  friend class ChromeVoxAccessibilityEventRewriterTest;
  friend class MouseKeysAccessibilityEventRewriterTest;

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

  // Maybe sends a mouse event to be dispatched to accessibility component
  // extensions.
  void MaybeSendMouseEvent(const ui::Event& event);

  // ui::EventRewriter:
  ui::EventDispatchDetails RewriteEvent(
      const ui::Event& event,
      const Continuation continuation) override;

  // input_method::InputMethodManager::Observer:
  void InputMethodChanged(input_method::InputMethodManager* manager,
                          Profile* profile,
                          bool show_message) override;

  // Continuation saved for OnUnhandledSpokenFeedbackEvent().
  Continuation chromevox_continuation_;

  // The delegate used to send events to the ChromeVox and Switch Access
  // extensions.
  raw_ptr<AccessibilityEventRewriterDelegate> delegate_ = nullptr;

  // Whether to send mouse events to accessibility component extensions.
  bool send_mouse_events_ = false;

  // Whether to capture all keys for ChromeVox.
  bool chromevox_capture_all_keys_ = false;

  // Maps a key to a set of devices which should be captured for Switch Access.
  std::map<int, std::set<ui::InputDeviceType>>
      switch_access_key_codes_to_capture_;

  // Maps a captured key from above to a Switch Access command.
  std::map<int, SwitchAccessCommand> key_code_to_switch_access_command_;

  // Used to rewrite events in special cases such as function keys for ChromeVox
  // taylored behavior.
  const raw_ptr<ui::EventRewriterAsh, DanglingUntriaged> event_rewriter_ash_;

  // Suspends key handling for Switch Access during key assignment in web ui.
  bool suspend_switch_access_key_handling_ = false;

  // Whether to try and rewrite positional keys for ChromeVox.
  bool try_rewriting_positional_keys_for_chromevox_ = true;

  // Used to monitor input method changes.
  base::ScopedObservation<input_method::InputMethodManager,
                          input_method::InputMethodManager::Observer>
      observation_{this};
};

}  // namespace ash

#endif  // ASH_EVENTS_ACCESSIBILITY_EVENT_REWRITER_H_
