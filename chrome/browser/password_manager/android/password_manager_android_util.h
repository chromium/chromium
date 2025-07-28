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

namespace password_manager_android_util {

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

// The login database is deprecated on Android. This function deletes the data
// if the user already exported any leftover data.
void MaybeDeleteLoginDatabases(
    PrefService* pref_service,
    const base::FilePath& login_db_directory,
    std::unique_ptr<PasswordManagerUtilBridgeInterface> util_bridge);

}  // namespace password_manager_android_util

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_ANDROID_UTIL_H_
