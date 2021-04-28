// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/demo_mode_web_app_info.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/web_applications/system_web_app_install_utils.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chromeos/components/demo_mode_app_ui/url_constants.h"
#include "chromeos/grit/chromeos_demo_mode_app_resources.h"

std::unique_ptr<WebApplicationInfo> CreateWebAppInfoForDemoModeApp() {
  std::unique_ptr<WebApplicationInfo> info =
      std::make_unique<WebApplicationInfo>();
  info->start_url = GURL(chromeos::kChromeUIDemoModeAppURL);
  info->scope = GURL(chromeos::kChromeUIDemoModeAppURL);
  // TODO(b/185608502): Convert the title to a localized string
  info->title = u"Demo Mode App";
  web_app::CreateIconInfoForSystemWebApp(
      info->start_url,
      {{"app_icon_192.png", 192, IDR_CHROMEOS_DEMO_MODE_APP_APP_ICON_192_PNG}},
      *info);
  info->theme_color = 0xFF4285F4;
  info->background_color = 0xFFFFFFFF;
  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  info->open_as_window = true;

  return info;
}
