// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_manager_android_util.h"

#include <string>

#include "base/android/build_info.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/password_manager/android/password_manager_eviction_util.h"
#include "components/browser_sync/sync_to_signin_migration.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/pref_names.h"
#include "third_party/abseil-cpp/absl/base/attributes.h"

using password_manager::prefs::UseUpmLocalAndSeparateStoresState;

namespace password_manager_android_util {

namespace {

// TODO(crbug.com/1495626): Make the min GmsCore version a base::FeatureParam
// and update the default value (233106000 is too low).
int kMinGmsVersionCodeForLocalUpm = 233106000;

enum class LocalUpmUserType {
  kNotEligible,
  kNotSyncingAndMigrationNeeded,
  kNotSyncingAndNoMigrationNeeded,
  kSyncing
};

UseUpmLocalAndSeparateStoresState GetSplitStoresAndLocalUpmPrefValue(
    PrefService* pref_service) {
  auto value =
      static_cast<UseUpmLocalAndSeparateStoresState>(pref_service->GetInteger(
          password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores));
  switch (value) {
    case UseUpmLocalAndSeparateStoresState::kOff:
    case UseUpmLocalAndSeparateStoresState::kOffAndMigrationPending:
    case UseUpmLocalAndSeparateStoresState::kOn:
      return value;
  }
  NOTREACHED_NORETURN();
}

LocalUpmUserType GetLocalUpmUserType(PrefService* pref_service,
                                     const base::FilePath& login_db_directory) {
  std::string gms_version_str =
      base::android::BuildInfo::GetInstance()->gms_version_code();
  int gms_version = 0;
  // `gms_version_str` must be converted to int for comparison, because it can
  // have legacy values "3(...)" and those evaluate > "2023(...)".
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          kSkipLocalUpmGmsCoreVersionCheckForTesting) &&
      (!base::StringToInt(gms_version_str, &gms_version) ||
       gms_version < kMinGmsVersionCodeForLocalUpm)) {
    return LocalUpmUserType::kNotEligible;
  }

  const PrefService::Preference* empty_profile_db_pref =
      pref_service->FindPreference(
          password_manager::prefs::kEmptyProfileStoreLoginDatabase);
  if (empty_profile_db_pref->IsDefaultValue()) {
    // The logic to write `empty_profile_db_pref` was added a few milestones
    // before the local UPM rollout. So either,
    // - The user skipped those milestones and only upgraded now (less likely).
    //   Wait until the pref value is known.
    // - The user just installed the app (more likely). They are not syncing yet
    //   and there are no existing passwords to migrate.
    return base::PathExists(login_db_directory.Append(
               password_manager::kLoginDataForProfileFileName))
               ? LocalUpmUserType::kNotEligible
               : LocalUpmUserType::kNotSyncingAndNoMigrationNeeded;
  }

  switch (browser_sync::GetSyncToSigninMigrationDataTypeDecision(
      pref_service, syncer::PASSWORDS,
      syncer::prefs::internal::kSyncPasswords)) {
    // `kDontMigrateTypeNotActive` is handled same as if the data type was
    // active, because all that matters is the user's choice to sync the type.
    case browser_sync::SyncToSigninMigrationDataTypeDecision::
        kDontMigrateTypeNotActive:
    case browser_sync::SyncToSigninMigrationDataTypeDecision::kMigrate:
      return LocalUpmUserType::kSyncing;
    case browser_sync::SyncToSigninMigrationDataTypeDecision::
        kDontMigrateTypeDisabled:
      return empty_profile_db_pref->GetValue()->GetBool() &&
                     pref_service
                         ->FindPreference(
                             password_manager::prefs::kCredentialsEnableService)
                         ->IsDefaultValue() &&
                     pref_service
                         ->FindPreference(password_manager::prefs::
                                              kCredentialsEnableAutosignin)
                         ->IsDefaultValue()
                 ? LocalUpmUserType::kNotSyncingAndNoMigrationNeeded
                 : LocalUpmUserType::kNotSyncingAndMigrationNeeded;
  }
  NOTREACHED_NORETURN();
}

}  // namespace

bool UsesSplitStoresAndUPMForLocal(PrefService* pref_service) {
  switch (GetSplitStoresAndLocalUpmPrefValue(pref_service)) {
    case UseUpmLocalAndSeparateStoresState::kOff:
    case UseUpmLocalAndSeparateStoresState::kOffAndMigrationPending:
      return false;
    case UseUpmLocalAndSeparateStoresState::kOn:
      return true;
  }
  NOTREACHED_NORETURN();
}

bool CanUseUPMBackend(bool is_pwd_sync_enabled, PrefService* pref_service) {
  // TODO(crbug.com/1327294): Re-evaluate if the SyncService can be passed here
  // instead of the `is_pwd_sync_enabled` boolean.
  // TODO(crbug.com/1500201): Re-evaluate unenrollment.
  if (is_pwd_sync_enabled &&
      password_manager_upm_eviction::IsCurrentUserEvicted(pref_service)) {
    return false;
  }
  if (is_pwd_sync_enabled) {
    return true;
  }
  return UsesSplitStoresAndUPMForLocal(pref_service);
}

void SetUsesSplitStoresAndUPMForLocal(
    PrefService* pref_service,
    const base::FilePath& login_db_directory) {
  switch (GetLocalUpmUserType(pref_service, login_db_directory)) {
    case LocalUpmUserType::kNotEligible: {
      // TODO(crbug.com/1495626): Consider also switching the 2 LoginDB files if
      // the min required GmsCore version is bumped and the user transitions to
      // kNotEligible.
      pref_service->SetInteger(
          password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores,
          static_cast<int>(UseUpmLocalAndSeparateStoresState::kOff));
      return;
    }
    case LocalUpmUserType::kSyncing: {
      bool no_migration_flag_enabled = base::FeatureList::IsEnabled(
          password_manager::features::
              kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration);

      if (no_migration_flag_enabled ==
          UsesSplitStoresAndUPMForLocal(pref_service)) {
        return;
      }

      // If this is a rollout, syncing will switch from the "profile" LoginDB
      // to the "account" DB (currently empty). Move the existing data by moving
      // the DB file. Note: one could rely on a redownload instead but that's
      // riskier.
      // If this is a rollback, it's the other way around. Move in the opposite
      // direction. The "profile" DB might not be empty, but if so it only
      // contains non-synced passwords previously migrated to the Android
      // backend, and thus fine to overwrite.
      base::FilePath from_path = login_db_directory.Append(
          password_manager::kLoginDataForProfileFileName);
      base::FilePath to_path = login_db_directory.Append(
          password_manager::kLoginDataForAccountFileName);
      if (!no_migration_flag_enabled) {
        std::swap(from_path, to_path);
      }
      if (!base::ReplaceFile(from_path, to_path, /*error=*/nullptr)) {
        // IO failed. Don't set kPasswordsUseUPMLocalAndSeparateStores so
        // it's retried on the next startup.
        return;
      }
      ABSL_FALLTHROUGH_INTENDED;
    }
    case LocalUpmUserType::kNotSyncingAndNoMigrationNeeded: {
      pref_service->SetInteger(
          password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores,
          static_cast<int>(
              base::FeatureList::IsEnabled(
                  password_manager::features::
                      kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration)
                  ? UseUpmLocalAndSeparateStoresState::kOn
                  : UseUpmLocalAndSeparateStoresState::kOff));
      return;
    }
    case LocalUpmUserType::kNotSyncingAndMigrationNeeded: {
      if (GetSplitStoresAndLocalUpmPrefValue(pref_service) ==
          UseUpmLocalAndSeparateStoresState::kOffAndMigrationPending) {
        // The browser was closed before the migration could finish, reset.
        pref_service->SetInteger(
            password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores,
            static_cast<int>(UseUpmLocalAndSeparateStoresState::kOff));
      }

      bool migration_flag_enabled = base::FeatureList::IsEnabled(
          password_manager::features::
              kUnifiedPasswordManagerLocalPasswordsAndroidWithMigration);
      if (migration_flag_enabled ==
          UsesSplitStoresAndUPMForLocal(pref_service)) {
        return;
      }
      pref_service->SetInteger(
          password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores,
          static_cast<int>(
              migration_flag_enabled
                  ? UseUpmLocalAndSeparateStoresState::kOffAndMigrationPending
                  : UseUpmLocalAndSeparateStoresState::kOff));

      return;
    }
  }
  NOTREACHED_NORETURN();
}

}  // namespace password_manager_android_util
