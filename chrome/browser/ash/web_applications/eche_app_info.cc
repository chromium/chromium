// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/eche_app_info.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/grit/ash_eche_bundle_resources.h"
#include "ash/webui/eche_app_ui/url_constants.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/web_applications/system_web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_application_info.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "ui/display/screen.h"

std::unique_ptr<WebApplicationInfo> CreateWebAppInfoForEcheApp() {
  std::unique_ptr<WebApplicationInfo> info =
      std::make_unique<WebApplicationInfo>();
  info->start_url = GURL(ash::eche_app::kChromeUIEcheAppURL);
  info->scope = GURL(ash::eche_app::kChromeUIEcheAppURL);
  // |title| should come from a resource string, but this is the Eche app, and
  // doesn't have one.
  info->title = u"Eche App";
  web_app::CreateIconInfoForSystemWebApp(
      info->start_url,
      {{"app_icon_256.png", 256, IDR_ASH_ECHE_APP_ICON_256_PNG}}, *info);
  info->theme_color = 0xFFFFFFFF;
  info->background_color = 0xFFFFFFFF;
  info->display_mode = blink::mojom::DisplayMode::kMinimalUi;
  info->user_display_mode = blink::mojom::DisplayMode::kStandalone;

  return info;
}

gfx::Rect GetDefaultBoundsForEche(Browser*) {
  // Ensures the Eche bounds is always 16:9 portrait aspect ratio and not more
  // than half of the windows.
  const float aspect_ratio = 16.0f / 9.0f;
  const gfx::Size min_size(240, 240);

  gfx::Rect bounds =
      display::Screen::GetScreen()->GetDisplayForNewWindows().work_area();
  const float bounds_aspect_ratio = bounds.width() / bounds.height();
  const bool is_landscape = (bounds_aspect_ratio >= 1);
  auto new_width = is_landscape ? (bounds.height() / 2) : bounds.width() / 2;
  if (min_size.width() > new_width) {
    new_width = min_size.width();
  }
  bounds.ClampToCenteredSize(gfx::Size(new_width, new_width * aspect_ratio));
  return bounds;
}

EcheSystemAppDelegate::EcheSystemAppDelegate(Profile* profile)
    : web_app::SystemWebAppDelegate(web_app::SystemAppType::ECHE,
                                    "Eche",
                                    GURL("chrome://eche-app"),
                                    profile) {}

std::unique_ptr<WebApplicationInfo> EcheSystemAppDelegate::GetWebAppInfo()
    const {
  return CreateWebAppInfoForEcheApp();
}
bool EcheSystemAppDelegate::ShouldCaptureNavigations() const {
  return true;
}
bool EcheSystemAppDelegate::ShouldShowInLauncher() const {
  return false;
}
bool EcheSystemAppDelegate::ShouldShowInSearch() const {
  return false;
}

bool EcheSystemAppDelegate::ShouldAllowResize() const {
  return base::FeatureList::IsEnabled(chromeos::features::kEcheSWAResizing);
}

bool EcheSystemAppDelegate::ShouldAllowMaximize() const {
  return false;
}

bool EcheSystemAppDelegate::ShouldHaveReloadButtonInMinimalUi() const {
  return false;
}

bool EcheSystemAppDelegate::ShouldAllowScriptsToCloseWindows() const {
  // For debug purposes, we do not allow closing windows via script under the
  // debug mode.
  return !base::FeatureList::IsEnabled(chromeos::features::kEcheSWADebugMode);
}

gfx::Rect EcheSystemAppDelegate::GetDefaultBounds(Browser* browser) const {
  return GetDefaultBoundsForEche(browser);
}

bool EcheSystemAppDelegate::IsAppEnabled() const {
  return base::FeatureList::IsEnabled(chromeos::features::kEcheSWA);
}
