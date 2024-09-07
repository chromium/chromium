// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/web_kiosk_browser_controller_base.h"

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/ui/web_applications/web_app_ui_manager_impl.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace chromeos {

WebKioskBrowserControllerBase::WebKioskBrowserControllerBase(
    web_app::WebAppProvider& provider,
    Browser* browser,
    webapps::AppId app_id)
    : AppBrowserController(browser, std::move(app_id), false),
      provider_(provider) {}

WebKioskBrowserControllerBase::~WebKioskBrowserControllerBase() = default;

bool WebKioskBrowserControllerBase::HasMinimalUiButtons() const {
  return true;
}

bool WebKioskBrowserControllerBase::IsHostedApp() const {
  return true;
}

bool WebKioskBrowserControllerBase::HasReloadButton() const {
  return false;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool WebKioskBrowserControllerBase::ShouldShowCustomTabBar() const {
  return false;
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

ui::ImageModel WebKioskBrowserControllerBase::GetWindowAppIcon() const {
  if (app_icon_) {
    return *app_icon_;
  }
  app_icon_ = GetFallbackAppIcon();
  return *app_icon_;
}

ui::ImageModel WebKioskBrowserControllerBase::GetWindowIcon() const {
  return GetWindowAppIcon();
}

GURL WebKioskBrowserControllerBase::GetAppStartUrl() const {
  return registrar().GetAppStartUrl(app_id());
}

bool WebKioskBrowserControllerBase::IsUrlInAppScope(const GURL& url) const {
  return registrar().IsUrlInAppScope(url, app_id());
}

std::u16string WebKioskBrowserControllerBase::GetAppShortName() const {
  return base::UTF8ToUTF16(registrar().GetAppShortName(app_id()));
}

std::u16string WebKioskBrowserControllerBase::GetFormattedUrlOrigin() const {
  return FormatUrlOrigin(GetAppStartUrl());
}

bool WebKioskBrowserControllerBase::CanUserUninstall() const {
  return false;
}

bool WebKioskBrowserControllerBase::IsInstalled() const {
  return registrar().IsInstalled(app_id());
}

void WebKioskBrowserControllerBase::OnTabInserted(
    content::WebContents* contents) {
  AppBrowserController::OnTabInserted(contents);
  web_app::WebAppTabHelper::FromWebContents(contents)->SetIsInAppWindow(true);
}

void WebKioskBrowserControllerBase::OnTabRemoved(
    content::WebContents* contents) {
  AppBrowserController::OnTabRemoved(contents);
  web_app::WebAppTabHelper::FromWebContents(contents)->SetIsInAppWindow(false);
}

web_app::WebAppRegistrar& WebKioskBrowserControllerBase::registrar() const {
  return provider_->registrar_unsafe();
}

}  // namespace chromeos
