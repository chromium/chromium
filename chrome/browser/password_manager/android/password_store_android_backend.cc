// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_store_android_backend.h"

#include <jni.h>
#include <cmath>
#include <list>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "base/android/build_info.h"
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
#include "base/time/time.h"
#include "chrome/browser/password_manager/android/password_manager_eviction_util.h"
#include "chrome/browser/password_manager/android/password_manager_lifecycle_helper_impl.h"
#include "chrome/browser/password_manager/android/password_store_android_backend_api_error_codes.h"
#include "chrome/browser/password_manager/android/password_store_android_backend_bridge_helper.h"
#include "chrome/browser/password_manager/android/password_store_android_backend_dispatcher_bridge.h"
#include "chrome/browser/password_manager/android/password_sync_controller_delegate_android.h"
#include "chrome/browser/password_manager/android/password_sync_controller_delegate_bridge_impl.h"
#include "components/autofill/core/common/autofill_regexes.h"
#include "components/password_manager/core/browser/affiliation/affiliated_match_helper.h"
#include "components/password_manager/core/browser/affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/affiliation/affiliations_prefetcher.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_store/android_backend_error.h"
#include "components/password_manager/core/browser/password_store/get_logins_with_affiliations_request_handler.h"
#include "components/password_manager/core/browser/password_store/password_store_backend_metrics_recorder.h"
#include "components/password_manager/core/browser/password_store/password_store_util.h"
#include "components/password_manager/core/browser/password_store/psl_matching_helper.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync/android/explicit_passphrase_platform_client.h"
#include "components/sync/base/features.h"
#include "components/sync/model/proxy_model_type_controller_delegate.h"
#include "components/sync/service/sync_service.h"

namespace password_manager {

namespace {

// Tasks that are older than this timeout are cleaned up whenever Chrome starts
// a new foreground session since it's likely that Chrome missed the response.
constexpr base::TimeDelta kAsyncTaskTimeout = base::Seconds(30);
constexpr char kRetryHistogramBase[] =
    "PasswordManager.PasswordStoreAndroidBackend.Retry";
constexpr char kUPMActiveHistogram[] =
    "PasswordManager.UnifiedPasswordManager.ActiveStatus2";
constexpr base::TimeDelta kTaskRetryTimeout = base::Seconds(16);
// Time in seconds by which calls to the password store happening on startup
// should be delayed.
constexpr base::TimeDelta kPasswordStoreCallDelaySeconds = base::Seconds(5);
constexpr int kMaxReportedRetryAttempts = 10;
constexpr int kMinGmsVersionCodeWithCustomPassphraseApi = 235204000;

using base::UTF8ToUTF16;
using password_manager::GetExpressionForFederatedMatching;
using password_manager::GetRegexForPSLFederatedMatching;
using password_manager::GetRegexForPSLMatching;

using JobId = PasswordStoreAndroidBackendReceiverBridge::JobId;
using SuccessStatus = PasswordStoreBackendMetricsRecorder::SuccessStatus;

std::string GetSyncingAccount(const syncer::SyncService* sync_service) {
  // TODO(crbug.com/1466445): Migrate away from `ConsentLevel::kSync` on
  // Android.
  return sync_util::GetAccountEmailIfSyncFeatureEnabledIncludingPasswords(
      sync_service);
}

std::string FormToSignonRealmQuery(const PasswordFormDigest& form,
                                   bool include_psl) {
  if (include_psl) {
    // Check PSL matches and matches for exact signon realm.
    return GetRegistryControlledDomain(GURL(form.signon_realm));
  }
  if (form.scheme == PasswordForm::Scheme::kHtml &&
      !IsValidAndroidFacetURI(form.signon_realm)) {
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
  if (retrieved_login.signon_realm == form_to_match.signon_realm)
    return true;

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
                                psl_federated_regex))
        return true;
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
  base::EraseIf(absl::get<LoginsResult>(logins_or_error),
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

  password_manager::metrics_util::LogGroupedPasswordsResults(
      absl::get<LoginsResult>(logins_or_error));
  // Remove grouped only matches if filling across groups is disabled.
  if (!base::FeatureList::IsEnabled(
          password_manager::features::kFillingAcrossGroupedSites)) {
    base::EraseIf(absl::get<LoginsResult>(logins_or_error),
                  [](const auto& form) {
                    return form.match_type == PasswordForm::MatchType::kGrouped;
                  });
  }

  std::move(callback).Run(std::move(logins_or_error));
}

LoginsResultOrError JoinRetrievedLoginsOrError(
    std::vector<LoginsResultOrError> results) {
  LoginsResult joined_logins;
  for (auto& result : results) {
    // If one of retrievals ended with an error, pass on the error.
    if (absl::holds_alternative<PasswordStoreBackendError>(result))
      return std::move(absl::get<PasswordStoreBackendError>(result));
    LoginsResult logins = std::move(absl::get<LoginsResult>(result));
    std::move(logins.begin(), logins.end(), std::back_inserter(joined_logins));
  }
  return joined_logins;
}

SuccessStatus GetSuccessStatusFromError(
    const std::optional<AndroidBackendError>& error) {
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
    case AndroidBackendErrorType::kBackendNotAvailable:
    case AndroidBackendErrorType::kFailedToCreateFacetId:
      return SuccessStatus::kError;
  }
  NOTREACHED();
  return SuccessStatus::kError;
}

void LogUPMActiveStatus(syncer::SyncService* sync_service, PrefService* prefs) {
  // TODO(crbug.com/1466445): Migrate away from `ConsentLevel::kSync` on
  // Android.
  if (!sync_util::IsSyncFeatureEnabledIncludingPasswords(sync_service)) {
    base::UmaHistogramEnumeration(
        kUPMActiveHistogram,
        UnifiedPasswordManagerActiveStatus::kInactiveSyncOff);
    return;
  }

  // This check enrolls the client into "RemoveUPMUnenrollment" study allowing us
  // to understand the impact of removing unenrollemnt and percentage of user
  // left without Password Manager / unenrolled from UPM.
  PasswordStoreAndroidBackendDispatcherBridge::CanRemoveUnenrollment();

  if (password_manager_upm_eviction::IsCurrentUserEvicted(prefs)) {
    base::UmaHistogramEnumeration(
        kUPMActiveHistogram,
        UnifiedPasswordManagerActiveStatus::kInactiveUnenrolledDueToErrors);
    return;
  }

  base::UmaHistogramEnumeration(kUPMActiveHistogram,
                                UnifiedPasswordManagerActiveStatus::kActive);
}

std::string GetOperationName(PasswordStoreOperation operation) {
  switch (operation) {
    case PasswordStoreOperation::kGetAllLoginsAsync:
      return "GetAllLoginsAsync";
    case PasswordStoreOperation::kGetAutofillableLoginsAsync:
      return "GetAutofillableLoginsAsync";
    case PasswordStoreOperation::kGetAllLoginsForAccountAsync:
      return "GetAllLoginsForAccountAsync";
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
  NOTREACHED() << "Operation code not handled";
  return "";
}

void RecordRetryHistograms(PasswordStoreOperation operation,
                           AndroidBackendAPIErrorCode api_error_code,
                           base::TimeDelta delay) {
  // Delays are exponential (powers of 2). Original operation delay is 0.
  int attempt = 1;
  if (delay.InSeconds() >= 1)
    attempt = log2(delay.InSeconds()) + 2;

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

enum class ActionOnApiError {
  // See password_manager_upm_eviction::EvictCurrentUser().
  kEvict,
  // See prefs::kSavePasswordsSuspendedByError.
  kDisableSaving,
  // See PasswordStoreAndroidBackend::TryFixPassphraseErrorCb.
  kDisableSavingAndTryFixPassphraseError,
  kRetry,
  kNone,
};

ActionOnApiError GetActionOnApiError(AndroidBackendAPIErrorCode api_error_code,
                                     PasswordStoreOperation operation,
                                     base::TimeDelta delay,
                                     bool can_remove_unenrollment,
                                     bool supports_passphrase_error_fix) {
  switch (api_error_code) {
    case AndroidBackendAPIErrorCode::kAuthErrorResolvable:
    case AndroidBackendAPIErrorCode::kAuthErrorUnresolvable:
      return ActionOnApiError::kDisableSaving;
    case AndroidBackendAPIErrorCode::kPassphraseRequired: {
      return supports_passphrase_error_fix
                 ? ActionOnApiError::kDisableSavingAndTryFixPassphraseError
                 : ActionOnApiError::kEvict;
    }
    case AndroidBackendAPIErrorCode::kNetworkError:
    case AndroidBackendAPIErrorCode::kApiNotConnected:
    case AndroidBackendAPIErrorCode::kConnectionSuspendedDuringCall:
    case AndroidBackendAPIErrorCode::kReconnectionTimedOut:
    case AndroidBackendAPIErrorCode::kBackendGeneric:
      if (operation == PasswordStoreOperation::kGetAllLoginsAsync ||
          operation == PasswordStoreOperation::kGetAutofillableLoginsAsync) {
        // This (error, operation) tuple is generally retriable. Still, impose
        // a max retry timeout. If time ran out...
        // - ...and unenrollment is present, the operation should still not
        // result in eviction (historical artifact).
        // - ...and unenrollment is gone, disable saving.
        return delay < kTaskRetryTimeout
                   ? ActionOnApiError::kRetry
                   : (can_remove_unenrollment ? ActionOnApiError::kDisableSaving
                                              : ActionOnApiError::kNone);
      }
      // Not retriable. Handle with other errors leading to eviction below.
      ABSL_FALLTHROUGH_INTENDED;
    case AndroidBackendAPIErrorCode::kInternalError:
    case AndroidBackendAPIErrorCode::kDeveloperError:
    case AndroidBackendAPIErrorCode::kAccessDenied:
    case AndroidBackendAPIErrorCode::kBadRequest:
    case AndroidBackendAPIErrorCode::kBackendResourceExhausted:
    case AndroidBackendAPIErrorCode::kInvalidData:
    case AndroidBackendAPIErrorCode::kUnmappedErrorCode:
    case AndroidBackendAPIErrorCode::kUnexpectedError:
    case AndroidBackendAPIErrorCode::kKeyRetrievalRequired:
    case AndroidBackendAPIErrorCode::kChromeSyncAPICallError:
    case AndroidBackendAPIErrorCode::kErrorWhileDoingLeakServiceGRPC:
    case AndroidBackendAPIErrorCode::kRequiredSyncingAccountMissing:
    case AndroidBackendAPIErrorCode::kLeakCheckServiceAuthError:
    case AndroidBackendAPIErrorCode::kLeakCheckServiceResourceExhausted:
      break;
  }
  return can_remove_unenrollment ? ActionOnApiError::kDisableSaving
                                 : ActionOnApiError::kEvict;
}

PasswordStoreBackendErrorType APIErrorCodeToErrorType(
    AndroidBackendAPIErrorCode api_error_code,
    bool can_remove_unenrollment) {
  switch (api_error_code) {
    case AndroidBackendAPIErrorCode::kAuthErrorResolvable:
      return PasswordStoreBackendErrorType::kAuthErrorResolvable;
    case AndroidBackendAPIErrorCode::kAuthErrorUnresolvable:
      return PasswordStoreBackendErrorType::kAuthErrorUnresolvable;
    case AndroidBackendAPIErrorCode::kKeyRetrievalRequired:
      return can_remove_unenrollment
                 ? PasswordStoreBackendErrorType::kKeyRetrievalRequired
                 : PasswordStoreBackendErrorType::kUncategorized;
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

PasswordStoreBackendError BackendErrorFromAndroidBackendError(
    const AndroidBackendError& error,
    PasswordStoreOperation operation,
    base::TimeDelta delay,
    bool can_remove_unenrollment,
    bool supports_passphrase_error_fix) {
  if (error.type != AndroidBackendErrorType::kExternalError) {
    return PasswordStoreBackendError(
        PasswordStoreBackendErrorType::kUncategorized,
        PasswordStoreBackendErrorRecoveryType::kUnspecified);
  }

  // External error with no api error code specified should never happen.
  // Treat is as unrecoverable.
  if (!error.api_error_code.has_value()) {
    return PasswordStoreBackendError(
        PasswordStoreBackendErrorType::kUncategorized,
        PasswordStoreBackendErrorRecoveryType::kUnrecoverable);
  }

  AndroidBackendAPIErrorCode api_error_code =
      static_cast<AndroidBackendAPIErrorCode>(error.api_error_code.value());
  PasswordStoreBackendErrorType error_type =
      APIErrorCodeToErrorType(api_error_code, can_remove_unenrollment);

  switch (GetActionOnApiError(api_error_code, operation, delay,
                              can_remove_unenrollment,
                              supports_passphrase_error_fix)) {
    case ActionOnApiError::kRetry:
      return PasswordStoreBackendError(
          error_type, PasswordStoreBackendErrorRecoveryType::kRetriable);
    case ActionOnApiError::kEvict:
      return PasswordStoreBackendError(
          error_type, PasswordStoreBackendErrorRecoveryType::kUnrecoverable);
    // Counterintuitively, kDisableSaving is kRecoverable, as kUnrecoverable is
    // reserved for eviction.
    case ActionOnApiError::kDisableSaving:
    case ActionOnApiError::kNone:
    case ActionOnApiError::kDisableSavingAndTryFixPassphraseError:
      return PasswordStoreBackendError(
          error_type, PasswordStoreBackendErrorRecoveryType::kRecoverable);
  }
  NOTREACHED_NORETURN();
}

}  // namespace

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

bool IsExplicitPassphrasePlatformClientSupported() {
  // TODO(crbug.com/1511304): Don't duplicate these checks. Instead, have
  // SyncService::GetExplicitPassphraseClient() which returns null if they are
  // not satisfied. Then try_fix_passphrase_error_cb_ can also be replaced with
  // faking a ExplicitPassphraseClient method.
  std::string version_code_str =
      base::android::BuildInfo::GetInstance()->gms_version_code();
  int version_code = 0;
  return base::StringToInt(version_code_str, &version_code) &&
         version_code >= kMinGmsVersionCodeWithCustomPassphraseApi &&
         base::FeatureList::IsEnabled(
             syncer::kPassExplicitSyncPassphraseToGmsCore);
}

PasswordStoreAndroidBackend::PasswordStoreAndroidBackend(
    PrefService* prefs,
    AffiliationsPrefetcher* affiliations_prefetcher)
    : lifecycle_helper_(std::make_unique<PasswordManagerLifecycleHelperImpl>()),
      bridge_helper_(PasswordStoreAndroidBackendBridgeHelper::Create()),
      try_fix_passphrase_error_cb_(
          IsExplicitPassphrasePlatformClientSupported()
              ? base::BindRepeating(
                    &syncer::SendExplicitPassphraseToJavaPlatformClient)
              : base::NullCallback()),
      affiliations_prefetcher_(affiliations_prefetcher) {
  DCHECK(bridge_helper_);
  prefs_ = prefs;
  DCHECK(prefs_);
  bridge_helper_->SetConsumer(weak_ptr_factory_.GetWeakPtr());
  sync_controller_delegate_ =
      std::make_unique<PasswordSyncControllerDelegateAndroid>(
          std::make_unique<PasswordSyncControllerDelegateBridgeImpl>(),
          base::BindOnce(&PasswordStoreAndroidBackend::SyncShutdown,
                         weak_ptr_factory_.GetWeakPtr()));
}

PasswordStoreAndroidBackend::PasswordStoreAndroidBackend(
    base::PassKey<class PasswordStoreAndroidBackendTest>,
    std::unique_ptr<PasswordStoreAndroidBackendBridgeHelper> bridge_helper,
    std::unique_ptr<PasswordManagerLifecycleHelper> lifecycle_helper,
    std::unique_ptr<PasswordSyncControllerDelegateAndroid>
        sync_controller_delegate,
    PrefService* prefs,
    const TryFixPassphraseErrorCb& try_fix_passphrase_error_cb,
    AffiliationsPrefetcher* affiliations_prefetcher)
    : lifecycle_helper_(std::move(lifecycle_helper)),
      bridge_helper_(std::move(bridge_helper)),
      sync_controller_delegate_(std::move(sync_controller_delegate)),
      try_fix_passphrase_error_cb_(try_fix_passphrase_error_cb),
      affiliations_prefetcher_(affiliations_prefetcher) {
  DCHECK(bridge_helper_);
  prefs_ = prefs;
  DCHECK(prefs_);
  bridge_helper_->SetConsumer(weak_ptr_factory_.GetWeakPtr());
}

PasswordStoreAndroidBackend::~PasswordStoreAndroidBackend() = default;

void PasswordStoreAndroidBackend::InitBackend(
    AffiliatedMatchHelper* affiliated_match_helper,
    RemoteChangesReceived remote_form_changes_received,
    base::RepeatingClosure sync_enabled_or_disabled_cb,
    base::OnceCallback<void(bool)> completion) {
  // The android backend doesn't currently support notifying the store of
  // sync changes. This currently only wired via the built-in backend being
  // notified by the `PasswordSyncBridge` and generally
  // applies to the account store. Support needs to be specifically implemented
  // if desired. See crbug.com/1004777.
  CHECK(!sync_enabled_or_disabled_cb);
  CHECK(completion);
  affiliated_match_helper_ = affiliated_match_helper;
  main_task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();
  stored_passwords_changed_ = std::move(remote_form_changes_received);
  lifecycle_helper_->RegisterObserver(base::BindRepeating(
      &PasswordStoreAndroidBackend::OnForegroundSessionStart,
      base::Unretained(this)));
  // TODO(https://crbug.com/1229650): Create subscription before completion.
  init_completion_callback_ = std::move(completion);
}

void PasswordStoreAndroidBackend::Shutdown(
    base::OnceClosure shutdown_completed) {
  affiliated_match_helper_ = nullptr;
  sync_service_ = nullptr;
  lifecycle_helper_->UnregisterObserver();
  // TODO(https://crbug.com/1229654): Implement (e.g. unsubscribe from GMS).
  std::move(shutdown_completed).Run();
}

void PasswordStoreAndroidBackend::GetAllLoginsAsync(
    LoginsOrErrorReply callback) {
  CHECK(!init_completion_callback_, base::NotFatalUntil::M123);
  GetAllLoginsForAccountInternal(GetSyncingAccount(sync_service_),
                                 std::move(callback),
                                 PasswordStoreOperation::kGetAllLoginsAsync,
                                 /*delay=*/base::Seconds(0));
}

void PasswordStoreAndroidBackend::GetAllLoginsWithAffiliationAndBrandingAsync(
    LoginsOrErrorReply callback) {
  CHECK(!init_completion_callback_, base::NotFatalUntil::M123);
  if (bridge_helper_->CanUseGetAllLoginsWithBrandingInfoAPI()) {
    JobId job_id = bridge_helper_->GetAllLoginsWithBrandingInfo(
        GetSyncingAccount(sync_service_));
    QueueNewJob(job_id, std::move(callback),
                MetricInfix("GetAllLoginsWithBrandingInfoAsync"),
                PasswordStoreOperation::kGetAllLoginsWithBrandingInfoAsync,
                /*delay=*/base::Seconds(0));
    return;
  }
  auto affiliation_injection = base::BindOnce(
      &PasswordStoreAndroidBackend::InjectAffiliationAndBrandingInformation,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback));
  GetAllLoginsForAccountInternal(GetSyncingAccount(sync_service_),
                                 std::move(affiliation_injection),
                                 PasswordStoreOperation::kGetAllLoginsAsync,
                                 /*delay=*/base::Seconds(0));
}

void PasswordStoreAndroidBackend::GetAutofillableLoginsAsync(
    LoginsOrErrorReply callback) {
  CHECK(!init_completion_callback_, base::NotFatalUntil::M123);
  GetAutofillableLoginsInternal(
      GetSyncingAccount(sync_service_), std::move(callback),
      PasswordStoreOperation::kGetAutofillableLoginsAsync,
      /*delay=*/base::Seconds(0));
}

void PasswordStoreAndroidBackend::GetAllLoginsForAccountAsync(
    std::string account,
    LoginsOrErrorReply callback) {
  CHECK(!account.empty());
  CHECK(!init_completion_callback_, base::NotFatalUntil::M123);

  GetAllLoginsForAccountInternal(
      std::move(account), std::move(callback),
      PasswordStoreOperation::kGetAllLoginsForAccountAsync,
      /*delay=*/base::Seconds(0));
}

void PasswordStoreAndroidBackend::FillMatchingLoginsAsync(
    LoginsOrErrorReply callback,
    bool include_psl,
    const std::vector<PasswordFormDigest>& forms) {
  CHECK(!init_completion_callback_, base::NotFatalUntil::M123);
  if (forms.empty()) {
    std::move(callback).Run(LoginsResult());
    return;
  }

  // Record FillMatchingLoginsAsync metrics prior to invoking |callback|.
  LoginsOrErrorReply record_metrics_and_reply =
      ReportMetricsAndInvokeCallbackForLoginsRetrieval(
          MetricInfix("FillMatchingLoginsAsync"), std::move(callback));

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
        weak_ptr_factory_.GetWeakPtr(), GetSyncingAccount(sync_service_),
        std::move(form), include_psl,
        base::BindOnce(barrier_callback).Then(std::move(callbacks_chain)),
        PasswordStoreOperation::kFillMatchingLoginsAsync);
  }
  std::move(callbacks_chain).Run();
}

void PasswordStoreAndroidBackend::GetGroupedMatchingLoginsAsync(
    const PasswordFormDigest& form_digest,
    LoginsOrErrorReply callback) {
  CHECK(!init_completion_callback_, base::NotFatalUntil::M123);
  if (bridge_helper_->CanUseGetAffiliatedPasswordsAPI()) {
    JobId job_id = bridge_helper_->GetAffiliatedLoginsForSignonRealm(
        form_digest.signon_realm, GetSyncingAccount(sync_service_));
    QueueNewJob(job_id,
                base::BindOnce(&ProcessGroupedLoginsAndReply, form_digest,
                               std::move(callback)),
                MetricInfix("GetGroupedMatchingLoginsAsync"),
                PasswordStoreOperation::kGetGroupedMatchingLoginsAsync,
                /*delay=*/base::Seconds(0));
    return;
  }
  GetLoginsWithAffiliationsRequestHandler(
      form_digest, this, affiliated_match_helper_.get(), std::move(callback));
}

void PasswordStoreAndroidBackend::AddLoginAsync(
    const PasswordForm& form,
    PasswordChangesOrErrorReply callback) {
  CHECK(!init_completion_callback_, base::NotFatalUntil::M123);
  PasswordForm sanitized_form = form;
  if (sanitized_form.blocked_by_user) {
    sanitized_form.username_value.clear();
    sanitized_form.password_value.clear();
  }
  JobId job_id = bridge_helper_->AddLogin(sanitized_form,
                                          GetSyncingAccount(sync_service_));
  QueueNewJob(job_id, std::move(callback), MetricInfix("AddLoginAsync"),
              PasswordStoreOperation::kAddLoginAsync,
              /*delay=*/base::Seconds(0));
}

void PasswordStoreAndroidBackend::UpdateLoginAsync(
    const PasswordForm& form,
    PasswordChangesOrErrorReply callback) {
  CHECK(!init_completion_callback_, base::NotFatalUntil::M123);
  PasswordForm sanitized_form = form;
  if (sanitized_form.blocked_by_user) {
    sanitized_form.username_value.clear();
    sanitized_form.password_value.clear();
  }
  UpdateLoginInternal(GetSyncingAccount(sync_service_), sanitized_form,
                      std::move(callback));
}

void PasswordStoreAndroidBackend::RemoveLoginAsync(
    const PasswordForm& form,
    PasswordChangesOrErrorReply callback) {
  CHECK(!init_completion_callback_, base::NotFatalUntil::M123);
  RemoveLoginInternal(
      GetSyncingAccount(sync_service_), form, std::move(callback),
      PasswordStoreOperation::kRemoveLoginAsync, /*delay=*/base::Seconds(0));
}

void PasswordStoreAndroidBackend::RemoveLoginsByURLAndTimeAsync(
    const base::RepeatingCallback<bool(const GURL&)>& url_filter,
    base::Time delete_begin,
    base::Time delete_end,
    base::OnceCallback<void(bool)> sync_completion,
    PasswordChangesOrErrorReply callback) {
  CHECK(!init_completion_callback_, base::NotFatalUntil::M123);
  // Record metrics prior to invoking |callback|.
  PasswordChangesOrErrorReply record_metrics_and_reply =
      ReportMetricsAndInvokeCallbackForStoreModifications(
          MetricInfix("RemoveLoginsByURLAndTimeAsync"), std::move(callback));

  std::string account = GetSyncingAccount(sync_service_);
  GetAllLoginsForAccountInternal(
      account,
      base::BindOnce(&PasswordStoreAndroidBackend::FilterAndRemoveLogins,
                     weak_ptr_factory_.GetWeakPtr(), account,
                     std::move(url_filter), delete_begin, delete_end,
                     std::move(record_metrics_and_reply),
                     PasswordStoreOperation::kRemoveLoginsByURLAndTimeAsync,
                     /*delay=*/base::Seconds(0)),
      PasswordStoreOperation::kRemoveLoginsByURLAndTimeAsync,
      /*delay=*/base::Seconds(0));
}

void PasswordStoreAndroidBackend::RemoveLoginsCreatedBetweenAsync(
    base::Time delete_begin,
    base::Time delete_end,
    PasswordChangesOrErrorReply callback) {
  CHECK(!init_completion_callback_, base::NotFatalUntil::M123);
  // Record metrics prior to invoking |callback|.
  PasswordChangesOrErrorReply record_metrics_and_reply =
      ReportMetricsAndInvokeCallbackForStoreModifications(
          MetricInfix("RemoveLoginsCreatedBetweenAsync"), std::move(callback));

  GetAllLoginsForAccountInternal(
      GetSyncingAccount(sync_service_),
      base::BindOnce(
          &PasswordStoreAndroidBackend::FilterAndRemoveLogins,
          weak_ptr_factory_.GetWeakPtr(), GetSyncingAccount(sync_service_),
          // Include all urls.
          base::BindRepeating([](const GURL&) { return true; }), delete_begin,
          delete_end, std::move(record_metrics_and_reply),
          PasswordStoreOperation::kRemoveLoginsCreatedBetweenAsync,
          /*delay=*/base::Seconds(0)),
      PasswordStoreOperation::kRemoveLoginsCreatedBetweenAsync,
      /*delay=*/base::Seconds(0));
}

void PasswordStoreAndroidBackend::DisableAutoSignInForOriginsAsync(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
    base::OnceClosure completion) {
  CHECK(!init_completion_callback_, base::NotFatalUntil::M123);
  // TODO(https://crbug.com/1229655) Switch to using base::PassThrough to handle
  // this callback more gracefully when it's implemented.
  PasswordChangesOrErrorReply record_metrics_and_run_completion =
      base::BindOnce(
          [](PasswordStoreBackendMetricsRecorder metrics_recorder,
             base::OnceClosure completion, PasswordChangesOrError changes) {
            // Errors are not recorded at the moment.
            // TODO(https://crbug.com/1278807): Implement error handling, when
            // actual store changes will be received from the store.
            metrics_recorder.RecordMetrics(SuccessStatus::kSuccess,
                                           /*error=*/std::nullopt);
            std::move(completion).Run();
          },
          PasswordStoreBackendMetricsRecorder(
              BackendInfix("AndroidBackend"),
              MetricInfix("DisableAutoSignInForOriginsAsync")),
          std::move(completion));

  std::string account = GetSyncingAccount(sync_service_);
  GetAllLoginsForAccountInternal(
      account,
      base::BindOnce(&PasswordStoreAndroidBackend::FilterAndDisableAutoSignIn,
                     weak_ptr_factory_.GetWeakPtr(), account, origin_filter,
                     std::move(record_metrics_and_run_completion)),
      PasswordStoreOperation::kDisableAutoSignInForOriginsAsync,
      /*delay=*/base::Seconds(0));
}

SmartBubbleStatsStore* PasswordStoreAndroidBackend::GetSmartBubbleStatsStore() {
  return nullptr;
}

std::unique_ptr<syncer::ProxyModelTypeControllerDelegate>
PasswordStoreAndroidBackend::CreateSyncControllerDelegate() {
  return sync_controller_delegate_->CreateProxyModelControllerDelegate();
}

void PasswordStoreAndroidBackend::OnSyncServiceInitialized(
    syncer::SyncService* sync_service) {
  // TODO(crbug.com/1335387) Check if this might be called multiple times
  // without a need for it. If it is don't repeatedly initialize the sync
  // service to make it clear that it's not needed to do so for future readers
  // of the code.
  if (!sync_service_) {
    LogUPMActiveStatus(sync_service, prefs_);
  }
  sync_service_ = sync_service;
  sync_controller_delegate_->OnSyncServiceInitialized(sync_service);

  // `PasswordStore` creation and initialization always happens before
  // `SyncService` creation.
  CHECK(init_completion_callback_);
  // The backend is now considered fully functional.
  std::move(init_completion_callback_).Run(/*success=*/true);

  // Stop fetching affiliations if AndroidBackend can be used and branding info
  // can be obtained directly from the GMS Core backend.
  if (!prefs_->GetBoolean(
          prefs::kUnenrolledFromGoogleMobileServicesDueToErrors) &&
      sync_util::IsSyncFeatureEnabledIncludingPasswords(sync_service_) &&
      bridge_helper_->CanUseGetAllLoginsWithBrandingInfoAPI()) {
    affiliations_prefetcher_->DisablePrefetching();
  }
}

base::WeakPtr<PasswordStoreBackend> PasswordStoreAndroidBackend::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void PasswordStoreAndroidBackend::GetAutofillableLoginsInternal(
    std::string account,
    LoginsOrErrorReply callback,
    PasswordStoreOperation operation,
    base::TimeDelta delay) {
  JobId job_id = bridge_helper_->GetAutofillableLogins(std::move(account));
  QueueNewJob(job_id, std::move(callback),
              MetricInfix("GetAutofillableLoginsAsync"),
              PasswordStoreOperation::kGetAutofillableLoginsAsync, delay);
}

void PasswordStoreAndroidBackend::GetAllLoginsForAccountInternal(
    std::string account,
    LoginsOrErrorReply callback,
    PasswordStoreOperation operation,
    base::TimeDelta delay) {
  JobId job_id = bridge_helper_->GetAllLogins(std::move(account));
  QueueNewJob(job_id, std::move(callback), MetricInfix("GetAllLoginsAsync"),
              operation, delay);
}

void PasswordStoreAndroidBackend::GetLoginsInternal(
    std::string account,
    const PasswordFormDigest& form,
    bool include_psl,
    LoginsOrErrorReply callback,
    PasswordStoreOperation operation) {
  JobId job_id = bridge_helper_->GetLoginsForSignonRealm(
      FormToSignonRealmQuery(form, include_psl), std::move(account));
  // TODO(crbug.com/1491084): Re-design metrics to be less reliant on exact
  // method name and separate external methods from internal ones.
  QueueNewJob(job_id,
              base::BindOnce(&ValidateSignonRealm, std::move(form), include_psl,
                             std::move(callback)),
              MetricInfix("GetLoginsAsync"), operation,
              /*delay=*/base::Seconds(0));
}

void PasswordStoreAndroidBackend::UpdateLoginInternal(
    std::string account,
    const PasswordForm& form,
    PasswordChangesOrErrorReply callback) {
  JobId job_id = bridge_helper_->UpdateLogin(form, std::move(account));
  QueueNewJob(job_id, std::move(callback), MetricInfix("UpdateLoginAsync"),
              PasswordStoreOperation::kUpdateLoginAsync,
              /*delay=*/base::Seconds(0));
}

void PasswordStoreAndroidBackend::RemoveLoginInternal(
    std::string account,
    const PasswordForm& form,
    PasswordChangesOrErrorReply callback,
    PasswordStoreOperation operation,
    base::TimeDelta delay) {
  JobId job_id = bridge_helper_->RemoveLogin(form, std::move(account));
  QueueNewJob(job_id, std::move(callback), MetricInfix("RemoveLoginAsync"),
              operation, delay);
}

void PasswordStoreAndroidBackend::RetryOperation(
    base::OnceCallback<void(base::TimeDelta)> callback,
    base::TimeDelta delay) {
  base::TimeDelta new_delay =
      delay.InSeconds() == 0 ? base::Seconds(1) : delay * 2;
  main_task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(std::move(callback), new_delay), new_delay);
}

void PasswordStoreAndroidBackend::OnCompleteWithLogins(
    JobId job_id,
    std::vector<PasswordForm> passwords) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  std::optional<JobReturnHandler> reply = GetAndEraseJob(job_id);
  if (!reply.has_value())
    return;  // Task cleaned up after returning from background.

  // Since the API call has succeeded, it's safe to reenable saving.
  prefs_->SetBoolean(prefs::kSavePasswordsSuspendedByError, false);

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
  if (!reply.has_value())
    return;  // Task cleaned up after returning from background.
  reply->RecordMetrics(/*error=*/std::nullopt);
  DCHECK(reply->Holds<PasswordChangesOrErrorReply>());

  // Since the API call has succeeded, it's safe to reenable saving.
  prefs_->SetBoolean(prefs::kSavePasswordsSuspendedByError, false);

  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(*reply).Get<PasswordChangesOrErrorReply>(),
                     changes));
}

void PasswordStoreAndroidBackend::OnError(JobId job_id,
                                          AndroidBackendError error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  std::optional<JobReturnHandler> reply = GetAndEraseJob(job_id);
  if (!reply.has_value())
    return;  // Task cleaned up after returning from background.
  PasswordStoreOperation operation = reply->GetOperation();

  // The error to report is computed before potential eviction. This is because
  // eviction resets state which might be used to infer the recovery type of
  // the error.
  base::TimeDelta delay = reply->GetDelay();
  PasswordStoreBackendError reported_error =
      BackendErrorFromAndroidBackendError(
          error, operation, delay, bridge_helper_->CanRemoveUnenrollment(),
          !!try_fix_passphrase_error_cb_);

  if (error.api_error_code.has_value() && sync_service_) {
    // TODO(crbug.com/1324588): DCHECK_EQ(api_error_code,
    // AndroidBackendAPIErrorCode::kDeveloperError) to catch dev errors.
    DCHECK_EQ(AndroidBackendErrorType::kExternalError, error.type);

    int api_error = error.api_error_code.value();
    auto api_error_code = static_cast<AndroidBackendAPIErrorCode>(api_error);

    // TODO(crbug.com/1372343): Extract the retry logic into a separate method.

    // Retry the call if the performed operation in combination with the error
    // was retriable and the time limit was not reached.
    switch (GetActionOnApiError(api_error_code, operation, delay,
                                bridge_helper_->CanRemoveUnenrollment(),
                                !!try_fix_passphrase_error_cb_)) {
      case ActionOnApiError::kRetry: {
        RecordRetryHistograms(operation, api_error_code, delay);
        CHECK(operation == PasswordStoreOperation::kGetAllLoginsAsync ||
              operation == PasswordStoreOperation::kGetAutofillableLoginsAsync);
        const auto method =
            operation == PasswordStoreOperation::kGetAllLoginsAsync
                ? &PasswordStoreAndroidBackend::GetAllLoginsForAccountInternal
                : &PasswordStoreAndroidBackend::GetAutofillableLoginsInternal;
        RetryOperation(
            base::BindOnce(method, weak_ptr_factory_.GetWeakPtr(),
                           GetSyncingAccount(sync_service_),
                           std::move(*reply).Get<LoginsOrErrorReply>(),
                           operation),
            delay);
        return;
      }
      case ActionOnApiError::kEvict: {
        if (!password_manager_upm_eviction::IsCurrentUserEvicted(prefs_)) {
          password_manager_upm_eviction::EvictCurrentUser(api_error, prefs_);
        }
        break;
      }
      case ActionOnApiError::kDisableSavingAndTryFixPassphraseError:
        CHECK(try_fix_passphrase_error_cb_);
        try_fix_passphrase_error_cb_.Run(sync_service_);
        ABSL_FALLTHROUGH_INTENDED;
      case ActionOnApiError::kDisableSaving:
        prefs_->SetBoolean(prefs::kSavePasswordsSuspendedByError, true);
        break;
      case ActionOnApiError::kNone:
        break;
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
                                              MetricInfix metric_infix,
                                              PasswordStoreOperation operation,
                                              base::TimeDelta delay) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  request_for_job_.emplace(
      job_id, JobReturnHandler(
                  std::move(callback),
                  PasswordStoreBackendMetricsRecorder(
                      BackendInfix("AndroidBackend"), std::move(metric_infix)),
                  delay, operation));
}

std::optional<PasswordStoreAndroidBackend::JobReturnHandler>
PasswordStoreAndroidBackend::GetAndEraseJob(JobId job_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  auto iter = request_for_job_.find(job_id);
  if (iter == request_for_job_.end())
    return std::nullopt;
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
    PasswordStoreOperation operation,
    base::TimeDelta delay,
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
        base::BindOnce(barrier_callback).Then(std::move(callbacks_chain)),
        operation, delay);
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
    CHECK(!login.blocked_by_user ||
          (login.username_value.empty() && login.password_value.empty()));
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
    const MetricInfix& metric_infix,
    LoginsOrErrorReply callback) {
  // TODO(https://crbug.com/1229655) Switch to using base::PassThrough to handle
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
                                          metric_infix),
      std::move(callback));
}

// static
PasswordChangesOrErrorReply PasswordStoreAndroidBackend::
    ReportMetricsAndInvokeCallbackForStoreModifications(
        const MetricInfix& metric_infix,
        PasswordChangesOrErrorReply callback) {
  // TODO(https://crbug.com/1229655) Switch to using base::PassThrough to handle
  // this callback more gracefully when it's implemented.
  return base::BindOnce(
      [](PasswordStoreBackendMetricsRecorder metrics_recorder,
         PasswordChangesOrErrorReply callback, PasswordChangesOrError results) {
        // Errors are not recorded at the moment.
        // TODO(https://crbug.com/1278807): Implement error handling, when
        // actual store changes will be received from the store.
        metrics_recorder.RecordMetrics(SuccessStatus::kSuccess,
                                       /*error=*/std::nullopt);
        std::move(callback).Run(std::move(results));
      },
      PasswordStoreBackendMetricsRecorder(BackendInfix("AndroidBackend"),
                                          metric_infix),
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

void PasswordStoreAndroidBackend::SyncShutdown() {
  sync_service_ = nullptr;
}

void PasswordStoreAndroidBackend::InjectAffiliationAndBrandingInformation(
    LoginsOrErrorReply callback,
    LoginsResultOrError forms_or_error) {
  if (!affiliated_match_helper_ ||
      absl::holds_alternative<PasswordStoreBackendError>(forms_or_error) ||
      absl::get<LoginsResult>(forms_or_error).empty()) {
    std::move(callback).Run(std::move(forms_or_error));
    return;
  }
  affiliated_match_helper_->InjectAffiliationAndBrandingInformation(
      std::move(absl::get<LoginsResult>(forms_or_error)), std::move(callback));
}

}  // namespace password_manager
