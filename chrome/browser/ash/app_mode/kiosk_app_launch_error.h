// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_KIOSK_APP_LAUNCH_ERROR_H_
#define CHROME_BROWSER_ASH_APP_MODE_KIOSK_APP_LAUNCH_ERROR_H_

#include <string>

#include "base/macros.h"
// TODO(https://crbug.com/1164001): forward declare AuthFailure when migrated
// to ash/components/.
#include "chromeos/login/auth/auth_status_consumer.h"

namespace ash {

class KioskAppLaunchError {
 public:
  // Enum used for UMA. Do NOT reorder or remove entry. Don't forget to
  // update histograms.xml when adding new entries.
  enum class Error {
    kNone = 0,                    // No error.
    kHasPendingLaunch = 1,        // There is a pending launch already.
    kCryptohomedNotRunning = 2,   // Unable to call cryptohome daemon.
    kAlreadyMounted = 3,          // Cryptohome is already mounted.
    kUnableToMount = 4,           // Unable to mount cryptohome.
    kUnableToRemove = 5,          // Unable to remove cryptohome.
    kUnableToInstall = 6,         // Unable to install app.
    kUserCancel = 7,              // Canceled by user.
    kNotKioskEnabled = 8,         // Not a kiosk enabled app.
    kUnableToRetrieveHash = 9,    // Unable to retrieve username hash.
    kPolicyLoadFailed = 10,       // Failed to load policy for kiosk account.
    kUnableToDownload = 11,       // Unable to download app's crx file.
    kUnableToLaunch = 12,         // Unable to launch app.
    kArcAuthFailed = 13,          // Failed to authorise ARC session
    kExtensionsLoadTimeout = 14,  // Timeout is triggered during loading
                                  // force-installed extensions.
    kExtensionsPolicyInvalid =
        15,  // The policy value of ExtensionInstallForcelist is invalid.
    kCount,  // Count of all errors.
  };

  // Returns a message for given |error|.
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

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(KioskAppLaunchError);
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
using ::ash::KioskAppLaunchError;
}

#endif  // CHROME_BROWSER_ASH_APP_MODE_KIOSK_APP_LAUNCH_ERROR_H_
