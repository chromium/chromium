// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_system_app_delegate.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/rgb_keyboard/rgb_keyboard_manager.h"
#include "ash/shell.h"
#include "ash/webui/grit/ash_personalization_app_resources.h"
#include "ash/webui/personalization_app/personalization_app_url_constants.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_utils.h"
#include "chrome/browser/ash/system_web_apps/apps/system_web_app_install_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom-shared.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

PersonalizationSystemAppDelegate::PersonalizationSystemAppDelegate(
    Profile* profile)
    : ash::SystemWebAppDelegate(
          ash::SystemWebAppType::PERSONALIZATION,
          "Personalization",
          GURL(ash::personalization_app::kChromeUIPersonalizationAppURL),
          profile) {}

std::unique_ptr<web_app::WebAppInstallInfo>
PersonalizationSystemAppDelegate::GetWebAppInfo() const {
  GURL start_url =
      GURL(ash::personalization_app::kChromeUIPersonalizationAppURL);
  auto info =
      web_app::CreateSystemWebAppInstallInfoWithStartUrlAsIdentity(start_url);
  info->scope = GURL(ash::personalization_app::kChromeUIPersonalizationAppURL);
  info->title = l10n_util::GetStringUTF16(
      IDS_PERSONALIZATION_APP_PERSONALIZATION_HUB_TITLE);

  web_app::CreateIconInfoForSystemWebApp(
      info->start_url(),
      {
          {
              "app_hub_icon_64.png",
              64,
              IDR_ASH_PERSONALIZATION_APP_HUB_ICON_64_PNG,
          },
          {
              "app_hub_icon_128.png",
              128,
              IDR_ASH_PERSONALIZATION_APP_HUB_ICON_128_PNG,
          },
          {
              "app_hub_icon_192.png",
              192,
              IDR_ASH_PERSONALIZATION_APP_HUB_ICON_192_PNG,
          },
          {
              "app_hub_icon_256.png",
              256,
              IDR_ASH_PERSONALIZATION_APP_HUB_ICON_256_PNG,
          },
      },
      *info);

  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  info->user_display_mode = web_app::mojom::UserDisplayMode::kStandalone;

  return info;
}

gfx::Size PersonalizationSystemAppDelegate::GetMinimumWindowSize() const {
  return {600, 420};
}

gfx::Rect PersonalizationSystemAppDelegate::GetDefaultBounds(
    Browser* browser) const {
  gfx::Rect bounds =
      display::Screen::GetScreen()->GetDisplayForNewWindows().work_area();
  if (ash::Shell::Get()->rgb_keyboard_manager()->IsRgbKeyboardSupported()) {
    bounds.ClampToCenteredSize({826, 881});
  } else {
    bounds.ClampToCenteredSize({826, 708});
  }
  return bounds;
}

bool PersonalizationSystemAppDelegate::ShouldCaptureNavigations() const {
  return true;
}

bool PersonalizationSystemAppDelegate::IsAppEnabled() const {
  return ash::personalization_app::CanSeeWallpaperOrPersonalizationApp(
      profile());
}

bool PersonalizationSystemAppDelegate::ShouldShowInLauncher() const {
  return false;
}

bool PersonalizationSystemAppDelegate::ShouldShowInSearchAndShelf() const {
  // Search is implemented by //ash/webui/personalization_app/search.
  return false;
}

bool PersonalizationSystemAppDelegate::ShouldAnimateThemeChanges() const {
  return false;
}
