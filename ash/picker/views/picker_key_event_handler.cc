// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_key_event_handler.h"

#include "ash/picker/views/picker_key_event_target.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace ash {

PickerKeyEventHandler::PickerKeyEventHandler() = default;

PickerKeyEventHandler::~PickerKeyEventHandler() = default;

bool PickerKeyEventHandler::HandleKeyEvent(const ui::KeyEvent& event) {
  if (active_key_event_target_ == nullptr) {
    return false;
  }

  if (event.key_code() == ui::VKEY_RETURN) {
    return active_key_event_target_->OnEnterKeyPressed();
  }

  return false;
}

void PickerKeyEventHandler::SetActiveKeyEventTarget(
    PickerKeyEventTarget* active_key_event_target) {
  active_key_event_target_ = active_key_event_target;
}

}  // namespace ash
