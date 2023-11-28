// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/os_feedback_ui/backend/feedback_service_provider.h"

#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/webui/os_feedback_ui/backend/histogram_util.h"
#include "ash/webui/os_feedback_ui/backend/os_feedback_delegate.h"
#include "ash/webui/os_feedback_ui/mojom/os_feedback_ui.mojom.h"
#include "base/functional/bind.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "url/gurl.h"

namespace ash {
namespace feedback {

using ::ash::os_feedback_ui::mojom::FeedbackContext;
using ::ash::os_feedback_ui::mojom::FeedbackContextPtr;
using ::ash::os_feedback_ui::mojom::ReportPtr;
using ::ash::os_feedback_ui::mojom::SendReportStatus;

void EmitTimeOnEachPageMetrics(
    bool feedback_sent,
    const base::Time app_open_timestamp_,
    const base::Time share_data_page_open_timestamp_,
    const base::Time share_data_page_close_timestamp_) {
  if (feedback_sent) {
    const base::TimeDelta search_page_open_duration =
        share_data_page_open_timestamp_ - app_open_timestamp_;
    const base::TimeDelta share_data_page_open_duration =
        share_data_page_close_timestamp_ - share_data_page_open_timestamp_;
    const base::TimeDelta confirmation_page_open_duration =
        base::Time::Now() - share_data_page_close_timestamp_;

    os_feedback_ui::metrics::EmitFeedbackAppTimeOnSearchPage(
        search_page_open_duration);
    os_feedback_ui::metrics::EmitFeedbackAppTimeOnShareDataPage(
        share_data_page_open_duration);
    os_feedback_ui::metrics::EmitFeedbackAppTimeOnConfirmationPage(
        confirmation_page_open_duration);
  }
}

bool IsInternalAccount(const std::optional<std::string>& email) {
  return email.has_value() && gaia::IsGoogleInternalAccountEmail(email.value());
}

FeedbackServiceProvider::FeedbackServiceProvider(
    std::unique_ptr<OsFeedbackDelegate> feedback_delegate)
    : feedback_delegate_(std::move(feedback_delegate)) {
  app_open_timestamp_ = base::Time::Now();
  feedback_sent = false;
}

FeedbackServiceProvider::~FeedbackServiceProvider() {
  const base::TimeDelta app_open_duration =
      base::Time::Now() - app_open_timestamp_;
  os_feedback_ui::metrics::EmitFeedbackAppOpenDuration(app_open_duration);

  EmitTimeOnEachPageMetrics(feedback_sent, app_open_timestamp_,
                            share_data_page_open_timestamp_,
                            share_data_page_close_timestamp_);
}

void FeedbackServiceProvider::GetFeedbackContext(
    GetFeedbackContextCallback callback) {
  FeedbackContextPtr feedback_context = FeedbackContext::New();
  feedback_context->page_url = feedback_delegate_->GetLastActivePageUrl();
  feedback_context->email = feedback_delegate_->GetSignedInUserEmail();
  feedback_context->wifi_debug_logs_allowed =
      feedback_delegate_->IsWifiDebugLogsAllowed();
  feedback_context->trace_id = feedback_delegate_->GetPerformanceTraceId();
  if (features::IsLinkCrossDeviceDogfoodFeedbackEnabled()) {
    feedback_context->has_linked_cross_device_phone =
        feedback_delegate_->GetLinkedPhoneMacAddress().has_value();
  }

  feedback_context->is_internal_account =
      IsInternalAccount(feedback_context->email);
  std::move(callback).Run(std::move(feedback_context));
}

void FeedbackServiceProvider::GetScreenshotPng(
    GetScreenshotPngCallback callback) {
  feedback_delegate_->GetScreenshotPng(std::move(callback));
}

void FeedbackServiceProvider::SendReport(ReportPtr report,
                                         SendReportCallback callback) {
  report->feedback_context->is_internal_account =
      IsInternalAccount(feedback_delegate_->GetSignedInUserEmail());
  feedback_delegate_->SendReport(std::move(report), std::move(callback));
  share_data_page_close_timestamp_ = base::Time::Now();
  feedback_sent = true;
}

void FeedbackServiceProvider::OpenDiagnosticsApp() {
  feedback_delegate_->OpenDiagnosticsApp();
}

void FeedbackServiceProvider::OpenExploreApp() {
  feedback_delegate_->OpenExploreApp();
}

void FeedbackServiceProvider::OpenMetricsDialog() {
  feedback_delegate_->OpenMetricsDialog();
}

void FeedbackServiceProvider::OpenSystemInfoDialog() {
  feedback_delegate_->OpenSystemInfoDialog();
}

void FeedbackServiceProvider::OpenAutofillDialog(
    const std::string& autofill_metadata) {
  feedback_delegate_->OpenAutofillMetadataDialog(autofill_metadata);
}

void FeedbackServiceProvider::RecordPostSubmitAction(
    os_feedback_ui::mojom::FeedbackAppPostSubmitAction action) {
  if (action ==
      os_feedback_ui::mojom::FeedbackAppPostSubmitAction::kSendNewReport) {
    EmitTimeOnEachPageMetrics(feedback_sent, app_open_timestamp_,
                              share_data_page_open_timestamp_,
                              share_data_page_close_timestamp_);
    feedback_sent = false;
    app_open_timestamp_ = base::Time::Now();
  }
  os_feedback_ui::metrics::EmitFeedbackAppPostSubmitAction(action);
}

void FeedbackServiceProvider::RecordPreSubmitAction(
    os_feedback_ui::mojom::FeedbackAppPreSubmitAction action) {
  os_feedback_ui::metrics::EmitFeedbackAppPreSubmitAction(action);
}

void FeedbackServiceProvider::RecordExitPath(
    os_feedback_ui::mojom::FeedbackAppExitPath exit_path) {
  os_feedback_ui::metrics::EmitFeedbackAppExitPath(exit_path);
}

void FeedbackServiceProvider::RecordHelpContentOutcome(
    os_feedback_ui::mojom::FeedbackAppHelpContentOutcome outcome) {
  if (outcome == os_feedback_ui::mojom::FeedbackAppHelpContentOutcome::
                     kContinueHelpContentClicked ||
      outcome == os_feedback_ui::mojom::FeedbackAppHelpContentOutcome::
                     kContinueNoHelpContentClicked) {
    share_data_page_open_timestamp_ = base::Time::Now();
  }
  os_feedback_ui::metrics::EmitFeedbackAppHelpContentOutcome(outcome);
}

void FeedbackServiceProvider::RecordHelpContentSearchResultCount(int count) {
  os_feedback_ui::metrics::EmitFeedbackAppHelpContentSearchResultCount(count);
}

void FeedbackServiceProvider::BindInterface(
    mojo::PendingReceiver<os_feedback_ui::mojom::FeedbackServiceProvider>
        receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

}  // namespace feedback
}  // namespace ash
