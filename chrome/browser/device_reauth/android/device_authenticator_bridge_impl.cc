// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/android/device_authenticator_bridge_impl.h"

#include "base/android/jni_android.h"
#include "base/functional/callback.h"
#include "chrome/browser/device_reauth/android/device_authenticator_android.h"
#include "components/device_reauth/device_authenticator.h"
#include "ui/android/window_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/device_reauth/android/jni_headers/DeviceAuthenticatorBridge_jni.h"

using base::android::AttachCurrentThread;
using device_reauth::BiometricsAvailability;
using device_reauth::DeviceAuthUIResult;

DeviceAuthenticatorBridgeImpl::DeviceAuthenticatorBridgeImpl(
    const gfx::NativeWindow window) {
  java_object_ = Java_DeviceAuthenticatorBridge_createForWindow(
      AttachCurrentThread(), reinterpret_cast<intptr_t>(this),
      window != nullptr ? window->GetJavaObject() : nullptr);
}

DeviceAuthenticatorBridgeImpl::DeviceAuthenticatorBridgeImpl(
    const base::android::JavaParamRef<jobject>& activity) {
  java_object_ = Java_DeviceAuthenticatorBridge_createForActivity(
      AttachCurrentThread(), reinterpret_cast<intptr_t>(this), activity);
}

DeviceAuthenticatorBridgeImpl::~DeviceAuthenticatorBridgeImpl() {
  Java_DeviceAuthenticatorBridge_destroy(AttachCurrentThread(), java_object_);
}

BiometricsAvailability
DeviceAuthenticatorBridgeImpl::CanAuthenticateWithBiometric() {
  return static_cast<BiometricsAvailability>(
      Java_DeviceAuthenticatorBridge_canAuthenticateWithBiometric(
          AttachCurrentThread(), java_object_));
}

bool DeviceAuthenticatorBridgeImpl::CanAuthenticateWithBiometricOrScreenLock() {
  return Java_DeviceAuthenticatorBridge_canAuthenticateWithBiometricOrScreenLock(
      AttachCurrentThread(), java_object_);
}

void DeviceAuthenticatorBridgeImpl::Authenticate(
    base::OnceCallback<void(device_reauth::DeviceAuthUIResult)>
        response_callback) {
  response_callback_ = std::move(response_callback);
  Java_DeviceAuthenticatorBridge_authenticate(AttachCurrentThread(),
                                              java_object_);
}

void DeviceAuthenticatorBridgeImpl::Cancel() {
  Java_DeviceAuthenticatorBridge_cancel(AttachCurrentThread(), java_object_);
}

void DeviceAuthenticatorBridgeImpl::OnAuthenticationCompleted(JNIEnv* env,
                                                              jint result) {
  if (!response_callback_) {
    return;
  }
  std::move(response_callback_).Run(static_cast<DeviceAuthUIResult>(result));
}
