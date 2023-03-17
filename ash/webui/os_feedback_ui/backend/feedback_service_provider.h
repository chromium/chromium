// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_OS_FEEDBACK_UI_BACKEND_FEEDBACK_SERVICE_PROVIDER_H_
#define ASH_WEBUI_OS_FEEDBACK_UI_BACKEND_FEEDBACK_SERVICE_PROVIDER_H_

#include <memory>

#include "ash/webui/os_feedback_ui/mojom/os_feedback_ui.mojom.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash {

class OsFeedbackDelegate;

namespace feedback {

class FeedbackServiceProvider
    : public os_feedback_ui::mojom::FeedbackServiceProvider {
 public:
  explicit FeedbackServiceProvider(
      std::unique_ptr<OsFeedbackDelegate> feedback_delegate);
  FeedbackServiceProvider(const FeedbackServiceProvider&) = delete;
  FeedbackServiceProvider& operator=(const FeedbackServiceProvider&) = delete;
  ~FeedbackServiceProvider() override;

  // os_feedback_ui::mojom::FeedbackServiceProvider:
  void GetFeedbackContext(GetFeedbackContextCallback callback) override;
  void GetScreenshotPng(GetScreenshotPngCallback callback) override;
  void SendReport(os_feedback_ui::mojom::ReportPtr report,
                  SendReportCallback callback) override;
  void OpenDiagnosticsApp() override;
  void OpenExploreApp() override;
  void OpenMetricsDialog() override;
  void OpenSystemInfoDialog() override;
  void OpenAutofillDialog(const std::string& autofill_metadata) override;
  void RecordPostSubmitAction(
      os_feedback_ui::mojom::FeedbackAppPostSubmitAction action) override;
  void RecordPreSubmitAction(
      os_feedback_ui::mojom::FeedbackAppPreSubmitAction action) override;
  void RecordExitPath(
      os_feedback_ui::mojom::FeedbackAppExitPath exit_path) override;
  void RecordHelpContentOutcome(
      os_feedback_ui::mojom::FeedbackAppHelpContentOutcome outcome) override;
  void RecordHelpContentSearchResultCount(int count) override;

  void BindInterface(
      mojo::PendingReceiver<os_feedback_ui::mojom::FeedbackServiceProvider>
          receiver);

 private:
  std::unique_ptr<OsFeedbackDelegate> feedback_delegate_;
  mojo::Receiver<os_feedback_ui::mojom::FeedbackServiceProvider> receiver_{
      this};
  // Timestamp of when the app was opened. Used to calculate a duration for
  // metrics.
  base::Time app_open_timestamp_;
  base::Time share_data_page_open_timestamp_;
  base::Time share_data_page_close_timestamp_;
  bool feedback_sent;
  base::WeakPtrFactory<FeedbackServiceProvider> weak_ptr_factory_{this};
};

}  // namespace feedback
}  // namespace ash

#endif  // ASH_WEBUI_OS_FEEDBACK_UI_BACKEND_FEEDBACK_SERVICE_PROVIDER_H_
