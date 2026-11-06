// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/actor/one_time_tokens/actor_one_time_token_filling_service_metrics.h"

#include <string_view>

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "chrome/browser/autofill/actor/one_time_tokens/actor_one_time_token_filling_service.h"

namespace autofill {

namespace {

// LINT.IfChange(AttemptOtpFillingPredictedOtpTypeSuffix)

// Histogram suffix for `predicted_otp_type`.
std::string_view GetUmaSuffix(
    ActorOneTimeTokenFillingService::PredictedOtpType predicted_otp_type) {
  switch (predicted_otp_type) {
    case ActorOneTimeTokenFillingService::PredictedOtpType::kUnknown:
      return "Unknown";
    case ActorOneTimeTokenFillingService::PredictedOtpType::kSms:
      return "Sms";
    case ActorOneTimeTokenFillingService::PredictedOtpType::kEmail:
      return "Email";
    case ActorOneTimeTokenFillingService::PredictedOtpType::kAuthenticatorApp:
      return "AuthenticatorApp";
  }
  NOTREACHED();
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/one_time_tokens/histograms.xml:AttemptOtpFillingPredictedOtpType)

// LINT.IfChange(ActorOtpFillOtpDurationOutcome)

// Histogram suffix for `ActorOtpFillOtpDurationOutcome`.
std::string_view GetUmaSuffix(ActorOtpFillOtpDurationOutcome outcome) {
  switch (outcome) {
    case ActorOtpFillOtpDurationOutcome::kError:
      return "Error";
    case ActorOtpFillOtpDurationOutcome::kSuccess:
      return "Success";
  }
  NOTREACHED();
}

// LINT.ThenChange(//tools/metrics/histograms/metadata/one_time_tokens/histograms.xml:ActorOtpFillOtpDurationOutcome)

// LINT.IfChange(ActorOtpRetrieveOtpDurationOutcomeSuffix)
std::string_view GetUmaSuffix(ActorOtpRetrieveOtpDurationOutcome outcome) {
  switch (outcome) {
    case ActorOtpRetrieveOtpDurationOutcome::kCacheMatch:
      return "CacheMatch";
    case ActorOtpRetrieveOtpDurationOutcome::kReceivedMatch:
      return "ReceivedMatch";
    case ActorOtpRetrieveOtpDurationOutcome::kTimeout:
      return "Timeout";
    case ActorOtpRetrieveOtpDurationOutcome::kSenderDomainMismatch:
      return "SenderDomainMismatch";
    case ActorOtpRetrieveOtpDurationOutcome::kError:
      return "Error";
    case ActorOtpRetrieveOtpDurationOutcome::kCancelled:
      return "Cancelled";
  }
  NOTREACHED();
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/one_time_tokens/histograms.xml:ActorOtpRetrieveOtpDurationOutcome)
}  // namespace

void RecordActorOneTimeTokenFillingServiceRetrieveOtp(
    ActorOneTimeTokenFillingServiceRetrieveOtp event) {
  base::UmaHistogramEnumeration(
      kActorOneTimeTokenFillingServiceRetrieveOtpHistogram, event);
}

void RecordActorOneTimeTokenFillingServiceRetrieveOtpOutcome(
    ActorOneTimeTokenFillingServiceRetrieveOtp event,
    ActorOneTimeTokenFillingService::PredictedOtpType predicted_otp_type) {
  RecordActorOneTimeTokenFillingServiceRetrieveOtp(event);
  base::UmaHistogramEnumeration(
      base::StrCat({kActorOneTimeTokenFillingServiceRetrieveOtpHistogram, ".",
                    GetUmaSuffix(predicted_otp_type)}),
      event);
}

void RecordActorOneTimeTokenFillingServiceRetrieveOtpDuration(
    base::TimeDelta duration,
    ActorOtpRetrieveOtpDurationOutcome outcome) {
  base::UmaHistogramMediumTimes(
      base::StrCat({kActorOneTimeTokenFillingServiceRetrieveOtpHistogram,
                    ".Duration.", GetUmaSuffix(outcome)}),
      duration);
}

void RecordActorOneTimeTokenFillingServiceFillOtp(
    ActorOneTimeTokenFillingServiceFillOtp event) {
  base::UmaHistogramEnumeration(
      kActorOneTimeTokenFillingServiceFillOtpHistogram, event);
}

void RecordActorOneTimeTokenFillingServiceFillOtpDuration(
    ActorOtpFillOtpDurationOutcome outcome,
    base::TimeDelta duration) {
  base::UmaHistogramTimes(
      base::StrCat({kActorOneTimeTokenFillingServiceFillOtpHistogram,
                    ".Duration.", GetUmaSuffix(outcome)}),
      duration);
}

void RecordActorOtpRetrieveOtpCallbackSuperseded(
    ActorOtpRetrieveOtpCallbackSuperseded event) {
  base::UmaHistogramEnumeration(kActorOtpRetrieveOtpCallbackSupersededHistogram,
                                event);
}

}  // namespace autofill
