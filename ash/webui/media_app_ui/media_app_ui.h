// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_MEDIA_APP_UI_MEDIA_APP_UI_H_
#define ASH_WEBUI_MEDIA_APP_UI_MEDIA_APP_UI_H_

#include <memory>

#include "ash/webui/media_app_ui/media_app_ui.mojom.h"
#include "ash/webui/media_app_ui/media_app_ui_delegate.h"
#include "ash/webui/media_app_ui/url_constants.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "ash/webui/system_apps/public/system_web_app_ui_config.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace ash {

class MediaAppPageHandler;
class MediaAppUI;

// The WebUIConfig for chrome://media-app.
class MediaAppUIConfig : public SystemWebAppUIConfig<MediaAppUI> {
 public:
  explicit MediaAppUIConfig(
      SystemWebAppUIConfig::CreateWebUIControllerFunc create_controller_func)
      : SystemWebAppUIConfig(ash::kChromeUIMediaAppHost,
                             SystemWebAppType::MEDIA,
                             create_controller_func) {}
};

// The WebUI controller for chrome://media-app.
class MediaAppUI : public ui::MojoWebUIController,
                   public media_app_ui::mojom::PageHandlerFactory {
 public:
  MediaAppUI(content::WebUI* web_ui,
             std::unique_ptr<MediaAppUIDelegate> delegate);
  ~MediaAppUI() override;

  MediaAppUI(const MediaAppUI&) = delete;
  MediaAppUI& operator=(const MediaAppUI&) = delete;

  void BindInterface(
      mojo::PendingReceiver<media_app_ui::mojom::PageHandlerFactory> receiver);
  MediaAppUIDelegate* delegate() { return delegate_.get(); }

  // content::WebUIController:
  void WebUIRenderFrameCreated(
      content::RenderFrameHost* render_frame_host) override;

  bool IsJavascriptErrorReportingEnabled() override;

 private:
  // media_app_ui::mojom::PageHandlerFactory:
  void CreatePageHandler(mojo::PendingReceiver<media_app_ui::mojom::PageHandler>
                             receiver) override;

  std::unique_ptr<MediaAppPageHandler> page_handler_;
  mojo::Receiver<media_app_ui::mojom::PageHandlerFactory>
      page_factory_receiver_{this};
  std::unique_ptr<MediaAppUIDelegate> delegate_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash

#endif  // ASH_WEBUI_MEDIA_APP_UI_MEDIA_APP_UI_H_
