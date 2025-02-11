// Copyright 2025 The Chromium Authors
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
      base::PassKey<class PasswordManagerSettingsServiceAndroidImplTest>,
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
  void UpdateSettingsCache(password_manager::PasswordManagerSetting setting,
                           std::optional<bool> value);

  // syncer::SyncServiceObserver implementation
  void OnStateChanged(syncer::SyncService* sync) override;

  // Pref service used to read and write password manager user prefs.
  raw_ptr<PrefService> pref_service_ = nullptr;

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

  base::WeakPtrFactory<PasswordManagerSettingsServiceAndroidImpl>
      weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_SETTINGS_SERVICE_ANDROID_IMPL_H_
