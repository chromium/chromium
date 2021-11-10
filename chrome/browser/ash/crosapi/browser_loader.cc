// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_loader.h"

#include <utility>

#include "ash/constants/ash_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "components/component_updater/component_updater_service.h"

namespace crosapi {

namespace {

// The rootfs lacros-chrome binary related files.
constexpr char kLacrosChromeBinary[] = "chrome";
constexpr char kLacrosMetadata[] = "metadata.json";

// The rootfs lacros-chrome binary related paths.
// Must be kept in sync with lacros upstart conf files.
constexpr char kRootfsLacrosMountPoint[] = "/run/lacros";
constexpr char kRootfsLacrosPath[] = "/opt/google/lacros";

// Lacros upstart jobs for mounting/unmounting the lacros-chrome image.
// The conversion of upstart job names to dbus object paths is undocumented. See
// function nih_dbus_path in libnih for the implementation.
constexpr char kLacrosMounterUpstartJob[] = "lacros_2dmounter";
constexpr char kLacrosUnmounterUpstartJob[] = "lacros_2dunmounter";

std::string GetLacrosComponentName() {
  return browser_util::GetLacrosComponentInfo().name;
}

// Returns whether lacros-chrome component is registered.
bool CheckRegisteredMayBlock(
    scoped_refptr<component_updater::CrOSComponentManager> manager) {
  return manager->IsRegisteredMayBlock(GetLacrosComponentName());
}

// Returns whether any lacros-chrome component is registered.
bool CheckAnyRegisteredMayBlock(
    scoped_refptr<component_updater::CrOSComponentManager> manager) {
  for (const auto& component_info : {browser_util::kLacrosDogfoodCanaryInfo,
                                     browser_util::kLacrosDogfoodDevInfo,
                                     browser_util::kLacrosDogfoodBetaInfo,
                                     browser_util::kLacrosDogfoodStableInfo}) {
    if (manager->IsRegisteredMayBlock(component_info.name))
      return true;
  }
  return false;
}

// Returns whether lacros-fishfood component is already installed.
// If it is, delete the user directory, too, because it will be
// uninstalled.
bool CheckInstalledAndMaybeRemoveUserDirectory(
    scoped_refptr<component_updater::CrOSComponentManager> manager) {
  if (!CheckRegisteredMayBlock(manager))
    return false;

  // Since we're already on a background thread, delete the user-data-dir
  // associated with lacros.
  // TODO(hidehiko): This approach has timing issue. Specifically, if Chrome
  // shuts down during the directory remove, some partially-removed directory
  // may be kept, and if the user flips the flag in the next time, that
  // partially-removed directory could be used. Fix this.
  base::DeletePathRecursively(browser_util::GetUserDataDir());
  return true;
}

}  // namespace

BrowserLoader::BrowserLoader(
    scoped_refptr<component_updater::CrOSComponentManager> manager)
    : BrowserLoader(manager,
                    g_browser_process->component_updater(),
                    chromeos::UpstartClient::Get()) {}

BrowserLoader::BrowserLoader(
    scoped_refptr<component_updater::CrOSComponentManager> manager,
    component_updater::ComponentUpdateService* updater,
    chromeos::UpstartClient* upstart_client)
    : component_manager_(manager),
      component_update_service_(updater),
      upstart_client_(upstart_client) {
  DCHECK(component_manager_);
}

BrowserLoader::~BrowserLoader() = default;

void BrowserLoader::Load(LoadCompletionCallback callback) {
  DCHECK(browser_util::IsLacrosEnabled());

  // TODO(crbug.com/1078607): Remove non-error logging from this class.
  LOG(WARNING) << "Starting lacros component load.";

  // If the user has specified a path for the lacros-chrome binary, use that
  // rather than component manager.
  base::FilePath lacros_chrome_path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          chromeos::switches::kLacrosChromePath);
  if (!lacros_chrome_path.empty()) {
    OnLoadComplete(std::move(callback),
                   component_updater::CrOSComponentManager::Error::NONE,
                   lacros_chrome_path);
    return;
  }

  // If the user has specified to force using stateful or rootfs lacros-chrome
  // binary, force the selection.
  const base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  if (cmdline->HasSwitch(browser_util::kLacrosSelectionSwitch)) {
    auto value =
        cmdline->GetSwitchValueASCII(browser_util::kLacrosSelectionSwitch);
    if (value == browser_util::kLacrosSelectionRootfs) {
      LoadRootfsLacros(std::move(callback));
      return;
    }
    if (value == browser_util::kLacrosSelectionStateful) {
      LoadStatefulLacros(std::move(callback));
      return;
    }
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&CheckAnyRegisteredMayBlock, component_manager_),
      base::BindOnce(&BrowserLoader::OnLoadSelection,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void BrowserLoader::OnLoadSelection(LoadCompletionCallback callback,
                                    bool any_lacros_component_registered) {
  // If there currently isn't a stateful lacros-chrome binary, proceed to use
  // the rootfs lacros-chrome binary and start the installation of the
  // stateful lacros-chrome binary in the background.
  if (!any_lacros_component_registered) {
    LoadRootfsLacros(std::move(callback));
    LoadStatefulLacros({});
    return;
  }

  // Otherwise proceed to load/mount the stateful lacros-chrome binary.
  // In the case that the stateful lacros-chrome binary wasn't installed, this
  // might take some time.
  component_manager_->Load(
      GetLacrosComponentName(),
      component_updater::CrOSComponentManager::MountPolicy::kMount,
      component_updater::CrOSComponentManager::UpdatePolicy::kDontForce,
      base::BindOnce(&BrowserLoader::OnLoadSelectionMountStateful,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void BrowserLoader::OnLoadSelectionMountStateful(
    LoadCompletionCallback callback,
    component_updater::CrOSComponentManager::Error error,
    const base::FilePath& path) {
  if (error != component_updater::CrOSComponentManager::Error::NONE ||
      path.empty()) {
    LOG(WARNING) << "Error loading lacros component image: "
                 << static_cast<int>(error);
    std::move(callback).Run(base::FilePath(), LacrosSelection::kStateful);
    return;
  }

  // Proceed to compare the lacros-chrome binary versions in case rootfs
  // lacros-chrome binary is newer than stateful lacros-chrome binary.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&browser_util::GetRootfsLacrosVersionMayBlock,
                     base::FilePath(kRootfsLacrosPath).Append(kLacrosMetadata)),
      base::BindOnce(&BrowserLoader::OnLoadVersionSelection,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void BrowserLoader::OnLoadVersionSelection(
    LoadCompletionCallback callback,
    base::Version rootfs_lacros_version) {
  // Compare the rootfs vs stateful lacros-chrome binary versions.
  // If the rootfs lacros-chrome is greater than or equal to the stateful
  // lacros-chrome version, prioritize using the rootfs lacros-chrome and let
  // stateful lacros-chrome update in the background.
  if (rootfs_lacros_version.IsValid()) {
    const auto lacros_component_name =
        base::UTF8ToUTF16(base::StringPiece(GetLacrosComponentName()));
    for (const auto& component_info :
         component_update_service_->GetComponents()) {
      if (component_info.name != lacros_component_name)
        continue;
      LOG(WARNING) << "Comparing lacros versions: "
                   << "rootfs (" << rootfs_lacros_version.GetString() << "), "
                   << "stateful " << lacros_component_name << " ("
                   << component_info.version.GetString() << ")";
      if (component_info.version <= rootfs_lacros_version) {
        LOG(WARNING)
            << "Stateful lacros version is older or same as the one in rootfs, "
            << "proceeding to use rootfs lacros.";
        LoadRootfsLacros(std::move(callback));
        LoadStatefulLacros({});
        return;
      }
      // Break out to use stateful lacros-chrome.
      LOG(WARNING)
          << "Stateful lacros version is newer than the one in rootfs, "
          << "procceding to use stateful lacros.";
      break;
    }
  }
  LoadStatefulLacros(std::move(callback));
}

void BrowserLoader::LoadStatefulLacros(LoadCompletionCallback callback) {
  LOG(WARNING) << "Loading stateful lacros.";
  // Unmount the rootfs lacros-chrome if we want to use stateful lacros-chrome.
  // This will keep stateful lacros-chrome only mounted and not hold the rootfs
  // lacros-chrome mount until a `Unload`.
  if (callback) {
    // Ignore the unmount result.
    upstart_client_->StartJob(kLacrosUnmounterUpstartJob, {},
                              base::BindOnce([](bool) {}));
  }
  component_manager_->Load(
      GetLacrosComponentName(),
      component_updater::CrOSComponentManager::MountPolicy::kMount,
      // If a compatible installation exists, use that and download any updates
      // in the background.
      component_updater::CrOSComponentManager::UpdatePolicy::kDontForce,
      // If `callback` is null, means stateful lacros-chrome should be
      // installed/updated but rootfs lacros-chrome will be used.
      callback
          ? base::BindOnce(&BrowserLoader::OnLoadComplete,
                           weak_factory_.GetWeakPtr(), std::move(callback))
          : base::BindOnce([](component_updater::CrOSComponentManager::Error,
                              const base::FilePath& path) {}));
}

void BrowserLoader::LoadRootfsLacros(LoadCompletionCallback callback) {
  LOG(WARNING) << "Loading rootfs lacros.";
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          &base::PathExists,
          base::FilePath(kRootfsLacrosMountPoint).Append(kLacrosChromeBinary)),
      base::BindOnce(&BrowserLoader::OnLoadRootfsLacros,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void BrowserLoader::OnLoadRootfsLacros(LoadCompletionCallback callback,
                                       bool already_mounted) {
  if (already_mounted) {
    OnUpstartLacrosMounter(std::move(callback), true);
    return;
  }
  upstart_client_->StartJob(
      kLacrosMounterUpstartJob, {},
      base::BindOnce(&BrowserLoader::OnUpstartLacrosMounter,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void BrowserLoader::OnUpstartLacrosMounter(LoadCompletionCallback callback,
                                           bool success) {
  if (!success)
    LOG(WARNING) << "Upstart failed to mount rootfs lacros.";
  OnLoadComplete(
      std::move(callback), component_updater::CrOSComponentManager::Error::NONE,
      // If mounting wasn't successful, return a empty mount point to indicate
      // failure. `OnLoadComplete` handles empty mount points and forwards the
      // errors on the return callbacks.
      success ? base::FilePath(kRootfsLacrosMountPoint) : base::FilePath());
}

void BrowserLoader::Unload() {
  // Can be called even if Lacros isn't enabled, to clean up the old install.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&CheckInstalledAndMaybeRemoveUserDirectory,
                     component_manager_),
      base::BindOnce(&BrowserLoader::OnCheckInstalled,
                     weak_factory_.GetWeakPtr()));
  // Unmount the rootfs lacros-chrome if it was mounted.
  // Ignore the unmount result.
  upstart_client_->StartJob(kLacrosUnmounterUpstartJob, {},
                            base::BindOnce([](bool) {}));
}

void BrowserLoader::OnLoadComplete(
    LoadCompletionCallback callback,
    component_updater::CrOSComponentManager::Error error,
    const base::FilePath& path) {
  LacrosSelection selection = LacrosSelection::kStateful;
  if (path == base::FilePath(kRootfsLacrosPath) ||
      path == base::FilePath(kRootfsLacrosMountPoint)) {
    selection = LacrosSelection::kRootfs;
  }

  // Bail out on error or empty `path`.
  if (error != component_updater::CrOSComponentManager::Error::NONE ||
      path.empty()) {
    LOG(WARNING) << "Error loading lacros component image: "
                 << static_cast<int>(error);
    std::move(callback).Run(base::FilePath(), selection);
    return;
  }
  // Log the path on success.
  LOG(WARNING) << "Loaded lacros image at " << path.MaybeAsASCII();
  std::move(callback).Run(path, selection);
}

void BrowserLoader::OnCheckInstalled(bool was_installed) {
  if (!was_installed)
    return;

  // Workaround for login crash when the user un-sets the LacrosSupport flag.
  // CrOSComponentManager::Unload() calls into code in MetadataTable that
  // assumes that system salt is available. This isn't always true when chrome
  // restarts to apply non-owner flags. It's hard to make MetadataTable async.
  // Ensure salt is available before unloading. https://crbug.com/1122674
  chromeos::SystemSaltGetter::Get()->GetSystemSalt(base::BindOnce(
      &BrowserLoader::UnloadAfterCleanUp, weak_factory_.GetWeakPtr()));
}

void BrowserLoader::UnloadAfterCleanUp(const std::string& ignored_salt) {
  CHECK(chromeos::SystemSaltGetter::Get()->GetRawSalt());
  component_manager_->Unload(GetLacrosComponentName());
}

}  // namespace crosapi
