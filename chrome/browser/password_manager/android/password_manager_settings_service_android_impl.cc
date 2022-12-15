// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_manager_settings_service_android_impl.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/password_manager/android/password_manager_lifecycle_helper_impl.h"
#include "chrome/browser/password_manager/android/password_settings_updater_android_bridge_helper.h"
#include "components/password_manager/core/browser/password_manager_setting.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using password_manager::PasswordManagerSetting;
using password_manager::PasswordSettingsUpdaterAndroidBridgeHelper;
using password_manager::sync_util::IsPasswordSyncEnabled;

namespace {

using Consumer =
    password_manager::PasswordSettingsUpdaterAndroidReceiverBridge::Consumer;
using SyncingAccount = password_manager::
    PasswordSettingsUpdaterAndroidReceiverBridge::SyncingAccount;

constexpr PasswordManagerSetting kAllPasswordSettings[] = {
    PasswordManagerSetting::kOfferToSavePasswords,
    PasswordManagerSetting::kAutoSignIn};

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

bool HasChosenToSyncPreferences(const syncer::SyncService* sync_service) {
  return sync_service && sync_service->IsSyncFeatureEnabled() &&
         sync_service->GetUserSettings()->GetSelectedTypes().Has(
             syncer::UserSelectableType::kPreferences);
}

bool IsUnenrolledFromUPM(PrefService* pref_service) {
  return pref_service->GetBoolean(
      password_manager::prefs::kUnenrolledFromGoogleMobileServicesDueToErrors);
}

// In error cases, the UPM can set the kSavePasswordsSuspendedByError
// pref to temporarily prevent password saves. If the user doesn't use GMS,
// saving keeps working and only the syncing of changes is delayed.
bool ShouldSuspendPasswordSavingDueToError(PrefService* pref_service) {
  // The messages feature is a subset of UPM. The actual feature is irrelevant.
  if (!base::FeatureList::IsEnabled(
          password_manager::features::kUnifiedPasswordManagerErrorMessages)) {
    return false;  // Non-UPM users can still save.
  }
  // Ensure the user is still enrolled. Evicted users can still save normally.
  return !IsUnenrolledFromUPM(pref_service) &&
         pref_service->GetBoolean(
             password_manager::prefs::kSavePasswordsSuspendedByError);
}

}  // namespace

PasswordManagerSettingsServiceAndroidImpl::
    PasswordManagerSettingsServiceAndroidImpl(PrefService* pref_service,
                                              syncer::SyncService* sync_service)
    : pref_service_(pref_service), sync_service_(sync_service) {
  DCHECK(pref_service_);
  DCHECK(sync_service_);
  DCHECK(password_manager::features::UsesUnifiedPasswordManagerUi());
  if (!PasswordSettingsUpdaterAndroidBridgeHelper::CanCreateAccessor())
    return;
  bridge_helper_ = PasswordSettingsUpdaterAndroidBridgeHelper::Create();
  lifecycle_helper_ = std::make_unique<PasswordManagerLifecycleHelperImpl>();
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
  DCHECK(pref_service_);
  DCHECK(sync_service_);
  if (!bridge_helper_)
    return;
  Init();
}

PasswordManagerSettingsServiceAndroidImpl::
    ~PasswordManagerSettingsServiceAndroidImpl() {
  if (lifecycle_helper_) {
    lifecycle_helper_->UnregisterObserver();
  }
}

bool PasswordManagerSettingsServiceAndroidImpl::IsSettingEnabled(
    PasswordManagerSetting setting) {
  if (setting == PasswordManagerSetting::kOfferToSavePasswords &&
      ShouldSuspendPasswordSavingDueToError(pref_service_)) {
    return false;
  }
  const PrefService::Preference* regular_pref =
      GetRegularPrefFromSetting(pref_service_, setting);
  DCHECK(regular_pref);

  if (IsUnenrolledFromUPM(pref_service_)) {
    return regular_pref->GetValue()->GetBool();
  }

  if (!IsPasswordSyncEnabled(sync_service_)) {
    return regular_pref->GetValue()->GetBool();
  }

  if (!bridge_helper_) {
    return regular_pref->GetValue()->GetBool();
  }

  if (regular_pref->IsManaged()) {
    return regular_pref->GetValue()->GetBool();
  }

  const PrefService::Preference* android_pref =
      GetGMSPrefFromSetting(pref_service_, setting);
  DCHECK(android_pref);
  return android_pref->GetValue()->GetBool();
}

void PasswordManagerSettingsServiceAndroidImpl::RequestSettingsFromBackend() {
  // Backend has settings data only if passwords are synced.
  if (bridge_helper_ && IsPasswordSyncEnabled(sync_service_) &&
      !IsUnenrolledFromUPM(pref_service_)) {
    FetchSettings();
  }
}

void PasswordManagerSettingsServiceAndroidImpl::TurnOffAutoSignIn() {
  if (!bridge_helper_ || !IsPasswordSyncEnabled(sync_service_) ||
      IsUnenrolledFromUPM(pref_service_)) {
    pref_service_->SetBoolean(
        password_manager::prefs::kCredentialsEnableAutosignin, false);
    return;
  }
  if (!HasChosenToSyncPreferences(sync_service_)) {
    pref_service_->SetBoolean(
        password_manager::prefs::kCredentialsEnableAutosignin, false);
  }

  pref_service_->SetBoolean(password_manager::prefs::kAutoSignInEnabledGMS,
                            false);
  bridge_helper_->SetPasswordSettingValue(
      SyncingAccount(sync_service_->GetAccountInfo().email),
      PasswordManagerSetting::kAutoSignIn, false);
}

void PasswordManagerSettingsServiceAndroidImpl::Init() {
  DCHECK(bridge_helper_);
  MigratePrefsIfNeeded();
  bridge_helper_->SetConsumer(weak_ptr_factory_.GetWeakPtr());

  lifecycle_helper_->RegisterObserver(base::BindRepeating(
      &PasswordManagerSettingsServiceAndroidImpl::OnChromeForegrounded,
      weak_ptr_factory_.GetWeakPtr()));
  is_password_sync_enabled_ = IsPasswordSyncEnabled(sync_service_);
  sync_service_->AddObserver(this);

  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      password_manager::prefs::kUnenrolledFromGoogleMobileServicesDueToErrors,
      base::BindRepeating(&PasswordManagerSettingsServiceAndroidImpl::
                              OnUnenrollmentPreferenceChanged,
                          weak_ptr_factory_.GetWeakPtr()));

  RequestSettingsFromBackend();
}

void PasswordManagerSettingsServiceAndroidImpl::OnChromeForegrounded() {
  RequestSettingsFromBackend();
}

void PasswordManagerSettingsServiceAndroidImpl::OnSettingValueFetched(
    PasswordManagerSetting setting,
    bool value) {
  UpdateSettingFetchState(setting);
  if (!fetch_after_sync_status_change_in_progress_ &&
      !IsPasswordSyncEnabled(sync_service_)) {
    return;
  }

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
  DCHECK(bridge_helper_);
  UpdateSettingFetchState(setting);
  if (IsUnenrolledFromUPM(pref_service_))
    return;

  if (!IsPasswordSyncEnabled(sync_service_))
    return;

  const PrefService::Preference* pref =
      GetGMSPrefFromSetting(pref_service_, setting);

  // If both GMS and Chrome have default values for the setting, then no update
  // is needed.
  if (!pref_service_->GetUserPrefValue(pref->name()))
    return;

  // If Chrome has an explicitly set value, GMS needs to know about it.
  // TODO(crbug.com/1289700): Check whether this should be guarded by a
  // migration pref.
  bridge_helper_->SetPasswordSettingValue(
      SyncingAccount(sync_service_->GetAccountInfo().email), setting,
      pref->GetValue()->GetBool());
}

void PasswordManagerSettingsServiceAndroidImpl::MigratePrefsIfNeeded() {
  if (IsUnenrolledFromUPM(pref_service_))
    return;

  if (pref_service_->GetBoolean(
          password_manager::prefs::kSettingsMigratedToUPM))
    return;

  base::UmaHistogramBoolean("PasswordManager.MigratedSettingsUPMAndroid", true);
  pref_service_->SetBoolean(password_manager::prefs::kSettingsMigratedToUPM,
                            true);
  // No need to copy the values until sync turns on. When sync turns on, this
  // will be handled as part of the sync state change rather than migration.
  if (!IsPasswordSyncEnabled(sync_service_))
    return;

  DumpChromePrefsIntoGMSPrefs();
}

void PasswordManagerSettingsServiceAndroidImpl::OnStateChanged(
    syncer::SyncService* sync) {
  if (IsUnenrolledFromUPM(pref_service_))
    return;

  // Return early if the setting didn't change and no sync errors were resolved.
  if (IsPasswordSyncEnabled(sync) == is_password_sync_enabled_)
    return;

  is_password_sync_enabled_ = IsPasswordSyncEnabled(sync);

  if (is_password_sync_enabled_)
    DumpChromePrefsIntoGMSPrefs();

  // Fetch settings from the backend to align values stored in GMS Core and
  // Chrome.
  fetch_after_sync_status_change_in_progress_ = true;
  for (PasswordManagerSetting setting : kAllPasswordSettings)
    awaited_settings_.insert(setting);
  FetchSettings();
}

void PasswordManagerSettingsServiceAndroidImpl::UpdateSettingFetchState(
    PasswordManagerSetting received_setting) {
  if (!fetch_after_sync_status_change_in_progress_)
    return;

  awaited_settings_.erase(received_setting);
  if (awaited_settings_.empty())
    fetch_after_sync_status_change_in_progress_ = false;
}

void PasswordManagerSettingsServiceAndroidImpl::FetchSettings() {
  DCHECK(bridge_helper_);
  for (PasswordManagerSetting setting : kAllPasswordSettings) {
    bridge_helper_->GetPasswordSettingValue(
        SyncingAccount(
            pref_service_->GetString(::prefs::kGoogleServicesLastUsername)),
        setting);
  }
}

void PasswordManagerSettingsServiceAndroidImpl::DumpChromePrefsIntoGMSPrefs() {
  for (PasswordManagerSetting setting : kAllPasswordSettings) {
    const PrefService::Preference* regular_pref =
        GetRegularPrefFromSetting(pref_service_, setting);

    if (!pref_service_->GetUserPrefValue(regular_pref->name()))
      continue;

    const PrefService::Preference* gms_pref =
        GetGMSPrefFromSetting(pref_service_, setting);

    // Make sure the user prefs are consistent. If the settings are set by
    // policy, the value of the managed pref will still apply in checks, but
    // the UPM prefs should contain the user set value.
    pref_service_->SetBoolean(
        gms_pref->name(),
        pref_service_->GetUserPrefValue(regular_pref->name())->GetBool());
  }
}

void PasswordManagerSettingsServiceAndroidImpl::
    OnUnenrollmentPreferenceChanged() {
  if (!IsUnenrolledFromUPM(pref_service_)) {
    // Perform actions that are usually done on startup, but were skipped
    // for the evicted users.
    MigratePrefsIfNeeded();
    RequestSettingsFromBackend();
  }
}
