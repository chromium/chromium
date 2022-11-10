// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILEAPI_FILE_CHANGE_SERVICE_OBSERVER_H_
#define CHROME_BROWSER_ASH_FILEAPI_FILE_CHANGE_SERVICE_OBSERVER_H_

#include "base/observer_list_types.h"

namespace storage {
class FileSystemURL;
}  // namespace storage

namespace ash {

// An interface for an observer which receives `FileChangeService` events.
class FileChangeServiceObserver : public base::CheckedObserver {
 public:
  // Invoked when a file identified by `url` has been modified. Note that this
  // will not get called on file creation or deletion.
  virtual void OnFileModified(const storage::FileSystemURL& url) {}

  // Invoked when a file has been copied from `src` to `dst`.
  virtual void OnFileCopied(const storage::FileSystemURL& src,
                            const storage::FileSystemURL& dst) {}

  // Invoked when a file has been moved from `src` to `dst`.
  virtual void OnFileMoved(const storage::FileSystemURL& src,
                           const storage::FileSystemURL& dst) {}
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_FILEAPI_FILE_CHANGE_SERVICE_OBSERVER_H_
