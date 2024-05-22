// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_MAHI_MAHI_PREFS_CONTROLLER_ASH_H_
#define CHROME_BROWSER_CHROMEOS_MAHI_MAHI_PREFS_CONTROLLER_ASH_H_

#include "ash/public/cpp/session/session_observer.h"
#include "ash/shell_observer.h"
#include "base/scoped_observation.h"
#include "chrome/browser/chromeos/mahi/mahi_prefs_controller.h"

class PrefChangeRegistrar;
class PrefService;

namespace ash {
class SessionController;
class Shell;
}  // namespace ash

namespace mahi {

// An ash implementation of `MahiPrefsController`.
class MahiPrefsControllerAsh : public ash::SessionObserver,
                               public ash::ShellObserver,
                               public MahiPrefsController {
 public:
  MahiPrefsControllerAsh();

  MahiPrefsControllerAsh(const MahiPrefsControllerAsh&) = delete;
  MahiPrefsControllerAsh& operator=(const MahiPrefsControllerAsh&) = delete;

  ~MahiPrefsControllerAsh() override;

  // MahiPrefsController:
  void SetMahiEnabled(bool enabled) override;

 private:
  // ash::SessionObserver:
  void OnFirstSessionStarted() override;
  void OnChromeTerminating() override;

  // ash::ShellObserver:
  void OnShellDestroying() override;

  void RegisterPrefChanges(PrefService* pref_service);

  // Called when the related preferences are obtained from the pref service.
  void OnMahiEnableStateChanged();

  // Observes user profile prefs for the Assistant.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  base::ScopedObservation<ash::SessionController, ash::SessionObserver>
      session_observation_{this};

  base::ScopedObservation<ash::Shell, ash::ShellObserver> shell_observation_{
      this};
};

}  // namespace mahi

#endif  // CHROME_BROWSER_CHROMEOS_MAHI_MAHI_PREFS_CONTROLLER_ASH_H_
