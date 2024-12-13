// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SCANNER_FEEDBACK_UI_SCANNER_FEEDBACK_UNTRUSTED_UI_H_
#define ASH_WEBUI_SCANNER_FEEDBACK_UI_SCANNER_FEEDBACK_UNTRUSTED_UI_H_

#include "ash/webui/common/chrome_os_webui_config.h"
#include "ui/webui/untrusted_web_ui_controller.h"

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

class ScannerFeedbackUntrustedUI : public ui::UntrustedWebUIController {
 public:
  explicit ScannerFeedbackUntrustedUI(content::WebUI* web_ui);

  ScannerFeedbackUntrustedUI(const ScannerFeedbackUntrustedUI&) = delete;
  ScannerFeedbackUntrustedUI& operator=(const ScannerFeedbackUntrustedUI&) =
      delete;

  ~ScannerFeedbackUntrustedUI() override;
};

}  // namespace ash

#endif  // ASH_WEBUI_SCANNER_FEEDBACK_UI_SCANNER_FEEDBACK_UNTRUSTED_UI_H_
