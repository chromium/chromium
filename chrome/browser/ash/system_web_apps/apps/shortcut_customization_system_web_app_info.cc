// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/shortcut_customization_system_web_app_info.h"

#include <memory>

#include "ash/strings/grit/ash_strings.h"
#include "ash/webui/grit/ash_shortcut_customization_app_resources.h"
#include "ash/webui/shortcut_customization_ui/url_constants.h"
#include "chrome/browser/ash/system_web_apps/apps/system_web_app_install_utils.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "ui/base/l10n/l10n_util.h"

std::unique_ptr<web_app::WebAppInstallInfo>
CreateWebAppInfoForShortcutCustomizationSystemWebApp() {
  GURL start_url(ash::kChromeUIShortcutCustomizationAppURL);
  auto info =
      web_app::CreateSystemWebAppInstallInfoWithStartUrlAsIdentity(start_url);
  info->scope = GURL(ash::kChromeUIShortcutCustomizationAppURL);
  info->title =
      l10n_util::GetStringUTF16(IDS_ASH_SHORTCUT_CUSTOMIZATION_APP_TITLE);
  info->theme_color =
      web_app::GetDefaultBackgroundColor(/*use_dark_mode=*/false);
  info->dark_mode_theme_color =
      web_app::GetDefaultBackgroundColor(/*use_dark_mode=*/true);
  info->background_color = info->theme_color;
  info->dark_mode_background_color = info->dark_mode_theme_color;
  web_app::CreateIconInfoForSystemWebApp(
      info->start_url(),
      {{"app_icon_192.png", 192,
        IDR_ASH_SHORTCUT_CUSTOMIZATION_APP_APP_ICON_192_PNG}},
      *info);
  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  info->user_display_mode = web_app::mojom::UserDisplayMode::kStandalone;

  return info;
}

ShortcutCustomizationSystemAppDelegate::ShortcutCustomizationSystemAppDelegate(
    Profile* profile)
    : ash::SystemWebAppDelegate(ash::SystemWebAppType::SHORTCUT_CUSTOMIZATION,
                                "ShortcutCustomization",
                                GURL(ash::kChromeUIShortcutCustomizationAppURL),
                                profile) {}

std::unique_ptr<web_app::WebAppInstallInfo>
ShortcutCustomizationSystemAppDelegate::GetWebAppInfo() const {
  return CreateWebAppInfoForShortcutCustomizationSystemWebApp();
}

bool ShortcutCustomizationSystemAppDelegate::IsAppEnabled() const {
  return true;
}

gfx::Size ShortcutCustomizationSystemAppDelegate::GetMinimumWindowSize() const {
  return {600, 600};
}
