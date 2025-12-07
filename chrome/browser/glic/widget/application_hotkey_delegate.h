// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_WIDGET_APPLICATION_HOTKEY_DELEGATE_H_
#define CHROME_BROWSER_GLIC_WIDGET_APPLICATION_HOTKEY_DELEGATE_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/glic/widget/local_hotkey_manager.h"
#include "ui/base/accelerators/accelerator.h"

namespace glic {

// Manages hotkeys that are active application-wide when Glic is relevant.
// This class acts as a delegate for a LocalHotkeyManager instance, configuring
// it with the set of hotkeys relevant application-wide (like focus toggle)
// and handling the registration and dispatch of those hotkeys within the
// application's scope (typically via BrowserView's FocusManager).
class ApplicationHotkeyDelegate : public LocalHotkeyManager::Delegate {
 public:
  explicit ApplicationHotkeyDelegate(
      base::WeakPtr<LocalHotkeyManager::Panel> panel);
  ~ApplicationHotkeyDelegate() override;

  // LocalHotkeyManager::Delegate:
  const base::span<const LocalHotkeyManager::Hotkey> GetSupportedHotkeys()
      const override;

  bool AcceleratorPressed(LocalHotkeyManager::Hotkey Hotkey) override;
  std::unique_ptr<LocalHotkeyManager::ScopedHotkeyRegistration>
  CreateScopedHotkeyRegistration(
      ui::Accelerator accelerator,
      base::WeakPtr<ui::AcceleratorTarget> target) override;

  base::WeakPtr<ApplicationHotkeyDelegate> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtr<LocalHotkeyManager::Panel> panel_;
  base::WeakPtrFactory<ApplicationHotkeyDelegate> weak_ptr_factory_{this};
};

std::unique_ptr<LocalHotkeyManager> MakeApplicationHotkeyManager(
    base::WeakPtr<LocalHotkeyManager::Panel> panel);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_WIDGET_APPLICATION_HOTKEY_DELEGATE_H_
