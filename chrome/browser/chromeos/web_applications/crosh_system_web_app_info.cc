// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/web_applications/crosh_system_web_app_info.h"

#include <memory>

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/web_applications/system_web_app_install_utils.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "url/gurl.h"

std::unique_ptr<WebApplicationInfo> CreateWebAppInfoForCroshSystemWebApp() {
  auto info = std::make_unique<WebApplicationInfo>();
  info->start_url = GURL(chrome::kChromeUIUntrustedCroshURL);
  info->scope = GURL(chrome::kChromeUIUntrustedCroshURL);
  info->title = base::string16(base::ASCIIToUTF16("crosh"));
  web_app::CreateIconInfoForSystemWebApp(
      info->start_url, {{"app_icon_256.png", 256, IDR_LOGO_CROSTINI_TERMINAL}},
      *info);
  info->background_color = 0xFF202124;
  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  return info;
}
