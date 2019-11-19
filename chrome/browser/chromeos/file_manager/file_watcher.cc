// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/file_watcher.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/drive/task_util.h"

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
  if (!watcher->Watch(watch_path, false /* recursive */, callback))
    return nullptr;

  return watcher.release();
}

}  // namespace

FileWatcher::FileWatcher(const base::FilePath& virtual_path)
    : sequenced_task_runner_(
          base::CreateSequencedTaskRunner({base::ThreadPool(), base::MayBlock(),
                                           base::TaskPriority::USER_VISIBLE})),
      local_file_watcher_(nullptr),
      virtual_path_(virtual_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

FileWatcher::~FileWatcher() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  sequenced_task_runner_->DeleteSoon(FROM_HERE, local_file_watcher_);
}

void FileWatcher::AddExtension(const std::string& extension_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  extensions_[extension_id]++;
}

void FileWatcher::RemoveExtension(const std::string& extension_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ExtensionCountMap::iterator it = extensions_.find(extension_id);
  if (it == extensions_.end()) {
    LOG(ERROR) << " Extension [" << extension_id
               << "] tries to unsubscribe from folder ["
               << virtual_path_.value()
               << "] it isn't subscribed";
    return;
  }

  // If entry found - decrease it's count and remove if necessary
  --it->second;
  if (it->second == 0)
    extensions_.erase(it);
}

std::vector<std::string> FileWatcher::GetExtensionIds() const {
  std::vector<std::string> extension_ids;
  for (ExtensionCountMap::const_iterator iter = extensions_.begin();
       iter != extensions_.end(); ++iter) {
    extension_ids.push_back(iter->first);
  }
  return extension_ids;
}

void FileWatcher::WatchLocalFile(
    const base::FilePath& local_path,
    const base::FilePathWatcher::Callback& file_watcher_callback,
    BoolCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());
  DCHECK(!local_file_watcher_);

  base::PostTaskAndReplyWithResult(
      sequenced_task_runner_.get(), FROM_HERE,
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
