// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/system_apps/public/system_web_app_ui_config.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"

namespace ash::internal {
bool BaseSystemWebAppUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  auto* swa_manager = ash::SystemWebAppManager::Get(
      Profile::FromBrowserContext(browser_context));
  if (!swa_manager)
    return false;

  return swa_manager->IsAppEnabled(swa_type_);
}
}  // namespace ash::internal
