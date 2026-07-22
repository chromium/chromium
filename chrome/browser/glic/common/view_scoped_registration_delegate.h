// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_COMMON_VIEW_SCOPED_REGISTRATION_DELEGATE_H_
#define CHROME_BROWSER_GLIC_COMMON_VIEW_SCOPED_REGISTRATION_DELEGATE_H_

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

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_COMMON_VIEW_SCOPED_REGISTRATION_DELEGATE_H_
