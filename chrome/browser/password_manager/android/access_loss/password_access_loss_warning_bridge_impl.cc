// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/access_loss/password_access_loss_warning_bridge_impl.h"

#include "base/android/jni_android.h"
#include "base/feature_list.h"
#include "chrome/browser/password_manager/android/access_loss/jni_headers/PasswordAccessLossWarningBridge_jni.h"
#include "components/password_manager/core/browser/features/password_features.h"

PasswordAccessLossWarningBridgeImpl::PasswordAccessLossWarningBridgeImpl() =
    default;

PasswordAccessLossWarningBridgeImpl::~PasswordAccessLossWarningBridgeImpl() =
    default;

void PasswordAccessLossWarningBridgeImpl::MaybeShowAccessLossNoticeSheet() {
  JNIEnv* env = base::android::AttachCurrentThread();
  jni_zero::ScopedJavaLocalRef<jobject> java_bridge =
      Java_PasswordAccessLossWarningBridge_create(env);
  Java_PasswordAccessLossWarningBridge_show(env, java_bridge);
}

bool PasswordAccessLossWarningBridgeImpl::ShouldShowAccessLossNoticeSheet() {
  // TODO: crbug.com/357063741 - Check all the criteria for showing the sheet.
  return base::FeatureList::IsEnabled(
      password_manager::features::
          kUnifiedPasswordManagerLocalPasswordsAndroidAccessLossWarning);
}
