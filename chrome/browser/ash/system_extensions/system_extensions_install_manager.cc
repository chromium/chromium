// Copyright 2021 The Chromium Authors. All rights reserved.
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
#include "chrome/browser/ash/system_extensions/system_extensions_profile_utils.h"
#include "chrome/browser/ash/system_extensions/system_extensions_registry_manager.h"
#include "chrome/browser/ash/system_extensions/system_extensions_webui_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/webui_config_map.h"
#include "content/public/common/url_constants.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/chromeos/system_extensions/window_management/cros_window_management.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration_options.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace ash {

SystemExtensionsInstallManager::SystemExtensionsInstallManager(
    Profile* profile,
    SystemExtensionsRegistryManager& registry_manager,
    SystemExtensionsRegistry& registry)
    : profile_(profile),
      registry_manager_(registry_manager),
      registry_(registry) {
  InstallFromCommandLineIfNecessary();
}

SystemExtensionsInstallManager::~SystemExtensionsInstallManager() = default;

void SystemExtensionsInstallManager::InstallUnpackedExtensionFromDir(
    const base::FilePath& unpacked_system_extension_dir,
    OnceInstallCallback final_callback) {
  StartInstallation(std::move(final_callback), unpacked_system_extension_dir);
}

void SystemExtensionsInstallManager::InstallFromCommandLineIfNecessary() {
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

  // Installation Step #3: Create a WebUIConfig so that resources are served.
  auto config = std::make_unique<SystemExtensionsWebUIConfig>(system_extension);
  content::WebUIConfigMap::GetInstance().AddUntrustedWebUIConfig(
      std::move(config));

  // Installation Step #4: Add the System Extension to the registry.
  SystemExtensionId id = system_extension.id;
  registry_manager_->AddSystemExtension(std::move(system_extension));

  std::move(final_callback).Run(std::move(id));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&SystemExtensionsInstallManager::RegisterServiceWorker,
                     weak_ptr_factory_.GetWeakPtr(), id));
}

void SystemExtensionsInstallManager::RegisterServiceWorker(
    const SystemExtensionId& system_extension_id) {
  auto* system_extension = registry_->GetById(system_extension_id);
  if (!system_extension) {
    LOG(ERROR) << "Tried to install service worker for non-existent extension";
    return;
  }

  blink::mojom::ServiceWorkerRegistrationOptions options(
      system_extension->base_url, blink::mojom::ScriptType::kClassic,
      blink::mojom::ServiceWorkerUpdateViaCache::kImports);
  blink::StorageKey key(url::Origin::Create(options.scope));

  // Installation Step #5: Register a Service Worker for the System Extension.
  auto* worker_context =
      profile_->GetDefaultStoragePartition()->GetServiceWorkerContext();
  worker_context->RegisterServiceWorker(
      system_extension->service_worker_url, key, options,
      base::BindOnce(
          &SystemExtensionsInstallManager::NotifyServiceWorkerRegistered,
          weak_ptr_factory_.GetWeakPtr(), system_extension_id));
}

void SystemExtensionsInstallManager::NotifyServiceWorkerRegistered(
    const SystemExtensionId& system_extension_id,
    blink::ServiceWorkerStatusCode status_code) {
  if (status_code != blink::ServiceWorkerStatusCode::kOk) {
    LOG(ERROR) << "Failed to register Service Worker: "
               << blink::ServiceWorkerStatusToString(status_code);
    return;
  }

  for (auto& observer : observers_)
    observer.OnServiceWorkerRegistered(system_extension_id, status_code);
}

void SystemExtensionsInstallManager::Uninstall(
    const SystemExtensionId& system_extension_id) {
  auto* system_extension = registry_->GetById(system_extension_id);
  if (!system_extension) {
    return;
  }

  const GURL& scope = system_extension->base_url;
  const url::Origin& origin = url::Origin::Create(system_extension->base_url);

  // The un-installation steps are in reverse order of the installation steps.

  // Uninstallation Step #1: Unregister the Service Worker.
  auto* worker_context =
      profile_->GetDefaultStoragePartition()->GetServiceWorkerContext();
  blink::StorageKey key(origin);
  worker_context->UnregisterServiceWorker(
      scope, key,
      base::BindOnce(
          &SystemExtensionsInstallManager::NotifyServiceWorkerUnregistered,
          weak_ptr_factory_.GetWeakPtr(), system_extension_id));

  // Uninstallation Step #2: Remove the WebUIConfig for the System Extension.
  content::WebUIConfigMap::GetInstance().RemoveConfig(origin);

  // Uninstallation Step #3: Remove System Extension from the registry.
  registry_manager_->RemoveSystemExtension(system_extension_id);

  // Uninstallation Step #4: Delete the System Extension assets.
  io_helper_.AsyncCall(&IOHelper::RemoveExtensionAssets)
      .WithArgs(GetDirectoryForSystemExtension(*profile_, system_extension_id))
      .Then(base::BindOnce(&SystemExtensionsInstallManager::NotifyAssetsRemoved,
                           weak_ptr_factory_.GetWeakPtr(),
                           system_extension_id));
}

void SystemExtensionsInstallManager::NotifyServiceWorkerUnregistered(
    const SystemExtensionId& system_extension_id,
    bool succeeded) {
  // TODO(b/238578914): Consider changing UnregisterServiceWorker to pass a
  // ServiceWorkerStatusCode instead of a bool.
  if (!succeeded)
    LOG(ERROR) << "Failed to unregister Service Worker.";

  for (auto& observer : observers_)
    observer.OnServiceWorkerUnregistered(system_extension_id, succeeded);
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
