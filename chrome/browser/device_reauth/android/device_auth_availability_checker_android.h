// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_REAUTH_ANDROID_DEVICE_AUTH_AVAILABILITY_CHECKER_ANDROID_H_
#define CHROME_BROWSER_DEVICE_REAUTH_ANDROID_DEVICE_AUTH_AVAILABILITY_CHECKER_ANDROID_H_

#include <memory>

#include "components/device_reauth/device_auth_availability_checker.h"

namespace device_reauth {
enum class BiometricsAvailability;
}

// Android implementation of `DeviceAuthAvailabilityChecker` that checks
// biometric and screen lock availability via JNI.
class DeviceAuthAvailabilityCheckerAndroid
    : public device_reauth::DeviceAuthAvailabilityChecker {
 public:
  class Bridge {
   public:
    Bridge() = default;
    Bridge(const Bridge&) = delete;
    Bridge& operator=(const Bridge&) = delete;
    virtual ~Bridge() = default;

    virtual device_reauth::BiometricsAvailability
    CanAuthenticateWithBiometric() = 0;
    virtual bool CanAuthenticateWithBiometricOrScreenLock() = 0;
  };

  DeviceAuthAvailabilityCheckerAndroid();
  explicit DeviceAuthAvailabilityCheckerAndroid(std::unique_ptr<Bridge> bridge);
  ~DeviceAuthAvailabilityCheckerAndroid() override;

  // Returns whether biometrics are available for a given device.
  bool CanAuthenticateWithBiometrics() override;

  // Returns whether biometrics or screen lock are available for a given device.
  bool CanAuthenticateWithBiometricOrScreenLock() override;
  device_reauth::BiometricStatus GetBiometricAvailabilityStatus() override;

 private:
  std::unique_ptr<Bridge> bridge_;
};

#endif  // CHROME_BROWSER_DEVICE_REAUTH_ANDROID_DEVICE_AUTH_AVAILABILITY_CHECKER_ANDROID_H_
