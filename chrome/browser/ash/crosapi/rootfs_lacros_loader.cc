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
  CHECK(state_ == State::kNotLoaded ||
        state_ == State::kVersionReadyButNotLoaded)
      << state_;
  LOG(WARNING) << "Loading rootfs lacros.";

  if (state_ == State::kVersionReadyButNotLoaded) {
    OnVersionReadyToLoad(std::move(callback), version_.value());
    return;
  }

  // Calculate `version_` before start loading.
  // It's not calculated yet in case when lacros selection is defined by
  // selection policy or stateful lacros is not installed.
  state_ = State::kReadingVersion;
  GetVersionInternal(base::BindOnce(&RootfsLacrosLoader::OnVersionReadyToLoad,
                                    weak_factory_.GetWeakPtr(),
                                    std::move(callback)));
}

void RootfsLacrosLoader::Unload(base::OnceClosure callback) {
  switch (state_) {
    case State::kNotLoaded:
    case State::kVersionReadyButNotLoaded:
    case State::kUnloaded:
      // Nothing to unload if it's not loaded or already unloaded.
      state_ = State::kUnloaded;
      std::move(callback).Run();
      break;
    case State::kReadingVersion:
    case State::kLoading:
    case State::kUnloading:
      // If loader is busy, wait Unload until the current task has finished.
      pending_unload_ =
          base::BindOnce(&RootfsLacrosLoader::Unload,
                         weak_factory_.GetWeakPtr(), std::move(callback));
      break;
    case State::kLoaded:
      state_ = State::kUnloading;

      upstart_client_->StartJob(
          kLacrosUnmounterUpstartJob, {},
          base::BindOnce(&RootfsLacrosLoader::OnUnloadCompleted,
                         weak_factory_.GetWeakPtr(), std::move(callback)));
  }
}

void RootfsLacrosLoader::GetVersion(
    base::OnceCallback<void(const base::Version&)> callback) {
  CHECK_EQ(state_, State::kNotLoaded) << state_;
  state_ = State::kReadingVersion;
  GetVersionInternal(std::move(callback));
}

bool RootfsLacrosLoader::IsUnloading() const {
  return state_ == State::kUnloading;
}

bool RootfsLacrosLoader::IsUnloaded() const {
  return state_ == State::kUnloaded;
}

void RootfsLacrosLoader::GetVersionInternal(
    base::OnceCallback<void(const base::Version&)> callback) {
  CHECK_EQ(state_, State::kReadingVersion) << state_;
  CHECK(!version_);

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
  CHECK_EQ(state_, State::kReadingVersion) << state_;

  version_ = version;
  state_ = State::kVersionReadyButNotLoaded;

  if (pending_unload_) {
    LOG(WARNING) << "Unload is requested during getting version of rootfs.";
    std::move(callback).Run(base::Version());
    std::move(pending_unload_).Run();
    return;
  }

  std::move(callback).Run(version_.value());
}

void RootfsLacrosLoader::OnVersionReadyToLoad(LoadCompletionCallback callback,
                                              const base::Version& version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(state_, State::kVersionReadyButNotLoaded) << state_;

  if (pending_unload_) {
    LOG(WARNING) << "Unload is requested during loading rootfs.";
    std::move(callback).Run(base::Version(), base::FilePath());
    std::move(pending_unload_).Run();
    return;
  }

  // `version_` must be already filled by `version`.
  CHECK(version_.has_value() &&
        ((!version_.value().IsValid() && !version.IsValid()) ||
         (version_.value() == version)));

  state_ = State::kLoading;

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
  CHECK_EQ(state_, State::kLoading) << state_;

  if (pending_unload_) {
    LOG(WARNING) << "Unload is requested during loading rootfs.";
    state_ = State::kVersionReadyButNotLoaded;
    std::move(callback).Run(base::Version(), base::FilePath());
    std::move(pending_unload_).Run();
    return;
  }

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
  CHECK_EQ(state_, State::kLoading) << state_;
  state_ = State::kLoaded;

  LOG_IF(WARNING, !success) << "Upstart failed to mount rootfs lacros.";

  if (pending_unload_) {
    LOG(WARNING) << "Unload is requested during loading rootfs.";
    std::move(callback).Run(base::Version(), base::FilePath());
    std::move(pending_unload_).Run();
    return;
  }

  // `version_` must be calculated before coming here.
  CHECK(version_.has_value());
  std::move(callback).Run(
      version_.value(),
      // If mounting wasn't successful, return a empty mount point to indicate
      // failure. `OnLoadComplete` handles empty mount points and forwards the
      // errors on the return callbacks.
      success ? base::FilePath(kRootfsLacrosMountPoint) : base::FilePath());
}

void RootfsLacrosLoader::OnUnloadCompleted(base::OnceClosure callback,
                                           bool success) {
  // Proceed anyway regardless of unload success.
  if (!success) {
    LOG(ERROR) << "Failed to unload rootfs lacros";
  }

  CHECK_EQ(state_, State::kUnloading) << state_;
  state_ = State::kUnloaded;
  std::move(callback).Run();
}

std::ostream& operator<<(std::ostream& ostream,
                         RootfsLacrosLoader::State state) {
  switch (state) {
    case RootfsLacrosLoader::State::kNotLoaded:
      return ostream << "NotLoaded";
    case RootfsLacrosLoader::State::kReadingVersion:
      return ostream << "ReadingVersion";
    case RootfsLacrosLoader::State::kVersionReadyButNotLoaded:
      return ostream << "VersionReadyButNotLoaded";
    case RootfsLacrosLoader::State::kLoading:
      return ostream << "Loading";
    case RootfsLacrosLoader::State::kLoaded:
      return ostream << "Loaded";
    case RootfsLacrosLoader::State::kUnloading:
      return ostream << "Unloading";
    case RootfsLacrosLoader::State::kUnloaded:
      return ostream << "Unloaded";
  }
}

}  // namespace crosapi
