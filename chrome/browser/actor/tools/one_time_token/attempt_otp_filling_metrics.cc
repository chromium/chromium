// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/one_time_token/attempt_otp_filling_metrics.h"

#include <string_view>

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace actor {

namespace {

// LINT.IfChange(AttemptOtpFillingStepSuffix)

// Histogram suffix for `step`.
std::string_view GetUmaSuffix(AttemptOtpFillingStep step) {
  switch (step) {
    case AttemptOtpFillingStep::kTimeOfUse:
      return "TimeOfUse";
    case AttemptOtpFillingStep::kAfterOtpRetrieved:
      return "AfterOtpRetrieved";
    case AttemptOtpFillingStep::kAfterConfirmation:
      return "AfterConfirmation";
  }
  NOTREACHED();
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/one_time_tokens/histograms.xml:AttemptOtpFillingStep)

}  // namespace

void RecordAttemptOtpFillingEvent(AttemptOtpFillingToolEvent event) {
  base::UmaHistogramEnumeration(kAttemptOtpFillingToolHistogram, event);
}

void RecordGmailOtpOptInCardInteraction(
    GmailOtpOptInCardInteraction interaction) {
  base::UmaHistogramEnumeration(kGmailOtpOptInCardInteractionHistogram,
                                interaction);
}

void RecordGmailOtpConfirmationDialogInteraction(
    GmailOtpConfirmationDialogInteraction interaction) {
  base::UmaHistogramEnumeration(kGmailOtpConfirmationDialogInteractionHistogram,
                                interaction);
}

void RecordActorLoginFlowVerification(ActorLoginFlowVerifier::Result result) {
  base::UmaHistogramEnumeration(kActorOtpVerifyIsActorLoginFlowHistogram,
                                result);
}

void RecordGmailAndGoogleSmartFeaturesConsent(
    ActorOtpGmailAndGoogleSmartFeaturesConsent consent) {
  base::UmaHistogramEnumeration(kGmailAndGoogleSmartFeaturesConsentHistogram,
                                consent);
}

void RecordFormFillingContextValidationResult(
    AttemptOtpFillingStep step,
    autofill::FormFillingContextValidationResult result) {
  base::UmaHistogramEnumeration(
      base::StrCat({kAttemptOtpFillingValidateFormFillingContextHistogram, ".",
                    GetUmaSuffix(step)}),
      result);
}
void RecordPredictedOtpTypeMetrics(
    AttemptOtpFillingToolRequest::OtpType predicted_otp_type,
    ukm::SourceId ukm_source_id) {
  base::UmaHistogramEnumeration(
      "OneTimeTokens.Actor.AttemptOtpFilling.PredictedOtpType",
      predicted_otp_type);

  if (ukm_source_id != ukm::kInvalidSourceId) {
    ukm::builders::Actor_AttemptOtpFilling(ukm_source_id)
        .SetPredictedOtpType(static_cast<int64_t>(predicted_otp_type))
        .Record(ukm::UkmRecorder::Get());
  }
}
}  // namespace actor
