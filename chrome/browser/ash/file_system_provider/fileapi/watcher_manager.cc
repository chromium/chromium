// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/fileapi/watcher_manager.h"

#include "base/files/file.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/file_system_provider/mount_path_util.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_interface.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/file_system/file_system_url.h"

using content::BrowserThread;

namespace ash::file_system_provider {

namespace {

using StatusCallback = storage::WatcherManager::StatusCallback;
using NotificationCallback = storage::WatcherManager::NotificationCallback;
using ChangeType = storage::WatcherManager::ChangeType;

void CallStatusCallbackOnIOThread(StatusCallback callback,
                                  base::File::Error error) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), error));
}

void CallNotificationCallbackOnIOThread(NotificationCallback callback,
                                        ChangeType type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), type));
}

void AddWatcherOnUIThread(const storage::FileSystemURL& url,
                          bool recursive,
                          StatusCallback callback,
                          NotificationCallback notification_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  util::FileSystemURLParser parser(url);
  if (!parser.Parse()) {
    std::move(callback).Run(base::File::FILE_ERROR_SECURITY);
    return;
  }

  if (!parser.file_system()->GetFileSystemInfo().watchable()) {
    std::move(callback).Run(base::File::FILE_ERROR_INVALID_OPERATION);
    return;
  }

  parser.file_system()->AddWatcher(url.origin().GetURL(), parser.file_path(),
                                   recursive, /*persistent=*/false,
                                   std::move(callback),
                                   std::move(notification_callback));
}

void RemoveWatcherOnUIThread(const storage::FileSystemURL& url,
                             bool recursive,
                             StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  util::FileSystemURLParser parser(url);
  if (!parser.Parse()) {
    std::move(callback).Run(base::File::FILE_ERROR_SECURITY);
    return;
  }

  if (!parser.file_system()->GetFileSystemInfo().watchable()) {
    std::move(callback).Run(base::File::FILE_ERROR_INVALID_OPERATION);
    return;
  }

  parser.file_system()->RemoveWatcher(url.origin().GetURL(), parser.file_path(),
                                      recursive, std::move(callback));
}

}  // namespace

WatcherManager::WatcherManager() = default;
WatcherManager::~WatcherManager() = default;

void WatcherManager::AddWatcher(const storage::FileSystemURL& url,
                                bool recursive,
                                StatusCallback callback,
                                NotificationCallback notification_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &AddWatcherOnUIThread, url, recursive,
          base::BindOnce(&CallStatusCallbackOnIOThread, std::move(callback)),
          base::BindRepeating(&CallNotificationCallbackOnIOThread,
                              std::move(notification_callback))));
}

void WatcherManager::RemoveWatcher(const storage::FileSystemURL& url,
                                   bool recursive,
                                   StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&RemoveWatcherOnUIThread, url, recursive,
                                base::BindOnce(&CallStatusCallbackOnIOThread,
                                               std::move(callback))));
}

}  // namespace ash::file_system_provider
