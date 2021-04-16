// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/shortcut_customization_system_web_app_info.h"

#include <memory>

#include "ash/content/shortcut_customization_ui/url_constants.h"
#include "ash/grit/ash_shortcut_customization_app_resources.h"
#include "ash/strings/grit/ash_strings.h"
#include "chrome/browser/ash/web_applications/system_web_app_install_utils.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "ui/base/l10n/l10n_util.h"

// TODO(jimmyxgong): Update to correct icon and app sizes.
std::unique_ptr<WebApplicationInfo>
CreateWebAppInfoForShortcutCustomizationSystemWebApp() {
  auto info = std::make_unique<WebApplicationInfo>();
  info->start_url = GURL(ash::kChromeUIShortcutCustomizationAppURL);
  info->scope = GURL(ash::kChromeUIShortcutCustomizationAppURL);
  info->title =
      l10n_util::GetStringUTF16(IDS_ASH_SHORTCUT_CUSTOMIZATION_APP_TITLE);
  web_app::CreateIconInfoForSystemWebApp(
      info->start_url,
      {{"app_icon_192.png", 192,
        IDR_ASH_SHORTCUT_CUSTOMIZATION_APP_APP_ICON_192_PNG}},
      *info);
  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  info->open_as_window = true;

  return info;
}
