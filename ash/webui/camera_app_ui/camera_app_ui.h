// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_CAMERA_APP_UI_CAMERA_APP_UI_H_
#define ASH_WEBUI_CAMERA_APP_UI_CAMERA_APP_UI_H_

#include "ash/webui/camera_app_ui/camera_app_helper.mojom.h"
#include "ash/webui/camera_app_ui/camera_app_ui_delegate.h"
#include "ash/webui/camera_app_ui/url_constants.h"
#include "ash/webui/system_apps/public/system_web_app_ui_config.h"
#include "base/memory/raw_ptr.h"
#include "content/public/browser/devtools_agent_host_observer.h"
#include "content/public/browser/web_ui.h"
#include "media/capture/video/chromeos/mojom/camera_app.mojom.h"
#include "ui/aura/window.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace media {
class CameraAppDeviceProviderImpl;
}  // namespace media

namespace media_device_salt {
class MediaDeviceSaltService;
}  // namespace media_device_salt

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
  // `media_device_salt_service` provides the salts used to translate media
  // device IDs. It has to be the service content itself uses for this browser
  // context, because the IDs translated here are the ones the renderer was
  // handed: ChromeContentBrowserClient::GetMediaDeviceIDSalt() looks the
  // service up for the frame's own browser context, and salts with that
  // context's unique ID when there is none.
  //
  // It is therefore null in exactly the cases where content finds no service
  // either, today a guest session. Borrowing the original profile's service
  // there would be wrong rather than merely unavailable: the salt is what
  // makes a device ID stable across sessions, and a guest session is meant to
  // leave nothing behind, so content deliberately gives it a salt that dies
  // with the browser context. The null case follows content and uses the
  // browser context's unique ID.
  //
  // The two agree on the salt only because CameraAppUI auto-grants itself the
  // cookie permission: content uses the ephemeral salt when cookies are
  // blocked for the origin, which is not checked here.
  // TODO(crbug.com/578879212): take the salt from content instead of
  // reproducing how it picks one.
  //
  // If non-null, it must outlive `this`.
  CameraAppUI(
      content::WebUI* web_ui,
      std::unique_ptr<CameraAppUIDelegate> delegate,
      media_device_salt::MediaDeviceSaltService* media_device_salt_service);

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

  const raw_ptr<media_device_salt::MediaDeviceSaltService>
      media_device_salt_service_;

  std::unique_ptr<media::CameraAppDeviceProviderImpl> provider_;

  std::unique_ptr<CameraAppHelperImpl> helper_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

bool CameraAppUIShouldEnableLocalOverride(const std::string& url);

}  // namespace ash

#endif  // ASH_WEBUI_CAMERA_APP_UI_CAMERA_APP_UI_H_
