// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_CAMERA_APP_UI_CAMERA_APP_UI_H_
#define ASH_WEBUI_CAMERA_APP_UI_CAMERA_APP_UI_H_

#include "ash/webui/camera_app_ui/camera_app_helper.mojom.h"
#include "ash/webui/camera_app_ui/camera_app_ui_delegate.h"
#include "ash/webui/camera_app_ui/url_constants.h"
#include "ash/webui/system_apps/public/system_web_app_ui_config.h"
#include "content/public/browser/devtools_agent_host_observer.h"
#include "content/public/browser/web_ui.h"
#include "media/capture/video/chromeos/mojom/camera_app.mojom.h"
#include "ui/aura/window.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"

namespace media {
class CameraAppDeviceProviderImpl;
}  // namespace media

namespace ui {
class ColorChangeHandler;
}  // namespace ui

namespace ash {

class CameraAppHelperImpl;
class CameraAppUI;

class CameraAppUIConfig : public SystemWebAppUIConfig<CameraAppUI> {
 public:
  explicit CameraAppUIConfig(
      SystemWebAppUIConfig::CreateWebUIControllerFunc create_controller_func)
      : SystemWebAppUIConfig(kChromeUICameraAppHost,
                             SystemWebAppType::CAMERA,
                             create_controller_func) {}
};

class CameraAppUI : public ui::MojoWebUIController,
                    public content::DevToolsAgentHostObserver {
 public:
  CameraAppUI(content::WebUI* web_ui,
              std::unique_ptr<CameraAppUIDelegate> delegate);

  CameraAppUI(const CameraAppUI&) = delete;
  CameraAppUI& operator=(const CameraAppUI&) = delete;

  ~CameraAppUI() override;

  // Instantiates implementor of the cros::mojom::CameraAppDeviceProvider mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<cros::mojom::CameraAppDeviceProvider> receiver);

  // Instantiates implementor of the camera_app::mojom::CameraAppHelper
  // mojo interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<camera_app::mojom::CameraAppHelper> receiver);

  // Instantiates implementor of the mojom::PageHandler mojo interface passing
  // the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
          receiver);

  CameraAppUIDelegate* delegate() { return delegate_.get(); }

  aura::Window* window();

  const GURL& url();

  // content::DevToolsAgentHostObserver overrides.
  void DevToolsAgentHostAttached(
      content::DevToolsAgentHost* agent_host) override;
  void DevToolsAgentHostDetached(
      content::DevToolsAgentHost* agent_host) override;

  // content::WebUIController overrides.
  bool IsJavascriptErrorReportingEnabled() override;

 private:
  std::unique_ptr<CameraAppUIDelegate> delegate_;

  std::unique_ptr<media::CameraAppDeviceProviderImpl> provider_;

  std::unique_ptr<CameraAppHelperImpl> helper_;

  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

bool CameraAppUIShouldEnableLocalOverride(const std::string& url);

}  // namespace ash

#endif  // ASH_WEBUI_CAMERA_APP_UI_CAMERA_APP_UI_H_
