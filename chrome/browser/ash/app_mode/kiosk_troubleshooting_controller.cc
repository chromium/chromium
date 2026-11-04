// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/kiosk_troubleshooting_controller.h"

#include <map>
#include <utility>
#include <vector>

#include "ash/accelerators/accelerator_commands.h"
#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/shell.h"
#include "base/logging.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

KioskTroubleshootingController::KioskTroubleshootingController(
    PrefService* pref_service,
    base::OnceClosure shutdown_kiosk_browser_session_callback)
    : pref_service_(pref_service),
      shutdown_kiosk_browser_session_callback_(
          std::move(shutdown_kiosk_browser_session_callback)) {
  pref_change_registrar_.Init(pref_service);
  pref_change_registrar_.Add(
      ash::prefs::kKioskTroubleshootingToolsEnabled,
      base::BindRepeating(&KioskTroubleshootingController::PolicyChanged,
                          base::Unretained(this)));
  RegisterTroubleshootingAccelerators();
}

KioskTroubleshootingController::~KioskTroubleshootingController() {
  ash::Shell::Get()->accelerator_controller()->UnregisterAll(this);
}

bool KioskTroubleshootingController::AreKioskTroubleshootingToolsEnabled()
    const {
  return pref_service_->GetBoolean(
      ash::prefs::kKioskTroubleshootingToolsEnabled);
}

bool KioskTroubleshootingController::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  auto it = accelerators_with_actions_.find(accelerator);
  if (it == accelerators_with_actions_.end()) {
    return false;
  }

  // Block registered accelerators if the troubleshooting tools are disabled.
  if (!AreKioskTroubleshootingToolsEnabled()) {
    return true;
  }

  switch (it->second) {
    case TroubleshootingAcceleratorAction::NEW_WINDOW:
      ash::accelerators::NewWindow();
      return true;
    case TroubleshootingAcceleratorAction::SWITCH_WINDOWS_FORWARD:
      ash::accelerators::CycleForwardMru();
      return true;
    case TroubleshootingAcceleratorAction::SWITCH_WINDOWS_BACKWARD:
      ash::accelerators::CycleBackwardMru();
      return true;
    case TroubleshootingAcceleratorAction::SHOW_TASK_MANAGER:
      ash::accelerators::ShowTaskManager();
      return true;
    case TroubleshootingAcceleratorAction::OPEN_FEEDBACK_PAGE:
      ash::accelerators::OpenFeedbackPage();
      return true;
    case TroubleshootingAcceleratorAction::TOGGLE_OVERVIEW:
      ash::accelerators::ToggleOverview();
      return true;
  }

  return false;
}

bool KioskTroubleshootingController::CanHandleAccelerators() const {
  return true;
}

void KioskTroubleshootingController::RegisterTroubleshootingAccelerators() {
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

  // Shift+Alt+I
  accelerators_with_actions_.insert(
      {ui::Accelerator(ui::VKEY_I, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN),
       TroubleshootingAcceleratorAction::OPEN_FEEDBACK_PAGE});
  // F5
  accelerators_with_actions_.insert(
      {ui::Accelerator(ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_NONE),
       TroubleshootingAcceleratorAction::TOGGLE_OVERVIEW});
  ash::Shell::Get()->accelerator_controller()->Register(GetAllAccelerators(),
                                                        this);
}

std::vector<ui::Accelerator>
KioskTroubleshootingController::GetAllAccelerators() const {
  std::vector<ui::Accelerator> accelerators;
  for (auto const& accelerator : accelerators_with_actions_) {
    accelerators.emplace_back(accelerator.first);
  }
  return accelerators;
}

void KioskTroubleshootingController::PolicyChanged() {
  // If the policy value is changed from enabled to disabled, exit the kiosk
  // session.
  if (!AreKioskTroubleshootingToolsEnabled()) {
    LOG(WARNING)
        << "Troubleshooting tools were disabled, ending kiosk session.";
    std::move(shutdown_kiosk_browser_session_callback_).Run();
  }
  // Policy is enabled now, no action needed.
}

}  // namespace chromeos
