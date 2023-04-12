// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/app_mode/test/ash_accelerator_helpers.h"

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/shell.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash {

bool PressSignOutAccelerator() {
  // Ctrl + Shift + Q twice.
  ash::AcceleratorControllerImpl* controller =
      ash::Shell::Get()->accelerator_controller();
  ui::Accelerator signOutAccelerator =
      ui::Accelerator(ui::VKEY_Q, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
  return controller->AcceleratorPressed(signOutAccelerator) &&
         controller->AcceleratorPressed(signOutAccelerator);
}

}  // namespace ash
