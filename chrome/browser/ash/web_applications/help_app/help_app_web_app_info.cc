// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/help_app/help_app_web_app_info.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/webui/grit/ash_help_app_resources.h"
#include "ash/webui/help_app_ui/url_constants.h"
#include "chrome/browser/ash/web_applications/system_web_app_install_utils.h"
#include "chrome/browser/web_applications/user_display_mode.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/styles/cros_styles.h"
#include "ui/display/screen.h"

namespace ash {

namespace {

constexpr gfx::Size HELP_DEFAULT_SIZE(960, 600);

}  // namespace

std::unique_ptr<WebAppInstallInfo> CreateWebAppInfoForHelpWebApp() {
  std::unique_ptr<WebAppInstallInfo> info =
      std::make_unique<WebAppInstallInfo>();
  info->start_url = GURL(kChromeUIHelpAppURL);
  info->scope = GURL(kChromeUIHelpAppURL);

  info->title = l10n_util::GetStringUTF16(IDS_HELP_APP_EXPLORE);
  web_app::CreateIconInfoForSystemWebApp(
      info->start_url,
      {
          {"app_icon_192.png", 192, IDR_HELP_APP_ICON_192},
          {"app_icon_512.png", 512, IDR_HELP_APP_ICON_512},

      },
      *info);

  if (chromeos::features::IsDarkLightModeEnabled()) {
    info->theme_color = cros_styles::ResolveColor(
        cros_styles::ColorName::kBgColor, /*is_dark_mode=*/false);
    info->dark_mode_theme_color = cros_styles::ResolveColor(
        cros_styles::ColorName::kBgColor, /*is_dark_mode=*/true);
    info->background_color = info->theme_color;
    info->dark_mode_background_color = info->dark_mode_theme_color;
  } else {
    info->theme_color = 0xffffffff;
    info->background_color = 0xffffffff;
  }

  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  info->user_display_mode = web_app::UserDisplayMode::kStandalone;
  return info;
}

gfx::Rect GetDefaultBoundsForHelpApp(Browser*) {
  // Help app is centered.
  gfx::Rect bounds =
      display::Screen::GetScreen()->GetDisplayForNewWindows().work_area();
  bounds.ClampToCenteredSize(HELP_DEFAULT_SIZE);
  return bounds;
}

HelpAppSystemAppDelegate::HelpAppSystemAppDelegate(Profile* profile)
    : SystemWebAppDelegate(SystemWebAppType::HELP,
                           "Help",
                           GURL("chrome://help-app/pwa.html"),
                           profile) {}

gfx::Rect HelpAppSystemAppDelegate::GetDefaultBounds(Browser* browser) const {
  return GetDefaultBoundsForHelpApp(browser);
}

bool HelpAppSystemAppDelegate::ShouldCaptureNavigations() const {
  return true;
}

gfx::Size HelpAppSystemAppDelegate::GetMinimumWindowSize() const {
  return {600, 320};
}

std::vector<int> HelpAppSystemAppDelegate::GetAdditionalSearchTerms() const {
  return {IDS_GENIUS_APP_NAME, IDS_HELP_APP_PERKS, IDS_HELP_APP_OFFERS};
}

absl::optional<SystemWebAppBackgroundTaskInfo>
HelpAppSystemAppDelegate::GetTimerInfo() const {
  if (base::FeatureList::IsEnabled(features::kHelpAppBackgroundPage)) {
    return SystemWebAppBackgroundTaskInfo(absl::nullopt,
                                          GURL("chrome://help-app/background"),
                                          /*open_immediately=*/true);
  } else {
    return absl::nullopt;
  }
}

std::unique_ptr<WebAppInstallInfo> HelpAppSystemAppDelegate::GetWebAppInfo()
    const {
  return CreateWebAppInfoForHelpWebApp();
}

}  // namespace ash
