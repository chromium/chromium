// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_store_android_backend_bridge_impl.h"

#include <jni.h>
#include <cstdint>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "chrome/browser/password_manager/android/jni_headers/PasswordStoreAndroidBackendBridgeImpl_jni.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/protos/list_passwords_result.pb.h"
#include "components/password_manager/core/browser/protos/password_with_local_data.pb.h"
#include "components/password_manager/core/browser/sync/password_proto_utils.h"
#include "components/password_manager/core/browser/unified_password_manager_proto_utils.h"

namespace password_manager {

namespace {

using JobId = PasswordStoreAndroidBackendBridge::JobId;

base::android::ScopedJavaLocalRef<jstring> GetJavaStringFromAccount(
    PasswordStoreAndroidBackendBridgeImpl::Account account) {
  if (absl::holds_alternative<PasswordStoreOperationTarget>(account)) {
    DCHECK(PasswordStoreOperationTarget::kLocalStorage ==
           absl::get<PasswordStoreOperationTarget>(account));
    return nullptr;
  }
  return base::android::ConvertUTF8ToJavaString(
      base::android::AttachCurrentThread(),
      absl::get<PasswordStoreAndroidBackendBridgeImpl::SyncingAccount>(account)
          .value());
}

}  // namespace

std::unique_ptr<PasswordStoreAndroidBackendBridge>
PasswordStoreAndroidBackendBridge::Create(
    const PasswordStoreAndroidBackendConsumerBridge& consumer_bridge) {
  return std::make_unique<PasswordStoreAndroidBackendBridgeImpl>(
      consumer_bridge);
}

bool PasswordStoreAndroidBackendBridge::CanCreateBackend() {
  return Java_PasswordStoreAndroidBackendBridgeImpl_canCreateBackend(
      base::android::AttachCurrentThread());
}

PasswordStoreAndroidBackendBridgeImpl::PasswordStoreAndroidBackendBridgeImpl(
    const PasswordStoreAndroidBackendConsumerBridge& consumer_bridge) {
  java_object_ = Java_PasswordStoreAndroidBackendBridgeImpl_create(
      base::android::AttachCurrentThread(), consumer_bridge.GetJavaBridge());
}

PasswordStoreAndroidBackendBridgeImpl::
    ~PasswordStoreAndroidBackendBridgeImpl() = default;

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

JobId PasswordStoreAndroidBackendBridgeImpl::AddLogin(const PasswordForm& form,
                                                      Account account) {
  JobId job_id = GetNextJobId();
  PasswordWithLocalData data = PasswordWithLocalDataFromPassword(form);
  Java_PasswordStoreAndroidBackendBridgeImpl_addLogin(
      base::android::AttachCurrentThread(), java_object_, job_id.value(),
      base::android::ToJavaByteArray(base::android::AttachCurrentThread(),
                                     data.SerializeAsString()),
      GetJavaStringFromAccount(std::move(account)));
  return job_id;
}

JobId PasswordStoreAndroidBackendBridgeImpl::UpdateLogin(
    const PasswordForm& form,
    Account account) {
  JobId job_id = GetNextJobId();
  PasswordWithLocalData data = PasswordWithLocalDataFromPassword(form);
  Java_PasswordStoreAndroidBackendBridgeImpl_updateLogin(
      base::android::AttachCurrentThread(), java_object_, job_id.value(),
      base::android::ToJavaByteArray(base::android::AttachCurrentThread(),
                                     data.SerializeAsString()),
      GetJavaStringFromAccount(std::move(account)));
  return job_id;
}

JobId PasswordStoreAndroidBackendBridgeImpl::RemoveLogin(
    const PasswordForm& form,
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

JobId PasswordStoreAndroidBackendBridgeImpl::GetNextJobId() {
  last_job_id_ = JobId(last_job_id_.value() + 1);
  return last_job_id_;
}

void PasswordStoreAndroidBackendBridgeImpl::ShowErrorNotification() {
  Java_PasswordStoreAndroidBackendBridgeImpl_showErrorUi(
      base::android::AttachCurrentThread(), java_object_);
}

}  // namespace password_manager
