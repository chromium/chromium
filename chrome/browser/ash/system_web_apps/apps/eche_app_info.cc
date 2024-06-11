// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/eche_app_info.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/webui/eche_app_ui/url_constants.h"
#include "ash/webui/grit/ash_eche_bundle_resources.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/system_web_apps/apps/system_web_app_install_utils.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/grit/generated_resources.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/screen.h"

namespace {

constexpr float kDefaultAspectRatio = 16.0 / 9.0f;
constexpr gfx::Size kMinimumEcheSize(240, 240);

}  // namespace

std::unique_ptr<web_app::WebAppInstallInfo> CreateWebAppInfoForEcheApp() {
  GURL start_url = GURL(ash::eche_app::kChromeUIEcheAppURL);
  auto info =
      web_app::CreateSystemWebAppInstallInfoWithStartUrlAsIdentity(start_url);
  info->scope = GURL(ash::eche_app::kChromeUIEcheAppURL);
  info->title = l10n_util::GetStringUTF16(IDS_ECHE_APP_NAME);
  web_app::CreateIconInfoForSystemWebApp(
      info->start_url(),
      {{"app_icon_256.png", 256, IDR_ASH_ECHE_APP_ICON_256_PNG}}, *info);
  info->theme_color = 0xFFFFFFFF;
  info->background_color = 0xFFFFFFFF;
  info->display_mode = blink::mojom::DisplayMode::kMinimalUi;
  info->user_display_mode = web_app::mojom::UserDisplayMode::kStandalone;

  return info;
}

EcheSystemAppDelegate::EcheSystemAppDelegate(Profile* profile)
    : ash::SystemWebAppDelegate(ash::SystemWebAppType::ECHE,
                                "Eche",
                                GURL("chrome://eche-app"),
                                profile) {}

std::unique_ptr<web_app::WebAppInstallInfo>
EcheSystemAppDelegate::GetWebAppInfo() const {
  return CreateWebAppInfoForEcheApp();
}
bool EcheSystemAppDelegate::ShouldCaptureNavigations() const {
  return true;
}
bool EcheSystemAppDelegate::ShouldShowInLauncher() const {
  return false;
}
bool EcheSystemAppDelegate::ShouldShowInSearchAndShelf() const {
  return false;
}

bool EcheSystemAppDelegate::ShouldAllowResize() const {
  return false;
}

bool EcheSystemAppDelegate::ShouldAllowMaximize() const {
  return false;
}

bool EcheSystemAppDelegate::ShouldAllowFullscreen() const {
  return false;
}

bool EcheSystemAppDelegate::ShouldHaveReloadButtonInMinimalUi() const {
  return false;
}

bool EcheSystemAppDelegate::ShouldAllowScriptsToCloseWindows() const {
  // For debug purposes, we do not allow closing windows via script under the
  // debug mode.
  return !base::FeatureList::IsEnabled(ash::features::kEcheSWADebugMode);
}

gfx::Rect EcheSystemAppDelegate::GetDefaultBounds(Browser* browser) const {
  return GetDefaultBoundsForEche();
}

bool EcheSystemAppDelegate::IsAppEnabled() const {
  return base::FeatureList::IsEnabled(ash::features::kEcheSWA);
}

// TODO(nayebi): Remove this after migrating completely from SWA to bubble.
gfx::Rect EcheSystemAppDelegate::GetDefaultBoundsForEche() const {
  // Ensures the Eche bounds is always 16:9 portrait aspect ratio and not more
  // than half of the windows.
  gfx::Rect bounds =
      display::Screen::GetScreen()->GetDisplayForNewWindows().work_area();
  const float bounds_aspect_ratio =
      static_cast<float>(bounds.width()) / bounds.height();
  const bool is_landscape = (bounds_aspect_ratio >= 1);
  auto new_width = is_landscape ? (bounds.height() / 2) : bounds.width() / 2;
  if (kMinimumEcheSize.width() > new_width) {
    new_width = kMinimumEcheSize.width();
  }
  bounds.ClampToCenteredSize(
      gfx::Size(new_width, new_width * kDefaultAspectRatio));
  return bounds;
}
