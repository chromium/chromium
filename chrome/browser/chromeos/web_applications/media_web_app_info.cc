// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/web_applications/media_web_app_info.h"

#include <memory>

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/web_applications/system_web_app_install_utils.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/common/web_application_info.h"
#include "chromeos/components/media_app_ui/url_constants.h"
#include "chromeos/grit/chromeos_media_app_resources.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"

std::unique_ptr<WebApplicationInfo> CreateWebAppInfoForMediaWebApp() {
  std::unique_ptr<WebApplicationInfo> info =
      std::make_unique<WebApplicationInfo>();
  info->start_url = GURL(chromeos::kChromeUIMediaAppURL);
  info->scope = GURL(chromeos::kChromeUIMediaAppURL);

  info->title = l10n_util::GetStringUTF16(IDS_MEDIA_APP_APP_NAME);
  web_app::CreateIconInfoForSystemWebApp(
      info->start_url,
      {
          {"app_icon_16.png", 16, IDR_MEDIA_APP_GALLERY_ICON_16_PNG},
          {"app_icon_32.png", 32, IDR_MEDIA_APP_GALLERY_ICON_32_PNG},
          {"app_icon_48.png", 48, IDR_MEDIA_APP_GALLERY_ICON_48_PNG},
          {"app_icon_64.png", 64, IDR_MEDIA_APP_GALLERY_ICON_64_PNG},
          {"app_icon_128.png", 128, IDR_MEDIA_APP_GALLERY_ICON_128_PNG},
          {"app_icon_192.png", 192, IDR_MEDIA_APP_GALLERY_ICON_192_PNG},
          {"app_icon_256.png", 256, IDR_MEDIA_APP_GALLERY_ICON_256_PNG},

      },
      *info);
  info->theme_color = 0xff202124;
  info->background_color = 0xff3c4043;
  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  info->open_as_window = true;

  blink::Manifest::FileHandler file_handler;
  file_handler.action = GURL(chromeos::kChromeUIMediaAppURL);
  file_handler.name = base::UTF8ToUTF16("Media File");
  file_handler.accept[base::UTF8ToUTF16("image/*")] = {};
  file_handler.accept[base::UTF8ToUTF16("video/*")] = {};
  info->file_handlers.push_back(file_handler);
  return info;
}
