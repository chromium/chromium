// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/delayed_install_manager.h"

#include <string>
#include <vector>

#include "base/check.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/extensions/install_gate.h"
#include "extensions/browser/extension_registrar.h"

namespace extensions {

DelayedInstallManager::DelayedInstallManager(
    ExtensionPrefs* extension_prefs,
    ExtensionRegistrar* extension_registrar)
    : extension_prefs_(extension_prefs),
      extension_registrar_(extension_registrar) {}

DelayedInstallManager::~DelayedInstallManager() = default;

void DelayedInstallManager::Shutdown() {
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

void DelayedInstallManager::FinishInstallationsDelayedByShutdown() {
  TRACE_EVENT0("browser,startup",
               "DelayedInstallManager::FinishInstallationsDelayedByShutdown");

  const ExtensionPrefs::ExtensionsInfo delayed_info =
      extension_prefs_->GetAllDelayedInstallInfo();
  for (const auto& info : delayed_info) {
    scoped_refptr<const Extension> extension;
    if (info.extension_manifest) {
      std::string error;
      extension = Extension::Create(
          info.extension_path, info.extension_location,
          *info.extension_manifest,
          extension_prefs_->GetDelayedInstallCreationFlags(info.extension_id),
          info.extension_id, &error);
      if (extension.get()) {
        delayed_installs_.Insert(extension);
      }
    }
  }
  MaybeFinishDelayedInstallations();
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
      ShouldDelayExtensionInstall(extension, install_immediately, &reason);
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

void DelayedInstallManager::RegisterInstallGate(
    ExtensionPrefs::DelayReason reason,
    InstallGate* install_delayer) {
  DCHECK(install_delayer_registry_.end() ==
         install_delayer_registry_.find(reason));
  install_delayer_registry_[reason] = install_delayer;
}

void DelayedInstallManager::UnregisterInstallGate(
    InstallGate* install_delayer) {
  std::erase_if(install_delayer_registry_, [&](const auto& pair) {
    return pair.second == install_delayer;
  });
}

InstallGate::Action DelayedInstallManager::ShouldDelayExtensionInstall(
    const Extension* extension,
    bool install_immediately,
    ExtensionPrefs::DelayReason* reason) const {
  for (const auto& entry : install_delayer_registry_) {
    InstallGate* const delayer = entry.second;
    InstallGate::Action action =
        delayer->ShouldDelay(extension, install_immediately);
    if (action != InstallGate::INSTALL) {
      *reason = entry.first;
      return action;
    }
  }

  return InstallGate::INSTALL;
}

}  // namespace extensions
