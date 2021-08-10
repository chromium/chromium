// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_store_android_backend_bridge_impl.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "chrome/browser/password_manager/android/jni_headers/PasswordStoreAndroidBackendBridgeImpl_jni.h"
#include "components/password_manager/core/browser/password_form.h"

using TaskId = PasswordStoreAndroidBackendBridgeImpl::TaskId;

namespace password_manager {

std::unique_ptr<PasswordStoreAndroidBackendBridge>
PasswordStoreAndroidBackendBridge::Create() {
  return std::make_unique<PasswordStoreAndroidBackendBridgeImpl>();
}

}  // namespace password_manager

PasswordStoreAndroidBackendBridgeImpl::PasswordStoreAndroidBackendBridgeImpl() {
  java_object_ = Java_PasswordStoreAndroidBackendBridgeImpl_create(
      base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this));
}

PasswordStoreAndroidBackendBridgeImpl::
    ~PasswordStoreAndroidBackendBridgeImpl() {
  Java_PasswordStoreAndroidBackendBridgeImpl_destroy(
      base::android::AttachCurrentThread(), java_object_);
}
void PasswordStoreAndroidBackendBridgeImpl::SetConsumer(Consumer* consumer) {
  consumer_ = consumer;
}

void PasswordStoreAndroidBackendBridgeImpl::OnCompleteWithLogins(
    JNIEnv* env,
    jint task_id,
    const base::android::JavaParamRef<jobject>& passwords) {
  DCHECK(consumer_);
  // TODO(crbug.com/1229650): Convert passwords to forms.
  consumer_->OnCompleteWithLogins(TaskId(task_id), {});
}

TaskId PasswordStoreAndroidBackendBridgeImpl::GetAllLogins() {
  TaskId task_id = GetNextTaskId();
  Java_PasswordStoreAndroidBackendBridgeImpl_getAllLogins(
      base::android::AttachCurrentThread(), java_object_, task_id.value());
  return task_id;
}

TaskId PasswordStoreAndroidBackendBridgeImpl::GetNextTaskId() {
  last_task_id_ = TaskId(last_task_id_.value() + 1);
  return last_task_id_;
}
