// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_store_android_backend_consumer_bridge_impl.h"

#include <jni.h>
#include <cstdint>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/password_manager/android/jni_headers/PasswordStoreAndroidBackendConsumerBridgeImpl_jni.h"
#include "components/password_manager/core/browser/android_backend_error.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/protos/list_passwords_result.pb.h"
#include "components/password_manager/core/browser/protos/password_with_local_data.pb.h"
#include "components/password_manager/core/browser/unified_password_manager_proto_utils.h"

namespace password_manager {

namespace {

using JobId = PasswordStoreAndroidBackendConsumerBridge::JobId;

std::vector<PasswordForm> CreateFormsVector(
    const base::android::JavaRef<jbyteArray>& passwords) {
  std::vector<uint8_t> serialized_result;
  base::android::JavaByteArrayToByteVector(base::android::AttachCurrentThread(),
                                           passwords, &serialized_result);
  ListPasswordsResult list_passwords_result;
  bool parsing_succeeds = list_passwords_result.ParseFromArray(
      serialized_result.data(), serialized_result.size());
  DCHECK(parsing_succeeds);
  auto forms = PasswordVectorFromListResult(list_passwords_result);
  for (auto& form : forms) {
    // Set proper in_store value for GMS Core storage.
    form.in_store = PasswordForm::Store::kProfileStore;
  }
  return forms;
}

}  // namespace

std::unique_ptr<PasswordStoreAndroidBackendConsumerBridge>
PasswordStoreAndroidBackendConsumerBridge::Create() {
  return std::make_unique<PasswordStoreAndroidBackendConsumerBridgeImpl>();
}

PasswordStoreAndroidBackendConsumerBridgeImpl::
    PasswordStoreAndroidBackendConsumerBridgeImpl() {
  java_object_ = Java_PasswordStoreAndroidBackendConsumerBridgeImpl_create(
      base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this));
}

PasswordStoreAndroidBackendConsumerBridgeImpl::
    ~PasswordStoreAndroidBackendConsumerBridgeImpl() {
  Java_PasswordStoreAndroidBackendConsumerBridgeImpl_destroy(
      base::android::AttachCurrentThread(), java_object_);
}

base::android::ScopedJavaGlobalRef<jobject>
PasswordStoreAndroidBackendConsumerBridgeImpl::GetJavaBridge() const {
  return java_object_;
}

void PasswordStoreAndroidBackendConsumerBridgeImpl::SetConsumer(
    base::WeakPtr<Consumer> consumer) {
  consumer_ = consumer;
}

void PasswordStoreAndroidBackendConsumerBridgeImpl::OnCompleteWithLogins(
    JNIEnv* env,
    jint job_id,
    const base::android::JavaParamRef<jbyteArray>& passwords) {
  DCHECK(consumer_);
  consumer_->OnCompleteWithLogins(JobId(job_id), CreateFormsVector(passwords));
}

void PasswordStoreAndroidBackendConsumerBridgeImpl::OnError(
    JNIEnv* env,
    jint job_id,
    jint error_type,
    jint api_error_code,
    jboolean has_connection_result,
    jint connection_result_code) {
  DCHECK(consumer_);
  // Posting the tasks to the same sequence prevents that synchronous responses
  // try to finish tasks before their registration was completed.
  AndroidBackendError error{static_cast<AndroidBackendErrorType>(error_type)};

  if (error.type == AndroidBackendErrorType::kExternalError) {
    error.api_error_code = static_cast<int>(api_error_code);
  }

  if (has_connection_result) {
    error.connection_result_code = static_cast<int>(connection_result_code);
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &PasswordStoreAndroidBackendConsumerBridge::Consumer::OnError,
          consumer_, JobId(job_id), std::move(error)));
}

void PasswordStoreAndroidBackendConsumerBridgeImpl::OnLoginChanged(
    JNIEnv* env,
    jint job_id) {
  DCHECK(consumer_);
  // Notifying that a login changed without providing a changelist prompts the
  // caller to explicitly check the remaining logins.
  consumer_->OnLoginsChanged(JobId(job_id), absl::nullopt);
}

}  // namespace password_manager
