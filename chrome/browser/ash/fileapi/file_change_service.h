// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILEAPI_FILE_CHANGE_SERVICE_H_
#define CHROME_BROWSER_ASH_FILEAPI_FILE_CHANGE_SERVICE_H_

#include "base/observer_list.h"
#include "chrome/browser/ash/fileapi/file_change_service_observer.h"
#include "components/keyed_service/core/keyed_service.h"

namespace ash {

// A service which notifies observers of file change events from external file
// systems. This serves as a bridge to allow for observation of file system
// changes across all file system contexts within a browser context.
class FileChangeService : public KeyedService {
 public:
  FileChangeService();
  FileChangeService(const FileChangeService& other) = delete;
  FileChangeService& operator=(const FileChangeService& other) = delete;
  ~FileChangeService() override;

  // Adds/removes the specified `observer` for service events.
  void AddObserver(FileChangeServiceObserver* observer);
  void RemoveObserver(FileChangeServiceObserver* observer);

  // Notifies the service that a file identified by `url` has been modified.
  void NotifyFileModified(const storage::FileSystemURL& url);

  // Notifies the service that a file has been copied from `src` to `dst`.
  void NotifyFileCopied(const storage::FileSystemURL& src,
                        const storage::FileSystemURL& dst);

  // Notifies the service that a file has been moved from `src` to `dst`.
  void NotifyFileMoved(const storage::FileSystemURL& src,
                       const storage::FileSystemURL& dst);

 private:
  base::ObserverList<FileChangeServiceObserver> observer_list_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_FILEAPI_FILE_CHANGE_SERVICE_H_
