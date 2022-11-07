// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_store_android_backend_bridge_impl.h"

#include <jni.h>
#include <cstdint>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/feature_list.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/password_manager/android/jni_headers/PasswordStoreAndroidBackendBridgeImpl_jni.h"
#include "chrome/browser/password_manager/android/password_store_android_backend_api_error_codes.h"
#include "components/password_manager/core/browser/android_backend_error.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/protos/list_passwords_result.pb.h"
#include "components/password_manager/core/browser/protos/password_with_local_data.pb.h"
#include "components/password_manager/core/browser/sync/password_proto_utils.h"
#include "components/password_manager/core/browser/unified_password_manager_proto_utils.h"
#include "components/password_manager/core/common/password_manager_features.h"

using JobId = PasswordStoreAndroidBackendBridgeImpl::JobId;

namespace {

std::vector<password_manager::PasswordForm> CreateFormsVector(
    const base::android::JavaRef<jbyteArray>& passwords) {
  std::vector<uint8_t> serialized_result;
  base::android::JavaByteArrayToByteVector(base::android::AttachCurrentThread(),
                                           passwords, &serialized_result);
  password_manager::ListPasswordsResult list_passwords_result;
  bool parsing_succeeds = list_passwords_result.ParseFromArray(
      serialized_result.data(), serialized_result.size());
  DCHECK(parsing_succeeds);
  auto forms =
      password_manager::PasswordVectorFromListResult(list_passwords_result);
  for (auto& form : forms) {
    // Set proper in_store value for GMS Core storage.
    form.in_store = password_manager::PasswordForm::Store::kProfileStore;
  }
  return forms;
}

base::android::ScopedJavaLocalRef<jstring> GetJavaStringFromAccount(
    PasswordStoreAndroidBackendBridgeImpl::Account account) {
  if (absl::holds_alternative<password_manager::PasswordStoreOperationTarget>(
          account)) {
    DCHECK(password_manager::PasswordStoreOperationTarget::kLocalStorage ==
           absl::get<password_manager::PasswordStoreOperationTarget>(account));
    return nullptr;
  }
  return base::android::ConvertUTF8ToJavaString(
      base::android::AttachCurrentThread(),
      absl::get<PasswordStoreAndroidBackendBridgeImpl::SyncingAccount>(account)
          .value());
}

}  // namespace

namespace password_manager {

std::unique_ptr<PasswordStoreAndroidBackendBridge>
PasswordStoreAndroidBackendBridge::Create() {
  return std::make_unique<PasswordStoreAndroidBackendBridgeImpl>();
}

bool PasswordStoreAndroidBackendBridge::CanCreateBackend() {
  return Java_PasswordStoreAndroidBackendBridgeImpl_canCreateBackend(
      base::android::AttachCurrentThread());
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

void PasswordStoreAndroidBackendBridgeImpl::OnError(
    JNIEnv* env,
    jint job_id,
    jint error_type,
    jint api_error_code,
    jboolean has_connection_result,
    jint connection_result_code) {
  DCHECK(consumer_);
  // Posting the tasks to the same sequence prevents that synchronous responses
  // try to finish tasks before their registration was completed.
  password_manager::AndroidBackendError error{
      static_cast<password_manager::AndroidBackendErrorType>(error_type)};

  if (error.type == password_manager::AndroidBackendErrorType::kExternalError) {
    error.api_error_code = static_cast<int>(api_error_code);
  }

  if (has_connection_result) {
    error.connection_result_code = static_cast<int>(connection_result_code);
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&PasswordStoreAndroidBackendBridge::Consumer::OnError,
                     consumer_, JobId(job_id), std::move(error)));
}

JobId PasswordStoreAndroidBackendBridgeImpl::GetAllLogins(Account account) {
  JobId job_id = GetNextJobId();
  Java_PasswordStoreAndroidBackendBridgeImpl_getAllLogins(
      base::android::AttachCurrentThread(), java_object_, job_id.value(),
      GetJavaStringFromAccount(std::move(account)));
  return job_id;
}

JobId PasswordStoreAndroidBackendBridgeImpl::GetAutofillableLogins(
    Account account) {
  JobId job_id = GetNextJobId();
  Java_PasswordStoreAndroidBackendBridgeImpl_getAutofillableLogins(
      base::android::AttachCurrentThread(), java_object_, job_id.value(),
      GetJavaStringFromAccount(std::move(account)));
  return job_id;
}

JobId PasswordStoreAndroidBackendBridgeImpl::GetLoginsForSignonRealm(
    const std::string& signon_realm,
    Account account) {
  JobId job_id = GetNextJobId();
  Java_PasswordStoreAndroidBackendBridgeImpl_getLoginsForSignonRealm(
      base::android::AttachCurrentThread(), java_object_, job_id.value(),
      base::android::ConvertUTF8ToJavaString(
          base::android::AttachCurrentThread(), signon_realm),
      GetJavaStringFromAccount(std::move(account)));
  return job_id;
}

JobId PasswordStoreAndroidBackendBridgeImpl::AddLogin(
    const password_manager::PasswordForm& form,
    Account account) {
  JobId job_id = GetNextJobId();
  password_manager::PasswordWithLocalData data =
      PasswordWithLocalDataFromPassword(form);
  Java_PasswordStoreAndroidBackendBridgeImpl_addLogin(
      base::android::AttachCurrentThread(), java_object_, job_id.value(),
      base::android::ToJavaByteArray(base::android::AttachCurrentThread(),
                                     data.SerializeAsString()),
      GetJavaStringFromAccount(std::move(account)));
  return job_id;
}

JobId PasswordStoreAndroidBackendBridgeImpl::UpdateLogin(
    const password_manager::PasswordForm& form,
    Account account) {
  JobId job_id = GetNextJobId();
  password_manager::PasswordWithLocalData data =
      PasswordWithLocalDataFromPassword(form);
  Java_PasswordStoreAndroidBackendBridgeImpl_updateLogin(
      base::android::AttachCurrentThread(), java_object_, job_id.value(),
      base::android::ToJavaByteArray(base::android::AttachCurrentThread(),
                                     data.SerializeAsString()),
      GetJavaStringFromAccount(std::move(account)));
  return job_id;
}

JobId PasswordStoreAndroidBackendBridgeImpl::RemoveLogin(
    const password_manager::PasswordForm& form,
    Account account) {
  JobId job_id = GetNextJobId();
  sync_pb::PasswordSpecificsData data =
      SpecificsDataFromPassword(form, /*base_password_data=*/{});
  Java_PasswordStoreAndroidBackendBridgeImpl_removeLogin(
      base::android::AttachCurrentThread(), java_object_, job_id.value(),
      base::android::ToJavaByteArray(base::android::AttachCurrentThread(),
                                     data.SerializeAsString()),
      GetJavaStringFromAccount(std::move(account)));
  return job_id;
}

void PasswordStoreAndroidBackendBridgeImpl::OnLoginChanged(JNIEnv* env,
                                                           jint job_id) {
  DCHECK(consumer_);
  // Notifying that a login changed without providing a changelist prompts the
  // caller to explicitly check the remaining logins.
  consumer_->OnLoginsChanged(JobId(job_id), absl::nullopt);
}

JobId PasswordStoreAndroidBackendBridgeImpl::GetNextJobId() {
  last_job_id_ = JobId(last_job_id_.value() + 1);
  return last_job_id_;
}

void PasswordStoreAndroidBackendBridgeImpl::ShowErrorNotification() {
  Java_PasswordStoreAndroidBackendBridgeImpl_showErrorUi(
      base::android::AttachCurrentThread(), java_object_);
}
