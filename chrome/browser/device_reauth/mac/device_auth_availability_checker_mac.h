// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_REAUTH_MAC_DEVICE_AUTH_AVAILABILITY_CHECKER_MAC_H_
#define CHROME_BROWSER_DEVICE_REAUTH_MAC_DEVICE_AUTH_AVAILABILITY_CHECKER_MAC_H_

#include <memory>

#include "components/device_reauth/device_auth_availability_checker.h"

class AuthenticatorMacInterface;

// Mac implementation of `DeviceAuthAvailabilityChecker` that checks Touch ID
// and password/screen lock availability using `AuthenticatorMacInterface`.
class DeviceAuthAvailabilityCheckerMac
    : public device_reauth::DeviceAuthAvailabilityChecker {
 public:
  DeviceAuthAvailabilityCheckerMac();
  explicit DeviceAuthAvailabilityCheckerMac(
      std::unique_ptr<AuthenticatorMacInterface> authenticator);
  ~DeviceAuthAvailabilityCheckerMac() override;

  // Returns whether biometrics are available for a given device.
  bool CanAuthenticateWithBiometrics() override;

  // Returns whether biometrics or screen lock are available for a given device.
  bool CanAuthenticateWithBiometricOrScreenLock() override;

 private:
  std::unique_ptr<AuthenticatorMacInterface> authenticator_;
};

#endif  // CHROME_BROWSER_DEVICE_REAUTH_MAC_DEVICE_AUTH_AVAILABILITY_CHECKER_MAC_H_
