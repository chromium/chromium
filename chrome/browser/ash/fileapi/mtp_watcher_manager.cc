// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/fileapi/mtp_watcher_manager.h"

#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace ash {

MTPWatcherManager::MTPWatcherManager(
    DeviceMediaAsyncFileUtil* device_media_async_file_util)
    : device_media_async_file_util_(device_media_async_file_util) {
  CHECK(device_media_async_file_util != nullptr, base::NotFatalUntil::M160);
}

MTPWatcherManager::~MTPWatcherManager() = default;

void MTPWatcherManager::AddWatcher(const storage::FileSystemURL& url,
                                   bool recursive,
                                   StatusCallback callback,
                                   NotificationCallback notification_callback) {
  CHECK_CURRENTLY_ON(BrowserThread::IO, base::NotFatalUntil::M160);
  device_media_async_file_util_->AddWatcher(url, recursive, std::move(callback),
                                            std::move(notification_callback));
}

void MTPWatcherManager::RemoveWatcher(const storage::FileSystemURL& url,
                                      bool recursive,
                                      StatusCallback callback) {
  CHECK_CURRENTLY_ON(BrowserThread::IO, base::NotFatalUntil::M160);
  device_media_async_file_util_->RemoveWatcher(url, recursive,
                                               std::move(callback));
}

}  // namespace ash
