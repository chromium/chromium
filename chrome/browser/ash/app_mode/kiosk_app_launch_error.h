// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_KIOSK_APP_LAUNCH_ERROR_H_
#define CHROME_BROWSER_ASH_APP_MODE_KIOSK_APP_LAUNCH_ERROR_H_

#include <string>

namespace ash {

class AuthFailure;

class KioskAppLaunchError {
 public:
  // Enum used for UMA. Do NOT reorder or remove entries.
  //
  // When adding new entries remember to update `kMaxValue` and
  // `KioskLaunchError` in tools/metrics/histograms/enums.xml.
  enum class Error {
    kNone = 0,                   // No error.
    kHasPendingLaunch = 1,       // There is a pending launch already.
    kCryptohomedNotRunning = 2,  // Unable to call cryptohome daemon.
    kAlreadyMounted = 3,         // Cryptohome is already mounted.
    kUnableToMount = 4,          // Unable to mount cryptohome.
    kUnableToRemove = 5,         // Unable to remove cryptohome.
    kUnableToInstall = 6,        // Unable to install app.
    kUserCancel = 7,             // Canceled by user.
    kNotKioskEnabled = 8,        // Not a kiosk enabled app.
    kUnableToRetrieveHash = 9,   // Unable to retrieve username hash.
    kPolicyLoadFailed = 10,      // Failed to load policy for kiosk account.
    kUnableToDownload = 11,      // Unable to download app's crx file.
    kUnableToLaunch = 12,        // Unable to launch app.
    // kArcAuthFailed = 13,       // Deprecated
    kExtensionsLoadTimeout = 14,  // Timeout is triggered during loading.
                                  // force-installed extensions.
    kExtensionsPolicyInvalid =
        15,  // The policy value of ExtensionInstallForcelist is invalid.
    kUserNotAllowlisted = 16,  // LoginPerformer disallowed this user.
    // kLacrosDataMigrationStarted = 17,  // Deprecated
    // kLacrosBackwardDataMigrationStarted = 18,  // Deprecated
    kMaxValue = kUserNotAllowlisted,  // Max value of errors.
  };

  // Returns a message for given `error`.
  static std::string GetErrorMessage(Error error);

  // Saves a launch error. The error is used on the next Chrome run to report
  // metrics and display a message to the user.
  static void Save(Error error);

  // Saves a cryptohome auth error. The error is used for metrics report on the
  // next Chrome run.
  static void SaveCryptohomeFailure(const AuthFailure& auth_failure);

  // Gets the last launch error.
  static Error Get();

  // Records the launch error and cryptohome auth error metric and clears them.
  static void RecordMetricAndClear();

  KioskAppLaunchError() = delete;
  KioskAppLaunchError(const KioskAppLaunchError&) = delete;
  KioskAppLaunchError& operator=(const KioskAppLaunchError&) = delete;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_KIOSK_APP_LAUNCH_ERROR_H_
