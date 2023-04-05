// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_FIRMWARE_UPDATE_UI_FIRMWARE_UPDATE_APP_UI_H_
#define ASH_WEBUI_FIRMWARE_UPDATE_UI_FIRMWARE_UPDATE_APP_UI_H_

#include "ash/webui/firmware_update_ui/mojom/firmware_update.mojom-forward.h"
#include "ash/webui/firmware_update_ui/url_constants.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/web_dialogs/web_dialog_ui.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom-forward.h"

namespace content {
class WebUI;
}  // namespace content

namespace ui {
class ColorChangeHandler;
}  // namespace ui

namespace ash {

class FirmwareUpdateAppUI;

// WebUIConfig for chrome://accessory-update
class FirmwareUpdateAppUIConfig
    : public content::DefaultWebUIConfig<FirmwareUpdateAppUI> {
 public:
  FirmwareUpdateAppUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           ash::kChromeUIFirmwareUpdateAppHost) {}
};

class FirmwareUpdateAppUI : public ui::MojoWebDialogUI {
 public:
  explicit FirmwareUpdateAppUI(content::WebUI* web_ui);
  FirmwareUpdateAppUI(const FirmwareUpdateAppUI&) = delete;
  FirmwareUpdateAppUI& operator=(const FirmwareUpdateAppUI&) = delete;
  ~FirmwareUpdateAppUI() override;
  void BindInterface(
      mojo::PendingReceiver<firmware_update::mojom::UpdateProvider> receiver);

  void BindInterface(
      mojo::PendingReceiver<color_change_listener::mojom::PageHandler>);

 private:
  // The color change handler notifies the WebUI when the color provider
  // changes.
  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash

#endif  // ASH_WEBUI_FIRMWARE_UPDATE_UI_FIRMWARE_UPDATE_APP_UI_H_