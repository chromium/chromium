// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/stateful_lacros_loader.h"

#include <utility>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
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

// Returns whether lacros-chrome component is registered.
bool CheckRegisteredMayBlock(
    scoped_refptr<component_updater::CrOSComponentManager> manager,
    const std::string& lacros_component_name) {
  return manager->IsRegisteredMayBlock(lacros_component_name);
}

// Checks the local disk structure to confirm whether a component is installed.
// We intentionally avoid going through CrOSComponentManager since the latter
// functions around the idea of "registration" -- but the timing of this method
// is prelogin, so the component might exist but not yet be registered.
bool IsInstalledMayBlock(const std::string& name) {
  base::FilePath root;
  if (!base::PathService::Get(component_updater::DIR_COMPONENT_USER, &root)) {
    return false;
  }

  base::FilePath component_root =
      root.Append(component_updater::kComponentsRootPath).Append(name);
  if (!base::PathExists(component_root)) {
    return false;
  }

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
    manager->Load(component,
                  component_updater::CrOSComponentManager::MountPolicy::kMount,
                  component_updater::CrOSComponentManager::UpdatePolicy::kSkip,
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
    scoped_refptr<component_updater::CrOSComponentManager> manager,
    const std::string& lacros_component_name) {
  if (!CheckRegisteredMayBlock(manager, lacros_component_name)) {
    return false;
  }

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

StatefulLacrosLoader::StatefulLacrosLoader(
    scoped_refptr<component_updater::CrOSComponentManager> manager)
    : StatefulLacrosLoader(manager,
                           g_browser_process->component_updater(),
                           browser_util::GetLacrosComponentInfo().name) {}

StatefulLacrosLoader::StatefulLacrosLoader(
    scoped_refptr<component_updater::CrOSComponentManager> manager,
    component_updater::ComponentUpdateService* updater,
    const std::string& lacros_component_name)
    : component_manager_(manager),
      component_update_service_(updater),
      lacros_component_name_(lacros_component_name) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&CheckForComponentToPreloadMayBlock),
      base::BindOnce(&PreloadComponent, component_manager_));
  DCHECK(component_manager_);
}

StatefulLacrosLoader::~StatefulLacrosLoader() = default;

void StatefulLacrosLoader::Load(LoadCompletionCallback callback, bool forced) {
  LOG(WARNING) << "Loading stateful lacros.";

  // If stateful lacros-chrome is already loaded once, run `callback`
  // immediately with cached version and path values.
  // This code path is used in most cases as they are already calculated on
  // getting version except for the case where BrowserLoader is forced to select
  // stateful lacros-chrome by lacros selection policy.
  if (IsReady()) {
    std::move(callback).Run(version_.value(), path_.value());
    return;
  }

  LoadInternal(std::move(callback), forced);
}

void StatefulLacrosLoader::Unload() {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&CheckInstalledAndMaybeRemoveUserDirectory,
                     component_manager_, lacros_component_name_),
      base::BindOnce(&StatefulLacrosLoader::OnCheckInstalledToUnload,
                     weak_factory_.GetWeakPtr()));
}

void StatefulLacrosLoader::Reset() {
  // TODO(crbug.com/1432069): Reset call while loading breaks the behavior. Need
  // to handle such edge cases.
  version_ = absl::nullopt;
  path_ = absl::nullopt;
}

void StatefulLacrosLoader::GetVersion(
    base::OnceCallback<void(const base::Version&)> callback) {
  // If version is already calculated, immediately return the cached value.
  // Calculate if not.
  // Note that version value is reset on reloading.
  if (IsReady()) {
    std::move(callback).Run(version_.value());
    return;
  }

  // TODO(crbug.com/1455070): There's KI that the current implementation
  // occasionally wrongly identifies there exists. Fix the logic.
  // If there currently isn't a stateful lacros-chrome binary, set `verison_`
  // null to proceed to use the rootfs lacros-chrome binary and start the
  // installation of the stateful lacros-chrome binary in the background.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&IsInstalledMayBlock, lacros_component_name_),
      base::BindOnce(&StatefulLacrosLoader::OnCheckInstalledToGetVersion,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void StatefulLacrosLoader::LoadInternal(LoadCompletionCallback callback,
                                        bool forced) {
  // If a compatible installation exists, use that and download any updates in
  // the background. If not, report just there is no available stateful lacros
  // unless stateful lacros is forced by policy or about:flag entry.
  // If stateful lacros is forced, we cannot fallback to rootfs lacros, so wait
  // until the installation of stateful to be completed.
  auto update_policy =
      forced ? component_updater::CrOSComponentManager::UpdatePolicy::kDontForce
             : component_updater::CrOSComponentManager::UpdatePolicy::kSkip;

  component_manager_->Load(
      lacros_component_name_,
      component_updater::CrOSComponentManager::MountPolicy::kMount,
      update_policy,
      // If `callback` is null, means stateful lacros-chrome should be
      // installed/updated but rootfs lacros-chrome will be used.
      base::BindOnce(&StatefulLacrosLoader::OnLoad, weak_factory_.GetWeakPtr(),
                     std::move(callback)));
}

bool StatefulLacrosLoader::IsReady() {
  return version_.has_value() && path_.has_value();
}

void StatefulLacrosLoader::OnLoad(
    LoadCompletionCallback callback,
    component_updater::CrOSComponentManager::Error error,
    const base::FilePath& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool is_stateful_lacros_available =
      error == component_updater::CrOSComponentManager::Error::NONE &&
      !path.empty();
  LOG_IF(WARNING, !is_stateful_lacros_available)
      << "Error loading lacros component image in the "
      << (callback ? "foreground" : "background") << ": "
      << static_cast<int>(error) << ", " << path;

  version_ = is_stateful_lacros_available
                 ? browser_util::GetInstalledLacrosComponentVersion(
                       component_update_service_)
                 : base::Version();
  path_ = path;

  if (callback) {
    std::move(callback).Run(version_.value(), path_.value());
  } else {
    if (is_stateful_lacros_available) {
      LOG(WARNING) << "stateful lacros-chrome installation completed in the "
                   << "background in " << path << ", version is "
                   << version_.value();
    }
  }
}

void StatefulLacrosLoader::OnCheckInstalledToGetVersion(
    base::OnceCallback<void(const base::Version&)> callback,
    bool is_installed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!is_installed) {
    // Run `callback` immediately with empty version and start loading stateful
    // lacros-chrome in the background.
    // TODO(crbug.com/1432069): Reconsider `version_` and `path_` values
    // semantics.
    LoadInternal({}, /*forced=*/false);
    std::move(callback).Run(base::Version());
    return;
  }

  LoadInternal(base::BindOnce(
                   [](base::OnceCallback<void(const base::Version&)> callback,
                      base::Version version, const base::FilePath&) {
                     std::move(callback).Run(version);
                   },
                   std::move(callback)),
               false);
}

void StatefulLacrosLoader::OnCheckInstalledToUnload(bool was_installed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!was_installed) {
    return;
  }

  // Workaround for login crash when the user disables Lacros.
  // CrOSComponentManager::Unload() calls into code in MetadataTable that
  // assumes that system salt is available. This isn't always true when chrome
  // restarts to apply non-owner flags. It's hard to make MetadataTable async.
  // Ensure salt is available before unloading. https://crbug.com/1122674
  ash::SystemSaltGetter::Get()->GetSystemSalt(base::BindOnce(
      &StatefulLacrosLoader::UnloadAfterCleanUp, weak_factory_.GetWeakPtr()));
}

void StatefulLacrosLoader::UnloadAfterCleanUp(const std::string& ignored_salt) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(ash::SystemSaltGetter::Get()->GetRawSalt());
  component_manager_->Unload(lacros_component_name_);
}

}  // namespace crosapi
