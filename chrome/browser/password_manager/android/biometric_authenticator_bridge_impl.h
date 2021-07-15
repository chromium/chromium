// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_BIOMETRIC_AUTHENTICATOR_BRIDGE_IMPL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_BIOMETRIC_AUTHENTICATOR_BRIDGE_IMPL_H_

#include "base/callback_forward.h"
#include "chrome/browser/password_manager/android/biometric_authenticator_android.h"
#include "ui/android/window_android.h"

class BiometricAuthenticatorBridgeImpl : public BiometricAuthenticatorBridge {
 public:
  explicit BiometricAuthenticatorBridgeImpl(ui::WindowAndroid* controller);
  ~BiometricAuthenticatorBridgeImpl() override;

  BiometricAuthenticatorBridgeImpl(const BiometricAuthenticatorBridgeImpl&) =
      delete;
  BiometricAuthenticatorBridgeImpl& operator=(
      const BiometricAuthenticatorBridgeImpl&) = delete;
  BiometricAuthenticatorBridgeImpl(const BiometricAuthenticatorBridgeImpl&&) =
      delete;
  BiometricAuthenticatorBridgeImpl&& operator=(
      const BiometricAuthenticatorBridgeImpl&&) = delete;

  password_manager::BiometricsAvailability CanAuthenticate() override;

  // Starts the authentication.
  void Authenticate(
      base::OnceCallback<void(password_manager::BiometricAuthUIResult)>
          response_callback) override;

  // Cancels the ongoing authentication.
  void Cancel() override;

  // Called by Java when the authentication completes with the `result`.
  void OnAuthenticationCompleted(JNIEnv* env, jint result);

 private:
  // Called when the authentication completes.
  base::OnceCallback<void(password_manager::BiometricAuthUIResult)>
      response_callback_;

  // This object is an instance of BiometricAuthenticatorBridge, i.e. the Java
  // counterpart to this class.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_BIOMETRIC_AUTHENTICATOR_BRIDGE_IMPL_H_
