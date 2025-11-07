// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_web_app_browser_controller.h"

#include <optional>
#include <string>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/ui/web_applications/web_app_ui_manager_impl.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/models/image_model.h"
#include "url/gurl.h"

namespace chromeos {

KioskWebAppBrowserController::KioskWebAppBrowserController(
    web_app::WebAppProvider& provider,
    Browser* browser,
    webapps::AppId app_id)
    : AppBrowserController(browser, std::move(app_id), false),
      provider_(provider) {}

KioskWebAppBrowserController::~KioskWebAppBrowserController() = default;

bool KioskWebAppBrowserController::HasMinimalUiButtons() const {
  return true;
}

bool KioskWebAppBrowserController::HasReloadButton() const {
  return false;
}

bool KioskWebAppBrowserController::ShouldShowCustomTabBar() const {
  return false;
}

ui::ImageModel KioskWebAppBrowserController::GetWindowAppIcon() const {
  if (app_icon_) {
    return *app_icon_;
  }
  app_icon_ = GetFallbackAppIcon();
  return *app_icon_;
}

ui::ImageModel KioskWebAppBrowserController::GetWindowIcon() const {
  return GetWindowAppIcon();
}

const GURL& KioskWebAppBrowserController::GetAppStartUrl() const {
  return registrar().GetAppStartUrl(app_id());
}

bool KioskWebAppBrowserController::IsUrlInAppScope(const GURL& url) const {
  return registrar().IsUrlInAppScope(url, app_id());
}

std::u16string KioskWebAppBrowserController::GetAppShortName() const {
  return base::UTF8ToUTF16(registrar().GetAppShortName(app_id()));
}

std::u16string KioskWebAppBrowserController::GetFormattedUrlOrigin() const {
  return FormatUrlOrigin(GetAppStartUrl());
}

bool KioskWebAppBrowserController::CanUserUninstall() const {
  return false;
}

bool KioskWebAppBrowserController::IsInstalled() const {
  return registrar().IsInstallState(
      app_id(), {web_app::proto::InstallState::SUGGESTED_FROM_ANOTHER_DEVICE,
                 web_app::proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION,
                 web_app::proto::InstallState::INSTALLED_WITH_OS_INTEGRATION});
}

bool KioskWebAppBrowserController::IsIsolatedWebApp() const {
  return registrar().AppMatches(app_id(),
                                web_app::WebAppFilter::IsIsolatedApp());
}

void KioskWebAppBrowserController::OnTabInserted(
    content::WebContents* contents) {
  AppBrowserController::OnTabInserted(contents);
  web_app::WebAppTabHelper::FromWebContents(contents)->SetIsInAppWindow(
      app_id());
}

void KioskWebAppBrowserController::OnTabRemoved(
    content::WebContents* contents) {
  AppBrowserController::OnTabRemoved(contents);
  web_app::WebAppTabHelper::FromWebContents(contents)->SetIsInAppWindow(
      /*window_app_id=*/std::nullopt);
}

web_app::WebAppRegistrar& KioskWebAppBrowserController::registrar() const {
  return provider_->registrar_unsafe();
}

}  // namespace chromeos
