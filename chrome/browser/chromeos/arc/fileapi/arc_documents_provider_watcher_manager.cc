// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/fileapi/arc_documents_provider_watcher_manager.h"

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/task/post_task.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_documents_provider_root.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_documents_provider_root_map.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/fileapi/file_system_url.h"

using content::BrowserThread;
using storage::FileSystemURL;

namespace arc {

namespace {

void OnAddWatcherOnUIThread(
    const ArcDocumentsProviderRoot::StatusCallback& callback,
    base::File::Error result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::IO},
                           base::BindOnce(callback, result));
}

void OnRemoveWatcherOnUIThread(
    const ArcDocumentsProviderRoot::StatusCallback& callback,
    base::File::Error result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::IO},
                           base::BindOnce(callback, result));
}

void OnNotificationOnUIThread(
    const ArcDocumentsProviderRoot::WatcherCallback& notification_callback,
    ArcDocumentsProviderRoot::ChangeType change_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::IO},
                           base::BindOnce(notification_callback, change_type));
}

void AddWatcherOnUIThread(
    const storage::FileSystemURL& url,
    const ArcDocumentsProviderRoot::StatusCallback& callback,
    const ArcDocumentsProviderRoot::WatcherCallback& notification_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ArcDocumentsProviderRootMap* roots =
      ArcDocumentsProviderRootMap::GetForArcBrowserContext();
  if (!roots) {
    OnAddWatcherOnUIThread(callback, base::File::FILE_ERROR_SECURITY);
    return;
  }

  base::FilePath path;
  ArcDocumentsProviderRoot* root = roots->ParseAndLookup(url, &path);
  if (!root) {
    OnAddWatcherOnUIThread(callback, base::File::FILE_ERROR_NOT_FOUND);
    return;
  }

  root->AddWatcher(path,
                   base::Bind(&OnNotificationOnUIThread, notification_callback),
                   base::Bind(&OnAddWatcherOnUIThread, callback));
}

void RemoveWatcherOnUIThread(
    const storage::FileSystemURL& url,
    const ArcDocumentsProviderRoot::StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ArcDocumentsProviderRootMap* roots =
      ArcDocumentsProviderRootMap::GetForArcBrowserContext();
  if (!roots) {
    OnRemoveWatcherOnUIThread(callback, base::File::FILE_ERROR_SECURITY);
    return;
  }

  base::FilePath path;
  ArcDocumentsProviderRoot* root = roots->ParseAndLookup(url, &path);
  if (!root) {
    OnRemoveWatcherOnUIThread(callback, base::File::FILE_ERROR_NOT_FOUND);
    return;
  }

  root->RemoveWatcher(path, base::Bind(&OnRemoveWatcherOnUIThread, callback));
}

}  // namespace

ArcDocumentsProviderWatcherManager::ArcDocumentsProviderWatcherManager()
    : weak_ptr_factory_(this) {}

ArcDocumentsProviderWatcherManager::~ArcDocumentsProviderWatcherManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void ArcDocumentsProviderWatcherManager::AddWatcher(
    const FileSystemURL& url,
    bool recursive,
    const StatusCallback& callback,
    const NotificationCallback& notification_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (recursive) {
    // Recursive watching is not supported.
    callback.Run(base::File::FILE_ERROR_INVALID_OPERATION);
    return;
  }

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(
          &AddWatcherOnUIThread, url,
          base::Bind(&ArcDocumentsProviderWatcherManager::OnAddWatcher,
                     weak_ptr_factory_.GetWeakPtr(), callback),
          base::Bind(&ArcDocumentsProviderWatcherManager::OnNotification,
                     weak_ptr_factory_.GetWeakPtr(), notification_callback)));
}

void ArcDocumentsProviderWatcherManager::RemoveWatcher(
    const FileSystemURL& url,
    bool recursive,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (recursive) {
    // Recursive watching is not supported.
    callback.Run(base::File::FILE_ERROR_INVALID_OPERATION);
    return;
  }

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(
          &RemoveWatcherOnUIThread, url,
          base::Bind(&ArcDocumentsProviderWatcherManager::OnRemoveWatcher,
                     weak_ptr_factory_.GetWeakPtr(), callback)));
}

void ArcDocumentsProviderWatcherManager::OnAddWatcher(
    const StatusCallback& callback,
    base::File::Error result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  callback.Run(result);
}

void ArcDocumentsProviderWatcherManager::OnRemoveWatcher(
    const StatusCallback& callback,
    base::File::Error result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  callback.Run(result);
}

void ArcDocumentsProviderWatcherManager::OnNotification(
    const NotificationCallback& notification_callback,
    ChangeType change_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  notification_callback.Run(change_type);
}

}  // namespace arc
