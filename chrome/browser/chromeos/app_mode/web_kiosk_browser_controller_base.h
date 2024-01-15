// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_WEB_KIOSK_BROWSER_CONTROLLER_BASE_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_WEB_KIOSK_BROWSER_CONTROLLER_BASE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/webapps/common/web_app_id.h"
#include "ui/base/models/image_model.h"

class Browser;

namespace web_app {
class WebAppRegistrar;
class WebAppProvider;
}  // namespace web_app

namespace chromeos {

// Class to encapsulate logic to control the browser UI for web Kiosk apps. It
// displays a fullscreen browser without tab strip and navigation bar. Therefore
// app name and icon are not needed.
class WebKioskBrowserControllerBase : public web_app::AppBrowserController {
 public:
  WebKioskBrowserControllerBase(web_app::WebAppProvider& provider,
                                Browser* browser,
                                webapps::AppId app_id);
  WebKioskBrowserControllerBase(const WebKioskBrowserControllerBase&) = delete;
  WebKioskBrowserControllerBase& operator=(
      const WebKioskBrowserControllerBase&) = delete;
  ~WebKioskBrowserControllerBase() override;

  // AppBrowserController:
  bool HasMinimalUiButtons() const override;
  ui::ImageModel GetWindowAppIcon() const override;
  ui::ImageModel GetWindowIcon() const override;
  std::u16string GetAppShortName() const override;
  std::u16string GetFormattedUrlOrigin() const override;
  GURL GetAppStartUrl() const override;
  bool IsUrlInAppScope(const GURL& url) const override;
  bool CanUserUninstall() const override;
  bool IsInstalled() const override;
  bool IsHostedApp() const override;
  bool HasReloadButton() const override;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  bool ShouldShowCustomTabBar() const override;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_WEB_KIOSK_BROWSER_CONTROLLER_BASE_H_
