// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_LAUNCH_ERROR_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_LAUNCH_ERROR_H_

#include <string>

#include "base/macros.h"

namespace chromeos {

class AuthFailure;

class KioskAppLaunchError {
 public:
  // Enum used for UMA. Do NOT reorder or remove entry. Don't forget to
  // update histograms.xml when adding new entries.
  enum Error {
    NONE = 0,                     // No error.
    HAS_PENDING_LAUNCH = 1,       // There is a pending launch already.
    CRYPTOHOMED_NOT_RUNNING = 2,  // Unable to call cryptohome daemon.
    ALREADY_MOUNTED = 3,          // Cryptohome is already mounted.
    UNABLE_TO_MOUNT = 4,          // Unable to mount cryptohome.
    UNABLE_TO_REMOVE = 5,         // Unable to remove cryptohome.
    UNABLE_TO_INSTALL = 6,        // Unable to install app.
    USER_CANCEL = 7,              // Canceled by user.
    NOT_KIOSK_ENABLED = 8,        // Not a kiosk enabled app.
    UNABLE_TO_RETRIEVE_HASH = 9,  // Unable to retrieve username hash.
    POLICY_LOAD_FAILED = 10,      // Failed to load policy for kiosk account.
    UNABLE_TO_DOWNLOAD = 11,      // Unable to download app's crx file.
    UNABLE_TO_LAUNCH = 12,        // Unable to launch app.
    ARC_AUTH_FAILED = 13,         // Failed to authorise ARC session
    ERROR_COUNT,                  // Count of all errors.
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

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_LAUNCH_ERROR_H_
