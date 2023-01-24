// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_kiosk_delegate.h"

#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"

namespace extensions {

bool ChromeKioskDelegate::IsAutoLaunchedKioskApp(const ExtensionId& id) const {
  if (!ash::KioskAppManager::IsInitialized()) {
    return false;
  }

  ash::KioskAppManager::App app_info;
  return ash::KioskAppManager::Get()->GetApp(id, &app_info) &&
         app_info.was_auto_launched_with_zero_delay;
}

}  // namespace extensions
