// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_COMMON_PANEL_VISIBILITY_DEPENDENT_HOTKEY_MANAGER_H_
#define CHROME_BROWSER_GLIC_COMMON_PANEL_VISIBILITY_DEPENDENT_HOTKEY_MANAGER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/glic/common/local_hotkey_manager.h"

namespace glic {

// Manages application-scoped hotkeys that are handled by a specific embedder.
// These hotkeys are registered application-wide (across all browser windows)
// but their actions are routed to and handled by a specific embedder panel
// (e.g., GlicFloatingUi or GlicSidePanelUi).
// Typically used for hotkeys like Focus Toggle which should work anywhere in
// the browser but need to target a specific Glic instance/panel.
class PanelVisibilityDependentHotkeyManager
    : public LocalHotkeyManager::EventHandler {
 public:
  explicit PanelVisibilityDependentHotkeyManager(
      base::WeakPtr<LocalHotkeyManager::Panel> panel);
  ~PanelVisibilityDependentHotkeyManager() override;

  // LocalHotkeyManager::EventHandler:
  bool AcceleratorPressed(LocalHotkeyManager::Command command) override;
  bool CanHandleAccelerators() const override;

  void InitializeAccelerators();

 private:
  std::unique_ptr<LocalHotkeyManager> hotkey_manager_;
  base::WeakPtr<LocalHotkeyManager::Panel> panel_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_COMMON_PANEL_VISIBILITY_DEPENDENT_HOTKEY_MANAGER_H_
