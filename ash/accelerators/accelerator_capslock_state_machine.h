// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_ACCELERATOR_CAPSLOCK_STATE_MACHINE_H_
#define ASH_ACCELERATORS_ACCELERATOR_CAPSLOCK_STATE_MACHINE_H_

#include "ash/ash_export.h"
#include "ui/events/event_handler.h"
#include "ui/ozone/public/input_controller.h"

namespace ash {

// Keeps track of when capslock is able to be toggled via Search + Alt keyboard
// shortcuts.
class ASH_EXPORT AcceleratorCapslockStateMachine : public ui::EventHandler {
 public:
  enum class CapslockState {
    // Initial state waiting for either Alt or Meta to be pressed.
    kStart,

    // Waiting for Alt to be pressed so we can advance to kPrimed.
    kWaitingAlt,

    // Waiting for Search to be pressed so we can advance to kPrimed.
    kWaitingSearch,

    // When Alt is released causing the accelerator to be able to be fired. Here
    // we are waiting for Alt to be pressed again so we can move to kPrimed.
    kTriggerAlt,

    // When Search is released causing the accelerator to be able to be fired.
    // Here we are waiting for Search to be pressed again so we can move to
    // kPrimed.
    kTriggerSearch,

    // When we should wait for all keys to be released so we move back to
    // kStart.
    kSuppress,

    // When both Search and Alt are being held down and we are waiting for one
    // of them to be released.
    kPrimed
  };

  explicit AcceleratorCapslockStateMachine(
      ui::InputController* input_controller);
  AcceleratorCapslockStateMachine(const AcceleratorCapslockStateMachine&) =
      delete;
  AcceleratorCapslockStateMachine& operator=(
      const AcceleratorCapslockStateMachine&) = delete;
  ~AcceleratorCapslockStateMachine() override;

  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;

  bool CanHandleCapsLock() const {
    return current_state_ == CapslockState::kTriggerAlt ||
           current_state_ == CapslockState::kTriggerSearch;
  }
  CapslockState current_state() const { return current_state_; }

  void SetCanHandleCapsLockForTesting(bool can_handle);

 private:
  CapslockState current_state_ = CapslockState::kStart;
  raw_ptr<ui::InputController> input_controller_;
};

}  // namespace ash

#endif  // ASH_ACCELERATORS_ACCELERATOR_CAPSLOCK_STATE_MACHINE_H_
