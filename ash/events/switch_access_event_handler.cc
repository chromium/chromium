// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/events/switch_access_event_handler.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/public/cpp/switch_access_event_handler_delegate.h"
#include "ash/shell.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"

namespace ash {

namespace {

bool IsSwitchAccessEnabled() {
  return Shell::Get()->accessibility_controller()->switch_access_enabled();
}

void CancelEvent(ui::Event* event) {
  DCHECK(event);
  if (event->cancelable()) {
    event->SetHandled();
    event->StopPropagation();
  }
}

}  // namespace

SwitchAccessEventHandler::SwitchAccessEventHandler(
    SwitchAccessEventHandlerDelegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
  Shell::Get()->AddPreTargetHandler(this,
                                    ui::EventTarget::Priority::kAccessibility);
}

SwitchAccessEventHandler::~SwitchAccessEventHandler() {
  Shell::Get()->RemovePreTargetHandler(this);
}

bool SwitchAccessEventHandler::SetKeyCodesForCommand(
    std::set<int> new_key_codes,
    SwitchAccessCommand command) {
  bool has_changed = false;
  std::set<int> to_clear;

  // Clear old values that conflict with the new assignment.
  for (const auto& val : command_for_key_code_) {
    int old_key_code = val.first;
    SwitchAccessCommand old_command = val.second;

    if (new_key_codes.count(old_key_code) > 0) {
      if (old_command != command) {
        has_changed = true;
        // Modifying the map while iterating through it causes reference
        // failures.
        to_clear.insert(old_key_code);
      } else {
        new_key_codes.erase(old_key_code);
      }
      continue;
    }

    // This value was previously mapped to the command, but is no longer.
    if (old_command == command) {
      has_changed = true;
      to_clear.insert(old_key_code);
      key_codes_to_capture_.erase(old_key_code);
    }
  }
  for (int key_code : to_clear) {
    command_for_key_code_.erase(key_code);
  }

  if (new_key_codes.size() == 0)
    return has_changed;

  // Add any new key codes to the map.
  for (int key_code : new_key_codes) {
    key_codes_to_capture_.insert(key_code);
    command_for_key_code_[key_code] = command;
  }

  return true;
}

void SwitchAccessEventHandler::OnKeyEvent(ui::KeyEvent* event) {
  DCHECK(IsSwitchAccessEnabled());
  DCHECK(event);

  if (ShouldCancelEvent(*event)) {
    CancelEvent(event);
    if (ShouldForwardEvent(*event)) {
      SwitchAccessCommand command = command_for_key_code_[event->key_code()];
      delegate_->SendSwitchAccessCommand(command);
    }
  }
}

bool SwitchAccessEventHandler::ShouldCancelEvent(
    const ui::KeyEvent& event) const {
  // Ignore virtual key events so users can type with the onscreen keyboard.
  if (ignore_virtual_key_events_ &&
      event.source_device_id() == ui::ED_UNKNOWN_DEVICE) {
    // When running Chrome OS on Linux, the source_device_id property is never
    // populated.
    auto* properties = event.properties();
    bool is_linux_xevent =
        properties &&
        properties->find(ui::kPropertyKeyboardIBusFlag) != properties->end();
    if (!is_linux_xevent)
      return false;
  }

  if (forward_key_events_)
    return true;

  return key_codes_to_capture_.count(event.key_code()) > 0;
}

// Returns whether to forward an event, assuming that ShouldCancelEvent(event)
// returns true.
bool SwitchAccessEventHandler::ShouldForwardEvent(
    const ui::KeyEvent& event) const {
  return event.type() == ui::ET_KEY_PRESSED;
}

}  // namespace ash
