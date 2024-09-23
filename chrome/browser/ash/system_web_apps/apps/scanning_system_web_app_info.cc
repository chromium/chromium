// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/scanning_system_web_app_info.h"

#include <memory>

#include "ash/webui/grit/ash_scanning_app_resources.h"
#include "ash/webui/scanning/url_constants.h"
#include "chrome/browser/ash/system_web_apps/apps/system_web_app_install_utils.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

std::unique_ptr<web_app::WebAppInstallInfo>
CreateWebAppInfoForScanningSystemWebApp() {
  GURL start_url = GURL(ash::kChromeUIScanningAppUrl);
  auto info =
      web_app::CreateSystemWebAppInstallInfoWithStartUrlAsIdentity(start_url);
  info->scope = GURL(ash::kChromeUIScanningAppUrl);
  info->title = l10n_util::GetStringUTF16(IDS_SCANNING_APP_TITLE);
  web_app::CreateIconInfoForSystemWebApp(
      info->start_url(),
      {
          {"scanning_app_icon_16.png", 16,
           IDR_ASH_SCANNING_APP_SCANNING_APP_ICON_16_PNG},
          {"scanning_app_icon_32.png", 32,
           IDR_ASH_SCANNING_APP_SCANNING_APP_ICON_32_PNG},
          {"scanning_app_icon_48.png", 48,
           IDR_ASH_SCANNING_APP_SCANNING_APP_ICON_48_PNG},
          {"scanning_app_icon_64.png", 64,
           IDR_ASH_SCANNING_APP_SCANNING_APP_ICON_64_PNG},
          {"scanning_app_icon_128.png", 128,
           IDR_ASH_SCANNING_APP_SCANNING_APP_ICON_128_PNG},
          {"scanning_app_icon_192.png", 192,
           IDR_ASH_SCANNING_APP_SCANNING_APP_ICON_192_PNG},
          {"scanning_app_icon_256.png", 256,
           IDR_ASH_SCANNING_APP_SCANNING_APP_ICON_256_PNG},
      },
      *info);
  info->theme_color =
      web_app::GetDefaultBackgroundColor(/*use_dark_mode=*/false);
  info->dark_mode_theme_color =
      web_app::GetDefaultBackgroundColor(/*use_dark_mode=*/true);
  info->background_color = info->theme_color;
  info->dark_mode_background_color = info->dark_mode_theme_color;
  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  info->user_display_mode = web_app::mojom::UserDisplayMode::kStandalone;

  return info;
}

ScanningSystemAppDelegate::ScanningSystemAppDelegate(Profile* profile)
    : ash::SystemWebAppDelegate(ash::SystemWebAppType::SCANNING,
                                "Scanning",
                                GURL("chrome://scanning"),
                                profile) {}

std::unique_ptr<web_app::WebAppInstallInfo>
ScanningSystemAppDelegate::GetWebAppInfo() const {
  return CreateWebAppInfoForScanningSystemWebApp();
}

bool ScanningSystemAppDelegate::ShouldShowInLauncher() const {
  return true;
}

bool ScanningSystemAppDelegate::ShouldCaptureNavigations() const {
  return true;
}

gfx::Size ScanningSystemAppDelegate::GetMinimumWindowSize() const {
  return {600, 420};
}
