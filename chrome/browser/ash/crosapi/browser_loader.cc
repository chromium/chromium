// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_loader.h"

#include <utility>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/lacros_selection_loader.h"
#include "chrome/browser/ash/crosapi/rootfs_lacros_loader.h"
#include "chrome/browser/ash/crosapi/stateful_lacros_loader.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/cros_component_manager.h"
#include "chromeos/ash/components/cryptohome/system_salt_getter.h"

namespace crosapi {

BrowserLoader::BrowserLoader(
    scoped_refptr<component_updater::CrOSComponentManager> manager)
    : BrowserLoader(std::make_unique<RootfsLacrosLoader>(),
                    std::make_unique<StatefulLacrosLoader>(manager)) {}

BrowserLoader::BrowserLoader(
    std::unique_ptr<LacrosSelectionLoader> rootfs_lacros_loader,
    std::unique_ptr<LacrosSelectionLoader> stateful_lacros_loader)
    : rootfs_lacros_loader_(std::move(rootfs_lacros_loader)),
      stateful_lacros_loader_(std::move(stateful_lacros_loader)) {}

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

void BrowserLoader::SelectRootfsLacros(LoadCompletionCallback callback,
                                       bool load_stateful_lacros) {
  rootfs_lacros_loader_->Load(
      base::BindOnce(&BrowserLoader::OnLoadComplete, weak_factory_.GetWeakPtr(),
                     std::move(callback), LacrosSelection::kRootfs));
  if (load_stateful_lacros) {
    stateful_lacros_loader_->Load({});
  }
}

void BrowserLoader::SelectStatefulLacros(LoadCompletionCallback callback) {
  stateful_lacros_loader_->Load(
      base::BindOnce(&BrowserLoader::OnLoadComplete, weak_factory_.GetWeakPtr(),
                     std::move(callback), LacrosSelection::kStateful));

  // Unmount the rootfs lacros-chrome when using stateful lacros-chrome.
  // This will keep stateful lacros-chrome only mounted and not hold the rootfs
  // lacros-chrome mount until an `Unload`.
  rootfs_lacros_loader_->Unload();
}

void BrowserLoader::Load(LoadCompletionCallback callback) {
  // Reset lacros selection loaders before reloading.
  rootfs_lacros_loader_->Reset();
  stateful_lacros_loader_->Reset();

  lacros_start_load_time_ = base::TimeTicks::Now();
  // TODO(crbug.com/1078607): Remove non-error logging from this class.
  LOG(WARNING) << "Starting lacros component load.";

  // If the user has specified a path for the lacros-chrome binary, use that
  // rather than component manager.
  base::FilePath lacros_chrome_path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          ash::switches::kLacrosChromePath);
  if (!lacros_chrome_path.empty()) {
    // TODO(cbug.com/1429137): LacrosSelection::kStateful is not appropriate
    // here. We should introduce unknown state and set it here.
    OnLoadComplete(std::move(callback), LacrosSelection::kDeployedLocally,
                   base::Version(), lacros_chrome_path);
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
        SelectRootfsLacros(std::move(callback));
        return;
      case browser_util::LacrosSelection::kStateful:
        SelectStatefulLacros(std::move(callback));
        return;
      case browser_util::LacrosSelection::kDeployedLocally:
        NOTREACHED();
        std::move(callback).Run(base::FilePath(),
                                LacrosSelection::kDeployedLocally,
                                base::Version());
        return;
    }
  }

  // Proceed to load/mount the stateful lacros-chrome binary.
  // In the case that the stateful lacros-chrome binary wasn't installed, this
  // might take some time.
  stateful_lacros_loader_->GetVersion(
      base::BindOnce(&BrowserLoader::OnLoadStatefulLacros,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void BrowserLoader::OnLoadStatefulLacros(
    LoadCompletionCallback callback,
    base::Version stateful_lacros_version) {
  // If there currently isn't a stateful lacros-chrome binary, proceed to use
  // the rootfs lacros-chrome binary and start the installation of the stateful
  // lacros-chrome binary in the background.
  if (!stateful_lacros_version.IsValid()) {
    SelectRootfsLacros(std::move(callback), /*load_stateful_lacros=*/true);
    return;
  }

  rootfs_lacros_loader_->GetVersion(base::BindOnce(
      &BrowserLoader::OnLoadVersionSelection, weak_factory_.GetWeakPtr(),
      std::move(callback), std::move(stateful_lacros_version)));
}

void BrowserLoader::OnLoadVersionSelection(
    LoadCompletionCallback callback,
    base::Version stateful_lacros_version,
    base::Version rootfs_lacros_version) {
  // Compare the rootfs vs stateful lacros-chrome binary versions.
  // If the rootfs lacros-chrome is greater than or equal to the stateful
  // lacros-chrome version, prioritize using the rootfs lacros-chrome and let
  // stateful lacros-chrome update in the background.

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
      SelectRootfsLacros(std::move(callback), /*load_stateful_lacros=*/true);
      break;
    }
    case LacrosSelection::kStateful: {
      LOG(WARNING) << "stateful lacros is selected";
      SelectStatefulLacros(std::move(callback));
      break;
    }
    case LacrosSelection::kDeployedLocally: {
      NOTREACHED();
      std::move(callback).Run(
          base::FilePath(), LacrosSelection::kDeployedLocally, base::Version());
      return;
    }
  }
}

void BrowserLoader::Unload() {
  // Can be called even if Lacros isn't enabled, to clean up the old install.
  stateful_lacros_loader_->Unload();
  // Unmount the rootfs lacros-chrome if it was mounted.
  rootfs_lacros_loader_->Unload();
}

void BrowserLoader::OnLoadComplete(LoadCompletionCallback callback,
                                   LacrosSelection selection,
                                   base::Version version,
                                   const base::FilePath& path) {
  // Bail out on empty `path` which implies there was an error on loading
  // lacros.
  if (path.empty()) {
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
      base::BindOnce(&base::PathExists,
                     path.Append(LacrosSelectionLoader::kLacrosChromeBinary)),
      base::BindOnce(&BrowserLoader::FinishOnLoadComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback), path,
                     selection, std::move(version)));
}

void BrowserLoader::FinishOnLoadComplete(LoadCompletionCallback callback,
                                         const base::FilePath& path,
                                         LacrosSelection selection,
                                         base::Version version,
                                         bool lacros_binary_exists) {
  if (!lacros_binary_exists) {
    LOG(ERROR) << "Failed to find chrome binary at " << path;
    std::move(callback).Run(base::FilePath(), selection, base::Version());
    return;
  }

  base::UmaHistogramMediumTimes(
      "ChromeOS.Lacros.LoadTime",
      base::TimeTicks::Now() - lacros_start_load_time_);

  // Log the path on success.
  LOG(WARNING) << "Loaded lacros image at " << path;
  std::move(callback).Run(path, selection, std::move(version));
}

}  // namespace crosapi
