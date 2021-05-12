// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_BIOMETRIC_AUTHENTICATOR_ANDROID_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_BIOMETRIC_AUTHENTICATOR_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "base/optional.h"
#include "chrome/browser/password_manager/chrome_biometric_authenticator.h"
#include "components/password_manager/core/browser/biometric_authenticator.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "ui/android/window_android.h"

// Android implementation of the BiometricAuthenticator interface.
class BiometricAuthenticatorAndroid : public ChromeBiometricAuthenticator {
 public:
  explicit BiometricAuthenticatorAndroid(ui::WindowAndroid* window_android);

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

  // Called by Java when the authentication completes.
  void OnAuthenticationCompleted(JNIEnv* env, jboolean success);

 private:
  ~BiometricAuthenticatorAndroid() override;

  // Callback to be executed after the authentication completes.
  AuthenticateCallback callback_;

  // Enum value representing the filling surface that has requested the current
  // authentication.
  base::Optional<password_manager::BiometricAuthRequester> requester_;

  // This object is an instance of BiometricAuthenticatorBridge, i.e. the Java
  // counterpart to this class.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_BIOMETRIC_AUTHENTICATOR_ANDROID_H_
