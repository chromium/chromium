// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/access_loss/password_access_loss_warning_bridge_impl.h"

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/feature_list.h"
#include "chrome/browser/password_manager/android/access_loss/jni_headers/PasswordAccessLossWarningBridge_jni.h"
#include "chrome/browser/password_manager/android/password_manager_android_util.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "ui/android/window_android.h"

PasswordAccessLossWarningBridgeImpl::PasswordAccessLossWarningBridgeImpl() =
    default;

PasswordAccessLossWarningBridgeImpl::~PasswordAccessLossWarningBridgeImpl() =
    default;

bool PasswordAccessLossWarningBridgeImpl::ShouldShowAccessLossNoticeSheet(
    PrefService* pref_service) {
  // TODO: crbug.com/357063741 - Check all the criteria for showing the sheet.
  if (!base::FeatureList::IsEnabled(
          password_manager::features::
              kUnifiedPasswordManagerLocalPasswordsAndroidAccessLossWarning)) {
    return false;
  }

  if (password_manager_android_util::GetPasswordAccessLossWarningType(
          pref_service) ==
      password_manager_android_util::PasswordAccessLossWarningType::kNone) {
    return false;
  }
  return true;
}

void PasswordAccessLossWarningBridgeImpl::MaybeShowAccessLossNoticeSheet(
    PrefService* pref_service,
    const gfx::NativeWindow window) {
  if (!window) {
    return;
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  jni_zero::ScopedJavaLocalRef<jobject> java_bridge =
      Java_PasswordAccessLossWarningBridge_create(env, window->GetJavaObject());
  if (!java_bridge) {
    return;
  }
  Java_PasswordAccessLossWarningBridge_show(
      env, java_bridge,
      static_cast<int>(
          password_manager_android_util::GetPasswordAccessLossWarningType(
              pref_service)));
}
