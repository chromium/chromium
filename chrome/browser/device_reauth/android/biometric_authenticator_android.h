// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_REAUTH_ANDROID_BIOMETRIC_AUTHENTICATOR_ANDROID_H_
#define CHROME_BROWSER_DEVICE_REAUTH_ANDROID_BIOMETRIC_AUTHENTICATOR_ANDROID_H_

#include "base/callback.h"
#include "base/time/time.h"
#include "chrome/browser/device_reauth/android/biometric_authenticator_bridge.h"
#include "chrome/browser/device_reauth/chrome_biometric_authenticator_factory.h"
#include "components/device_reauth/biometric_authenticator.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// Android implementation of the BiometricAuthenticator interface.
class BiometricAuthenticatorAndroid
    : public device_reauth::BiometricAuthenticator {
 public:
  // Checks whether biometrics are available.
  device_reauth::BiometricsAvailability CanAuthenticate(
      device_reauth::BiometricAuthRequester requester) override;

  // Trigges an authentication flow based on biometrics, with the
  // screen lock as fallback. Note: this only supports one authentication
  // request at a time.
  void Authenticate(device_reauth::BiometricAuthRequester requester,
                    AuthenticateCallback callback) override;

  // Should be called by the object using the authenticator if the purpose
  // for which the auth was requested becomes obsolete or the object is
  // destroyed.
  void Cancel(device_reauth::BiometricAuthRequester requester) override;

  // Creates an instance of BiometricAuthenticatorAndroid for testing purposes
  // only.
  static scoped_refptr<BiometricAuthenticatorAndroid> CreateForTesting(
      std::unique_ptr<BiometricAuthenticatorBridge> bridge);

 private:
  friend class BiometricAuthenticatorAndroidFactory;

  explicit BiometricAuthenticatorAndroid(
      std::unique_ptr<BiometricAuthenticatorBridge> bridge);
  ~BiometricAuthenticatorAndroid() override;

  // Called when the authentication compeletes with the result
  void OnAuthenticationCompleted(
      device_reauth::BiometricAuthUIResult ui_result);

  // Time of last successful re-auth. nullopt if there hasn't been an auth yet.
  absl::optional<base::TimeTicks> last_good_auth_timestamp_;

  // Callback to be executed after the authentication completes.
  AuthenticateCallback callback_;

  // Enum value representing the filling surface that has requested the current
  // authentication.
  absl::optional<device_reauth::BiometricAuthRequester> requester_;

  // Bridge used to call into the Java side.
  std::unique_ptr<BiometricAuthenticatorBridge> bridge_;
};

#endif  // CHROME_BROWSER_DEVICE_REAUTH_ANDROID_BIOMETRIC_AUTHENTICATOR_ANDROID_H_
