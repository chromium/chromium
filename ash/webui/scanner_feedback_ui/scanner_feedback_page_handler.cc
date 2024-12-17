// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/scanner_feedback_ui/scanner_feedback_page_handler.h"

#include <optional>
#include <utility>

#include "ash/webui/scanner_feedback_ui/mojom/scanner_feedback_ui.mojom.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace ash {

ScannerFeedbackPageHandler::ScannerFeedbackPageHandler() = default;

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
  if (!feedback_info_.has_value()) {
    mojo::ReportBadMessage(
        "No feedback info was attached to the page handler.");
    // The callback still needs to be called with a valid `FeedbackInfoPtr`
    // before the renderer can be killed.
    std::move(callback).Run(std::move(feedback_info_ptr));
    return;
  }

  feedback_info_ptr->action_details = feedback_info_->action_details;

  std::move(callback).Run(std::move(feedback_info_ptr));
}

}  // namespace ash
