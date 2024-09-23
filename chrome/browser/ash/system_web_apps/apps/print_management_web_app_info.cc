// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/print_management_web_app_info.h"

#include <memory>

#include "ash/webui/grit/ash_print_management_resources.h"
#include "ash/webui/print_management/url_constants.h"
#include "chrome/browser/ash/system_web_apps/apps/system_web_app_install_utils.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

std::unique_ptr<web_app::WebAppInstallInfo>
CreateWebAppInfoForPrintManagementApp() {
  GURL start_url = GURL(ash::kChromeUIPrintManagementAppUrl);
  auto info =
      web_app::CreateSystemWebAppInstallInfoWithStartUrlAsIdentity(start_url);
  info->scope = GURL(ash::kChromeUIPrintManagementAppUrl);
  info->title = l10n_util::GetStringUTF16(IDS_PRINT_MANAGEMENT_TITLE);
  web_app::CreateIconInfoForSystemWebApp(
      info->start_url(),
      {{"print_management_192.png", 192,
        IDR_ASH_PRINT_MANAGEMENT_PRINT_MANAGEMENT_192_PNG}},
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

PrintManagementSystemAppDelegate::PrintManagementSystemAppDelegate(
    Profile* profile)
    : ash::SystemWebAppDelegate(ash::SystemWebAppType::PRINT_MANAGEMENT,
                                "PrintManagement",
                                GURL("chrome://print-management/pwa.html"),
                                profile) {}

std::unique_ptr<web_app::WebAppInstallInfo>
PrintManagementSystemAppDelegate::GetWebAppInfo() const {
  return CreateWebAppInfoForPrintManagementApp();
}

bool PrintManagementSystemAppDelegate::ShouldShowInLauncher() const {
  return true;
}
gfx::Size PrintManagementSystemAppDelegate::GetMinimumWindowSize() const {
  return {600, 320};
}
