// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/shimless_rma_system_web_app_info.h"

#include <memory>

#include "ash/content/shimless_rma/url_constants.h"
#include "ash/grit/ash_shimless_rma_resources.h"
#include "ash/strings/grit/ash_strings.h"
#include "chrome/browser/ash/web_applications/system_web_app_install_utils.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "ui/base/l10n/l10n_util.h"

std::unique_ptr<WebApplicationInfo>
CreateWebAppInfoForShimlessRMASystemWebApp() {
  auto info = std::make_unique<WebApplicationInfo>();
  info->start_url = GURL(ash::kChromeUIShimlessRMAUrl);
  info->scope = GURL(ash::kChromeUIShimlessRMAUrl);
  info->title = l10n_util::GetStringUTF16(IDS_ASH_SHIMLESS_RMA_APP_TITLE);
  web_app::CreateIconInfoForSystemWebApp(
      info->start_url,
      {{"app_icon_192.png", 192, IDR_ASH_SHIMLESS_RMA_APP_ICON_192_PNG}},
      *info);
  info->theme_color = 0xFFFFFFFF;
  info->background_color = 0xFFFFFFFF;
  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  info->open_as_window = true;

  return info;
}
