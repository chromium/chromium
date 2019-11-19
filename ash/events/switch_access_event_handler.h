// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_EVENTS_SWITCH_ACCESS_EVENT_HANDLER_H_
#define ASH_EVENTS_SWITCH_ACCESS_EVENT_HANDLER_H_

#include <map>
#include <set>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/events/event_handler.h"

namespace ash {

enum class SwitchAccessCommand;
class SwitchAccessEventHandlerDelegate;

// SwitchAccessEventHandler sends events to the Switch Access extension
// (via the delegate) when it is enabled.
class ASH_EXPORT SwitchAccessEventHandler : public ui::EventHandler {
 public:
  explicit SwitchAccessEventHandler(SwitchAccessEventHandlerDelegate* delegate);
  ~SwitchAccessEventHandler() override;

  // Sets what key_codes are captured for a given command.
  bool SetKeyCodesForCommand(std::set<int> key_codes,
                             SwitchAccessCommand command);

  // Sets whether virtual key events should be ignored.
  void set_ignore_virtual_key_events(bool should_ignore) {
    ignore_virtual_key_events_ = should_ignore;
  }

  // Tells the handler whether to forward all incoming key events to the Switch
  // Access extension.
  void set_forward_key_events(bool should_forward) {
    forward_key_events_ = should_forward;
  }

  // For testing usage only.
  const std::set<int> key_codes_to_capture_for_test() {
    return key_codes_to_capture_;
  }
  const std::map<int, SwitchAccessCommand> command_for_key_code_map_for_test() {
    return command_for_key_code_;
  }

 private:
  bool ShouldCancelEvent(const ui::KeyEvent& event) const;
  bool ShouldForwardEvent(const ui::KeyEvent& event) const;

  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;

  // The delegate used to send key events to the Switch Access extension.
  SwitchAccessEventHandlerDelegate* delegate_;

  std::set<int> key_codes_to_capture_;
  std::map<int, SwitchAccessCommand> command_for_key_code_;
  bool forward_key_events_ = false;
  bool ignore_virtual_key_events_ = true;

  DISALLOW_COPY_AND_ASSIGN(SwitchAccessEventHandler);
};

}  // namespace ash

#endif  // ASH_EVENTS_SWITCH_ACCESS_EVENT_HANDLER_H_
