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

using JobId = PasswordStoreAndroidBackendBridgeImpl::JobId;

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
void PasswordStoreAndroidBackendBridgeImpl::SetConsumer(
    base::WeakPtr<Consumer> consumer) {
  consumer_ = consumer;
}

void PasswordStoreAndroidBackendBridgeImpl::OnCompleteWithLogins(
    JNIEnv* env,
    jint job_id,
    const base::android::JavaParamRef<jbyteArray>& passwords) {
  DCHECK(consumer_);
  consumer_->OnCompleteWithLogins(JobId(job_id), CreateFormsVector(passwords));
}

void PasswordStoreAndroidBackendBridgeImpl::OnError(JNIEnv* env, jint job_id) {
  DCHECK(consumer_);
  // Posting the tasks to the same sequence prevents that synchronous responses
  // try to finish tasks before their registration was completed.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&PasswordStoreAndroidBackendBridge::Consumer::OnError,
                     consumer_, JobId(job_id)));
}

JobId PasswordStoreAndroidBackendBridgeImpl::GetAllLogins() {
  JobId job_id = GetNextJobId();
  Java_PasswordStoreAndroidBackendBridgeImpl_getAllLogins(
      base::android::AttachCurrentThread(), java_object_, job_id.value());
  return job_id;
}

JobId PasswordStoreAndroidBackendBridgeImpl::GetNextJobId() {
  last_job_id_ = JobId(last_job_id_.value() + 1);
  return last_job_id_;
}
