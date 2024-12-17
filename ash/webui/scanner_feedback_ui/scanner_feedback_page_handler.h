// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SCANNER_FEEDBACK_UI_SCANNER_FEEDBACK_PAGE_HANDLER_H_
#define ASH_WEBUI_SCANNER_FEEDBACK_UI_SCANNER_FEEDBACK_PAGE_HANDLER_H_

#include <optional>
#include <utility>

#include "ash/public/cpp/scanner/scanner_feedback_info.h"
#include "ash/webui/scanner_feedback_ui/mojom/scanner_feedback_ui.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash {

// Stores and handles all data related to a Scanner feedback prompt.
//
// Used for serving Mojo calls for a given `ScannerFeedbackUntrustedUI`.
class ScannerFeedbackPageHandler
    : public mojom::scanner_feedback_ui::PageHandler {
 public:
  explicit ScannerFeedbackPageHandler();

  ScannerFeedbackPageHandler(const ScannerFeedbackPageHandler&) = delete;
  ScannerFeedbackPageHandler& operator=(const ScannerFeedbackPageHandler&) =
      delete;

  ~ScannerFeedbackPageHandler() override;

  // Binds the receiver, unbinding the existing one if necessary.
  void Bind(
      mojo::PendingReceiver<mojom::scanner_feedback_ui::PageHandler> receiver);

  void SetFeedbackInfo(ScannerFeedbackInfo feedback_info) {
    feedback_info_ = std::move(feedback_info);
  }

  // mojom::scanner_feedback_ui::PageHandler:
  void GetFeedbackInfo(GetFeedbackInfoCallback callback) override;

 private:
  // Unset on construction. Set in `SetFeedbackInfo`.
  std::optional<ScannerFeedbackInfo> feedback_info_;

  mojo::Receiver<mojom::scanner_feedback_ui::PageHandler> receiver_{this};
};

}  // namespace ash

#endif  // ASH_WEBUI_SCANNER_FEEDBACK_UI_SCANNER_FEEDBACK_PAGE_HANDLER_H_
