// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SCANNER_FEEDBACK_UI_SCANNER_FEEDBACK_PAGE_HANDLER_H_
#define ASH_WEBUI_SCANNER_FEEDBACK_UI_SCANNER_FEEDBACK_PAGE_HANDLER_H_

#include <string>
#include <utility>

#include "ash/public/cpp/scanner/scanner_delegate.h"
#include "ash/webui/scanner_feedback_ui/mojom/scanner_feedback_ui.mojom.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {
class BrowserContext;
}

namespace ash {

// Stores and handles all data related to a Scanner feedback prompt.
//
// Used for serving Mojo calls for a given `ScannerFeedbackUntrustedUI`. Serves
// data from feedback info stored on the browser context - see
// scanner_feedback_browser_context_data.h for more details. Use `id()` to get
// the ID used for this page.
class ScannerFeedbackPageHandler
    : public mojom::scanner_feedback_ui::PageHandler {
 public:
  // Callback called from `CloseDialog`.
  // Not guaranteed to be called if, for example, if the user closes the UI
  // through other means such as the Escape key.
  // May be called multiple times if, for example, this callback is a no-op and
  // the button is clicked multiple times, or a user's click races the closing
  // of the dialog.
  using CloseDialogCallback = base::RepeatingClosure;

  explicit ScannerFeedbackPageHandler(content::BrowserContext& browser_context);

  ScannerFeedbackPageHandler(const ScannerFeedbackPageHandler&) = delete;
  ScannerFeedbackPageHandler& operator=(const ScannerFeedbackPageHandler&) =
      delete;

  ~ScannerFeedbackPageHandler() override;

  // Binds the receiver, unbinding the existing one if necessary.
  void Bind(
      mojo::PendingReceiver<mojom::scanner_feedback_ui::PageHandler> receiver);

  void SetCloseDialogCallback(CloseDialogCallback close_dialog_callback) {
    close_dialog_callback_ = std::move(close_dialog_callback);
  }
  void SetSendFeedbackCallback(
      ScannerDelegate::SendFeedbackCallback send_feedback_callback) {
    send_feedback_callback_ = std::move(send_feedback_callback);
  }

  base::UnguessableToken id() const { return id_; }

  // mojom::scanner_feedback_ui::PageHandler:
  void GetFeedbackInfo(GetFeedbackInfoCallback callback) override;
  void CloseDialog() override;
  void SendFeedback(const std::string& user_description) override;

 private:
  const base::UnguessableToken id_;

  // Null on construction. Set in `SetCloseDialogCallback`.
  CloseDialogCallback close_dialog_callback_;
  // Null on construction. Set in `SetSendFeedbackCallback`. Reset to null when
  // `SendFeedback` is called.
  ScannerDelegate::SendFeedbackCallback send_feedback_callback_;

  const raw_ref<content::BrowserContext> browser_context_;

  mojo::Receiver<mojom::scanner_feedback_ui::PageHandler> receiver_{this};
};

}  // namespace ash

#endif  // ASH_WEBUI_SCANNER_FEEDBACK_UI_SCANNER_FEEDBACK_PAGE_HANDLER_H_
