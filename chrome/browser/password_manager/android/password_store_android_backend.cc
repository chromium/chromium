// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_store_android_backend.h"

#include <jni.h>
#include <memory>
#include <vector>

#include "base/barrier_callback.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/sparse_histogram.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/password_manager/android/password_store_android_backend_bridge.h"
#include "chrome/browser/password_manager/android/password_store_operation_target.h"
#include "components/autofill/core/browser/autofill_regexes.h"
#include "components/password_manager/core/browser/login_database.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store_backend.h"
#include "components/password_manager/core/browser/password_store_util.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/model/model_type_controller_delegate.h"
#include "components/sync/model/proxy_model_type_controller_delegate.h"
#include "components/sync/model/type_entities_count.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace password_manager {

namespace {

using autofill::MatchesPattern;
using base::UTF8ToUTF16;
using password_manager::GetExpressionForFederatedMatching;
using password_manager::GetRegexForPSLFederatedMatching;
using password_manager::GetRegexForPSLMatching;

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

std::string FormToSignonRealmQuery(const PasswordFormDigest& form,
                                   bool include_psl) {
  if (include_psl) {
    // Check PSL matches and matches for exact signon realm.
    return GetRegistryControlledDomain(GURL(form.signon_realm));
  }
  if (form.scheme == PasswordForm::Scheme::kHtml) {
    // Check federated matches and matches for exact signon realm.
    return form.url.host();
  }
  // Check matches for exact signon realm.
  return form.signon_realm;
}

bool MatchesIncludedPSLAndFederation(const PasswordForm& retrieved_login,
                                     const PasswordFormDigest& form_to_match,
                                     bool include_psl) {
  if (retrieved_login.signon_realm == form_to_match.signon_realm)
    return true;

  std::u16string retrieved_login_signon_realm =
      UTF8ToUTF16(retrieved_login.signon_realm);
  const bool include_federated =
      form_to_match.scheme == PasswordForm::Scheme::kHtml;

  if (include_psl) {
    const std::u16string psl_regex =
        UTF8ToUTF16(GetRegexForPSLMatching(form_to_match.signon_realm));
    if (MatchesPattern(retrieved_login_signon_realm, psl_regex))
      return true;
    if (include_federated) {
      const std::u16string psl_federated_regex = UTF8ToUTF16(
          GetRegexForPSLFederatedMatching(form_to_match.signon_realm));
      if (MatchesPattern(retrieved_login_signon_realm, psl_federated_regex))
        return true;
    }
  } else if (include_federated) {
    const std::u16string federated_regex =
        UTF8ToUTF16("^" + GetExpressionForFederatedMatching(form_to_match.url));
    return include_federated &&
           MatchesPattern(retrieved_login_signon_realm, federated_regex);
  }
  return false;
}

void ValidateSignonRealm(const PasswordFormDigest& form_digest_to_match,
                         bool include_psl,
                         LoginsOrErrorReply callback,
                         LoginsResultOrError logins_or_error) {
  if (absl::holds_alternative<PasswordStoreBackendError>(logins_or_error)) {
    std::move(callback).Run(std::move(logins_or_error));
    return;
  }
  LoginsResult retrieved_logins =
      std::move(absl::get<LoginsResult>(logins_or_error));
  LoginsResult matching_logins;
  for (auto it = retrieved_logins.begin(); it != retrieved_logins.end();) {
    if (MatchesIncludedPSLAndFederation(*it->get(), form_digest_to_match,
                                        include_psl)) {
      matching_logins.push_back(std::move(*it));
      // std::vector::erase returns the iterator for the next element.
      it = retrieved_logins.erase(it);
    } else {
      it++;
    }
  }
  std::move(callback).Run(std::move(matching_logins));
}

LoginsResultOrError JoinRetrievedLoginsOrError(
    std::vector<LoginsResultOrError> results) {
  LoginsResult joined_logins;
  for (auto& result : results) {
    // If one of retrievals ended with an error, pass on the error.
    if (absl::holds_alternative<PasswordStoreBackendError>(result))
      return std::move(result);
    LoginsResult logins = std::move(absl::get<LoginsResult>(result));
    std::move(logins.begin(), logins.end(), std::back_inserter(joined_logins));
  }
  return joined_logins;
}

}  // namespace

PasswordStoreAndroidBackend::MetricsRecorder::MetricsRecorder() = default;

PasswordStoreAndroidBackend::MetricsRecorder::MetricsRecorder(
    MetricInfix metric_infix)
    : metric_infix_(std::move(metric_infix)) {}

PasswordStoreAndroidBackend::MetricsRecorder::MetricsRecorder(
    MetricsRecorder&&) = default;

PasswordStoreAndroidBackend::MetricsRecorder&
PasswordStoreAndroidBackend::MetricsRecorder::MetricsRecorder::operator=(
    MetricsRecorder&&) = default;

PasswordStoreAndroidBackend::MetricsRecorder::~MetricsRecorder() = default;

void PasswordStoreAndroidBackend::MetricsRecorder::RecordMetrics(
    bool success,
    absl::optional<AndroidBackendError> error) const {
  auto BuildMetricName = [this](base::StringPiece suffix) {
    return base::StrCat({"PasswordManager.PasswordStoreAndroidBackend.",
                         *metric_infix_, ".", suffix});
  };
  base::TimeDelta duration = base::Time::Now() - start_;
  base::UmaHistogramMediumTimes(BuildMetricName("Latency"), duration);
  base::UmaHistogramBoolean(BuildMetricName("Success"), success);
  if (!error.has_value())
    return;

  DCHECK(!success);
  // In case of AndroidBackend error, we report additional metrics.
  base::UmaHistogramEnumeration(
      "PasswordManager.PasswordStoreAndroidBackend.ErrorCode",
      error.value().type);
  base::UmaHistogramEnumeration(BuildMetricName("ErrorCode"),
                                error.value().type);
  if (error.value().type == AndroidBackendErrorType::kExternalError) {
    DCHECK(error.value().api_error_code.has_value());
    base::HistogramBase* histogram = base::SparseHistogram::FactoryGet(
        "PasswordManager.PasswordStoreAndroidBackend.APIError",
        base::HistogramBase::kUmaTargetedHistogramFlag);
    histogram->Add(error.value().api_error_code.value());
    histogram = base::SparseHistogram::FactoryGet(
        BuildMetricName("APIError"),
        base::HistogramBase::kUmaTargetedHistogramFlag);
    histogram->Add(error.value().api_error_code.value());
  }
}

PasswordStoreAndroidBackend::JobReturnHandler::JobReturnHandler() = default;

PasswordStoreAndroidBackend::JobReturnHandler::JobReturnHandler(
    LoginsOrErrorReply callback,
    MetricsRecorder metrics_recorder)
    : success_callback_(std::move(callback)),
      metrics_recorder_(std::move(metrics_recorder)) {}

PasswordStoreAndroidBackend::JobReturnHandler::JobReturnHandler(
    PasswordStoreChangeListReply callback,
    MetricsRecorder metrics_recorder)
    : success_callback_(std::move(callback)),
      metrics_recorder_(std::move(metrics_recorder)) {}

PasswordStoreAndroidBackend::JobReturnHandler::JobReturnHandler(
    JobReturnHandler&&) = default;
PasswordStoreAndroidBackend::JobReturnHandler&

PasswordStoreAndroidBackend::JobReturnHandler::JobReturnHandler::operator=(
    JobReturnHandler&&) = default;

PasswordStoreAndroidBackend::JobReturnHandler::~JobReturnHandler() = default;

void PasswordStoreAndroidBackend::JobReturnHandler::RecordMetrics(
    absl::optional<AndroidBackendError> error) const {
  metrics_recorder_.RecordMetrics(!error.has_value(), std::move(error));
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

base::WeakPtr<PasswordStoreBackend> PasswordStoreAndroidBackend::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

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

void PasswordStoreAndroidBackend::GetAllLoginsAsync(
    LoginsOrErrorReply callback) {
  GetAllLoginsForTarget(PasswordStoreOperationTarget::kDefault,
                        std::move(callback));
}

void PasswordStoreAndroidBackend::GetAutofillableLoginsAsync(
    LoginsOrErrorReply callback) {
  JobId job_id = bridge_->GetAutofillableLogins();
  QueueNewJob(job_id,
              JobReturnHandler(
                  std::move(callback),
                  MetricsRecorder(MetricInfix("GetAutofillableLoginsAsync"))));
}

void PasswordStoreAndroidBackend::FillMatchingLoginsAsync(
    LoginsReply callback,
    bool include_psl,
    const std::vector<PasswordFormDigest>& forms) {
  if (forms.empty()) {
    std::move(callback).Run({});
    return;
  }

  // Record FillMatchingLoginsAsync metrics prior to invoking |callback|.
  LoginsOrErrorReply record_metrics_and_reply =
      ReportMetricsAndInvokeCallbackForLoginsRetrieval(
          MetricInfix("FillMatchingLoginsAsync"), std::move(callback));

  // Create a barrier callback that aggregates results of a multiple
  // calls to GetLoginsAsync.
  auto barrier_callback = base::BarrierCallback<LoginsResultOrError>(
      forms.size(), base::BindOnce(&JoinRetrievedLoginsOrError)
                        .Then(std::move(record_metrics_and_reply)));

  // Create and run a callbacks chain that retrieves logins and invokes
  // |barrier_callback| afterwards.
  base::OnceClosure callbacks_chain = base::DoNothing();
  for (const PasswordFormDigest& form : forms) {
    callbacks_chain = base::BindOnce(
        &PasswordStoreAndroidBackend::GetLoginsAsync,
        weak_ptr_factory_.GetWeakPtr(), std::move(form), include_psl,
        base::BindOnce(barrier_callback).Then(std::move(callbacks_chain)));
  }
  std::move(callbacks_chain).Run();
}

void PasswordStoreAndroidBackend::AddLoginAsync(
    const PasswordForm& form,
    PasswordStoreChangeListReply callback) {
  JobId job_id = bridge_->AddLogin(form);
  QueueNewJob(job_id,
              JobReturnHandler(std::move(callback),
                               MetricsRecorder(MetricInfix("AddLoginAsync"))));
}

void PasswordStoreAndroidBackend::UpdateLoginAsync(
    const PasswordForm& form,
    PasswordStoreChangeListReply callback) {
  JobId job_id = bridge_->UpdateLogin(form);
  QueueNewJob(job_id, JobReturnHandler(
                          std::move(callback),
                          MetricsRecorder(MetricInfix("UpdateLoginAsync"))));
}

void PasswordStoreAndroidBackend::RemoveLoginAsync(
    const PasswordForm& form,
    PasswordStoreChangeListReply callback) {
  RemoveLoginForTarget(form, PasswordStoreOperationTarget::kDefault,
                       std::move(callback));
}

void PasswordStoreAndroidBackend::FilterAndRemoveLogins(
    const base::RepeatingCallback<bool(const GURL&)>& url_filter,
    base::Time delete_begin,
    base::Time delete_end,
    PasswordStoreChangeListReply reply,
    LoginsResultOrError result) {
  if (absl::holds_alternative<PasswordStoreBackendError>(result)) {
    std::move(reply).Run({});
    return;
  }

  LoginsResult logins = std::move(absl::get<LoginsResult>(result));
  std::vector<PasswordForm> logins_to_remove;
  for (const auto& login : logins) {
    if (login->date_created >= delete_begin &&
        login->date_created < delete_end && url_filter.Run(login->url)) {
      logins_to_remove.push_back(std::move(*login));
    }
  }

  // Create a barrier callback that aggregates results of a multiple
  // calls to RemoveLoginAsync.
  auto barrier_callback =
      base::BarrierCallback<absl::optional<PasswordStoreChangeList>>(
          logins_to_remove.size(),
          base::BindOnce(&JoinPasswordStoreChanges).Then(std::move(reply)));

  // Create and run the callback chain that removes the logins.
  base::OnceClosure callbacks_chain = base::DoNothing();
  for (const auto& login : logins_to_remove) {
    callbacks_chain = base::BindOnce(
        &PasswordStoreAndroidBackend::RemoveLoginAsync,
        weak_ptr_factory_.GetWeakPtr(), std::move(login),
        base::BindOnce(barrier_callback).Then(std::move(callbacks_chain)));
  }
  std::move(callbacks_chain).Run();
}

void PasswordStoreAndroidBackend::RemoveLoginsByURLAndTimeAsync(
    const base::RepeatingCallback<bool(const GURL&)>& url_filter,
    base::Time delete_begin,
    base::Time delete_end,
    base::OnceCallback<void(bool)> sync_completion,
    PasswordStoreChangeListReply callback) {
  // Record metrics prior to invoking |callback|.
  PasswordStoreChangeListReply record_metrics_and_reply =
      ReportMetricsAndInvokeCallbackForStoreModifications(
          MetricInfix("RemoveLoginsByURLAndTimeAsync"), std::move(callback));

  JobId get_logins_job_id =
      bridge_->GetAllLogins(PasswordStoreOperationTarget::kDefault);
  QueueNewJob(
      get_logins_job_id,
      JobReturnHandler(
          base::BindOnce(&PasswordStoreAndroidBackend::FilterAndRemoveLogins,
                         weak_ptr_factory_.GetWeakPtr(), std::move(url_filter),
                         delete_begin, delete_end,
                         std::move(record_metrics_and_reply)),
          MetricsRecorder(MetricInfix("GetAllLoginsAsync"))));
}

void PasswordStoreAndroidBackend::RemoveLoginsCreatedBetweenAsync(
    base::Time delete_begin,
    base::Time delete_end,
    PasswordStoreChangeListReply callback) {
  // Record metrics prior to invoking |callback|.
  PasswordStoreChangeListReply record_metrics_and_reply =
      ReportMetricsAndInvokeCallbackForStoreModifications(
          MetricInfix("RemoveLoginsCreatedBetweenAsync"), std::move(callback));

  JobId get_logins_job_id =
      bridge_->GetAllLogins(PasswordStoreOperationTarget::kDefault);
  QueueNewJob(
      get_logins_job_id,
      JobReturnHandler(
          base::BindOnce(&PasswordStoreAndroidBackend::FilterAndRemoveLogins,
                         weak_ptr_factory_.GetWeakPtr(),
                         // Include all urls.
                         base::BindRepeating([](const GURL&) { return true; }),
                         delete_begin, delete_end,
                         std::move(record_metrics_and_reply)),
          MetricsRecorder(MetricInfix("GetAllLoginsAsync"))));
}

void PasswordStoreAndroidBackend::DisableAutoSignInForOriginsAsync(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
    base::OnceClosure completion) {
  // TODO(https://crbug.com/1229655) Switch to using base::PassThrough to handle
  // this callback more gracefully when it's implemented.
  PasswordStoreChangeListReply record_metrics_and_run_completion =
      base::BindOnce(
          [](MetricsRecorder metrics_recorder, base::OnceClosure completion,
             absl::optional<PasswordStoreChangeList> changes) {
            // Errors are not recorded at the moment.
            // TODO(https://crbug.com/1278807): Implement error handling, when
            // actual store changes will be received from the store.
            metrics_recorder.RecordMetrics(/*success=*/true,
                                           /*error=*/absl::nullopt);
            std::move(completion).Run();
          },
          MetricsRecorder(MetricInfix("DisableAutoSignInForOriginsAsync")),
          std::move(completion));

  JobId get_logins_job_id =
      bridge_->GetAllLogins(PasswordStoreOperationTarget::kDefault);
  QueueNewJob(get_logins_job_id,
              JobReturnHandler(
                  base::BindOnce(
                      &PasswordStoreAndroidBackend::FilterAndDisableAutoSignIn,
                      weak_ptr_factory_.GetWeakPtr(), origin_filter,
                      std::move(record_metrics_and_run_completion)),
                  MetricsRecorder(MetricInfix("GetAllLoginsAsync"))));
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

void PasswordStoreAndroidBackend::ClearAllLocalPasswords() {
  LoginsOrErrorReply cleaning_callback = base::BindOnce(
      [](base::WeakPtr<PasswordStoreAndroidBackend> weak_self,
         LoginsResultOrError logins_or_error) {
        if (!weak_self ||
            absl::holds_alternative<PasswordStoreBackendError>(logins_or_error))
          return;

        base::OnceClosure callbacks_chain = base::DoNothing();

        for (const auto& login : absl::get<LoginsResult>(logins_or_error)) {
          callbacks_chain = base::BindOnce(
              &PasswordStoreAndroidBackend::RemoveLoginForTarget, weak_self,
              std::move(*login), PasswordStoreOperationTarget::kLocalStorage,
              IgnoreChangeListAndRunCallback(std::move(callbacks_chain)));
        }

        std::move(callbacks_chain).Run();
      },
      weak_ptr_factory_.GetWeakPtr());

  // TODO(https://crbug.com/1278748) Record whether the operation was
  // successful.
  GetAllLoginsForTarget(PasswordStoreOperationTarget::kLocalStorage,
                        std::move(cleaning_callback));
}

void PasswordStoreAndroidBackend::OnCompleteWithLogins(
    JobId job_id,
    std::vector<PasswordForm> passwords) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  JobReturnHandler reply = GetAndEraseJob(job_id);
  reply.RecordMetrics(/*error=*/absl::nullopt);
  DCHECK(reply.Holds<LoginsOrErrorReply>());
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(reply).Get<LoginsOrErrorReply>(),
                     WrapPasswordsIntoPointers(std::move(passwords))));
}

void PasswordStoreAndroidBackend::OnLoginsChanged(
    JobId job_id,
    absl::optional<PasswordStoreChangeList> changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  JobReturnHandler reply = GetAndEraseJob(job_id);
  reply.RecordMetrics(/*error=*/absl::nullopt);
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
  reply.RecordMetrics(std::move(error));
  if (reply.Holds<LoginsOrErrorReply>()) {
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(reply).Get<LoginsOrErrorReply>(),
                                  PasswordStoreBackendError::kUnspecified));
  } else if (reply.Holds<PasswordStoreChangeListReply>()) {
    // Run callback with empty resulting changelist.
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(reply).Get<PasswordStoreChangeListReply>(),
                       PasswordStoreChangeList()));
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

void PasswordStoreAndroidBackend::GetLoginsAsync(const PasswordFormDigest& form,
                                                 bool include_psl,
                                                 LoginsOrErrorReply callback) {
  JobId job_id = bridge_->GetLoginsForSignonRealm(
      FormToSignonRealmQuery(form, include_psl));
  QueueNewJob(job_id, JobReturnHandler(
                          base::BindOnce(&ValidateSignonRealm, std::move(form),
                                         include_psl, std::move(callback)),
                          MetricsRecorder(MetricInfix("GetLoginsAsync"))));
}

void PasswordStoreAndroidBackend::FilterAndDisableAutoSignIn(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
    PasswordStoreChangeListReply completion,
    LoginsResultOrError result) {
  if (absl::holds_alternative<PasswordStoreBackendError>(result)) {
    std::move(completion).Run({});
    return;
  }

  LoginsResult logins = std::move(absl::get<LoginsResult>(result));
  std::vector<PasswordForm> logins_to_update;
  for (std::unique_ptr<PasswordForm>& login : logins) {
    // Update login if it matches |origin_filer| and has autosignin enabled.
    if (origin_filter.Run(login->url) && !login->skip_zero_click) {
      logins_to_update.push_back(std::move(*login));
      logins_to_update.back().skip_zero_click = true;
    }
  }

  auto barrier_callback =
      base::BarrierCallback<absl::optional<PasswordStoreChangeList>>(
          logins_to_update.size(), base::BindOnce(&JoinPasswordStoreChanges)
                                       .Then(std::move(completion)));

  // Create and run a callbacks chain that updates the logins.
  base::OnceClosure callbacks_chain = base::DoNothing();
  for (PasswordForm& login : logins_to_update) {
    callbacks_chain = base::BindOnce(
        &PasswordStoreAndroidBackend::UpdateLoginAsync,
        weak_ptr_factory_.GetWeakPtr(), std::move(login),
        base::BindOnce(barrier_callback).Then(std::move(callbacks_chain)));
  }
  std::move(callbacks_chain).Run();
}

// static
LoginsOrErrorReply
PasswordStoreAndroidBackend::ReportMetricsAndInvokeCallbackForLoginsRetrieval(
    const MetricInfix& metric_infix,
    LoginsReply callback) {
  // TODO(https://crbug.com/1229655) Switch to using base::PassThrough to handle
  // this callback more gracefully when it's implemented.
  return base::BindOnce(
      [](MetricsRecorder metrics_recorder, LoginsReply callback,
         LoginsResultOrError results) {
        metrics_recorder.RecordMetrics(
            /*success=*/!(
                absl::holds_alternative<PasswordStoreBackendError>(results)),
            /*error=*/absl::nullopt);
        std::move(callback).Run(
            GetLoginsOrEmptyListOnFailure(std::move(results)));
      },
      MetricsRecorder(metric_infix), std::move(callback));
}

// static
PasswordStoreChangeListReply PasswordStoreAndroidBackend::
    ReportMetricsAndInvokeCallbackForStoreModifications(
        const MetricInfix& metric_infix,
        PasswordStoreChangeListReply callback) {
  // TODO(https://crbug.com/1229655) Switch to using base::PassThrough to handle
  // this callback more gracefully when it's implemented.
  return base::BindOnce(
      [](MetricsRecorder metrics_recorder,
         PasswordStoreChangeListReply callback,
         absl::optional<PasswordStoreChangeList> results) {
        // Errors are not recorded at the moment.
        // TODO(https://crbug.com/1278807): Implement error handling, when
        // actual store changes will be received from the store.
        metrics_recorder.RecordMetrics(/*success=*/true,
                                       /*error=*/absl::nullopt);
        std::move(callback).Run(std::move(results));
      },
      MetricsRecorder(metric_infix), std::move(callback));
}

void PasswordStoreAndroidBackend::GetAllLoginsForTarget(
    PasswordStoreOperationTarget target,
    LoginsOrErrorReply callback) {
  JobId job_id = bridge_->GetAllLogins(target);
  QueueNewJob(job_id, JobReturnHandler(
                          std::move(callback),
                          MetricsRecorder(MetricInfix("GetAllLoginsAsync"))));
}

void PasswordStoreAndroidBackend::RemoveLoginForTarget(
    const PasswordForm& form,
    PasswordStoreOperationTarget target,
    PasswordStoreChangeListReply callback) {
  JobId job_id = bridge_->RemoveLogin(form, target);
  QueueNewJob(job_id, JobReturnHandler(
                          std::move(callback),
                          MetricsRecorder(MetricInfix("RemoveLoginAsync"))));
}

}  // namespace password_manager
