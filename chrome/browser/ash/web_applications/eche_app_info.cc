// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/eche_app_info.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/web_applications/system_web_app_install_utils.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chromeos/components/eche_app_ui/url_constants.h"
#include "chromeos/grit/chromeos_eche_bundle_resources.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"

std::unique_ptr<WebApplicationInfo> CreateWebAppInfoForEcheApp() {
  std::unique_ptr<WebApplicationInfo> info =
      std::make_unique<WebApplicationInfo>();
  info->start_url = GURL(chromeos::eche_app::kChromeUIEcheAppURL);
  info->scope = GURL(chromeos::eche_app::kChromeUIEcheAppURL);
  // |title| should come from a resource string, but this is the Eche app, and
  // doesn't have one.
  info->title = u"Eche App";
  web_app::CreateIconInfoForSystemWebApp(
      info->start_url,
      {{"app_icon_256.png", 256, IDR_CHROMEOS_ECHE_APP_ICON_256_PNG}}, *info);
  info->theme_color = 0xFFFFFFFF;
  info->background_color = 0xFFFFFFFF;
  info->display_mode = blink::mojom::DisplayMode::kMinimalUi;
  info->open_as_window = true;

  return info;
}
