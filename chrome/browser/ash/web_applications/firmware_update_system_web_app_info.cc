// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/firmware_update_system_web_app_info.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/grit/ash_firmware_update_app_resources.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/webui/firmware_update_ui/url_constants.h"
#include "chrome/browser/ash/web_applications/system_web_app_install_utils.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_types.h"
#include "chrome/browser/web_applications/web_application_info.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"

// TODO(michaelcheco): Update to correct icon.
std::unique_ptr<WebApplicationInfo>
CreateWebAppInfoForFirmwareUpdateSystemWebApp() {
  auto info = std::make_unique<WebApplicationInfo>();
  info->start_url = GURL(ash::kChromeUIFirmwareUpdateAppURL);
  info->scope = GURL(ash::kChromeUIFirmwareUpdateAppURL);
  info->title = l10n_util::GetStringUTF16(IDS_ASH_FIRMWARE_UPDATE_APP_TITLE);
  web_app::CreateIconInfoForSystemWebApp(
      info->start_url,
      {{"app_icon_192.png", 192, IDR_ASH_FIRMWARE_UPDATE_APP_APP_ICON_192_PNG}},
      *info);
  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  info->user_display_mode = blink::mojom::DisplayMode::kStandalone;

  return info;
}

FirmwareUpdateSystemAppDelegate::FirmwareUpdateSystemAppDelegate(
    Profile* profile)
    : web_app::SystemWebAppDelegate(web_app::SystemAppType::FIRMWARE_UPDATE,
                                    "FirmwareUpdate",
                                    GURL(ash::kChromeUIFirmwareUpdateAppURL),
                                    profile) {}

std::unique_ptr<WebApplicationInfo>
FirmwareUpdateSystemAppDelegate::GetWebAppInfo() const {
  return CreateWebAppInfoForFirmwareUpdateSystemWebApp();
}

bool FirmwareUpdateSystemAppDelegate::IsAppEnabled() const {
  return ash::features::IsFirmwareUpdaterAppEnabled();
}

gfx::Size FirmwareUpdateSystemAppDelegate::GetMinimumWindowSize() const {
  return {600, 512};
}
