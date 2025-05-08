// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_WIDGET_GLIC_WINDOW_HOTKEY_DELEGATE_H_
#define CHROME_BROWSER_GLIC_WIDGET_GLIC_WINDOW_HOTKEY_DELEGATE_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/glic/widget/local_hotkey_manager.h"
#include "ui/base/accelerators/accelerator.h"

namespace glic {

class GlicWindowController;

// Manages hotkeys that are active only when the Glic window itself has focus.
// This class acts as a delegate for a LocalHotkeyManager instance, configuring
// it with the set of hotkeys relevant to the Glic window (like Escape to close)
// and handling the registration and dispatch of those hotkeys within the
// Glic window's scope (typically via GlicView).
class GlicWindowHotkeyDelegate : public LocalHotkeyManager::Delegate {
 public:
  explicit GlicWindowHotkeyDelegate(
      base::WeakPtr<GlicWindowController> window_controller);
  ~GlicWindowHotkeyDelegate() override;

  // LocalHotkeyManager::Delegate:
  const base::span<const LocalHotkeyManager::Hotkey> GetSupportedHotkeys()
      const override;

  bool AcceleratorPressed(LocalHotkeyManager::Hotkey hotkey) override;
  std::unique_ptr<LocalHotkeyManager::ScopedHotkeyRegistration>
  CreateScopedHotkeyRegistration(
      ui::Accelerator accelerator,
      base::WeakPtr<ui::AcceleratorTarget> target) override;

  base::WeakPtr<GlicWindowHotkeyDelegate> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtr<GlicWindowController> window_controller_;
  base::WeakPtrFactory<GlicWindowHotkeyDelegate> weak_ptr_factory_{this};
};

std::unique_ptr<LocalHotkeyManager> MakeGlicWindowHotkeyManager(
    base::WeakPtr<GlicWindowController> window_controller);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_WIDGET_GLIC_WINDOW_HOTKEY_DELEGATE_H_
