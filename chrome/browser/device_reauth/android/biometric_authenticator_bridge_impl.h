// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_REAUTH_ANDROID_BIOMETRIC_AUTHENTICATOR_BRIDGE_IMPL_H_
#define CHROME_BROWSER_DEVICE_REAUTH_ANDROID_BIOMETRIC_AUTHENTICATOR_BRIDGE_IMPL_H_

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/device_reauth/android/biometric_authenticator_android.h"

class BiometricAuthenticatorBridgeImpl : public BiometricAuthenticatorBridge {
 public:
  BiometricAuthenticatorBridgeImpl();
  ~BiometricAuthenticatorBridgeImpl() override;

  BiometricAuthenticatorBridgeImpl(const BiometricAuthenticatorBridgeImpl&) =
      delete;
  BiometricAuthenticatorBridgeImpl& operator=(
      const BiometricAuthenticatorBridgeImpl&) = delete;
  BiometricAuthenticatorBridgeImpl(const BiometricAuthenticatorBridgeImpl&&) =
      delete;
  BiometricAuthenticatorBridgeImpl&& operator=(
      const BiometricAuthenticatorBridgeImpl&&) = delete;

  device_reauth::BiometricsAvailability CanAuthenticateWithBiometric() override;
  bool CanAuthenticateWithBiometricOrScreenLock() override;

  // Starts the authentication.
  void Authenticate(
      base::OnceCallback<void(device_reauth::BiometricAuthUIResult)>
          response_callback) override;

  // Cancels the ongoing authentication.
  void Cancel() override;

  // Called by Java when the authentication completes with the `result`.
  void OnAuthenticationCompleted(JNIEnv* env, jint result);

 private:
  // Called when the authentication completes.
  base::OnceCallback<void(device_reauth::BiometricAuthUIResult)>
      response_callback_;

  // This object is an instance of BiometricAuthenticatorBridge, i.e. the Java
  // counterpart to this class.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
};

#endif  // CHROME_BROWSER_DEVICE_REAUTH_ANDROID_BIOMETRIC_AUTHENTICATOR_BRIDGE_IMPL_H_
