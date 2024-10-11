// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/mall_system_web_app_info.h"

#include "ash/constants/web_app_id_constants.h"
#include "ash/webui/grit/ash_mall_cros_app_resources.h"
#include "ash/webui/mall/url_constants.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "base/feature_list.h"
#include "chrome/browser/apps/user_type_filter.h"
#include "chrome/browser/ash/system_web_apps/apps/system_web_app_install_utils.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_delegate.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/webapps/common/web_app_id.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
constexpr char kMallInternalName[] = "Mall";
}

MallSystemAppDelegate::MallSystemAppDelegate(Profile* profile)
    : ash::SystemWebAppDelegate(ash::SystemWebAppType::MALL,
                                kMallInternalName,
                                GURL(ash::kChromeUIMallUrl),
                                profile) {}

std::unique_ptr<web_app::WebAppInstallInfo>
MallSystemAppDelegate::GetWebAppInfo() const {
  const GURL url = GURL(ash::kChromeUIMallUrl);
  // `manifest_id` must remain fixed even if start_url changes.
  webapps::ManifestId manifest_id =
      web_app::GenerateManifestIdFromStartUrlOnly(url);
  std::unique_ptr<web_app::WebAppInstallInfo> info =
      std::make_unique<web_app::WebAppInstallInfo>(manifest_id, url);
  info->scope = url;
  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  info->user_display_mode = web_app::mojom::UserDisplayMode::kStandalone;
  info->title = l10n_util::GetStringUTF16(IDS_MALL_APP_NAME);

  web_app::CreateIconInfoForSystemWebApp(
      info->start_url(),
      {{"mall_icon_192.png", 192,
        IDR_ASH_MALL_CROS_APP_IMAGES_MALL_ICON_192_PNG}},
      *info);

  return info;
}

bool MallSystemAppDelegate::IsAppEnabled() const {
  if (apps::DetermineUserType(profile()) != apps::kUserTypeUnmanaged) {
    return false;
  }
  return chromeos::features::IsCrosMallSwaEnabled();
}

std::vector<std::string> MallSystemAppDelegate::GetAppIdsToUninstallAndReplace()
    const {
  // Attempt to migrate preferences from Mall preloaded web app. Note that
  // synchronizing of preloaded PWAs and SWAs race against each other. The
  // migration will only happen if the SWA installs before
  // PreinstalledWebAppManager attempts to uninstall the Mall PWA. If migration
  // does not occur, shelf and launcher will reset to default positions for the
  // Mall app. Fixing this requires teaching ExternallyManagedAppManager how
  // to ignore apps, the buggy behaviour is considered an acceptable tradeoff.
  return {web_app::kMallAppId};
}

bool MallSystemAppDelegate::ShouldCaptureNavigations() const {
  return true;
}
