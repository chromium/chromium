// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/vc_background_ui/vc_background_ui_system_app_delegate.h"

#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "ash/webui/vc_background_ui/url_constants.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom-shared.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/manta/features.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-shared.h"
#include "url/gurl.h"

namespace ash::vc_background_ui {

VcBackgroundUISystemAppDelegate::VcBackgroundUISystemAppDelegate(
    Profile* profile)
    : SystemWebAppDelegate(
          SystemWebAppType::VC_BACKGROUND,
          "VcBackground",
          GURL(ash::vc_background_ui::kChromeUIVcBackgroundURL),
          profile) {}

std::unique_ptr<web_app::WebAppInstallInfo>
VcBackgroundUISystemAppDelegate::GetWebAppInfo() const {
  std::unique_ptr<web_app::WebAppInstallInfo> info =
      std::make_unique<web_app::WebAppInstallInfo>();
  info->start_url = GURL(kChromeUIVcBackgroundURL);
  info->scope = GURL(kChromeUIVcBackgroundURL);
  // TODO(b/311416410) real title and icon.
  info->title = u"VC Background";
  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  info->user_display_mode = web_app::mojom::UserDisplayMode::kStandalone;
  return info;
}

bool VcBackgroundUISystemAppDelegate::IsAppEnabled() const {
  return ash::features::IsSeaPenEnabled() &&
         manta::features::IsMantaServiceEnabled();
}

bool VcBackgroundUISystemAppDelegate::ShouldShowInLauncher() const {
  return false;
}

bool VcBackgroundUISystemAppDelegate::ShouldShowInSearch() const {
  return false;
}

}  // namespace ash::vc_background_ui
