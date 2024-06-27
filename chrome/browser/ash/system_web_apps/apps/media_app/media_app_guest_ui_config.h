// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_MEDIA_APP_MEDIA_APP_GUEST_UI_CONFIG_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_MEDIA_APP_MEDIA_APP_GUEST_UI_CONFIG_H_

#include "ash/webui/media_app_ui/media_app_guest_ui.h"
#include "ash/webui/media_app_ui/media_app_ui_untrusted.mojom.h"
#include "chrome/browser/accessibility/media_app/ax_media_app_untrusted_handler.h"
#include "content/public/browser/webui_config.h"

namespace content {
class WebUIDataSource;
class WebUIController;
class WebUI;
}  // namespace content

// Implementation of the chromeos::MediaAppGuestUIDelegate to expose some
// //chrome functions to //chromeos.
class ChromeMediaAppGuestUIDelegate : public ash::MediaAppGuestUIDelegate {
 public:
  ChromeMediaAppGuestUIDelegate();
  ChromeMediaAppGuestUIDelegate(const ChromeMediaAppGuestUIDelegate&) = delete;
  ChromeMediaAppGuestUIDelegate& operator=(
      const ChromeMediaAppGuestUIDelegate&) = delete;
  void PopulateLoadTimeData(content::WebUI* web_ui,
                            content::WebUIDataSource* source) override;
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);
  void CreateAndBindOcrHandler(
      content::BrowserContext& context,
      gfx::NativeWindow native_window,
      mojo::PendingReceiver<ash::media_app_ui::mojom::OcrUntrustedPageHandler>
          receiver,
      mojo::PendingRemote<ash::media_app_ui::mojom::OcrUntrustedPage> page)
      override;

  void CreateAndBindMahiHandler(
      mojo::PendingReceiver<ash::media_app_ui::mojom::MahiUntrustedPageHandler>
          receiver,
      mojo::PendingRemote<ash::media_app_ui::mojom::MahiUntrustedPage> page,
      const std::string& file_name,
      aura::Window* window) override;
};

// A webui config for the chrome-untrusted:// part of media-app.
class MediaAppGuestUIConfig : public content::WebUIConfig {
 public:
  MediaAppGuestUIConfig();
  MediaAppGuestUIConfig(const MediaAppGuestUIConfig& other) = delete;
  MediaAppGuestUIConfig& operator=(const MediaAppGuestUIConfig&) = delete;
  ~MediaAppGuestUIConfig() override;

  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override;
};

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_MEDIA_APP_MEDIA_APP_GUEST_UI_CONFIG_H_
