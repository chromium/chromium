// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_ACCELERATOR_LAUNCHER_STATE_MACHINE_H_
#define ASH_ACCELERATORS_ACCELERATOR_LAUNCHER_STATE_MACHINE_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/events/event_handler.h"
#include "ui/ozone/public/input_controller.h"

namespace ash {

// Keeps track when the launcher is able to be opened via Search release or
// Shift + Search release shortcuts.
class ASH_EXPORT AcceleratorLauncherStateMachine : public ui::EventHandler {
 public:
  enum class LauncherState {
    // Initial state, when we are waiting for our first bit of input for the
    // state machine.
    kStart,

    // When the launcher can be triggered. In any other state, the launcher will
    // be suppressed.
    kTrigger,

    // When we are waiting solely for the release of Meta so we can move to
    // kTrigger.
    kPrimed,

    // When the launcher accelerator is being suppressed until there is a state
    // where all keys are released.
    kSuppress,
  };

  explicit AcceleratorLauncherStateMachine(
      ui::InputController* input_controller);
  AcceleratorLauncherStateMachine(const AcceleratorLauncherStateMachine&) =
      delete;
  AcceleratorLauncherStateMachine& operator=(
      const AcceleratorLauncherStateMachine&) = delete;
  ~AcceleratorLauncherStateMachine() override;

  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;

  bool CanHandleLauncher() const {
    return current_state_ == LauncherState::kTrigger;
  }

  LauncherState current_state() const { return current_state_; }

  void SetCanHandleLauncherForTesting(bool can_handle);

 private:
  LauncherState current_state_ = LauncherState::kStart;
  raw_ptr<ui::InputController> input_controller_;
};

}  // namespace ash

#endif  // ASH_ACCELERATORS_ACCELERATOR_LAUNCHER_STATE_MACHINE_H_
