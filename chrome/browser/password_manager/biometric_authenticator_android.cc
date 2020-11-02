// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/biometric_authenticator_android.h"

#include <memory>
#include <utility>

#include "base/android/jni_android.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "chrome/android/chrome_jni_headers/BiometricAuthenticatorBridge_jni.h"
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
std::unique_ptr<ChromeBiometricAuthenticator>
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

  return std::make_unique<BiometricAuthenticatorAndroid>(window_android);
}

BiometricAuthenticatorAndroid::BiometricAuthenticatorAndroid(
    ui::WindowAndroid* window_android) {
  java_object_ = Java_BiometricAuthenticatorBridge_create(
      AttachCurrentThread(), window_android->GetJavaObject());
}

BiometricAuthenticatorAndroid::~BiometricAuthenticatorAndroid() = default;

BiometricsAvailability BiometricAuthenticatorAndroid::CanAuthenticate() {
  return static_cast<BiometricsAvailability>(
      Java_BiometricAuthenticatorBridge_canAuthenticate(AttachCurrentThread(),
                                                        java_object_));
}

void BiometricAuthenticatorAndroid::Authenticate(
    const UiCredential& credential,
    AuthenticateCallback callback) {
  // TODO(crbug.com/1031483): Implement.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}
