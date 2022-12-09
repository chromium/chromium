// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_store_android_backend_receiver_bridge_impl.h"

#include <jni.h>
#include <cstdint>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/password_manager/android/jni_headers/PasswordStoreAndroidBackendReceiverBridgeImpl_jni.h"
#include "components/password_manager/core/browser/android_backend_error.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/protos/list_passwords_result.pb.h"
#include "components/password_manager/core/browser/protos/password_with_local_data.pb.h"
#include "components/password_manager/core/browser/unified_password_manager_proto_utils.h"

namespace password_manager {

namespace {

using JobId = PasswordStoreAndroidBackendReceiverBridge::JobId;

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

std::unique_ptr<PasswordStoreAndroidBackendReceiverBridge>
PasswordStoreAndroidBackendReceiverBridge::Create() {
  return std::make_unique<PasswordStoreAndroidBackendReceiverBridgeImpl>();
}

PasswordStoreAndroidBackendReceiverBridgeImpl::
    PasswordStoreAndroidBackendReceiverBridgeImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  java_object_ = Java_PasswordStoreAndroidBackendReceiverBridgeImpl_create(
      base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this));
}

PasswordStoreAndroidBackendReceiverBridgeImpl::
    ~PasswordStoreAndroidBackendReceiverBridgeImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  Java_PasswordStoreAndroidBackendReceiverBridgeImpl_destroy(
      base::android::AttachCurrentThread(), java_object_);
}

base::android::ScopedJavaGlobalRef<jobject>
PasswordStoreAndroidBackendReceiverBridgeImpl::GetJavaBridge() const {
  return java_object_;
}

void PasswordStoreAndroidBackendReceiverBridgeImpl::SetConsumer(
    base::WeakPtr<Consumer> consumer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  consumer_ = consumer;
}

void PasswordStoreAndroidBackendReceiverBridgeImpl::OnCompleteWithLogins(
    JNIEnv* env,
    jint job_id,
    const base::android::JavaParamRef<jbyteArray>& passwords) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  DCHECK(consumer_);
  consumer_->OnCompleteWithLogins(JobId(job_id), CreateFormsVector(passwords));
}

void PasswordStoreAndroidBackendReceiverBridgeImpl::OnError(
    JNIEnv* env,
    jint job_id,
    jint error_type,
    jint api_error_code,
    jboolean has_connection_result,
    jint connection_result_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
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
          &PasswordStoreAndroidBackendReceiverBridge::Consumer::OnError,
          consumer_, JobId(job_id), std::move(error)));
}

void PasswordStoreAndroidBackendReceiverBridgeImpl::OnLoginChanged(
    JNIEnv* env,
    jint job_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  DCHECK(consumer_);
  // Notifying that a login changed without providing a changelist prompts the
  // caller to explicitly check the remaining logins.
  consumer_->OnLoginsChanged(JobId(job_id), absl::nullopt);
}

}  // namespace password_manager
