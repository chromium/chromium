// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/scanner_feedback_ui/scanner_feedback_page_handler.h"

#include <optional>
#include <string>
#include <utility>

#include "ash/public/cpp/scanner/scanner_feedback_info.h"
#include "ash/webui/scanner_feedback_ui/mojom/scanner_feedback_ui.mojom.h"
#include "ash/webui/scanner_feedback_ui/scanner_feedback_browser_context_data.h"
#include "ash/webui/scanner_feedback_ui/url_constants.h"
#include "base/strings/strcat.h"
#include "base/unguessable_token.h"
#include "content/public/browser/browser_context.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace ash {

ScannerFeedbackPageHandler::ScannerFeedbackPageHandler(
    content::BrowserContext& browser_context)
    : id_(base::UnguessableToken::Create()),
      browser_context_(browser_context) {}

ScannerFeedbackPageHandler::~ScannerFeedbackPageHandler() = default;

void ScannerFeedbackPageHandler::Bind(
    mojo::PendingReceiver<mojom::scanner_feedback_ui::PageHandler> receiver) {
  if (receiver_.is_bound()) {
    receiver_.reset();
  }
  receiver_.Bind(std::move(receiver));
}

void ScannerFeedbackPageHandler::GetFeedbackInfo(
    GetFeedbackInfoCallback callback) {
  auto feedback_info_ptr = mojom::scanner_feedback_ui::FeedbackInfo::New();
  ScannerFeedbackInfo* feedback_info =
      GetScannerFeedbackInfoForBrowserContext(*browser_context_, id_);
  if (feedback_info == nullptr) {
    mojo::ReportBadMessage("No feedback info was set.");
    // The callback still needs to be called with a valid `FeedbackInfoPtr`
    // before the renderer can be killed.
    std::move(callback).Run(std::move(feedback_info_ptr));
    return;
  }

  feedback_info_ptr->action_details = feedback_info->action_details;
  if (feedback_info->screenshot != nullptr) {
    feedback_info_ptr->screenshot_url = GURL(base::StrCat({
        kScannerFeedbackUntrustedUrl,
        kScannerFeedbackScreenshotPrefix,
        id_.ToString(),
        kScannerFeedbackScreenshotSuffix,
    }));
  } else {
    feedback_info_ptr->screenshot_url = std::nullopt;
  }

  std::move(callback).Run(std::move(feedback_info_ptr));
}

void ScannerFeedbackPageHandler::CloseDialog() {
  if (close_dialog_callback_.is_null()) {
    mojo::ReportBadMessage(
        "No close dialog callback was attached to the page handler.");
    return;
  }
  close_dialog_callback_.Run();
}

void ScannerFeedbackPageHandler::SendFeedback(
    const std::string& user_description) {
  if (send_feedback_callback_.is_null()) {
    mojo::ReportBadMessage(
        "No send feedback callback was attached to the page handler.");
    return;
  }

  std::optional<ScannerFeedbackInfo> feedback_info =
      TakeScannerFeedbackInfoForBrowserContext(*browser_context_, id_);
  if (!feedback_info.has_value()) {
    mojo::ReportBadMessage("No feedback info was set.");
    return;
  }

  std::move(send_feedback_callback_)
      .Run(std::move(*feedback_info), user_description);
}

}  // namespace ash
