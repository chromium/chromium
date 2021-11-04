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
#include "base/metrics/sparse_histogram.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/password_manager/android/password_store_android_backend_bridge.h"
#include "components/password_manager/core/browser/login_database.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store_backend.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/model/model_type_controller_delegate.h"
#include "components/sync/model/proxy_model_type_controller_delegate.h"
#include "components/sync/model/type_entities_count.h"

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
    SyncModelTypeControllerDelegate(PasswordStoreAndroidBackendBridge* bridge)
    : bridge_(bridge) {
  DCHECK(bridge_);
}

PasswordStoreAndroidBackend::SyncModelTypeControllerDelegate::
    ~SyncModelTypeControllerDelegate() = default;

void PasswordStoreAndroidBackend::SyncModelTypeControllerDelegate::
    OnSyncStarting(const syncer::DataTypeActivationRequest& request,
                   StartCallback callback) {
  // Sync started for passwords, either because the user just turned it on or
  // because of a browser startup.
  // TODO(crbug.com/1260837): Cache a boolean or similar enum in local storage
  // to distinguish browser startups from the case where the user just turned
  // sync on. This cached value will need cleanup in OnSyncStopping().
  NOTIMPLEMENTED();

  // Set |skip_engine_connection| to true to indicate that, actually, this sync
  // datatype doesn't depend on the built-in SyncEngine to communicate changes
  // to/from the Sync server. Instead, Android specific functionality is
  // leveraged to achieve similar behavior.
  auto activation_response =
      std::make_unique<syncer::DataTypeActivationResponse>();
  activation_response->skip_engine_connection = true;
  std::move(callback).Run(std::move(activation_response));
}

void PasswordStoreAndroidBackend::SyncModelTypeControllerDelegate::
    OnSyncStopping(syncer::SyncStopMetadataFate metadata_fate) {
  switch (metadata_fate) {
    case syncer::KEEP_METADATA:
      // Sync got temporarily paused. Just ignore.
      break;
    case syncer::CLEAR_METADATA:
      // The user (or something equivalent like an enterprise policy)
      // permanently disabled sync, either fully or specifically for passwords.
      // This also includes more advanced cases like the user having cleared all
      // sync data in the dashboard (birthday reset) or, at least in theory, the
      // sync server reporting that all sync metadata is obsolete (i.e.
      // CLIENT_DATA_OBSOLETE in the sync protocol).
      // TODO(crbug.com/1260837): Notify |bridge_| that sync was permanently
      // disabled such that sync-ed data remains available in local storage. If
      // OnSyncStarting() caches any local state, it should probably be cleared
      // here.
      NOTIMPLEMENTED();
      break;
  }
}

void PasswordStoreAndroidBackend::SyncModelTypeControllerDelegate::
    GetAllNodesForDebugging(AllNodesCallback callback) {
  // This is not implemented because it's not worth the hassle just to display
  // debug information in chrome://sync-internals.
  std::move(callback).Run(syncer::PASSWORDS,
                          std::make_unique<base::ListValue>());
}

void PasswordStoreAndroidBackend::SyncModelTypeControllerDelegate::
    GetTypeEntitiesCountForDebugging(
        base::OnceCallback<void(const syncer::TypeEntitiesCount&)> callback)
        const {
  // This is not implemented because it's not worth the hassle just to display
  // debug information in chrome://sync-internals.
  std::move(callback).Run(syncer::TypeEntitiesCount(syncer::PASSWORDS));
}

void PasswordStoreAndroidBackend::SyncModelTypeControllerDelegate::
    RecordMemoryUsageAndCountsHistograms() {
  // This is not implemented because it's not worth the hassle.
}

PasswordStoreAndroidBackend::PasswordStoreAndroidBackend(
    std::unique_ptr<PasswordStoreAndroidBackendBridge> bridge)
    : bridge_(std::move(bridge)), sync_controller_delegate_(bridge_.get()) {
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
  // Run callback with an empty forms list to facilitate testing of other
  // backend methods while this method is not implemented.
  std::move(callback).Run({});
}

void PasswordStoreAndroidBackend::AddLoginAsync(
    const PasswordForm& form,
    PasswordStoreChangeListReply callback) {
  JobId job_id = bridge_->AddLogin(form);
  QueueNewJob(job_id,
              JobReturnHandler(std::move(callback),
                               JobReturnHandler::MetricInfix("AddLoginAsync")));
}

void PasswordStoreAndroidBackend::UpdateLoginAsync(
    const PasswordForm& form,
    PasswordStoreChangeListReply callback) {
  JobId job_id = bridge_->UpdateLogin(form);
  QueueNewJob(job_id, JobReturnHandler(
                          std::move(callback),
                          JobReturnHandler::MetricInfix("UpdateLoginAsync")));
}

void PasswordStoreAndroidBackend::RemoveLoginAsync(
    const PasswordForm& form,
    PasswordStoreChangeListReply callback) {
  JobId job_id = bridge_->RemoveLogin(form);
  QueueNewJob(job_id, JobReturnHandler(
                          std::move(callback),
                          JobReturnHandler::MetricInfix("RemoveLoginAsync")));
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
PasswordStoreAndroidBackend::CreateSyncControllerDelegate() {
  return std::make_unique<syncer::ProxyModelTypeControllerDelegate>(
      base::SequencedTaskRunnerHandle::Get(),
      base::BindRepeating(
          &PasswordStoreAndroidBackend::GetSyncControllerDelegate,
          base::Unretained(this)));
}

void PasswordStoreAndroidBackend::GetSyncStatus(
    base::OnceCallback<void(bool)> callback) {
  NOTREACHED();
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

void PasswordStoreAndroidBackend::OnLoginsChanged(
    JobId job_id,
    const PasswordStoreChangeList& changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  JobReturnHandler reply = GetAndEraseJob(job_id);
  reply.RecordMetrics(JobReturnHandler::WasSuccess(true));
  DCHECK(reply.Holds<PasswordStoreChangeListReply>());

  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(reply).Get<PasswordStoreChangeListReply>(),
                     changes));
}

void PasswordStoreAndroidBackend::OnError(JobId job_id,
                                          AndroidBackendError error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  JobReturnHandler reply = GetAndEraseJob(job_id);
  reply.RecordMetrics(JobReturnHandler::WasSuccess(false));
  base::UmaHistogramEnumeration(
      "PasswordManager.PasswordStoreAndroidBackend.ErrorCode", error.type);
  if (error.type == AndroidBackendErrorType::kExternalError) {
    DCHECK(error.api_error_code.has_value());
    base::HistogramBase* histogram = base::SparseHistogram::FactoryGet(
        "PasswordManager.PasswordStoreAndroidBackend.APIError",
        base::HistogramBase::kUmaTargetedHistogramFlag);
    histogram->Add(error.api_error_code.value());
  }
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
