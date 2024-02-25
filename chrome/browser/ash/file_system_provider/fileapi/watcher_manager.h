// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_FILEAPI_WATCHER_MANAGER_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_FILEAPI_WATCHER_MANAGER_H_

#include "storage/browser/file_system/watcher_manager.h"

namespace storage {
class FileSystemURL;
}  // namespace storage

namespace ash::file_system_provider {

// Exposes entry watching capability to fileapi.
class WatcherManager : public storage::WatcherManager {
 public:
  WatcherManager();
  ~WatcherManager() override;

  // storage::WatcherManager overrides.
  void AddWatcher(const storage::FileSystemURL& url,
                  bool recursive,
                  StatusCallback callback,
                  NotificationCallback notification_callback) override;
  void RemoveWatcher(const storage::FileSystemURL& url,
                     bool recursive,
                     StatusCallback callback) override;
};

}  // namespace ash::file_system_provider

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_FILEAPI_WATCHER_MANAGER_H_
