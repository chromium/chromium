// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/welcome_tour/welcome_tour_accelerator_handler.h"

#include "ash/accelerators/ash_accelerator_configuration.h"
#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "base/ranges/algorithm.h"
#include "base/task/sequenced_task_runner.h"
#include "ui/events/event.h"
#include "ui/events/event_target.h"

namespace ash {

// static
constexpr std::array<WelcomeTourAcceleratorHandler::AllowedAction, 13>
    WelcomeTourAcceleratorHandler::kAllowedActions;

WelcomeTourAcceleratorHandler::WelcomeTourAcceleratorHandler(
    base::RepeatingClosure abort_tour_callback)
    : abort_tour_callback_(abort_tour_callback) {
  Shell::Get()->AddPreTargetHandler(this, ui::EventTarget::Priority::kSystem);
}

WelcomeTourAcceleratorHandler::~WelcomeTourAcceleratorHandler() {
  Shell::Get()->RemovePreTargetHandler(this);
}

void WelcomeTourAcceleratorHandler::OnKeyEvent(ui::KeyEvent* event) {
  const AcceleratorAction* const action =
      Shell::Get()->ash_accelerator_configuration()->FindAcceleratorAction(
          ui::Accelerator(*event));
  if (!action) {
    // Return early if `event` does not trigger any accelerator action.
    return;
  }

  auto action_it =
      base::ranges::find(kAllowedActions, *action, &AllowedAction::action);

  if (action_it == kAllowedActions.cend()) {
    // Block `event` if `action` is not allowed.
    event->StopPropagation();
  } else if (action_it->aborts_tour) {
    if (action_it->action == AcceleratorAction::kToggleSpokenFeedback &&
        features::IsWelcomeTourChromeVoxSupported()) {
      return;
    }

    // Aborting the Welcome Tour could affect the enabling of `action`.
    // Therefore, abort the Welcome Tour asynchronously.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, abort_tour_callback_);
  }
}

}  // namespace ash
