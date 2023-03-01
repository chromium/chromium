// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_REAUTH_ANDROID_DEVICE_AUTHENTICATOR_ANDROID_H_
#define CHROME_BROWSER_DEVICE_REAUTH_ANDROID_DEVICE_AUTHENTICATOR_ANDROID_H_

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "chrome/browser/device_reauth/android/device_authenticator_bridge.h"
#include "chrome/browser/device_reauth/chrome_device_authenticator_common.h"
#include "chrome/browser/device_reauth/chrome_device_authenticator_factory.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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
class DeviceAuthenticatorAndroid : public ChromeDeviceAuthenticatorCommon {
 public:
  bool CanAuthenticateWithBiometrics() override;

  bool CanAuthenticateWithBiometricOrScreenLock() override;

  // Trigges an authentication flow based on biometrics, with the
  // screen lock as fallback. Note: this only supports one authentication
  // request at a time.
  // |use_last_valid_auth| if set to false, ignores the grace 60 seconds
  // period between the last valid authentication and the current
  // authentication, and re-invokes system authentication.
  void Authenticate(device_reauth::DeviceAuthRequester requester,
                    AuthenticateCallback callback,
                    bool use_last_valid_auth) override;

  // Trigges an authentication flow based on biometrics, with the
  // screen lock as fallback. Displays `message` in the authentication UI.
  // Note: this only supports one authentication request at a time.
  void AuthenticateWithMessage(const std::u16string& message,
                               AuthenticateCallback callback) override;

  // Should be called by the object using the authenticator if the purpose
  // for which the auth was requested becomes obsolete or the object is
  // destroyed.
  void Cancel(device_reauth::DeviceAuthRequester requester) override;

  // Creates an instance of DeviceAuthenticatorAndroid for testing purposes
  // only.
  static scoped_refptr<DeviceAuthenticatorAndroid> CreateForTesting(
      std::unique_ptr<DeviceAuthenticatorBridge> bridge);

 private:
  friend class ChromeDeviceAuthenticatorFactory;

  explicit DeviceAuthenticatorAndroid(
      std::unique_ptr<DeviceAuthenticatorBridge> bridge);
  ~DeviceAuthenticatorAndroid() override;

  // Called when the authentication compeletes with the result
  void OnAuthenticationCompleted(device_reauth::DeviceAuthUIResult ui_result);

  // Callback to be executed after the authentication completes.
  AuthenticateCallback callback_;

  // Enum value representing the filling surface that has requested the current
  // authentication.
  absl::optional<device_reauth::DeviceAuthRequester> requester_;

  // Bridge used to call into the Java side.
  std::unique_ptr<DeviceAuthenticatorBridge> bridge_;
};

#endif  // CHROME_BROWSER_DEVICE_REAUTH_ANDROID_DEVICE_AUTHENTICATOR_ANDROID_H_
