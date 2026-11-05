// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_ONE_TIME_TOKEN_ATTEMPT_OTP_FILLING_METRICS_H_
#define CHROME_BROWSER_ACTOR_TOOLS_ONE_TIME_TOKEN_ATTEMPT_OTP_FILLING_METRICS_H_

#include <string_view>

#include "chrome/browser/actor/tools/one_time_token/actor_login_flow_verifier.h"
#include "chrome/browser/actor/tools/one_time_token/attempt_otp_filling_tool_request.h"
#include "chrome/browser/autofill/actor/one_time_tokens/actor_one_time_token_filling_service.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace actor {

// LINT.IfChange(AttemptOtpFillingEvent)

// Events recorded during the AttemptOtpFilling tool execution.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AttemptOtpFillingToolEvent {
  // kStartFillingAttempt = 0,  // Obsolete.
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
  kGmailSmartFeaturesConsentRequired = 19,
  kGoogleSmartFeaturesConsentRequired = 20,
  kUnableToRetrieveGmailAndGoogleSmartFeaturesConsent = 21,
  // Recorded when the tool is destroyed (e.g. the task is stopped, paused or
  // cancelled, or the tab is closed) before the callback of the named
  // asynchronous step runs.
  kCancelledDuringConsentFetch = 22,
  kCancelledDuringOptInCard = 23,
  kCancelledDuringLoginFlowVerification = 24,
  kCancelledDuringOtpRetrieval = 25,
  kCancelledDuringConfirmationDialog = 26,
  kCancelledDuringFillOtp = 27,
  kFormNotFoundBeforeFilling = 28,
  kTabNotAvailableBeforeFilling = 29,
  // OTP retrieval timed out after a received token was rejected for its sender
  // domain.
  kGmailOtpSenderDomainMismatch = 30,
  // OTP retrieval timed out without a sender-domain rejection.
  kOtpRetrievalTimeout = 31,
  kMaxValue = kOtpRetrievalTimeout
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/one_time_tokens/enums.xml:AttemptOtpFillingEvent)

// LINT.IfChange(GmailOtpOptInCardInteraction)

// Outcomes of the Gmail OTP opt-in card interaction.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class GmailOtpOptInCardInteraction {
  // kShowCard = 0,  // Obsolete.
  kErrorResponse = 1,
  kPermissionDenied = 2,
  kPermissionGranted = 3,
  // The tool was destroyed after the opt-in card was requested and before a
  // response was received.
  kCancelled = 4,
  kMaxValue = kCancelled
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/one_time_tokens/enums.xml:GmailOtpOptInCardInteraction)

// LINT.IfChange(GmailOtpConfirmationDialogInteraction)

// Outcomes of the Gmail OTP confirmation dialog interaction.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class GmailOtpConfirmationDialogInteraction {
  // kShowDialog = 0,  // Obsolete.
  kErrorResponse = 1,
  kPermissionDenied = 2,
  kPermissionGranted = 3,
  // The tool was destroyed after the confirmation dialog was requested and
  // before a response was received.
  kCancelled = 4,
  kMaxValue = kCancelled
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/one_time_tokens/enums.xml:GmailOtpConfirmationDialogInteraction)

// LINT.IfChange(ActorOtpGmailAndGoogleSmartFeaturesConsent)

// Combination of the Gmail (`comms_apps`) and Google (`google_apps`) Smart
// Features consent states, one value per combination.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ActorOtpGmailAndGoogleSmartFeaturesConsent {
  kGmailEnabledGoogleEnabled = 0,
  kGmailEnabledGoogleDisabled = 1,
  kGmailEnabledGoogleUnknown = 2,
  kGmailEnabledGoogleUndefined = 3,
  kGmailDisabledGoogleEnabled = 4,
  kGmailDisabledGoogleDisabled = 5,
  kGmailDisabledGoogleUnknown = 6,
  kGmailDisabledGoogleUndefined = 7,
  kGmailUnknownGoogleEnabled = 8,
  kGmailUnknownGoogleDisabled = 9,
  kGmailUnknownGoogleUnknown = 10,
  kGmailUnknownGoogleUndefined = 11,
  kGmailUndefinedGoogleEnabled = 12,
  kGmailUndefinedGoogleDisabled = 13,
  kGmailUndefinedGoogleUnknown = 14,
  kGmailUndefinedGoogleUndefined = 15,
  kMaxValue = kGmailUndefinedGoogleUndefined
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/one_time_tokens/enums.xml:ActorOtpGmailAndGoogleSmartFeaturesConsent)

// The step of the AttemptOtpFilling tool that validates the form filling
// context. This enum is only used to select the
// ValidateFormFillingContext.{Step} patterned histogram suffix and is never
// logged as a bucket value.
enum class AttemptOtpFillingStep {
  // In `TimeOfUseValidation()`, right before the tool is invoked.
  kTimeOfUse,
  // After the OTP is retrieved, before it is filled (and before the
  // confirmation dialog is shown, if applicable).
  kAfterOtpRetrieved,
  // After the user approves the confirmation dialog, before the OTP is filled.
  kAfterConfirmation,
};

// Histogram name for AttemptOtpFilling tool invocation events.
inline constexpr char kAttemptOtpFillingToolHistogram[] =
    "OneTimeTokens.Actor.AttemptOtpFilling.ToolInvocation";

// Histogram name for Gmail OTP opt-in card interaction events.
inline constexpr char kGmailOtpOptInCardInteractionHistogram[] =
    "OneTimeTokens.Actor.AttemptOtpFilling.GmailOtpOptInCardInteraction";

// Histogram name for Gmail OTP confirmation dialog interaction events.
inline constexpr char kGmailOtpConfirmationDialogInteractionHistogram[] =
    "OneTimeTokens.Actor.AttemptOtpFilling."
    "GmailOtpConfirmationDialogInteraction";

// Histogram name for VerifyIsActorLoginFlow events.
inline constexpr std::string_view kActorOtpVerifyIsActorLoginFlowHistogram =
    "OneTimeTokens.Actor.AttemptOtpFilling.VerifyIsActorLoginFlow";

// Histogram name for the combination of the Gmail and Google Smart Features
// consent states.
inline constexpr char kGmailAndGoogleSmartFeaturesConsentHistogram[] =
    "OneTimeTokens.Actor.AttemptOtpFilling.GmailAndGoogleSmartFeaturesConsent";

// Prefix of the histograms recorded per `AttemptOtpFillingStep`.
inline constexpr char kAttemptOtpFillingValidateFormFillingContextHistogram[] =
    "OneTimeTokens.Actor.AttemptOtpFilling.ValidateFormFillingContext";

// Records events during the AttemptOtpFilling tool invocation events.
void RecordAttemptOtpFillingEvent(AttemptOtpFillingToolEvent event);

// Records user interactions with the Gmail OTP opt-in card.
void RecordGmailOtpOptInCardInteraction(
    GmailOtpOptInCardInteraction interaction);

// Records user interactions with the Gmail OTP confirmation dialog.
void RecordGmailOtpConfirmationDialogInteraction(
    GmailOtpConfirmationDialogInteraction interaction);

// Records the result of VerifyIsActorLoginFlow.
void RecordActorLoginFlowVerification(ActorLoginFlowVerifier::Result result);

// Records the combination of the Gmail and Google Smart Features consent
// states.
void RecordGmailAndGoogleSmartFeaturesConsent(
    ActorOtpGmailAndGoogleSmartFeaturesConsent consent);

// Records the detailed `result` of the form filling context validation at
// `step`.
void RecordFormFillingContextValidationResult(
    AttemptOtpFillingStep step,
    autofill::FormFillingContextValidationResult result);

void RecordPredictedOtpTypeMetrics(
    AttemptOtpFillingToolRequest::OtpType predicted_otp_type,
    ukm::SourceId ukm_source_id);
}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_ONE_TIME_TOKEN_ATTEMPT_OTP_FILLING_METRICS_H_
