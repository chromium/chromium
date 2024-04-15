// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_manager_eviction_util.h"

#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "chrome/browser/password_manager/android/password_store_android_backend_api_error_codes.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_store/split_stores_and_local_upm.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"

namespace password_manager_upm_eviction {

bool IsCurrentUserEvicted(const PrefService* prefs) {
  if (password_manager::UsesSplitStoresAndUPMForLocal(prefs)) {
    return false;
  }

  // Users without any passwords saved, can use UPM regardless of unenrollment
  // status when `kUnifiedPasswordManagerSyncOnlyInGMSCore` is enabled because
  // there is no re-enrollment.
  if (prefs->GetBoolean(
          password_manager::prefs::kEmptyProfileStoreLoginDatabase) &&
      base::FeatureList::IsEnabled(
          password_manager::features::
              kUnifiedPasswordManagerSyncOnlyInGMSCore)) {
    return false;
  }

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

  // Reset migration prefs so when the user can join the experiment again,
  // non-syncable data and settings can be migrated to GMS Core.
  prefs->SetInteger(
      password_manager::prefs::kCurrentMigrationVersionToGoogleMobileServices,
      0);
  prefs->SetDouble(password_manager::prefs::kTimeOfLastMigrationAttempt, 0.0);

  base::UmaHistogramBoolean("PasswordManager.UnenrolledFromUPMDueToErrors",
                            true);
  base::UmaHistogramSparse("PasswordManager.UPMUnenrollmentReason",
                           api_error_code);
  LOG(ERROR) << "Unenrolled from UPM due to error with code: "
             << api_error_code;
}

void ReenrollCurrentUser(PrefService* prefs) {
  DCHECK(prefs->GetBoolean(
      password_manager::prefs::kUnenrolledFromGoogleMobileServicesDueToErrors));

  prefs->ClearPref(
      password_manager::prefs::kUnenrolledFromGoogleMobileServicesDueToErrors);
  prefs->ClearPref(password_manager::prefs::
                       kUnenrolledFromGoogleMobileServicesAfterApiErrorCode);
  prefs->ClearPref(
      password_manager::prefs::kTimesReenrolledToGoogleMobileServices);
  prefs->ClearPref(
      password_manager::prefs::kTimesAttemptedToReenrollToGoogleMobileServices);
}

}  // namespace password_manager_upm_eviction
