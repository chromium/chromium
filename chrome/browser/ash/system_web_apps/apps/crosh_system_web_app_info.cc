// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/crosh_system_web_app_info.h"

#include <memory>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/system_web_apps/apps/system_web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "url/gurl.h"

std::unique_ptr<web_app::WebAppInstallInfo>
CreateWebAppInfoForCroshSystemWebApp() {
  GURL start_url(chrome::kChromeUIUntrustedCroshURL);
  auto info =
      web_app::CreateSystemWebAppInstallInfoWithStartUrlAsIdentity(start_url);
  info->scope = GURL(chrome::kChromeUIUntrustedCroshURL);
  info->title = std::u16string(u"crosh");
  web_app::CreateIconInfoForSystemWebApp(
      info->start_url(), {{"app_icon_256.png", 256, IDR_LOGO_CROSH}}, *info);
  info->background_color = 0xFF202124;
  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  return info;
}

CroshSystemAppDelegate::CroshSystemAppDelegate(Profile* profile)
    : ash::SystemWebAppDelegate(ash::SystemWebAppType::CROSH,
                                "Crosh",
                                GURL(chrome::kChromeUIUntrustedCroshURL),
                                profile) {}

std::unique_ptr<web_app::WebAppInstallInfo>
CroshSystemAppDelegate::GetWebAppInfo() const {
  return CreateWebAppInfoForCroshSystemWebApp();
}

bool CroshSystemAppDelegate::ShouldShowInLauncher() const {
  return false;
}

Browser* CroshSystemAppDelegate::GetWindowForLaunch(Profile* profile,
                                                    const GURL& url) const {
  return nullptr;
}

bool CroshSystemAppDelegate::ShouldShowInSearchAndShelf() const {
  return false;
}

bool CroshSystemAppDelegate::ShouldHaveTabStrip() const {
  return true;
}

bool CroshSystemAppDelegate::UseSystemThemeColor() const {
  return false;
}
