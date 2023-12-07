// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_store_android_backend_dispatcher_bridge_impl.h"

#include <jni.h>
#include <cstdint>

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "chrome/browser/password_manager/android/jni_headers/PasswordStoreAndroidBackendDispatcherBridgeImpl_jni.h"
#include "chrome/browser/password_manager/android/protos/list_passwords_result.pb.h"
#include "chrome/browser/password_manager/android/protos/password_with_local_data.pb.h"
#include "chrome/browser/password_manager/android/unified_password_manager_proto_utils.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/sync/password_proto_utils.h"

namespace password_manager {

namespace {

using JobId = PasswordStoreAndroidBackendDispatcherBridge::JobId;

constexpr int kGMSCoreMinVersionForGetAffiliatedAPI = 232012000;
constexpr int kGMSCoreMinVersionForGetAllLoginsWithBrandingAPI = 233812000;
constexpr int kGMSCoreVersionWithFewerErrors = 225012000;

base::android::ScopedJavaLocalRef<jstring> GetJavaStringFromAccount(
    PasswordStoreAndroidBackendDispatcherBridgeImpl::Account account) {
  if (absl::holds_alternative<PasswordStoreOperationTarget>(account)) {
    DCHECK(PasswordStoreOperationTarget::kLocalStorage ==
           absl::get<PasswordStoreOperationTarget>(account));
    return nullptr;
  }
  return base::android::ConvertUTF8ToJavaString(
      base::android::AttachCurrentThread(),
      absl::get<
          PasswordStoreAndroidBackendDispatcherBridgeImpl::SyncingAccount>(
          account)
          .value());
}

}  // namespace

std::unique_ptr<PasswordStoreAndroidBackendDispatcherBridge>
PasswordStoreAndroidBackendDispatcherBridge::Create() {
  return std::make_unique<PasswordStoreAndroidBackendDispatcherBridgeImpl>();
}

bool PasswordStoreAndroidBackendDispatcherBridge::CanCreateBackend() {
  return Java_PasswordStoreAndroidBackendDispatcherBridgeImpl_canCreateBackend(
      base::android::AttachCurrentThread());
}

bool PasswordStoreAndroidBackendDispatcherBridge::
    CanUseGetAffiliatedPasswordsAPI() {
  base::android::BuildInfo* info = base::android::BuildInfo::GetInstance();
  int current_gms_core_version;
  if (!base::StringToInt(info->gms_version_code(), &current_gms_core_version)) {
    return false;
  }
  if (kGMSCoreMinVersionForGetAffiliatedAPI > current_gms_core_version) {
    return false;
  }

  return base::FeatureList::IsEnabled(
      password_manager::features::kFillingAcrossAffiliatedWebsitesAndroid);
}

bool PasswordStoreAndroidBackendDispatcherBridge::
    CanUseGetAllLoginsWithBrandingInfoAPI() {
  base::android::BuildInfo* info = base::android::BuildInfo::GetInstance();
  int current_gms_core_version;
  if (!base::StringToInt(info->gms_version_code(), &current_gms_core_version)) {
    return false;
  }
  if (kGMSCoreMinVersionForGetAllLoginsWithBrandingAPI >
      current_gms_core_version) {
    return false;
  }

  return base::FeatureList::IsEnabled(
      password_manager::features::kUseGMSCoreForBrandingInfo);
}

bool PasswordStoreAndroidBackendDispatcherBridge::CanRemoveUnenrollment() {
  base::android::BuildInfo* info = base::android::BuildInfo::GetInstance();
  int current_gms_core_version;
  if (!base::StringToInt(info->gms_version_code(), &current_gms_core_version)) {
    return false;
  }

  if (current_gms_core_version < kGMSCoreVersionWithFewerErrors) {
    return false;
  }

  // Check minimum GMSCore version from Finch in case it was bumped.
  if (current_gms_core_version <
      features::kMinimumGMSCoreVersionToRemoveUnenrollment.Get()) {
    return false;
  }

  return base::FeatureList::IsEnabled(features::kRemoveUPMUnenrollment);
}

PasswordStoreAndroidBackendDispatcherBridgeImpl::
    PasswordStoreAndroidBackendDispatcherBridgeImpl() {
  DETACH_FROM_THREAD(thread_checker_);
}

PasswordStoreAndroidBackendDispatcherBridgeImpl::
    ~PasswordStoreAndroidBackendDispatcherBridgeImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void PasswordStoreAndroidBackendDispatcherBridgeImpl::Init(
    base::android::ScopedJavaGlobalRef<jobject> receiver_bridge) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  java_object_ = Java_PasswordStoreAndroidBackendDispatcherBridgeImpl_create(
      base::android::AttachCurrentThread(), receiver_bridge);
}

void PasswordStoreAndroidBackendDispatcherBridgeImpl::GetAllLogins(
    JobId job_id,
    Account account) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  Java_PasswordStoreAndroidBackendDispatcherBridgeImpl_getAllLogins(
      base::android::AttachCurrentThread(), java_object_, job_id.value(),
      GetJavaStringFromAccount(std::move(account)));
}

void PasswordStoreAndroidBackendDispatcherBridgeImpl::
    GetAllLoginsWithBrandingInfo(JobId job_id, Account account) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  Java_PasswordStoreAndroidBackendDispatcherBridgeImpl_getAllLoginsWithBrandingInfo(
      base::android::AttachCurrentThread(), java_object_, job_id.value(),
      GetJavaStringFromAccount(std::move(account)));
}

void PasswordStoreAndroidBackendDispatcherBridgeImpl::GetAutofillableLogins(
    JobId job_id,
    Account account) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  Java_PasswordStoreAndroidBackendDispatcherBridgeImpl_getAutofillableLogins(
      base::android::AttachCurrentThread(), java_object_, job_id.value(),
      GetJavaStringFromAccount(std::move(account)));
}

void PasswordStoreAndroidBackendDispatcherBridgeImpl::GetLoginsForSignonRealm(
    JobId job_id,
    const std::string& signon_realm,
    Account account) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  Java_PasswordStoreAndroidBackendDispatcherBridgeImpl_getLoginsForSignonRealm(
      base::android::AttachCurrentThread(), java_object_, job_id.value(),
      base::android::ConvertUTF8ToJavaString(
          base::android::AttachCurrentThread(), signon_realm),
      GetJavaStringFromAccount(std::move(account)));
}

void PasswordStoreAndroidBackendDispatcherBridgeImpl::
    GetAffiliatedLoginsForSignonRealm(JobId job_id,
                                      const std::string& signon_realm,
                                      Account account) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  Java_PasswordStoreAndroidBackendDispatcherBridgeImpl_getAffiliatedLoginsForSignonRealm(
      base::android::AttachCurrentThread(), java_object_, job_id.value(),
      base::android::ConvertUTF8ToJavaString(
          base::android::AttachCurrentThread(), signon_realm),
      GetJavaStringFromAccount(std::move(account)));
}

void PasswordStoreAndroidBackendDispatcherBridgeImpl::AddLogin(
    JobId job_id,
    const password_manager::PasswordForm& form,
    Account account) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  password_manager::PasswordWithLocalData data =
      PasswordWithLocalDataFromPassword(form);
  Java_PasswordStoreAndroidBackendDispatcherBridgeImpl_addLogin(
      base::android::AttachCurrentThread(), java_object_, job_id.value(),
      base::android::ToJavaByteArray(base::android::AttachCurrentThread(),
                                     data.SerializeAsString()),
      GetJavaStringFromAccount(std::move(account)));
}

void PasswordStoreAndroidBackendDispatcherBridgeImpl::UpdateLogin(
    JobId job_id,
    const password_manager::PasswordForm& form,
    Account account) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  password_manager::PasswordWithLocalData data =
      PasswordWithLocalDataFromPassword(form);
  Java_PasswordStoreAndroidBackendDispatcherBridgeImpl_updateLogin(
      base::android::AttachCurrentThread(), java_object_, job_id.value(),
      base::android::ToJavaByteArray(base::android::AttachCurrentThread(),
                                     data.SerializeAsString()),
      GetJavaStringFromAccount(std::move(account)));
}

void PasswordStoreAndroidBackendDispatcherBridgeImpl::RemoveLogin(
    JobId job_id,
    const password_manager::PasswordForm& form,
    Account account) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  sync_pb::PasswordSpecificsData data =
      SpecificsDataFromPassword(form, /*base_password_data=*/{});
  Java_PasswordStoreAndroidBackendDispatcherBridgeImpl_removeLogin(
      base::android::AttachCurrentThread(), java_object_, job_id.value(),
      base::android::ToJavaByteArray(base::android::AttachCurrentThread(),
                                     data.SerializeAsString()),
      GetJavaStringFromAccount(std::move(account)));
}

}  // namespace password_manager
