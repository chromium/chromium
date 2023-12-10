// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/kiosk_app_update_install_gate.h"

#include "base/logging.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"

namespace ash {

KioskAppUpdateInstallGate::KioskAppUpdateInstallGate(Profile* profile)
    : profile_(profile),
      registry_(extensions::ExtensionRegistry::Get(profile)) {}

KioskAppUpdateInstallGate::~KioskAppUpdateInstallGate() = default;

extensions::InstallGate::Action KioskAppUpdateInstallGate::ShouldDelay(
    const extensions::Extension* extension,
    bool install_immediately) {
  // Install if this is the first install or the required platform version is
  // compliant with the current platform version.
  const bool first_install = !registry_->GetInstalledExtension(extension->id());
  const bool platform_compliant =
      KioskChromeAppManager::Get()->IsPlatformCompliantWithApp(extension);
  if (first_install || platform_compliant) {
    LOG_IF(WARNING, first_install && !platform_compliant)
        << "Install on an incompliant platform for the first install.";
    return INSTALL;
  }

  // Otherwise, delay install but update the required platform version meta data
  // to allow update engine to move on to the new platform version.
  KioskChromeAppManager::Get()->UpdateAppDataFromProfile(extension->id(),
                                                         profile_, extension);
  return DELAY;
}

}  // namespace ash
