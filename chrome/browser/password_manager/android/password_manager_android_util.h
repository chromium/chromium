// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_ANDROID_UTIL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_ANDROID_UTIL_H_

#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"

class PrefService;

namespace base {
class FilePath;
}  // namespace base

namespace syncer {
class SyncService;
}  // namespace syncer

namespace password_manager_android_util {

// Represents different types of password access loss warning.
//
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.access_loss
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Always keep this enum in sync with the
// corresponding PasswordAccessLossWarningTriggers in enums.xml.
enum class PasswordAccessLossWarningType {
  kNone = 0,       // No warning.
  kNoGmsCore = 1,  // A warning that the password manager will stop working.
  kNoUpm = 2,      // A warning that GMS Core is outdated; updated suggested.
  kOnlyAccountUpm = 3,  // A warning that GMSCore is outdated; update suggested.
  kNewGmsCoreMigrationFailed = 4,  // A warning for fixing the migration error.
  kMaxValue = kNewGmsCoreMigrationFailed,
};

// Represents different causes for showing the password access loss warning.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Always keep this enum in sync with the
// corresponding PasswordAccessLossWarningTriggers in enums.xml.
enum class PasswordAccessLossWarningTriggers {
  kChromeStartup = 0,
  kPasswordSaveUpdateMessage = 1,
  kTouchToFill = 2,
  kKeyboardAcessorySheet = 3,
  kKeyboardAcessoryBar = 4,
  kAllPasswords = 5,
  kMaxValue = kAllPasswords,
};

// Represents different actions that the user can take on the password access
// loss warning.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Always keep this enum in sync with the
// corresponding PasswordAccessLossWarningUserActions in enums.xml.
enum class PasswordAccessLossWarningUserActions {
  kMainAction = 0,
  kHelpCenter = 1,
  kDismissed = 2,
  kMaxValue = kDismissed,
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

// Records the histogram that tracks when the password access loss warning was
// shown.
// TODO: crbug.com/369076084 - Clean-up when the warning UI is no longer used.
void RecordPasswordAccessLossWarningTriggerSource(
    PasswordAccessLossWarningTriggers trigger_source,
    PasswordAccessLossWarningType warning_type);

}  // namespace password_manager_android_util

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_ANDROID_UTIL_H_
