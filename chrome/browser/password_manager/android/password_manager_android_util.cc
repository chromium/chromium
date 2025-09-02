// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_manager_android_util.h"

#include <string>

#include "base/android/device_info.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/password_manager/android/password_manager_util_bridge.h"
#include "chrome/browser/password_manager/android/password_manager_util_bridge_interface.h"
#include "components/password_manager/core/browser/password_manager_buildflags.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/password_manager/core/browser/split_stores_and_local_upm.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"

namespace password_manager_android_util {

namespace {

bool HasMinGmsVersionForFullUpmSupport() {
  std::string gms_version_str = base::android::device_info::gms_version_code();
  int gms_version = 0;
  // gms_version_code() must be converted to int for comparison, because it can
  // have legacy values "3(...)" and those evaluate > "2023(...)".
  return base::StringToInt(gms_version_str, &gms_version) &&
         gms_version >= password_manager::GetSplitStoresUpmMinVersion();
}

#if !BUILDFLAG(USE_LOGIN_DATABASE_AS_BACKEND)
// Called on startup to delete the login data files.
void DeleteLoginDataFiles(const base::FilePath& login_db_directory) {
  base::FilePath profile_db_path =
      login_db_directory.Append(password_manager::kLoginDataForProfileFileName);
  base::FilePath account_db_path =
      login_db_directory.Append(password_manager::kLoginDataForAccountFileName);
  base::FilePath profile_db_journal_path = login_db_directory.Append(
      password_manager::kLoginDataJournalForProfileFileName);
  base::FilePath account_db_journal_path = login_db_directory.Append(
      password_manager::kLoginDataJournalForAccountFileName);

  if (PathExists(profile_db_path)) {
    bool success = base::DeleteFile(profile_db_path);
    base::UmaHistogramBoolean("PasswordManager.ProfileLoginData.RemovalStatus",
                              success);
  }
  base::DeleteFile(profile_db_journal_path);

  if (PathExists(account_db_path)) {
    bool success = base::DeleteFile(account_db_path);
    base::UmaHistogramBoolean("PasswordManager.AccountLoginData.RemovalStatus",
                              success);
  }
  base::DeleteFile(account_db_journal_path);
}

void DeleteAutoExportedCsv(PrefService* prefs,
                           const base::FilePath& login_db_directory) {
  base::FilePath csv_path =
      login_db_directory.Append(FILE_PATH_LITERAL(kExportedPasswordsFileName));
  if (base::PathExists(csv_path)) {
    bool success = base::DeleteFile(csv_path);
    if (success) {
      prefs->SetBoolean(password_manager::prefs::kUpmAutoExportCsvNeedsDeletion,
                        false);
    }
    base::UmaHistogramBoolean(
        "PasswordManager.UPM.AutoExportedCsvStartupDeletionSuccess", success);
  }
}

#endif  // !BUILDFLAG(USE_LOGIN_DATABASE_AS_BACKEND)

}  // namespace

PasswordManagerNotAvailableReason GetPasswordManagerNotActiveReason(
    PrefService* pref_service,
    PasswordManagerUtilBridgeInterface* util_bridge,
    bool is_internal_backend_present) {
  if (!is_internal_backend_present) {
    return PasswordManagerNotAvailableReason::kInternalBackendNotPresent;
  }

  if (!HasMinGmsVersionForFullUpmSupport()) {
    if (!util_bridge->IsGooglePlayServicesUpdatable()) {
      return PasswordManagerNotAvailableReason::kNoGmsCore;
    }
    return PasswordManagerNotAvailableReason::kOutdatedGmsCore;
  }

  NOTREACHED();
}

void RecordLocalUpmActivationMetrics(
    PrefService* pref_service,
    PasswordManagerUtilBridgeInterface* util_bridge) {
  bool is_internal_backend_present = util_bridge->IsInternalBackendPresent();
  bool is_pwm_available =
      IsPasswordManagerAvailable(pref_service, is_internal_backend_present);
  base::UmaHistogramBoolean("PasswordManager.LocalUpmActivated",
                            is_pwm_available);
  if (!is_pwm_available) {
    base::UmaHistogramEnumeration(
        "PasswordManager.Android.NotAvailableReason",
        GetPasswordManagerNotActiveReason(pref_service, util_bridge,
                                          is_internal_backend_present));
  }
}

bool IsPasswordManagerAvailable(
    const PrefService* prefs,
    std::unique_ptr<PasswordManagerUtilBridgeInterface> util_bridge) {
  return IsPasswordManagerAvailable(prefs,
                                    util_bridge->IsInternalBackendPresent());
}

bool IsPasswordManagerAvailable(const PrefService* prefs,
                                bool is_internal_backend_present) {
  if (!is_internal_backend_present) {
    return false;
  }

  if (!HasMinGmsVersionForFullUpmSupport()) {
    return false;
  }

  return true;
}

void MaybeDeleteLoginDatabases(
    PrefService* pref_service,
    const base::FilePath& login_db_directory,
    std::unique_ptr<PasswordManagerUtilBridgeInterface> util_bridge) {
  RecordLocalUpmActivationMetrics(pref_service, util_bridge.get());
#if !BUILDFLAG(USE_LOGIN_DATABASE_AS_BACKEND)
  DeleteLoginDataFiles(login_db_directory);
  if (pref_service->GetBoolean(
          password_manager::prefs::kUpmAutoExportCsvNeedsDeletion)) {
    DeleteAutoExportedCsv(pref_service, login_db_directory);
  }
#endif
}

}  // namespace password_manager_android_util
