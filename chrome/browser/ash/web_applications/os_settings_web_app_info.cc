// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/os_settings_web_app_info.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "base/feature_list.h"
#include "chrome/browser/ash/web_applications/system_web_app_install_utils.h"
#include "chrome/browser/web_applications/user_display_mode.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/os_settings_resources.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/styles/cros_styles.h"

namespace {

SkColor GetBgColor(bool use_dark_mode) {
  return cros_styles::ResolveColor(
      cros_styles::ColorName::kBgColor, use_dark_mode,
      base::FeatureList::IsEnabled(
          ash::features::kSemanticColorsDebugOverride));
}

}  // namespace

std::unique_ptr<WebAppInstallInfo> CreateWebAppInfoForOSSettingsSystemWebApp() {
  std::unique_ptr<WebAppInstallInfo> info =
      std::make_unique<WebAppInstallInfo>();
  info->start_url = GURL(chrome::kChromeUIOSSettingsURL);
  info->scope = GURL(chrome::kChromeUIOSSettingsURL);
  info->title = l10n_util::GetStringUTF16(IDS_SETTINGS_SETTINGS);
  web_app::CreateIconInfoForSystemWebApp(
      info->start_url,
      {
          {"icon-192.png", 192, IDR_SETTINGS_LOGO_192},

      },
      *info);
  info->theme_color = GetBgColor(/*use_dark_mode=*/false);
  info->dark_mode_theme_color = GetBgColor(/*use_dark_mode=*/true);
  info->background_color = info->theme_color;
  info->dark_mode_background_color = info->dark_mode_theme_color;
  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  info->user_display_mode = web_app::UserDisplayMode::kStandalone;
  return info;
}

OSSettingsSystemAppDelegate::OSSettingsSystemAppDelegate(Profile* profile)
    : web_app::SystemWebAppDelegate(web_app::SystemAppType::SETTINGS,
                                    "OSSettings",
                                    GURL(chrome::kChromeUISettingsURL),
                                    profile) {}

std::unique_ptr<WebAppInstallInfo> OSSettingsSystemAppDelegate::GetWebAppInfo()
    const {
  return CreateWebAppInfoForOSSettingsSystemWebApp();
}

bool OSSettingsSystemAppDelegate::ShouldCaptureNavigations() const {
  return true;
}

gfx::Size OSSettingsSystemAppDelegate::GetMinimumWindowSize() const {
  return {300, 100};
}

std::vector<web_app::AppId>
OSSettingsSystemAppDelegate::GetAppIdsToUninstallAndReplace() const {
  return {web_app::kSettingsAppId, ash::kInternalAppIdSettings};
}

bool OSSettingsSystemAppDelegate::PreferManifestBackgroundColor() const {
  return true;
}

bool OSSettingsSystemAppDelegate::ShouldAnimateThemeChanges() const {
  return ash::features::IsSettingsAppThemeChangeAnimationEnabled();
}
