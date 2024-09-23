// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PRINT_PREVIEW_CROS_PRINT_PREVIEW_CROS_UI_H_
#define ASH_WEBUI_PRINT_PREVIEW_CROS_PRINT_PREVIEW_CROS_UI_H_

#include <memory>

#include "ash/webui/common/chrome_os_webui_config.h"
#include "ash/webui/print_preview_cros/mojom/destination_provider.mojom.h"
#include "ash/webui/print_preview_cros/url_constants.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/web_dialogs/web_dialog_ui.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"

namespace content {
class BrowserContext;
class WebUI;
}  // namespace content

namespace ui {
class ColorChangeHandler;
}  // namespace ui

namespace ash::printing::print_preview {

class DestinationProvider;
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

  void BindInterface(
      mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
          receiver);

  void BindInterface(
      mojo::PendingReceiver<mojom::DestinationProvider> receiver);

 private:
  std::unique_ptr<DestinationProvider> destination_provider_;
  // The color change handler notifies the WebUI when the color provider
  // changes.
  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash::printing::print_preview

#endif  // ASH_WEBUI_PRINT_PREVIEW_CROS_PRINT_PREVIEW_CROS_UI_H_
