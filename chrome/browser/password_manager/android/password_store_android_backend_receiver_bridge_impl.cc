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
#include "chrome/browser/password_manager/android/protos/list_affiliated_passwords_result.pb.h"
#include "chrome/browser/password_manager/android/protos/list_passwords_result.pb.h"
#include "chrome/browser/password_manager/android/protos/password_with_local_data.pb.h"
#include "chrome/browser/password_manager/android/unified_password_manager_proto_utils.h"
#include "components/password_manager/core/browser/password_form.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/password_manager/android/jni_headers/PasswordStoreAndroidBackendReceiverBridgeImpl_jni.h"

namespace password_manager {

namespace {

using JobId = PasswordStoreAndroidBackendReceiverBridge::JobId;

template <typename ProtoType>
std::vector<PasswordForm> CreateFormsVector(
    const base::android::JavaRef<jbyteArray>& passwords,
    password_manager::IsAccountStore is_account_store) {
  std::vector<uint8_t> serialized_result;
  base::android::JavaByteArrayToByteVector(base::android::AttachCurrentThread(),
                                           passwords, &serialized_result);
  ProtoType list_passwords_result;
  bool parsing_succeeds = list_passwords_result.ParseFromArray(
      serialized_result.data(), serialized_result.size());
  DCHECK(parsing_succeeds);
  return PasswordVectorFromListResult(list_passwords_result, is_account_store);
}

}  // namespace

std::unique_ptr<PasswordStoreAndroidBackendReceiverBridge>
PasswordStoreAndroidBackendReceiverBridge::Create(
    password_manager::IsAccountStore is_account_store) {
  return std::make_unique<PasswordStoreAndroidBackendReceiverBridgeImpl>(
      is_account_store);
}

PasswordStoreAndroidBackendReceiverBridgeImpl::
    PasswordStoreAndroidBackendReceiverBridgeImpl(
        password_manager::IsAccountStore is_account_store)
    : is_account_store_(is_account_store) {
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
  consumer_->OnCompleteWithLogins(
      JobId(job_id),
      CreateFormsVector<ListPasswordsResult>(passwords, is_account_store_));
}

void PasswordStoreAndroidBackendReceiverBridgeImpl::OnCompleteWithBrandedLogins(
    JNIEnv* env,
    jint job_id,
    const base::android::JavaParamRef<jbyteArray>& passwords) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  DCHECK(consumer_);
  consumer_->OnCompleteWithLogins(
      JobId(job_id), CreateFormsVector<ListPasswordsWithUiInfoResult>(
                         passwords, is_account_store_));
}

void PasswordStoreAndroidBackendReceiverBridgeImpl::
    OnCompleteWithAffiliatedLogins(
        JNIEnv* env,
        jint job_id,
        const base::android::JavaParamRef<jbyteArray>& passwords) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  CHECK(consumer_);
  consumer_->OnCompleteWithLogins(
      JobId(job_id), CreateFormsVector<ListAffiliatedPasswordsResult>(
                         passwords, is_account_store_));
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
  consumer_->OnLoginsChanged(JobId(job_id), std::nullopt);
}

}  // namespace password_manager
