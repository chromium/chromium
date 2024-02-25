// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_SNAPSHOT_MANAGER_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_SNAPSHOT_MANAGER_H_

#include <stdint.h>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"

class Profile;

namespace base {
class FilePath;
}  // namespace base

namespace storage {
class FileSystemURL;
}  // namespace storage

namespace file_manager {

// Utility class for creating a snapshot of a file system file on local disk.
// The class wraps the underlying implementation of fileapi's CreateSnapshotFile
// and prolongs the lifetime of snapshot files so that the client code that just
// accepts file paths works without problems.
class SnapshotManager {
 public:
  // The callback type for CreateManagedSnapshot.
  typedef base::OnceCallback<void(const base::FilePath&)> LocalPathCallback;

  explicit SnapshotManager(Profile* profile);

  SnapshotManager(const SnapshotManager&) = delete;
  SnapshotManager& operator=(const SnapshotManager&) = delete;

  ~SnapshotManager();

  // Creates a snapshot file copy of a file system file |absolute_file_path| and
  // returns back to |callback|. Returns empty path for failure.
  void CreateManagedSnapshot(const base::FilePath& absolute_file_path,
                             LocalPathCallback callback);

 private:
  class FileRefsHolder;

  // Part of CreateManagedSnapshot.
  void CreateManagedSnapshotAfterSpaceComputed(
      const storage::FileSystemURL& filesystem_url,
      LocalPathCallback callback,
      int64_t needed_space);

  raw_ptr<Profile> profile_;
  scoped_refptr<FileRefsHolder> holder_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<SnapshotManager> weak_ptr_factory_{this};
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_SNAPSHOT_MANAGER_H_
