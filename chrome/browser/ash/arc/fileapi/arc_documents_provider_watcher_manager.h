// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_DOCUMENTS_PROVIDER_WATCHER_MANAGER_H_
#define CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_DOCUMENTS_PROVIDER_WATCHER_MANAGER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "storage/browser/file_system/watcher_manager.h"

namespace arc {

// The implementation of storage::WatcherManager for ARC documents provider
// filesystem.
//
// Note that this WatcherManager is not always correct. See comments at
// ArcDocumentsProviderRoot::AddWatcher().
//
// After destruction of this object, callbacks of pending operations will not
// be called.
class ArcDocumentsProviderWatcherManager : public storage::WatcherManager {
 public:
  ArcDocumentsProviderWatcherManager();
  ~ArcDocumentsProviderWatcherManager() override;

  // storage::WatcherManager overrides.
  void AddWatcher(const storage::FileSystemURL& url,
                  bool recursive,
                  StatusCallback callback,
                  NotificationCallback notification_callback) override;
  void RemoveWatcher(const storage::FileSystemURL& url,
                     bool recursive,
                     StatusCallback callback) override;

 private:
  void OnAddWatcher(StatusCallback callback, base::File::Error result);
  void OnRemoveWatcher(StatusCallback callback, base::File::Error result);
  void OnNotification(NotificationCallback notification_callback,
                      ChangeType change_type);

  base::WeakPtrFactory<ArcDocumentsProviderWatcherManager> weak_ptr_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(ArcDocumentsProviderWatcherManager);
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_DOCUMENTS_PROVIDER_WATCHER_MANAGER_H_
