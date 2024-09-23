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
#include "chrome/browser/ash/crosapi/lacros_selection_loader.h"
#include "chrome/browser/ash/crosapi/lacros_selection_loader_factory.h"
#include "chrome/browser/ash/crosapi/rootfs_lacros_loader.h"
#include "chrome/browser/ash/crosapi/stateful_lacros_loader.h"
#include "chrome/browser/browser_process.h"
#include "chromeos/ash/components/cryptohome/system_salt_getter.h"
#include "components/component_updater/ash/component_manager_ash.h"

namespace crosapi {

namespace {
// There are 2 lacros selections, rootfs lacros and stateful lacros.
constexpr size_t kLacrosSelectionTypes = 2;

class LacrosSelectionLoaderFactoryImpl : public LacrosSelectionLoaderFactory {
 public:
  explicit LacrosSelectionLoaderFactoryImpl(
      scoped_refptr<component_updater::ComponentManagerAsh> manager)
      : component_manager_(manager) {}

  LacrosSelectionLoaderFactoryImpl(const LacrosSelectionLoaderFactoryImpl&) =
      delete;
  LacrosSelectionLoaderFactoryImpl& operator=(
      const LacrosSelectionLoaderFactoryImpl&) = delete;

  ~LacrosSelectionLoaderFactoryImpl() override = default;

  std::unique_ptr<LacrosSelectionLoader> CreateRootfsLacrosLoader() override {
    return std::make_unique<RootfsLacrosLoader>();
  }

  std::unique_ptr<LacrosSelectionLoader> CreateStatefulLacrosLoader() override {
    return std::make_unique<StatefulLacrosLoader>(component_manager_);
  }

 private:
  scoped_refptr<component_updater::ComponentManagerAsh> component_manager_;
};

bool IsUnloading(LacrosSelectionLoader* loader) {
  return loader && loader->IsUnloading();
}

}  // namespace

BrowserLoader::BrowserLoader(
    scoped_refptr<component_updater::ComponentManagerAsh> manager)
    : factory_(std::make_unique<LacrosSelectionLoaderFactoryImpl>(manager)) {}

BrowserLoader::BrowserLoader(
    std::unique_ptr<LacrosSelectionLoaderFactory> factory)
    : factory_(std::move(factory)) {}

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
  std::optional<ash::standalone_browser::LacrosSelection> lacros_selection =
      ash::standalone_browser::DetermineLacrosSelection();
  if (lacros_selection == ash::standalone_browser::LacrosSelection::kRootfs) {
    return false;
  }

  return true;
}

void BrowserLoader::SelectRootfsLacros(LoadCompletionCallback callback,
                                       LacrosSelectionSource source) {
  CHECK(rootfs_lacros_loader_);

  LOG(WARNING) << "rootfs lacros is selected by " << source;

  rootfs_lacros_loader_->Load(
      base::BindOnce(&BrowserLoader::OnLoadComplete, weak_factory_.GetWeakPtr(),
                     std::move(callback), LacrosSelection::kRootfs),
      source == LacrosSelectionSource::kForced);
}

void BrowserLoader::SelectStatefulLacros(LoadCompletionCallback callback,
                                         LacrosSelectionSource source) {
  CHECK(stateful_lacros_loader_);

  LOG(WARNING) << "stateful lacros is selected by " << source;

  stateful_lacros_loader_->Load(
      base::BindOnce(&BrowserLoader::OnLoadComplete, weak_factory_.GetWeakPtr(),
                     std::move(callback), LacrosSelection::kStateful),
      source == LacrosSelectionSource::kForced);

  // Unmount the rootfs lacros-chrome when using stateful lacros-chrome.
  // This will keep stateful lacros-chrome only mounted and not hold the rootfs
  // lacros-chrome mount until an `Unload`.
  if (rootfs_lacros_loader_) {
    rootfs_lacros_loader_->Unload(
        base::BindOnce(&BrowserLoader::OnUnloadCompleted,
                       weak_factory_.GetWeakPtr(), LacrosSelection::kRootfs));
  }
}

void BrowserLoader::Load(LoadCompletionCallback callback) {
  // Load should NOT be called after Unload is requested to BrowserLoader.
  CHECK(!is_unload_requested_);

  // If either of rootfs or stateful lacros loader is still unloading, wait
  // until the unload completion.
  if (IsUnloading(rootfs_lacros_loader_.get()) ||
      IsUnloading(stateful_lacros_loader_.get())) {
    LOG(WARNING) << "Wait load until unload completes";
    callback_on_unload_completion_ =
        base::BindOnce(&BrowserLoader::LoadNow, weak_factory_.GetWeakPtr(),
                       std::move(callback));
    return;
  }

  LoadNow(std::move(callback));
}

void BrowserLoader::LoadNow(LoadCompletionCallback callback) {
  // Reset lacros selection loaders since it may be already initialized one if
  // this is reloading.
  // TODO(elkurin): We should call Unload before reloading if these loaders
  // exist, then we can remove `reset` here.
  rootfs_lacros_loader_.reset();
  stateful_lacros_loader_.reset();

  lacros_start_load_time_ = base::TimeTicks::Now();
  // TODO(crbug.com/40689435): Remove non-error logging from this class.
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
  if (std::optional<ash::standalone_browser::LacrosSelection> lacros_selection =
          ash::standalone_browser::DetermineLacrosSelection()) {
    // TODO(crbug.com/40213424): We should check the version compatibility here,
    // too.
    switch (lacros_selection.value()) {
      case ash::standalone_browser::LacrosSelection::kRootfs:
        rootfs_lacros_loader_ = factory_->CreateRootfsLacrosLoader();
        SelectRootfsLacros(std::move(callback), LacrosSelectionSource::kForced);
        return;
      case ash::standalone_browser::LacrosSelection::kStateful:
        stateful_lacros_loader_ = factory_->CreateStatefulLacrosLoader();
        SelectStatefulLacros(std::move(callback),
                             LacrosSelectionSource::kForced);
        return;
      case ash::standalone_browser::LacrosSelection::kDeployedLocally:
        NOTREACHED_IN_MIGRATION();
        std::move(callback).Run(base::FilePath(),
                                LacrosSelection::kDeployedLocally,
                                base::Version());
        return;
    }
  }

  rootfs_lacros_loader_ = factory_->CreateRootfsLacrosLoader();
  stateful_lacros_loader_ = factory_->CreateStatefulLacrosLoader();

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

  if (is_unload_requested_) {
    LOG(WARNING) << "Unload is requested during collecting Lacros version.";
    std::move(callback).Run(base::FilePath(), LacrosSelection::kStateful,
                            base::Version());
    return;
  }

  // Compare the rootfs vs stateful lacros-chrome binary versions.
  // If the rootfs lacros-chrome is greater than lacros-chrome version,
  // prioritize using the rootfs lacros-chrome. If the stateful lacros-chrome is
  // not installed, let stateful lacros-chrome load in the background.
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
  // TODO(crbug.com/40213424): Check the condition and report it via UMA stats.

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
      NOTREACHED_IN_MIGRATION();
      std::move(callback).Run(
          base::FilePath(), LacrosSelection::kDeployedLocally, base::Version());
      return;
    }
  }
}

void BrowserLoader::Unload() {
  is_unload_requested_ = true;

  // Can be called even if Lacros isn't enabled, to clean up the old install.
  // Unmount the rootfs/stateful lacros-chrome if it was mounted.
  if (rootfs_lacros_loader_) {
    rootfs_lacros_loader_->Unload(
        base::BindOnce(&BrowserLoader::OnUnloadCompleted,
                       weak_factory_.GetWeakPtr(), LacrosSelection::kRootfs));
  }

  if (stateful_lacros_loader_) {
    stateful_lacros_loader_->Unload(
        base::BindOnce(&BrowserLoader::OnUnloadCompleted,
                       weak_factory_.GetWeakPtr(), LacrosSelection::kStateful));
  }
}

void BrowserLoader::OnUnloadCompleted(LacrosSelection selection) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (selection) {
    case LacrosSelection::kRootfs:
      CHECK(rootfs_lacros_loader_->IsUnloaded());
      rootfs_lacros_loader_.reset();
      break;
    case LacrosSelection::kStateful:
      CHECK(stateful_lacros_loader_->IsUnloaded());
      stateful_lacros_loader_.reset();
      break;
    case LacrosSelection::kDeployedLocally:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  // If either of rootfs or stateful lacros loader is still in the process of
  // unload, wait running completion callback.
  if (IsUnloading(rootfs_lacros_loader_.get()) ||
      IsUnloading(stateful_lacros_loader_.get())) {
    return;
  }

  // If both of the rootfs and stateful lacros load completed unloading, run the
  // stored callback if exists.
  if (callback_on_unload_completion_) {
    std::move(callback_on_unload_completion_).Run();
  }
}

base::FilePath DetermineLacrosBinaryPath(const base::FilePath& path) {
  // Interpret path as directory. If that fails, interpret it as the executable.
  base::FilePath expanded =
      path.Append(LacrosSelectionLoader::kLacrosChromeBinary);
  if (base::PathExists(expanded)) {
    return expanded;
  }
  if (base::PathExists(path)) {
    return path;
  }
  return {};
}

void BrowserLoader::OnLoadComplete(LoadCompletionCallback callback,
                                   LacrosSelection selection,
                                   base::Version version,
                                   const base::FilePath& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (is_unload_requested_) {
    LOG(WARNING) << "Unload is requested during loading.";
    std::move(callback).Run(base::FilePath(), LacrosSelection::kStateful,
                            base::Version());
    return;
  }

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
      base::BindOnce(&DetermineLacrosBinaryPath, path),
      base::BindOnce(&BrowserLoader::FinishOnLoadComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback), path,
                     selection, std::move(version)));
}

void BrowserLoader::FinishOnLoadComplete(LoadCompletionCallback callback,
                                         const base::FilePath& path,
                                         LacrosSelection selection,
                                         base::Version version,
                                         const base::FilePath& lacros_binary) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (is_unload_requested_) {
    LOG(WARNING) << "Unload is requested during determining lacros path.";
    std::move(callback).Run(base::FilePath(), LacrosSelection::kStateful,
                            base::Version());
    return;
  }

  if (lacros_binary.empty()) {
    LOG(ERROR) << "Failed to find binary at " << path;
    std::move(callback).Run(base::FilePath(), selection, base::Version());
    return;
  }

  base::UmaHistogramMediumTimes(
      "ChromeOS.Lacros.LoadTime",
      base::TimeTicks::Now() - lacros_start_load_time_);

  // Log the path on success.
  LOG(WARNING) << "Loaded lacros image with binary " << lacros_binary;
  std::move(callback).Run(lacros_binary, selection, std::move(version));
}

std::ostream& operator<<(std::ostream& ostream,
                         BrowserLoader::LacrosSelectionSource source) {
  switch (source) {
    case BrowserLoader::LacrosSelectionSource::kUnknown:
      return ostream << "Unknown";
    case BrowserLoader::LacrosSelectionSource::kCompatibilityCheck:
      return ostream << "CompatibilityCheck";
    case BrowserLoader::LacrosSelectionSource::kForced:
      return ostream << "Forced";
    case BrowserLoader::LacrosSelectionSource::kDeployedPath:
      return ostream << "DeployedPath";
  }
}

}  // namespace crosapi
