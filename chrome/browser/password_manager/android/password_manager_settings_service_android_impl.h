// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_SETTINGS_SERVICE_ANDROID_IMPL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_SETTINGS_SERVICE_ANDROID_IMPL_H_

#include <memory>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/password_manager/android/password_manager_lifecycle_helper.h"
#include "chrome/browser/password_manager/android/password_settings_updater_android_bridge.h"
#include "chrome/browser/profiles/profile.h"
#include "components/password_manager/core/browser/password_manager_settings_service.h"
#include "components/sync/driver/sync_service.h"

class PrefService;

// Service implementation responsible with requesting and updating settings
// prefs based on settings changes in Google Mobile Services. It also answers
// password manager prefs queries, taking into account managed prefs and
// the possibility of communicating with GMS.
class PasswordManagerSettingsServiceAndroidImpl
    : public PasswordManagerSettingsService,
      public password_manager::PasswordSettingsUpdaterAndroidBridge::Consumer,
      public syncer::SyncServiceObserver {
 public:
  PasswordManagerSettingsServiceAndroidImpl(PrefService* pref_service,
                                            syncer::SyncService* sync_service);
  PasswordManagerSettingsServiceAndroidImpl(
      base::PassKey<class PasswordManagerSettingsServiceAndroidImplTest>,
      PrefService* pref_service,
      syncer::SyncService* sync_service,
      std::unique_ptr<password_manager::PasswordSettingsUpdaterAndroidBridge>
          bridge,
      std::unique_ptr<PasswordManagerLifecycleHelper> lifecycle_helper);

  PasswordManagerSettingsServiceAndroidImpl(
      const PasswordManagerSettingsServiceAndroidImpl&) = delete;
  PasswordManagerSettingsServiceAndroidImpl(
      PasswordManagerSettingsServiceAndroidImpl&&) = delete;
  PasswordManagerSettingsServiceAndroidImpl& operator=(
      const PasswordManagerSettingsServiceAndroidImpl&) = delete;
  PasswordManagerSettingsServiceAndroidImpl& operator=(
      const PasswordManagerSettingsServiceAndroidImpl&&) = delete;

  ~PasswordManagerSettingsServiceAndroidImpl() override;

  // PasswordManagerSettingsService implementation
  bool IsSettingEnabled(
      password_manager::PasswordManagerSetting setting) override;
  void RequestSettingsFromBackend() override;
  void TurnOffAutoSignIn() override;

 private:
  void OnChromeForegrounded();

  // PasswordSettingsUpdaterAndroidBridge::Consumer implementation
  void OnSettingValueFetched(password_manager::PasswordManagerSetting setting,
                             bool value) override;
  void OnSettingValueAbsent(
      password_manager::PasswordManagerSetting setting) override;

  // Updates the non syncable, android-only prefs with the values of the
  // syncable cross-platform prefs as the latter won't be used when UPM
  // is up and running. There is no need to migrate the values until sync turns
  // on, because UPM is not running until then. When sync turns on, this
  // will be handled as part of the sync state change rather than migration.
  // If a migration was already performed, there is no need
  // to migrate again.
  void MigratePrefsIfNeeded();

  // syncer::SyncServiceObserver implementation
  void OnStateChanged(syncer::SyncService* sync) override;

  // Updates information about the current setting fetch after receiving
  // a reply from the backend.
  void UpdateSettingFetchState(
      password_manager::PasswordManagerSetting received_setting);

  // Asynchronously fetches settings from backend regardless of sync status.
  void FetchSettings();

  // Copies the values of chrome prefs that have user-set values into the
  // GMS prefs.
  void DumpChromePrefsIntoGMSPrefs();

  // Pref service used to read and write password manager user prefs.
  raw_ptr<PrefService> pref_service_ = nullptr;

  // Sync service used to check whether the user has chosen to sync passwords
  // or settings.
  raw_ptr<syncer::SyncService> sync_service_ = nullptr;

  // Bridge used by the service to talk to the Java side.
  std::unique_ptr<password_manager::PasswordSettingsUpdaterAndroidBridge>
      bridge_;

  // Notifies the service when Chrome is foregrounded, so that the service
  // can request settings values from Google Mobile Services.
  std::unique_ptr<PasswordManagerLifecycleHelper> lifecycle_helper_;

  // Cached value of the password sync setting.
  bool is_password_sync_enabled_ = false;

  // True if settings were requested from the backend after password sync
  // setting was changed, and the fetch is still in progress.
  bool fetch_after_sync_status_change_in_progress_ = false;

  // Settings requested from the backend after a sunc status change, but not
  // fetched yet.
  base::flat_set<password_manager::PasswordManagerSetting> awaited_settings_;

  base::WeakPtrFactory<PasswordManagerSettingsServiceAndroidImpl>
      weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_SETTINGS_SERVICE_ANDROID_IMPL_H_
