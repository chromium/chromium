// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_kiosk_delegate.h"

#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"

namespace extensions {

bool ChromeKioskDelegate::IsAutoLaunchedKioskApp(const ExtensionId& id) const {
  if (!ash::KioskChromeAppManager::IsInitialized()) {
    return false;
  }

  auto app = ash::KioskChromeAppManager::Get()->GetApp(id);
  return app.has_value() && app->was_auto_launched_with_zero_delay;
}

}  // namespace extensions
