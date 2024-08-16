// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_SETTINGS_SERVICE_ANDROID_IMPL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_SETTINGS_SERVICE_ANDROID_IMPL_H_

#include <memory>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/types/pass_key.h"
#include "chrome/browser/password_manager/android/password_manager_lifecycle_helper.h"
#include "chrome/browser/password_manager/android/password_settings_updater_android_bridge_helper.h"
#include "chrome/browser/password_manager/android/password_settings_updater_android_receiver_bridge.h"
#include "components/password_manager/core/browser/password_manager_settings_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sync/service/sync_service.h"

class PrefService;

struct PasswordManagerSettingGmsAccessResult {
  password_manager::PasswordManagerSetting setting;
  bool was_successful;
};

// Service implementation responsible with requesting and updating settings
// prefs based on settings changes in Google Mobile Services. It also answers
// password manager prefs queries, taking into account managed prefs and
// the possibility of communicating with GMS.
class PasswordManagerSettingsServiceAndroidImpl
    : public password_manager::PasswordManagerSettingsService,
      public password_manager::PasswordSettingsUpdaterAndroidReceiverBridge::
          Consumer,
      public syncer::SyncServiceObserver {
 public:
  PasswordManagerSettingsServiceAndroidImpl(PrefService* pref_service,
                                            syncer::SyncService* sync_service);
  PasswordManagerSettingsServiceAndroidImpl(
      base::PassKey<class PasswordManagerSettingsServiceAndroidImplBaseTest>,
      PrefService* pref_service,
      syncer::SyncService* sync_service,
      std::unique_ptr<
          password_manager::PasswordSettingsUpdaterAndroidBridgeHelper>
          bridge_helper,
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
      password_manager::PasswordManagerSetting setting) const override;
  void RequestSettingsFromBackend() override;
  void TurnOffAutoSignIn() override;

 private:
  // Does actions that need to be done on startup (e.g. attaches services
  // observers and migrates and requests settings if needed).
  void Init();

  void OnChromeForegrounded();

  // PasswordSettingsUpdaterAndroidBridgeHelper::Consumer implementation
  void OnSettingValueFetched(password_manager::PasswordManagerSetting setting,
                             bool value) override;
  void OnSettingValueAbsent(
      password_manager::PasswordManagerSetting setting) override;
  void OnSettingFetchingError(
      password_manager::PasswordManagerSetting setting,
      AndroidBackendAPIErrorCode api_error_code) override;
  void OnSuccessfulSettingChange(
      password_manager::PasswordManagerSetting setting) override;
  void OnFailedSettingChange(
      password_manager::PasswordManagerSetting setting,
      AndroidBackendAPIErrorCode api_error_code) override;

  // Stores the given `value` of the `setting` into the android-only GMS prefs.
  // Stores the same `value` in the old prefs are not being synced.
  // If the `value` is not given, the prefs will be set to default.
  void WriteToTheCacheAndRegularPref(
      password_manager::PasswordManagerSetting setting,
      std::optional<bool> value);

  // syncer::SyncServiceObserver implementation
  void OnStateChanged(syncer::SyncService* sync) override;

  // Updates information about the current setting fetch after receiving
  // a reply from the backend.
  void UpdateSettingFetchState(
      password_manager::PasswordManagerSetting received_setting);

  // Asynchronously fetches settings from backend regardless of sync status.
  void FetchSettings();

  // Migrates settings to GMS Core if the user is reenrolled into the UPM
  // in the middle of the browser session.
  void OnUnenrollmentPreferenceChanged();

  // Checks that the user is either syncing and enrolled in UPM or not syncing
  // and ready to use local UPM.
  bool UsesUPMBackend() const;

  // Checks for the settings migration requirements. It goes through every
  // setting pref and resolves differences between value in Chrome and GMS.
  // This method will be run for upm users with and without local passwords
  // support. For the former ones this should be a noop without any changes to
  // their prefs. For the latter ones pref values might change.
  void MigratePrefsIfNeeded(
      const std::vector<PasswordManagerSettingGmsAccessResult>& results);

  // Checks if setting prefs was successful and marks the migation as complete
  // if there were no errors.
  void FinishSettingsMigration(
      const std::vector<PasswordManagerSettingGmsAccessResult>& results);

  // Pref service used to read and write password manager user prefs.
  raw_ptr<PrefService> pref_service_ = nullptr;

  // An observer for the pref service.
  PrefChangeRegistrar pref_change_registrar_;

  // Sync service used to check whether the user has chosen to sync passwords
  // or settings.
  raw_ptr<syncer::SyncService> sync_service_ = nullptr;

  // Bridge helper used by the service to communicate with the Java backend.
  std::unique_ptr<password_manager::PasswordSettingsUpdaterAndroidBridgeHelper>
      bridge_helper_;

  // Notifies the service when Chrome is foregrounded, so that the service
  // can request settings values from Google Mobile Services.
  std::unique_ptr<PasswordManagerLifecycleHelper> lifecycle_helper_;

  // Cached value of the password sync setting.
  bool is_password_sync_enabled_ = false;

  // True if settings were requested from the backend after password sync
  // setting was changed, and the fetch is still in progress.
  bool fetch_after_sync_status_change_in_progress_ = false;

  // Tires to start migration after getting prefs from gms finished.
  // This callback needs to be run even if getting pref value from GMS failed,
  // because of the following scenario:
  // 1. Chrome is opened, fetching prefs has started.
  // 2. One of the calls to GMS fails, the other succeeds.
  // 3. Chrome is put to the background and then to the foreground again.
  // 4. That causes fetching prefs from the GMS again. And if one of them fails
  // and the other succeeds, we would count the migration as successful. By
  // calling the `start_migration_callback_` with was_successful set to false we
  // ensure that if anything failed, migration will be counted as failed.
  base::RepeatingCallback<void(PasswordManagerSettingGmsAccessResult)>
      start_migration_callback_;

  // Finishes migration and checks for errors.
  base::RepeatingCallback<void(PasswordManagerSettingGmsAccessResult)>
      migration_finished_callback_;

  // Settings requested from the backend after a sync status change, but not
  // fetched yet.
  base::flat_set<password_manager::PasswordManagerSetting> awaited_settings_;

  base::WeakPtrFactory<PasswordManagerSettingsServiceAndroidImpl>
      weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_SETTINGS_SERVICE_ANDROID_IMPL_H_
