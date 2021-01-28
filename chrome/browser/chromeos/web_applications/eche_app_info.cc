// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/web_applications/eche_app_info.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/web_applications/system_web_app_install_utils.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chromeos/components/eche_app_ui/url_constants.h"
#include "chromeos/grit/chromeos_eche_app_resources.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"

std::unique_ptr<WebApplicationInfo> CreateWebAppInfoForEcheApp() {
  std::unique_ptr<WebApplicationInfo> info =
      std::make_unique<WebApplicationInfo>();
  info->start_url = GURL(chromeos::kChromeUIEcheAppURL);
  info->scope = GURL(chromeos::kChromeUIEcheAppURL);
  // |title| should come from a resource string, but this is the Eche app, and
  // doesn't have one.
  info->title = base::UTF8ToUTF16("Eche App");
  // TODO(b/178159012): create icon info for system web app.
  info->theme_color = 0xFF4285F4;
  info->background_color = 0xFFFFFFFF;
  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  info->open_as_window = true;

  return info;
}
