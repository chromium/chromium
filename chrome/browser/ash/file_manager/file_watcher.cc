// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/file_watcher.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "chrome/browser/ash/guest_os/guest_os_file_watcher.h"
#include "chrome/browser/ash/guest_os/public/guest_os_mount_provider.h"
#include "chrome/browser/ash/guest_os/public/guest_os_service.h"
#include "chrome/browser/ash/guest_os/public/guest_os_service_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/common/task_util.h"

using content::BrowserThread;

namespace file_manager {
namespace {

// Creates a base::FilePathWatcher and starts watching at |watch_path| with
// |callback|. Returns NULL on failure.
base::FilePathWatcher* CreateAndStartFilePathWatcher(
    const base::FilePath& watch_path,
    const base::FilePathWatcher::Callback& callback) {
  DCHECK(!callback.is_null());

  std::unique_ptr<base::FilePathWatcher> watcher(new base::FilePathWatcher);
  if (!watcher->Watch(watch_path, base::FilePathWatcher::Type::kNonRecursive,
                      callback)) {
    return nullptr;
  }

  return watcher.release();
}

std::unique_ptr<guest_os::GuestOsFileWatcher> GetForPath(
    Profile* profile,
    const base::FilePath& local_path) {
  // TODO(b/217469540): The default Crostini mount isn't using mount providers
  // yet so check for it explicitly and handle it differently.
  base::FilePath crostini_mount = util::GetCrostiniMountDirectory(profile);
  base::FilePath relative_path;
  if (local_path == crostini_mount ||
      crostini_mount.AppendRelativePath(local_path, &relative_path)) {
    return std::make_unique<guest_os::GuestOsFileWatcher>(
        ash::ProfileHelper::GetUserIdHashFromProfile(profile),
        crostini::DefaultContainerId(), std::move(crostini_mount),
        std::move(relative_path));
  }
  auto* service = guest_os::GuestOsServiceFactory::GetForProfile(profile);
  if (!service) {
    return nullptr;
  }
  auto* registry = service->MountProviderRegistry();
  for (const auto id : registry->List()) {
    auto* provider = registry->Get(id);
    base::FilePath mount_path = util::GetGuestOsMountDirectory(
        util::GetGuestOsMountPointName(profile, provider->GuestId()));
    if (local_path == mount_path ||
        mount_path.AppendRelativePath(local_path, &relative_path)) {
      return provider->CreateFileWatcher(std::move(mount_path),
                                         std::move(relative_path));
    }
  }
  return nullptr;
}
}  // namespace

FileWatcher::FileWatcher(const base::FilePath& virtual_path)
    : sequenced_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE})),
      virtual_path_(virtual_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

FileWatcher::~FileWatcher() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  sequenced_task_runner_->DeleteSoon(FROM_HERE, local_file_watcher_.get());
}

void FileWatcher::AddListener(const url::Origin& listener) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  origins_[listener]++;
}

void FileWatcher::RemoveListener(const url::Origin& listener) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto it = origins_.find(listener);
  if (it == origins_.end()) {
    LOG(ERROR) << " Listener [" << listener
               << "] tries to unsubscribe from virtual path ["
               << virtual_path_.value() << "] it isn't subscribed";
    return;
  }

  // If entry found - decrease it's count and remove if necessary
  --it->second;
  if (it->second == 0) {
    origins_.erase(it);
  }
}

std::vector<url::Origin> FileWatcher::GetListeners() const {
  std::vector<url::Origin> origins;
  for (const auto& kv : origins_) {
    origins.push_back(kv.first);
  }
  return origins;
}

void FileWatcher::WatchLocalFile(
    Profile* profile,
    const base::FilePath& local_path,
    const base::FilePathWatcher::Callback& file_watcher_callback,
    BoolCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());
  DCHECK(!local_file_watcher_);

  // If this is a crostini SSHFS path, use CrostiniFileWatcher.
  crostini_file_watcher_ = GetForPath(profile, local_path);
  if (crostini_file_watcher_) {
    crostini_file_watcher_->Watch(std::move(file_watcher_callback),
                                  std::move(callback));
    return;
  }

  sequenced_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&CreateAndStartFilePathWatcher, local_path,
                     google_apis::CreateRelayCallback(file_watcher_callback)),
      base::BindOnce(&FileWatcher::OnWatcherStarted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FileWatcher::OnWatcherStarted(BoolCallback callback,
                                   base::FilePathWatcher* file_watcher) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());
  DCHECK(!local_file_watcher_);

  if (file_watcher) {
    local_file_watcher_ = file_watcher;
    std::move(callback).Run(true);
  } else {
    std::move(callback).Run(false);
  }
}

}  // namespace file_manager
