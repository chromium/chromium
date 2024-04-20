// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/suspend_state_machine.h"

#include "ash/accelerators/accelerator_commands.h"
#include "base/containers/fixed_flat_set.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/types/event_type.h"
#include "ui/ozone/public/input_controller.h"

namespace ash {

SuspendStateMachine::SuspendStateMachine(ui::InputController* input_controller)
    : input_controller_(input_controller) {}

SuspendStateMachine::~SuspendStateMachine() = default;

void SuspendStateMachine::StartObservingToTriggerSuspend(
    const ui::Accelerator& accelerator) {
  trigger_accelerator_ = accelerator;
}

void SuspendStateMachine::OnKeyEvent(ui::KeyEvent* event) {
  if (!trigger_accelerator_) {
    return;
  }

  // Ignore all repeats.
  if (event->is_repeat()) {
    return;
  }

  // If any key is pressed, the suspend trigger should be cancelled.
  if (event->type() == ui::ET_KEY_PRESSED) {
    trigger_accelerator_.reset();
    return;
  }
  DCHECK_EQ(ui::ET_KEY_RELEASED, event->type());

  // Either the key code of the accelerator must match OR the release key must
  // have modifiers that match in the accelerator.
  const bool key_codes_match =
      trigger_accelerator_->key_code() == event->key_code();
  const bool modifier_flags_match =
      (ui::ModifierDomKeyToEventFlag(event->GetDomKey()) &
       trigger_accelerator_->modifiers()) != 0;
  if (!key_codes_match && !modifier_flags_match) {
    trigger_accelerator_.reset();
    return;
  }

  // Only trigger suspend if no keys are currently being held down.
  if (input_controller_->AreAnyKeysPressed()) {
    return;
  }

  trigger_accelerator_.reset();
  accelerators::Suspend();
}

}  // namespace ash
