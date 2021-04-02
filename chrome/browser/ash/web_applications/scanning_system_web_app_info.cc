// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/scanning_system_web_app_info.h"

#include <memory>

#include "ash/content/scanning/url_constants.h"
#include "ash/grit/ash_scanning_app_resources.h"
#include "chrome/browser/ash/web_applications/system_web_app_install_utils.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

std::unique_ptr<WebApplicationInfo> CreateWebAppInfoForScanningSystemWebApp() {
  std::unique_ptr<WebApplicationInfo> info =
      std::make_unique<WebApplicationInfo>();
  info->start_url = GURL(ash::kChromeUIScanningAppUrl);
  info->scope = GURL(ash::kChromeUIScanningAppUrl);
  info->title = l10n_util::GetStringUTF16(IDS_SCANNING_APP_TITLE);
  web_app::CreateIconInfoForSystemWebApp(
      info->start_url,
      {
          {"scanning_app_icon_16.png", 16, IDR_SCANNING_APP_ICON_16},
          {"scanning_app_icon_32.png", 32, IDR_SCANNING_APP_ICON_32},
          {"scanning_app_icon_48.png", 48, IDR_SCANNING_APP_ICON_48},
          {"scanning_app_icon_64.png", 64, IDR_SCANNING_APP_ICON_64},
          {"scanning_app_icon_128.png", 128, IDR_SCANNING_APP_ICON_128},
          {"scanning_app_icon_192.png", 192, IDR_SCANNING_APP_ICON_192},
          {"scanning_app_icon_256.png", 256, IDR_SCANNING_APP_ICON_256},
      },
      *info);
  info->theme_color = 0xFFFFFFFF;
  info->background_color = 0xFFFFFFFF;
  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  info->open_as_window = true;

  return info;
}
