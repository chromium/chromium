// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_WIDGET_GLIC_PANEL_HOTKEY_DELEGATE_H_
#define CHROME_BROWSER_GLIC_WIDGET_GLIC_PANEL_HOTKEY_DELEGATE_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/glic/widget/local_hotkey_manager.h"
#include "ui/base/accelerators/accelerator.h"

namespace glic {

// Manages hotkeys that are active only when the Glic window itself has focus.
// This class acts as a delegate for a LocalHotkeyManager instance, configuring
// it with the set of hotkeys relevant to the Glic window (like Escape to close)
// and handling the registration and dispatch of those hotkeys within the
// Glic window's scope (typically via GlicView).
class GlicPanelHotkeyDelegate : public LocalHotkeyManager::Delegate {
 public:
  explicit GlicPanelHotkeyDelegate(
      base::WeakPtr<LocalHotkeyManager::Panel> panel);
  ~GlicPanelHotkeyDelegate() override;

  // LocalHotkeyManager::Delegate:
  const base::span<const LocalHotkeyManager::Hotkey> GetSupportedHotkeys()
      const override;

  bool AcceleratorPressed(LocalHotkeyManager::Hotkey hotkey) override;
  std::unique_ptr<LocalHotkeyManager::ScopedHotkeyRegistration>
  CreateScopedHotkeyRegistration(
      ui::Accelerator accelerator,
      base::WeakPtr<ui::AcceleratorTarget> target) override;

  base::WeakPtr<GlicPanelHotkeyDelegate> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtr<LocalHotkeyManager::Panel> panel_;
  base::WeakPtrFactory<GlicPanelHotkeyDelegate> weak_ptr_factory_{this};
};

std::unique_ptr<LocalHotkeyManager> MakeGlicWindowHotkeyManager(
    base::WeakPtr<LocalHotkeyManager::Panel> panel);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_WIDGET_GLIC_PANEL_HOTKEY_DELEGATE_H_
