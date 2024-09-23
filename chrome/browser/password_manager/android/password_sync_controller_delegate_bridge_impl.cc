// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_sync_controller_delegate_bridge_impl.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/password_manager/android/password_manager_android_util.h"
#include "components/password_manager/core/browser/password_store/android_backend_error.h"
#include "components/password_manager/core/common/password_manager_features.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/password_manager/android/jni_headers/PasswordSyncControllerDelegateBridgeImpl_jni.h"

using password_manager::AndroidBackendError;
using password_manager::AndroidBackendErrorType;

PasswordSyncControllerDelegateBridgeImpl::
    PasswordSyncControllerDelegateBridgeImpl() {
  // The bridge is not supposed to be created when UPM is completely unusable.
  CHECK(password_manager_android_util::AreMinUpmRequirementsMet());
  java_object_ = Java_PasswordSyncControllerDelegateBridgeImpl_create(
      base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this));
}

PasswordSyncControllerDelegateBridgeImpl::
    ~PasswordSyncControllerDelegateBridgeImpl() = default;

void PasswordSyncControllerDelegateBridgeImpl::SetConsumer(
    base::WeakPtr<Consumer> consumer) {
  consumer_ = consumer;
}

void PasswordSyncControllerDelegateBridgeImpl::
    NotifyCredentialManagerWhenSyncing(const std::string& account_email) {
  if (java_object_) {
    Java_PasswordSyncControllerDelegateBridgeImpl_notifyCredentialManagerWhenSyncing(
        base::android::AttachCurrentThread(), java_object_,
        base::android::ConvertUTF8ToJavaString(
            base::android::AttachCurrentThread(), account_email));
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
  consumer_->OnCredentialManagerNotified();
}

void PasswordSyncControllerDelegateBridgeImpl::OnCredentialManagerError(
    JNIEnv* env,
    jint error_code,
    jint api_error_code) {
  AndroidBackendError error{static_cast<AndroidBackendErrorType>(error_code)};
  consumer_->OnCredentialManagerError(error, static_cast<int>(api_error_code));
}
