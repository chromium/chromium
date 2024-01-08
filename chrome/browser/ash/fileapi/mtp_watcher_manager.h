// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILEAPI_MTP_WATCHER_MANAGER_H_
#define CHROME_BROWSER_ASH_FILEAPI_MTP_WATCHER_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/media_galleries/fileapi/device_media_async_file_util.h"
#include "storage/browser/file_system/watcher_manager.h"

namespace storage {

class FileSystemURL;

}  // namespace storage

namespace ash {

class MTPWatcherManager : public storage::WatcherManager {
 public:
  explicit MTPWatcherManager(
      DeviceMediaAsyncFileUtil* device_media_async_file_util);
  ~MTPWatcherManager() override;

  void AddWatcher(const storage::FileSystemURL& url,
                  bool recursive,
                  StatusCallback callback,
                  NotificationCallback notification_callback) override;

  void RemoveWatcher(const storage::FileSystemURL& url,
                     bool recursive,
                     StatusCallback callback) override;

 private:
  const raw_ptr<DeviceMediaAsyncFileUtil> device_media_async_file_util_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_FILEAPI_MTP_WATCHER_MANAGER_H_
