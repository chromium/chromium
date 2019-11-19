// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_FILEAPI_WATCHER_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_FILEAPI_WATCHER_MANAGER_H_

#include "storage/browser/file_system/watcher_manager.h"

namespace storage {
class FileSystemURL;
}  // namespace storage

namespace chromeos {
namespace file_system_provider {

// Exposes entry watching capability to fileapi.
class WatcherManager : public storage::WatcherManager {
 public:
  WatcherManager();
  ~WatcherManager() override;

  // storage::WatcherManager overrides.
  void AddWatcher(const storage::FileSystemURL& url,
                  bool recursive,
                  const StatusCallback& callback,
                  const NotificationCallback& notification_callback) override;
  void RemoveWatcher(const storage::FileSystemURL& url,
                     bool recursive,
                     const StatusCallback& callback) override;
};

}  // namespace file_system_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_FILEAPI_WATCHER_MANAGER_H_
