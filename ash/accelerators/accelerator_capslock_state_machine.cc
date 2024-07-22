// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_capslock_state_machine.h"

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

constexpr auto kAltKeys =
    base::MakeFixedFlatSet<ui::KeyboardCode>({ui::VKEY_MENU});

AcceleratorCapslockStateMachine::AcceleratorCapslockStateMachine(
    ui::InputController* input_controller)
    : input_controller_(input_controller) {}

AcceleratorCapslockStateMachine::~AcceleratorCapslockStateMachine() = default;

void AcceleratorCapslockStateMachine::OnKeyEvent(ui::KeyEvent* event) {
  if (event->type() != ui::EventType::kKeyReleased &&
      event->type() != ui::EventType::kKeyPressed) {
    return;
  }

  switch (current_state_) {
    // Waiting for either Alt or Search. Anything else we should move to
    // kSuppress.
    case CapslockState::kStart:
      if (event->type() == ui::EventType::kKeyReleased) {
        break;
      }

      if (kMetaKeys.contains(event->key_code())) {
        current_state_ = CapslockState::kWaitingAlt;
        break;
      }

      if (kAltKeys.contains(event->key_code())) {
        current_state_ = CapslockState::kWaitingSearch;
        break;
      }

      current_state_ = CapslockState::kSuppress;
      break;

    // Waiting for Alt to be pressed so we can move to kPrimed.
    // Anything besides Alt being pressed means we move to kSuppress.
    // kTriggerAlt and kWaitingAlt share the same logic except the accelerator
    // cannot be triggered in kWaitingAlt.
    case CapslockState::kTriggerAlt:
    case CapslockState::kWaitingAlt: {
      const bool is_meta_key = kMetaKeys.contains(event->key_code());
      const bool is_alt_key = kAltKeys.contains(event->key_code());

      if (event->type() == ui::EventType::kKeyReleased) {
        // If alt key is released, we go back to kStart.
        if (is_meta_key) {
          current_state_ = CapslockState::kStart;
          break;
        }

        current_state_ = CapslockState::kSuppress;
        break;
      }

      if (is_alt_key) {
        current_state_ = CapslockState::kPrimed;
        break;
      }

      if (is_meta_key) {
        current_state_ = CapslockState::kWaitingAlt;
        break;
      }

      current_state_ = CapslockState::kSuppress;
      break;
    }

    // Waiting for Search to be pressed so we can move to kPrimed.
    // Anything besides Search being pressed means we move to kSuppress.
    // kTriggerSearch and kWaitingSearch share the same logic except the
    // accelerator cannot be triggered in kWaitingSearch.
    case CapslockState::kTriggerSearch:
    case CapslockState::kWaitingSearch: {
      const bool is_meta_key = kMetaKeys.contains(event->key_code());
      const bool is_alt_key = kAltKeys.contains(event->key_code());

      if (event->type() == ui::EventType::kKeyReleased) {
        // If alt key is released, we go back to kStart.
        if (is_alt_key) {
          current_state_ = CapslockState::kStart;
          break;
        }

        current_state_ = CapslockState::kSuppress;
        break;
      }

      if (is_meta_key) {
        current_state_ = CapslockState::kPrimed;
        break;
      }

      if (is_alt_key) {
        current_state_ = CapslockState::kWaitingSearch;
        break;
      }

      current_state_ = CapslockState::kSuppress;
      break;
    }

    // Waiting for all keys to be released to move back to kStart.
    case CapslockState::kSuppress:
      if (!ui::OzonePlatform::GetInstance()
               ->GetInputController()
               ->AreAnyKeysPressed()) {
        current_state_ = CapslockState::kStart;
      }
      break;

    // Waiting for either Search or Alt to be released.
    // If Search is released, move to kTriggerSearch.
    // If Alt is released, move to kTriggerAlt.
    // Anything else, move to kSuppress.
    case CapslockState::kPrimed:
      const bool is_meta = kMetaKeys.contains(event->key_code());
      const bool is_alt = kAltKeys.contains(event->key_code());

      if (!is_alt && !is_meta) {
        current_state_ = CapslockState::kSuppress;
        break;
      }

      if (event->type() == ui::EventType::kKeyPressed) {
        break;
      }

      if (is_meta) {
        current_state_ = CapslockState::kTriggerSearch;
        break;
      }

      current_state_ = CapslockState::kTriggerAlt;
      break;
  }
}

void AcceleratorCapslockStateMachine::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() != ui::EventType::kMousePressed &&
      event->type() != ui::EventType::kMouseReleased) {
    return;
  }

  switch (current_state_) {
    // On mouse press, move to kSuppress for any state except kSuppress.
    case CapslockState::kStart:
    case CapslockState::kWaitingAlt:
    case CapslockState::kWaitingSearch:
    case CapslockState::kTriggerAlt:
    case CapslockState::kTriggerSearch:
    case CapslockState::kPrimed:
      if (event->type() == ui::EventType::kMousePressed) {
        current_state_ = CapslockState::kSuppress;
      }
      break;

    // When in kSuppress, move to kStart if the mouse button is released and
    // there are no keys currently being pressed.
    case CapslockState::kSuppress:
      if (event->type() == ui::EventType::kMouseReleased &&
          !ui::OzonePlatform::GetInstance()
               ->GetInputController()
               ->AreAnyKeysPressed()) {
        current_state_ = CapslockState::kStart;
      }
      break;
  }
}

void AcceleratorCapslockStateMachine::SetCanHandleCapsLockForTesting(
    bool can_handle) {
  current_state_ =
      can_handle ? CapslockState::kTriggerAlt : CapslockState::kStart;
}

}  // namespace ash
