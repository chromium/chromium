// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_LEVEL_LOGS_MANAGER_WRAPPER_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_LEVEL_LOGS_MANAGER_WRAPPER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_level_logs_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "components/prefs/pref_change_registrar.h"

namespace chromeos {

// Manages enabling/disabling kiosk application log collection controlled by the
// KioskApplicationLogCollectionEnabled policy pref.
class KioskAppLevelLogsManagerWrapper : public ProfileManagerObserver {
 public:
  explicit KioskAppLevelLogsManagerWrapper(ash::KioskAppId app_id);
  KioskAppLevelLogsManagerWrapper(Profile* profile, ash::KioskAppId app_id);

  KioskAppLevelLogsManagerWrapper(const KioskAppLevelLogsManagerWrapper&) =
      delete;
  KioskAppLevelLogsManagerWrapper& operator=(
      const KioskAppLevelLogsManagerWrapper&) = delete;

  ~KioskAppLevelLogsManagerWrapper() override;

  bool IsLogCollectionEnabled();

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override;

 private:
  void Init(Profile* profile);

  void EnableLogging();

  void DisableLogging();

  void OnPolicyChanged();

  // Handles collection and storage of kiosk app level logs.
  std::unique_ptr<KioskAppLevelLogsManager> logs_manager_;

  // The profile whose kiosk app logs should be collected. It is set by the
  // `OnProfileAdded` method if the profile is not set in the constructor.
  raw_ptr<Profile> profile_ = nullptr;

  const ash::KioskAppId app_id_;

  PrefChangeRegistrar pref_change_registrar_;

  // The profile manager is only observed when the profile is not passed in the
  // constructor.
  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observer_{this};

  base::WeakPtrFactory<KioskAppLevelLogsManagerWrapper> weak_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_LEVEL_LOGS_MANAGER_WRAPPER_H_
