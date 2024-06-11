// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/shimless_rma_system_web_app_info.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/webui/grit/ash_shimless_rma_resources.h"
#include "ash/webui/shimless_rma/url_constants.h"
#include "chrome/browser/ash/system_web_apps/apps/system_web_app_install_utils.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "ui/base/l10n/l10n_util.h"

std::unique_ptr<web_app::WebAppInstallInfo>
CreateWebAppInfoForShimlessRMASystemWebApp() {
  GURL start_url(ash::kChromeUIShimlessRMAUrl);
  auto info =
      web_app::CreateSystemWebAppInstallInfoWithStartUrlAsIdentity(start_url);
  info->scope = GURL(ash::kChromeUIShimlessRMAUrl);
  info->title = l10n_util::GetStringUTF16(IDS_ASH_SHIMLESS_RMA_APP_TITLE);
  web_app::CreateIconInfoForSystemWebApp(
      info->start_url(),
      {{"app_icon_192.png", 192, IDR_ASH_SHIMLESS_RMA_APP_ICON_192_PNG}},
      *info);
  info->theme_color = 0xFFFFFFFF;
  info->background_color = 0xFFFFFFFF;
  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  info->user_display_mode = web_app::mojom::UserDisplayMode::kStandalone;

  return info;
}

ShimlessRMASystemAppDelegate::ShimlessRMASystemAppDelegate(Profile* profile)
    : ash::SystemWebAppDelegate(ash::SystemWebAppType::SHIMLESS_RMA,
                                "ShimlessRMA",
                                GURL(ash::kChromeUIShimlessRMAUrl),
                                profile) {}

std::unique_ptr<web_app::WebAppInstallInfo>
ShimlessRMASystemAppDelegate::GetWebAppInfo() const {
  return CreateWebAppInfoForShimlessRMASystemWebApp();
}

bool ShimlessRMASystemAppDelegate::ShouldCaptureNavigations() const {
  return true;
}

bool ShimlessRMASystemAppDelegate::ShouldShowInLauncher() const {
  return false;
}

bool ShimlessRMASystemAppDelegate::ShouldShowInSearchAndShelf() const {
  return false;
}

bool ShimlessRMASystemAppDelegate::ShouldAllowResize() const {
  return false;
}

bool ShimlessRMASystemAppDelegate::ShouldAllowScriptsToCloseWindows() const {
  return true;
}

bool ShimlessRMASystemAppDelegate::IsAppEnabled() const {
  return true;
}
