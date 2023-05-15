// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/face_ml_system_web_app_info.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/webui/face_ml_app_ui/url_constants.h"
#include "ash/webui/grit/ash_face_ml_app_resources.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/ash/web_applications/system_web_app_install_utils.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "ui/chromeos/styles/cros_styles.h"
#include "ui/display/screen.h"

namespace {
constexpr gfx::Size DEFAULT_SIZE(800, 600);
}  // namespace

std::unique_ptr<WebAppInstallInfo> CreateWebAppInfoForFaceMLApp() {
  std::unique_ptr<WebAppInstallInfo> info =
      std::make_unique<WebAppInstallInfo>();
  info->start_url = GURL(ash::kChromeUIFaceMLAppURL);
  info->scope = GURL(ash::kChromeUIFaceMLAppURL);
  // TODO(b/239374316): Convert the title to a localized string
  info->title = u"Face ML";
  web_app::CreateIconInfoForSystemWebApp(
      info->start_url,
      {
          {"app_icon_192.png", 192, IDR_ASH_FACE_ML_APP_APP_ICON_192_PNG},
          {"app_icon_512.png", 512, IDR_ASH_FACE_ML_APP_APP_ICON_512_PNG},
      },
      *info);

  info->theme_color = cros_styles::ResolveColor(
      cros_styles::ColorName::kBgColor, /*is_dark_mode=*/false);
  info->dark_mode_theme_color =
      cros_styles::ResolveColor(cros_styles::ColorName::kBgColor,
                                /*is_dark_mode=*/true);
  info->background_color = info->theme_color;
  info->dark_mode_background_color = info->dark_mode_theme_color;

  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  info->user_display_mode = web_app::mojom::UserDisplayMode::kStandalone;
  return info;
}

FaceMLSystemAppDelegate::FaceMLSystemAppDelegate(Profile* profile)
    : ash::SystemWebAppDelegate(ash::SystemWebAppType::FACE_ML,
                                "FaceML",
                                GURL(ash::kChromeUIFaceMLAppURL),
                                profile) {}

std::unique_ptr<WebAppInstallInfo> FaceMLSystemAppDelegate::GetWebAppInfo()
    const {
  return CreateWebAppInfoForFaceMLApp();
}

gfx::Rect FaceMLSystemAppDelegate::GetDefaultBounds(Browser* browser) const {
  gfx::Rect bounds =
      display::Screen::GetScreen()->GetDisplayForNewWindows().work_area();
  bounds.ClampToCenteredSize(DEFAULT_SIZE);
  return bounds;
}

bool FaceMLSystemAppDelegate::IsAppEnabled() const {
  return base::FeatureList::IsEnabled(ash::features::kFaceMLApp);
}

bool FaceMLSystemAppDelegate::ShouldCaptureNavigations() const {
  return true;
}

bool FaceMLSystemAppDelegate::ShouldShowNewWindowMenuOption() const {
  return false;
}
