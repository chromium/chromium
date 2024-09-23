// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_manager_android_util.h"

#include <string>

#include "base/android/build_info.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/password_manager/android/password_manager_eviction_util.h"
#include "chrome/browser/password_manager/android/password_manager_util_bridge.h"
#include "components/browser_sync/sync_to_signin_migration.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_manager_buildflags.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/password_manager/core/browser/split_stores_and_local_upm.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/pref_names.h"
#include "components/version_info/android/channel_getter.h"

using password_manager::prefs::kCurrentMigrationVersionToGoogleMobileServices;
using password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores;
using password_manager::prefs::UseUpmLocalAndSeparateStoresState;
using password_manager::prefs::UseUpmLocalAndSeparateStoresState::kOff;
using password_manager::prefs::UseUpmLocalAndSeparateStoresState::
    kOffAndMigrationPending;
using password_manager::prefs::UseUpmLocalAndSeparateStoresState::kOn;

namespace password_manager_android_util {

namespace {

enum class UserType {
  kSyncing,
  kNonSyncingAndMigrationNeeded,
  kNonSyncingAndNoMigrationNeeded,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Keep in sync with the corresponding
// enum in tools/metrics/histograms/metadata/password/enums.xml.
enum class ActivationError {
  kNone = 0,
  // (Deprecated) kUnenrolled = 1,
  // (Deprecated) kInitialUpmMigrationMissing = 2,
  kLoginDbFileMoveFailed = 3,
  kOutdatedGmsCore = 4,
  // (Deprecated) kFlagDisabled = 5,
  kMigrationWarningUnacknowledged = 6,
  kMaxValue = kMigrationWarningUnacknowledged,
};

// Set on startup before the local passwords migration starts.
bool last_migration_attempt_failed = false;

bool IsPasswordSyncEnabled(PrefService* pref_service) {
  // It's not possible to ask the SyncService whether password sync is enabled,
  // the object wasn't created yet. Instead, that information is written to a
  // pref during the previous execution and read now.
  switch (browser_sync::GetSyncToSigninMigrationDataTypeDecision(
      pref_service, syncer::PASSWORDS,
      syncer::prefs::internal::kSyncPasswords)) {
    // `kDontMigrateTypeNotActive` is handled same as if the data type was
    // active, because all that matters is the user's choice to sync the type.
    case browser_sync::SyncToSigninMigrationDataTypeDecision::
        kDontMigrateTypeNotActive:
    case browser_sync::SyncToSigninMigrationDataTypeDecision::kMigrate:
      return true;
    case browser_sync::SyncToSigninMigrationDataTypeDecision::
        kDontMigrateTypeDisabled:
      return false;
  }
}

bool HasMinGmsVersion() {
  std::string gms_version_str =
      base::android::BuildInfo::GetInstance()->gms_version_code();
  int gms_version = 0;
  // gms_version_code() must be converted to int for comparison, because it can
  // have legacy values "3(...)" and those evaluate > "2023(...)".
  return base::StringToInt(gms_version_str, &gms_version) &&
         gms_version >= password_manager::GetLocalUpmMinGmsVersion();
}

bool ShouldDelayMigrationUntillMigrationWarningIsAcknowledged(
    PrefService* pref_service) {
  // The migration warning is only relevant for non-stable channels.
  version_info::Channel channel = version_info::android::GetChannel();
  if (channel == version_info::Channel::STABLE) {
    return false;
  }
  // If there are no passwords to migrate and migration is still needed for
  // settings, there is no need to acknowledge the password migration warning.
  if (pref_service->GetBoolean(
          password_manager::prefs::kEmptyProfileStoreLoginDatabase)) {
    return false;
  }

  // There is no warning shown on automotive.
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    return false;
  }

  if (!base::FeatureList::IsEnabled(
          password_manager::features::
              kUnifiedPasswordManagerLocalPasswordsMigrationWarning)) {
    return false;
  }
  return !pref_service->GetBoolean(
      password_manager::prefs::kUserAcknowledgedLocalPasswordsMigrationWarning);
}

bool HasCustomPasswordSettings(PrefService* pref_service) {
  bool has_custom_enable_service_setting =
      !pref_service
           ->FindPreference(password_manager::prefs::kCredentialsEnableService)
           ->IsDefaultValue();
  bool has_custom_auto_signin_setting =
      !pref_service
           ->FindPreference(
               password_manager::prefs::kCredentialsEnableAutosignin)
           ->IsDefaultValue();
  return has_custom_enable_service_setting || has_custom_auto_signin_setting;
}

bool MustMigrateLocalPasswordsOrSettingsOnActivation(
    PrefService* pref_service,
    const base::FilePath& login_db_directory) {
  CHECK(!IsPasswordSyncEnabled(pref_service));

  // It's not possible to ask the (profile) PasswordStore whether it is empty,
  // the object wasn't created yet. Instead, that information is written to the
  // kEmptyProfileStoreLoginDatabase pref during the previous execution and read
  // now. The pref is false by default, so a migration is required in doubt.
  bool has_passwords_in_profile_login_db =
      !pref_service->GetBoolean(
          password_manager::prefs::kEmptyProfileStoreLoginDatabase) &&
      base::PathExists(login_db_directory.Append(
          password_manager::kLoginDataForProfileFileName));
  return HasCustomPasswordSettings(pref_service) ||
         has_passwords_in_profile_login_db;
}

UserType GetUserType(PrefService* pref_service,
                     const base::FilePath& login_db_directory) {
  if (IsPasswordSyncEnabled(pref_service)) {
    return UserType::kSyncing;
  }

  return MustMigrateLocalPasswordsOrSettingsOnActivation(pref_service,
                                                         login_db_directory)
             ? UserType::kNonSyncingAndMigrationNeeded
             : UserType::kNonSyncingAndNoMigrationNeeded;
}

void RecordActivationError(UserType user_type, ActivationError error) {
  const char kHistogramPrefix[] = "PasswordManager.LocalUpmActivationError";
  const char* suffix = nullptr;
  switch (user_type) {
    case UserType::kNonSyncingAndMigrationNeeded:
      suffix = ".NonSyncingWithMigration";
      break;
    case UserType::kNonSyncingAndNoMigrationNeeded:
      suffix = ".NonSyncingNoMigration";
      break;
    case UserType::kSyncing:
      suffix = ".Syncing";
      break;
  }
  CHECK(suffix);
  base::UmaHistogramEnumeration(base::StrCat({kHistogramPrefix, suffix}),
                                error);
  base::UmaHistogramEnumeration(kHistogramPrefix, error);
}

// Must only be called if the state pref is kOff, to set it to kOn or
// kOffAndMigrationPending if all the preconditions are satisfied.
void MaybeActivateSplitStoresAndLocalUpm(
    PrefService* pref_service,
    const base::FilePath& login_db_directory) {
  CHECK_EQ(GetSplitStoresAndLocalUpmPrefValue(pref_service), kOff);

  UserType user_type = GetUserType(pref_service, login_db_directory);
  if (!HasMinGmsVersion()) {
    RecordActivationError(user_type, ActivationError::kOutdatedGmsCore);
    return;
  }

  UseUpmLocalAndSeparateStoresState state_to_set_on_success = kOn;
  ActivationError error = ActivationError::kNone;
  switch (user_type) {
    case UserType::kNonSyncingAndNoMigrationNeeded:
      break;
    case UserType::kNonSyncingAndMigrationNeeded:
      if (ShouldDelayMigrationUntillMigrationWarningIsAcknowledged(
              pref_service)) {
        error = ActivationError::kMigrationWarningUnacknowledged;
        break;
      }
      state_to_set_on_success = kOffAndMigrationPending;
      break;
    case UserType::kSyncing: {
      // kCurrentMigrationVersionToGoogleMobileServices is only 0 or 1.
      if (password_manager_upm_eviction::IsCurrentUserEvicted(pref_service) ||
          pref_service->GetInteger(
              kCurrentMigrationVersionToGoogleMobileServices) == 0) {
        // Initial UPM was not activated properly. Attempt to migrate passwords
        // to local GMSCore.
        state_to_set_on_success = kOffAndMigrationPending;
        break;
      }
      // Move the "profile" login DB to the "account" path, the latter is the
      // synced one after activation. We could rely on a redownload instead, but
      // a) this is a safety net, and b)it spares traffic.
      base::FilePath profile_db_path = login_db_directory.Append(
          password_manager::kLoginDataForProfileFileName);
      if (!base::ReplaceFile(
              profile_db_path,
              login_db_directory.Append(
                  password_manager::kLoginDataForAccountFileName),
              /*error=*/nullptr)) {
        error = ActivationError::kLoginDbFileMoveFailed;
        break;
      }
      break;
    }
  }
  RecordActivationError(user_type, error);

  if (error == ActivationError::kNone) {
    pref_service->SetInteger(kPasswordsUseUPMLocalAndSeparateStores,
                             static_cast<int>(state_to_set_on_success));
  }
}

// Called on startup from `MaybeDeactivateSplitStoresAndLocalUpm` to delete the
// login data files for users migrated to UPM. Must only be called if the value
// of the state pref `PasswordsUseUPMLocalAndSeparateStores` is `On` and there
// is no need for deactivation of local UPM.
void MaybeDeleteLoginDataFiles(PrefService* prefs,
                               const base::FilePath& login_db_directory) {
  CHECK(password_manager::UsesSplitStoresAndUPMForLocal(prefs));

  base::FilePath profile_db_path =
      login_db_directory.Append(password_manager::kLoginDataForProfileFileName);
  base::FilePath account_db_path =
      login_db_directory.Append(password_manager::kLoginDataForAccountFileName);
  base::FilePath profile_db_journal_path = login_db_directory.Append(
      password_manager::kLoginDataJournalForProfileFileName);
  base::FilePath account_db_journal_path = login_db_directory.Append(
      password_manager::kLoginDataJournalForAccountFileName);

  // Delete the login data files for the user migrated to UPM.
  // In the unlikely case that the deletion operation fails, it will be
  // retried upon next startup as part of
  // `MaybeDeactivateSplitStoresAndLocalUpm`.
  if (PathExists(profile_db_path)) {
    bool success = base::DeleteFile(profile_db_path);
    base::UmaHistogramBoolean("PasswordManager.ProfileLoginData.RemovalStatus",
                              success);
    if (success) {
      prefs->SetBoolean(
          password_manager::prefs::kEmptyProfileStoreLoginDatabase, true);
    }
  }
  base::DeleteFile(profile_db_journal_path);

  if (PathExists(account_db_path)) {
    bool success = base::DeleteFile(account_db_path);
    base::UmaHistogramBoolean("PasswordManager.AccountLoginData.RemovalStatus",
                              success);
  }
  base::DeleteFile(account_db_journal_path);
}

// Must only be called if the state pref is kOn or kOffAndMigrationPending, to
// set it to kOff if the user downgraded GmsCore. Any passwords saved to GmsCore
// while in kOn will stay in GmsCore and become available again on the next
// successful activation; they will not be migrated back to the LoginDB. If the
// user is syncing, this function tries to undo [1] the Login DB file move done
// in MaybeActivateSplitStoresAndLocalUpm(), and aborts on failure [2].
//
// [1] In truth, this is only an "undo" if the user was already syncing *before*
// the activation. In rare cases, they might have been signed out with saved
// passwords, activated, enabled sync and now get deactivated. If so, this
// function overwrites a non-empty profile Login DB. That's fine: the content
// got migrated to GmsCore and will become available again on the next
// successful activation.
//
// [2] In hindsight, this is questionable, because the user stays marked as
// activated even though they can't use GmsCore APIs.
void MaybeDeactivateSplitStoresAndLocalUpm(
    PrefService* pref_service,
    const base::FilePath& login_db_directory) {
  CHECK_NE(GetSplitStoresAndLocalUpmPrefValue(pref_service), kOff);

  // Continue recording the metric for previously activated users. so they show
  // up on the dashboard no matter the aggregation window. One caveat is the
  // state recorded now might not be the same one where the user got activated
  // E.g. they might have gone from syncing to non-syncing. Also the recording
  // here ignores the possibility that rollback fails due to base::ReplaceFile()
  // below, but that should be negligible.
  RecordActivationError(GetUserType(pref_service, login_db_directory),
                        HasMinGmsVersion() ? ActivationError::kNone
                                           : ActivationError::kOutdatedGmsCore);
  if (HasMinGmsVersion()) {
    // GmsCore was not downgraded, no need to deactivate.
    if (GetSplitStoresAndLocalUpmPrefValue(pref_service) == kOn &&
        base::FeatureList::IsEnabled(
            password_manager::features::
                kClearLoginDatabaseForAllMigratedUPMUsers)) {
      MaybeDeleteLoginDataFiles(pref_service, login_db_directory);
    }
    return;
  }

  // GmsCore was downgraded, so from here on the function wants to deactivate.
  base::FilePath profile_db_path =
      login_db_directory.Append(password_manager::kLoginDataForProfileFileName);
  base::FilePath account_db_path =
      login_db_directory.Append(password_manager::kLoginDataForAccountFileName);
  if (GetSplitStoresAndLocalUpmPrefValue(pref_service) == kOn &&
      IsPasswordSyncEnabled(pref_service) &&
      base::PathExists(account_db_path) &&
      !base::ReplaceFile(account_db_path, profile_db_path, /*error=*/nullptr)) {
    // See point [2] above.
    return;
  }

  pref_service->SetInteger(kPasswordsUseUPMLocalAndSeparateStores,
                           static_cast<int>(kOff));
}

bool EmptyProfileStore(PrefService* pref_service) {
  return pref_service->GetBoolean(
      password_manager::prefs::kEmptyProfileStoreLoginDatabase);
}

std::string_view GetAccessLossWarningTypeName(
    PasswordAccessLossWarningType warning_type) {
  switch (warning_type) {
    case PasswordAccessLossWarningType::kNoUpm:
      return "NoUPM";
    case PasswordAccessLossWarningType::kOnlyAccountUpm:
      return "OnlyAccountUpm";
    case PasswordAccessLossWarningType::kNoGmsCore:
      return "NoGmsCore";
    case PasswordAccessLossWarningType::kNewGmsCoreMigrationFailed:
      return "NewGmsCoreMigrationFailed";
    case PasswordAccessLossWarningType::kNone:
      NOTREACHED();
  }
}

}  // namespace

UseUpmLocalAndSeparateStoresState GetSplitStoresAndLocalUpmPrefValue(
    PrefService* pref_service) {
  auto value = static_cast<UseUpmLocalAndSeparateStoresState>(
      pref_service->GetInteger(kPasswordsUseUPMLocalAndSeparateStores));
  switch (value) {
    case kOff:
    case kOffAndMigrationPending:
    case kOn:
      return value;
  }
  NOTREACHED();
}

bool AreMinUpmRequirementsMet() {
  if (!IsInternalBackendPresent()) {
    return false;
  }

  int gms_version = 0;
  // GMSCore version could not be parsed, probably no GMSCore installed.
  if (!base::StringToInt(
          base::android::BuildInfo::GetInstance()->gms_version_code(),
          &gms_version)) {
    return false;
  }

  // If the GMSCore version is pre-UPM an update is required.
  return gms_version >= password_manager::kAccountUpmMinGmsVersion;
}

bool ShouldUseUpmWiring(const syncer::SyncService* sync_service,
                        const PrefService* pref_service) {
  bool is_pwd_sync_enabled =
      password_manager::sync_util::HasChosenToSyncPasswords(sync_service);
  if (is_pwd_sync_enabled &&
      password_manager_upm_eviction::IsCurrentUserEvicted(pref_service)) {
    return false;
  }
  if (is_pwd_sync_enabled) {
    return true;
  }
  return password_manager::UsesSplitStoresAndUPMForLocal(pref_service);
}

void SetUsesSplitStoresAndUPMForLocal(
    PrefService* pref_service,
    const base::FilePath& login_db_directory) {
  UseUpmLocalAndSeparateStoresState split_stores_and_local_upm =
      GetSplitStoresAndLocalUpmPrefValue(pref_service);
  last_migration_attempt_failed =
      split_stores_and_local_upm == kOffAndMigrationPending ? true : false;

  if (split_stores_and_local_upm != kOff) {
    MaybeDeactivateSplitStoresAndLocalUpm(pref_service, login_db_directory);
  } else {
    MaybeActivateSplitStoresAndLocalUpm(pref_service, login_db_directory);
  }

  // Records false for users who had a migration scheduled but weren't activated
  // yet, which is different from RecordActivationError().
  base::UmaHistogramBoolean(
      "PasswordManager.LocalUpmActivated",
      password_manager::UsesSplitStoresAndUPMForLocal(pref_service));
  base::UmaHistogramEnumeration(
      "PasswordManager.LocalUpmActivationStatus",
      GetSplitStoresAndLocalUpmPrefValue(pref_service));
}

PasswordAccessLossWarningType GetPasswordAccessLossWarningType(
    PrefService* pref_service) {
  // No warning should be displayed to the users, who don't have any passwords
  // in the profile store.
  if (EmptyProfileStore(pref_service)) {
    return PasswordAccessLossWarningType::kNone;
  }

  std::string gms_version_str =
      base::android::BuildInfo::GetInstance()->gms_version_code();
  int gms_version = 0;
  // GMSCore version could not be parsed, probably no GMSCore installed.
  if (!base::StringToInt(gms_version_str, &gms_version)) {
    return PasswordAccessLossWarningType::kNoGmsCore;
  }

  // GMSCore version is pre-UPM, update is required.
  if (gms_version < password_manager::kAccountUpmMinGmsVersion) {
    return PasswordAccessLossWarningType::kNoUpm;
  }

  // GMSCore version supports the account passwords, but doesn't support local
  // passwords. Update is still required.
  if (gms_version < password_manager::GetLocalUpmMinGmsVersion()) {
    return PasswordAccessLossWarningType::kOnlyAccountUpm;
  }

  // GMSCore is up to date, but the local passwords migration has failed, so
  // manual export/import flow should be done. Checking the
  // `SplitStoresAndLocalUpmState` again here because the migration might have
  // succeeded in this run.
  if (last_migration_attempt_failed &&
      GetSplitStoresAndLocalUpmPrefValue(pref_service) ==
          kOffAndMigrationPending) {
    return PasswordAccessLossWarningType::kNewGmsCoreMigrationFailed;
  }

  // Everything is fine, no warning will be shown.
  return PasswordAccessLossWarningType::kNone;
}

void RecordPasswordAccessLossWarningTriggerSource(
    PasswordAccessLossWarningTriggers trigger_source,
    PasswordAccessLossWarningType warning_type) {
  base::UmaHistogramEnumeration(
      base::StrCat({"PasswordManager.PasswordAccessLossWarningSheet",
                    GetAccessLossWarningTypeName(warning_type), "Trigger"}),
      trigger_source);
}

}  // namespace password_manager_android_util
