// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/print_management_web_app_info.h"

#include <memory>

#include "chrome/browser/ash/web_applications/system_web_app_install_utils.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chromeos/components/print_management/url_constants.h"
#include "chromeos/grit/chromeos_print_management_resources.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

std::unique_ptr<WebApplicationInfo> CreateWebAppInfoForPrintManagementApp() {
  std::unique_ptr<WebApplicationInfo> info =
      std::make_unique<WebApplicationInfo>();
  info->start_url = GURL(chromeos::kChromeUIPrintManagementAppUrl);
  info->scope = GURL(chromeos::kChromeUIPrintManagementAppUrl);
  info->title = l10n_util::GetStringUTF16(IDS_PRINT_MANAGEMENT_TITLE);
  web_app::CreateIconInfoForSystemWebApp(
      info->start_url,
      {{"print_management_192.png", 192, IDR_PRINT_MANAGEMENT_ICON}}, *info);
  info->theme_color = 0xFFFFFFFF;
  info->background_color = 0xFFFFFFFF;
  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  info->open_as_window = true;

  return info;
}
