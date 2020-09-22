// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/web_applications/camera_system_web_app_info.h"

#include "chrome/browser/chromeos/web_applications/system_web_app_install_utils.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/common/web_application_info.h"
#include "chromeos/components/camera_app_ui/resources.h"
#include "chromeos/components/camera_app_ui/url_constants.h"
#include "ui/base/l10n/l10n_util.h"

std::unique_ptr<WebApplicationInfo> CreateWebAppInfoForCameraSystemWebApp() {
  auto info = std::make_unique<WebApplicationInfo>();
  info->start_url = GURL(chromeos::kChromeUICameraAppMainURL);
  info->scope = GURL(chromeos::kChromeUICameraAppURL);

  info->title = l10n_util::GetStringUTF16(IDS_NAME);
  web_app::CreateIconInfoForSystemWebApp(
      info->start_url,
      {
          {"camera_app_icons_48.png", 48, IDR_CAMERA_CAMERA_APP_ICONS_48_PNG},
          {"camera_app_icons_128.png", 128,
           IDR_CAMERA_CAMERA_APP_ICONS_128_PNG},
          {"camera_app_icons_192.png", 192,
           IDR_CAMERA_CAMERA_APP_ICONS_192_PNG},
      },
      *info);
  info->theme_color = 0xff000000;
  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  info->open_as_window = true;
  return info;
}
