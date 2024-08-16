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

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.access_loss
enum class PasswordAccessLossWarningType {
  kNone = 0,       // No warning.
  kNoGmsCore = 1,  // A warning that the password manager will stop working.
  kNoUpm = 2,      // A warning that GMS Core is outdated; updated suggested.
  kOnlyAccountUpm = 3,  // A warning that GMSCore is outdated; update suggested.
  kNewGmsCoreMigrationFailed = 4,  // A warning for fixing the migration error.
};

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
// based on minimum GmsCore version and other criteria.
void SetUsesSplitStoresAndUPMForLocal(PrefService* pref_service,
                                      const base::FilePath& login_db_directory);

// This is part of UPM 4.1 implementation. Checks which type of passwords access
// loss warning to show to the user if any (`kNone` means that no warning will
// be displayed). The order of the checks is the following:
// - If there are no passwords in the profile store, no warning is needed.
// - If GMS Core is not installed, `kNoGms` is returned.
// - If GMS Core is installed, but has no support for passwords (neither
// account, nor local), `kOutdatedGms` is returned.
// - If GMS Core is installed and has the version which supports account
// passwords, but doesn't support local passwords, `kNoLocalSupportGms` is
// returned.
// - If there is a local passwords migration pending, then `kMigrationPending`
// is returned.
// - Otherwise no warning is shown.
PasswordAccessLossWarningType GetPasswordAccessLossWarningType(
    PrefService* pref_service);

}  // namespace password_manager_android_util

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_ANDROID_UTIL_H_
