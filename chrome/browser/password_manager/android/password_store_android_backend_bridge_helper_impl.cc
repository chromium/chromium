// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_store_android_backend_bridge_helper_impl.h"

#include <cstdint>
#include <memory>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/bind_post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/password_manager/android/password_store_android_backend_bridge.h"
#include "chrome/browser/password_manager/android/password_store_android_backend_consumer_bridge.h"
#include "components/password_manager/core/browser/password_form.h"

namespace password_manager {

namespace {

using JobId = PasswordStoreAndroidBackendBridgeHelper::JobId;

}

std::unique_ptr<PasswordStoreAndroidBackendBridgeHelper>
PasswordStoreAndroidBackendBridgeHelper::Create() {
  return std::make_unique<PasswordStoreAndroidBackendBridgeHelperImpl>();
}

bool PasswordStoreAndroidBackendBridgeHelper::CanCreateBackend() {
  // TODO(crbug.com/1394715): Either move this call to the background thread or
  // use cached GMS Core version from `BuildInfo.gmsVersionCode`.
  return PasswordStoreAndroidBackendBridge::CanCreateBackend();
}

PasswordStoreAndroidBackendBridgeHelperImpl::
    PasswordStoreAndroidBackendBridgeHelperImpl()
    : consumer_bridge_(PasswordStoreAndroidBackendConsumerBridge::Create()),
      bridge_(PasswordStoreAndroidBackendBridge::Create()),
      background_task_runner_(base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::TaskPriority::USER_VISIBLE})) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  // Bridge is manually shut down on the sequence where all operations are
  // executed. It's safe to use `base::Unretained(bridge_)` for binding.
  background_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&PasswordStoreAndroidBackendBridge::Init,
                                base::Unretained(bridge_.get()),
                                std::ref(*consumer_bridge_)));
}

PasswordStoreAndroidBackendBridgeHelperImpl::
    PasswordStoreAndroidBackendBridgeHelperImpl(
        base::PassKey<class PasswordStoreAndroidBackendBridgeHelperImplTest>,
        std::unique_ptr<PasswordStoreAndroidBackendConsumerBridge>
            consumer_bridge,
        std::unique_ptr<PasswordStoreAndroidBackendBridge> bridge)
    : consumer_bridge_(std::move(consumer_bridge)),
      bridge_(std::move(bridge)),
      background_task_runner_(base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::TaskPriority::USER_VISIBLE})) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  background_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&PasswordStoreAndroidBackendBridge::Init,
                                base::Unretained(bridge_.get()),
                                std::ref(*consumer_bridge_)));
}

PasswordStoreAndroidBackendBridgeHelperImpl::
    ~PasswordStoreAndroidBackendBridgeHelperImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  // Delete bridge on the background thread where it lives.
  bool will_delete =
      background_task_runner_->DeleteSoon(FROM_HERE, std::move(bridge_));
  DCHECK(will_delete);
}

void PasswordStoreAndroidBackendBridgeHelperImpl::SetConsumer(
    base::WeakPtr<Consumer> consumer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  DCHECK(consumer_bridge_);
  consumer_bridge_->SetConsumer(consumer);
}

JobId PasswordStoreAndroidBackendBridgeHelperImpl::GetAllLogins(
    Account account) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  DCHECK(bridge_);
  JobId job_id = GetNextJobId();
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PasswordStoreAndroidBackendBridge::GetAllLogins,
                     base::Unretained(bridge_.get()), job_id,
                     std::move(account)));
  return job_id;
}

JobId PasswordStoreAndroidBackendBridgeHelperImpl::GetAutofillableLogins(
    Account account) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  DCHECK(bridge_);
  JobId job_id = GetNextJobId();
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PasswordStoreAndroidBackendBridge::GetAutofillableLogins,
                     base::Unretained(bridge_.get()), job_id,
                     std::move(account)));
  return job_id;
}

JobId PasswordStoreAndroidBackendBridgeHelperImpl::GetLoginsForSignonRealm(
    const std::string& signon_realm,
    Account account) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  DCHECK(bridge_);
  JobId job_id = GetNextJobId();
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &PasswordStoreAndroidBackendBridge::GetLoginsForSignonRealm,
          base::Unretained(bridge_.get()), job_id, signon_realm,
          std::move(account)));
  return job_id;
}

JobId PasswordStoreAndroidBackendBridgeHelperImpl::AddLogin(
    const password_manager::PasswordForm& form,
    Account account) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  DCHECK(bridge_);
  JobId job_id = GetNextJobId();
  background_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&PasswordStoreAndroidBackendBridge::AddLogin,
                                base::Unretained(bridge_.get()), job_id, form,
                                std::move(account)));
  return job_id;
}

JobId PasswordStoreAndroidBackendBridgeHelperImpl::UpdateLogin(
    const password_manager::PasswordForm& form,
    Account account) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  DCHECK(bridge_);
  JobId job_id = GetNextJobId();
  background_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&PasswordStoreAndroidBackendBridge::UpdateLogin,
                                base::Unretained(bridge_.get()), job_id, form,
                                std::move(account)));
  return job_id;
}

JobId PasswordStoreAndroidBackendBridgeHelperImpl::RemoveLogin(
    const password_manager::PasswordForm& form,
    Account account) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  DCHECK(bridge_);
  JobId job_id = GetNextJobId();
  background_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&PasswordStoreAndroidBackendBridge::RemoveLogin,
                                base::Unretained(bridge_.get()), job_id, form,
                                std::move(account)));
  return job_id;
}

JobId PasswordStoreAndroidBackendBridgeHelperImpl::GetNextJobId() {
  last_job_id_ = JobId(last_job_id_.value() + 1);
  return last_job_id_;
}

void PasswordStoreAndroidBackendBridgeHelperImpl::ShowErrorNotification() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  DCHECK(bridge_);
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PasswordStoreAndroidBackendBridge::ShowErrorNotification,
                     base::Unretained(bridge_.get())));
}

}  // namespace password_manager
