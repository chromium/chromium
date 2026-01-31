// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/android/reauthenticator_bridge.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chrome/browser/device_reauth/chrome_device_authenticator_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/password_manager/core/browser/password_manager_util.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/device_reauth/android/jni_headers/ReauthenticatorBridge_jni.h"

static int64_t JNI_ReauthenticatorBridge_Create(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& java_bridge,
    const base::android::JavaRef<jobject>& activity,
    Profile* profile,
    int32_t source) {
  return reinterpret_cast<intptr_t>(
      new ReauthenticatorBridge(java_bridge, activity, profile, source));
}

ReauthenticatorBridge::ReauthenticatorBridge(
    const base::android::JavaRef<jobject>& java_bridge,
    const base::android::JavaRef<jobject>& activity,
    Profile* profile,
    int32_t source)
    : java_bridge_(java_bridge), profile_(profile) {
  device_reauth::DeviceAuthParams params(
      base::Seconds(0), static_cast<device_reauth::DeviceAuthSource>(source));

  authenticator_ = ChromeDeviceAuthenticatorFactory::GetForProfile(
      profile, activity, params);
}

ReauthenticatorBridge::~ReauthenticatorBridge() {
  if (authenticator_) {
    authenticator_->Cancel();
  }
}

int32_t ReauthenticatorBridge::GetBiometricAvailabilityStatus(JNIEnv* env) {
  if (authenticator_ == nullptr) {
    return static_cast<int32_t>(device_reauth::BiometricStatus::kUnavailable);
  }
  return static_cast<int32_t>(authenticator_->GetBiometricAvailabilityStatus());
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

void ReauthenticatorBridge::Destroy(JNIEnv* env) {
  delete this;
}

DEFINE_JNI(ReauthenticatorBridge)
