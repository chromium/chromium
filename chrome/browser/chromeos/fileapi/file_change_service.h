// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILEAPI_FILE_CHANGE_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_FILEAPI_FILE_CHANGE_SERVICE_H_

#include "base/observer_list.h"
#include "components/keyed_service/core/keyed_service.h"
#include "storage/browser/file_system/file_observers.h"

namespace chromeos {

// A service which notifies observers of file change events from external file
// systems. This serves as a bridge to allow for observation of file system
// changes across all file system contexts within a browser context.
class FileChangeService : public KeyedService,
                          public storage::FileChangeObserver {
 public:
  FileChangeService();
  FileChangeService(const FileChangeService& other) = delete;
  FileChangeService& operator=(const FileChangeService& other) = delete;
  ~FileChangeService() override;

  // Adds/removes the specified `observer` for file change events.
  void AddObserver(storage::FileChangeObserver* observer);
  void RemoveObserver(storage::FileChangeObserver* observer);

 private:
  // storage::FileChangeObserver:
  void OnCreateFile(const storage::FileSystemURL& url) override;
  void OnCreateFileFrom(const storage::FileSystemURL& url,
                        const storage::FileSystemURL& src) override;
  void OnRemoveFile(const storage::FileSystemURL& url) override;
  void OnModifyFile(const storage::FileSystemURL& url) override;
  void OnCreateDirectory(const storage::FileSystemURL& url) override;
  void OnRemoveDirectory(const storage::FileSystemURL& url) override;

  base::ObserverList<storage::FileChangeObserver>::Unchecked observer_list_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILEAPI_FILE_CHANGE_SERVICE_H_
