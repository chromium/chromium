// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_COMMON_APPLICATION_HOTKEY_DELEGATE_H_
#define CHROME_BROWSER_GLIC_COMMON_APPLICATION_HOTKEY_DELEGATE_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/glic/common/local_hotkey_manager.h"
#include "ui/base/accelerators/accelerator.h"

namespace glic {

// A registration delegate that registers hotkeys application-wide.
// It ensures that the registered accelerators are active in all current and
// future browser windows by observing the global browser collection and
// registering/unregistering with each window's FocusManager (or
// platform-specific equivalent on Android).
class ApplicationScopedRegistrationDelegate
    : public LocalHotkeyManager::RegistrationDelegate {
 public:
  ApplicationScopedRegistrationDelegate();
  ~ApplicationScopedRegistrationDelegate() override;

  std::unique_ptr<LocalHotkeyManager::ScopedHotkeyRegistration>
  CreateScopedHotkeyRegistration(
      ui::Accelerator accelerator,
      base::WeakPtr<ui::AcceleratorTarget> target) override;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_COMMON_APPLICATION_HOTKEY_DELEGATE_H_
