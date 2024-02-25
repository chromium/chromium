// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_ACCELERATOR_SHIFT_DISABLE_CAPSLOCK_STATE_MACHINE_H_
#define ASH_ACCELERATORS_ACCELERATOR_SHIFT_DISABLE_CAPSLOCK_STATE_MACHINE_H_

#include "ash/ash_export.h"
#include "ui/events/event_handler.h"
#include "ui/ozone/public/input_controller.h"

namespace ash {

// Keeps rtack of when capslock is able to be disabled by the Shift release
// keyboard shortcut.
class ASH_EXPORT AcceleratorShiftDisableCapslockStateMachine
    : public ui::EventHandler {
 public:
  enum class ShiftDisableState {
    // Initial state waiting for shift to be pressed.
    kStart,

    // After shift was pressed, waiting for shift to be released to move to
    // kTrigger.
    kPrimed,

    // Suppressing the accelerator from activating until all keys are released
    // which causes us to go back to kStart.
    kSuppress,

    // When releasing the Shift key will be allowed to trigger the accelerator.
    kTrigger
  };

  explicit AcceleratorShiftDisableCapslockStateMachine(
      ui::InputController* input_controller);
  AcceleratorShiftDisableCapslockStateMachine(
      const AcceleratorShiftDisableCapslockStateMachine&) = delete;
  AcceleratorShiftDisableCapslockStateMachine& operator=(
      const AcceleratorShiftDisableCapslockStateMachine&) = delete;
  ~AcceleratorShiftDisableCapslockStateMachine() override;

  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;

  bool CanHandleCapsLock() const {
    return current_state_ == ShiftDisableState::kTrigger;
  }

  ShiftDisableState current_state() const { return current_state_; }

 private:
  ShiftDisableState current_state_ = ShiftDisableState::kStart;
  raw_ptr<ui::InputController> input_controller_;
};

}  // namespace ash

#endif  // ASH_ACCELERATORS_ACCELERATOR_SHIFT_DISABLE_CAPSLOCK_STATE_MACHINE_H_
