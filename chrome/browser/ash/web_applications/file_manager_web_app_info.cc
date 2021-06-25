// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/file_manager_web_app_info.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/webui/file_manager/resources/grit/file_manager_swa_resources.h"
#include "ash/webui/file_manager/url_constants.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/web_applications/system_web_app_install_utils.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"

std::unique_ptr<WebApplicationInfo> CreateWebAppInfoForFileManager() {
  auto info = std::make_unique<WebApplicationInfo>();
  info->start_url = GURL(ash::file_manager::kChromeUIFileManagerURL);
  info->scope = GURL(ash::file_manager::kChromeUIFileManagerURL);
  // TODO(majewski): Fetch from a resource.
  info->title = u"File Manager";
  web_app::CreateIconInfoForSystemWebApp(
      info->start_url,
      {{"icon192.png", 192, IDR_FILE_MANAGER_SWA_IMAGES_ICON192_PNG}}, *info);
  info->theme_color = 0xFF4285F4;
  info->background_color = 0xFFFFFFFF;
  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  info->open_as_window = true;

  return info;
}

FileManagerSystemAppDelegate::FileManagerSystemAppDelegate(Profile* profile)
    : web_app::SystemWebAppDelegate(web_app::SystemAppType::FILE_MANAGER,
                                    "File Manager",
                                    GURL("chrome://file-manager"),
                                    profile) {}

std::unique_ptr<WebApplicationInfo>
FileManagerSystemAppDelegate::GetWebAppInfo() const {
  return CreateWebAppInfoForFileManager();
}

bool FileManagerSystemAppDelegate::ShouldCaptureNavigations() const {
  return true;
}

bool FileManagerSystemAppDelegate::ShouldBeSingleWindow() const {
  return false;
}

bool FileManagerSystemAppDelegate::IsAppEnabled() const {
  return ash::features::IsFileManagerSwaEnabled();
}

bool FileManagerSystemAppDelegate::ShouldShowNewWindowMenuOption() const {
  return true;
}
