// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_COMMON_PANEL_FOCUS_DEPENDENT_HOTKEY_MANAGER_H_
#define CHROME_BROWSER_GLIC_COMMON_PANEL_FOCUS_DEPENDENT_HOTKEY_MANAGER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/glic/common/local_hotkey_manager.h"

namespace glic {

class ViewScopedRegistrationDelegate
    : public LocalHotkeyManager::RegistrationDelegate {
 public:
  explicit ViewScopedRegistrationDelegate(
      base::WeakPtr<LocalHotkeyManager::Panel> panel);
  ~ViewScopedRegistrationDelegate() override;

  std::unique_ptr<LocalHotkeyManager::ScopedHotkeyRegistration>
  CreateScopedHotkeyRegistration(
      ui::Accelerator accelerator,
      base::WeakPtr<ui::AcceleratorTarget> target) override;

 private:
  base::WeakPtr<LocalHotkeyManager::Panel> panel_;
};

// Manages hotkeys that are active ONLY when the Glic panel itself has focus.
// These hotkeys are registered locally within the Glic view/panel scope
// (typically via GlicView) and are only triggered when the user is actively
// interacting with Glic.
// Typically used for local controls like Escape to close, or zoom controls.
class PanelFocusDependentHotkeyManager
    : public LocalHotkeyManager::EventHandler {
 public:
  explicit PanelFocusDependentHotkeyManager(
      base::WeakPtr<LocalHotkeyManager::Panel> panel);
  ~PanelFocusDependentHotkeyManager() override;

  // LocalHotkeyManager::EventHandler:
  bool AcceleratorPressed(LocalHotkeyManager::Command command) override;
  bool CanHandleAccelerators() const override;

  // Initializes accelerators. This must be called after the panel's view has
  // been created (i.e., panel_->GetView() returns a valid view), as it
  // registers accelerators with that view. Calling it too early will result in
  // a CHECK failure.
  void InitializeAccelerators();

  base::WeakPtr<ui::AcceleratorTarget> GetAcceleratorTargetWeakPtr() {
    return hotkey_manager_->GetWeakPtr();
  }

 private:
  std::unique_ptr<LocalHotkeyManager> hotkey_manager_;
  base::WeakPtr<LocalHotkeyManager::Panel> panel_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_COMMON_PANEL_FOCUS_DEPENDENT_HOTKEY_MANAGER_H_
