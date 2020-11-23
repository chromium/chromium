// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/fileapi/file_change_service.h"

namespace chromeos {

FileChangeService::FileChangeService() = default;

FileChangeService::~FileChangeService() {
  DCHECK(!observer_list_.might_have_observers());
}

void FileChangeService::AddObserver(storage::FileChangeObserver* observer) {
  observer_list_.AddObserver(observer);
}

void FileChangeService::RemoveObserver(storage::FileChangeObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void FileChangeService::OnCreateFile(const storage::FileSystemURL& url) {
  for (storage::FileChangeObserver& observer : observer_list_)
    observer.OnCreateFile(url);
}

void FileChangeService::OnCreateFileFrom(const storage::FileSystemURL& url,
                                         const storage::FileSystemURL& src) {
  for (storage::FileChangeObserver& observer : observer_list_)
    observer.OnCreateFileFrom(url, src);
}

void FileChangeService::OnRemoveFile(const storage::FileSystemURL& url) {
  for (storage::FileChangeObserver& observer : observer_list_)
    observer.OnRemoveFile(url);
}

void FileChangeService::OnModifyFile(const storage::FileSystemURL& url) {
  for (storage::FileChangeObserver& observer : observer_list_)
    observer.OnModifyFile(url);
}

void FileChangeService::OnCreateDirectory(const storage::FileSystemURL& url) {
  for (storage::FileChangeObserver& observer : observer_list_)
    observer.OnCreateDirectory(url);
}

void FileChangeService::OnRemoveDirectory(const storage::FileSystemURL& url) {
  for (storage::FileChangeObserver& observer : observer_list_)
    observer.OnRemoveDirectory(url);
}

}  // namespace chromeos
