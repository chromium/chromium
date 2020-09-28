// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/web_applications/help_app_web_app_info.h"

#include <memory>

#include "chrome/browser/chromeos/web_applications/system_web_app_install_utils.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/common/web_application_info.h"
#include "chromeos/components/help_app_ui/url_constants.h"
#include "chromeos/grit/chromeos_help_app_resources.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"

std::unique_ptr<WebApplicationInfo> CreateWebAppInfoForHelpWebApp() {
  std::unique_ptr<WebApplicationInfo> info =
      std::make_unique<WebApplicationInfo>();
  info->start_url = GURL(chromeos::kChromeUIHelpAppURL);
  info->scope = GURL(chromeos::kChromeUIHelpAppURL);

  info->title = l10n_util::GetStringUTF16(IDS_HELP_APP_EXPLORE);
  web_app::CreateIconInfoForSystemWebApp(
      info->start_url,
      {
          {"app_icon_192.png", 192, IDR_HELP_APP_ICON_192},
          {"app_icon_512.png", 512, IDR_HELP_APP_ICON_512},

      },
      *info);
  info->theme_color = 0xffffffff;
  info->background_color = 0xffffffff;
  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  info->open_as_window = true;
  return info;
}
