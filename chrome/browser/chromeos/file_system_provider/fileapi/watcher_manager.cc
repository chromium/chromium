// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/fileapi/watcher_manager.h"

#include "base/files/file.h"
#include "base/task/post_task.h"
#include "chrome/browser/chromeos/file_system_provider/mount_path_util.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_interface.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/fileapi/file_system_url.h"

using content::BrowserThread;

namespace chromeos {
namespace file_system_provider {

namespace {

using StatusCallback = storage::WatcherManager::StatusCallback;
using NotificationCallback = storage::WatcherManager::NotificationCallback;
using ChangeType = storage::WatcherManager::ChangeType;

void CallStatusCallbackOnIOThread(const StatusCallback& callback,
                                  base::File::Error error) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::IO},
                           base::BindOnce(callback, error));
}

void CallNotificationCallbackOnIOThread(const NotificationCallback& callback,
                                        ChangeType type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::IO},
                           base::BindOnce(callback, type));
}

void AddWatcherOnUIThread(const storage::FileSystemURL& url,
                          bool recursive,
                          const StatusCallback& callback,
                          const NotificationCallback& notification_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  util::FileSystemURLParser parser(url);
  if (!parser.Parse()) {
    callback.Run(base::File::FILE_ERROR_SECURITY);
    return;
  }

  if (!parser.file_system()->GetFileSystemInfo().watchable()) {
    callback.Run(base::File::FILE_ERROR_INVALID_OPERATION);
    return;
  }

  parser.file_system()->AddWatcher(url.origin(),
                                   parser.file_path(),
                                   recursive,
                                   false /* persistent */,
                                   callback,
                                   notification_callback);
}

void RemoveWatcherOnUIThread(const storage::FileSystemURL& url,
                             bool recursive,
                             const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  util::FileSystemURLParser parser(url);
  if (!parser.Parse()) {
    callback.Run(base::File::FILE_ERROR_SECURITY);
    return;
  }

  if (!parser.file_system()->GetFileSystemInfo().watchable()) {
    callback.Run(base::File::FILE_ERROR_INVALID_OPERATION);
    return;
  }

  parser.file_system()->RemoveWatcher(
      url.origin(), parser.file_path(), recursive, callback);
}

}  // namespace

WatcherManager::WatcherManager() = default;
WatcherManager::~WatcherManager() = default;

void WatcherManager::AddWatcher(
    const storage::FileSystemURL& url,
    bool recursive,
    const StatusCallback& callback,
    const NotificationCallback& notification_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&AddWatcherOnUIThread, url, recursive,
                     base::Bind(&CallStatusCallbackOnIOThread, callback),
                     base::Bind(&CallNotificationCallbackOnIOThread,
                                notification_callback)));
}

void WatcherManager::RemoveWatcher(const storage::FileSystemURL& url,
                                   bool recursive,
                                   const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&RemoveWatcherOnUIThread, url, recursive,
                     base::Bind(&CallStatusCallbackOnIOThread, callback)));
}

}  // namespace file_system_provider
}  // namespace chromeos
