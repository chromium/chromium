// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_sync_controller_delegate_bridge_impl.h"

#include "base/android/jni_android.h"
#include "chrome/browser/password_manager/android/android_backend_error.h"
#include "chrome/browser/password_manager/android/jni_headers/PasswordSyncControllerDelegateBridgeImpl_jni.h"
#include "components/password_manager/core/common/password_manager_features.h"

using password_manager::AndroidBackendError;
using password_manager::AndroidBackendErrorType;

PasswordSyncControllerDelegateBridgeImpl::
    PasswordSyncControllerDelegateBridgeImpl() {
  if (base::FeatureList::IsEnabled(
          password_manager::features::kUnifiedPasswordManagerAndroid)) {
    java_object_ = Java_PasswordSyncControllerDelegateBridgeImpl_create(
        base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this));
  }
}

PasswordSyncControllerDelegateBridgeImpl::
    ~PasswordSyncControllerDelegateBridgeImpl() = default;

void PasswordSyncControllerDelegateBridgeImpl::
    NotifyCredentialManagerWhenSyncing() {
  if (java_object_) {
    Java_PasswordSyncControllerDelegateBridgeImpl_notifyCredentialManagerWhenSyncing(
        base::android::AttachCurrentThread(), java_object_);
  }
}

void PasswordSyncControllerDelegateBridgeImpl::
    NotifyCredentialManagerWhenNotSyncing() {
  if (java_object_) {
    Java_PasswordSyncControllerDelegateBridgeImpl_notifyCredentialManagerWhenNotSyncing(
        base::android::AttachCurrentThread(), java_object_);
  }
}

void PasswordSyncControllerDelegateBridgeImpl::OnCredentialManagerNotified(
    JNIEnv* env) {
  // TODO(crbug/1297615): Record success metrics.
}

void PasswordSyncControllerDelegateBridgeImpl::OnCredentialManagerError(
    JNIEnv* env,
    jint error_code,
    jint api_error_code) {
  // TODO(crbug/1297615): Record failure metrics.
  // TODO(crbug/1297615): Record API errors metrcis when the API is actually
  // implemented.
}
