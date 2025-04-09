// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_ANDROID_UTIL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_ANDROID_UTIL_H_

#include "chrome/browser/password_manager/android/password_manager_util_bridge_interface.h"
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

// All GMS version categories with regards to UPM support.
enum class GmsVersionCohort {
  kNoGms,
  kNoUpmSupport,
  kOnlyAccountUpmSupport,
  kFullUpmSupport,
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

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(PasswordManagerNotAvailableReason)
enum class PasswordManagerNotAvailableReason {
  // The code wiring the requests to the internal backend is not part of
  // the build. Note: Since this enum is used for metrics, this should never
  // be recorded. Added here for completeness.
  kInternalBackendNotPresent = 0,
  // GmsCore is not available and Google Play Store is not available.
  kNoGmsCore = 1,
  // GmsCore version doesn't support UPM at all, or not fully.
  kOutdatedGmsCore = 2,
  // GmsCore version supports UPM, but there are still unmigrated passwords.
  kAutoExportPending = 3,

  kMaxValue = kAutoExportPending
};

// LINT.ThenChange(/tools/metrics/histograms/metadata/password/enums.xml:PasswordManagerNotAvailableReason)

// Checks whether the password manager can be used on Android.
// Once the login db is deprecated, for clients not fulfilling the criteria
// for talking to the Android backend, the password manager will no longer
// be available.
// The criteria are:
// - access to the internal backend
// - GMS Core version with full UPM support
// - passwords were either migrated or exported
bool IsPasswordManagerAvailable(
    const PrefService* prefs,
    std::unique_ptr<PasswordManagerUtilBridgeInterface> util_bridge);

// As above, except the caller already knows whether the internal backend
// is present, probably because the call originates in Java.
bool IsPasswordManagerAvailable(const PrefService* prefs,
                                bool is_internal_backend_present);

// The login DB is ready to be deprecated when all the passwords have either
// been already migrated to UPM or exported.
//
// Note: This should only be used if looking to identify whether deprecation
// is ongoing or not. For most other  purposes `IsPasswordManagerAvailable` is
// the correct util to check.
bool LoginDbDeprecationReady(PrefService* pref_service);

// Used to prevent static casting issues with
// `PasswordsUseUPMLocalAndSeparateStores` pref.
password_manager::prefs::UseUpmLocalAndSeparateStoresState
GetSplitStoresAndLocalUpmPrefValue(PrefService* pref_service);

// Used to decide whether using UPM as backend is possible. The check is based
// on whether the GMSCore is installed and the internal wiring is present, and
// whether the requirement for the minimum version is met.
// TODO(crbug.com/391829891): This becomes obsolete after login db migration,
// when the only accepted UPM version is the one offering full support.
bool AreMinUpmRequirementsMet();

// Used to decide whether to show the UPM password settings and password check
// UIs or the old pre-UPM UIs. There are 2 cases when this check returns true:
//  - If the user is using UPM and everything works as expected.
//  - If the user is eligible for using UPM, but the GMSCore version is too old
//  and doesn't support UPM.
// TODO(crbug.com/391829891): This should be removed after login db deprecation.
// All the checks will be consolidated in a single util.
bool ShouldUseUpmWiring(const syncer::SyncService* sync_service,
                        const PrefService* pref_service);

// Called on startup to update the value of UsesSplitStoresAndUPMForLocal(),
// based on minimum GmsCore version and other criteria.
void SetUsesSplitStoresAndUPMForLocal(
    PrefService* pref_service,
    const base::FilePath& login_db_directory,
    std::unique_ptr<PasswordManagerUtilBridgeInterface> util_bridge);

// Returns the GMS version type based on which kind of UPM support is possible
// in that version.
GmsVersionCohort GetGmsVersionCohort();

// Returns whether the last attempt to migrate to UPM local failed.
bool LastMigrationAttemptToUpmLocalFailed();

// Checks which type of passwords access loss warning to show to the user if any
// (`kNone` means that no warning will be displayed).
// The order of the checks is the following:
// - If GMS Core is not installed, `kNoGmsCore` is returned.
// - If GMS Core is installed, but has no support for passwords (neither
// account, nor local), `kNoUpm` is returned.
// - If GMS Core is installed and has the version which supports account
// passwords, but doesn't support local passwords, `kOnlyAccountUpm` is
// returned.
// - If there is a local passwords migration pending, then
// `kNewGmsCoreMigrationFailed` is returned.
// - Otherwise no warning should be shown so the function returens `kNone`.
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
