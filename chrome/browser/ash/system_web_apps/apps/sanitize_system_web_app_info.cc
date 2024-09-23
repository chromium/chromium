// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/sanitize_system_web_app_info.h"

#include <memory>

#include "ash/webui/grit/ash_sanitize_app_resources.h"
#include "ash/webui/sanitize_ui/url_constants.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/system_web_apps/apps/system_web_app_install_utils.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/screen.h"
#include "url/gurl.h"

const int kSanitizeWindowWidth = 680;
const int kSanitizeWindowHeight = 680;

std::unique_ptr<web_app::WebAppInstallInfo>
CreateWebAppInfoForSanitizeSystemWebApp() {
  GURL start_url = GURL(ash::kChromeUISanitizeAppURL);
  auto info =
      web_app::CreateSystemWebAppInstallInfoWithStartUrlAsIdentity(start_url);
  info->scope = GURL(ash::kChromeUISanitizeAppURL);
  web_app::CreateIconInfoForSystemWebApp(
      info->start_url(),
      {{"app_icon_192.png", 192, IDR_ASH_SANITIZE_APP_APP_ICON_192_PNG}},
      *info);

  info->title = l10n_util::GetStringUTF16(IDS_SANITIZE);
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

SanitizeSystemAppDelegate::SanitizeSystemAppDelegate(Profile* profile)
    : ash::SystemWebAppDelegate(ash::SystemWebAppType::OS_SANITIZE,
                                "Sanitize",
                                GURL("chrome://sanitize"),
                                profile) {}

std::unique_ptr<web_app::WebAppInstallInfo>
SanitizeSystemAppDelegate::GetWebAppInfo() const {
  return CreateWebAppInfoForSanitizeSystemWebApp();
}

bool SanitizeSystemAppDelegate::ShouldAllowResize() const {
  return false;
}

bool SanitizeSystemAppDelegate::ShouldShowInLauncher() const {
  return false;
}

gfx::Rect SanitizeSystemAppDelegate::GetDefaultBounds(Browser* browser) const {
  gfx::Rect bounds =
      display::Screen::GetScreen()->GetDisplayForNewWindows().work_area();
  bounds.ClampToCenteredSize({kSanitizeWindowWidth, kSanitizeWindowHeight});
  return bounds;
}

bool SanitizeSystemAppDelegate::ShouldCaptureNavigations() const {
  return true;
}
