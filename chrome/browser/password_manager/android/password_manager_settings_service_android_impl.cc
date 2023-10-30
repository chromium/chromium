// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_manager_settings_service_android_impl.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/password_manager/android/password_manager_lifecycle_helper_impl.h"
#include "chrome/browser/password_manager/android/password_settings_updater_android_bridge_helper.h"
#include "components/password_manager/core/browser/password_manager_setting.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using password_manager::PasswordManagerSetting;
using password_manager::PasswordSettingsUpdaterAndroidBridgeHelper;
using password_manager::sync_util::IsSyncFeatureEnabledIncludingPasswords;
using password_manager_util::UsesUPMForLocalM2;

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
bool ShouldSuspendPasswordSavingDueToError(PrefService* pref_service,
                                           syncer::SyncService* sync_service) {
  // Ensure the user is still enrolled. Evicted users can still save normally.
  bool is_pwd_sync_enabled =
      IsSyncFeatureEnabledIncludingPasswords(sync_service);
  return is_pwd_sync_enabled && !IsUnenrolledFromUPM(pref_service) &&
         pref_service->GetBoolean(
             password_manager::prefs::kSavePasswordsSuspendedByError);
}

// Checks that the user is either syncing and enrolled in UPM or not syncing and
// ready to use local UPM.
bool UsesUPMBackend(PrefService* pref_service,
                    syncer::SyncService* sync_service) {
  // TODO(crbug.com/1494913): Include the bridge helper check here.
  // TODO(crbug.com/1466445): Migrate away from `ConsentLevel::kSync` on
  // Android.
  bool is_pwd_sync_enabled =
      IsSyncFeatureEnabledIncludingPasswords(sync_service);
  bool is_unenrolled = IsUnenrolledFromUPM(pref_service);
  if (is_pwd_sync_enabled && is_unenrolled) {
    return false;
  }
  if (is_pwd_sync_enabled) {
    return true;
  }
  return UsesUPMForLocalM2(pref_service);
}

}  // namespace

PasswordManagerSettingsServiceAndroidImpl::
    PasswordManagerSettingsServiceAndroidImpl(PrefService* pref_service,
                                              syncer::SyncService* sync_service)
    : pref_service_(pref_service), sync_service_(sync_service) {
  CHECK(pref_service_);
  if (!PasswordSettingsUpdaterAndroidBridgeHelper::CanCreateAccessor())
    return;
  bridge_helper_ = PasswordSettingsUpdaterAndroidBridgeHelper::Create();
  lifecycle_helper_ = std::make_unique<PasswordManagerLifecycleHelperImpl>();
  Init();
}

// Constructor for tests
PasswordManagerSettingsServiceAndroidImpl::
    PasswordManagerSettingsServiceAndroidImpl(
        base::PassKey<class PasswordManagerSettingsServiceAndroidImplBaseTest>,
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
    PasswordManagerSetting setting) const {
  if (setting == PasswordManagerSetting::kOfferToSavePasswords &&
      ShouldSuspendPasswordSavingDueToError(pref_service_, sync_service_)) {
    return false;
  }
  const PrefService::Preference* regular_pref =
      GetRegularPrefFromSetting(pref_service_, setting);
  CHECK(regular_pref);

  if (!bridge_helper_) {
    return regular_pref->GetValue()->GetBool();
  }

  if (!UsesUPMBackend(pref_service_, sync_service_)) {
    return regular_pref->GetValue()->GetBool();
  }

  if (regular_pref->IsManaged()) {
    return regular_pref->GetValue()->GetBool();
  }

  const PrefService::Preference* android_pref =
      GetGMSPrefFromSetting(pref_service_, setting);
  CHECK(android_pref);
  return android_pref->GetValue()->GetBool();
}

void PasswordManagerSettingsServiceAndroidImpl::RequestSettingsFromBackend() {
  if (!bridge_helper_) {
    return;
  }
  if (!UsesUPMBackend(pref_service_, sync_service_)) {
    return;
  }
  FetchSettings();
}

void PasswordManagerSettingsServiceAndroidImpl::TurnOffAutoSignIn() {
  // TODO(crbug.com/1466445): Migrate away from `ConsentLevel::kSync` on
  // Android.
  if (!bridge_helper_ || !UsesUPMBackend(pref_service_, sync_service_)) {
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
  absl::optional<SyncingAccount> account = absl::nullopt;
  // TODO(crbug.com/1466445): Migrate away from `ConsentLevel::kSync` on
  // Android.
  if (IsSyncFeatureEnabledIncludingPasswords(sync_service_)) {
    account = SyncingAccount(sync_service_->GetAccountInfo().email);
  }
  // TODO(crbug.com/1492135): Implement retries for writing to GMSCore.
  bridge_helper_->SetPasswordSettingValue(
      account, PasswordManagerSetting::kAutoSignIn, false);
}

void PasswordManagerSettingsServiceAndroidImpl::Init() {
  CHECK(bridge_helper_);
  // TODO(crbug.com/1485556): Copy the pref values to GMSCore for local users.
  bridge_helper_->SetConsumer(weak_ptr_factory_.GetWeakPtr());

  lifecycle_helper_->RegisterObserver(base::BindRepeating(
      &PasswordManagerSettingsServiceAndroidImpl::OnChromeForegrounded,
      weak_ptr_factory_.GetWeakPtr()));
  // TODO(crbug.com/1466445): Migrate away from `ConsentLevel::kSync` on
  // Android.
  is_password_sync_enabled_ =
      IsSyncFeatureEnabledIncludingPasswords(sync_service_);
  if (sync_service_) {
    // The `sync_service_` can be null when --disable-sync has been passed in as
    // a command line flag.
    sync_service_->AddObserver(this);
  }

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
  // For the users not using the UPM backend, the setting value should not be
  // written to the cache and the regular pref, unless this call to
  // `OnSettingValueFetched` was part of the final fetch after a sync state
  // change.
  if (!UsesUPMBackend(pref_service_, sync_service_) &&
      !fetch_after_sync_status_change_in_progress_) {
    return;
  }

  WriteToTheCacheAndRegularPref(setting, value);
}

void PasswordManagerSettingsServiceAndroidImpl::OnSettingValueAbsent(
    password_manager::PasswordManagerSetting setting) {
  CHECK(bridge_helper_);
  UpdateSettingFetchState(setting);

  if (!UsesUPMBackend(pref_service_, sync_service_)) {
    return;
  }

  // This code is currently called only for UPM users. If the setting value
  // is absent in GMSCore, the cached setting value is set to the default value,
  // which is true for both of the password-related settings: AutoSignIn and
  // OfferToSavePasswords.
  WriteToTheCacheAndRegularPref(setting, absl::nullopt);
}

void PasswordManagerSettingsServiceAndroidImpl::WriteToTheCacheAndRegularPref(
    PasswordManagerSetting setting,
    absl::optional<bool> value) {
  const PrefService::Preference* android_pref =
      GetGMSPrefFromSetting(pref_service_, setting);
  if (value.has_value()) {
    pref_service_->SetBoolean(android_pref->name(), value.value());
  } else {
    pref_service_->ClearPref(android_pref->name());
  }

  // Updating the regular pref now will ensure that if passwods sync turns off
  // the regular pref contains the latest setting value. This can only be done
  // when preference syncing is off, otherwise it might cause sync cycles.
  // When sync is on, the regular preference gets updated via sync, so this
  // step is not necessary.
  if (!HasChosenToSyncPreferences(sync_service_)) {
    const PrefService::Preference* regular_pref =
        GetRegularPrefFromSetting(pref_service_, setting);
    if (value.has_value()) {
      pref_service_->SetBoolean(regular_pref->name(), value.value());
    } else {
      pref_service_->ClearPref(regular_pref->name());
    }
  }
}

void PasswordManagerSettingsServiceAndroidImpl::OnStateChanged(
    syncer::SyncService* sync) {
  // Return early if the setting didn't change and no sync errors were resolved.
  // TODO(crbug.com/1466445): Migrate away from `ConsentLevel::kSync` on
  // Android.
  if (IsSyncFeatureEnabledIncludingPasswords(sync) ==
      is_password_sync_enabled_) {
    return;
  }

  // TODO(crbug.com/1466445): Migrate away from `ConsentLevel::kSync` on
  // Android.
  // TODO(crbug.com/1493631): Consider using is_password_sync_enabled_ where
  // possible, instead of calling IsSyncFeatureEnabledIncludingPasswords.
  is_password_sync_enabled_ = IsSyncFeatureEnabledIncludingPasswords(sync);

  if (is_password_sync_enabled_ && IsUnenrolledFromUPM(pref_service_)) {
    return;
  }

  // If sync just turned off, but the client was unenrolled prior to the change
  // and they are not using local storage support, it means that there is no
  // backend to talk to and Chrome will be reading the settings from the regular
  // prefs, so there is no point in making a request for new settings values.
  // Users not syncing passwords that have local storage support ignore
  // unenrollment and need to fetch new settings from the local backend to
  // replace the account ones.
  if (!is_password_sync_enabled_ && IsUnenrolledFromUPM(pref_service_) &&
      !UsesUPMForLocalM2(pref_service_)) {
    return;
  }

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
  CHECK(bridge_helper_);
  absl::optional<SyncingAccount> account = absl::nullopt;
  // TODO(crbug.com/1466445): Migrate away from `ConsentLevel::kSync` on
  // Android.
  bool is_syncing_passwords =
      IsSyncFeatureEnabledIncludingPasswords(sync_service_);
  CHECK(!(is_syncing_passwords && IsUnenrolledFromUPM(pref_service_)));
  bool is_final_fetch_for_local_user_without_upm =
      fetch_after_sync_status_change_in_progress_ && !is_syncing_passwords &&
      !UsesUPMForLocalM2(pref_service_);
  if (is_syncing_passwords || is_final_fetch_for_local_user_without_upm) {
    // Note: This method also handles the case where the previously-syncing
    // account has just signed out. So the account can't be queried via
    // `sync_service_->GetAccountInfo().email` but instead needs to be retrieved
    // via kGoogleServices*Last*SyncingUsername.
    // TODO(crbug.com/1490523): Revisit this logic - does anything need to be
    // done for signed-in non-syncing users too?
    account = SyncingAccount(
        pref_service_->GetString(::prefs::kGoogleServicesLastSyncingUsername));
  }
  for (PasswordManagerSetting setting : kAllPasswordSettings) {
    bridge_helper_->GetPasswordSettingValue(account, setting);
  }
}

void PasswordManagerSettingsServiceAndroidImpl::
    OnUnenrollmentPreferenceChanged() {
  if (!IsUnenrolledFromUPM(pref_service_)) {
    // Perform actions that are usually done on startup, but were skipped
    // for the evicted users.
    RequestSettingsFromBackend();
  }
}
