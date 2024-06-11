// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/projector_system_web_app_info.h"

#include "ash/webui/grit/ash_projector_app_untrusted_resources.h"
#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "chrome/browser/ash/system_web_apps/apps/system_web_app_install_utils.h"
#include "chrome/browser/ui/ash/projector/projector_utils.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/grit/generated_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"

ProjectorSystemWebAppDelegate::ProjectorSystemWebAppDelegate(Profile* profile)
    : ash::SystemWebAppDelegate(ash::SystemWebAppType::PROJECTOR,
                                "Projector",
                                GURL(ash::kChromeUIUntrustedProjectorUrl),
                                profile) {}

ProjectorSystemWebAppDelegate::~ProjectorSystemWebAppDelegate() = default;

std::unique_ptr<web_app::WebAppInstallInfo>
ProjectorSystemWebAppDelegate::GetWebAppInfo() const {
  GURL start_url(ash::kChromeUIUntrustedProjectorUrl);
  auto info =
      web_app::CreateSystemWebAppInstallInfoWithStartUrlAsIdentity(start_url);
  info->scope = GURL(ash::kChromeUIUntrustedProjectorUrl);

  info->title = l10n_util::GetStringUTF16(IDS_PROJECTOR_APP_NAME);

  web_app::CreateIconInfoForSystemWebApp(
      info->start_url(),
      {
          {"app_icon_16.png", 16,
           IDR_ASH_PROJECTOR_APP_UNTRUSTED_ASSETS_ICON_16_PNG},
          {"app_icon_32.png", 32,
           IDR_ASH_PROJECTOR_APP_UNTRUSTED_ASSETS_ICON_32_PNG},
          {"app_icon_48.png", 48,
           IDR_ASH_PROJECTOR_APP_UNTRUSTED_ASSETS_ICON_48_PNG},
          {"app_icon_64.png", 64,
           IDR_ASH_PROJECTOR_APP_UNTRUSTED_ASSETS_ICON_64_PNG},
          {"app_icon_96.png", 96,
           IDR_ASH_PROJECTOR_APP_UNTRUSTED_ASSETS_ICON_96_PNG},
          {"app_icon_128.png", 128,
           IDR_ASH_PROJECTOR_APP_UNTRUSTED_ASSETS_ICON_128_PNG},
          {"app_icon_192.png", 192,
           IDR_ASH_PROJECTOR_APP_UNTRUSTED_ASSETS_ICON_192_PNG},
          {"app_icon_256.png", 256,
           IDR_ASH_PROJECTOR_APP_UNTRUSTED_ASSETS_ICON_256_PNG},
      },
      *info);

  info->theme_color =
      web_app::GetDefaultBackgroundColor(/*use_dark_mode=*/false);
  info->dark_mode_theme_color =
      web_app::GetDefaultBackgroundColor(/*use_dark_mode=*/true);
  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  info->user_display_mode = web_app::mojom::UserDisplayMode::kStandalone;

  return info;
}

bool ProjectorSystemWebAppDelegate::ShouldCaptureNavigations() const {
  return true;
}

gfx::Size ProjectorSystemWebAppDelegate::GetMinimumWindowSize() const {
  // The minimum width matches the minimum width of the Projector viewer left
  // panel defined in the web component.
  return {222, 550};
}

bool ProjectorSystemWebAppDelegate::IsAppEnabled() const {
  return IsProjectorAppEnabled(profile_);
}
