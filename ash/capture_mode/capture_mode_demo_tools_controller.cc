// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_demo_tools_controller.h"

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/types/event_type.h"

namespace ash {

namespace {

int GetModifierFlagForKeyCode(ui::KeyboardCode key_code) {
  switch (key_code) {
    case ui::VKEY_COMMAND:
    case ui::VKEY_RWIN:
      return ui::EF_COMMAND_DOWN;
    case ui::VKEY_CONTROL:
    case ui::VKEY_LCONTROL:
    case ui::VKEY_RCONTROL:
      return ui::EF_CONTROL_DOWN;
    case ui::VKEY_MENU:
    case ui::VKEY_LMENU:
    case ui::VKEY_RMENU:
      return ui::EF_ALT_DOWN;
    case ui::VKEY_SHIFT:
    case ui::VKEY_LSHIFT:
    case ui::VKEY_RSHIFT:
      return ui::EF_SHIFT_DOWN;
    default:
      return ui::EF_NONE;
  }
}

// Includes non-modifier keys that can be shown independently without a modifier
// key being pressed.
constexpr ui::KeyboardCode kNotNeedingModifierKeys[] = {
    ui::VKEY_COMMAND, ui::VKEY_RWIN, ui::VKEY_MEDIA_LAUNCH_APP1,
    ui::VKEY_ESCAPE, ui::VKEY_TAB};

// Returns true if `key_code` is a non-modifier key for which a `KeyComboViewer`
// can be shown even if there are no modifier keys are currently pressed.
bool ShouldConsiderKey(ui::KeyboardCode key_code) {
  return base::Contains(kNotNeedingModifierKeys, key_code);
}

}  // namespace

CaptureModeDemoToolsController::CaptureModeDemoToolsController() = default;

CaptureModeDemoToolsController::~CaptureModeDemoToolsController() = default;

void CaptureModeDemoToolsController::OnKeyEvent(ui::KeyEvent* event) {
  if (event->type() == ui::ET_KEY_RELEASED) {
    OnKeyUpEvent(event);
    return;
  }

  DCHECK_EQ(event->type(), ui::ET_KEY_PRESSED);
  OnKeyDownEvent(event);
}

void CaptureModeDemoToolsController::OnKeyUpEvent(ui::KeyEvent* event) {
  const ui::KeyboardCode key_code = event->key_code();
  modifiers_ &= ~GetModifierFlagForKeyCode(key_code);

  if (last_non_modifier_key_ == key_code)
    last_non_modifier_key_ = ui::VKEY_UNKNOWN;

  RefreshKeyComboViewer();
}

void CaptureModeDemoToolsController::OnKeyDownEvent(ui::KeyEvent* event) {
  const ui::KeyboardCode key_code = event->key_code();
  const int modifier_flag = GetModifierFlagForKeyCode(key_code);
  modifiers_ |= modifier_flag;

  if (modifier_flag == ui::EF_NONE)
    last_non_modifier_key_ = key_code;

  RefreshKeyComboViewer();
}

void CaptureModeDemoToolsController::RefreshKeyComboViewer() {
  demo_tools_widget_ =
      (modifiers_ != 0) || ShouldConsiderKey(last_non_modifier_key_);
}

}  // namespace ash