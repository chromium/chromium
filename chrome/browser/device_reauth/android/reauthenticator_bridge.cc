// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/android/reauthenticator_bridge.h"

#include <jni.h>
#include "base/android/jni_android.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chrome/browser/device_reauth/android/jni_headers/ReauthenticatorBridge_jni.h"
#include "chrome/browser/device_reauth/chrome_device_authenticator_factory.h"
#include "components/password_manager/core/browser/password_manager_util.h"

static jlong JNI_ReauthenticatorBridge_Create(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& java_bridge,
    jint requester) {
  return reinterpret_cast<intptr_t>(
      new ReauthenticatorBridge(java_bridge, requester));
}

ReauthenticatorBridge::ReauthenticatorBridge(
    const base::android::JavaParamRef<jobject>& java_bridge,
    jint requester)
    : java_bridge_(java_bridge),
      requester_(static_cast<device_reauth::DeviceAuthRequester>(requester)) {
  authenticator_ = ChromeDeviceAuthenticatorFactory::GetDeviceAuthenticator();
}

ReauthenticatorBridge::~ReauthenticatorBridge() {
  if (authenticator_) {
    authenticator_->Cancel(requester_);
  }
}

bool ReauthenticatorBridge::CanUseAuthenticationWithBiometric(JNIEnv* env) {
  return authenticator_ && authenticator_->CanAuthenticateWithBiometrics();
}

bool ReauthenticatorBridge::CanUseAuthenticationWithBiometricOrScreenLock(
    JNIEnv* env) {
  return authenticator_ &&
         authenticator_->CanAuthenticateWithBiometricOrScreenLock();
}

void ReauthenticatorBridge::Reauthenticate(JNIEnv* env,
                                           bool use_last_valid_auth) {
  if (!authenticator_) {
    return;
  }

  // `this` notifies the authenticator when it is destructed, resulting in
  // the callback being reset by the authenticator. Therefore, it is safe
  // to use base::Unretained.
  authenticator_->Authenticate(
      requester_,
      base::BindOnce(&ReauthenticatorBridge::OnReauthenticationCompleted,
                     base::Unretained(this)),
      use_last_valid_auth);
}

void ReauthenticatorBridge::OnReauthenticationCompleted(bool auth_succeeded) {
  Java_ReauthenticatorBridge_onReauthenticationCompleted(
      base::android::AttachCurrentThread(), java_bridge_, auth_succeeded);
}
