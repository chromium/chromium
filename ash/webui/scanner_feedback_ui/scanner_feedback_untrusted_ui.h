// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SCANNER_FEEDBACK_UI_SCANNER_FEEDBACK_UNTRUSTED_UI_H_
#define ASH_WEBUI_SCANNER_FEEDBACK_UI_SCANNER_FEEDBACK_UNTRUSTED_UI_H_

#include <memory>

#include "ash/webui/common/chrome_os_webui_config.h"
#include "ash/webui/scanner_feedback_ui/mojom/scanner_feedback_ui.mojom-forward.h"
#include "ash/webui/scanner_feedback_ui/scanner_feedback_page_handler.h"
#include "content/public/browser/web_ui_controller.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace content {
class BrowserContext;
}

namespace ash {

class ScannerFeedbackUntrustedUI;

class ScannerFeedbackUntrustedUIConfig
    : public ChromeOSWebUIConfig<ScannerFeedbackUntrustedUI> {
 public:
  ScannerFeedbackUntrustedUIConfig();
  ~ScannerFeedbackUntrustedUIConfig() override;

  // ChromeOSWebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

class ScannerFeedbackUntrustedUI : public ui::WebDialogUI {
 public:
  explicit ScannerFeedbackUntrustedUI(content::WebUI* web_ui);

  ScannerFeedbackUntrustedUI(const ScannerFeedbackUntrustedUI&) = delete;
  ScannerFeedbackUntrustedUI& operator=(const ScannerFeedbackUntrustedUI&) =
      delete;

  ~ScannerFeedbackUntrustedUI() override;

  ScannerFeedbackPageHandler& page_handler() { return page_handler_; }

  void BindInterface(
      mojo::PendingReceiver<mojom::scanner_feedback_ui::PageHandler> receiver);

 private:
  ScannerFeedbackPageHandler page_handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash

#endif  // ASH_WEBUI_SCANNER_FEEDBACK_UI_SCANNER_FEEDBACK_UNTRUSTED_UI_H_
