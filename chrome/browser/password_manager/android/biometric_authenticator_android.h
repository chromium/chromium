// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_BIOMETRIC_AUTHENTICATOR_ANDROID_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_BIOMETRIC_AUTHENTICATOR_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "base/time/time.h"
#include "chrome/browser/password_manager/chrome_biometric_authenticator.h"
#include "components/password_manager/core/browser/biometric_authenticator.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#include "ui/android/window_android.h"

namespace password_manager {

// The biometric authentication result as returned by the biometric prompt.
//
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.password_manager
enum class BiometricAuthUIResult {
  kSuccessWithUnknownMethod = 0,
  kSuccessWithBiometrics = 1,
  kSuccessWithDeviceLock = 2,
  kCanceledByUser = 3,
  kFailed = 4,
};

}  // namespace password_manager

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
  void OnAuthenticationCompleted(JNIEnv* env, jint result);

 private:
  ~BiometricAuthenticatorAndroid() override;

  // Time of last successful re-auth. nullopt if there hasn't been an auth yet.
  absl::optional<base::TimeTicks> last_good_auth_timestamp_;

  // Callback to be executed after the authentication completes.
  AuthenticateCallback callback_;

  // Enum value representing the filling surface that has requested the current
  // authentication.
  absl::optional<password_manager::BiometricAuthRequester> requester_;

  // This object is an instance of BiometricAuthenticatorBridge, i.e. the Java
  // counterpart to this class.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_BIOMETRIC_AUTHENTICATOR_ANDROID_H_
