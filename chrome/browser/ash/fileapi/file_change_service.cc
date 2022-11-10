// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/fileapi/file_change_service.h"

namespace ash {

FileChangeService::FileChangeService() = default;

FileChangeService::~FileChangeService() = default;

void FileChangeService::AddObserver(FileChangeServiceObserver* observer) {
  observer_list_.AddObserver(observer);
}

void FileChangeService::RemoveObserver(FileChangeServiceObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void FileChangeService::NotifyFileModified(const storage::FileSystemURL& url) {
  for (FileChangeServiceObserver& observer : observer_list_)
    observer.OnFileModified(url);
}

void FileChangeService::NotifyFileCopied(const storage::FileSystemURL& src,
                                         const storage::FileSystemURL& dst) {
  for (FileChangeServiceObserver& observer : observer_list_)
    observer.OnFileCopied(src, dst);
}

void FileChangeService::NotifyFileMoved(const storage::FileSystemURL& src,
                                        const storage::FileSystemURL& dst) {
  for (FileChangeServiceObserver& observer : observer_list_)
    observer.OnFileMoved(src, dst);
}

}  // namespace ash
