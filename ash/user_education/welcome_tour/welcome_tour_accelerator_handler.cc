// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/welcome_tour/welcome_tour_accelerator_handler.h"

#include "ash/accelerators/ash_accelerator_configuration.h"
#include "ash/shell.h"
#include "base/containers/contains.h"
#include "ui/events/event.h"
#include "ui/events/event_target.h"

namespace ash {

// static
constexpr std::array<AcceleratorAction, 7>
    WelcomeTourAcceleratorHandler::kAllowedActions;

WelcomeTourAcceleratorHandler::WelcomeTourAcceleratorHandler() {
  Shell::Get()->AddPreTargetHandler(this, ui::EventTarget::Priority::kSystem);
}

WelcomeTourAcceleratorHandler::~WelcomeTourAcceleratorHandler() {
  Shell::Get()->RemovePreTargetHandler(this);
}

void WelcomeTourAcceleratorHandler::OnKeyEvent(ui::KeyEvent* event) {
  const AcceleratorAction* const action =
      Shell::Get()->ash_accelerator_configuration()->FindAcceleratorAction(
          ui::Accelerator(*event));

  // Block `event` if the action corresponding to `event` is not allowed.
  if (action && !base::Contains(kAllowedActions, *action)) {
    event->StopPropagation();
  }
}

}  // namespace ash
