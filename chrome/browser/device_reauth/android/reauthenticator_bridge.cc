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
#include "chrome/browser/profiles/profile_manager.h"
#include "components/password_manager/core/browser/password_manager_util.h"

static jlong JNI_ReauthenticatorBridge_Create(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& java_bridge,
    jint source) {
  return reinterpret_cast<intptr_t>(
      new ReauthenticatorBridge(java_bridge, source));
}

ReauthenticatorBridge::ReauthenticatorBridge(
    const base::android::JavaParamRef<jobject>& java_bridge,
    jint source)
    : java_bridge_(java_bridge) {
  device_reauth::DeviceAuthParams params(
      base::Seconds(0), static_cast<device_reauth::DeviceAuthSource>(source));

  // TODO(crbug.com/1479361): Replace GetLastUsedProfile() when Android starts
  // supporting multiple profiles.
  authenticator_ = ChromeDeviceAuthenticatorFactory::GetForProfile(
      ProfileManager::GetLastUsedProfile(), params);
}

ReauthenticatorBridge::~ReauthenticatorBridge() {
  if (authenticator_) {
    authenticator_->Cancel();
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

void ReauthenticatorBridge::Reauthenticate(JNIEnv* env) {
  if (!authenticator_) {
    return;
  }

  // `this` notifies the authenticator when it is destructed, resulting in
  // the callback being reset by the authenticator. Therefore, it is safe
  // to use base::Unretained.
  authenticator_->AuthenticateWithMessage(
      u"", base::BindOnce(&ReauthenticatorBridge::OnReauthenticationCompleted,
                          base::Unretained(this)));
}

void ReauthenticatorBridge::OnReauthenticationCompleted(bool auth_succeeded) {
  Java_ReauthenticatorBridge_onReauthenticationCompleted(
      base::android::AttachCurrentThread(), java_bridge_, auth_succeeded);
}
