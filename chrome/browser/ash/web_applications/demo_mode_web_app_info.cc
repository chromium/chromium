// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/demo_mode_web_app_info.h"

#include "ash/webui/demo_mode_app_ui/url_constants.h"
#include "ash/webui/grit/ash_demo_mode_app_resources.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/web_applications/system_web_app_install_utils.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chromeos/constants/chromeos_features.h"

std::unique_ptr<WebAppInstallInfo> CreateWebAppInfoForDemoModeApp() {
  std::unique_ptr<WebAppInstallInfo> info =
      std::make_unique<WebAppInstallInfo>();
  info->start_url = GURL(ash::kChromeUntrustedUIDemoModeAppIndexURL);
  info->scope = GURL(ash::kChromeUntrustedUIDemoModeAppURL);
  // TODO(b/185608502): Convert the title to a localized string
  info->title = u"Demo Mode App";
  web_app::CreateIconInfoForSystemWebApp(
      info->start_url,
      {{"app_icon_192.png", 192, IDR_ASH_DEMO_MODE_APP_APP_ICON_192_PNG}},
      *info);
  info->theme_color = 0xFF4285F4;
  info->background_color = 0xFFFFFFFF;
  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  info->user_display_mode = web_app::mojom::UserDisplayMode::kStandalone;

  return info;
}

DemoModeSystemAppDelegate::DemoModeSystemAppDelegate(Profile* profile)
    : ash::SystemWebAppDelegate(ash::SystemWebAppType::DEMO_MODE,
                                "DemoMode",
                                GURL(ash::kChromeUntrustedUIDemoModeAppURL),
                                profile) {}

std::unique_ptr<WebAppInstallInfo> DemoModeSystemAppDelegate::GetWebAppInfo()
    const {
  return CreateWebAppInfoForDemoModeApp();
}

bool DemoModeSystemAppDelegate::ShouldCaptureNavigations() const {
  return true;
}

bool DemoModeSystemAppDelegate::IsAppEnabled() const {
  return chromeos::features::IsDemoModeSWAEnabled() &&
         ash::DemoSession::IsDeviceInDemoMode();
}
