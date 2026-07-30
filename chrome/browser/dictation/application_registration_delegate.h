// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DICTATION_APPLICATION_REGISTRATION_DELEGATE_H_
#define CHROME_BROWSER_DICTATION_APPLICATION_REGISTRATION_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/dictation/local_hotkey_manager.h"

class Profile;

namespace dictation {

// A registration delegate that registers hotkeys application-wide for a
// specific profile.
class ApplicationRegistrationDelegate
    : public LocalHotkeyManager::RegistrationDelegate {
 public:
  ApplicationRegistrationDelegate();
  ApplicationRegistrationDelegate(const ApplicationRegistrationDelegate&) =
      delete;
  ApplicationRegistrationDelegate& operator=(
      const ApplicationRegistrationDelegate&) = delete;
  ~ApplicationRegistrationDelegate() override;

  std::unique_ptr<LocalHotkeyManager::ScopedHotkeyRegistration>
  CreateScopedHotkeyRegistration(Profile* profile,
                                 ui::Accelerator accelerator,
                                 LocalHotkeyManager& hotkey_manager) override;
};

}  // namespace dictation

#endif  // CHROME_BROWSER_DICTATION_APPLICATION_REGISTRATION_DELEGATE_H_
