// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ACTOR_ONE_TIME_TOKENS_ACTOR_ONE_TIME_TOKEN_FILLING_SERVICE_METRICS_H_
#define CHROME_BROWSER_AUTOFILL_ACTOR_ONE_TIME_TOKENS_ACTOR_ONE_TIME_TOKEN_FILLING_SERVICE_METRICS_H_

#include <string_view>

#include "chrome/browser/autofill/actor/one_time_tokens/actor_one_time_token_filling_service.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace autofill {

// LINT.IfChange(ActorOneTimeTokenFillingServiceRetrieveOtp)

// Events recorded during the ActorOneTimeTokenFillingService RetrieveOtp
// operation.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ActorOneTimeTokenFillingServiceRetrieveOtp {
  // kStart = 0,  // Obsolete.
  kNullTab = 1,
  kNoService = 2,
  kSuccessCacheMatchFound = 3,
  // kSuccess = 4,  // Obsolete. Replaced by kSuccessReceivedMatchFound.
  // kError = 5,  // Obsolete. Replaced by the specific error values.
  kMockOtp = 6,
  // kNoCallback = 7,  // Obsolete.
  kSuccessReceivedMatchFound = 8,
  kRetrievalTimeout = 9,
  kGmailOtpBackendAuthError = 10,
  kGmailOtpBackendNetworkError = 11,
  kGmailOtpBackendInvalidResponse = 12,
  kGmailOtpUnknown = 13,
  kGmailOtpBackendApiNotAvailable = 14,
  kGmailOtpBackendInitializationFailed = 15,
  kGmailOtpBackendSmartFeaturesInGmailConsentRequired = 16,
  kGmailOtpBackendSmartFeaturesInOtherGoogleProductsConsentRequired = 17,
  kGmailOtpBackendDmaCrossProductSharingConsentRequired = 18,
  kGmailOtpBackendBadMessageReference = 19,
  kGmailOtpBackendMessageIdNotFound = 20,
  kGmailOtpBackendWrongTokenTypeRequested = 21,
  kGmailOtpBackendOneTimeTokenExpired = 22,
  kGmailOtpBackendOtpAttributeNotFound = 23,
  kGmailOtpBackendServerError = 24,
  kUnknownError = 25,
  kGmailOtpBackendMessageAuthenticationFailed = 26,
  kGmailOtpSenderDomainMismatch = 27,
  // The service was destroyed while a retrieval was pending.
  kCancelled = 28,
  kMaxValue = kCancelled,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/one_time_tokens/enums.xml:ActorOneTimeTokenFillingServiceRetrieveOtpEvent)

// LINT.IfChange(ActorOneTimeTokenFillingServiceFillOtp)

// Events recorded during the ActorOneTimeTokenFillingService FillOtp operation.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ActorOneTimeTokenFillingServiceFillOtp {
  // kStart = 0,  // Obsolete.
  kNullTab = 1,
  kEmptyTriggerFieldIds = 2,
  kConcurrentCall = 3,
  kNoAutofillManager = 4,
  kFormStructureNotFound = 5,
  kTriggerFieldNotFound = 6,
  kEmptyFillData = 7,
  kSuccess = 8,
  kError = 9,
  // The service was destroyed while a fill was pending.
  kCancelled = 10,
  kMaxValue = kCancelled
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/one_time_tokens/enums.xml:ActorOneTimeTokenFillingServiceFillOtpEvent)

// Outcomes of the `FillOtp()` operation, used to slice the
// `OneTimeTokens.Actor.OneTimeTokenFillingService.FillOtp.Duration` histogram.
// Note: This enum is only used for the patterned histogram slice and is not
// logged directly.
enum class ActorOtpFillOtpDurationOutcome {
  kError,
  kSuccess,
};

// LINT.IfChange(ActorOtpRetrieveOtpCallbackSuperseded)

// Events recorded during the ActorOneTimeTokenFillingService RetrieveOtp
// callback superseded operation.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ActorOtpRetrieveOtpCallbackSuperseded {
  kRetrieveOtpStarted = 0,
  kCallbackSuperseded = 1,
  kMaxValue = kCallbackSuperseded
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/one_time_tokens/enums.xml:ActorOtpRetrieveOtpCallbackSupersededEvent)

// LINT.IfChange(ActorOtpRetrieveOtpDurationOutcome)

// Outcomes for the RetrieveOtp operation used to slice the duration metric.
// Note: This enum is only used for the patterned histogram slice and is not
// logged directly.
enum class ActorOtpRetrieveOtpDurationOutcome {
  kCacheMatch,
  kReceivedMatch,
  kTimeout,
  kSenderDomainMismatch,
  kError,
  kCancelled,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/one_time_tokens/histograms.xml:ActorOtpRetrieveOtpDurationOutcome)

// Histogram names for ActorOneTimeTokenFillingService operations.
inline constexpr std::string_view
    kActorOneTimeTokenFillingServiceRetrieveOtpHistogram =
        "OneTimeTokens.Actor.OneTimeTokenFillingService.RetrieveOtp";
inline constexpr std::string_view
    kActorOneTimeTokenFillingServiceFillOtpHistogram =
        "OneTimeTokens.Actor.OneTimeTokenFillingService.FillOtp";
inline constexpr std::string_view
    kActorOtpRetrieveOtpCallbackSupersededHistogram =
        "OneTimeTokens.Actor.OneTimeTokenFillingService."
        "RetrieveOtpCallbackSuperseded";

// Records events during RetrieveOtp operation.
void RecordActorOneTimeTokenFillingServiceRetrieveOtp(
    ActorOneTimeTokenFillingServiceRetrieveOtp event);

// Records the outcome `event` of a `RetrieveOtp()` request to
// `kActorOneTimeTokenFillingServiceRetrieveOtpHistogram` and to its slice for
// the request's `predicted_otp_type`.
void RecordActorOneTimeTokenFillingServiceRetrieveOtpOutcome(
    ActorOneTimeTokenFillingServiceRetrieveOtp event,
    ActorOneTimeTokenFillingService::PredictedOtpType predicted_otp_type);

// Records the duration of a `RetrieveOtp()` request to
// `kActorOneTimeTokenFillingServiceRetrieveOtpHistogram.Duration.{Outcome}`.
void RecordActorOneTimeTokenFillingServiceRetrieveOtpDuration(
    base::TimeDelta duration,
    ActorOtpRetrieveOtpDurationOutcome outcome);

// Records events during FillOtp operation.
void RecordActorOneTimeTokenFillingServiceFillOtp(
    ActorOneTimeTokenFillingServiceFillOtp event);

// Records the time from triggering an OTP fill until `ActorFillingObserver`
// reported its result.
void RecordActorOneTimeTokenFillingServiceFillOtpDuration(
    ActorOtpFillOtpDurationOutcome outcome,
    base::TimeDelta duration);

// Records events measuring superseded callbacks during RetrieveOtp operation.
void RecordActorOtpRetrieveOtpCallbackSuperseded(
    ActorOtpRetrieveOtpCallbackSuperseded event);

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_ACTOR_ONE_TIME_TOKENS_ACTOR_ONE_TIME_TOKEN_FILLING_SERVICE_METRICS_H_
