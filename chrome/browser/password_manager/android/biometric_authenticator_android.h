// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_BIOMETRIC_AUTHENTICATOR_ANDROID_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_BIOMETRIC_AUTHENTICATOR_ANDROID_H_

#include "base/callback.h"
#include "base/time/time.h"
#include "chrome/browser/password_manager/android/biometric_authenticator_bridge.h"
#include "chrome/browser/password_manager/chrome_biometric_authenticator.h"
#include "components/password_manager/core/browser/biometric_authenticator.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// Android implementation of the BiometricAuthenticator interface.
class BiometricAuthenticatorAndroid : public ChromeBiometricAuthenticator {
 public:
  explicit BiometricAuthenticatorAndroid(
      std::unique_ptr<BiometricAuthenticatorBridge> bridge);

  // Checks whether biometrics are available.
  password_manager::BiometricsAvailability CanAuthenticate() override;

  // Trigges an authentication flow based on biometrics, with the
  // screen lock as fallback. Note: this only supports one authentication
  // request at a time.
  void Authenticate(password_manager::BiometricAuthRequester requester,
                    AuthenticateCallback callback) override;

  // Should be called by the object using the authenticator if the purpose
  // for which the auth was requested becomes obsolete or the object is
  // destroyed.
  void Cancel(password_manager::BiometricAuthRequester requester) override;

 private:
  ~BiometricAuthenticatorAndroid() override;

  // Called when the authentication compeletes with the result
  void OnAuthenticationCompleted(
      password_manager::BiometricAuthUIResult ui_result);

  // Time of last successful re-auth. nullopt if there hasn't been an auth yet.
  absl::optional<base::TimeTicks> last_good_auth_timestamp_;

  // Callback to be executed after the authentication completes.
  AuthenticateCallback callback_;

  // Enum value representing the filling surface that has requested the current
  // authentication.
  absl::optional<password_manager::BiometricAuthRequester> requester_;

  // Bridge used to call into the Java side.
  std::unique_ptr<BiometricAuthenticatorBridge> bridge_;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_BIOMETRIC_AUTHENTICATOR_ANDROID_H_
