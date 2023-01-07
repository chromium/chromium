// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_extensions/system_extensions_internals_page_handler.h"

#include "base/debug/stack_trace.h"
#include "base/functional/callback_helpers.h"
#include "base/system/sys_info.h"
#include "chrome/browser/ash/system_extensions/system_extensions_profile_utils.h"

namespace ash {

SystemExtensionsInternalsPageHandler::SystemExtensionsInternalsPageHandler(
    Profile* profile,
    mojo::PendingReceiver<mojom::system_extensions_internals::PageHandler>
        receiver)
    : profile_(profile), receiver_(this, std::move(receiver)) {}

SystemExtensionsInternalsPageHandler::~SystemExtensionsInternalsPageHandler() =
    default;

void SystemExtensionsInternalsPageHandler::
    InstallSystemExtensionFromDownloadsDir(
        const base::SafeBaseName& system_extension_dir_name,
        InstallSystemExtensionFromDownloadsDirCallback callback) {
  if (!IsSystemExtensionsEnabled(profile_)) {
    std::move(callback).Run(false);
    return;
  }

  base::FilePath downloads_path;
  // Return a different path when we are running ChromeOS on Linux, where the
  // Downloads folder is different than on a real device. This is to make
  // development on ChromeOS on Linux easier.
  auto path_enum = base::SysInfo::IsRunningOnChromeOS()
                       ? chrome::DIR_DEFAULT_DOWNLOADS_SAFE
                       : chrome::DIR_DEFAULT_DOWNLOADS;
  if (!base::PathService::Get(path_enum, &downloads_path)) {
    std::move(callback).Run(false);
    return;
  }

  auto& install_manager =
      SystemExtensionsProvider::Get(profile_).install_manager();
  base::FilePath system_extension_dir =
      downloads_path.Append(system_extension_dir_name);

  install_manager.InstallUnpackedExtensionFromDir(
      system_extension_dir,
      base::BindOnce(&SystemExtensionsInternalsPageHandler::OnInstallFinished,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SystemExtensionsInternalsPageHandler::IsSystemExtensionInstalled(
    IsSystemExtensionInstalledCallback callback) {
  auto& registry = SystemExtensionsProvider::Get(profile_).registry();

  const bool is_installed = !registry.GetIds().empty();
  std::move(callback).Run(is_installed);
}

void SystemExtensionsInternalsPageHandler::UninstallSystemExtension(
    UninstallSystemExtensionCallback callback) {
  auto scoped_callback_runner = base::ScopedClosureRunner(std::move(callback));

  auto& provider = SystemExtensionsProvider::Get(profile_);
  auto& registry = provider.registry();
  const auto ids = registry.GetIds();

  if (ids.empty())
    return;

  provider.install_manager().Uninstall(ids[0]);
}

void SystemExtensionsInternalsPageHandler::OnInstallFinished(
    InstallSystemExtensionFromDownloadsDirCallback callback,
    InstallStatusOrSystemExtensionId result) {
  if (!result.ok()) {
    LOG(ERROR) << "failed with: " << static_cast<int32_t>(result.status());
  }

  std::move(callback).Run(result.ok());
}

}  // namespace ash
