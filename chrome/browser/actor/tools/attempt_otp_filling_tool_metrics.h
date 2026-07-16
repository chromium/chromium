// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_ATTEMPT_OTP_FILLING_TOOL_METRICS_H_
#define CHROME_BROWSER_ACTOR_TOOLS_ATTEMPT_OTP_FILLING_TOOL_METRICS_H_

#include "chrome/browser/actor/tools/attempt_otp_filling_tool_request.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace actor {

// LINT.IfChange(AttemptOtpFillingEvent)

// Events recorded during the AttemptOtpFilling tool execution.
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
  kNoActorLogin = 12,
  kOtpRetrievalError = 13,
  kFormFillingNotSecureBeforeFilling = 14,
  kFillingOtpSuccess = 15,
  kFillingOtpError = 16,
  kMaxValue = kFillingOtpError
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/one_time_tokens/enums.xml:AttemptOtpFillingEvent)

// Histogram name for AttemptOtpFilling tool invocation events.
inline constexpr char kAttemptOtpFillingToolHistogram[] =
    "OneTimeTokens.Actor.AttemptOtpFilling.ToolInvocation";

// Records events during the AttemptOtpFilling tool invocation events.
void RecordAttemptOtpFillingEvent(AttemptOtpFillingToolEvent event);

void RecordPredictedOtpTypeMetrics(
    AttemptOtpFillingToolRequest::OtpType predicted_otp_type,
    ukm::SourceId ukm_source_id);

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_ATTEMPT_OTP_FILLING_TOOL_METRICS_H_
