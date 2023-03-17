// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "ash/webui/os_feedback_ui/backend/histogram_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"

namespace ash::os_feedback_ui::metrics {

void EmitFeedbackAppOpenDuration(const base::TimeDelta& time_elapsed) {
  base::UmaHistogramLongTimes100(kFeedbackAppOpenDuration, time_elapsed);
}

void EmitFeedbackAppTimeOnSearchPage(const base::TimeDelta& time_elapsed) {
  base::UmaHistogramLongTimes100(kFeedbackAppTimeOnPageSearchPage,
                                 time_elapsed);
}

void EmitFeedbackAppTimeOnShareDataPage(const base::TimeDelta& time_elapsed) {
  base::UmaHistogramLongTimes100(kFeedbackAppTimeOnPageShareDataPage,
                                 time_elapsed);
}

void EmitFeedbackAppTimeOnConfirmationPage(
    const base::TimeDelta& time_elapsed) {
  base::UmaHistogramLongTimes100(kFeedbackAppTimeOnPageConfirmationPage,
                                 time_elapsed);
}

void EmitFeedbackAppPostSubmitAction(
    mojom::FeedbackAppPostSubmitAction action) {
  base::UmaHistogramEnumeration(kFeedbackAppPostSubmitAction, action);
}

void EmitFeedbackAppPreSubmitAction(mojom::FeedbackAppPreSubmitAction action) {
  switch (action) {
    case mojom::FeedbackAppPreSubmitAction::kViewedScreenshot:
      base::UmaHistogramBoolean(kFeedbackAppViewedScreenshot, true);
      break;
    case mojom::FeedbackAppPreSubmitAction::kViewedImage:
      base::UmaHistogramBoolean(kFeedbackAppViewedImage, true);
      break;
    case mojom::FeedbackAppPreSubmitAction::kViewedSystemAndAppInfo:
      base::UmaHistogramBoolean(kFeedbackAppViewedSystemAndAppInfo, true);
      break;
    case mojom::FeedbackAppPreSubmitAction::kViewedAutofillMetadata:
      base::UmaHistogramBoolean(kFeedbackAppViewedAutofillMetadata, true);
      break;
    case mojom::FeedbackAppPreSubmitAction::kViewedMetrics:
      base::UmaHistogramBoolean(kFeedbackAppViewedMetrics, true);
      break;
    case mojom::FeedbackAppPreSubmitAction::kViewedHelpContent:
      base::UmaHistogramBoolean(kFeedbackAppViewedHelpContent, true);
      break;
  }
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

void EmitFeedbackAppDescriptionLength(int length) {
  base::UmaHistogramCounts1000(kFeedbackAppDescriptionLength, length);
}

void EmitFeedbackAppExitPath(mojom::FeedbackAppExitPath exit_path) {
  base::UmaHistogramEnumeration(kFeedbackAppExitPath, exit_path);
}

void EmitFeedbackAppHelpContentOutcome(
    mojom::FeedbackAppHelpContentOutcome outcome) {
  base::UmaHistogramEnumeration(kFeedbackAppHelpContentOutcome, outcome);
}

void EmitFeedbackAppHelpContentSearchResultCount(int count) {
  base::UmaHistogramCounts100(kFeedbackAppHelpContentSearchResultCount, count);
}

}  // namespace ash::os_feedback_ui::metrics
