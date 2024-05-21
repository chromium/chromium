// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/boca_web_app_info.h"

#include "ash/constants/ash_features.h"
#include "ash/webui/boca_ui/url_constants.h"
#include "ash/webui/grit/ash_boca_ui_resources.h"
#include "chrome/browser/ash/system_web_apps/apps/system_web_app_install_utils.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chromeos/constants/chromeos_features.h"

std::unique_ptr<web_app::WebAppInstallInfo> CreateWebAppInfoForBocaApp() {
  GURL start_url = GURL(ash::kChromeBocaAppIndexURL);
  auto info =
      web_app::CreateSystemWebAppInstallInfoWithStartUrlAsIdentity(start_url);
  info->scope = GURL(ash::kChromeBocaAppIndexURL);
  // TODO(aprilzhou): Convert the title to a localized string
  info->title = u"BOCA";
  web_app::CreateIconInfoForSystemWebApp(
      info->start_url,
      {{"app_icon_120.png", 120, IDR_ASH_BOCA_UI_APP_ICON_120_PNG}}, *info);
  info->theme_color =
      web_app::GetDefaultBackgroundColor(/*use_dark_mode=*/false);
  info->dark_mode_theme_color =
      web_app::GetDefaultBackgroundColor(/*use_dark_mode=*/true);
  info->background_color = info->theme_color;
  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  info->user_display_mode = web_app::mojom::UserDisplayMode::kStandalone;

  return info;
}

BocaSystemAppDelegate::BocaSystemAppDelegate(Profile* profile)
    : ash::SystemWebAppDelegate(ash::SystemWebAppType::BOCA,
                                "Boca",
                                GURL(ash::kChromeBocaAppURL),
                                profile) {}

std::unique_ptr<web_app::WebAppInstallInfo>
BocaSystemAppDelegate::GetWebAppInfo() const {
  return CreateWebAppInfoForBocaApp();
}

bool BocaSystemAppDelegate::ShouldCaptureNavigations() const {
  return true;
}

bool BocaSystemAppDelegate::IsAppEnabled() const {
  return ash::features::IsBocaEnabled();
}
