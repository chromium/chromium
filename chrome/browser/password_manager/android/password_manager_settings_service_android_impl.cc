// Copyright 2022 The Chromium Authors
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
#include "chrome/browser/password_manager/android/password_manager_android_util.h"
#include "chrome/browser/password_manager/android/password_manager_eviction_util.h"
#include "chrome/browser/password_manager/android/password_manager_lifecycle_helper_impl.h"
#include "chrome/browser/password_manager/android/password_settings_updater_android_bridge_helper.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_manager_setting.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/password_manager/core/browser/split_stores_and_local_upm.h"
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
using password_manager::UsesSplitStoresAndUPMForLocal;
using password_manager_upm_eviction::IsCurrentUserEvicted;

namespace {

using Consumer =
    password_manager::PasswordSettingsUpdaterAndroidReceiverBridge::Consumer;
using SyncingAccount = password_manager::
    PasswordSettingsUpdaterAndroidReceiverBridge::SyncingAccount;
using password_manager::prefs::UseUpmLocalAndSeparateStoresState;

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

constexpr PasswordManagerSetting kMigratablePasswordSettings[] = {
    PasswordManagerSetting::kOfferToSavePasswords,
    PasswordManagerSetting::kAutoSignIn,
};

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

bool HasChosenToSyncPreferences(const syncer::SyncService* sync_service) {
  return sync_service && sync_service->GetDisableReasons().empty() &&
         sync_service->GetUserSettings()->GetSelectedTypes().Has(
             syncer::UserSelectableType::kPreferences);
}

bool DoesUpmPrefAllowForSettingsMigration(PrefService* pref_service) {
  return static_cast<UseUpmLocalAndSeparateStoresState>(
             pref_service->GetInteger(
                 password_manager::prefs::
                     kPasswordsUseUPMLocalAndSeparateStores)) !=
         UseUpmLocalAndSeparateStoresState::kOff;
}

bool ShouldMigrateLocalSettings(PrefService* pref_service,
                                bool is_password_sync_enabled) {
  // Settings should be migrated if the user is enrolled in UPM with local
  // passwords and they have never successfully completed settings migration.
  return !is_password_sync_enabled &&
         !pref_service->GetBoolean(
             password_manager::prefs::kSettingsMigratedToUPMLocal) &&
         DoesUpmPrefAllowForSettingsMigration(pref_service);
}

// This function is called after a setting is fetched from
// GMS Core to update the cache. Updating the non-cache (regular)
// prefs is also done in cases where sync isn't running so that the value
// is up-to-data in case of rollback.
bool ShouldWriteToRegularPref(syncer::SyncService* sync_service,
                              PrefService* pref_service) {
  // We should write to regular pref if sync for preferences is disabled and:
  // 1) User is using upm with local passwords and their settings migration
  // finished.
  // 2) User is not using upm with local passwords.
  bool is_using_upm_with_local_passwords_and_had_settings_migrated =
      DoesUpmPrefAllowForSettingsMigration(pref_service) &&
      pref_service->GetBoolean(
          password_manager::prefs::kSettingsMigratedToUPMLocal);
  bool is_not_using_upm_with_local_passwords =
      !DoesUpmPrefAllowForSettingsMigration(pref_service);
  return !HasChosenToSyncPreferences(sync_service) &&
         (is_using_upm_with_local_passwords_and_had_settings_migrated ||
          is_not_using_upm_with_local_passwords);
}

bool DidAccessingGMSPrefsFailed(
    const std::vector<PasswordManagerSettingGmsAccessResult>& results) {
  return !results[0].was_successful || !results[1].was_successful ||
         results[0].setting == results[1].setting;
}

std::string_view GetMetricsInfixForSetting(
    password_manager::PasswordManagerSetting setting) {
  switch (setting) {
    case password_manager::PasswordManagerSetting::kOfferToSavePasswords:
      return "OfferToSavePasswords";
    case password_manager::PasswordManagerSetting::kAutoSignIn:
      return "AutoSignIn";
    case password_manager::PasswordManagerSetting::
        kBiometricReauthBeforePwdFilling:
      return "BiometricReauthBeforePwdFilling";
  }
}

void RecordFailedMigrationMetric(std::string_view infix_for_setting,
                                 AndroidBackendAPIErrorCode api_error) {
  base::UmaHistogramSparse(
      base::StrCat({"PasswordManager.PasswordSettingsMigrationFailed.",
                    infix_for_setting, ".APIError2"}),
      static_cast<int>(api_error));
}

void RecordMigrationResult(bool result) {
  base::UmaHistogramBoolean(
      "PasswordManager.PasswordSettingsMigrationSucceeded2", result);
}

void MarkSettingsMigrationAsSuccessfulIfNothingToMigrate(PrefService* prefs) {
  if (GetRegularPrefFromSetting(prefs, PasswordManagerSetting::kAutoSignIn)
          ->IsDefaultValue() &&
      GetRegularPrefFromSetting(prefs,
                                PasswordManagerSetting::kOfferToSavePasswords)
          ->IsDefaultValue()) {
    RecordMigrationResult(true);
    prefs->SetBoolean(password_manager::prefs::kSettingsMigratedToUPMLocal,
                      true);
  }
}

}  // namespace

PasswordManagerSettingsServiceAndroidImpl::
    PasswordManagerSettingsServiceAndroidImpl(PrefService* pref_service,
                                              syncer::SyncService* sync_service)
    : pref_service_(pref_service), sync_service_(sync_service) {
  CHECK(pref_service_);
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
  CHECK(bridge_helper_);
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
  const PrefService::Preference* regular_pref =
      GetRegularPrefFromSetting(pref_service_, setting);
  CHECK(regular_pref);

  if (!UsesUPMBackend()) {
    return regular_pref->GetValue()->GetBool();
  }

  if (regular_pref->IsManaged() || regular_pref->IsManagedByCustodian()) {
    return regular_pref->GetValue()->GetBool();
  }

  // Until the settings migration finished successfully, Chrome's setting value
  // will be returned.
  if (!is_password_sync_enabled_ &&
      !pref_service_->GetBoolean(
          password_manager::prefs::kSettingsMigratedToUPMLocal)) {
    return regular_pref->GetValue()->GetBool();
  }

  const PrefService::Preference* android_pref =
      GetGMSPrefFromSetting(pref_service_, setting);
  CHECK(android_pref);
  return android_pref->GetValue()->GetBool();
}

void PasswordManagerSettingsServiceAndroidImpl::RequestSettingsFromBackend() {
  if (!UsesUPMBackend()) {
    return;
  }
  FetchSettings();
}

void PasswordManagerSettingsServiceAndroidImpl::TurnOffAutoSignIn() {
  if (!UsesUPMBackend()) {
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
  std::optional<SyncingAccount> account = std::nullopt;
  if (is_password_sync_enabled_) {
    account = SyncingAccount(sync_service_->GetAccountInfo().email);
  }
  // TODO(crbug.com/40285405): Implement retries for writing to GMSCore.
  bridge_helper_->SetPasswordSettingValue(
      account, PasswordManagerSetting::kAutoSignIn, false);
}

void PasswordManagerSettingsServiceAndroidImpl::Init() {
  CHECK(bridge_helper_);
  // TODO(crbug.com/40282601): Copy the pref values to GMSCore for local users.
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

  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      password_manager::prefs::kUnenrolledFromGoogleMobileServicesDueToErrors,
      base::BindRepeating(&PasswordManagerSettingsServiceAndroidImpl::
                              OnUnenrollmentPreferenceChanged,
                          weak_ptr_factory_.GetWeakPtr()));

  if (ShouldMigrateLocalSettings(pref_service_, is_password_sync_enabled_)) {
    MarkSettingsMigrationAsSuccessfulIfNothingToMigrate(pref_service_);
    // If the migration was marked as done because there was nothing to migrate,
    // there is no reason to create the migration callback.
    if (!pref_service_->GetBoolean(
            password_manager::prefs::kSettingsMigratedToUPMLocal)) {
      start_migration_callback_ = base::BarrierCallback<
          PasswordManagerSettingGmsAccessResult>(
          2,
          base::BindOnce(
              &PasswordManagerSettingsServiceAndroidImpl::MigratePrefsIfNeeded,
              weak_ptr_factory_.GetWeakPtr()));
    }
  }

  // Unset the pref that marks the settings migration done, if the user is not
  // eligible for split stores and UPM for local. This is useful in case of
  // rollback and it also fixes the issue of the pref being set to true for
  // not-yet-enrolled users that had default prefs.
  if (password_manager_android_util::GetSplitStoresAndLocalUpmPrefValue(
          pref_service_) == UseUpmLocalAndSeparateStoresState::kOff) {
    pref_service_->SetBoolean(
        password_manager::prefs::kSettingsMigratedToUPMLocal, false);
  }
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
  if (!UsesUPMBackend() && !fetch_after_sync_status_change_in_progress_) {
    return;
  }

  WriteToTheCacheAndRegularPref(setting, value);
  if (start_migration_callback_) {
    start_migration_callback_.Run(PasswordManagerSettingGmsAccessResult(
        setting, /*was_successful=*/true));
  }
}

void PasswordManagerSettingsServiceAndroidImpl::OnSettingValueAbsent(
    password_manager::PasswordManagerSetting setting) {
  CHECK(bridge_helper_);
  UpdateSettingFetchState(setting);

  if (!UsesUPMBackend()) {
    return;
  }

  // This code is currently called only for UPM users. If the setting value
  // is absent in GMSCore, the cached setting value is set to the default
  // value, which is true for both of the password-related settings:
  // AutoSignIn and OfferToSavePasswords.
  WriteToTheCacheAndRegularPref(setting, std::nullopt);
  if (start_migration_callback_) {
    start_migration_callback_.Run(PasswordManagerSettingGmsAccessResult(
        setting, /*was_successful=*/true));
  }
}

void PasswordManagerSettingsServiceAndroidImpl::OnSettingFetchingError(
    password_manager::PasswordManagerSetting setting,
    AndroidBackendAPIErrorCode api_error_code) {
  CHECK(bridge_helper_);
  if (!UsesUPMBackend()) {
    return;
  }
  if (start_migration_callback_) {
    RecordFailedMigrationMetric(GetMetricsInfixForSetting(setting),
                                api_error_code);
    start_migration_callback_.Run(PasswordManagerSettingGmsAccessResult(
        setting, /*was_successful=*/false));
  }
}

void PasswordManagerSettingsServiceAndroidImpl::OnSuccessfulSettingChange(
    password_manager::PasswordManagerSetting setting) {
  CHECK(bridge_helper_);
  if (!UsesUPMBackend()) {
    return;
  }
  if (migration_finished_callback_) {
    migration_finished_callback_.Run(PasswordManagerSettingGmsAccessResult(
        setting, /*was_successful=*/true));
  }
}
void PasswordManagerSettingsServiceAndroidImpl::OnFailedSettingChange(
    password_manager::PasswordManagerSetting setting,
    AndroidBackendAPIErrorCode api_error_code) {
  CHECK(bridge_helper_);
  if (!UsesUPMBackend()) {
    return;
  }
  if (migration_finished_callback_) {
    RecordFailedMigrationMetric(GetMetricsInfixForSetting(setting),
                                api_error_code);
    migration_finished_callback_.Run(PasswordManagerSettingGmsAccessResult(
        setting, /*was_successful=*/false));
  }
}

void PasswordManagerSettingsServiceAndroidImpl::WriteToTheCacheAndRegularPref(
    PasswordManagerSetting setting,
    std::optional<bool> value) {
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
  if (ShouldWriteToRegularPref(sync_service_, pref_service_)) {
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
  CHECK(sync);
  // Return early if the setting didn't change and no sync errors were resolved.
  bool is_password_sync_enabled =
      password_manager::sync_util::HasChosenToSyncPasswords(sync_service_);
  if (is_password_sync_enabled == is_password_sync_enabled_) {
    return;
  }

  is_password_sync_enabled_ = is_password_sync_enabled;

  if (is_password_sync_enabled_ && IsCurrentUserEvicted(pref_service_)) {
    return;
  }

  // If sync just turned off, but the client was unenrolled prior to the change
  // and they are not using local storage support, it means that there is no
  // backend to talk to and Chrome will be reading the settings from the regular
  // prefs, so there is no point in making a request for new settings values.
  // Users not syncing passwords that have local storage support ignore
  // unenrollment and need to fetch new settings from the local backend to
  // replace the account ones.
  if (!is_password_sync_enabled_ && IsCurrentUserEvicted(pref_service_) &&
      !UsesSplitStoresAndUPMForLocal(pref_service_)) {
    return;
  }

  // Fetch settings from the backend to align values stored in GMS Core and
  // Chrome.
  fetch_after_sync_status_change_in_progress_ = true;
  for (PasswordManagerSetting setting : GetAllPasswordSettings()) {
    awaited_settings_.insert(setting);
  }
  FetchSettings();
}

void PasswordManagerSettingsServiceAndroidImpl::UpdateSettingFetchState(
    PasswordManagerSetting received_setting) {
  if (!fetch_after_sync_status_change_in_progress_) {
    return;
  }

  awaited_settings_.erase(received_setting);
  if (awaited_settings_.empty()) {
    fetch_after_sync_status_change_in_progress_ = false;
  }
}

void PasswordManagerSettingsServiceAndroidImpl::FetchSettings() {
  CHECK(bridge_helper_);
  // This code would not be executed for syncing users who are unenrolled.
  CHECK(!is_password_sync_enabled_ || !IsCurrentUserEvicted(pref_service_));
  std::optional<SyncingAccount> account = std::nullopt;
  bool is_final_fetch_for_local_user_without_upm =
      fetch_after_sync_status_change_in_progress_ &&
      !is_password_sync_enabled_ &&
      !UsesSplitStoresAndUPMForLocal(pref_service_);
  if (is_password_sync_enabled_ || is_final_fetch_for_local_user_without_upm) {
    // Note: This method also handles the case where the previously signed-in
    // account has just signed out. So the account can't be queried via
    // `sync_service_->GetAccountInfo().email` but instead needs to be retrieved
    // via kGoogleServices*Last*SignedInUsername.
    std::string last_account_pref = pref_service_->GetString(
        base::FeatureList::IsEnabled(
            syncer::kEnablePasswordsAccountStorageForNonSyncingUsers)
            ? prefs::kGoogleServicesLastSignedInUsername
            : prefs::kGoogleServicesLastSyncingUsername);
    account = SyncingAccount(last_account_pref);
  }
  for (PasswordManagerSetting setting : GetAllPasswordSettings()) {
    bridge_helper_->GetPasswordSettingValue(account, setting);
  }
}

void PasswordManagerSettingsServiceAndroidImpl::
    OnUnenrollmentPreferenceChanged() {
  if (!IsCurrentUserEvicted(pref_service_)) {
    // Perform actions that are usually done on startup, but were skipped
    // for the evicted users.
    RequestSettingsFromBackend();
  }
}

bool PasswordManagerSettingsServiceAndroidImpl::UsesUPMBackend() const {
  return password_manager_android_util::ShouldUseUpmWiring(sync_service_,
                                                           pref_service_);
}

void PasswordManagerSettingsServiceAndroidImpl::MigratePrefsIfNeeded(
    const std::vector<PasswordManagerSettingGmsAccessResult>& results) {
  start_migration_callback_.Reset();
  // Check if migration should happen.
  if (!ShouldMigrateLocalSettings(pref_service_, is_password_sync_enabled_)) {
    return;
  }

  // Check if getting settings prefs failed. In rare cases (when Chrome was put
  // into the background with running migration and then to the foreground
  // again), since the settings from GMS are fetched when foregrounding Chrome,
  // it might happen that two fetches for the same pref will finish first. We
  // want to ensure that each one of the settings was fetched successfully.
  if (DidAccessingGMSPrefsFailed(results)) {
    RecordMigrationResult(false);
    return;
  }

  migration_finished_callback_ = base::BarrierCallback<
      PasswordManagerSettingGmsAccessResult>(
      2,
      base::BindOnce(
          &PasswordManagerSettingsServiceAndroidImpl::FinishSettingsMigration,
          weak_ptr_factory_.GetWeakPtr()));

  for (auto setting : kMigratablePasswordSettings) {
    const PrefService::Preference* regular_pref =
        GetRegularPrefFromSetting(pref_service_, setting);
    const PrefService::Preference* android_pref =
        GetGMSPrefFromSetting(pref_service_, setting);

    if (regular_pref->IsDefaultValue()) {
      // If Chrome had default value then value from gms is saved to Chrome.
      pref_service_->SetBoolean(regular_pref->name(),
                                android_pref->GetValue()->GetBool());
      // Migraion will finish only if this callback is called twice, but in this
      // case we didn't call gms, so we can mark this setting manually as
      // successfully migrated.
      migration_finished_callback_.Run(PasswordManagerSettingGmsAccessResult(
          setting, /*was_successful=*/true));
      continue;
    }

    if (android_pref->GetValue()->GetBool() ==
        pref_service_->GetDefaultPrefValue(android_pref->name())->GetBool()) {
      // If Chrome had user set pref value and gms had default value, then
      // Chrome value is saved in gms.
      bridge_helper_->SetPasswordSettingValue(
          std::nullopt, setting, regular_pref->GetValue()->GetBool());
      pref_service_->SetBoolean(android_pref->name(),
                                regular_pref->GetValue()->GetBool());
      continue;
    }

    // If Chrome had user set pref value and gms had non default value, then
    // the most conservative value is saved. Meaning that if either one of
    // them, had this setting disabled, it should be disabled after the
    // migration.
    bool conservative_value = regular_pref->GetValue()->GetBool() &&
                              android_pref->GetValue()->GetBool();
    bridge_helper_->SetPasswordSettingValue(std::nullopt, setting,
                                            conservative_value);
    pref_service_->SetBoolean(android_pref->name(), conservative_value);
    pref_service_->SetBoolean(regular_pref->name(), conservative_value);
  }
}

void PasswordManagerSettingsServiceAndroidImpl::FinishSettingsMigration(
    const std::vector<PasswordManagerSettingGmsAccessResult>& results) {
  migration_finished_callback_.Reset();
  // Check if setting settings prefs failed.
  if (DidAccessingGMSPrefsFailed(results)) {
    RecordMigrationResult(false);
    return;
  }

  RecordMigrationResult(true);
  pref_service_->SetBoolean(
      password_manager::prefs::kSettingsMigratedToUPMLocal, true);
}
