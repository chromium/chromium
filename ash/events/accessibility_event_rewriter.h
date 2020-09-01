// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_EVENTS_ACCESSIBILITY_EVENT_REWRITER_H_
#define ASH_EVENTS_ACCESSIBILITY_EVENT_REWRITER_H_

#include <map>
#include <memory>
#include <set>

#include "ash/ash_export.h"
#include "ui/events/event_rewriter.h"

namespace ui {
class EventRewriterChromeOS;
}

namespace ash {

class AccessibilityEventRewriterDelegate;
enum class SwitchAccessCommand;

// AccessibilityEventRewriter sends key events to Accessibility extensions (such
// as ChromeVox and Switch Access) via the delegate when the corresponding
// extension is enabled. Continues dispatch of unhandled key events.
class ASH_EXPORT AccessibilityEventRewriter : public ui::EventRewriter {
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

  // Sets what key_codes are captured for a given Switch Access command.
  bool SetKeyCodesForSwitchAccessCommand(std::set<int> key_codes,
                                         SwitchAccessCommand command);

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
  bool RewriteEventForChromeVox(const ui::Event& event,
                                const Continuation continuation);
  bool RewriteEventForSwitchAccess(const ui::Event& event,
                                   const Continuation continuation);

  // ui::EventRewriter:
  ui::EventDispatchDetails RewriteEvent(
      const ui::Event& event,
      const Continuation continuation) override;

  // Continuation saved for OnUnhandledSpokenFeedbackEvent().
  Continuation chromevox_continuation_;

  // The delegate used to send events to the ChromeVox and Switch Access
  // extensions.
  AccessibilityEventRewriterDelegate* delegate_ = nullptr;

  // Whether to send mouse events to the ChromeVox extension.
  bool chromevox_send_mouse_events_ = false;

  // Whether to capture all keys.
  bool chromevox_capture_all_keys_ = false;

  std::set<int> switch_access_key_codes_to_capture_;
  std::map<int, SwitchAccessCommand> key_code_to_switch_access_command_;

  ui::EventRewriterChromeOS* const event_rewriter_chromeos_;
};

}  // namespace ash

#endif  // ASH_EVENTS_ACCESSIBILITY_EVENT_REWRITER_H_
