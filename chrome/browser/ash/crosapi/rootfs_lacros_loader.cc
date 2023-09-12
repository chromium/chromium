// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/rootfs_lacros_loader.h"

#include <string>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chromeos/ash/components/dbus/upstart/upstart_client.h"
#include "components/user_manager/user_manager.h"

namespace crosapi {

namespace {

// The rootfs lacros-chrome binary related files.
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

}  // namespace

RootfsLacrosLoader::RootfsLacrosLoader()
    : RootfsLacrosLoader(
          ash::UpstartClient::Get(),
          base::FilePath(kRootfsLacrosPath).Append(kLacrosMetadata)) {}

RootfsLacrosLoader::RootfsLacrosLoader(ash::UpstartClient* upstart_client,
                                       base::FilePath metadata_path)
    : upstart_client_(upstart_client),
      metadata_path_(std::move(metadata_path)) {}

RootfsLacrosLoader::~RootfsLacrosLoader() = default;

void RootfsLacrosLoader::Load(LoadCompletionCallback callback, bool forced) {
  LOG(WARNING) << "Loading rootfs lacros.";

  // Make sure to calculate `version_` before start loading.
  // It may not be calculated yet in case when lacros selection is defined by
  // selection policy or stateful lacros is not installed.
  GetVersion(base::BindOnce(&RootfsLacrosLoader::OnVersionReadyToLoad,
                            weak_factory_.GetWeakPtr(), std::move(callback)));
}

void RootfsLacrosLoader::Unload() {
  upstart_client_->StartJob(kLacrosUnmounterUpstartJob, {},
                            base::BindOnce([](bool) {}));
}

void RootfsLacrosLoader::Reset() {
  // TODO(crbug.com/1432069): Reset call while loading breaks the behavior. Need
  // to handle such edge cases.
  version_ = absl::nullopt;
}

void RootfsLacrosLoader::GetVersion(
    base::OnceCallback<void(const base::Version&)> callback) {
  // If version is already calculated, immediately return the cached value.
  // Calculate if not.
  // Note that version value is reset on reloading.
  if (version_.has_value()) {
    std::move(callback).Run(version_.value());
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&browser_util::GetRootfsLacrosVersionMayBlock,
                     metadata_path_),
      base::BindOnce(&RootfsLacrosLoader::OnGetVersion,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void RootfsLacrosLoader::OnGetVersion(
    base::OnceCallback<void(const base::Version&)> callback,
    base::Version version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  version_ = version;
  std::move(callback).Run(version_.value());
}

void RootfsLacrosLoader::OnVersionReadyToLoad(LoadCompletionCallback callback,
                                              const base::Version& version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // `version_` must be already filled by `version`.
  DCHECK(version_.has_value() &&
         ((!version_.value().IsValid() && !version.IsValid()) ||
          (version_.value() == version)));

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          &base::PathExists,
          base::FilePath(kRootfsLacrosMountPoint).Append(kLacrosChromeBinary)),
      base::BindOnce(&RootfsLacrosLoader::OnMountCheckToLoad,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void RootfsLacrosLoader::OnMountCheckToLoad(LoadCompletionCallback callback,
                                            bool already_mounted) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (already_mounted) {
    OnUpstartLacrosMounter(std::move(callback), true);
    return;
  }

  std::vector<std::string> job_env;
  if (user_manager::UserManager::Get()->IsLoggedInAsGuest()) {
    job_env.emplace_back("USE_SESSION_NAMESPACE=true");
  }

  upstart_client_->StartJob(
      kLacrosMounterUpstartJob, job_env,
      base::BindOnce(&RootfsLacrosLoader::OnUpstartLacrosMounter,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void RootfsLacrosLoader::OnUpstartLacrosMounter(LoadCompletionCallback callback,
                                                bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  LOG_IF(WARNING, !success) << "Upstart failed to mount rootfs lacros.";

  // `version_` must be calculated before coming here.
  // If `version_` is not filled, it implies Reset() is called, so handling this
  // case as an error.
  if (!version_.has_value()) {
    std::move(callback).Run(base::Version(), base::FilePath());
    return;
  }

  std::move(callback).Run(
      version_.value(),
      // If mounting wasn't successful, return a empty mount point to indicate
      // failure. `OnLoadComplete` handles empty mount points and forwards the
      // errors on the return callbacks.
      success ? base::FilePath(kRootfsLacrosMountPoint) : base::FilePath());
}

}  // namespace crosapi
