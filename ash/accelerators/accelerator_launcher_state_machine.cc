// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_launcher_state_machine.h"

#include "base/containers/fixed_flat_set.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/types/event_type.h"
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"

namespace ash {

constexpr auto kMetaKeys =
    base::MakeFixedFlatSet<ui::KeyboardCode>({ui::VKEY_LWIN, ui::VKEY_RWIN});

constexpr auto kShiftKeys = base::MakeFixedFlatSet<ui::KeyboardCode>(
    {ui::VKEY_SHIFT, ui::VKEY_LSHIFT, ui::VKEY_RSHIFT});

AcceleratorLauncherStateMachine::AcceleratorLauncherStateMachine(
    ui::InputController* input_controller)
    : input_controller_(input_controller) {}

AcceleratorLauncherStateMachine::~AcceleratorLauncherStateMachine() = default;

void AcceleratorLauncherStateMachine::OnKeyEvent(ui::KeyEvent* event) {
  if (event->type() != ui::EventType::kKeyReleased &&
      event->type() != ui::EventType::kKeyPressed) {
    return;
  }

  switch (current_state_) {
    // When in kStart, if anything but Meta or Shift is pressed, we move to
    // kSuppress.
    // If Shift is pressed, its a no-op.
    // If Meta is pressed, we move to kPrimed.
    //
    // kTrigger and kStart share the same logic as they are the same state
    // except kTrigger will allow the launcher to be opened while kStart will
    // not.
    case LauncherState::kTrigger:
    case LauncherState::kStart:
      if (event->type() != ui::EventType::kKeyPressed) {
        current_state_ = LauncherState::kStart;
        break;
      }

      if (kShiftKeys.contains(event->key_code())) {
        current_state_ = LauncherState::kStart;
        break;
      }

      if (!kMetaKeys.contains(event->key_code())) {
        current_state_ = LauncherState::kSuppress;
        break;
      }

      current_state_ = LauncherState::kPrimed;
      break;

    // In kPrimed, if anything besides Meta is pressed or released, we move to
    // kSuppress.
    // If Meta is released, we move to kTrigger.
    case LauncherState::kPrimed:
      if (!kMetaKeys.contains(event->key_code())) {
        current_state_ = LauncherState::kSuppress;
        break;
      }

      if (event->type() == ui::EventType::kKeyPressed) {
        break;
      }

      current_state_ = LauncherState::kTrigger;
      break;

    // While in kSuppress, if there is ever a point where no keys are being
    // pressed, we move to kStart.
    case LauncherState::kSuppress:
      if (!input_controller_->AreAnyKeysPressed()) {
        current_state_ = LauncherState::kStart;
      }
      break;
  }
}

void AcceleratorLauncherStateMachine::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() != ui::EventType::kMousePressed &&
      event->type() != ui::EventType::kMouseReleased) {
    return;
  }

  switch (current_state_) {
    // If the Mouse is pressed during any non-kSuppress state, move to
    // kSuppress.
    case LauncherState::kStart:
    case LauncherState::kTrigger:
    case LauncherState::kPrimed:
      if (event->type() == ui::EventType::kMousePressed) {
        current_state_ = LauncherState::kSuppress;
      }
      break;

    // If the mouse is released during kSuppress and there are no keys pressed,
    // move back to kStart.
    case LauncherState::kSuppress:
      if (event->type() == ui::EventType::kMouseReleased &&
          !input_controller_->AreAnyKeysPressed()) {
        current_state_ = LauncherState::kStart;
      }
      break;
  }
}

void AcceleratorLauncherStateMachine::SetCanHandleLauncherForTesting(
    bool can_handle) {
  current_state_ = can_handle ? LauncherState::kTrigger : LauncherState::kStart;
}

}  // namespace ash
