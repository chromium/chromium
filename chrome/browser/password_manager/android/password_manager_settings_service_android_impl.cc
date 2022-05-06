// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_manager_settings_service_android_impl.h"

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/password_manager/android/password_manager_lifecycle_helper_impl.h"
#include "chrome/browser/password_manager/android/password_settings_updater_android_bridge.h"
#include "chrome/browser/password_manager/android/password_settings_updater_android_bridge_impl.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_manager_setting.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"

using password_manager::PasswordManagerSetting;
using password_manager::PasswordSettingsUpdaterAndroidBridge;

namespace {

// Returns the preference in which a setting value coming from Google Mobile
// Services should be stored.
const PrefService::Preference* GetGMSPrefFromSetting(
    PrefService* pref_service,
    PasswordManagerSetting setting) {
  switch (setting) {
    case PasswordManagerSetting::kOfferToSavePasswords:
      return pref_service->FindPreference(
          password_manager::prefs::kOfferToSavePasswordsEnabledGMS);
    case PasswordManagerSetting::kAutoSignIn:
      return pref_service->FindPreference(
          password_manager::prefs::kAutoSignInEnabledGMS);
  }
}

// Returns the cross-platform preferences in which password manager settings
// are stored. These are not directly used on Android when the unified password
// manager is enabled.
const PrefService::Preference* GetRegularPrefFromSetting(
    PrefService* pref_service,
    PasswordManagerSetting setting) {
  switch (setting) {
    case PasswordManagerSetting::kOfferToSavePasswords:
      return pref_service->FindPreference(
          password_manager::prefs::kCredentialsEnableService);
    case PasswordManagerSetting::kAutoSignIn:
      return pref_service->FindPreference(
          password_manager::prefs::kCredentialsEnableAutosignin);
  }
}

bool HasChosenToSyncPreferences(syncer::SyncService* sync_service) {
  return sync_service && sync_service->IsSyncFeatureEnabled() &&
         sync_service->GetUserSettings()->GetSelectedTypes().Has(
             syncer::UserSelectableType::kPreferences);
}

}  // namespace

PasswordManagerSettingsServiceAndroidImpl::
    PasswordManagerSettingsServiceAndroidImpl(PrefService* pref_service,
                                              syncer::SyncService* sync_service)
    : pref_service_(pref_service),
      sync_service_(sync_service),
      bridge_(PasswordSettingsUpdaterAndroidBridge::Create()),
      lifecycle_helper_(
          std::make_unique<PasswordManagerLifecycleHelperImpl>()) {
  DCHECK(pref_service_);
  DCHECK(sync_service_);
  bridge_->SetConsumer(weak_ptr_factory_.GetWeakPtr());
  lifecycle_helper_->RegisterObserver(base::BindRepeating(
      &PasswordManagerSettingsServiceAndroidImpl::OnChromeForegrounded,
      weak_ptr_factory_.GetWeakPtr()));
}

PasswordManagerSettingsServiceAndroidImpl::
    PasswordManagerSettingsServiceAndroidImpl(
        base::PassKey<class PasswordManagerSettingsServiceAndroidImplTest>,
        PrefService* pref_service,
        syncer::SyncService* sync_service,
        std::unique_ptr<PasswordSettingsUpdaterAndroidBridge> bridge,
        std::unique_ptr<PasswordManagerLifecycleHelper> lifecycle_helper)
    : pref_service_(pref_service),
      sync_service_(sync_service),
      bridge_(std::move(bridge)),
      lifecycle_helper_(std::move(lifecycle_helper)) {
  DCHECK(pref_service_);
  DCHECK(sync_service_);
  DCHECK(bridge_);
  DCHECK(lifecycle_helper_);
  bridge_->SetConsumer(weak_ptr_factory_.GetWeakPtr());
  lifecycle_helper_->RegisterObserver(base::BindRepeating(
      &PasswordManagerSettingsServiceAndroidImpl::OnChromeForegrounded,
      weak_ptr_factory_.GetWeakPtr()));
}

PasswordManagerSettingsServiceAndroidImpl::
    ~PasswordManagerSettingsServiceAndroidImpl() {
  lifecycle_helper_->UnregisterObserver();
}

void PasswordManagerSettingsServiceAndroidImpl::OnChromeForegrounded() {
  if (!password_bubble_experiment::HasChosenToSyncPasswords(sync_service_))
    return;

  // TODO(crbug.com/1289700): Request the settings from the backend.
}

void PasswordManagerSettingsServiceAndroidImpl::OnSettingValueFetched(
    password_manager::PasswordManagerSetting setting,
    bool value) {
  if (!password_bubble_experiment::HasChosenToSyncPasswords(sync_service_))
    return;
  const PrefService::Preference* android_pref =
      GetGMSPrefFromSetting(pref_service_, setting);
  pref_service_->SetBoolean(android_pref->name(), value);

  // Updating the regular pref now will ensure that if passwods sync turns off
  // the regular pref contains the latest setting value. This can only be done
  // when preference syncing is off, otherwise it might cause sync cycles.
  // When sync is on, the regular preference gets updated via sync, so this
  // step is not necessary.
  if (!HasChosenToSyncPreferences(sync_service_)) {
    const PrefService::Preference* regular_pref =
        GetRegularPrefFromSetting(pref_service_, setting);
    pref_service_->SetBoolean(regular_pref->name(), value);
  }
}

void PasswordManagerSettingsServiceAndroidImpl::OnSettingValueAbsent(
    password_manager::PasswordManagerSetting setting) {
  if (!password_bubble_experiment::HasChosenToSyncPasswords(sync_service_))
    return;
  const PrefService::Preference* pref =
      GetGMSPrefFromSetting(pref_service_, setting);

  // If both GMS and Chrome have default values for the setting, then no update
  // is needed.
  if (pref->IsDefaultValue())
    return;

  // If Chrome has an explicitly set value, GMS needs to know about it.
  // TODO(crbug.com/1289700): Check whether this should be guarded by a
  // migration pref.
  bridge_->SetPasswordSettingValue(
      PasswordSettingsUpdaterAndroidBridge::SyncingAccount(
          sync_service_->GetAccountInfo().email),
      setting, pref->GetValue()->GetBool());
}
