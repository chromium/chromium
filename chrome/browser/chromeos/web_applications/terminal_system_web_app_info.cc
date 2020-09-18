// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/web_applications/terminal_system_web_app_info.h"

#include <memory>

#include "base/feature_list.h"
#include "chrome/browser/chromeos/web_applications/system_web_app_install_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/web_application_info.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/generated_resources.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

std::unique_ptr<WebApplicationInfo> CreateWebAppInfoForTerminalSystemWebApp() {
  std::unique_ptr<WebApplicationInfo> info =
      std::make_unique<WebApplicationInfo>();
  // URL used for crostini::kCrostiniTerminalSystemAppId.
  info->start_url = GURL("chrome-untrusted://terminal/html/terminal.html");
  if (base::FeatureList::IsEnabled(features::kDesktopPWAsWithoutExtensions))
    info->scope = GURL(chrome::kChromeUIUntrustedTerminalURL);
  info->title = l10n_util::GetStringUTF16(IDS_CROSTINI_TERMINAL_APP_NAME);
  web_app::CreateIconInfoForSystemWebApp(
      info->start_url, {{"app_icon_192.png", 192, IDR_LOGO_CROSTINI_TERMINAL}},
      *info);
  info->background_color = 0xFF202124;
  info->display_mode = blink::mojom::DisplayMode::kStandalone;
  return info;
}
