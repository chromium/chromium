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
#include "chrome/browser/password_manager/android/password_store_android_backend_dispatcher_bridge.h"
#include "chrome/browser/password_manager/android/password_store_android_backend_receiver_bridge.h"
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
  return PasswordStoreAndroidBackendDispatcherBridge::CanCreateBackend();
}

PasswordStoreAndroidBackendBridgeHelperImpl::
    PasswordStoreAndroidBackendBridgeHelperImpl()
    : receiver_bridge_(PasswordStoreAndroidBackendReceiverBridge::Create()),
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

void PasswordStoreAndroidBackendBridgeHelperImpl::SetConsumer(
    base::WeakPtr<Consumer> consumer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  DCHECK(receiver_bridge_);
  receiver_bridge_->SetConsumer(consumer);
}

JobId PasswordStoreAndroidBackendBridgeHelperImpl::GetAllLogins(
    Account account) {
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

JobId PasswordStoreAndroidBackendBridgeHelperImpl::GetAutofillableLogins(
    Account account) {
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
    Account account) {
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

JobId PasswordStoreAndroidBackendBridgeHelperImpl::AddLogin(
    const password_manager::PasswordForm& form,
    Account account) {
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
    Account account) {
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
    Account account) {
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

void PasswordStoreAndroidBackendBridgeHelperImpl::ShowErrorNotification() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  DCHECK(dispatcher_bridge_);
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &PasswordStoreAndroidBackendDispatcherBridge::ShowErrorNotification,
          base::Unretained(dispatcher_bridge_.get())));
}

}  // namespace password_manager
