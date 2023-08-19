// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_loader.h"

#include <utility>

#include "ash/constants/ash_switches.h"
#include "base/barrier_callback.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/lacros_selection_loader.h"
#include "chrome/browser/ash/crosapi/rootfs_lacros_loader.h"
#include "chrome/browser/ash/crosapi/stateful_lacros_loader.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/cros_component_manager.h"
#include "chromeos/ash/components/cryptohome/system_salt_getter.h"

namespace crosapi {

namespace {
// There are 2 lacros selections, rootfs lacros and stateful lacros.
constexpr size_t kLacrosSelectionTypes = 2;
}  // namespace

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

BrowserLoader::LacrosSelectionVersion::LacrosSelectionVersion(
    LacrosSelection selection,
    base::Version version)
    : selection(selection), version(std::move(version)) {
  CHECK_NE(selection, LacrosSelection::kDeployedLocally);
}

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
                                       LacrosSelectionSource source) {
  LOG(WARNING) << "rootfs lacros is selected by " << source;

  rootfs_lacros_loader_->Load(
      base::BindOnce(&BrowserLoader::OnLoadComplete, weak_factory_.GetWeakPtr(),
                     std::move(callback), LacrosSelection::kRootfs));
}

void BrowserLoader::SelectStatefulLacros(LoadCompletionCallback callback,
                                         LacrosSelectionSource source) {
  LOG(WARNING) << "stateful lacros is selected by " << source;

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
        SelectRootfsLacros(std::move(callback), LacrosSelectionSource::kPolicy);
        return;
      case browser_util::LacrosSelection::kStateful:
        SelectStatefulLacros(std::move(callback),
                             LacrosSelectionSource::kPolicy);
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
  auto barrier_callback = base::BarrierCallback<LacrosSelectionVersion>(
      kLacrosSelectionTypes,
      base::BindOnce(&BrowserLoader::OnLoadVersions, weak_factory_.GetWeakPtr(),
                     std::move(callback)));

  rootfs_lacros_loader_->GetVersion(
      base::BindOnce(&BrowserLoader::OnGetVersion, weak_factory_.GetWeakPtr(),
                     LacrosSelection::kRootfs, barrier_callback));
  stateful_lacros_loader_->GetVersion(
      base::BindOnce(&BrowserLoader::OnGetVersion, weak_factory_.GetWeakPtr(),
                     LacrosSelection::kStateful, barrier_callback));
}

void BrowserLoader::OnGetVersion(
    LacrosSelection selection,
    base::OnceCallback<void(LacrosSelectionVersion)> barrier_callback,
    const base::Version& version) {
  std::move(barrier_callback).Run(LacrosSelectionVersion(selection, version));
}

void BrowserLoader::OnLoadVersions(
    LoadCompletionCallback callback,
    std::vector<LacrosSelectionVersion> versions) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(versions.size(), kLacrosSelectionTypes);

  // Compare the rootfs vs stateful lacros-chrome binary versions.
  // If the rootfs lacros-chrome is greater than or equal to the stateful
  // lacros-chrome version, prioritize using the rootfs lacros-chrome and let
  // stateful lacros-chrome update in the background.
  auto selected = base::ranges::max_element(
      versions,
      [](const LacrosSelectionVersion& lhs, const LacrosSelectionVersion& rhs) {
        if (!lhs.version.IsValid()) {
          return true;
        }

        if (!rhs.version.IsValid()) {
          return false;
        }

        if (lhs.version != rhs.version) {
          return lhs.version < rhs.version;
        }

        // If the versions are the same, stateful lacros-chrome should be
        // prioritized, so considers LacrosSelectionVersion with kRootfs to be
        // smaller. Note that this comparison only happens between kRootfs and
        // kStateful.
        return lhs.selection == LacrosSelection::kRootfs;
      });

  if (!selected->version.IsValid()) {
    // Neither rootfs lacros nor stateful lacros are available.
    // Returning an empty file path to notify error.
    LOG(ERROR) << "No lacros is available";
    std::move(callback).Run(base::FilePath(), LacrosSelection::kStateful,
                            base::Version());
    return;
  }

  // Selected lacros may be older than the one which was running in a previous
  // sessions, accidentally. For experiment, now we intentionally ignore
  // the case and forcibly load the selected one, which is the best we could do
  // at this moment.
  // TODO(crbug.com/1293250): Check the condition and report it via UMA stats.

  switch (selected->selection) {
    case LacrosSelection::kRootfs: {
      SelectRootfsLacros(std::move(callback),
                         LacrosSelectionSource::kCompatibilityCheck);
      break;
    }
    case LacrosSelection::kStateful: {
      SelectStatefulLacros(std::move(callback),
                           LacrosSelectionSource::kCompatibilityCheck);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

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

std::ostream& operator<<(std::ostream& ostream,
                         BrowserLoader::LacrosSelectionSource source) {
  switch (source) {
    case BrowserLoader::LacrosSelectionSource::kUnknown:
      return ostream << "Unknown";
    case BrowserLoader::LacrosSelectionSource::kCompatibilityCheck:
      return ostream << "CompatibilityCheck";
    case BrowserLoader::LacrosSelectionSource::kPolicy:
      return ostream << "Policy";
    case BrowserLoader::LacrosSelectionSource::kDeployedPath:
      return ostream << "DeployedPath";
  }
}

}  // namespace crosapi
