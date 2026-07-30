// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ACTOR_ONE_TIME_TOKENS_ACTOR_ONE_TIME_TOKEN_FILLING_SERVICE_METRICS_H_
#define CHROME_BROWSER_AUTOFILL_ACTOR_ONE_TIME_TOKENS_ACTOR_ONE_TIME_TOKEN_FILLING_SERVICE_METRICS_H_

#include <string_view>

namespace autofill {

// LINT.IfChange(ActorOneTimeTokenFillingServiceRetrieveOtp)

// Events recorded during the ActorOneTimeTokenFillingService RetrieveOtp
// operation.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ActorOneTimeTokenFillingServiceRetrieveOtp {
  kStart = 0,
  kNullTab = 1,
  kNoService = 2,
  kSuccessCacheMatchFound = 3,
  // kSuccess = 4,  // Obsolete. Replaced by kSuccessReceivedMatchFound.
  kError = 5,
  kMockOtp = 6,
  // kNoCallback = 7,  // Obsolete.
  kSuccessReceivedMatchFound = 8,
  kRetrievalTimeout = 9,
  kMaxValue = kRetrievalTimeout,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/one_time_tokens/enums.xml:ActorOneTimeTokenFillingServiceRetrieveOtpEvent)

// LINT.IfChange(ActorOneTimeTokenFillingServiceFillOtp)

// Events recorded during the ActorOneTimeTokenFillingService FillOtp operation.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ActorOneTimeTokenFillingServiceFillOtp {
  kStart = 0,
  kNullTab = 1,
  kEmptyTriggerFieldIds = 2,
  kConcurrentCall = 3,
  kNoAutofillManager = 4,
  kFormStructureNotFound = 5,
  kTriggerFieldNotFound = 6,
  kEmptyFillData = 7,
  kSuccess = 8,
  kError = 9,
  kMaxValue = kError
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/one_time_tokens/enums.xml:ActorOneTimeTokenFillingServiceFillOtpEvent)

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

// Records events during FillOtp operation.
void RecordActorOneTimeTokenFillingServiceFillOtp(
    ActorOneTimeTokenFillingServiceFillOtp event);

// Records events measuring superseded callbacks during RetrieveOtp operation.
void RecordActorOtpRetrieveOtpCallbackSuperseded(
    ActorOtpRetrieveOtpCallbackSuperseded event);

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_ACTOR_ONE_TIME_TOKENS_ACTOR_ONE_TIME_TOKEN_FILLING_SERVICE_METRICS_H_
