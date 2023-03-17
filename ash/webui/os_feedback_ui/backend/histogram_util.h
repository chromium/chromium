// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_OS_FEEDBACK_UI_BACKEND_HISTOGRAM_UTIL_H_
#define ASH_WEBUI_OS_FEEDBACK_UI_BACKEND_HISTOGRAM_UTIL_H_

#include "ash/webui/os_feedback_ui/mojom/os_feedback_ui.mojom.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace ash::os_feedback_ui::metrics {

constexpr char kFeedbackAppOpenDuration[] = "Feedback.ChromeOSApp.OpenDuration";
constexpr char kFeedbackAppPostSubmitAction[] =
    "Feedback.ChromeOSApp.PostSubmitAction";
constexpr char kFeedbackAppIncludedScreenshot[] =
    "Feedback.ChromeOSApp.IncludedScreenshot";
constexpr char kFeedbackAppViewedScreenshot[] =
    "Feedback.ChromeOSApp.ViewedScreenshot";
constexpr char kFeedbackAppViewedImage[] = "Feedback.ChromeOSApp.ViewedImage";
constexpr char kFeedbackAppViewedMetrics[] =
    "Feedback.ChromeOSApp.ViewedMetrics";
constexpr char kFeedbackAppViewedSystemAndAppInfo[] =
    "Feedback.ChromeOSApp.ViewedSystemAndAppInfo";
constexpr char kFeedbackAppViewedAutofillMetadata[] =
    "Feedback.ChromeOSApp.ViewedAutofillMetadata";
constexpr char kFeedbackAppViewedHelpContent[] =
    "Feedback.ChromeOSApp.ViewedHelpContent";
constexpr char kFeedbackAppCanContactUser[] =
    "Feedback.ChromeOSApp.CanContactUser";
constexpr char kFeedbackAppIncludedFile[] = "Feedback.ChromeOSApp.IncludedFile";
constexpr char kFeedbackAppIncludedEmail[] =
    "Feedback.ChromeOSApp.IncludedEmail";
constexpr char kFeedbackAppIncludedUrl[] = "Feedback.ChromeOSApp.IncludedUrl";
constexpr char kFeedbackAppIncludedSystemInfo[] =
    "Feedback.ChromeOSApp.IncludedSystemInfo";
constexpr char kFeedbackAppDescriptionLength[] =
    "Feedback.ChromeOSApp.DescriptionLength";
constexpr char kFeedbackAppExitPath[] = "Feedback.ChromeOSApp.ExitPath";
constexpr char kFeedbackAppHelpContentOutcome[] =
    "Feedback.ChromeOSApp.HelpContentOutcome";
constexpr char kFeedbackAppTimeOnPageSearchPage[] =
    "Feedback.ChromeOSApp.TimeOnPage.SearchPage";
constexpr char kFeedbackAppTimeOnPageShareDataPage[] =
    "Feedback.ChromeOSApp.TimeOnPage.ShareDataPage";
constexpr char kFeedbackAppTimeOnPageConfirmationPage[] =
    "Feedback.ChromeOSApp.TimeOnPage.ConfirmationPage";
constexpr char kFeedbackAppHelpContentSearchResultCount[] =
    "Feedback.ChromeOSApp.HelpContentSearchResultCount";

// The enums below are used in histograms, do not remove/renumber entries. If
// you're adding to any of these enums, update the corresponding enum listing in
// tools/metrics/histograms/enums.xml: FeedbackAppContactUserConsentType
enum class FeedbackAppContactUserConsentType {
  kNoEmail = 0,
  kYes = 1,
  kNo = 2,
  kMaxValue = kNo,
};

void EmitFeedbackAppOpenDuration(const base::TimeDelta& time_elapsed);

void EmitFeedbackAppPostSubmitAction(mojom::FeedbackAppPostSubmitAction action);

void EmitFeedbackAppPreSubmitAction(mojom::FeedbackAppPreSubmitAction action);

void EmitFeedbackAppIncludedScreenshot(bool included_screenshot);

void EmitFeedbackAppCanContactUser(
    FeedbackAppContactUserConsentType contact_user_consent);

void EmitFeedbackAppIncludedFile(bool included_file);

void EmitFeedbackAppIncludedEmail(bool included_email);

void EmitFeedbackAppIncludedUrl(bool included_url);

void EmitFeedbackAppIncludedSystemInfo(bool included_system_info);

void EmitFeedbackAppDescriptionLength(int length);

void EmitFeedbackAppExitPath(mojom::FeedbackAppExitPath exit_path);

void EmitFeedbackAppHelpContentOutcome(
    mojom::FeedbackAppHelpContentOutcome outcome);

void EmitFeedbackAppTimeOnSearchPage(const base::TimeDelta& time_elapsed);

void EmitFeedbackAppTimeOnShareDataPage(const base::TimeDelta& time_elapsed);

void EmitFeedbackAppTimeOnConfirmationPage(const base::TimeDelta& time_elapsed);

void EmitFeedbackAppHelpContentSearchResultCount(int count);

}  // namespace ash::os_feedback_ui::metrics

#endif  // ASH_WEBUI_OS_FEEDBACK_UI_BACKEND_HISTOGRAM_UTIL_H_
