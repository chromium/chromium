// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/boca_web_app_config.h"

#include "ash/constants/ash_features.h"
#include "ash/webui/boca_ui/boca_ui.h"

namespace ash {
BocaUIConfig::BocaUIConfig()
    : content::WebUIConfig(content::kChromeUIUntrustedScheme,
                           ash::kChromeBocaAppHost) {}
bool BocaUIConfig::IsWebUIEnabled(content::BrowserContext* browser_context) {
  return ash::features::IsBocaEnabled();
}

std::unique_ptr<content::WebUIController> BocaUIConfig::CreateWebUIController(
    content::WebUI* web_ui,
    const GURL& url) {
  return std::make_unique<ash::BocaUI>(web_ui);
}
}  // namespace ash
