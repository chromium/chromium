// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DICTATION_LOCAL_HOTKEY_MANAGER_H_
#define CHROME_BROWSER_DICTATION_LOCAL_HOTKEY_MANAGER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/base/accelerators/accelerator.h"

class Profile;

namespace dictation {

// Manages the local hotkey for Dictation.
// It listens to preference changes and registers the hotkey with standard
// priority using a delegate.
class LocalHotkeyManager : public ui::AcceleratorTarget {
 public:
  // Interface for managing the lifetime of a registered hotkey.
  // Implementations handle the specific registration/unregistration logic
  // for their scope.
  class ScopedHotkeyRegistration {
   public:
    virtual ~ScopedHotkeyRegistration() = default;
  };

  // Delegate for registering hotkeys. Allows for different registration
  // mechanisms.
  class RegistrationDelegate {
   public:
    virtual ~RegistrationDelegate() = default;
    virtual std::unique_ptr<ScopedHotkeyRegistration>
    CreateScopedHotkeyRegistration(Profile* profile,
                                   ui::Accelerator accelerator,
                                   LocalHotkeyManager& hotkey_manager) = 0;
  };

  LocalHotkeyManager(
      Profile* profile,
      std::unique_ptr<RegistrationDelegate> registration_delegate);

  LocalHotkeyManager(const LocalHotkeyManager&) = delete;
  LocalHotkeyManager& operator=(const LocalHotkeyManager&) = delete;

  ~LocalHotkeyManager() override;

  // ui::AcceleratorTarget:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  bool CanHandleAccelerators() const override;

 private:
  void OnHotkeyPrefChanged();
  void UpdateRegistration();

  raw_ptr<Profile> profile_;
  std::unique_ptr<RegistrationDelegate> registration_delegate_;

  PrefChangeRegistrar pref_registrar_;
  std::unique_ptr<ScopedHotkeyRegistration> hotkey_registration_;
};

}  // namespace dictation

#endif  // CHROME_BROWSER_DICTATION_LOCAL_HOTKEY_MANAGER_H_
