// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_VC_BACKGROUND_UI_VC_BACKGROUND_UI_H_
#define ASH_WEBUI_VC_BACKGROUND_UI_VC_BACKGROUND_UI_H_

#include <memory>

#include "ash/webui/common/mojom/sea_pen.mojom-forward.h"
#include "ash/webui/system_apps/public/system_web_app_ui_config.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"

namespace content {
class WebUIDataSource;
}  // namespace content

namespace ui {
class ColorChangeHandler;
}

namespace ash::common {
class SeaPenProvider;
}  // namespace ash::common

namespace ash::vc_background_ui {

class VcBackgroundUI;

class VcBackgroundUIConfig : public SystemWebAppUIConfig<VcBackgroundUI> {
 public:
  explicit VcBackgroundUIConfig(
      SystemWebAppUIConfig::CreateWebUIControllerFunc create_controller_func);

  VcBackgroundUIConfig(const VcBackgroundUIConfig&) = delete;
  VcBackgroundUIConfig& operator=(const VcBackgroundUIConfig&) = delete;

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

class VcBackgroundUI : public ui::MojoWebUIController {
 public:
  VcBackgroundUI(
      content::WebUI* web_ui,
      std::unique_ptr<::ash::common::SeaPenProvider> sea_pen_provider);

  VcBackgroundUI(const VcBackgroundUI&) = delete;
  VcBackgroundUI& operator=(const VcBackgroundUI&) = delete;

  ~VcBackgroundUI() override;

  void BindInterface(
      mojo::PendingReceiver<::ash::personalization_app::mojom::SeaPenProvider>
          receiver);
  void BindInterface(
      mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
          receiver);

 private:
  void AddBooleans(content::WebUIDataSource* source);

  std::unique_ptr<::ash::common::SeaPenProvider> sea_pen_provider_;
  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash::vc_background_ui

#endif  // ASH_WEBUI_VC_BACKGROUND_UI_VC_BACKGROUND_UI_H_
