// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/demo_mode_web_app_info.h"

#include "ash/webui/demo_mode_app_ui/url_constants.h"
#include "ash/webui/grit/ash_demo_mode_app_resources.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/system_web_apps/apps/system_web_app_install_utils.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chromeos/constants/chromeos_features.h"

std::unique_ptr<web_app::WebAppInstallInfo> CreateWebAppInfoForDemoModeApp() {
  std::unique_ptr<web_app::WebAppInstallInfo> info =
      std::make_unique<web_app::WebAppInstallInfo>();
  info->start_url = GURL(ash::kChromeUntrustedUIDemoModeAppIndexURL);
  info->scope = GURL(ash::kChromeUntrustedUIDemoModeAppURL);
  // TODO(b/185608502): Convert the title to a localized string
  info->title = u"Demo Mode App";
  web_app::CreateIconInfoForSystemWebApp(
      info->start_url,
      {{"app_icon_192.png", 192, IDR_ASH_DEMO_MODE_APP_APP_ICON_192_PNG}},
      *info);
  info->theme_color =
      web_app::GetDefaultBackgroundColor(/*use_dark_mode=*/false);
  info->dark_mode_theme_color =
      web_app::GetDefaultBackgroundColor(/*use_dark_mode=*/true);
  info->background_color = info->theme_color;
  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  info->user_display_mode = web_app::mojom::UserDisplayMode::kStandalone;

  return info;
}

DemoModeSystemAppDelegate::DemoModeSystemAppDelegate(Profile* profile)
    : ash::SystemWebAppDelegate(ash::SystemWebAppType::DEMO_MODE,
                                "DemoMode",
                                GURL(ash::kChromeUntrustedUIDemoModeAppURL),
                                profile) {}

std::unique_ptr<web_app::WebAppInstallInfo>
DemoModeSystemAppDelegate::GetWebAppInfo() const {
  return CreateWebAppInfoForDemoModeApp();
}

bool DemoModeSystemAppDelegate::ShouldCaptureNavigations() const {
  return true;
}

bool DemoModeSystemAppDelegate::IsAppEnabled() const {
  return ash::DemoSession::IsDeviceInDemoMode();
}
