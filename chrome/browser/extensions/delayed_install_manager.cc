// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/delayed_install_manager.h"

#include <string>
#include <vector>

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/install_gate.h"
#include "extensions/browser/extension_registrar.h"

namespace extensions {

DelayedInstallManager::DelayedInstallManager(
    ExtensionService* extension_service,
    ExtensionPrefs* extension_prefs,
    ExtensionRegistrar* extension_registrar)
    : extension_service_(extension_service),
      extension_prefs_(extension_prefs),
      extension_registrar_(extension_registrar) {}

DelayedInstallManager::~DelayedInstallManager() = default;

void DelayedInstallManager::Shutdown() {
  extension_service_ = nullptr;
  extension_prefs_ = nullptr;
  extension_registrar_ = nullptr;
}

bool DelayedInstallManager::Contains(const ExtensionId& id) const {
  return delayed_installs_.Contains(id);
}

void DelayedInstallManager::Insert(scoped_refptr<const Extension> extension) {
  delayed_installs_.Insert(extension);
}

void DelayedInstallManager::Remove(const ExtensionId& id) {
  delayed_installs_.Remove(id);
}

const Extension* DelayedInstallManager::GetPendingExtensionUpdate(
    const ExtensionId& id) const {
  return delayed_installs_.GetByID(id);
}

void DelayedInstallManager::MaybeFinishDelayedInstallations() {
  std::vector<std::string> to_be_installed;
  for (const auto& extension : delayed_installs_) {
    to_be_installed.push_back(extension->id());
  }
  for (const auto& extension_id : to_be_installed) {
    FinishDelayedInstallationIfReady(extension_id,
                                     /*install_immediately=*/false);
  }
}

bool DelayedInstallManager::FinishDelayedInstallationIfReady(
    const std::string& extension_id,
    bool install_immediately) {
  // Check if the extension already got installed.
  const Extension* extension = delayed_installs_.GetByID(extension_id);
  if (!extension) {
    return false;
  }

  ExtensionPrefs::DelayReason reason;
  const InstallGate::Action action =
      extension_service_->ShouldDelayExtensionInstall(
          extension, install_immediately, &reason);
  switch (action) {
    case InstallGate::INSTALL:
      break;
    case InstallGate::DELAY:
      // Bail out and continue to delay the install.
      return false;
    case InstallGate::ABORT:
      delayed_installs_.Remove(extension_id);
      // Make sure no version of the extension is actually installed, (i.e.,
      // that this delayed install was not an update).
      CHECK(!extension_prefs_->GetInstalledExtensionInfo(extension_id));
      extension_prefs_->DeleteExtensionPrefs(extension_id);
      return false;
  }

  scoped_refptr<const Extension> delayed_install =
      GetPendingExtensionUpdate(extension_id);
  CHECK(delayed_install.get());
  delayed_installs_.Remove(extension_id);

  if (!extension_prefs_->FinishDelayedInstallInfo(extension_id)) {
    NOTREACHED();
  }

  extension_registrar_->FinishInstallation(delayed_install.get());
  return true;
}

}  // namespace extensions
