// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_info.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/webui/grit/ash_personalization_app_resources.h"
#include "ash/webui/personalization_app/personalization_app_url_constants.h"
#include "chrome/browser/ash/web_applications/system_web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/size.h"

std::unique_ptr<WebAppInstallInfo> CreateWebAppInfoForPersonalizationApp() {
  std::unique_ptr<WebAppInstallInfo> info =
      std::make_unique<WebAppInstallInfo>();
  info->start_url = GURL(ash::kChromeUIPersonalizationAppURL);
  info->scope = GURL(ash::kChromeUIPersonalizationAppURL);
  info->title =
      (chromeos::features::IsPersonalizationHubEnabled())
          ? l10n_util::GetStringUTF16(
                IDS_PERSONALIZATION_APP_PERSONALIZATION_HUB_TITLE)
          : l10n_util::GetStringUTF16(IDS_PERSONALIZATION_APP_WALLPAPER_LABEL);
  web_app::CreateIconInfoForSystemWebApp(
      info->start_url,
      {{"app_icon_192.png", 192, IDR_ASH_PERSONALIZATION_APP_ICON_192_PNG}},
      *info);
  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  info->user_display_mode = blink::mojom::DisplayMode::kStandalone;

  return info;
}

PersonalizationSystemAppDelegate::PersonalizationSystemAppDelegate(
    Profile* profile)
    : web_app::SystemWebAppDelegate(web_app::SystemAppType::PERSONALIZATION,
                                    "Personalization",
                                    GURL(ash::kChromeUIPersonalizationAppURL),
                                    profile) {}

std::unique_ptr<WebAppInstallInfo>
PersonalizationSystemAppDelegate::GetWebAppInfo() const {
  return CreateWebAppInfoForPersonalizationApp();
}

gfx::Size PersonalizationSystemAppDelegate::GetMinimumWindowSize() const {
  return {600, 420};
}

bool PersonalizationSystemAppDelegate::ShouldCaptureNavigations() const {
  return true;
}

bool PersonalizationSystemAppDelegate::IsAppEnabled() const {
  return chromeos::features::IsWallpaperWebUIEnabled();
}

bool PersonalizationSystemAppDelegate::ShouldShowInLauncher() const {
  return false;
}
