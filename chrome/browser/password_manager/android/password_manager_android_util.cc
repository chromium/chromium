// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_manager_android_util.h"

#include "base/feature_list.h"
#include "chrome/browser/password_manager/android/password_manager_eviction_util.h"
#include "components/password_manager/core/browser/features/password_features.h"

namespace password_manager_android_util {

bool UsesSplitStoresAndUPMForLocal(PrefService* pref_service) {
  bool is_upm_local_enabled = base::FeatureList::IsEnabled(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration);
  // TODO(crbug.com/1495626): Replace the flag check with the readiness pref
  // check.
  return is_upm_local_enabled;
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

}  // namespace password_manager_android_util
