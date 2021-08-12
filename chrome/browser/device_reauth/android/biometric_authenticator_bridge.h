// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_REAUTH_ANDROID_BIOMETRIC_AUTHENTICATOR_BRIDGE_H_
#define CHROME_BROWSER_DEVICE_REAUTH_ANDROID_BIOMETRIC_AUTHENTICATOR_BRIDGE_H_

#include "base/callback_forward.h"
#include "components/device_reauth/biometric_authenticator.h"

namespace device_reauth {

// The biometric authentication result as returned by the biometric prompt.
//
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.device_reauth
enum class BiometricAuthUIResult {
  kSuccessWithUnknownMethod = 0,
  kSuccessWithBiometrics = 1,
  kSuccessWithDeviceLock = 2,
  kCanceledByUser = 3,
  kFailed = 4,
};

}  // namespace device_reauth

// Interface for the biometric authenticator bridge connecting the C++ side
// of the implementation to the Java one.
class BiometricAuthenticatorBridge {
 public:
  virtual ~BiometricAuthenticatorBridge() = default;

  // Checks whether biometrics are available.
  virtual device_reauth::BiometricsAvailability CanAuthenticate() = 0;

  // Trigges an authentication flow based on biometrics, with the
  // screen lock as fallback. Note: this only supports one authentication
  // request at a time.
  virtual void Authenticate(
      base::OnceCallback<void(device_reauth::BiometricAuthUIResult)>
          response_callback) = 0;

  // Called when the authentication flow becomes obsolete (e.g. the original
  // purpose doesn't exist anymore, the tab was destroyed, etc).
  virtual void Cancel() = 0;
};

#endif  // CHROME_BROWSER_DEVICE_REAUTH_ANDROID_BIOMETRIC_AUTHENTICATOR_BRIDGE_H_
