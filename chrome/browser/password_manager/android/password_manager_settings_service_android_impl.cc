// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_manager_settings_service_android_impl.h"

#include <optional>
#include <vector>

#include "base/barrier_callback.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "chrome/browser/password_manager/android/password_manager_lifecycle_helper_impl.h"
#include "chrome/browser/password_manager/android/password_settings_updater_android_bridge_helper.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_manager_setting.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/base/features.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"

using password_manager::PasswordManagerSetting;
using password_manager::PasswordSettingsUpdaterAndroidBridgeHelper;

namespace {

using Consumer =
    password_manager::PasswordSettingsUpdaterAndroidReceiverBridge::Consumer;
using SyncingAccount = password_manager::
    PasswordSettingsUpdaterAndroidReceiverBridge::SyncingAccount;

const std::vector<PasswordManagerSetting> GetAllPasswordSettings() {
  return base::FeatureList::IsEnabled(
             password_manager::features::kBiometricTouchToFill)
             ? std::vector(
                   {PasswordManagerSetting::kOfferToSavePasswords,
                    PasswordManagerSetting::kAutoSignIn,
                    PasswordManagerSetting::kBiometricReauthBeforePwdFilling})
             : std::vector({PasswordManagerSetting::kOfferToSavePasswords,
                            PasswordManagerSetting::kAutoSignIn});
}

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
    case PasswordManagerSetting::kBiometricReauthBeforePwdFilling:
      return pref_service->FindPreference(
          password_manager::prefs::kBiometricAuthenticationBeforeFilling);
  }
}

// Returns the cross-platform preferences in which password manager settings
// are stored. These are not directly used on Android when the unified password
// manager is enabled.
// TODO(crbug.com/394299374): Find a different way to apply policies.
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
    // Never existed in Chrome on Android before.
    case PasswordManagerSetting::kBiometricReauthBeforePwdFilling:
      return pref_service->FindPreference(
          password_manager::prefs::kBiometricAuthenticationBeforeFilling);
  }
}

}  // namespace

PasswordManagerSettingsServiceAndroidImpl::
    PasswordManagerSettingsServiceAndroidImpl(PrefService* pref_service,
                                              syncer::SyncService* sync_service)
    : pref_service_(pref_service),
      sync_service_(sync_service),
      bridge_helper_(PasswordSettingsUpdaterAndroidBridgeHelper::Create()),
      lifecycle_helper_(
          std::make_unique<PasswordManagerLifecycleHelperImpl>()) {
  CHECK(pref_service_);
  Init();
}

// Constructor for tests
PasswordManagerSettingsServiceAndroidImpl::
    PasswordManagerSettingsServiceAndroidImpl(
        base::PassKey<class PasswordManagerSettingsServiceAndroidImplTest>,
        PrefService* pref_service,
        syncer::SyncService* sync_service,
        std::unique_ptr<PasswordSettingsUpdaterAndroidBridgeHelper>
            bridge_helper,
        std::unique_ptr<PasswordManagerLifecycleHelper> lifecycle_helper)
    : pref_service_(pref_service),
      sync_service_(sync_service),
      bridge_helper_(std::move(bridge_helper)),
      lifecycle_helper_(std::move(lifecycle_helper)) {
  CHECK(pref_service_);
  CHECK(bridge_helper_);
  Init();
}

PasswordManagerSettingsServiceAndroidImpl::
    ~PasswordManagerSettingsServiceAndroidImpl() {
  lifecycle_helper_->UnregisterObserver();
}

bool PasswordManagerSettingsServiceAndroidImpl::IsSettingEnabled(
    PasswordManagerSetting setting) const {
  const PrefService::Preference* regular_pref =
      GetRegularPrefFromSetting(pref_service_, setting);
  CHECK(regular_pref);

  if (regular_pref->IsManaged() || regular_pref->IsManagedByCustodian()) {
    return regular_pref->GetValue()->GetBool();
  }

  const PrefService::Preference* android_pref =
      GetGMSPrefFromSetting(pref_service_, setting);
  CHECK(android_pref);
  return android_pref->GetValue()->GetBool();
}

void PasswordManagerSettingsServiceAndroidImpl::RequestSettingsFromBackend() {
  std::optional<SyncingAccount> account = std::nullopt;
  if (is_password_sync_enabled_) {
    account = SyncingAccount(sync_service_->GetAccountInfo().email);
  }

  for (PasswordManagerSetting setting : GetAllPasswordSettings()) {
    bridge_helper_->GetPasswordSettingValue(account, setting);
  }
}

void PasswordManagerSettingsServiceAndroidImpl::TurnOffAutoSignIn() {
  pref_service_->SetBoolean(password_manager::prefs::kAutoSignInEnabledGMS,
                            false);
  std::optional<SyncingAccount> account = std::nullopt;
  if (is_password_sync_enabled_) {
    account = SyncingAccount(sync_service_->GetAccountInfo().email);
  }
  // TODO(crbug.com/40285405): Implement retries for writing to GMSCore.
  bridge_helper_->SetPasswordSettingValue(
      account, PasswordManagerSetting::kAutoSignIn, false);
}

void PasswordManagerSettingsServiceAndroidImpl::Shutdown() {
  if (sync_service_) {
    sync_service_->RemoveObserver(this);
    sync_service_ = nullptr;
  }
}

void PasswordManagerSettingsServiceAndroidImpl::Init() {
  bridge_helper_->SetConsumer(weak_ptr_factory_.GetWeakPtr());
  lifecycle_helper_->RegisterObserver(base::BindRepeating(
      &PasswordManagerSettingsServiceAndroidImpl::OnChromeForegrounded,
      weak_ptr_factory_.GetWeakPtr()));
  is_password_sync_enabled_ = false;
  if (sync_service_) {
    is_password_sync_enabled_ =
        password_manager::sync_util::HasChosenToSyncPasswords(sync_service_);
    // The `sync_service_` can be null when --disable-sync has been passed in as
    // a command line flag.
    sync_service_->AddObserver(this);
  }
}

void PasswordManagerSettingsServiceAndroidImpl::OnChromeForegrounded() {
  RequestSettingsFromBackend();
}

void PasswordManagerSettingsServiceAndroidImpl::OnSettingValueFetched(
    PasswordManagerSetting setting,
    bool value) {
  UpdateSettingsCache(setting, value);
}

void PasswordManagerSettingsServiceAndroidImpl::OnSettingValueAbsent(
    password_manager::PasswordManagerSetting setting) {
  UpdateSettingsCache(setting, std::nullopt);
}

void PasswordManagerSettingsServiceAndroidImpl::OnSettingFetchingError(
    password_manager::PasswordManagerSetting setting,
    AndroidBackendAPIErrorCode api_error_code) {
  // Not used. The metrics are recorded on the java side.
  // TODO(crbug.com/394547508): Remove from the `Consumer` interface once the
  // migration service is deprecated.
}

void PasswordManagerSettingsServiceAndroidImpl::OnSuccessfulSettingChange(
    password_manager::PasswordManagerSetting setting) {
  // Not used. The metrics are recorded on the java side.
  // TODO(crbug.com/394547508): Remove from the `Consumer` interface once the
  // migration service is deprecated.
}

void PasswordManagerSettingsServiceAndroidImpl::OnFailedSettingChange(
    password_manager::PasswordManagerSetting setting,
    AndroidBackendAPIErrorCode api_error_code) {
  // Not used. The metrics are recorded on the java side.
  // TODO(crbug.com/394547508): Remove from the `Consumer` interface once the
  // migration service is deprecated.
}

void PasswordManagerSettingsServiceAndroidImpl::UpdateSettingsCache(
    PasswordManagerSetting setting,
    std::optional<bool> value) {
  const PrefService::Preference* android_pref =
      GetGMSPrefFromSetting(pref_service_, setting);
  if (value.has_value()) {
    pref_service_->SetBoolean(android_pref->name(), value.value());
  } else {
    pref_service_->ClearPref(android_pref->name());
  }
}

void PasswordManagerSettingsServiceAndroidImpl::OnStateChanged(
    syncer::SyncService* sync) {
  CHECK(sync);

  bool is_password_sync_enabled =
      password_manager::sync_util::HasChosenToSyncPasswords(sync_service_);

  // Return early if the setting didn't change.
  if (is_password_sync_enabled == is_password_sync_enabled_) {
    return;
  }
  is_password_sync_enabled_ = is_password_sync_enabled;

  // Fetch settings from the backend to align values stored in GMS Core and
  // Chrome's cache.
  RequestSettingsFromBackend();
}

void PasswordManagerSettingsServiceAndroidImpl::OnSyncShutdown(
    syncer::SyncService* sync) {
  // Unreachable, since this service is Shutdown() before the SyncService.
  NOTREACHED();
}
