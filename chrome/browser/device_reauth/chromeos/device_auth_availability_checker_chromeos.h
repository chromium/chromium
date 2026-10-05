// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_REAUTH_CHROMEOS_DEVICE_AUTH_AVAILABILITY_CHECKER_CHROMEOS_H_
#define CHROME_BROWSER_DEVICE_REAUTH_CHROMEOS_DEVICE_AUTH_AVAILABILITY_CHECKER_CHROMEOS_H_

#include <memory>

#include "components/device_reauth/device_auth_availability_checker.h"

class AuthenticatorChromeOSInterface;

// ChromeOS implementation of `DeviceAuthAvailabilityChecker` that checks
// biometric (fingerprint) and PIN availability using
// `AuthenticatorChromeOSInterface`.
class DeviceAuthAvailabilityCheckerChromeOS
    : public device_reauth::DeviceAuthAvailabilityChecker {
 public:
  DeviceAuthAvailabilityCheckerChromeOS();
  explicit DeviceAuthAvailabilityCheckerChromeOS(
      std::unique_ptr<AuthenticatorChromeOSInterface> authenticator);
  ~DeviceAuthAvailabilityCheckerChromeOS() override;

  // Returns whether biometrics are available for a given device.
  bool CanAuthenticateWithBiometrics() override;

  // Returns whether biometrics or screen lock are available for a given device.
  bool CanAuthenticateWithBiometricOrScreenLock() override;

 private:
  std::unique_ptr<AuthenticatorChromeOSInterface> authenticator_;
};

#endif  // CHROME_BROWSER_DEVICE_REAUTH_CHROMEOS_DEVICE_AUTH_AVAILABILITY_CHECKER_CHROMEOS_H_
