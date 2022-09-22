// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_manager_eviction_util.h"

#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"

namespace password_manager_upm_eviction {

bool IsCurrentUserEvicted(const PrefService* prefs) {
  return prefs->GetBoolean(
      password_manager::prefs::kUnenrolledFromGoogleMobileServicesDueToErrors);
}

void EvictCurrentUser(int api_error_code, PrefService* prefs) {
  if (prefs->GetBoolean(password_manager::prefs::
                            kUnenrolledFromGoogleMobileServicesDueToErrors))
    // User is already evicted, keep the original eviction reason.
    return;

  prefs->SetBoolean(
      password_manager::prefs::kUnenrolledFromGoogleMobileServicesDueToErrors,
      true);
  prefs->SetInteger(password_manager::prefs::
                        kUnenrolledFromGoogleMobileServicesAfterApiErrorCode,
                    api_error_code);
  prefs->SetInteger(password_manager::prefs::
                        kUnenrolledFromGoogleMobileServicesWithErrorListVersion,
                    password_manager::features::kGmsApiErrorListVersion.Get());

  // Reset migration prefs so when the user can join the experiment again,
  // non-syncable data and settings can be migrated to GMS Core.
  prefs->SetInteger(
      password_manager::prefs::kCurrentMigrationVersionToGoogleMobileServices,
      0);
  prefs->SetDouble(password_manager::prefs::kTimeOfLastMigrationAttempt, 0.0);
  prefs->SetBoolean(password_manager::prefs::kSettingsMigratedToUPM, false);

  base::UmaHistogramBoolean("PasswordManager.UnenrolledFromUPMDueToErrors",
                            true);
  LOG(ERROR) << "Unenrolled from UPM due to error with code: "
             << api_error_code;
}

bool ShouldInvalidateEviction(const PrefService* prefs) {
  if (!IsCurrentUserEvicted(prefs))
    return false;

  // Configured error versions are > 0, default stored version is 0.
  int stored_version = prefs->GetInteger(
      password_manager::prefs::
          kUnenrolledFromGoogleMobileServicesWithErrorListVersion);

  return stored_version <
         password_manager::features::kGmsApiErrorListVersion.Get();
}

void ReenrollCurrentUser(PrefService* prefs) {
  DCHECK(prefs->GetBoolean(
      password_manager::prefs::kUnenrolledFromGoogleMobileServicesDueToErrors));

  prefs->ClearPref(
      password_manager::prefs::kUnenrolledFromGoogleMobileServicesDueToErrors);
  prefs->ClearPref(password_manager::prefs::
                       kUnenrolledFromGoogleMobileServicesAfterApiErrorCode);
  prefs->ClearPref(password_manager::prefs::
                       kUnenrolledFromGoogleMobileServicesWithErrorListVersion);
}

}  // namespace password_manager_upm_eviction
