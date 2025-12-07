// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_REAUTH_ANDROID_DEVICE_AUTHENTICATOR_ANDROID_H_
#define CHROME_BROWSER_DEVICE_REAUTH_ANDROID_DEVICE_AUTHENTICATOR_ANDROID_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "chrome/browser/device_reauth/android/device_authenticator_bridge.h"
#include "chrome/browser/device_reauth/chrome_device_authenticator_factory.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/device_reauth/device_authenticator_common.h"
#include "components/password_manager/core/browser/origin_credential_store.h"

// The result of the device authentication.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class DeviceAuthFinalResult {
  // This value is used for when we don't know the exact auth method used. This
  // can be the case on Android versions under 11.
  kSuccessWithUnknownMethod = 0,
  kSuccessWithBiometrics = 1,
  kSuccessWithDeviceLock = 2,
  kCanceledByUser = 3,
  kFailed = 4,

  // Deprecated in favour of kCanceledByChrome. Recorded when the auth succeeds
  // after Chrome cancelled it.
  // kSuccessButCanceled = 5,

  // Deprecated in favour of kCanceledByChrome. Recorded when the auth fails
  // after Chrome cancelled it.
  // kFailedAndCanceled = 6,

  // Recorded if an authentication was requested within 60s of the previous
  // successful authentication.
  kAuthStillValid = 7,

  // Recorded when the authentication flow is cancelled by Chrome.
  kCanceledByChrome = 8,

  kMaxValue = kCanceledByChrome,
};

// Android implementation of the DeviceAuthenticator interface.
class DeviceAuthenticatorAndroid : public DeviceAuthenticatorCommon {
 public:
  DeviceAuthenticatorAndroid(std::unique_ptr<DeviceAuthenticatorBridge> bridge,
                             DeviceAuthenticatorProxy* proxy,
                             const device_reauth::DeviceAuthParams& params);
  ~DeviceAuthenticatorAndroid() override;

  bool CanAuthenticateWithBiometrics() override;

  bool CanAuthenticateWithBiometricOrScreenLock() override;

  // Trigges an authentication flow based on biometrics, with the
  // screen lock as fallback. Displays `message` in the authentication UI.
  // Note: this only supports one authentication request at a time.
  // On Android `message` is not relevant, can be empty.
  void AuthenticateWithMessage(const std::u16string& message,
                               AuthenticateCallback callback) override;

  device_reauth::BiometricStatus GetBiometricAvailabilityStatus() override;

  // Should be called by the object using the authenticator if the purpose
  // for which the auth was requested becomes obsolete or the object is
  // destroyed.
  void Cancel() override;

 private:
  // Called when the authentication compeletes with the result
  void OnAuthenticationCompleted(device_reauth::DeviceAuthUIResult ui_result);

  // Callback to be executed after the authentication completes.
  AuthenticateCallback callback_;

  // Bridge used to call into the Java side.
  std::unique_ptr<DeviceAuthenticatorBridge> bridge_;

  // Enum value representing where the device reauthentication flow is requested
  // from.
  device_reauth::DeviceAuthSource source_;
};

#endif  // CHROME_BROWSER_DEVICE_REAUTH_ANDROID_DEVICE_AUTHENTICATOR_ANDROID_H_
