// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PRINT_PREVIEW_CROS_PRINT_PREVIEW_CROS_UI_H_
#define ASH_WEBUI_PRINT_PREVIEW_CROS_PRINT_PREVIEW_CROS_UI_H_

#include "ash/webui/common/chrome_os_webui_config.h"
#include "ash/webui/print_preview_cros/url_constants.h"
#include "content/public/common/url_constants.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace content {
class BrowserContext;
class WebUI;
}  // namespace content

namespace ash::printing::print_preview {

class PrintPreviewCrosUI;

// The WebUI configuration for chrome://os-print.
class PrintPreviewCrosUIConfig
    : public ash::ChromeOSWebUIConfig<PrintPreviewCrosUI> {
 public:
  PrintPreviewCrosUIConfig()
      : ChromeOSWebUIConfig(content::kChromeUIScheme,
                            ash::kChromeUIPrintPreviewCrosHost) {}

  // ash::ChromeOSWebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// The WebUI controller for chrome://os-print.
class PrintPreviewCrosUI : public ui::MojoWebDialogUI {
 public:
  explicit PrintPreviewCrosUI(content::WebUI* web_ui);
  PrintPreviewCrosUI(const PrintPreviewCrosUI&) = delete;
  PrintPreviewCrosUI& operator=(const PrintPreviewCrosUI&) = delete;
  ~PrintPreviewCrosUI() override;
};

}  // namespace ash::printing::print_preview

#endif  // ASH_WEBUI_PRINT_PREVIEW_CROS_PRINT_PREVIEW_CROS_UI_H_
