// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_SUSPEND_STATE_MACHINE_H_
#define ASH_ACCELERATORS_SUSPEND_STATE_MACHINE_H_

#include "ash/ash_export.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event_handler.h"

namespace ui {
class InputController;
}

namespace ash {

// Tracks when all keys are released for a suspend accelerator and then triggers
// a suspend DBUS call to power_manager. This prevents the key releases from the
// accelerator from waking up the system instantly.
class ASH_EXPORT SuspendStateMachine : public ui::EventHandler {
 public:
  // This enum is used for metrics, do not reorder entries.
  enum class SuspendStateMachineEvent {
    kTriggered,
    kCancelled,
    kSuspended,
    kMaxValue = kSuspended
  };

  explicit SuspendStateMachine(ui::InputController* input_controller);
  SuspendStateMachine(const SuspendStateMachine&) = delete;
  SuspendStateMachine& operator=(const SuspendStateMachine&) = delete;
  ~SuspendStateMachine() override;

  // Initializes the state machine to start observing for when all keys are
  // released to trigger DBUS call to start suspend.
  void StartObservingToTriggerSuspend(const ui::Accelerator& accelerator);

  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;

 private:
  void CancelSuspend();

  std::optional<ui::Accelerator> trigger_accelerator_;
  raw_ptr<ui::InputController> input_controller_;
};

}  // namespace ash

#endif  // ASH_ACCELERATORS_SUSPEND_STATE_MACHINE_H_
