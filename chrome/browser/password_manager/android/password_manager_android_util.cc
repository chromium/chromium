// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_manager_android_util.h"

#include <string>

#include "base/android/build_info.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/password_manager/android/password_manager_eviction_util.h"
#include "chrome/browser/password_manager/android/password_manager_util_bridge.h"
#include "chrome/common/chrome_switches.h"
#include "components/browser_sync/sync_to_signin_migration.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_manager_buildflags.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/password_manager/core/browser/password_store/split_stores_and_local_upm.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/pref_names.h"
#include "components/version_info/android/channel_getter.h"
#include "third_party/abseil-cpp/absl/base/attributes.h"

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
  kUnenrolled = 1,
  kInitialUpmMigrationMissing = 2,
  kLoginDbFileMoveFailed = 3,
  kOutdatedGmsCore = 4,
  kFlagDisabled = 5,
  kMigrationWarningUnacknowledged = 6,
  kMaxValue = kMigrationWarningUnacknowledged,
};

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

// WARNING: Use this function rather than base::FeatureList::IsEnabled(), it
// defers the base::Feature checks to avoid adding ineligible users to the A/B
// experiment.
ActivationError CheckMinGmsVersionAndFlagEnabled(const base::Feature& feature) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSkipLocalUpmGmsCoreVersionCheckForTesting)) {
    return base::FeatureList::IsEnabled(feature)
               ? ActivationError::kNone
               : ActivationError::kFlagDisabled;
  }

  std::string gms_version_str =
      base::android::BuildInfo::GetInstance()->gms_version_code();
  int gms_version = 0;
  // gms_version_code() must be converted to int for comparison, because it can
  // have legacy values "3(...)" and those evaluate > "2023(...)".
  if (!base::StringToInt(gms_version_str, &gms_version)) {
    return ActivationError::kOutdatedGmsCore;
  }

  // Compare with the compile-time constant before comparing with the
  // runtime value, as the latter will add the user to the A/B experiment.
  //
  // Note: We need to use this value as a sentinel value for auto as well
  // at this point, to allow server-side changes to activate the feature without
  // client-side changes being needed, once a min version is established.
  // As soon as the min GMS version for auto can be changed client-side,
  // consider using it as a sentinel value here instead.
  if (gms_version < password_manager::features::kDefaultLocalUpmMinGmsVersion) {
    return ActivationError::kOutdatedGmsCore;
  }

  if (base::android::BuildInfo::GetInstance()->is_automotive() &&
      gms_version <
          base::GetFieldTrialParamByFeatureAsInt(
              feature,
              password_manager::features::kLocalUpmMinGmsVersionParamForAuto,
              password_manager::features::
                  kDefaultLocalUpmMinGmsVersionForAuto)) {
    return ActivationError::kOutdatedGmsCore;
  }

  if (!base::android::BuildInfo::GetInstance()->is_automotive() &&
      gms_version <
          base::GetFieldTrialParamByFeatureAsInt(
              feature, password_manager::features::kLocalUpmMinGmsVersionParam,
              password_manager::features::kDefaultLocalUpmMinGmsVersion)) {
    return ActivationError::kOutdatedGmsCore;
  }

  return base::FeatureList::IsEnabled(feature) ? ActivationError::kNone
                                               : ActivationError::kFlagDisabled;
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

  UseUpmLocalAndSeparateStoresState state_to_set_on_success = kOn;
  ActivationError error = ActivationError::kNone;
  UserType user_type = GetUserType(pref_service, login_db_directory);
  switch (user_type) {
    case UserType::kNonSyncingAndNoMigrationNeeded:
      error = CheckMinGmsVersionAndFlagEnabled(
          password_manager::features::
              kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration);
      break;
    case UserType::kNonSyncingAndMigrationNeeded:
      if (ShouldDelayMigrationUntillMigrationWarningIsAcknowledged(
              pref_service)) {
        error = ActivationError::kMigrationWarningUnacknowledged;
        break;
      }
      error = CheckMinGmsVersionAndFlagEnabled(
          password_manager::features::
              kUnifiedPasswordManagerLocalPasswordsAndroidWithMigration);
      state_to_set_on_success = kOffAndMigrationPending;
      break;
    case UserType::kSyncing: {
      if (password_manager_upm_eviction::IsCurrentUserEvicted(pref_service)) {
        error = ActivationError::kUnenrolled;
        break;
      }
      // kCurrentMigrationVersionToGoogleMobileServices is only 0 or 1.
      if (pref_service->GetInteger(
              kCurrentMigrationVersionToGoogleMobileServices) == 0) {
        error = ActivationError::kInitialUpmMigrationMissing;
        break;
      }
      error = CheckMinGmsVersionAndFlagEnabled(
          password_manager::features::
              kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration);
      if (error != ActivationError::kNone) {
        break;
      }
      // Move the "profile" login DB to the "account" path, the latter is the
      // synced one after activation. We could rely on a redownload instead, but
      // a) this is a safety net, and b)it spares traffic.
      if (!base::ReplaceFile(
              login_db_directory.Append(
                  password_manager::kLoginDataForProfileFileName),
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

  if (ActivationError::kUnenrolled == error ||
      ActivationError::kInitialUpmMigrationMissing == error) {
    // Initial UPM was not activated properly. Attempt to migrate passwords
    // to local GMSCore.
    state_to_set_on_success = kOffAndMigrationPending;
    error = CheckMinGmsVersionAndFlagEnabled(
        password_manager::features::kUnifiedPasswordManagerSyncOnlyInGMSCore);
  }

  if (error == ActivationError::kNone) {
    pref_service->SetInteger(kPasswordsUseUPMLocalAndSeparateStores,
                             static_cast<int>(state_to_set_on_success));
  }
}

// Must only be called if the state pref is kOn or kOffAndMigrationPending, to
// set it to kOff if any of these happened:
// - The user downgraded GmsCore and can no longer use the local UPM properly.
// - The min GmsCore version for the A/B experiment was bumped server-side.
// - The A/B experiment was stopped due to bugs.
// - The user manually turned off the flag.
void MaybeDeactivateSplitStoresAndLocalUpm(
    PrefService* pref_service,
    const base::FilePath& login_db_directory) {
  CHECK_NE(GetSplitStoresAndLocalUpmPrefValue(pref_service), kOff);

  if (GetSplitStoresAndLocalUpmPrefValue(pref_service) ==
      kOffAndMigrationPending) {
    // The migration was previously scheduled but didn't succeed yet. Cancel it
    // if the WithMigration flag was disabled since, or if the GmsCore version
    // is no longer suitable. This provides an escape hatch for users who fail
    // the migration every time and would otherwise stay with sync supppressed
    // forever.
    //
    // Note: disabling the WithMigration flag does nothing to users who were
    // already activated (kOn), see below.
    ActivationError error = CheckMinGmsVersionAndFlagEnabled(
        password_manager::features::
            kUnifiedPasswordManagerLocalPasswordsAndroidWithMigration);
    // See comment in the other RecordActivationError() call below.
    RecordActivationError(GetUserType(pref_service, login_db_directory), error);
    if (error != ActivationError::kNone) {
      pref_service->SetInteger(kPasswordsUseUPMLocalAndSeparateStores,
                               static_cast<int>(kOff));
    }
    return;
  }

  // The user was activated. Only deactivate based on the *NoMigration* flag.
  // - If problems arise when rolling out NoMigration (first launch), disable
  //   that flag server-side. Non-syncing users will revert to using the login
  //   DB. Syncing users will revert to a single PasswordStore talking to
  //   GmsCore.
  // - If problems arise when rolling out WithMigration (second launch), there
  //   are 2 options:
  //     1. Keep NoMigration enabled. This means:
  //       * Users whose migration always fails stay deactivated, which is good.
  //         For those, it's enough to implement client-side fixes for the
  //         migration.
  //       * Users whose migration was incorrectly reported as successful (e.g.
  //         some passwords are missing) stay activated, which is bad. For
  //         those, either implement new client-side fixes as above (the
  //         login DB data still exists, the migration can be re-attempted) and
  //         wait for them to launch, or go with option 2 below.
  //     2. Disable NoMigration.
  //       * Deactivates all users, reverting them to the old behavior, even
  //         healthy ones.
  // This flag check also keeps the user in the A/B experiment after activation.
  ActivationError error = CheckMinGmsVersionAndFlagEnabled(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration);
  // Continue recording the metric for previously activated users. so they show
  // up on the dashboard no matter the aggregation window. One caveat is the
  // state recorded now might not be the same one where the user got activated
  // E.g. they might have gone from syncing to non-syncing. Also the recording
  // here ignores the possibility that rollback fails due to base::ReplaceFile()
  // below, but that should be negligible.
  RecordActivationError(GetUserType(pref_service, login_db_directory), error);
  if (error == ActivationError::kNone) {
    // Artificial check to keep the user in the A/B experiment after activation.
    // (In practice, the check for NoMigration above might be enough, the flags
    // will probably be in a combined study.)
    base::FeatureList::IsEnabled(
        password_manager::features::
            kUnifiedPasswordManagerLocalPasswordsAndroidWithMigration);
    return;
  }

  // If the user is non-syncing, there's no reverse migration from GmsCore back
  // to the login DBs. Any passwords saved to GmsCore while activated will stay
  // there and will become available again on the next successful activation.
  // If the user is syncing, undo the DB file move (see comment in activation
  // function). In truth, this is only an "undo" if the user was already syncing
  // *before* the activation. In rare cases, they might have been signed out
  // with saved passwords, activated by the WithMigration flag, enabled sync and
  // now get deactivated. If so, `profile_db_path` is non-empty and gets
  // overwritten nevertheless. That's fine, the content got migrated to GmsCore
  // and will become available again on the next successful activation.
  // An alternative that would perform better in such case is to rely on a
  // redownload. But that would entail more risk for syncing users, a population
  // much larger than the one affected by this unlikely case.
  base::FilePath profile_db_path =
      login_db_directory.Append(password_manager::kLoginDataForProfileFileName);
  base::FilePath account_db_path =
      login_db_directory.Append(password_manager::kLoginDataForAccountFileName);
  if (GetSplitStoresAndLocalUpmPrefValue(pref_service) == kOn &&
      IsPasswordSyncEnabled(pref_service) &&
      !base::ReplaceFile(account_db_path, profile_db_path, /*error=*/nullptr)) {
    return;
  }
  pref_service->SetInteger(kPasswordsUseUPMLocalAndSeparateStores,
                           static_cast<int>(kOff));
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
  NOTREACHED_NORETURN();
}

bool AreMinUpmRequirementsMet() {
  if (!IsInternalBackendPresent()) {
    return false;
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSkipLocalUpmGmsCoreVersionCheckForTesting)) {
    return true;
  }

  int gms_version = 0;
  // GMSCore version could not be parsed, probably no GMSCore installed.
  if (!base::StringToInt(
          base::android::BuildInfo::GetInstance()->gms_version_code(),
          &gms_version)) {
    return false;
  }

  // If the GMSCore version is pre-UPM an update is required.
  return gms_version >= password_manager::features::kAccountUpmMinGmsVersion;
}

bool ShouldUseUpmWiring(bool is_pwd_sync_enabled, PrefService* pref_service) {
  // TODO(crbug.com/40226137): Re-evaluate if the SyncService can be passed here
  // instead of the `is_pwd_sync_enabled` boolean.
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
  if (GetSplitStoresAndLocalUpmPrefValue(pref_service) != kOff) {
    MaybeDeactivateSplitStoresAndLocalUpm(pref_service, login_db_directory);
  } else {
    MaybeActivateSplitStoresAndLocalUpm(pref_service, login_db_directory);
  }

  // Records false for users who had a migration scheduled but weren't activated
  // yet, which is different from RecordActivationError().
  base::UmaHistogramBoolean(
      "PasswordManager.LocalUpmActivated",
      password_manager::UsesSplitStoresAndUPMForLocal(pref_service));
}

}  // namespace password_manager_android_util
