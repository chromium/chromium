// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_store_android_backend.h"

#include <jni.h>
#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/password_manager/android/password_store_android_backend_bridge.h"
#include "components/password_manager/core/browser/login_database.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store_backend.h"
#include "components/sync/model/model_type_controller_delegate.h"
#include "components/sync/model/proxy_model_type_controller_delegate.h"

namespace password_manager {

namespace {

using JobId = PasswordStoreAndroidBackendBridge::JobId;

std::vector<std::unique_ptr<PasswordForm>> WrapPasswordsIntoPointers(
    std::vector<PasswordForm> passwords) {
  std::vector<std::unique_ptr<PasswordForm>> password_ptrs;
  password_ptrs.reserve(passwords.size());
  for (auto& password : passwords) {
    password_ptrs.push_back(
        std::make_unique<PasswordForm>(std::move(password)));
  }
  return password_ptrs;
}

}  // namespace

PasswordStoreAndroidBackend::JobReturnHandler::JobReturnHandler() = default;

PasswordStoreAndroidBackend::JobReturnHandler::JobReturnHandler(
    LoginsReply callback,
    MetricInfix metric_infix)
    : success_callback_(std::move(callback)),
      metric_infix_(std::move(metric_infix)) {}

PasswordStoreAndroidBackend::JobReturnHandler::JobReturnHandler(
    PasswordStoreChangeListReply callback,
    MetricInfix metric_infix)
    : success_callback_(std::move(callback)),
      metric_infix_(std::move(metric_infix)) {}

PasswordStoreAndroidBackend::JobReturnHandler::JobReturnHandler(
    JobReturnHandler&&) = default;
PasswordStoreAndroidBackend::JobReturnHandler&

PasswordStoreAndroidBackend::JobReturnHandler::JobReturnHandler::operator=(
    JobReturnHandler&&) = default;

PasswordStoreAndroidBackend::JobReturnHandler::~JobReturnHandler() = default;

void PasswordStoreAndroidBackend::JobReturnHandler::RecordMetrics(
    WasSuccess success) const {
  auto BuildMetricName = [this](base::StringPiece suffix) {
    return base::StrCat({"PasswordManager.PasswordStoreAndroidBackend.",
                         *metric_infix_, ".", suffix});
  };
  base::TimeDelta duration = base::Time::Now() - start_;
  base::UmaHistogramMediumTimes(BuildMetricName("Latency"), duration);
  base::UmaHistogramBoolean(BuildMetricName("Success"), *success);
}

PasswordStoreAndroidBackend::SyncModelTypeControllerDelegate::
    SyncModelTypeControllerDelegate() = default;

PasswordStoreAndroidBackend::SyncModelTypeControllerDelegate::
    ~SyncModelTypeControllerDelegate() = default;

PasswordStoreAndroidBackend::PasswordStoreAndroidBackend(
    std::unique_ptr<PasswordStoreAndroidBackendBridge> bridge)
    : bridge_(std::move(bridge)) {
  DCHECK(bridge_);
  bridge_->SetConsumer(weak_ptr_factory_.GetWeakPtr());
}

PasswordStoreAndroidBackend::~PasswordStoreAndroidBackend() = default;

void PasswordStoreAndroidBackend::InitBackend(
    RemoteChangesReceived remote_form_changes_received,
    base::RepeatingClosure sync_enabled_or_disabled_cb,
    base::OnceCallback<void(bool)> completion) {
  main_task_runner_ = base::SequencedTaskRunnerHandle::Get();
  remote_form_changes_received_ = std::move(remote_form_changes_received);
  // TODO(https://crbug.com/1229650): Create subscription before completion.
  std::move(completion).Run(/*success=*/true);
}

void PasswordStoreAndroidBackend::Shutdown(
    base::OnceClosure shutdown_completed) {
  // TODO(https://crbug.com/1229654): Implement (e.g. unsubscribe from GMS).
  std::move(shutdown_completed).Run();
}

void PasswordStoreAndroidBackend::GetAllLoginsAsync(LoginsReply callback) {
  JobId job_id = bridge_->GetAllLogins();
  QueueNewJob(job_id, JobReturnHandler(
                          std::move(callback),
                          JobReturnHandler::MetricInfix("GetAllLoginsAsync")));
}

void PasswordStoreAndroidBackend::GetAutofillableLoginsAsync(
    LoginsReply callback) {
  // TODO(https://crbug.com/1229654): Implement.
}

void PasswordStoreAndroidBackend::FillMatchingLoginsAsync(
    LoginsReply callback,
    bool include_psl,
    const std::vector<PasswordFormDigest>& forms) {
  // TODO(https://crbug.com/1229654): Implement.
}

void PasswordStoreAndroidBackend::AddLoginAsync(
    const PasswordForm& form,
    PasswordStoreChangeListReply callback) {
  // TODO(https://crbug.com/1229655):Implement.
}

void PasswordStoreAndroidBackend::UpdateLoginAsync(
    const PasswordForm& form,
    PasswordStoreChangeListReply callback) {
  // TODO(https://crbug.com/1229655):Implement.
}

void PasswordStoreAndroidBackend::RemoveLoginAsync(
    const PasswordForm& form,
    PasswordStoreChangeListReply callback) {
  // TODO(https://crbug.com/1229655):Implement.
}

void PasswordStoreAndroidBackend::RemoveLoginsByURLAndTimeAsync(
    const base::RepeatingCallback<bool(const GURL&)>& url_filter,
    base::Time delete_begin,
    base::Time delete_end,
    base::OnceCallback<void(bool)> sync_completion,
    PasswordStoreChangeListReply callback) {
  // TODO(https://crbug.com/1229655):Implement.
}

void PasswordStoreAndroidBackend::RemoveLoginsCreatedBetweenAsync(
    base::Time delete_begin,
    base::Time delete_end,
    PasswordStoreChangeListReply callback) {
  // TODO(https://crbug.com/1229655):Implement.
}

void PasswordStoreAndroidBackend::DisableAutoSignInForOriginsAsync(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
    base::OnceClosure completion) {
  // TODO(https://crbug.com/1229655):Implement.
}

SmartBubbleStatsStore* PasswordStoreAndroidBackend::GetSmartBubbleStatsStore() {
  return nullptr;
}

FieldInfoStore* PasswordStoreAndroidBackend::GetFieldInfoStore() {
  return nullptr;
}

std::unique_ptr<syncer::ProxyModelTypeControllerDelegate>
PasswordStoreAndroidBackend::CreateSyncControllerDelegateFactory() {
  return std::make_unique<syncer::ProxyModelTypeControllerDelegate>(
      base::SequencedTaskRunnerHandle::Get(),
      base::BindRepeating(
          &PasswordStoreAndroidBackend::GetSyncControllerDelegate,
          base::Unretained(this)));
}

void PasswordStoreAndroidBackend::OnCompleteWithLogins(
    JobId job_id,
    std::vector<PasswordForm> passwords) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  JobReturnHandler reply = GetAndEraseJob(job_id);
  reply.RecordMetrics(JobReturnHandler::WasSuccess(true));
  DCHECK(reply.Holds<LoginsReply>());
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(reply).Get<LoginsReply>(),
                     WrapPasswordsIntoPointers(std::move(passwords))));
}

void PasswordStoreAndroidBackend::OnError(JobId job_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  JobReturnHandler reply = GetAndEraseJob(job_id);
  reply.RecordMetrics(JobReturnHandler::WasSuccess(false));
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
PasswordStoreAndroidBackend::GetSyncControllerDelegate() {
  return sync_controller_delegate_.GetWeakPtr();
}

void PasswordStoreAndroidBackend::QueueNewJob(JobId job_id,
                                              JobReturnHandler return_handler) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  request_for_job_.emplace(job_id, std::move(return_handler));
}

PasswordStoreAndroidBackend::JobReturnHandler
PasswordStoreAndroidBackend::GetAndEraseJob(JobId job_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  auto iter = request_for_job_.find(job_id);
  DCHECK(iter != request_for_job_.end());
  JobReturnHandler reply = std::move(iter->second);
  request_for_job_.erase(iter);
  return reply;
}

}  // namespace password_manager
