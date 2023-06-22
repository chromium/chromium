// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PRINT_MANAGEMENT_PRINT_MANAGEMENT_UI_H_
#define ASH_WEBUI_PRINT_MANAGEMENT_PRINT_MANAGEMENT_UI_H_

#include <memory>

#include "ash/webui/common/chrome_os_webui_config.h"
#include "ash/webui/print_management/url_constants.h"
#include "chromeos/components/print_management/mojom/printing_manager.mojom-forward.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"

namespace ui {
class ColorChangeHandler;
}  // namespace ui

namespace ash {
namespace printing {
namespace printing_manager {

class PrintManagementDelegate;
class PrintManagementHandler;
class PrintManagementUI;

// The WebUIConfig for chrome://print-management/.
class PrintManagementUIConfig : public ChromeOSWebUIConfig<PrintManagementUI> {
 public:
  explicit PrintManagementUIConfig(
      CreateWebUIControllerFunc create_controller_func)
      : ChromeOSWebUIConfig(content::kChromeUIScheme,
                            ash::kChromeUIPrintManagementHost,
                            create_controller_func) {}
};

// The WebUI for chrome://print-management/.
class PrintManagementUI : public ui::MojoWebUIController {
 public:
  using BindPrintingMetadataProviderCallback = base::RepeatingCallback<void(
      mojo::PendingReceiver<chromeos::printing::printing_manager::mojom::
                                PrintingMetadataProvider>)>;

  // |callback_| should bind the pending receiver to an implementation of
  // chromeos::printing::printing_manager::mojom::PrintingMetadataProvider.
  PrintManagementUI(content::WebUI* web_ui,
                    BindPrintingMetadataProviderCallback callback_,
                    std::unique_ptr<PrintManagementDelegate> delegate);
  ~PrintManagementUI() override;

  PrintManagementUI(const PrintManagementUI&) = delete;
  PrintManagementUI& operator=(const PrintManagementUI&) = delete;

  // Instantiates implementor of the
  // chromeos::printing::printing_manager::mojom::PrintingManager mojo interface
  // by passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<
          chromeos::printing::printing_manager::mojom::PrintingMetadataProvider>
          receiver);

  void BindInterface(
      mojo::PendingReceiver<
          chromeos::printing::printing_manager::mojom::PrintManagementHandler>
          receiver);

  void BindInterface(
      mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
          receiver);

 private:
  const BindPrintingMetadataProviderCallback bind_pending_receiver_callback_;

  // The print management handler provides extra functionality such as opening
  // Printer settings.
  std::unique_ptr<ash::printing::printing_manager::PrintManagementHandler>
      print_management_handler_;

  // The color change handler notifies the WebUI when the color provider
  // changes.
  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace printing_manager
}  // namespace printing
}  // namespace ash

#endif  // ASH_WEBUI_PRINT_MANAGEMENT_PRINT_MANAGEMENT_UI_H_
