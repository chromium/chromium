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
#include "chrome/browser/ash/system_extensions/system_extensions_web_ui_config_map.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"
#include "url/origin.h"

SystemExtensionsInstallManager::SystemExtensionsInstallManager(Profile* profile)
    : profile_(profile) {
  InstallFromCommandLineIfNecessary();
}

SystemExtensionsInstallManager::~SystemExtensionsInstallManager() = default;

std::vector<SystemExtensionId>
SystemExtensionsInstallManager::GetSystemExtensionIds() {
  std::vector<SystemExtensionId> extension_ids;
  for (const auto& id_and_extension : system_extensions_) {
    extension_ids.emplace_back(id_and_extension.first);
  }
  return extension_ids;
}

const SystemExtension* SystemExtensionsInstallManager::GetSystemExtensionById(
    const SystemExtensionId& id) {
  const auto it = system_extensions_.find(id);
  if (it == system_extensions_.end())
    return nullptr;
  return &it->second;
}

void SystemExtensionsInstallManager::InstallUnpackedExtensionFromDir(
    const base::FilePath& unpacked_system_extension_dir,
    OnceInstallCallback final_callback) {
  StartInstallation(std::move(final_callback), unpacked_system_extension_dir);
}

void SystemExtensionsInstallManager::InstallFromCommandLineIfNecessary() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(chromeos::switches::kInstallSystemExtension)) {
    return;
  }
  base::FilePath system_extension_dir = command_line->GetSwitchValuePath(
      chromeos::switches::kInstallSystemExtension);

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

  SystemExtensionId id = system_extension.id;
  SystemExtensionsWebUIConfigMap::GetInstance().AddForSystemExtension(
      system_extension);
  system_extensions_[{1, 2, 3, 4}] = std::move(system_extension);
  std::move(final_callback).Run(std::move(id));
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
    return false;
    LOG(ERROR) << "Failed to copy System Extension assets.";
  }

  return true;
}

const SystemExtension* SystemExtensionsInstallManager::GetSystemExtensionByURL(
    const GURL& url) {
  for (const auto& id_and_system_extension : system_extensions_) {
    if (url::IsSameOriginWith(id_and_system_extension.second.base_url, url))
      return &id_and_system_extension.second;
  }
  return nullptr;
}
