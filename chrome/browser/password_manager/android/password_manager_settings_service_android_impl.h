// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_SETTINGS_SERVICE_ANDROID_IMPL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_SETTINGS_SERVICE_ANDROID_IMPL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/password_manager/android/password_manager_lifecycle_helper.h"
#include "chrome/browser/password_manager/android/password_settings_updater_android_bridge.h"
#include "chrome/browser/profiles/profile.h"
#include "components/password_manager/core/browser/password_manager_settings_service.h"
#include "components/sync/driver/sync_service.h"

class PrefService;

// Service implementation responsible with requesting and updating settings
// prefs based on settings changes in Google Mobile Services.
class PasswordManagerSettingsServiceAndroidImpl
    : public PasswordManagerSettingsService,
      public password_manager::PasswordSettingsUpdaterAndroidBridge::Consumer {
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

 private:
  // PasswordSettingsUpdaterAndroidBridge::Consumer implementation
  void OnSettingValueFetched(password_manager::PasswordManagerSetting setting,
                             bool value) override;
  void OnSettingValueAbsent(
      password_manager::PasswordManagerSetting setting) override;

  void OnChromeForegrounded();

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

  base::WeakPtrFactory<PasswordManagerSettingsServiceAndroidImpl>
      weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_SETTINGS_SERVICE_ANDROID_IMPL_H_
