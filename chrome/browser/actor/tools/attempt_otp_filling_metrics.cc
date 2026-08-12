// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/attempt_otp_filling_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace actor {

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

void RecordActorLoginFlowVerification(VerifyIsActorLoginFlowEvent event) {
  base::UmaHistogramEnumeration(kActorOtpVerifyIsActorLoginFlowHistogram,
                                event);
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
