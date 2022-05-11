// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_store_android_backend.h"

#include <jni.h>
#include <list>
#include <memory>
#include <vector>

#include "base/barrier_callback.h"
#include "base/callback.h"
#include "base/debug/dump_without_crashing.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/sparse_histogram.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/password_manager/android/password_manager_lifecycle_helper_impl.h"
#include "chrome/browser/password_manager/android/password_store_android_backend_bridge.h"
#include "chrome/browser/password_manager/android/password_store_operation_target.h"
#include "chrome/browser/password_manager/android/password_sync_controller_delegate_android.h"
#include "chrome/browser/password_manager/android/password_sync_controller_delegate_bridge_impl.h"
#include "components/autofill/core/browser/autofill_regexes.h"
#include "components/password_manager/core/browser/login_database.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store_backend.h"
#include "components/password_manager/core/browser/password_store_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "components/sync/model/proxy_model_type_controller_delegate.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace password_manager {

namespace {

// Tasks that are older than this timeout are cleaned up whenever Chrome starts
// a new foreground session since it's likely that Chrome missed the response.
constexpr base::TimeDelta kAsyncTaskTimeout = base::Seconds(30);

using autofill::MatchesPattern;
using base::UTF8ToUTF16;
using password_manager::GetExpressionForFederatedMatching;
using password_manager::GetRegexForPSLFederatedMatching;
using password_manager::GetRegexForPSLMatching;

using JobId = PasswordStoreAndroidBackendBridge::JobId;
using SuccessStatus = PasswordStoreBackendMetricsRecorder::SuccessStatus;

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

PasswordStoreAndroidBackendBridge::Account GetAccount(
    absl::optional<std::string> syncing_account) {
  if (syncing_account.has_value()) {
    return PasswordStoreAndroidBackendBridge::SyncingAccount(
        syncing_account.value());
  }
  return PasswordStoreOperationTarget::kLocalStorage;
}

SuccessStatus GetSuccessStatusFromError(
    const absl::optional<AndroidBackendError>& error) {
  if (!error.has_value())
    return SuccessStatus::kSuccess;
  switch (error.value().type) {
    case AndroidBackendErrorType::kCleanedUpWithoutResponse:
      return SuccessStatus::kCancelled;
    case AndroidBackendErrorType::kUncategorized:
    case AndroidBackendErrorType::kNoContext:
    case AndroidBackendErrorType::kNoAccount:
    case AndroidBackendErrorType::kProfileNotInitialized:
    case AndroidBackendErrorType::kSyncServiceUnavailable:
    case AndroidBackendErrorType::kPassphraseNotSupported:
    case AndroidBackendErrorType::kGMSVersionNotSupported:
    case AndroidBackendErrorType::kExternalError:
      return SuccessStatus::kError;
  }
  NOTREACHED();
  return SuccessStatus::kError;
}

}  // namespace

class PasswordStoreAndroidBackend::ClearAllLocalPasswordsMetricRecorder {
 public:
  explicit ClearAllLocalPasswordsMetricRecorder(
      PasswordStoreBackendMetricsRecorder metrics_recorder)
      : metrics_recorder_(std::move(metrics_recorder)) {}

  void OnAllRemovalsFinished() {
    metrics_recorder_.RecordMetrics(SuccessStatus::kSuccess,
                                    /*error=*/absl::nullopt);
    base::UmaHistogramCounts1M(
        "PasswordManager.PasswordStoreAndroidBackend.ClearAllLocalPasswords."
        "LoginsToRemove",
        total_count_);
    if (total_count_ != 0) {
      size_t success_rate =
          100 * (total_count_ - failure_count_) / total_count_;
      base::UmaHistogramPercentage(
          "PasswordManager.PasswordStoreAndroidBackend.ClearAllLocalPasswords."
          "SuccessRate",
          success_rate);
    }
  }

  void OnLoginRemoved(absl::optional<PasswordStoreChangeList> change_list) {
    if (change_list && change_list.value().empty())
      failure_count_++;
    total_count_++;
  }

 private:
  int total_count_ = 0;
  int failure_count_ = 0;
  PasswordStoreBackendMetricsRecorder metrics_recorder_;
};

PasswordStoreAndroidBackend::JobReturnHandler::JobReturnHandler() = default;

PasswordStoreAndroidBackend::JobReturnHandler::JobReturnHandler(
    LoginsOrErrorReply callback,
    PasswordStoreBackendMetricsRecorder metrics_recorder)
    : success_callback_(std::move(callback)),
      metrics_recorder_(std::move(metrics_recorder)) {}

PasswordStoreAndroidBackend::JobReturnHandler::JobReturnHandler(
    PasswordStoreChangeListReply callback,
    PasswordStoreBackendMetricsRecorder metrics_recorder)
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
  metrics_recorder_.RecordMetrics(GetSuccessStatusFromError(error),
                                  std::move(error));
}

base::TimeDelta
PasswordStoreAndroidBackend::JobReturnHandler::GetElapsedTimeSinceStart()
    const {
  // The recorder is always created right before the task starts.
  return metrics_recorder_.GetElapsedTimeSinceCreation();
}

PasswordStoreAndroidBackend::PasswordStoreAndroidBackend(
    std::unique_ptr<SyncDelegate> sync_delegate)
    : lifecycle_helper_(std::make_unique<PasswordManagerLifecycleHelperImpl>()),
      bridge_(PasswordStoreAndroidBackendBridge::Create()),
      sync_delegate_(std::move(sync_delegate)) {
  DCHECK(bridge_);
  bridge_->SetConsumer(weak_ptr_factory_.GetWeakPtr());
  sync_controller_delegate_ =
      std::make_unique<PasswordSyncControllerDelegateAndroid>(
          std::make_unique<PasswordSyncControllerDelegateBridgeImpl>(),
          sync_delegate_.get());
}

PasswordStoreAndroidBackend::PasswordStoreAndroidBackend(
    base::PassKey<class PasswordStoreAndroidBackendTest>,
    std::unique_ptr<PasswordStoreAndroidBackendBridge> bridge,
    std::unique_ptr<PasswordManagerLifecycleHelper> lifecycle_helper,
    std::unique_ptr<SyncDelegate> sync_delegate,
    std::unique_ptr<PasswordSyncControllerDelegateAndroid>
        sync_controller_delegate)
    : lifecycle_helper_(std::move(lifecycle_helper)),
      bridge_(std::move(bridge)),
      sync_delegate_(std::move(sync_delegate)),
      sync_controller_delegate_(std::move(sync_controller_delegate)) {
  DCHECK(bridge_);
  bridge_->SetConsumer(weak_ptr_factory_.GetWeakPtr());
}

PasswordStoreAndroidBackend::~PasswordStoreAndroidBackend() = default;

void PasswordStoreAndroidBackend::InitBackend(
    RemoteChangesReceived stored_passwords_changed,
    base::RepeatingClosure sync_enabled_or_disabled_cb,
    base::OnceCallback<void(bool)> completion) {
  main_task_runner_ = base::SequencedTaskRunnerHandle::Get();
  stored_passwords_changed_ = std::move(stored_passwords_changed);
  lifecycle_helper_->RegisterObserver(base::BindRepeating(
      &PasswordStoreAndroidBackend::OnForegroundSessionStart,
      base::Unretained(this)));
  // TODO(https://crbug.com/1229650): Create subscription before completion.
  std::move(completion).Run(/*success=*/true);
}

void PasswordStoreAndroidBackend::Shutdown(
    base::OnceClosure shutdown_completed) {
  lifecycle_helper_->UnregisterObserver();
  // TODO(https://crbug.com/1229654): Implement (e.g. unsubscribe from GMS).
  std::move(shutdown_completed).Run();
}

void PasswordStoreAndroidBackend::GetAllLoginsAsync(
    LoginsOrErrorReply callback) {
  GetAllLoginsForAccount(GetAccount(sync_delegate_->GetSyncingAccount()),
                         std::move(callback));
}

void PasswordStoreAndroidBackend::GetAutofillableLoginsAsync(
    LoginsOrErrorReply callback) {
  JobId job_id = bridge_->GetAutofillableLogins(
      GetAccount(sync_delegate_->GetSyncingAccount()));
  QueueNewJob(job_id, std::move(callback),
              MetricInfix("GetAutofillableLoginsAsync"));
}

void PasswordStoreAndroidBackend::GetAllLoginsForAccountAsync(
    absl::optional<std::string> account,
    LoginsOrErrorReply callback) {
  DCHECK(account.has_value());
  GetAllLoginsForAccount(GetAccount(account), std::move(callback));
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
  JobId job_id =
      bridge_->AddLogin(form, GetAccount(sync_delegate_->GetSyncingAccount()));
  QueueNewJob(job_id, std::move(callback), MetricInfix("AddLoginAsync"));
}

void PasswordStoreAndroidBackend::UpdateLoginAsync(
    const PasswordForm& form,
    PasswordStoreChangeListReply callback) {
  JobId job_id = bridge_->UpdateLogin(
      form, GetAccount(sync_delegate_->GetSyncingAccount()));
  QueueNewJob(job_id, std::move(callback), MetricInfix("UpdateLoginAsync"));
}

void PasswordStoreAndroidBackend::RemoveLoginAsync(
    const PasswordForm& form,
    PasswordStoreChangeListReply callback) {
  RemoveLoginForAccount(form, GetAccount(sync_delegate_->GetSyncingAccount()),
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

  GetAllLoginsAsync(base::BindOnce(
      &PasswordStoreAndroidBackend::FilterAndRemoveLogins,
      weak_ptr_factory_.GetWeakPtr(), std::move(url_filter), delete_begin,
      delete_end, std::move(record_metrics_and_reply)));
}

void PasswordStoreAndroidBackend::RemoveLoginsCreatedBetweenAsync(
    base::Time delete_begin,
    base::Time delete_end,
    PasswordStoreChangeListReply callback) {
  // Record metrics prior to invoking |callback|.
  PasswordStoreChangeListReply record_metrics_and_reply =
      ReportMetricsAndInvokeCallbackForStoreModifications(
          MetricInfix("RemoveLoginsCreatedBetweenAsync"), std::move(callback));

  GetAllLoginsAsync(base::BindOnce(
      &PasswordStoreAndroidBackend::FilterAndRemoveLogins,
      weak_ptr_factory_.GetWeakPtr(),
      // Include all urls.
      base::BindRepeating([](const GURL&) { return true; }), delete_begin,
      delete_end, std::move(record_metrics_and_reply)));
}

void PasswordStoreAndroidBackend::DisableAutoSignInForOriginsAsync(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
    base::OnceClosure completion) {
  // TODO(https://crbug.com/1229655) Switch to using base::PassThrough to handle
  // this callback more gracefully when it's implemented.
  PasswordStoreChangeListReply record_metrics_and_run_completion =
      base::BindOnce(
          [](PasswordStoreBackendMetricsRecorder metrics_recorder,
             base::OnceClosure completion,
             absl::optional<PasswordStoreChangeList> changes) {
            // Errors are not recorded at the moment.
            // TODO(https://crbug.com/1278807): Implement error handling, when
            // actual store changes will be received from the store.
            metrics_recorder.RecordMetrics(SuccessStatus::kSuccess,
                                           /*error=*/absl::nullopt);
            std::move(completion).Run();
          },
          PasswordStoreBackendMetricsRecorder(
              BackendInfix("AndroidBackend"),
              MetricInfix("DisableAutoSignInForOriginsAsync")),
          std::move(completion));

  GetAllLoginsAsync(
      base::BindOnce(&PasswordStoreAndroidBackend::FilterAndDisableAutoSignIn,
                     weak_ptr_factory_.GetWeakPtr(), origin_filter,
                     std::move(record_metrics_and_run_completion)));
}

SmartBubbleStatsStore* PasswordStoreAndroidBackend::GetSmartBubbleStatsStore() {
  return nullptr;
}

FieldInfoStore* PasswordStoreAndroidBackend::GetFieldInfoStore() {
  return nullptr;
}

std::unique_ptr<syncer::ProxyModelTypeControllerDelegate>
PasswordStoreAndroidBackend::CreateSyncControllerDelegate() {
  return sync_controller_delegate_->CreateProxyModelControllerDelegate();
}

void PasswordStoreAndroidBackend::ClearAllLocalPasswords() {
  LoginsOrErrorReply cleaning_callback = base::BindOnce(
      [](base::WeakPtr<PasswordStoreAndroidBackend> weak_self,
         PasswordStoreBackendMetricsRecorder metrics_recorder,
         LoginsResultOrError logins_or_error) {
        if (!weak_self || absl::holds_alternative<PasswordStoreBackendError>(
                              logins_or_error)) {
          metrics_recorder.RecordMetrics(SuccessStatus::kError,
                                         /*error=*/absl::nullopt);
          return;
        }

        auto detailed_metric_recorder =
            std::make_unique<ClearAllLocalPasswordsMetricRecorder>(
                std::move(metrics_recorder));

        raw_ptr<ClearAllLocalPasswordsMetricRecorder> raw_recorder =
            detailed_metric_recorder.get();

        base::OnceClosure callbacks_chain = base::BindOnce(
            &ClearAllLocalPasswordsMetricRecorder::OnAllRemovalsFinished,
            std::move(detailed_metric_recorder));

        for (const auto& login : absl::get<LoginsResult>(logins_or_error)) {
          base::OnceCallback record_removal_result = base::BindOnce(
              &ClearAllLocalPasswordsMetricRecorder::OnLoginRemoved,
              // This is safe because |detailed_metric_recorder| will be deleted
              // only after all removals are finished.
              base::Unretained(raw_recorder));

          callbacks_chain = base::BindOnce(
              &PasswordStoreAndroidBackend::RemoveLoginForAccount, weak_self,
              std::move(*login), PasswordStoreOperationTarget::kLocalStorage,
              std::move(record_removal_result)
                  .Then(std::move(callbacks_chain)));
        }

        std::move(callbacks_chain).Run();
      },
      weak_ptr_factory_.GetWeakPtr(),
      PasswordStoreBackendMetricsRecorder(
          BackendInfix("AndroidBackend"),
          MetricInfix("ClearAllLocalPasswords")));

  GetAllLoginsForAccount(PasswordStoreOperationTarget::kLocalStorage,
                         std::move(cleaning_callback));
}

void PasswordStoreAndroidBackend::OnSyncServiceInitialized(
    syncer::SyncService* sync_service) {
  sync_service->AddObserver(sync_controller_delegate_.get());
}

void PasswordStoreAndroidBackend::OnCompleteWithLogins(
    JobId job_id,
    std::vector<PasswordForm> passwords) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  absl::optional<JobReturnHandler> reply = GetAndEraseJob(job_id);
  if (!reply.has_value())
    return;  // Task cleaned up after returning from background.
  reply->RecordMetrics(/*error=*/absl::nullopt);
  DCHECK(reply->Holds<LoginsOrErrorReply>());
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(*reply).Get<LoginsOrErrorReply>(),
                     WrapPasswordsIntoPointers(std::move(passwords))));
}

void PasswordStoreAndroidBackend::OnLoginsChanged(
    JobId job_id,
    absl::optional<PasswordStoreChangeList> changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  absl::optional<JobReturnHandler> reply = GetAndEraseJob(job_id);
  if (!reply.has_value())
    return;  // Task cleaned up after returning from background.
  reply->RecordMetrics(/*error=*/absl::nullopt);
  DCHECK(reply->Holds<PasswordStoreChangeListReply>());

  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(*reply).Get<PasswordStoreChangeListReply>(),
                     changes));
}

void PasswordStoreAndroidBackend::OnError(JobId job_id,
                                          AndroidBackendError error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  absl::optional<JobReturnHandler> reply = GetAndEraseJob(job_id);
  if (!reply.has_value())
    return;  // Task cleaned up after returning from background.
  // TODO(crbug.com/1324588): DCHECK_EQ(api_error_code, 10) to catch dev errors.
  reply->RecordMetrics(std::move(error));
  if (reply->Holds<LoginsOrErrorReply>()) {
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(*reply).Get<LoginsOrErrorReply>(),
                                  PasswordStoreBackendError::kUnspecified));
  } else if (reply->Holds<PasswordStoreChangeListReply>()) {
    // Run callback with empty resulting changelist.
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(*reply).Get<PasswordStoreChangeListReply>(),
                       PasswordStoreChangeList()));
  }
}

template <typename Callback>
void PasswordStoreAndroidBackend::QueueNewJob(JobId job_id,
                                              Callback callback,
                                              MetricInfix metric_infix) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  request_for_job_.emplace(
      job_id,
      JobReturnHandler(std::move(callback), PasswordStoreBackendMetricsRecorder(
                                                BackendInfix("AndroidBackend"),
                                                std::move(metric_infix))));
}

absl::optional<PasswordStoreAndroidBackend::JobReturnHandler>
PasswordStoreAndroidBackend::GetAndEraseJob(JobId job_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  auto iter = request_for_job_.find(job_id);
  if (iter == request_for_job_.end())
    return absl::nullopt;
  JobReturnHandler reply = std::move(iter->second);
  request_for_job_.erase(iter);
  return reply;
}

void PasswordStoreAndroidBackend::GetLoginsAsync(const PasswordFormDigest& form,
                                                 bool include_psl,
                                                 LoginsOrErrorReply callback) {
  JobId job_id = bridge_->GetLoginsForSignonRealm(
      FormToSignonRealmQuery(form, include_psl),
      GetAccount(sync_delegate_->GetSyncingAccount()));
  QueueNewJob(job_id,
              base::BindOnce(&ValidateSignonRealm, std::move(form), include_psl,
                             std::move(callback)),
              MetricInfix("GetLoginsAsync"));
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
      [](PasswordStoreBackendMetricsRecorder metrics_recorder,
         LoginsReply callback, LoginsResultOrError results) {
        metrics_recorder.RecordMetrics(
            absl::holds_alternative<PasswordStoreBackendError>(results)
                ? SuccessStatus::kError
                : SuccessStatus::kSuccess,
            /*error=*/absl::nullopt);
        std::move(callback).Run(
            GetLoginsOrEmptyListOnFailure(std::move(results)));
      },
      PasswordStoreBackendMetricsRecorder(BackendInfix("AndroidBackend"),
                                          metric_infix),
      std::move(callback));
}

// static
PasswordStoreChangeListReply PasswordStoreAndroidBackend::
    ReportMetricsAndInvokeCallbackForStoreModifications(
        const MetricInfix& metric_infix,
        PasswordStoreChangeListReply callback) {
  // TODO(https://crbug.com/1229655) Switch to using base::PassThrough to handle
  // this callback more gracefully when it's implemented.
  return base::BindOnce(
      [](PasswordStoreBackendMetricsRecorder metrics_recorder,
         PasswordStoreChangeListReply callback,
         absl::optional<PasswordStoreChangeList> results) {
        // Errors are not recorded at the moment.
        // TODO(https://crbug.com/1278807): Implement error handling, when
        // actual store changes will be received from the store.
        metrics_recorder.RecordMetrics(SuccessStatus::kSuccess,
                                       /*error=*/absl::nullopt);
        std::move(callback).Run(std::move(results));
      },
      PasswordStoreBackendMetricsRecorder(BackendInfix("AndroidBackend"),
                                          metric_infix),
      std::move(callback));
}

void PasswordStoreAndroidBackend::GetAllLoginsForAccount(
    PasswordStoreAndroidBackendBridge::Account account,
    LoginsOrErrorReply callback) {
  JobId job_id = bridge_->GetAllLogins(std::move(account));
  QueueNewJob(job_id, std::move(callback), MetricInfix("GetAllLoginsAsync"));
}

void PasswordStoreAndroidBackend::RemoveLoginForAccount(
    const PasswordForm& form,
    PasswordStoreAndroidBackendBridge::Account account,
    PasswordStoreChangeListReply callback) {
  JobId job_id = bridge_->RemoveLogin(form, std::move(account));
  QueueNewJob(job_id, std::move(callback), MetricInfix("RemoveLoginAsync"));
}

void PasswordStoreAndroidBackend::OnForegroundSessionStart() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  DCHECK(stored_passwords_changed_);

  // Clear outdated pending tasks before the store queues a new request.
  ClearZombieTasks();

  // Calling the remote form changes with a nullopt means that changes are not
  // available and the store should request all logins asynchronously to
  // invoke `PasswordStoreInterface::Observer::OnLoginsRetained`.
  stored_passwords_changed_.Run(absl::nullopt);
}

void PasswordStoreAndroidBackend::ClearZombieTasks() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  // Collect expired jobs. Deleting them immediately would invalidate iterators.
  std::list<JobId> timed_out_job_ids;
  for (const auto& [id, job] : request_for_job_) {
    if (job.GetElapsedTimeSinceStart() >= kAsyncTaskTimeout) {
      timed_out_job_ids.push_back(id);
    }
  }
  // Erase each timed out job and record that it was cleaned up.
  base::ranges::for_each(timed_out_job_ids, [&](const JobId& job_id) {
    GetAndEraseJob(job_id)->RecordMetrics(AndroidBackendError(
        AndroidBackendErrorType::kCleanedUpWithoutResponse));
  });
}

}  // namespace password_manager
