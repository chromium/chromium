// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/fileapi/file_change_service.h"
#include "chrome/browser/file_system_access/chrome_file_system_access_permission_context.h"
#include "chrome/browser/file_system_access/file_system_access_permission_context_factory.h"
#include "chrome/browser/profiles/profile.h"

namespace ash {

FileChangeService::FileChangeService(Profile* profile) {
  file_created_from_show_save_file_picker_subscription_ =
      FileSystemAccessPermissionContextFactory::GetForProfile(profile)
          ->AddFileCreatedFromShowSaveFilePickerCallback(base::BindRepeating(
              &FileChangeService::NotifyFileCreatedFromShowSaveFilePicker,
              base::Unretained(this)));
}

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

void FileChangeService::NotifyFileMoved(const storage::FileSystemURL& src,
                                        const storage::FileSystemURL& dst) {
  for (FileChangeServiceObserver& observer : observer_list_)
    observer.OnFileMoved(src, dst);
}

void FileChangeService::NotifyFileCreatedFromShowSaveFilePicker(
    const GURL& file_picker_binding_context,
    const storage::FileSystemURL& url) {
  for (FileChangeServiceObserver& observer : observer_list_) {
    observer.OnFileCreatedFromShowSaveFilePicker(file_picker_binding_context,
                                                 url);
  }
}

}  // namespace ash
