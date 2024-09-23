// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_manager_eviction_util.h"

#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "chrome/browser/password_manager/android/password_store_android_backend_api_error_codes.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/split_stores_and_local_upm.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"

namespace password_manager_upm_eviction {

bool IsCurrentUserEvicted(const PrefService* prefs) {
  if (password_manager::UsesSplitStoresAndUPMForLocal(prefs)) {
    return false;
  }

  // Users without any passwords saved, can use UPM regardless of unenrollment
  // status because there is no re-enrollment after M4.
  if (prefs->GetBoolean(
          password_manager::prefs::kEmptyProfileStoreLoginDatabase)) {
    return false;
  }

  return prefs->GetBoolean(
      password_manager::prefs::kUnenrolledFromGoogleMobileServicesDueToErrors);
}

}  // namespace password_manager_upm_eviction
