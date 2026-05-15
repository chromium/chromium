// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/common/panel_visibility_dependent_hotkey_manager.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/glic/common/application_hotkey_delegate.h"

namespace glic {

namespace {
constexpr std::array<LocalHotkeyManager::Command, 1> kSupportedCommands = {
    LocalHotkeyManager::Command::kFocusToggle};
}  // namespace

PanelVisibilityDependentHotkeyManager::PanelVisibilityDependentHotkeyManager(
    base::WeakPtr<LocalHotkeyManager::Panel> panel)
    : panel_(panel) {
  hotkey_manager_ = std::make_unique<LocalHotkeyManager>(
      std::make_unique<ApplicationScopedRegistrationDelegate>(), this,
      kSupportedCommands);
}

PanelVisibilityDependentHotkeyManager::
    ~PanelVisibilityDependentHotkeyManager() = default;

bool PanelVisibilityDependentHotkeyManager::AcceleratorPressed(
    LocalHotkeyManager::Command command) {
  if (!panel_) {
    return false;
  }

  switch (command) {
    case LocalHotkeyManager::Command::kFocusToggle:
      if (!panel_->IsShowing()) {
        return false;
      }
      panel_->FocusIfOpen();
      base::RecordAction(base::UserMetricsAction("Glic.FocusHotKey"));
      return true;
    default:
      NOTREACHED() << "no handling implemented for "
                   << LocalHotkeyManager::CommandToString(command);
  }
}

bool PanelVisibilityDependentHotkeyManager::CanHandleAccelerators() const {
  return panel_ && panel_->IsShowing();
}

void PanelVisibilityDependentHotkeyManager::InitializeAccelerators() {
  hotkey_manager_->InitializeAccelerators();
}

}  // namespace glic
