// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_troubleshooting_controller_ash.h"

#include <map>
#include <memory>
#include <vector>

#include "ash/accelerators/accelerator_commands.h"
#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/shell.h"

namespace ash {

KioskTroubleshootingControllerAsh::KioskTroubleshootingControllerAsh(
    PrefService* pref_service,
    base::OnceClosure shutdown_app_session_callback)
    : KioskTroubleshootingController(pref_service,
                                     std::move(shutdown_app_session_callback)) {
  RegisterTroubleshootingAccelerators();
}

KioskTroubleshootingControllerAsh::~KioskTroubleshootingControllerAsh() {
  Shell::Get()->accelerator_controller()->UnregisterAll(this);
}

bool KioskTroubleshootingControllerAsh::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  // Do not process any accelerators if troubleshooting tools are disabled.
  if (!AreKioskTroubleshootingToolsEnabled()) {
    return false;
  }

  auto it = accelerators_with_actions_.find(accelerator);
  DCHECK(it != accelerators_with_actions_.end());

  switch (it->second) {
    case TroubleshootingAcceleratorAction::NEW_WINDOW:
      accelerators::NewWindow();
      return true;
    case TroubleshootingAcceleratorAction::SWITCH_WINDOWS_FORWARD:
      accelerators::CycleForwardMru(/*same_app_only=*/false);
      return true;
    case TroubleshootingAcceleratorAction::SWITCH_WINDOWS_BACKWARD:
      accelerators::CycleBackwardMru(/*same_app_only=*/false);
      return true;
    case TroubleshootingAcceleratorAction::SHOW_TASK_MANAGER:
      accelerators::ShowTaskManager();
      return true;
  }

  return false;
}

bool KioskTroubleshootingControllerAsh::CanHandleAccelerators() const {
  return AreKioskTroubleshootingToolsEnabled();
}

void KioskTroubleshootingControllerAsh::RegisterTroubleshootingAccelerators() {
  // Ctrl+N
  accelerators_with_actions_.insert(
      {ui::Accelerator(ui::VKEY_N, ui::EF_CONTROL_DOWN),
       TroubleshootingAcceleratorAction::NEW_WINDOW});

  // Alt+Tab
  accelerators_with_actions_.insert(
      {ui::Accelerator(ui::VKEY_TAB, ui::EF_ALT_DOWN),
       TroubleshootingAcceleratorAction::SWITCH_WINDOWS_FORWARD});

  // Shift+Alt+Tab
  accelerators_with_actions_.insert(
      {ui::Accelerator(ui::VKEY_TAB, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN),
       TroubleshootingAcceleratorAction::SWITCH_WINDOWS_BACKWARD});

  // Search+Esc
  accelerators_with_actions_.insert(
      {ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_COMMAND_DOWN),
       TroubleshootingAcceleratorAction::SHOW_TASK_MANAGER});

  Shell::Get()->accelerator_controller()->Register(GetAllAccelerators(), this);
}

std::vector<ui::Accelerator>
KioskTroubleshootingControllerAsh::GetAllAccelerators() const {
  std::vector<ui::Accelerator> accelerators;
  for (auto const& accelerator : accelerators_with_actions_) {
    accelerators.emplace_back(accelerator.first);
  }
  return accelerators;
}

}  // namespace ash
