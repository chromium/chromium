// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_ANDROID_UTIL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_ANDROID_UTIL_H_

#include "components/password_manager/core/common/password_manager_pref_names.h"

class PrefService;

namespace base {
class FilePath;
}  // namespace base

namespace syncer {
class SyncService;
}  // namespace syncer

namespace password_manager_android_util {

// Used to prevent static casting issues with
// `PasswordsUseUPMLocalAndSeparateStores` pref.
password_manager::prefs::UseUpmLocalAndSeparateStoresState
GetSplitStoresAndLocalUpmPrefValue(PrefService* pref_service);

// Used to decide whether using UPM as backend is possible. The check is based
// on whether the GMSCore is installed and the internal wiring is present, and
// whether the requirement for the minimum version is met.
bool AreMinUpmRequirementsMet();

// Used to decide whether to show the UPM password settings and password check
// UIs or the old pre-UPM UIs. There are 2 cases when this check returns true:
//  - If the user is using UPM and everything works as expected.
//  - If the user is eligible for using UPM, but the GMSCore version is too old
//  and doesn't support UPM.
bool ShouldUseUpmWiring(const syncer::SyncService* sync_service,
                        const PrefService* pref_service);

// Called on startup to update the value of UsesSplitStoresAndUPMForLocal(),
// based on feature flags, minimum GmsCore version and other criteria.
// If switches::kSkipLocalUpmGmsCoreVersionCheckForTesting is added to the
// command-line, the GmsCore version check will be skipped.
void SetUsesSplitStoresAndUPMForLocal(PrefService* pref_service,
                                      const base::FilePath& login_db_directory);

}  // namespace password_manager_android_util

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_ANDROID_UTIL_H_
