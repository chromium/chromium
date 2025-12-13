// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_WEB_APP_BROWSER_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_WEB_APP_BROWSER_CONTROLLER_H_

#include <optional>
#include <string>

#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/models/image_model.h"

class Browser;

namespace web_app {
class WebAppRegistrar;
class WebAppProvider;
}  // namespace web_app

namespace chromeos {

// Encapsulates logic to control the browser UI for web Kiosk apps. It displays
// a fullscreen browser without tab strip and navigation bar. Therefore app name
// and icon are not needed.
class KioskWebAppBrowserController : public web_app::AppBrowserController {
 public:
  KioskWebAppBrowserController(web_app::WebAppProvider& provider,
                               Browser* browser,
                               webapps::AppId app_id);
  KioskWebAppBrowserController(const KioskWebAppBrowserController&) = delete;
  KioskWebAppBrowserController& operator=(const KioskWebAppBrowserController&) =
      delete;
  ~KioskWebAppBrowserController() override;

  // AppBrowserController:
  bool HasMinimalUiButtons() const override;
  ui::ImageModel GetWindowAppIcon() const override;
  ui::ImageModel GetWindowIcon() const override;
  std::u16string GetAppShortName() const override;
  std::u16string GetFormattedUrlOrigin() const override;
  const GURL& GetAppStartUrl() const override;
  bool IsUrlInAppScope(const GURL& url) const override;
  bool CanUserUninstall() const override;
  bool IsInstalled() const override;
  bool HasReloadButton() const override;
  bool ShouldShowCustomTabBar() const override;
  bool IsIsolatedWebApp() const override;

 protected:
  // AppBrowserController:
  void OnTabInserted(content::WebContents* contents) override;
  void OnTabRemoved(content::WebContents* contents) override;

 private:
  web_app::WebAppRegistrar& registrar() const;

  const raw_ref<web_app::WebAppProvider> provider_;
  mutable std::optional<ui::ImageModel> app_icon_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_WEB_APP_BROWSER_CONTROLLER_H_
