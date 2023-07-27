// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_extensions/system_extensions_install_manager.h"

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/one_shot_event.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/ash/system_extensions/system_extension.h"
#include "chrome/browser/ash/system_extensions/system_extensions_persistent_storage.h"
#include "chrome/browser/ash/system_extensions/system_extensions_profile_utils.h"
#include "chrome/browser/ash/system_extensions/system_extensions_registry_manager.h"
#include "chrome/browser/ash/system_extensions/system_extensions_service_worker_manager.h"
#include "chrome/browser/ash/system_extensions/system_extensions_webui_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/webui_config_map.h"
#include "content/public/common/url_constants.h"
#include "third_party/blink/public/mojom/chromeos/system_extensions/window_management/cros_window_management.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace ash {

SystemExtensionsInstallManager::SystemExtensionsInstallManager(
    Profile* profile,
    SystemExtensionsRegistryManager& registry_manager,
    SystemExtensionsRegistry& registry,
    SystemExtensionsServiceWorkerManager& service_worker_manager,
    SystemExtensionsPersistentStorage& persistent_storage)
    : profile_(profile),
      service_worker_manager_(service_worker_manager),
      registry_manager_(registry_manager),
      registry_(registry),
      persistent_storage_(persistent_storage) {
  RegisterPreviouslyPersistedSystemExtensions();
  InstallFromCommandLineIfNecessary();
}

SystemExtensionsInstallManager::~SystemExtensionsInstallManager() = default;

void SystemExtensionsInstallManager::
    RegisterPreviouslyPersistedSystemExtensions() {
  const std::vector<SystemExtensionPersistedInfo> persisted_infos =
      persistent_storage_->GetAll();
  for (const auto& persisted_info : persisted_infos) {
    InstallStatusOrSystemExtension status_or_extension =
        sandboxed_unpacker_.GetSystemExtensionFromValue(
            persisted_info.manifest);

    if (!status_or_extension.ok()) {
      LOG(ERROR) << "Failed to register System Extension from Persistence "
                 << "Manager.";
      continue;
    }

    RegisterSystemExtension(std::move(status_or_extension).value());
  }

  on_register_previously_persisted_finished_.Signal();
}

void SystemExtensionsInstallManager::InstallUnpackedExtensionFromDir(
    const base::FilePath& unpacked_system_extension_dir,
    OnceInstallCallback final_callback) {
  DCHECK(on_register_previously_persisted_finished_.is_signaled());

  StartInstallation(std::move(final_callback), unpacked_system_extension_dir);
}

void SystemExtensionsInstallManager::InstallFromCommandLineIfNecessary() {
  DCHECK(on_register_previously_persisted_finished_.is_signaled());

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(ash::switches::kInstallSystemExtension)) {
    return;
  }
  base::FilePath system_extension_dir =
      command_line->GetSwitchValuePath(ash::switches::kInstallSystemExtension);

  StartInstallation(
      base::BindOnce(
          &SystemExtensionsInstallManager::OnInstallFromCommandLineFinished,
          weak_ptr_factory_.GetWeakPtr()),
      system_extension_dir);
}

void SystemExtensionsInstallManager::OnInstallFromCommandLineFinished(
    InstallStatusOrSystemExtensionId result) {
  if (!result.ok()) {
    LOG(ERROR) << "Failed to install extension from command line: "
               << static_cast<int32_t>(result.status());
  }

  on_command_line_install_finished_.Signal();
}

void SystemExtensionsInstallManager::StartInstallation(
    OnceInstallCallback final_callback,
    const base::FilePath& unpacked_system_extension_dir) {
  // Installation Step #1: Convert a manifest into a SystemExtension object.
  sandboxed_unpacker_.GetSystemExtensionFromDir(
      unpacked_system_extension_dir,
      base::BindOnce(
          &SystemExtensionsInstallManager::OnGetSystemExtensionFromDir,
          weak_ptr_factory_.GetWeakPtr(), std::move(final_callback),
          unpacked_system_extension_dir));
}

void SystemExtensionsInstallManager::OnGetSystemExtensionFromDir(
    OnceInstallCallback final_callback,
    const base::FilePath& unpacked_system_extension_dir,
    InstallStatusOrSystemExtension result) {
  if (!result.ok()) {
    std::move(final_callback).Run(result.status());
    return;
  }

  SystemExtensionId system_extension_id = result.value().id;
  const base::FilePath dest_dir =
      GetDirectoryForSystemExtension(*profile_, system_extension_id);
  const base::FilePath system_extensions_dir =
      GetSystemExtensionsProfileDir(*profile_);

  // Installation Step #2: Copy the System Extensions assets to a profile
  // directory.
  io_helper_.AsyncCall(&IOHelper::CopyExtensionAssets)
      .WithArgs(unpacked_system_extension_dir, dest_dir, system_extensions_dir)
      .Then(base::BindOnce(
          &SystemExtensionsInstallManager::OnAssetsCopiedToProfileDir,
          weak_ptr_factory_.GetWeakPtr(), std::move(final_callback),
          std::move(result).value()));
}

void SystemExtensionsInstallManager::OnAssetsCopiedToProfileDir(
    OnceInstallCallback final_callback,
    SystemExtension system_extension,
    bool did_succeed) {
  if (!did_succeed) {
    std::move(final_callback)
        .Run(SystemExtensionsInstallStatus::kFailedToCopyAssetsToProfileDir);
    return;
  }

  // Installation Step #3: Persist the System Extension across restarts.
  persistent_storage_->Add(system_extension);

  SystemExtensionId id = system_extension.id;
  RegisterSystemExtension(std::move(system_extension));
  std::move(final_callback).Run(std::move(id));
}

void SystemExtensionsInstallManager::RegisterSystemExtension(
    SystemExtension system_extension) {
  // Installation Step #4: Create a WebUIConfig so that resources are served.
  auto config = std::make_unique<SystemExtensionsWebUIConfig>(system_extension);
  content::WebUIConfigMap::GetInstance().AddUntrustedWebUIConfig(
      std::move(config));

  // Installation Step #5: Add the System Extension to the registry.
  SystemExtensionId id = system_extension.id;
  registry_manager_->AddSystemExtension(std::move(system_extension));

  // Installation Step #6: Register a Service Worker for the System Extension.
  service_worker_manager_->RegisterServiceWorker(id);
}

void SystemExtensionsInstallManager::Uninstall(
    const SystemExtensionId& system_extension_id) {
  auto* system_extension = registry_->GetById(system_extension_id);
  if (!system_extension) {
    return;
  }

  // Uninstallation Step #1: Unregister the Service Worker.
  service_worker_manager_->UnregisterServiceWorker(system_extension_id);

  // Uninstallation Step #2: Remove the WebUIConfig for the System Extension.
  content::WebUIConfigMap::GetInstance().RemoveConfig(
      system_extension->base_url);

  // Installation Step #3: Remove the System Extension from persistent storage.
  persistent_storage_->Remove(system_extension_id);

  // Uninstallation Step #4: Remove System Extension from the registry.
  registry_manager_->RemoveSystemExtension(system_extension_id);

  // Uninstallation Step #5: Delete the System Extension assets.
  io_helper_.AsyncCall(&IOHelper::RemoveExtensionAssets)
      .WithArgs(GetDirectoryForSystemExtension(*profile_, system_extension_id))
      .Then(base::BindOnce(&SystemExtensionsInstallManager::NotifyAssetsRemoved,
                           weak_ptr_factory_.GetWeakPtr(),
                           system_extension_id));
}

void SystemExtensionsInstallManager::NotifyAssetsRemoved(
    const SystemExtensionId& system_extension_id,
    bool succeeded) {
  if (!succeeded)
    LOG(ERROR) << "Failed to remove System Extension assets.";

  for (auto& observer : observers_)
    observer.OnSystemExtensionAssetsDeleted(system_extension_id, succeeded);
}

bool SystemExtensionsInstallManager::IOHelper::CopyExtensionAssets(
    const base::FilePath& unpacked_extension_dir,
    const base::FilePath& dest_dir,
    const base::FilePath& system_extensions_dir) {
  // TODO(crbug.com/1267802): Perform more checks when moving files or share
  // code with Extensions.

  // Create the System Extensions directory if it doesn't exist already e.g.
  // `/{profile_path}/System Extensions/`
  if (!base::PathExists(system_extensions_dir)) {
    if (!base::CreateDirectory(system_extensions_dir)) {
      LOG(ERROR) << "Failed to create the System Extensions dir.";
      return false;
    }
  }

  // Delete existing System Extension directory if necessary.
  if (!base::DeletePathRecursively(dest_dir)) {
    LOG(ERROR) << "Target System Extension dir already exists and couldn't be"
               << " deleted.";
    return false;
  }

  // Copy assets to their destination System Extensions directory e.g.
  // `/{profile_path}/System Extensions/{system_extension_id}/`
  if (!base::CopyDirectory(unpacked_extension_dir, dest_dir,
                           /*recursive=*/true)) {
    LOG(ERROR) << "Failed to copy System Extension assets.";
    return false;
  }

  return true;
}

bool SystemExtensionsInstallManager::IOHelper::RemoveExtensionAssets(
    const base::FilePath& system_extension_dir) {
  if (!base::DeletePathRecursively(system_extension_dir)) {
    LOG(ERROR) << "Failed to delete System Extension assets.";
    return false;
  }
  return true;
}

}  // namespace ash
