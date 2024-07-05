// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_store_android_backend_bridge_helper_impl.h"

#include <cstdint>
#include <memory>

#include "base/android/build_info.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/bind_post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/password_manager/android/password_manager_android_util.h"
#include "chrome/browser/password_manager/android/password_store_android_backend_dispatcher_bridge.h"
#include "chrome/browser/password_manager/android/password_store_android_backend_receiver_bridge.h"
#include "components/password_manager/core/browser/password_form.h"

namespace password_manager {

namespace {

constexpr int kGMSCoreMinVersionForGetAffiliatedAPI = 232012000;
constexpr int kGMSCoreMinVersionForGetAllLoginsWithBrandingAPI = 233812000;

using JobId = PasswordStoreAndroidBackendBridgeHelper::JobId;

}

std::unique_ptr<PasswordStoreAndroidBackendBridgeHelper>
PasswordStoreAndroidBackendBridgeHelper::Create(
    password_manager::IsAccountStore is_account_store) {
  // The bridge is not supposed to be created when UPM is completely unusable.
  // But it should be created for non-syncing users if sync is enabled later.
  CHECK(password_manager_android_util::AreMinUpmRequirementsMet());
  return std::make_unique<PasswordStoreAndroidBackendBridgeHelperImpl>(
      is_account_store);
}

PasswordStoreAndroidBackendBridgeHelperImpl::
    PasswordStoreAndroidBackendBridgeHelperImpl(
        password_manager::IsAccountStore is_account_store)
    : receiver_bridge_(
          PasswordStoreAndroidBackendReceiverBridge::Create(is_account_store)),
      dispatcher_bridge_(PasswordStoreAndroidBackendDispatcherBridge::Create()),
      background_task_runner_(base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::TaskPriority::USER_VISIBLE})) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  // Bridge is manually shut down on the sequence where all operations are
  // executed. It's safe to use `base::Unretained(dispatcher_bridge_)` for
  // binding.
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PasswordStoreAndroidBackendDispatcherBridge::Init,
                     base::Unretained(dispatcher_bridge_.get()),
                     receiver_bridge_->GetJavaBridge()));
}

PasswordStoreAndroidBackendBridgeHelperImpl::
    PasswordStoreAndroidBackendBridgeHelperImpl(
        base::PassKey<class PasswordStoreAndroidBackendBridgeHelperImplTest>,
        std::unique_ptr<PasswordStoreAndroidBackendReceiverBridge>
            receiver_bridge,
        std::unique_ptr<PasswordStoreAndroidBackendDispatcherBridge>
            dispatcher_bridge)
    : receiver_bridge_(std::move(receiver_bridge)),
      dispatcher_bridge_(std::move(dispatcher_bridge)),
      background_task_runner_(base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::TaskPriority::USER_VISIBLE})) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PasswordStoreAndroidBackendDispatcherBridge::Init,
                     base::Unretained(dispatcher_bridge_.get()),
                     receiver_bridge_->GetJavaBridge()));
}

PasswordStoreAndroidBackendBridgeHelperImpl::
    ~PasswordStoreAndroidBackendBridgeHelperImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  // Delete dispatcher bridge on the background thread where it lives.
  bool will_delete = background_task_runner_->DeleteSoon(
      FROM_HERE, std::move(dispatcher_bridge_));
  DCHECK(will_delete);
}

bool PasswordStoreAndroidBackendBridgeHelperImpl::
    CanUseGetAffiliatedPasswordsAPI() {
  base::android::BuildInfo* info = base::android::BuildInfo::GetInstance();
  int current_gms_core_version;
  if (!base::StringToInt(info->gms_version_code(), &current_gms_core_version)) {
    return false;
  }
  if (kGMSCoreMinVersionForGetAffiliatedAPI > current_gms_core_version) {
    return false;
  }

  return true;
}

bool PasswordStoreAndroidBackendBridgeHelperImpl::
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

  return true;
}

void PasswordStoreAndroidBackendBridgeHelperImpl::SetConsumer(
    base::WeakPtr<Consumer> consumer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  DCHECK(receiver_bridge_);
  receiver_bridge_->SetConsumer(consumer);
}

JobId PasswordStoreAndroidBackendBridgeHelperImpl::GetAllLogins(
    std::string account) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  DCHECK(dispatcher_bridge_);
  JobId job_id = GetNextJobId();
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PasswordStoreAndroidBackendDispatcherBridge::GetAllLogins,
                     base::Unretained(dispatcher_bridge_.get()), job_id,
                     std::move(account)));
  return job_id;
}

JobId PasswordStoreAndroidBackendBridgeHelperImpl::GetAllLoginsWithBrandingInfo(
    std::string account) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  DCHECK(dispatcher_bridge_);
  JobId job_id = GetNextJobId();
  background_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&PasswordStoreAndroidBackendDispatcherBridge::
                                    GetAllLoginsWithBrandingInfo,
                                base::Unretained(dispatcher_bridge_.get()),
                                job_id, std::move(account)));
  return job_id;
}

JobId PasswordStoreAndroidBackendBridgeHelperImpl::GetAutofillableLogins(
    std::string account) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  DCHECK(dispatcher_bridge_);
  JobId job_id = GetNextJobId();
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &PasswordStoreAndroidBackendDispatcherBridge::GetAutofillableLogins,
          base::Unretained(dispatcher_bridge_.get()), job_id,
          std::move(account)));
  return job_id;
}

JobId PasswordStoreAndroidBackendBridgeHelperImpl::GetLoginsForSignonRealm(
    const std::string& signon_realm,
    std::string account) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  DCHECK(dispatcher_bridge_);
  JobId job_id = GetNextJobId();
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &PasswordStoreAndroidBackendDispatcherBridge::GetLoginsForSignonRealm,
          base::Unretained(dispatcher_bridge_.get()), job_id, signon_realm,
          std::move(account)));
  return job_id;
}

JobId PasswordStoreAndroidBackendBridgeHelperImpl::
    GetAffiliatedLoginsForSignonRealm(const std::string& signon_realm,
                                      std::string account) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  CHECK(dispatcher_bridge_);
  JobId job_id = GetNextJobId();
  background_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&PasswordStoreAndroidBackendDispatcherBridge::
                                    GetAffiliatedLoginsForSignonRealm,
                                base::Unretained(dispatcher_bridge_.get()),
                                job_id, signon_realm, std::move(account)));
  return job_id;
}

JobId PasswordStoreAndroidBackendBridgeHelperImpl::AddLogin(
    const password_manager::PasswordForm& form,
    std::string account) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  DCHECK(dispatcher_bridge_);
  JobId job_id = GetNextJobId();
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PasswordStoreAndroidBackendDispatcherBridge::AddLogin,
                     base::Unretained(dispatcher_bridge_.get()), job_id, form,
                     std::move(account)));
  return job_id;
}

JobId PasswordStoreAndroidBackendBridgeHelperImpl::UpdateLogin(
    const password_manager::PasswordForm& form,
    std::string account) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  DCHECK(dispatcher_bridge_);
  JobId job_id = GetNextJobId();
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PasswordStoreAndroidBackendDispatcherBridge::UpdateLogin,
                     base::Unretained(dispatcher_bridge_.get()), job_id, form,
                     std::move(account)));
  return job_id;
}

JobId PasswordStoreAndroidBackendBridgeHelperImpl::RemoveLogin(
    const password_manager::PasswordForm& form,
    std::string account) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  DCHECK(dispatcher_bridge_);
  JobId job_id = GetNextJobId();
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PasswordStoreAndroidBackendDispatcherBridge::RemoveLogin,
                     base::Unretained(dispatcher_bridge_.get()), job_id, form,
                     std::move(account)));
  return job_id;
}

JobId PasswordStoreAndroidBackendBridgeHelperImpl::GetNextJobId() {
  last_job_id_ = JobId(last_job_id_.value() + 1);
  return last_job_id_;
}

}  // namespace password_manager
