// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_store_android_backend_bridge_impl.h"

#include <jni.h>
#include <cstdint>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "chrome/browser/password_manager/android/jni_headers/PasswordStoreAndroidBackendBridgeImpl_jni.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/sync/password_proto_utils.h"
#include "components/sync/protocol/list_passwords_result.pb.h"

using TaskId = PasswordStoreAndroidBackendBridgeImpl::TaskId;

namespace {

std::vector<password_manager::PasswordForm> CreateFormsVector(
    const base::android::JavaRef<jbyteArray>& passwords) {
  std::vector<uint8_t> serializedResult;
  base::android::JavaByteArrayToByteVector(base::android::AttachCurrentThread(),
                                           passwords, &serializedResult);
  sync_pb::ListPasswordsResult list_passwords_result;
  list_passwords_result.ParseFromArray(serializedResult.data(),
                                       serializedResult.size());
  return password_manager::PasswordVectorFromListResult(list_passwords_result);
}

}  // namespace

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
    const base::android::JavaParamRef<jbyteArray>& passwords) {
  DCHECK(consumer_);
  consumer_->OnCompleteWithLogins(TaskId(task_id),
                                  CreateFormsVector(passwords));
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
