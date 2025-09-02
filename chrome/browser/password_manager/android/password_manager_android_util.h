// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_ANDROID_UTIL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_ANDROID_UTIL_H_

#include <memory>
#include <string_view>

#include "chrome/browser/password_manager/android/password_manager_util_bridge_interface.h"

class PrefService;

namespace base {
class FilePath;
}  // namespace base

namespace password_manager_android_util {

inline constexpr std::string_view kExportedPasswordsFileName =
    "ChromePasswords.csv";

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
  // Deprecated: kAutoExportPending = 3,

  kMaxValue = kOutdatedGmsCore
};

// LINT.ThenChange(/tools/metrics/histograms/metadata/password/enums.xml:PasswordManagerNotAvailableReason)

// Checks whether the password manager can be used on Android.
// The criteria are:
// - access to the internal backend
// - GMS Core version with full UPM support
bool IsPasswordManagerAvailable(
    std::unique_ptr<PasswordManagerUtilBridgeInterface> util_bridge);

// As above, except the caller already knows whether the internal backend
// is present, probably because the call originates in Java.
bool IsPasswordManagerAvailable(bool is_internal_backend_present);

// The login database is deprecated on Android. This function deletes the data.
void MaybeDeleteLoginDatabases(
    PrefService* pref_service,
    const base::FilePath& login_db_directory,
    std::unique_ptr<PasswordManagerUtilBridgeInterface> util_bridge);

}  // namespace password_manager_android_util

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_ANDROID_UTIL_H_
