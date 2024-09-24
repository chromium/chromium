// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_store_android_backend.h"

#include <jni.h>

#include <cmath>
#include <list>
#include <memory>
#include <optional>
#include <vector>

#include "base/barrier_callback.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/password_manager/android/password_manager_eviction_util.h"
#include "chrome/browser/password_manager/android/password_manager_lifecycle_helper_impl.h"
#include "chrome/browser/password_manager/android/password_store_android_backend_api_error_codes.h"
#include "chrome/browser/password_manager/android/password_store_android_backend_bridge_helper.h"
#include "chrome/browser/password_manager/android/password_store_android_backend_dispatcher_bridge.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/autofill/core/common/autofill_regexes.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_store/android_backend_error.h"
#include "components/password_manager/core/browser/password_store/password_store_backend_error.h"
#include "components/password_manager/core/browser/password_store/password_store_backend_metrics_recorder.h"
#include "components/password_manager/core/browser/password_store/password_store_util.h"
#include "components/password_manager/core/browser/password_store/psl_matching_helper.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync/model/proxy_data_type_controller_delegate.h"
#include "components/sync/service/sync_service.h"

namespace password_manager {

namespace {

// Tasks that are older than this timeout are cleaned up whenever Chrome starts
// a new foreground session since it's likely that Chrome missed the response.
constexpr base::TimeDelta kAsyncTaskTimeout = base::Seconds(30);
constexpr char kRetryHistogramBase[] =
    "PasswordManager.PasswordStoreAndroidBackend.Retry";
constexpr base::TimeDelta kTaskRetryTimeout = base::Seconds(16);
// Time in seconds by which calls to the password store happening on startup
// should be delayed.
constexpr base::TimeDelta kPasswordStoreCallDelaySeconds = base::Seconds(5);
constexpr int kMaxReportedRetryAttempts = 10;

using base::UTF8ToUTF16;
using password_manager::GetExpressionForFederatedMatching;
using password_manager::GetRegexForPSLFederatedMatching;
using password_manager::GetRegexForPSLMatching;

using JobId = PasswordStoreAndroidBackendReceiverBridge::JobId;
using SuccessStatus = PasswordStoreBackendMetricsRecorder::SuccessStatus;

std::string FormToSignonRealmQuery(const PasswordFormDigest& form,
                                   bool include_psl) {
  if (include_psl) {
    // Check PSL matches and matches for exact signon realm.
    return GetRegistryControlledDomain(GURL(form.signon_realm));
  }
  if (form.scheme == PasswordForm::Scheme::kHtml &&
      !affiliations::IsValidAndroidFacetURI(form.signon_realm)) {
    // Check federated matches and matches for exact signon realm.
    return form.url.host();
  }
  // Check matches for exact signon realm.
  return form.signon_realm;
}

bool MatchesRegexWithCache(std::u16string_view input,
                           std::u16string_view regex) {
  static base::NoDestructor<autofill::AutofillRegexCache> cache(
      autofill::ThreadSafe(true));
  const icu::RegexPattern* regex_pattern = cache->GetRegexPattern(regex);
  return autofill::MatchesRegex(input, *regex_pattern);
}

bool MatchesIncludedPSLAndFederation(const PasswordForm& retrieved_login,
                                     const PasswordFormDigest& form_to_match,
                                     bool include_psl) {
  if (retrieved_login.signon_realm == form_to_match.signon_realm) {
    return true;
  }

  if (form_to_match.scheme != retrieved_login.scheme) {
    return false;
  }

  std::u16string retrieved_login_signon_realm =
      UTF8ToUTF16(retrieved_login.signon_realm);
  const bool include_federated =
      form_to_match.scheme == PasswordForm::Scheme::kHtml;

  if (include_psl) {
    const std::u16string psl_regex =
        UTF8ToUTF16(GetRegexForPSLMatching(form_to_match.signon_realm));
    if (MatchesRegexWithCache(retrieved_login_signon_realm, psl_regex)) {
      // Ensure match qualifies as PSL Match.
      return IsPublicSuffixDomainMatch(retrieved_login.signon_realm,
                                       form_to_match.signon_realm);
    }
    if (include_federated) {
      const std::u16string psl_federated_regex = UTF8ToUTF16(
          GetRegexForPSLFederatedMatching(form_to_match.signon_realm));
      if (MatchesRegexWithCache(retrieved_login_signon_realm,
                                psl_federated_regex)) {
        return true;
      }
    }
  } else if (include_federated) {
    const std::u16string federated_regex =
        UTF8ToUTF16("^" + GetExpressionForFederatedMatching(form_to_match.url));
    return include_federated &&
           MatchesRegexWithCache(retrieved_login_signon_realm, federated_regex);
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
  std::erase_if(absl::get<LoginsResult>(logins_or_error),
                [&form_digest_to_match, include_psl](const auto& form) {
                  return !MatchesIncludedPSLAndFederation(
                      form, form_digest_to_match, include_psl);
                });
  std::move(callback).Run(std::move(logins_or_error));
}

void ProcessGroupedLoginsAndReply(const PasswordFormDigest& form_digest,
                                  LoginsOrErrorReply callback,
                                  LoginsResultOrError logins_or_error) {
  if (absl::holds_alternative<PasswordStoreBackendError>(logins_or_error)) {
    std::move(callback).Run(std::move(logins_or_error));
    return;
  }
  for (auto& form : absl::get<LoginsResult>(logins_or_error)) {
    switch (GetMatchResult(form, form_digest)) {
      case MatchResult::NO_MATCH:
        // If it's not PSL nor exact match it has to be affiliated or grouped.
        CHECK(form.match_type.has_value());
        break;
      case MatchResult::EXACT_MATCH:
      case MatchResult::FEDERATED_MATCH:
        // Rewrite match type completely for exact matches so it won't be
        // confused as other types.
        form.match_type = PasswordForm::MatchType::kExact;
        break;
      case MatchResult::PSL_MATCH:
      case MatchResult::FEDERATED_PSL_MATCH:
        // PSL match is only possible if form was marked as grouped match.
        CHECK(form.match_type.has_value());
        form.match_type |= PasswordForm::MatchType::kPSL;
        break;
    }
  }

  std::move(callback).Run(std::move(logins_or_error));
}

LoginsResultOrError JoinRetrievedLoginsOrError(
    std::vector<LoginsResultOrError> results) {
  LoginsResult joined_logins;
  for (auto& result : results) {
    // If one of retrievals ended with an error, pass on the error.
    if (absl::holds_alternative<PasswordStoreBackendError>(result)) {
      return std::move(absl::get<PasswordStoreBackendError>(result));
    }
    LoginsResult logins = std::move(absl::get<LoginsResult>(result));
    std::move(logins.begin(), logins.end(), std::back_inserter(joined_logins));
  }
  return joined_logins;
}

SuccessStatus GetSuccessStatusFromError(
    const std::optional<AndroidBackendError>& error) {
  if (!error.has_value()) {
    return SuccessStatus::kSuccess;
  }
  switch (error.value().type) {
    case AndroidBackendErrorType::kCleanedUpWithoutResponse:
      return SuccessStatus::kCancelledTimeout;
    case AndroidBackendErrorType::kCancelledPwdSyncStateChanged:
      return SuccessStatus::kCancelledPwdSyncStateChanged;
    case AndroidBackendErrorType::kUncategorized:
    case AndroidBackendErrorType::kNoContext:
    case AndroidBackendErrorType::kNoAccount:
    case AndroidBackendErrorType::kProfileNotInitialized:
    case AndroidBackendErrorType::kSyncServiceUnavailable:
    case AndroidBackendErrorType::kPassphraseNotSupported:
    case AndroidBackendErrorType::kGMSVersionNotSupported:
    case AndroidBackendErrorType::kExternalError:
    case AndroidBackendErrorType::kBackendNotAvailable:
    case AndroidBackendErrorType::kFailedToCreateFacetId:
      return SuccessStatus::kError;
  }
  NOTREACHED_IN_MIGRATION();
  return SuccessStatus::kError;
}

std::string GetOperationName(PasswordStoreOperation operation) {
  switch (operation) {
    case PasswordStoreOperation::kGetAllLoginsAsync:
      return "GetAllLoginsAsync";
    case PasswordStoreOperation::kGetAutofillableLoginsAsync:
      return "GetAutofillableLoginsAsync";
    case PasswordStoreOperation::kFillMatchingLoginsAsync:
      return "FillMatchingLoginsAsync";
    case PasswordStoreOperation::kAddLoginAsync:
      return "AddLoginAsync";
    case PasswordStoreOperation::kUpdateLoginAsync:
      return "UpdateLoginAsync";
    case PasswordStoreOperation::kRemoveLoginAsync:
      return "RemoveLoginAsync";
    case PasswordStoreOperation::kRemoveLoginsByURLAndTimeAsync:
      return "RemoveLoginsByURLAndTimeAsync";
    case PasswordStoreOperation::kRemoveLoginsCreatedBetweenAsync:
      return "RemoveLoginsCreatedBetweenAsync";
    case PasswordStoreOperation::kDisableAutoSignInForOriginsAsync:
      return "DisableAutoSignInForOriginsAsync";
    case PasswordStoreOperation::kGetGroupedMatchingLoginsAsync:
      return "GetGroupedMatchingLoginsAsync";
    case PasswordStoreOperation::kGetAllLoginsWithBrandingInfoAsync:
      return "GetAllLoginsWithBrandingInfoAsync";
  }
  NOTREACHED_IN_MIGRATION() << "Operation code not handled";
  return "";
}

int GetRetryAttemptFromDelay(base::TimeDelta delay) {
  // Delays are exponential (powers of 2). Original operation delay is 0.
  int attempt = 1;
  if (delay.InSeconds() >= 1) {
    attempt = log2(delay.InSeconds()) + 2;
  }
  return attempt;
}

void RecordRetryHistograms(PasswordStoreOperation operation,
                           AndroidBackendAPIErrorCode api_error_code,
                           base::TimeDelta delay) {
  int attempt = GetRetryAttemptFromDelay(delay);
  // Record per-operation metrics
  base::UmaHistogramSparse(
      base::StrCat(
          {kRetryHistogramBase, ".", GetOperationName(operation), ".APIError"}),
      static_cast<int>(api_error_code));
  base::UmaHistogramExactLinear(
      base::StrCat(
          {kRetryHistogramBase, ".", GetOperationName(operation), ".Attempt"}),
      attempt, kMaxReportedRetryAttempts);

  // Record aggregated metrics
  base::UmaHistogramSparse(base::StrCat({kRetryHistogramBase, ".APIError"}),
                           static_cast<int>(api_error_code));
  base::UmaHistogramExactLinear(base::StrCat({kRetryHistogramBase, ".Attempt"}),
                                attempt, kMaxReportedRetryAttempts);
}

void RecordCancelledRetryMetrics(PasswordStoreOperation operation,
                                 base::TimeDelta delay) {
  int attempt = GetRetryAttemptFromDelay(delay);

  // Record per-operation metrics
  base::UmaHistogramExactLinear(
      base::StrCat({kRetryHistogramBase, ".", GetOperationName(operation),
                    ".CancelledAtAttempt"}),
      attempt, kMaxReportedRetryAttempts);

  base::UmaHistogramExactLinear(
      base::StrCat({kRetryHistogramBase, ".CancelledAtAttempt"}), attempt,
      kMaxReportedRetryAttempts);
}
enum class ActionOnApiError {
  // See password_manager_upm_eviction::EvictCurrentUser().
  kEvict,
  // See prefs::kSavePasswordsSuspendedByError.
  kDisableSaving,
  // See PasswordStoreAndroidBackend::TryFixPassphraseErrorCb.
  kDisableSavingAndTryFixPassphraseError,
  kRetry,
};

bool ShouldRetryOperationOnError(PasswordStoreOperation operation,
                                 AndroidBackendAPIErrorCode api_error_code,
                                 base::TimeDelta delay) {
  const base::flat_set<PasswordStoreOperation> kRetriableOperations = {
      PasswordStoreOperation::kGetAllLoginsAsync,
      PasswordStoreOperation::kGetAutofillableLoginsAsync,
  };
  const base::flat_set<AndroidBackendAPIErrorCode> kRetriableErrors = {
      AndroidBackendAPIErrorCode::kNetworkError,
      AndroidBackendAPIErrorCode::kApiNotConnected,
      AndroidBackendAPIErrorCode::kConnectionSuspendedDuringCall,
      AndroidBackendAPIErrorCode::kReconnectionTimedOut,
      AndroidBackendAPIErrorCode::kBackendGeneric};
  return delay < kTaskRetryTimeout &&
         kRetriableOperations.contains(operation) &&
         kRetriableErrors.contains(
             static_cast<AndroidBackendAPIErrorCode>(api_error_code));
}

PasswordStoreBackendErrorType APIErrorCodeToErrorType(
    AndroidBackendAPIErrorCode api_error_code) {
  switch (api_error_code) {
    case AndroidBackendAPIErrorCode::kAuthErrorResolvable:
      return PasswordStoreBackendErrorType::kAuthErrorResolvable;
    case AndroidBackendAPIErrorCode::kAuthErrorUnresolvable:
      return PasswordStoreBackendErrorType::kAuthErrorUnresolvable;
    case AndroidBackendAPIErrorCode::kKeyRetrievalRequired:
      return PasswordStoreBackendErrorType::kKeyRetrievalRequired;
    case AndroidBackendAPIErrorCode::kNetworkError:
    case AndroidBackendAPIErrorCode::kInternalError:
    case AndroidBackendAPIErrorCode::kDeveloperError:
    case AndroidBackendAPIErrorCode::kApiNotConnected:
    case AndroidBackendAPIErrorCode::kConnectionSuspendedDuringCall:
    case AndroidBackendAPIErrorCode::kReconnectionTimedOut:
    case AndroidBackendAPIErrorCode::kPassphraseRequired:
    case AndroidBackendAPIErrorCode::kAccessDenied:
    case AndroidBackendAPIErrorCode::kBadRequest:
    case AndroidBackendAPIErrorCode::kBackendGeneric:
    case AndroidBackendAPIErrorCode::kBackendResourceExhausted:
    case AndroidBackendAPIErrorCode::kInvalidData:
    case AndroidBackendAPIErrorCode::kUnmappedErrorCode:
    case AndroidBackendAPIErrorCode::kUnexpectedError:
    case AndroidBackendAPIErrorCode::kChromeSyncAPICallError:
    case AndroidBackendAPIErrorCode::kErrorWhileDoingLeakServiceGRPC:
    case AndroidBackendAPIErrorCode::kRequiredSyncingAccountMissing:
    case AndroidBackendAPIErrorCode::kLeakCheckServiceAuthError:
    case AndroidBackendAPIErrorCode::kLeakCheckServiceResourceExhausted:
      return PasswordStoreBackendErrorType::kUncategorized;
  }
  // The api_error_code is determined by static casting an int. It is thus
  // possible for the value to not be among the explicit enum values, however
  // that case should still be handled. Not adding a default statement to the
  // switch, so that the compiler still warns when a new enum value is added and
  // not explicitly handled here.
  return PasswordStoreBackendErrorType::kUncategorized;
}

}  // namespace

PasswordStoreAndroidBackend::PasswordStoreAndroidBackend(
    std::unique_ptr<PasswordStoreAndroidBackendBridgeHelper> bridge_helper,
    std::unique_ptr<PasswordManagerLifecycleHelper> lifecycle_helper,
    PrefService* prefs)
    : lifecycle_helper_(std::move(lifecycle_helper)),
      bridge_helper_(std::move(bridge_helper)),
      prefs_(prefs) {
  DCHECK(bridge_helper_);
  DCHECK(prefs_);
  bridge_helper_->SetConsumer(weak_ptr_factory_.GetWeakPtr());
}

PasswordStoreAndroidBackend::~PasswordStoreAndroidBackend() = default;

void PasswordStoreAndroidBackend::Init(
    PasswordStoreBackend::RemoteChangesReceived remote_form_changes_received) {
  main_task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();
  stored_passwords_changed_ = std::move(remote_form_changes_received);
  lifecycle_helper_->RegisterObserver(base::BindRepeating(
      &PasswordStoreAndroidBackend::OnForegroundSessionStart,
      base::Unretained(this)));
}

void PasswordStoreAndroidBackend::Shutdown(
    base::OnceClosure shutdown_completed) {
  lifecycle_helper_->UnregisterObserver();
  std::move(shutdown_completed).Run();
}

void PasswordStoreAndroidBackend::GetAutofillableLoginsInternal(
    std::string account,
    LoginsOrErrorReply callback,
    PasswordStoreOperation operation,
    base::TimeDelta delay) {
  JobId job_id = bridge_helper_->GetAutofillableLogins(std::move(account));
  QueueNewJob(job_id, std::move(callback),
              MethodName("GetAutofillableLoginsAsync"), operation, delay);
}

void PasswordStoreAndroidBackend::
    GetAllLoginsWithAffiliationAndBrandingInternal(
        std::string account,
        LoginsOrErrorReply callback) {
  JobId job_id =
      bridge_helper_->GetAllLoginsWithBrandingInfo(std::move(account));
  QueueNewJob(job_id, std::move(callback),
              MethodName("GetAllLoginsWithBrandingInfoAsync"),
              PasswordStoreOperation::kGetAllLoginsWithBrandingInfoAsync,
              /*delay=*/base::Seconds(0));
}

void PasswordStoreAndroidBackend::GetAllLoginsInternal(
    std::string account,
    LoginsOrErrorReply callback,
    PasswordStoreOperation operation,
    base::TimeDelta delay) {
  JobId job_id = bridge_helper_->GetAllLogins(std::move(account));
  QueueNewJob(job_id, std::move(callback), MethodName("GetAllLoginsAsync"),
              operation, delay);
}

void PasswordStoreAndroidBackend::GetLoginsInternal(
    std::string account,
    const PasswordFormDigest& form,
    bool include_psl,
    LoginsOrErrorReply callback) {
  JobId job_id = bridge_helper_->GetLoginsForSignonRealm(
      FormToSignonRealmQuery(form, include_psl), std::move(account));
  // TODO(crbug.com/40284943): Re-design metrics to be less reliant on exact
  // method name and separate external methods from internal ones.
  QueueNewJob(job_id,
              base::BindOnce(&ValidateSignonRealm, std::move(form), include_psl,
                             std::move(callback)),
              MethodName("GetLoginsAsync"),
              PasswordStoreOperation::kFillMatchingLoginsAsync,
              /*delay=*/base::Seconds(0));
}

void PasswordStoreAndroidBackend::AddLoginInternal(
    std::string account,
    const PasswordForm& form,
    PasswordChangesOrErrorReply callback) {
  PasswordForm sanitized_form = form;
  if (sanitized_form.blocked_by_user) {
    sanitized_form.username_value.clear();
    sanitized_form.password_value.clear();
  }
  JobId job_id = bridge_helper_->AddLogin(sanitized_form, std::move(account));
  QueueNewJob(job_id, std::move(callback), MethodName("AddLoginAsync"),
              PasswordStoreOperation::kAddLoginAsync,
              /*delay=*/base::Seconds(0));
}

void PasswordStoreAndroidBackend::UpdateLoginInternal(
    std::string account,
    const PasswordForm& form,
    PasswordChangesOrErrorReply callback) {
  PasswordForm sanitized_form = form;
  if (sanitized_form.blocked_by_user) {
    sanitized_form.username_value.clear();
    sanitized_form.password_value.clear();
  }
  JobId job_id =
      bridge_helper_->UpdateLogin(sanitized_form, std::move(account));
  QueueNewJob(job_id, std::move(callback), MethodName("UpdateLoginAsync"),
              PasswordStoreOperation::kUpdateLoginAsync,
              /*delay=*/base::Seconds(0));
}

void PasswordStoreAndroidBackend::RemoveLoginInternal(
    std::string account,
    const PasswordForm& form,
    PasswordChangesOrErrorReply callback) {
  JobId job_id = bridge_helper_->RemoveLogin(form, std::move(account));
  QueueNewJob(job_id, std::move(callback), MethodName("RemoveLoginAsync"),
              PasswordStoreOperation::kRemoveLoginAsync,
              /*delay=*/base::Seconds(0));
}

void PasswordStoreAndroidBackend::FillMatchingLoginsInternal(
    std::string account,
    LoginsOrErrorReply callback,
    bool include_psl,
    const std::vector<PasswordFormDigest>& forms) {
  if (forms.empty()) {
    std::move(callback).Run(LoginsResult());
    return;
  }

  // Record FillMatchingLoginsAsync metrics prior to invoking |callback|.
  LoginsOrErrorReply record_metrics_and_reply =
      ReportMetricsAndInvokeCallbackForLoginsRetrieval(
          MethodName("FillMatchingLoginsAsync"), std::move(callback),
          GetStorageType());

  // Create a barrier callback that aggregates results of a multiple
  // calls to GetLoginsInternal.
  auto barrier_callback = base::BarrierCallback<LoginsResultOrError>(
      forms.size(), base::BindOnce(&JoinRetrievedLoginsOrError)
                        .Then(std::move(record_metrics_and_reply)));

  // Create and run a callbacks chain that retrieves logins and invokes
  // |barrier_callback| afterwards.
  base::OnceClosure callbacks_chain = base::DoNothing();
  for (const PasswordFormDigest& form : forms) {
    callbacks_chain = base::BindOnce(
        &PasswordStoreAndroidBackend::GetLoginsInternal,
        weak_ptr_factory_.GetWeakPtr(), account, std::move(form), include_psl,
        base::BindOnce(barrier_callback).Then(std::move(callbacks_chain)));
  }
  std::move(callbacks_chain).Run();
}

void PasswordStoreAndroidBackend::GetGroupedMatchingLoginsInternal(
    std::string account,
    const PasswordFormDigest& form_digest,
    LoginsOrErrorReply callback) {
  JobId job_id = bridge_helper_->GetAffiliatedLoginsForSignonRealm(
      form_digest.signon_realm, std::move(account));
  QueueNewJob(job_id,
              base::BindOnce(&ProcessGroupedLoginsAndReply, form_digest,
                             std::move(callback)),
              MethodName("GetGroupedMatchingLoginsAsync"),
              PasswordStoreOperation::kGetGroupedMatchingLoginsAsync,
              /*delay=*/base::Seconds(0));
}

void PasswordStoreAndroidBackend::RemoveLoginsByURLAndTimeInternal(
    std::string account,
    const base::RepeatingCallback<bool(const GURL&)>& url_filter,
    base::Time delete_begin,
    base::Time delete_end,
    PasswordChangesOrErrorReply callback) {
  // Record metrics prior to invoking |callback|.
  PasswordChangesOrErrorReply record_metrics_and_reply =
      ReportMetricsAndInvokeCallbackForStoreModifications(
          MethodName("RemoveLoginsByURLAndTimeAsync"), std::move(callback),
          GetStorageType());

  GetAllLoginsInternal(
      account,
      base::BindOnce(&PasswordStoreAndroidBackend::FilterAndRemoveLogins,
                     weak_ptr_factory_.GetWeakPtr(), account,
                     std::move(url_filter), delete_begin, delete_end,
                     std::move(record_metrics_and_reply)),
      PasswordStoreOperation::kRemoveLoginsByURLAndTimeAsync);
}

void PasswordStoreAndroidBackend::RemoveLoginsCreatedBetweenInternal(
    std::string account,
    base::Time delete_begin,
    base::Time delete_end,
    PasswordChangesOrErrorReply callback) {
  // Record metrics prior to invoking |callback|.
  PasswordChangesOrErrorReply record_metrics_and_reply =
      ReportMetricsAndInvokeCallbackForStoreModifications(
          MethodName("RemoveLoginsCreatedBetweenAsync"), std::move(callback),
          GetStorageType());

  GetAllLoginsInternal(
      account,
      base::BindOnce(&PasswordStoreAndroidBackend::FilterAndRemoveLogins,
                     weak_ptr_factory_.GetWeakPtr(), account,
                     // Include all urls.
                     base::BindRepeating([](const GURL&) { return true; }),
                     delete_begin, delete_end,
                     std::move(record_metrics_and_reply)),
      PasswordStoreOperation::kRemoveLoginsCreatedBetweenAsync);
}

void PasswordStoreAndroidBackend::DisableAutoSignInForOriginsInternal(
    std::string account,
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
    base::OnceClosure completion) {
  // TODO(crbug.com/40778511) Switch to using base::PassThrough to
  // handle this callback more gracefully when it's implemented.
  PasswordChangesOrErrorReply record_metrics_and_run_completion =
      base::BindOnce(
          [](PasswordStoreBackendMetricsRecorder metrics_recorder,
             base::OnceClosure completion, PasswordChangesOrError changes) {
            // Errors are not recorded at the moment.
            // TODO(crbug.com/40208332): Implement error handling,
            // when actual store changes will be received from the store.
            metrics_recorder.RecordMetrics(SuccessStatus::kSuccess,
                                           /*error=*/std::nullopt);
            std::move(completion).Run();
          },
          PasswordStoreBackendMetricsRecorder(
              BackendInfix("AndroidBackend"),
              MethodName("DisableAutoSignInForOriginsAsync"), GetStorageType()),
          std::move(completion));

  GetAllLoginsInternal(
      account,
      base::BindOnce(&PasswordStoreAndroidBackend::FilterAndDisableAutoSignIn,
                     weak_ptr_factory_.GetWeakPtr(), account, origin_filter,
                     std::move(record_metrics_and_run_completion)),
      PasswordStoreOperation::kDisableAutoSignInForOriginsAsync);
}

void PasswordStoreAndroidBackend::ClearAllTasksAndReplyWithReason(
    const AndroidBackendError& reason,
    const PasswordStoreBackendError& reply_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);

  // Cancel queued jobs that haven't yet received a reply.
  for (auto& [id, job_reply] : request_for_job_) {
    job_reply.RecordMetrics(reason);
    if (job_reply.Holds<LoginsOrErrorReply>()) {
      main_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(job_reply).Get<LoginsOrErrorReply>(),
                         reply_error));

    } else if (job_reply.Holds<PasswordChangesOrErrorReply>()) {
      main_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(
              std::move(job_reply).Get<PasswordChangesOrErrorReply>(),
              reply_error));
    }
  }
  request_for_job_.clear();

  // Cancel posted delayed retries
  for (const auto& [id, retry_wrapper] : scheduled_retries_) {
    LoginsOrErrorReply reply_callback =
        retry_wrapper->GetReplyCallbackAndCancel();
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(reply_callback), reply_error));
  }
  scheduled_retries_.clear();
}

PasswordStoreAndroidBackend::JobReturnHandler::JobReturnHandler(
    LoginsOrErrorReply callback,
    PasswordStoreBackendMetricsRecorder metrics_recorder,
    base::TimeDelta delay,
    PasswordStoreOperation operation)
    : success_callback_(std::move(callback)),
      metrics_recorder_(std::move(metrics_recorder)),
      delay_(delay),
      operation_(operation) {}

PasswordStoreAndroidBackend::JobReturnHandler::JobReturnHandler(
    PasswordChangesOrErrorReply callback,
    PasswordStoreBackendMetricsRecorder metrics_recorder,
    base::TimeDelta delay,
    PasswordStoreOperation operation)
    : success_callback_(std::move(callback)),
      metrics_recorder_(std::move(metrics_recorder)),
      delay_(delay),
      operation_(operation) {}

PasswordStoreAndroidBackend::JobReturnHandler::JobReturnHandler(
    JobReturnHandler&&) = default;
PasswordStoreAndroidBackend::JobReturnHandler::~JobReturnHandler() = default;

void PasswordStoreAndroidBackend::JobReturnHandler::RecordMetrics(
    std::optional<AndroidBackendError> error) const {
  SuccessStatus sucess_status = GetSuccessStatusFromError(error);
  metrics_recorder_.RecordMetrics(sucess_status, std::move(error));
}

base::TimeDelta
PasswordStoreAndroidBackend::JobReturnHandler::GetElapsedTimeSinceStart()
    const {
  // The recorder is always created right before the task starts.
  return metrics_recorder_.GetElapsedTimeSinceCreation();
}

base::TimeDelta PasswordStoreAndroidBackend::JobReturnHandler::GetDelay() {
  return delay_;
}

PasswordStoreOperation
PasswordStoreAndroidBackend::JobReturnHandler::GetOperation() {
  return operation_;
}

PasswordStoreAndroidBackend::CancellableRetryCallback::CancellableRetryCallback(
    base::OnceCallback<void(LoginsOrErrorReply,
                            PasswordStoreOperation,
                            base::TimeDelta)> callback,
    PasswordStoreOperation operation,
    LoginsOrErrorReply reply_callback,
    base::TimeDelta current_delay)
    : callback_(std::move(callback)),
      operation_(operation),
      reply_callback_(std::move(reply_callback)),
      current_delay_(current_delay) {}
PasswordStoreAndroidBackend::CancellableRetryCallback::
    ~CancellableRetryCallback() = default;

base::WeakPtr<PasswordStoreAndroidBackend::CancellableRetryCallback>
PasswordStoreAndroidBackend::CancellableRetryCallback::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void PasswordStoreAndroidBackend::CancellableRetryCallback::Run() {
  CHECK(callback_);
  std::move(callback_).Run(std::move(reply_callback_), operation_,
                           current_delay_);
}

LoginsOrErrorReply PasswordStoreAndroidBackend::CancellableRetryCallback::
    GetReplyCallbackAndCancel() {
  RecordCancelledRetryMetrics(operation_, current_delay_);
  weak_ptr_factory_.InvalidateWeakPtrs();

  return std::move(reply_callback_);
}

base::OnceCallback<
    void(LoginsOrErrorReply, PasswordStoreOperation, base::TimeDelta)>
PasswordStoreAndroidBackend::GetRetryCallbackForOperation(
    PasswordStoreOperation operation) {
  switch (operation) {
    case PasswordStoreOperation::kGetAllLoginsAsync:
      return base::BindOnce(&PasswordStoreAndroidBackend::GetAllLoginsInternal,
                            weak_ptr_factory_.GetWeakPtr(),
                            GetAccountToRetryOperation());
    case PasswordStoreOperation::kGetAutofillableLoginsAsync:
      return base::BindOnce(
          &PasswordStoreAndroidBackend::GetAutofillableLoginsInternal,
          weak_ptr_factory_.GetWeakPtr(), GetAccountToRetryOperation());
    case PasswordStoreOperation::kFillMatchingLoginsAsync:
    case PasswordStoreOperation::kAddLoginAsync:
    case PasswordStoreOperation::kUpdateLoginAsync:
    case PasswordStoreOperation::kRemoveLoginAsync:
    case PasswordStoreOperation::kRemoveLoginsByURLAndTimeAsync:
    case PasswordStoreOperation::kRemoveLoginsCreatedBetweenAsync:
    case PasswordStoreOperation::kDisableAutoSignInForOriginsAsync:
    case PasswordStoreOperation::kGetGroupedMatchingLoginsAsync:
    case PasswordStoreOperation::kGetAllLoginsWithBrandingInfoAsync:
      NOTREACHED();
  }
}

void PasswordStoreAndroidBackend::RetryOperation(
    PasswordStoreOperation operation,
    AndroidBackendAPIErrorCode api_error_code,
    base::TimeDelta delay,
    LoginsOrErrorReply reply) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  RecordRetryHistograms(operation, api_error_code, delay);

  base::TimeDelta new_delay =
      delay.InSeconds() == 0 ? base::Seconds(1) : delay * 2;

  auto retry =
      std::make_unique<PasswordStoreAndroidBackend::CancellableRetryCallback>(
          GetRetryCallbackForOperation(operation), operation, std::move(reply),
          new_delay);

  DelayedRetryId id = delayed_retry_id_generator_.GenerateNextId();
  base::OnceClosure cleanup =
      base::BindOnce(&PasswordStoreAndroidBackend::CleanupRetryAfterRun,
                     weak_ptr_factory_.GetWeakPtr(), id);

  main_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CancellableRetryCallback::Run, retry->AsWeakPtr())
          .Then(std::move(cleanup)),
      new_delay);
  scheduled_retries_.emplace(id, std::move(retry));
}

void PasswordStoreAndroidBackend::CleanupRetryAfterRun(
    DelayedRetryId retry_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  scheduled_retries_.erase(retry_id);
}

void PasswordStoreAndroidBackend::OnCompleteWithLogins(
    JobId job_id,
    std::vector<PasswordForm> passwords) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  std::optional<JobReturnHandler> reply = GetAndEraseJob(job_id);
  if (!reply.has_value()) {
    return;  // Task cleaned up after returning from background.
  }

  OnCallToGMSCoreSucceeded();
  reply->RecordMetrics(/*error=*/std::nullopt);
  DCHECK(reply->Holds<LoginsOrErrorReply>());
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(*reply).Get<LoginsOrErrorReply>(),
                                std::move(passwords)));
}

void PasswordStoreAndroidBackend::OnLoginsChanged(JobId job_id,
                                                  PasswordChanges changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  std::optional<JobReturnHandler> reply = GetAndEraseJob(job_id);
  if (!reply.has_value()) {
    return;  // Task cleaned up after returning from background.
  }
  reply->RecordMetrics(/*error=*/std::nullopt);
  DCHECK(reply->Holds<PasswordChangesOrErrorReply>());

  OnCallToGMSCoreSucceeded();
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(*reply).Get<PasswordChangesOrErrorReply>(),
                     changes));
}

void PasswordStoreAndroidBackend::OnError(JobId job_id,
                                          AndroidBackendError error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  std::optional<JobReturnHandler> reply = GetAndEraseJob(job_id);
  if (!reply.has_value()) {
    return;  // Task cleaned up after returning from background.
  }

  PasswordStoreOperation operation = reply->GetOperation();

  // The error to report is computed before potential eviction. This is because
  // eviction resets state which might be used to infer the recovery type of
  // the error.
  base::TimeDelta delay = reply->GetDelay();
  PasswordStoreBackendError reported_error(
      PasswordStoreBackendErrorType::kUncategorized);

  if (error.api_error_code.has_value()) {
    // TODO(crbug.com/40839365): DCHECK_EQ(api_error_code,
    // AndroidBackendAPIErrorCode::kDeveloperError) to catch dev errors.
    DCHECK_EQ(AndroidBackendErrorType::kExternalError, error.type);
    int api_error = error.api_error_code.value();
    reported_error.android_backend_api_error = api_error;
    auto api_error_code = static_cast<AndroidBackendAPIErrorCode>(api_error);

    // Retry the call if the performed operation in combination with the error
    // was retriable and the time limit was not reached.
    if (ShouldRetryOperationOnError(operation, api_error_code, delay)) {
      RetryOperation(operation, api_error_code, delay,
                     std::move(*reply).Get<LoginsOrErrorReply>());
      return;
    }

    if (delay < kTaskRetryTimeout) {
      // Either the operation or error is not retriable.
      RecoverOnError(api_error_code);
      reported_error.type = APIErrorCodeToErrorType(api_error_code);
    }
  }

  reply->RecordMetrics(std::move(error));
  // The decision whether to show an error UI depends on the re-enrollment pref
  // and as such the consumers should be called last.
  if (reply->Holds<LoginsOrErrorReply>()) {
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(*reply).Get<LoginsOrErrorReply>(),
                                  reported_error));
    return;
  }
  if (reply->Holds<PasswordChangesOrErrorReply>()) {
    // Run callback with empty resulting changelist.
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(*reply).Get<PasswordChangesOrErrorReply>(),
                       reported_error));
  }
}

template <typename Callback>
void PasswordStoreAndroidBackend::QueueNewJob(JobId job_id,
                                              Callback callback,
                                              MethodName method_name,
                                              PasswordStoreOperation operation,
                                              base::TimeDelta delay) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  request_for_job_.emplace(
      job_id, JobReturnHandler(std::move(callback),
                               PasswordStoreBackendMetricsRecorder(
                                   BackendInfix("AndroidBackend"),
                                   std::move(method_name), GetStorageType()),
                               delay, operation));
}

std::optional<PasswordStoreAndroidBackend::JobReturnHandler>
PasswordStoreAndroidBackend::GetAndEraseJob(JobId job_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  auto iter = request_for_job_.find(job_id);
  if (iter == request_for_job_.end()) {
    return std::nullopt;
  }
  JobReturnHandler reply = std::move(iter->second);
  request_for_job_.erase(iter);
  return reply;
}

void PasswordStoreAndroidBackend::FilterAndRemoveLogins(
    std::string account,
    const base::RepeatingCallback<bool(const GURL&)>& url_filter,
    base::Time delete_begin,
    base::Time delete_end,
    PasswordChangesOrErrorReply reply,
    LoginsResultOrError result) {
  if (absl::holds_alternative<PasswordStoreBackendError>(result)) {
    std::move(reply).Run(
        std::move(absl::get<PasswordStoreBackendError>(result)));
    return;
  }

  LoginsResult logins = std::move(absl::get<LoginsResult>(result));
  std::vector<PasswordForm> logins_to_remove;
  for (auto& login : logins) {
    if (login.date_created >= delete_begin && login.date_created < delete_end &&
        url_filter.Run(login.url)) {
      logins_to_remove.push_back(std::move(login));
    }
  }

  // Create a barrier callback that aggregates results of a multiple
  // calls to RemoveLoginAsync.
  auto barrier_callback = base::BarrierCallback<PasswordChangesOrError>(
      logins_to_remove.size(),
      base::BindOnce(&JoinPasswordStoreChanges).Then(std::move(reply)));

  // Create and run the callback chain that removes the logins.
  base::OnceClosure callbacks_chain = base::DoNothing();
  for (const auto& login : logins_to_remove) {
    callbacks_chain = base::BindOnce(
        &PasswordStoreAndroidBackend::RemoveLoginInternal,
        weak_ptr_factory_.GetWeakPtr(), account, std::move(login),
        base::BindOnce(barrier_callback).Then(std::move(callbacks_chain)));
  }
  std::move(callbacks_chain).Run();
}

void PasswordStoreAndroidBackend::FilterAndDisableAutoSignIn(
    std::string account,
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
    PasswordChangesOrErrorReply completion,
    LoginsResultOrError result) {
  if (absl::holds_alternative<PasswordStoreBackendError>(result)) {
    std::move(completion)
        .Run(std::move(absl::get<PasswordStoreBackendError>(result)));
    return;
  }

  LoginsResult logins = std::move(absl::get<LoginsResult>(result));
  std::vector<PasswordForm> logins_to_update;
  for (auto& login : logins) {
    // Update login if it matches |origin_filer| and has autosignin enabled.
    if (origin_filter.Run(login.url) && !login.skip_zero_click) {
      logins_to_update.push_back(std::move(login));
      logins_to_update.back().skip_zero_click = true;
    }
  }

  auto barrier_callback = base::BarrierCallback<PasswordChangesOrError>(
      logins_to_update.size(),
      base::BindOnce(&JoinPasswordStoreChanges).Then(std::move(completion)));

  // Create and run a callbacks chain that updates the logins.
  base::OnceClosure callbacks_chain = base::DoNothing();
  for (PasswordForm& login : logins_to_update) {
    callbacks_chain = base::BindOnce(
        &PasswordStoreAndroidBackend::UpdateLoginInternal,
        weak_ptr_factory_.GetWeakPtr(), account, std::move(login),
        base::BindOnce(barrier_callback).Then(std::move(callbacks_chain)));
  }
  std::move(callbacks_chain).Run();
}

// static
LoginsOrErrorReply
PasswordStoreAndroidBackend::ReportMetricsAndInvokeCallbackForLoginsRetrieval(
    const MethodName& method_name,
    LoginsOrErrorReply callback,
    PasswordStoreBackendMetricsRecorder::PasswordStoreAndroidBackendType
        store_type) {
  // TODO(crbug.com/40778511) Switch to using base::PassThrough to handle
  // this callback more gracefully when it's implemented.
  return base::BindOnce(
      [](PasswordStoreBackendMetricsRecorder metrics_recorder,
         LoginsOrErrorReply callback, LoginsResultOrError results) {
        metrics_recorder.RecordMetrics(
            absl::holds_alternative<PasswordStoreBackendError>(results)
                ? SuccessStatus::kError
                : SuccessStatus::kSuccess,
            /*error=*/std::nullopt);
        std::move(callback).Run(std::move(results));
      },
      PasswordStoreBackendMetricsRecorder(BackendInfix("AndroidBackend"),
                                          method_name, store_type),
      std::move(callback));
}

// static
PasswordChangesOrErrorReply PasswordStoreAndroidBackend::
    ReportMetricsAndInvokeCallbackForStoreModifications(
        const MethodName& method_name,
        PasswordChangesOrErrorReply callback,
        PasswordStoreBackendMetricsRecorder::PasswordStoreAndroidBackendType
            store_type) {
  // TODO(crbug.com/40778511) Switch to using base::PassThrough to handle
  // this callback more gracefully when it's implemented.
  return base::BindOnce(
      [](PasswordStoreBackendMetricsRecorder metrics_recorder,
         PasswordChangesOrErrorReply callback, PasswordChangesOrError results) {
        // Errors are not recorded at the moment.
        // TODO(crbug.com/40208332): Implement error handling, when
        // actual store changes will be received from the store.
        metrics_recorder.RecordMetrics(SuccessStatus::kSuccess,
                                       /*error=*/std::nullopt);
        std::move(callback).Run(std::move(results));
      },
      PasswordStoreBackendMetricsRecorder(BackendInfix("AndroidBackend"),
                                          method_name, store_type),
      std::move(callback));
}

void PasswordStoreAndroidBackend::OnForegroundSessionStart() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  DCHECK(stored_passwords_changed_);

  // Clear outdated pending tasks before the store queues a new request.
  ClearZombieTasks();

  // If this is the first foregrounding signal, it corresponds to Chrome
  // starting up. In that case, calls to Google Play Services should be delayed
  // as they tend to be resource-intensive.
  if (should_delay_refresh_on_foregrounding_) {
    should_delay_refresh_on_foregrounding_ = false;
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, base::BindOnce(stored_passwords_changed_, std::nullopt),
        kPasswordStoreCallDelaySeconds);
    return;
  }

  // Calling the remote form changes with a nullopt means that changes are not
  // available and the store should request all logins asynchronously to
  // invoke `PasswordStoreInterface::Observer::OnLoginsRetained`.
  stored_passwords_changed_.Run(std::nullopt);
}

// TODO(b/322163027): Merge this with `ClearAllTasksAndReplyWithReason(...)`.
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
    GetAndEraseJob(job_id)->RecordMetrics(AndroidBackendError{
        .type = AndroidBackendErrorType::kCleanedUpWithoutResponse});
  });
}

}  // namespace password_manager
