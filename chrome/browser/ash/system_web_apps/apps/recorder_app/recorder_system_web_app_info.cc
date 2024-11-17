// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/recorder_app/recorder_system_web_app_info.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/webui/recorder_app_ui/resources/grit/recorder_app_resources.h"
#include "ash/webui/recorder_app_ui/url_constants.h"
#include "chrome/browser/ash/system_web_apps/apps/system_web_app_install_utils.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/soda/soda_features.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
// The height of title bar on ChromeOS.
constexpr int kChromeRecorderAppTitlebarHeight = 32;
}  // namespace

std::unique_ptr<web_app::WebAppInstallInfo>
CreateWebAppInfoForRecorderSystemWebApp() {
  GURL start_url(ash::kChromeUIRecorderAppURL);
  auto info =
      web_app::CreateSystemWebAppInstallInfoWithStartUrlAsIdentity(start_url);
  info->scope = GURL(ash::kChromeUIRecorderAppURL);

  info->title = l10n_util::GetStringUTF16(IDS_RECORDER_APP_NAME);
  web_app::CreateIconInfoForSystemWebApp(
      info->start_url(),
      {
          {"app_icons_256.png", 256, IDR_STATIC_STATIC_IMAGES_ICON_PNG},
      },
      *info);
  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  info->user_display_mode = web_app::mojom::UserDisplayMode::kStandalone;
  return info;
}

RecorderSystemAppDelegate::RecorderSystemAppDelegate(Profile* profile)
    : ash::SystemWebAppDelegate(ash::SystemWebAppType::RECORDER,
                                "Recorder",
                                GURL(ash::kChromeUIRecorderAppURL),
                                profile) {}

std::unique_ptr<web_app::WebAppInstallInfo>
RecorderSystemAppDelegate::GetWebAppInfo() const {
  return CreateWebAppInfoForRecorderSystemWebApp();
}

bool RecorderSystemAppDelegate::ShouldCaptureNavigations() const {
  return true;
}

gfx::Size RecorderSystemAppDelegate::GetMinimumWindowSize() const {
  // Window size includes title bar height, and the minimal content size is
  // 480x600.
  return {480, 600 + kChromeRecorderAppTitlebarHeight};
}

bool RecorderSystemAppDelegate::IsAppEnabled() const {
  // TODO(b/369262781): Conch flag is currently default disabled and we only
  // enabled recorder app on device where large SODA model is available due to
  // lack of sufficient testing on other devices.
  return base::FeatureList::IsEnabled(ash::features::kConch) ||
         base::FeatureList::IsEnabled(
             speech::kFeatureManagementCrosSodaConchLanguages);
}
