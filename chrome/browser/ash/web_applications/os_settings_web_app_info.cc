// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/os_settings_web_app_info.h"

#include <memory>

#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "chrome/browser/ash/web_applications/system_web_app_install_utils.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/os_settings_resources.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"

std::unique_ptr<WebApplicationInfo>
CreateWebAppInfoForOSSettingsSystemWebApp() {
  std::unique_ptr<WebApplicationInfo> info =
      std::make_unique<WebApplicationInfo>();
  info->start_url = GURL(chrome::kChromeUIOSSettingsURL);
  info->scope = GURL(chrome::kChromeUIOSSettingsURL);
  info->title = l10n_util::GetStringUTF16(IDS_SETTINGS_SETTINGS);
  web_app::CreateIconInfoForSystemWebApp(
      info->start_url,
      {
          {"icon-192.png", 192, IDR_SETTINGS_LOGO_192},

      },
      *info);
  info->theme_color = 0xffffffff;
  info->background_color = 0xffffffff;
  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  info->open_as_window = true;
  return info;
}
