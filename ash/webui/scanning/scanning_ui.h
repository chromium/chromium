// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SCANNING_SCANNING_UI_H_
#define ASH_WEBUI_SCANNING_SCANNING_UI_H_

#include <memory>

#include "ash/webui/common/chrome_os_webui_config.h"
#include "ash/webui/common/mojom/accessibility_features.mojom.h"
#include "ash/webui/scanning/mojom/scanning.mojom-forward.h"
#include "ash/webui/scanning/scanning_handler.h"
#include "ash/webui/scanning/url_constants.h"
#include "base/functional/callback.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"

namespace content {
class WebUI;
}  // namespace content

namespace ui {
class ColorChangeHandler;
}

namespace ash {

class AccessibilityFeatures;

class ScanningAppDelegate;
class ScanningUI;

// The WebUIConfig for chrome://scanning.
class ScanningUIConfig : public ChromeOSWebUIConfig<ScanningUI> {
 public:
  explicit ScanningUIConfig(CreateWebUIControllerFunc create_controller_func)
      : ChromeOSWebUIConfig(content::kChromeUIScheme,
                            ash::kChromeUIScanningAppHost,
                            create_controller_func) {}
};

// The WebUI for chrome://scanning.
class ScanningUI : public ui::MojoWebUIController {
 public:
  using BindScanServiceCallback = base::RepeatingCallback<void(
      mojo::PendingReceiver<scanning::mojom::ScanService>)>;

  ScanningUI(content::WebUI* web_ui,
             std::unique_ptr<ScanningAppDelegate> scanning_app_delegate);
  ~ScanningUI() override;

  ScanningUI(const ScanningUI&) = delete;
  ScanningUI& operator=(const ScanningUI&) = delete;

  // Instantiates the implementor of the ash::scanning::mojom::ScanService
  // Mojo interface by passing the pending receiver that will be internally
  // bound.
  void BindInterface(
      mojo::PendingReceiver<scanning::mojom::ScanService> pending_receiver);

  // Instantiates the implementor of the
  // ash::common::mojom::AccessibilityFeatures Mojo interface by passing the
  // pending receiver that will be internally bound.
  void BindInterface(mojo::PendingReceiver<common::mojom::AccessibilityFeatures>
                         pending_receiver);

  void BindInterface(
      mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
          receiver);

 private:
  // Callback to bind the pending receiver to an implementation of
  // ash::scanning::mojom::ScanService.
  const BindScanServiceCallback bind_pending_receiver_callback_;

  std::unique_ptr<AccessibilityFeatures> accessibility_features_;
  // The color change handler notifies the WebUI when the color provider
  // changes.
  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash

#endif  // ASH_WEBUI_SCANNING_SCANNING_UI_H_
