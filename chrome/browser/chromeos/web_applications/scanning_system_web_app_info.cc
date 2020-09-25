// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/web_applications/scanning_system_web_app_info.h"

#include <memory>

#include "chrome/browser/chromeos/web_applications/system_web_app_install_utils.h"
#include "chrome/common/web_application_info.h"
#include "chromeos/components/scanning/url_constants.h"
#include "chromeos/grit/chromeos_scanning_app_resources.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

std::unique_ptr<WebApplicationInfo> CreateWebAppInfoForScanningSystemWebApp() {
  std::unique_ptr<WebApplicationInfo> info =
      std::make_unique<WebApplicationInfo>();
  info->start_url = GURL(chromeos::kChromeUIScanningAppUrl);
  info->scope = GURL(chromeos::kChromeUIScanningAppUrl);
  info->title = l10n_util::GetStringUTF16(IDS_SCANNING_APP_TITLE);
  web_app::CreateIconInfoForSystemWebApp(
      info->start_url, {{"app_icon_192.png", 192, IDR_SCANNING_APP_ICON}},
      *info);
  info->theme_color = 0xFFFFFFFF;
  info->background_color = 0xFFFFFFFF;
  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  info->open_as_window = true;

  return info;
}
