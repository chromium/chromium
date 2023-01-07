// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_REMOTE_CHANGE_PROCESSOR_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_REMOTE_CHANGE_PROCESSOR_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/sync_file_system/sync_callbacks.h"
#include "chrome/browser/sync_file_system/sync_status_code.h"

namespace base {
class FilePath;
}

namespace storage {
class FileSystemURL;
}

namespace sync_file_system {

class FileChange;
class FileChangeList;
class SyncFileMetadata;

// Represents an interface to process one remote change and applies
// it to the local file system.
// This interface is to be implemented/backed by LocalSyncFileService.
class RemoteChangeProcessor {
 public:
  // Callback type for PrepareForProcessRemoteChange.
  // |file_type| indicates the current file/directory type of the target
  // URL in the local filesystem. If the target URL does not exist it is
  // set to SYNC_FILE_TYPE_UNKNOWN.
  // |changes| indicates a set of pending changes for the target URL.
  using PrepareChangeCallback =
      base::OnceCallback<void(SyncStatusCode status,
                              const SyncFileMetadata& metadata,
                              const FileChangeList& changes)>;

  RemoteChangeProcessor() {}

  RemoteChangeProcessor(const RemoteChangeProcessor&) = delete;
  RemoteChangeProcessor& operator=(const RemoteChangeProcessor&) = delete;

  virtual ~RemoteChangeProcessor() {}

  // This must be called before processing the change for the |url|.
  // This tries to lock the target |url| and returns the local changes
  // if any. (The change returned by the callback is to make a decision
  // on conflict resolution, but NOT for applying local changes to the remote,
  // which is supposed to be done by LocalChangeProcessor)
  virtual void PrepareForProcessRemoteChange(
      const storage::FileSystemURL& url,
      PrepareChangeCallback callback) = 0;

  // This is called to apply the remote |change|. If the change type is
  // ADD_OR_UPDATE for a file, |local_path| needs to point to a
  // local file path that contains the latest file image (e.g. a path
  // to a temporary file which has the data downloaded from the server).
  // This may fail with an error but should NOT result in a conflict
  // (as we must have checked the change status in PrepareRemoteSync and
  // have disabled any further writing).
  virtual void ApplyRemoteChange(const FileChange& change,
                                 const base::FilePath& local_path,
                                 const storage::FileSystemURL& url,
                                 SyncStatusCallback callback) = 0;

  // Finalizes the remote sync started by PrepareForProcessRemoteChange.
  // This clears sync flag on |url| to unlock the file for future writes/sync.
  // Clears all local changes if |clear_local_changes| is true.
  // This should be set to true when the remote sync service reconciled or
  // processed the existing local changes while processing a remote change.
  virtual void FinalizeRemoteSync(const storage::FileSystemURL& url,
                                  bool clear_local_changes,
                                  base::OnceClosure completion_callback) = 0;

  // Records a fake local change so that the change will be processed in the
  // next local sync.
  // This is called when the remote side wants to trigger a local sync
  // to propagate the local change to the remote change (e.g. to
  // resolve a conflict by uploading the local file).
  virtual void RecordFakeLocalChange(const storage::FileSystemURL& url,
                                     const FileChange& change,
                                     SyncStatusCallback callback) = 0;
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_REMOTE_CHANGE_PROCESSOR_H_
