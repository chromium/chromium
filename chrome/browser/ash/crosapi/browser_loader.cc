// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_loader.h"

#include <utility>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/ash/crosapi/browser_data_back_migrator.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/cros_component_installer_chromeos.h"
#include "chrome/common/channel_info.h"
#include "chromeos/ash/components/cryptohome/system_salt_getter.h"
#include "components/component_updater/component_updater_paths.h"
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

// Checks the local disk structure to confirm whether a component is installed.
// We intentionally avoid going through CrOSComponentManager since the latter
// functions around the idea of "registration" -- but the timing of this method
// is prelogin, so the component might exist but not yet be registered.
bool IsInstalledMayBlock(const std::string& name) {
  base::FilePath root;
  if (!base::PathService::Get(component_updater::DIR_COMPONENT_USER, &root))
    return false;

  base::FilePath component_root =
      root.Append(component_updater::kComponentsRootPath).Append(name);
  if (!base::PathExists(component_root))
    return false;

  // Check for any subdirectory
  base::FileEnumerator enumerator(component_root, /*recursive=*/false,
                                  base::FileEnumerator::DIRECTORIES);
  base::FilePath path = enumerator.Next();
  return !path.empty();
}

// Called after preloading is finished.
void DonePreloading(component_updater::CrOSComponentManager::Error error,
                    const base::FilePath& path) {
  LOG(WARNING) << "Done preloading stateful Lacros. " << static_cast<int>(error)
               << " " << path;
}

// Preloads the given component, or does nothing if |component| is empty.
// Must be called on main thread.
void PreloadComponent(
    scoped_refptr<component_updater::CrOSComponentManager> manager,
    std::string component) {
  if (!component.empty()) {
    LOG(WARNING) << "Preloading stateful lacros. " << component;
    manager->Load(
        component, component_updater::CrOSComponentManager::MountPolicy::kMount,
        component_updater::CrOSComponentManager::UpdatePolicy::kDontForce,
        base::BindOnce(&DonePreloading));
  }
}

// This method is dispatched pre-login. At this time, we don't know whether
// Lacros is enabled. This method checks to see if the Lacros stateful component
// matching the ash channel is installed -- if it is then Lacros is enabled. At
// which point this method will begin loading stateful lacros.
// Returns the name of the component on success, empty string on failure.
std::string CheckForComponentToPreloadMayBlock() {
  browser_util::ComponentInfo info =
      browser_util::GetLacrosComponentInfoForChannel(chrome::GetChannel());
  bool registered = IsInstalledMayBlock(info.name);
  if (registered) {
    return info.name;
  }
  return "";
}

// Returns whether lacros-fishfood component is already installed.
// If it is, delete the user directory, too, because it will be
// uninstalled.
bool CheckInstalledAndMaybeRemoveUserDirectory(
    scoped_refptr<component_updater::CrOSComponentManager> manager) {
  if (!CheckRegisteredMayBlock(manager))
    return false;

  // Since we're already on a background thread, delete the user-data-dir
  // associated with lacros. Skip if Chrome is in safe mode to avoid deleting of
  // user data when Lacros is disabled only temporarily.
  // TODO(hidehiko): This approach has timing issue. Specifically, if Chrome
  // shuts down during the directory remove, some partially-removed directory
  // may be kept, and if the user flips the flag in the next time, that
  // partially-removed directory could be used. Fix this.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kSafeMode)) {
    // If backward migration is enabled, don't remove the lacros folder as it
    // will used by the migration and will be removed after it completes.
    if (!ash::BrowserDataBackMigrator::IsBackMigrationEnabled(
            crosapi::browser_util::PolicyInitState::kBeforeInit)) {
      base::DeletePathRecursively(browser_util::GetUserDataDir());
    }
  }
  return true;
}

}  // namespace

BrowserLoader::BrowserLoader(
    scoped_refptr<component_updater::CrOSComponentManager> manager)
    : BrowserLoader(manager,
                    g_browser_process->component_updater(),
                    ash::UpstartClient::Get()) {}

BrowserLoader::BrowserLoader(
    scoped_refptr<component_updater::CrOSComponentManager> manager,
    component_updater::ComponentUpdateService* updater,
    ash::UpstartClient* upstart_client)
    : component_manager_(manager),
      component_update_service_(updater),
      upstart_client_(upstart_client) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&CheckForComponentToPreloadMayBlock),
      base::BindOnce(&PreloadComponent, component_manager_));
  DCHECK(component_manager_);
}

BrowserLoader::~BrowserLoader() = default;

// static.
bool BrowserLoader::WillLoadStatefulComponentBuilds() {
  // If the lacros chrome path is specified BrowserLoader will always attempt to
  // load lacros from this path and component manager builds are ignored.
  const base::FilePath lacros_chrome_path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          ash::switches::kLacrosChromePath);
  if (!lacros_chrome_path.empty())
    return false;

  // If the lacros selection is forced by the user or by policy to rootfs it
  // will always be loaded and stateful component manager builds are ignored.
  absl::optional<browser_util::LacrosSelection> lacros_selection =
      browser_util::DetermineLacrosSelection();
  if (lacros_selection == browser_util::LacrosSelection::kRootfs) {
    return false;
  }

  return true;
}

void BrowserLoader::Load(LoadCompletionCallback callback) {
  lacros_start_load_time_ = base::TimeTicks::Now();
  // TODO(crbug.com/1078607): Remove non-error logging from this class.
  LOG(WARNING) << "Starting lacros component load.";

  // If the user has specified a path for the lacros-chrome binary, use that
  // rather than component manager.
  base::FilePath lacros_chrome_path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          ash::switches::kLacrosChromePath);
  if (!lacros_chrome_path.empty()) {
    OnLoadComplete(std::move(callback),
                   component_updater::CrOSComponentManager::Error::NONE,
                   lacros_chrome_path);
    return;
  }

  // If the LacrosSelection policy or the user have specified to force using
  // stateful or rootfs lacros-chrome binary, force the selection. Otherwise,
  // load the newest available binary.
  if (absl::optional<browser_util::LacrosSelection> lacros_selection =
          browser_util::DetermineLacrosSelection()) {
    // TODO(crbug.com/1293250): We should check the version compatibility here,
    // too.
    switch (lacros_selection.value()) {
      case browser_util::LacrosSelection::kRootfs:
        LoadRootfsLacros(std::move(callback));
        return;
      case browser_util::LacrosSelection::kStateful:
        LoadStatefulLacros(std::move(callback));
        return;
    }
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&IsInstalledMayBlock, GetLacrosComponentName()),
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
  bool is_stateful_lacros_available =
      error == component_updater::CrOSComponentManager::Error::NONE &&
      !path.empty();
  LOG_IF(WARNING, !is_stateful_lacros_available)
      << "Error loading lacros component image: " << static_cast<int>(error)
      << ", " << path;

  // Continue to check Lacros version, even if it fails to see stateful
  // lacros version.

  // Proceed to compare the lacros-chrome binary versions in case rootfs
  // lacros-chrome binary is newer than stateful lacros-chrome binary.
  if (rootfs_lacros_version_.has_value()) {
    BrowserLoader::OnLoadVersionSelection(is_stateful_lacros_available,
                                          std::move(callback),
                                          rootfs_lacros_version_.value());
  } else {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(
            &browser_util::GetRootfsLacrosVersionMayBlock,
            base::FilePath(kRootfsLacrosPath).Append(kLacrosMetadata)),
        base::BindOnce(&BrowserLoader::OnLoadVersionSelection,
                       weak_factory_.GetWeakPtr(), is_stateful_lacros_available,
                       std::move(callback)));
  }
}

void BrowserLoader::OnLoadVersionSelection(
    bool is_stateful_lacros_available,
    LoadCompletionCallback callback,
    base::Version rootfs_lacros_version) {
  if (!rootfs_lacros_version_.has_value() && rootfs_lacros_version.IsValid())
    rootfs_lacros_version_ = rootfs_lacros_version;

  // Compare the rootfs vs stateful lacros-chrome binary versions.
  // If the rootfs lacros-chrome is greater than or equal to the stateful
  // lacros-chrome version, prioritize using the rootfs lacros-chrome and let
  // stateful lacros-chrome update in the background.
  // TODO(crbug.com/1293250): Clean up the code. `is_stateful_lacros_available`
  // is likely not needed here, consider removing this.
  base::Version stateful_lacros_version =
      is_stateful_lacros_available
          ? browser_util::GetInstalledLacrosComponentVersion(
                component_update_service_)
          : base::Version();

  LOG(WARNING) << "Lacros candidates: rootfs=" << rootfs_lacros_version
               << ", stateful=" << stateful_lacros_version;
  if (!rootfs_lacros_version.IsValid() && !stateful_lacros_version.IsValid()) {
    // Neither rootfs lacros nor stateful lacros are available.
    // Returning an empty file path to notify error.
    LOG(ERROR) << "No lacros is available";
    std::move(callback).Run(base::FilePath(), LacrosSelection::kStateful,
                            base::Version());
    return;
  }

  LacrosSelection selection;
  if (rootfs_lacros_version.IsValid() && stateful_lacros_version.IsValid()) {
    selection = rootfs_lacros_version < stateful_lacros_version
                    ? LacrosSelection::kStateful
                    : LacrosSelection::kRootfs;
  } else if (rootfs_lacros_version.IsValid()) {
    selection = LacrosSelection::kRootfs;
  } else {
    DCHECK(stateful_lacros_version.IsValid());
    selection = LacrosSelection::kStateful;
  }

  // Selected lacros may be older than the one which was running in a previous
  // sessions, accidentally. For experiment, now we intentionally ignore
  // the case and forcibly load the selected one, which is the best we could do
  // at this moment.
  // TODO(crbug.com/1293250): Check the condition and report it via UMA stats.

  switch (selection) {
    case LacrosSelection::kRootfs: {
      LOG(WARNING) << "rootfs lacros is selected";
      LoadRootfsLacros(std::move(callback));
      LoadStatefulLacros({});
      break;
    }
    case LacrosSelection::kStateful: {
      LOG(WARNING) << "stateful lacros is selected";
      LoadStatefulLacros(std::move(callback));
      break;
    }
  }
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
    std::move(callback).Run(base::FilePath(), selection, base::Version());
    return;
  }

  // Fail early if the chrome binary still doesn't exist, such that
  // (1) we end up with an error message in Ash's log, and
  // (2) BrowserManager doesn't endlessly try to spawn Lacros.
  // For example, in the past there have been issues with mounting rootfs Lacros
  // that resulted in /run/lacros being empty at this point.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&base::PathExists, path.Append(kLacrosChromeBinary)),
      base::BindOnce(&BrowserLoader::FinishOnLoadComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback), path,
                     selection));
}

void BrowserLoader::FinishOnLoadComplete(LoadCompletionCallback callback,
                                         const base::FilePath& path,
                                         LacrosSelection selection,
                                         bool lacros_binary_exists) {
  if (!lacros_binary_exists) {
    LOG(ERROR) << "Failed to find chrome binary at " << path;
    std::move(callback).Run(base::FilePath(), selection, base::Version());
    return;
  }

  base::Version version =
      selection == LacrosSelection::kStateful
          ? browser_util::GetInstalledLacrosComponentVersion(
                component_update_service_)
          : rootfs_lacros_version_.value_or(base::Version());

  base::UmaHistogramMediumTimes(
      "ChromeOS.Lacros.LoadTime",
      base::TimeTicks::Now() - lacros_start_load_time_);

  // Log the path on success.
  LOG(WARNING) << "Loaded lacros image at " << path;
  std::move(callback).Run(path, selection, std::move(version));
}

void BrowserLoader::OnCheckInstalled(bool was_installed) {
  if (!was_installed)
    return;

  // Workaround for login crash when the user un-sets the LacrosSupport flag.
  // CrOSComponentManager::Unload() calls into code in MetadataTable that
  // assumes that system salt is available. This isn't always true when chrome
  // restarts to apply non-owner flags. It's hard to make MetadataTable async.
  // Ensure salt is available before unloading. https://crbug.com/1122674
  ash::SystemSaltGetter::Get()->GetSystemSalt(base::BindOnce(
      &BrowserLoader::UnloadAfterCleanUp, weak_factory_.GetWeakPtr()));
}

void BrowserLoader::UnloadAfterCleanUp(const std::string& ignored_salt) {
  CHECK(ash::SystemSaltGetter::Get()->GetRawSalt());
  component_manager_->Unload(GetLacrosComponentName());
}

}  // namespace crosapi
