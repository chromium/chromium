// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_REAUTH_WIN_DEVICE_AUTH_AVAILABILITY_CHECKER_WIN_H_
#define CHROME_BROWSER_DEVICE_REAUTH_WIN_DEVICE_AUTH_AVAILABILITY_CHECKER_WIN_H_

#include <memory>

#include "components/device_reauth/device_auth_availability_checker.h"

class AuthenticatorWinInterface;

// Windows implementation of `DeviceAuthAvailabilityChecker` that checks Windows
// Hello biometric and screen lock / PIN availability using
// `AuthenticatorWinInterface`.
class DeviceAuthAvailabilityCheckerWin
    : public device_reauth::DeviceAuthAvailabilityChecker {
 public:
  DeviceAuthAvailabilityCheckerWin();
  explicit DeviceAuthAvailabilityCheckerWin(
      std::unique_ptr<AuthenticatorWinInterface> authenticator);
  ~DeviceAuthAvailabilityCheckerWin() override;

  // Asks Windows if user has configured and enabled biometrics on
  // their machine. Stores the response in a local state pref for future usage,
  // as that check is very expensive. Prefer using the cached value over calling
  // this for every auth attempt.
  static void CacheIfBiometricsAvailable(
      std::unique_ptr<AuthenticatorWinInterface> authenticator);

  // Returns true, when biometrics are available.
  bool CanAuthenticateWithBiometrics() override;

  // Returns true, when biometrics or screen lock is available.
  bool CanAuthenticateWithBiometricOrScreenLock() override;

 private:
  std::unique_ptr<AuthenticatorWinInterface> authenticator_;
};

#endif  // CHROME_BROWSER_DEVICE_REAUTH_WIN_DEVICE_AUTH_AVAILABILITY_CHECKER_WIN_H_
