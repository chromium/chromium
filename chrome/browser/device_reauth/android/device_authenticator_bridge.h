// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_REAUTH_ANDROID_DEVICE_AUTHENTICATOR_BRIDGE_H_
#define CHROME_BROWSER_DEVICE_REAUTH_ANDROID_DEVICE_AUTHENTICATOR_BRIDGE_H_

#include "base/functional/callback_forward.h"
#include "components/device_reauth/device_authenticator.h"

namespace device_reauth {

// The biometric authentication result as returned by the biometric prompt.
//
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.device_reauth
enum class DeviceAuthUIResult {
  kSuccessWithUnknownMethod = 0,
  kSuccessWithBiometrics = 1,
  kSuccessWithDeviceLock = 2,
  kCanceledByUser = 3,
  kFailed = 4,
  kLockout = 5,
};

// Different states for biometric availability for a given device. Either no
// biometric hardware is available, hardware is available but the user has no
// biometrics enrolled, or hardware is available and the user makes use of it.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.device_reauth
enum class BiometricsAvailability {
  kOtherError = 0,
  kAvailable = 1,
  kAvailableNoFallback = 2,
  kNoHardware = 3,
  kHwUnavailable = 4,
  kNotEnrolled = 5,
  kSecurityUpdateRequired = 6,
  kAndroidVersionNotSupported = 7,
  kRequired = 8,
  kRequiredButHasError = 9,

  kMaxValue = kRequiredButHasError,
};

}  // namespace device_reauth

// Interface for the biometric authenticator bridge connecting the C++ side
// of the implementation to the Java one.
class DeviceAuthenticatorBridge {
 public:
  virtual ~DeviceAuthenticatorBridge() = default;

  // Checks whether biometrics are available.
  virtual device_reauth::BiometricsAvailability
  CanAuthenticateWithBiometric() = 0;

  // Checks whether biometrics OR screen lock are available.
  virtual bool CanAuthenticateWithBiometricOrScreenLock() = 0;

  // Trigges an authentication flow based on biometrics, with the
  // screen lock as fallback. Note: this only supports one authentication
  // request at a time.
  virtual void Authenticate(
      base::OnceCallback<void(device_reauth::DeviceAuthUIResult)>
          response_callback) = 0;

  // Called when the authentication flow becomes obsolete (e.g. the original
  // purpose doesn't exist anymore, the tab was destroyed, etc).
  virtual void Cancel() = 0;
};

#endif  // CHROME_BROWSER_DEVICE_REAUTH_ANDROID_DEVICE_AUTHENTICATOR_BRIDGE_H_
