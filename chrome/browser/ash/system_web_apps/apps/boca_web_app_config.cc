// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/boca_web_app_config.h"

#include "ash/constants/ash_features.h"
#include "ash/webui/boca_ui/boca_ui.h"
#include "chromeos/ash/components/boca/boca_role_util.h"

namespace ash {
BocaUIConfig::BocaUIConfig()
    : DefaultWebUIConfig(content::kChromeUIUntrustedScheme,
                         ash::boca::kChromeBocaAppHost) {}

bool BocaUIConfig::IsWebUIEnabled(content::BrowserContext* browser_context) {
  return ash::boca_util::IsEnabled();
}
}  // namespace ash
