// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_shift_disable_capslock_state_machine.h"

#include "base/containers/fixed_flat_set.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/types/event_type.h"
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"

namespace ash {

constexpr auto kShiftKeys = base::MakeFixedFlatSet<ui::KeyboardCode>(
    {ui::VKEY_SHIFT, ui::VKEY_LSHIFT, ui::VKEY_RSHIFT});

AcceleratorShiftDisableCapslockStateMachine::
    AcceleratorShiftDisableCapslockStateMachine(
        ui::InputController* input_controller)
    : input_controller_(input_controller) {}

AcceleratorShiftDisableCapslockStateMachine::
    ~AcceleratorShiftDisableCapslockStateMachine() = default;

void AcceleratorShiftDisableCapslockStateMachine::OnKeyEvent(
    ui::KeyEvent* event) {
  if (event->type() != ui::EventType::kKeyReleased &&
      event->type() != ui::EventType::kKeyPressed) {
    return;
  }

  if (event->is_repeat()) {
    return;
  }

  switch (current_state_) {
    // Waiting for Shift to be pressed to move to kPrimed. Anything else should
    // move to kSuppress.
    //
    // kTrigger and kStart share the same logic as they are the same state
    // except kTrigger will allow the accelerator to be activated.
    case ShiftDisableState::kStart:
    case ShiftDisableState::kTrigger:
      if (event->type() == ui::EventType::kKeyReleased) {
        current_state_ = ShiftDisableState::kStart;
        return;
      }

      if (kShiftKeys.contains(event->key_code())) {
        current_state_ = ShiftDisableState::kPrimed;
        break;
      }

      current_state_ = ShiftDisableState::kSuppress;
      break;

    // In kPrimed, if anything besides Shift is pressed or released, move to
    // kSuppress.
    // If Shift is released, move to kTrigger.
    case ShiftDisableState::kPrimed:
      if (event->type() == ui::EventType::kKeyPressed) {
        if (kShiftKeys.contains(event->key_code())) {
          break;
        }

        current_state_ = ShiftDisableState::kSuppress;
        break;
      }

      if (kShiftKeys.contains(event->key_code())) {
        current_state_ = ShiftDisableState::kTrigger;
        break;
      }

      current_state_ = ShiftDisableState::kSuppress;
      break;

    // Suppress accelerator from triggering until all keys are released. Then
    // move back to kStart.
    case ShiftDisableState::kSuppress:
      if (!input_controller_->AreAnyKeysPressed()) {
        current_state_ = ShiftDisableState::kStart;
      }
      break;
  }
}

void AcceleratorShiftDisableCapslockStateMachine::OnMouseEvent(
    ui::MouseEvent* event) {
  if (event->type() != ui::EventType::kMousePressed &&
      event->type() != ui::EventType::kMouseReleased) {
    return;
  }

  switch (current_state_) {
    // If the Mouse is pressed during any non-kSuppress state, move to
    // kSuppress.
    case ShiftDisableState::kStart:
    case ShiftDisableState::kTrigger:
    case ShiftDisableState::kPrimed:
      if (event->type() == ui::EventType::kMousePressed) {
        current_state_ = ShiftDisableState::kSuppress;
      }
      break;

    // If the mouse is released during kSuppress and there are no keys pressed,
    // move back to kStart.
    case ShiftDisableState::kSuppress:
      if (event->type() == ui::EventType::kMouseReleased &&
          !input_controller_->AreAnyKeysPressed()) {
        current_state_ = ShiftDisableState::kStart;
      }
      break;
  }
}

}  // namespace ash
