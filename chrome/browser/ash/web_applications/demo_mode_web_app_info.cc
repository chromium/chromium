// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/demo_mode_web_app_info.h"

#include "ash/constants/ash_features.h"
#include "ash/webui/demo_mode_app_ui/url_constants.h"
#include "ash/webui/grit/ash_demo_mode_app_resources.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/web_applications/system_web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"

std::unique_ptr<WebAppInstallInfo> CreateWebAppInfoForDemoModeApp() {
  std::unique_ptr<WebAppInstallInfo> info =
      std::make_unique<WebAppInstallInfo>();
  info->start_url = GURL(ash::kChromeUIDemoModeAppURL);
  info->scope = GURL(ash::kChromeUIDemoModeAppURL);
  // TODO(b/185608502): Convert the title to a localized string
  info->title = u"Demo Mode App";
  web_app::CreateIconInfoForSystemWebApp(
      info->start_url,
      {{"app_icon_192.png", 192, IDR_ASH_DEMO_MODE_APP_APP_ICON_192_PNG}},
      *info);
  info->theme_color = 0xFF4285F4;
  info->background_color = 0xFFFFFFFF;
  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  info->user_display_mode = blink::mojom::DisplayMode::kStandalone;

  return info;
}

DemoModeSystemAppDelegate::DemoModeSystemAppDelegate(Profile* profile)
    : web_app::SystemWebAppDelegate(web_app::SystemAppType::DEMO_MODE,
                                    "DemoMode",
                                    GURL("chrome://demo-mode-app"),
                                    profile) {}

std::unique_ptr<WebAppInstallInfo> DemoModeSystemAppDelegate::GetWebAppInfo()
    const {
  return CreateWebAppInfoForDemoModeApp();
}

bool DemoModeSystemAppDelegate::ShouldCaptureNavigations() const {
  return true;
}

bool DemoModeSystemAppDelegate::IsAppEnabled() const {
  return chromeos::features::IsDemoModeSWAEnabled();
}
