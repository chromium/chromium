// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/calculator_app/calculator_app_utils.h"

#include "ash/constants/web_app_id_constants.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"

namespace ash {
namespace calculator_app {

std::string GetInstalledCalculatorAppId(
    content::BrowserContext* browser_context) {
  if (extensions::ExtensionRegistry::Get(browser_context)
          ->GetInstalledExtension(extension_misc::kCalculatorAppId)) {
    return extension_misc::kCalculatorAppId;
  }
  return ash::kCalculatorAppId;
}

}  // namespace calculator_app
}  // namespace ash
