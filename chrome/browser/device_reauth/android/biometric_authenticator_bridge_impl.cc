// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/android/biometric_authenticator_bridge_impl.h"

#include "base/android/jni_android.h"
#include "base/callback.h"
#include "chrome/browser/device_reauth/android/biometric_authenticator_android.h"
#include "chrome/browser/device_reauth/android/jni_headers/BiometricAuthenticatorBridge_jni.h"
#include "components/device_reauth/biometric_authenticator.h"

using base::android::AttachCurrentThread;
using device_reauth::BiometricAuthUIResult;
using device_reauth::BiometricsAvailability;

BiometricAuthenticatorBridgeImpl::BiometricAuthenticatorBridgeImpl() {
  java_object_ = Java_BiometricAuthenticatorBridge_create(
      AttachCurrentThread(), reinterpret_cast<intptr_t>(this));
}

BiometricAuthenticatorBridgeImpl::~BiometricAuthenticatorBridgeImpl() {
  Java_BiometricAuthenticatorBridge_destroy(AttachCurrentThread(),
                                            java_object_);
}

BiometricsAvailability
BiometricAuthenticatorBridgeImpl::CanAuthenticateWithBiometric() {
  return static_cast<BiometricsAvailability>(
      Java_BiometricAuthenticatorBridge_canAuthenticateWithBiometric(
          AttachCurrentThread(), java_object_));
}

bool BiometricAuthenticatorBridgeImpl::
    CanAuthenticateWithBiometricOrScreenLock() {
  return Java_BiometricAuthenticatorBridge_canAuthenticateWithBiometricOrScreenLock(
      AttachCurrentThread(), java_object_);
}

void BiometricAuthenticatorBridgeImpl::Authenticate(
    base::OnceCallback<void(device_reauth::BiometricAuthUIResult)>
        response_callback) {
  response_callback_ = std::move(response_callback);
  Java_BiometricAuthenticatorBridge_authenticate(AttachCurrentThread(),
                                                 java_object_);
}

void BiometricAuthenticatorBridgeImpl::Cancel() {
  Java_BiometricAuthenticatorBridge_cancel(AttachCurrentThread(), java_object_);
}

void BiometricAuthenticatorBridgeImpl::OnAuthenticationCompleted(JNIEnv* env,
                                                                 jint result) {
  if (!response_callback_)
    return;
  std::move(response_callback_).Run(static_cast<BiometricAuthUIResult>(result));
}
