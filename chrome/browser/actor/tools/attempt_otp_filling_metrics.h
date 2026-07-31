// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_ATTEMPT_OTP_FILLING_METRICS_H_
#define CHROME_BROWSER_ACTOR_TOOLS_ATTEMPT_OTP_FILLING_METRICS_H_

#include <string_view>

#include "chrome/browser/actor/tools/attempt_otp_filling_tool_request.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace actor {

// LINT.IfChange(VerifyIsActorLoginFlowEvent)

// Events recorded during ActorLoginFlowVerifier::VerifyIsActorLoginFlow.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class VerifyIsActorLoginFlowEvent {
  kStart = 0,
  kNoActorLoginContext = 1,
  kFrameNotInLoginContext = 2,
  kAllFramesHaveTooManyNavigations = 3,
  kNoMatch = 4,
  kPslMatchAllowed = 5,
  kPslMatchDisallowed = 6,
  kGroupedOrOtherMismatch = 7,
  kExactMatchAllowed = 8,
  kAffiliatedMatchAllowed = 9,
  kMaxValue = kAffiliatedMatchAllowed
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/one_time_tokens/enums.xml:VerifyIsActorLoginFlowEvent)

// LINT.IfChange(AttemptOtpFillingEvent)

// Events recorded during the AttemptOtpFilling tool execution.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AttemptOtpFillingToolEvent {
  kStartFillingAttempt = 0,
  kWithinOptInCoolOffPeriod = 1,
  kOptInNullResponse = 2,
  kOptInErrorResponse = 3,
  kOptInPermissionDenied = 4,
  kTabWentAwayBeforeInvocation = 5,
  kNoLastTabObservation = 6,
  kTriggerFieldNotFound = 7,
  kFormFillingStatusInsecureContext = 8,
  kFormFillingStatusFormNotFound = 9,
  kFormFillingStatusTabNotAvailable = 10,
  kNoTargetFrameWithOtpFound = 11,
  // kNoActorLogin = 12,  Obsolete: this is no longer an exit condition for the
  // flow.
  kOtpRetrievalError = 13,
  kFormFillingNotSecureBeforeFilling = 14,
  kFillingOtpSuccess = 15,
  kFillingOtpError = 16,
  kGmailOtpConfirmationResponseNotValid = 17,
  kGmailOtpConfirmationDeclinedByUser = 18,
  kMaxValue = kGmailOtpConfirmationDeclinedByUser
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/one_time_tokens/enums.xml:AttemptOtpFillingEvent)

// LINT.IfChange(GmailOtpOptInCardInteraction)

// Outcomes of the Gmail OTP opt-in card interaction.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class GmailOtpOptInCardInteraction {
  kShowCard = 0,
  kErrorResponse = 1,
  kPermissionDenied = 2,
  kPermissionGranted = 3,
  kMaxValue = kPermissionGranted
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/one_time_tokens/enums.xml:GmailOtpOptInCardInteraction)

// Histogram name for AttemptOtpFilling tool invocation events.
inline constexpr char kAttemptOtpFillingToolHistogram[] =
    "OneTimeTokens.Actor.AttemptOtpFilling.ToolInvocation";

// Histogram name for Gmail OTP opt-in card interaction events.
inline constexpr char kGmailOtpOptInCardInteractionHistogram[] =
    "OneTimeTokens.Actor.AttemptOtpFilling.GmailOtpOptInCardInteraction";

// Histogram name for VerifyIsActorLoginFlow events.
inline constexpr std::string_view kActorOtpVerifyIsActorLoginFlowHistogram =
    "OneTimeTokens.Actor.AttemptOtpFilling.VerifyIsActorLoginFlow";

// Records events during the AttemptOtpFilling tool invocation events.
void RecordAttemptOtpFillingEvent(AttemptOtpFillingToolEvent event);

// Records user interactions with the Gmail OTP opt-in card.
void RecordGmailOtpOptInCardInteraction(
    GmailOtpOptInCardInteraction interaction);

// Records events during VerifyIsActorLoginFlow.
void RecordActorLoginFlowVerification(VerifyIsActorLoginFlowEvent event);

void RecordPredictedOtpTypeMetrics(
    AttemptOtpFillingToolRequest::OtpType predicted_otp_type,
    ukm::SourceId ukm_source_id);
}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_ATTEMPT_OTP_FILLING_METRICS_H_
