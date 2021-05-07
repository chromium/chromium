// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/biometric_authenticator_android.h"

#include <memory>
#include <utility>

#include "base/android/jni_android.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "chrome/browser/password_manager/android/jni_headers/BiometricAuthenticatorBridge_jni.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/password_manager/core/browser/biometric_authenticator.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/view_android.h"

using base::android::AttachCurrentThread;
using content::WebContents;
using password_manager::BiometricsAvailability;
using password_manager::UiCredential;

// static
scoped_refptr<password_manager::BiometricAuthenticator>
ChromeBiometricAuthenticator::Create(WebContents* web_contents) {
  if (!base::FeatureList::IsEnabled(
          password_manager::features::kBiometricTouchToFill)) {
    return nullptr;
  }

  auto* window_android = web_contents->GetNativeView()->GetWindowAndroid();
  if (!window_android) {
    // GetWindowAndroid() can be null in tests.
    return nullptr;
  }

  return base::WrapRefCounted(
      new BiometricAuthenticatorAndroid(window_android));
}

BiometricAuthenticatorAndroid::BiometricAuthenticatorAndroid(
    ui::WindowAndroid* window_android) {
  java_object_ = Java_BiometricAuthenticatorBridge_create(
      AttachCurrentThread(), reinterpret_cast<intptr_t>(this),
      window_android->GetJavaObject());
}

BiometricAuthenticatorAndroid::~BiometricAuthenticatorAndroid() {
  Java_BiometricAuthenticatorBridge_destroy(AttachCurrentThread(),
                                            java_object_);
}

BiometricsAvailability BiometricAuthenticatorAndroid::CanAuthenticate() {
  return static_cast<BiometricsAvailability>(
      Java_BiometricAuthenticatorBridge_canAuthenticate(AttachCurrentThread(),
                                                        java_object_));
}

void BiometricAuthenticatorAndroid::Authenticate(
    const UiCredential& credential,
    AuthenticateCallback callback) {
  callback_ = std::move(callback);
  Java_BiometricAuthenticatorBridge_authenticate(AttachCurrentThread(),
                                                 java_object_);
}

void BiometricAuthenticatorAndroid::Cancel() {
  callback_.Reset();
  Java_BiometricAuthenticatorBridge_cancel(AttachCurrentThread(), java_object_);
}

void BiometricAuthenticatorAndroid::OnAuthenticationCompleted(
    JNIEnv* env,
    jboolean success) {
  if (callback_.is_null())
    return;
  std::move(callback_).Run(success);
}
