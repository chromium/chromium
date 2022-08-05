// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "ash/webui/os_feedback_ui/backend/histogram_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"

namespace ash::os_feedback_ui::metrics {

void EmitFeedbackAppOpenDuration(const base::TimeDelta& time_elapsed) {
  base::UmaHistogramLongTimes100(kFeedbackAppOpenDuration, time_elapsed);
}

void EmitFeedbackAppPostSubmitAction(
    mojom::FeedbackAppPostSubmitAction action) {
  base::UmaHistogramEnumeration(kFeedbackAppPostSubmitAction, action);
}

void EmitFeedbackAppPreSubmitAction(mojom::FeedbackAppPreSubmitAction action) {
  // TODO(longbowei) Add preSubmit actions and use switch case statement.
  base::UmaHistogramBoolean(kFeedbackAppViewedScreenshot, true);
}

void EmitFeedbackAppIncludedScreenshot(bool included_screenshot) {
  base::UmaHistogramBoolean(kFeedbackAppIncludedScreenshot,
                            included_screenshot);
}

void EmitFeedbackAppCanContactUser(
    FeedbackAppContactUserConsentType contact_user_consent) {
  base::UmaHistogramEnumeration(kFeedbackAppCanContactUser,
                                contact_user_consent);
}

void EmitFeedbackAppIncludedFile(bool included_file) {
  base::UmaHistogramBoolean(kFeedbackAppIncludedFile, included_file);
}

void EmitFeedbackAppIncludedEmail(bool included_email) {
  base::UmaHistogramBoolean(kFeedbackAppIncludedEmail, included_email);
}

void EmitFeedbackAppIncludedUrl(bool included_url) {
  base::UmaHistogramBoolean(kFeedbackAppIncludedUrl, included_url);
}

void EmitFeedbackAppIncludedSystemInfo(bool included_system_info) {
  base::UmaHistogramBoolean(kFeedbackAppIncludedSystemInfo,
                            included_system_info);
}

}  // namespace ash::os_feedback_ui::metrics
