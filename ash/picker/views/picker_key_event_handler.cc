// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_key_event_handler.h"

#include "ash/picker/views/picker_pseudo_focus_handler.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/focus/focus_manager.h"

namespace ash {

PickerKeyEventHandler::PickerKeyEventHandler() = default;

PickerKeyEventHandler::~PickerKeyEventHandler() = default;

bool PickerKeyEventHandler::HandleKeyEvent(const ui::KeyEvent& event) {
  if (active_pseudo_focus_handler_ == nullptr || event.handled() ||
      event.type() != ui::ET_KEY_PRESSED) {
    return false;
  }

  if (views::FocusManager::IsTabTraversalKeyEvent(event)) {
    active_pseudo_focus_handler_->AdvancePseudoFocus(
        event.IsShiftDown()
            ? PickerPseudoFocusHandler::PseudoFocusDirection::kBackward
            : PickerPseudoFocusHandler::PseudoFocusDirection::kForward);
    return true;
  }

  const bool has_modifier =
      event.IsShiftDown() || event.IsControlDown() || event.IsAltDown();
  switch (event.key_code()) {
    case ui::VKEY_RETURN:
      return active_pseudo_focus_handler_->DoPseudoFocusedAction();
    case ui::VKEY_UP:
      return has_modifier ? false
                          : active_pseudo_focus_handler_->MovePseudoFocusUp();
    case ui::VKEY_DOWN:
      return has_modifier ? false
                          : active_pseudo_focus_handler_->MovePseudoFocusDown();
    case ui::VKEY_LEFT:
      return has_modifier ? false
                          : active_pseudo_focus_handler_->MovePseudoFocusLeft();
    case ui::VKEY_RIGHT:
      return has_modifier
                 ? false
                 : active_pseudo_focus_handler_->MovePseudoFocusRight();
    default:
      return false;
  }
}

void PickerKeyEventHandler::SetActivePseudoFocusHandler(
    PickerPseudoFocusHandler* active_pseudo_focus_handler) {
  active_pseudo_focus_handler_ = active_pseudo_focus_handler;
}

}  // namespace ash
