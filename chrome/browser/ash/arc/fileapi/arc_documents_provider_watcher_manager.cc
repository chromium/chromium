// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_watcher_manager.h"

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_root.h"
#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_root_map.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/file_system/file_system_url.h"

using content::BrowserThread;
using storage::FileSystemURL;

namespace arc {

namespace {

void OnAddWatcherOnUIThread(storage::WatcherManager::StatusCallback callback,
                            base::File::Error result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

void OnRemoveWatcherOnUIThread(storage::WatcherManager::StatusCallback callback,
                               base::File::Error result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

void OnNotificationOnUIThread(
    storage::WatcherManager::NotificationCallback notification_callback,
    ArcDocumentsProviderRoot::ChangeType change_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(notification_callback), change_type));
}

void AddWatcherOnUIThread(
    const storage::FileSystemURL& url,
    storage::WatcherManager::StatusCallback callback,
    storage::WatcherManager::NotificationCallback notification_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ArcDocumentsProviderRootMap* roots =
      ArcDocumentsProviderRootMap::GetForArcBrowserContext();
  if (!roots) {
    OnAddWatcherOnUIThread(std::move(callback),
                           base::File::FILE_ERROR_SECURITY);
    return;
  }

  base::FilePath path;
  ArcDocumentsProviderRoot* root = roots->ParseAndLookup(url, &path);
  if (!root) {
    OnAddWatcherOnUIThread(std::move(callback),
                           base::File::FILE_ERROR_NOT_FOUND);
    return;
  }

  root->AddWatcher(
      path,
      base::BindRepeating(&OnNotificationOnUIThread,
                          std::move(notification_callback)),
      base::BindOnce(&OnAddWatcherOnUIThread, std::move(callback)));
}

void RemoveWatcherOnUIThread(
    const storage::FileSystemURL& url,
    ArcDocumentsProviderRoot::WatcherStatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ArcDocumentsProviderRootMap* roots =
      ArcDocumentsProviderRootMap::GetForArcBrowserContext();
  if (!roots) {
    OnRemoveWatcherOnUIThread(std::move(callback),
                              base::File::FILE_ERROR_SECURITY);
    return;
  }

  base::FilePath path;
  ArcDocumentsProviderRoot* root = roots->ParseAndLookup(url, &path);
  if (!root) {
    OnRemoveWatcherOnUIThread(std::move(callback),
                              base::File::FILE_ERROR_NOT_FOUND);
    return;
  }

  root->RemoveWatcher(
      path, base::BindOnce(&OnRemoveWatcherOnUIThread, std::move(callback)));
}

}  // namespace

ArcDocumentsProviderWatcherManager::ArcDocumentsProviderWatcherManager() {}

ArcDocumentsProviderWatcherManager::~ArcDocumentsProviderWatcherManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void ArcDocumentsProviderWatcherManager::AddWatcher(
    const FileSystemURL& url,
    bool recursive,
    StatusCallback callback,
    NotificationCallback notification_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (recursive) {
    // Recursive watching is not supported.
    std::move(callback).Run(base::File::FILE_ERROR_INVALID_OPERATION);
    return;
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &AddWatcherOnUIThread, url,
          base::BindOnce(&ArcDocumentsProviderWatcherManager::OnAddWatcher,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
          base::BindRepeating(
              &ArcDocumentsProviderWatcherManager::OnNotification,
              weak_ptr_factory_.GetWeakPtr(),
              std::move(notification_callback))));
}

void ArcDocumentsProviderWatcherManager::RemoveWatcher(
    const FileSystemURL& url,
    bool recursive,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (recursive) {
    // Recursive watching is not supported.
    std::move(callback).Run(base::File::FILE_ERROR_INVALID_OPERATION);
    return;
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &RemoveWatcherOnUIThread, url,
          base::BindOnce(&ArcDocumentsProviderWatcherManager::OnRemoveWatcher,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback))));
}

void ArcDocumentsProviderWatcherManager::OnAddWatcher(
    StatusCallback callback,
    base::File::Error result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  std::move(callback).Run(result);
}

void ArcDocumentsProviderWatcherManager::OnRemoveWatcher(
    StatusCallback callback,
    base::File::Error result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  std::move(callback).Run(result);
}

void ArcDocumentsProviderWatcherManager::OnNotification(
    NotificationCallback notification_callback,
    ChangeType change_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  std::move(notification_callback).Run(change_type);
}

}  // namespace arc
