// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_LOCAL_LOCAL_FILE_CHANGE_TRACKER_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_LOCAL_LOCAL_FILE_CHANGE_TRACKER_H_

#include <stdint.h>

#include <map>
#include <memory>

#include "base/containers/circular_deque.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/sync_file_system/file_change.h"
#include "chrome/browser/sync_file_system/sync_status_code.h"
#include "storage/browser/file_system/file_observers.h"
#include "storage/browser/file_system/file_system_url.h"

namespace base {
class SequencedTaskRunner;
}

namespace storage {
class FileSystemContext;
class FileSystemURL;
}

namespace leveldb {
class Env;
class WriteBatch;
}

namespace sync_file_system {

// Tracks local file changes for cloud-backed file systems.
// All methods must be called on the file_task_runner given to the constructor.
// Owned by FileSystemContext.
class LocalFileChangeTracker : public storage::FileUpdateObserver,
                               public storage::FileChangeObserver {
 public:
  // |file_task_runner| must be the one where the observee file operations run.
  // (So that we can make sure DB operations are done before actual update
  // happens)
  LocalFileChangeTracker(const base::FilePath& base_path,
                         leveldb::Env* env_override,
                         base::SequencedTaskRunner* file_task_runner);

  LocalFileChangeTracker(const LocalFileChangeTracker&) = delete;
  LocalFileChangeTracker& operator=(const LocalFileChangeTracker&) = delete;

  ~LocalFileChangeTracker() override;

  // FileUpdateObserver overrides.
  void OnStartUpdate(const storage::FileSystemURL& url) override;
  void OnUpdate(const storage::FileSystemURL& url, int64_t delta) override {}
  void OnEndUpdate(const storage::FileSystemURL& url) override;

  // FileChangeObserver overrides.
  void OnCreateFile(const storage::FileSystemURL& url) override;
  void OnCreateFileFrom(const storage::FileSystemURL& url,
                        const storage::FileSystemURL& src) override;
  void OnMoveFileFrom(const storage::FileSystemURL& url,
                      const storage::FileSystemURL& src) override;
  void OnRemoveFile(const storage::FileSystemURL& url) override;
  void OnModifyFile(const storage::FileSystemURL& url) override;
  void OnCreateDirectory(const storage::FileSystemURL& url) override;
  void OnRemoveDirectory(const storage::FileSystemURL& url) override;

  // Retrieves an array of |url| which have more than one pending changes.
  // If |max_urls| is non-zero (recommended in production code) this
  // returns URLs up to the number from the ones that have smallest
  // change_seq numbers (i.e. older changes).
  void GetNextChangedURLs(base::circular_deque<storage::FileSystemURL>* urls,
                          int max_urls);

  // Returns all changes recorded for the given |url|.
  // Note that this also returns demoted changes.
  // This should be called after writing is disabled.
  void GetChangesForURL(const storage::FileSystemURL& url,
                        FileChangeList* changes);

  // Clears the pending changes recorded in this tracker for |url|.
  void ClearChangesForURL(const storage::FileSystemURL& url);

  // Creates a fresh (empty) in-memory record for |url|.
  // Note that new changes are recorded to the mirror too.
  void CreateFreshMirrorForURL(const storage::FileSystemURL& url);

  // Removes a mirror for |url|, and commits the change status to database.
  void RemoveMirrorAndCommitChangesForURL(const storage::FileSystemURL& url);

  // Resets the changes to the ones recorded in mirror for |url|, and
  // commits the updated change status to database.
  void ResetToMirrorAndCommitChangesForURL(const storage::FileSystemURL& url);

  // Re-inserts changes into the separate demoted_changes_ queue. They won't
  // be fetched by GetNextChangedURLs() unless PromoteDemotedChanges() is
  // called.
  void DemoteChangesForURL(const storage::FileSystemURL& url);

  // Promotes demoted changes for |url| to the normal queue.
  void PromoteDemotedChangesForURL(const storage::FileSystemURL& url);

  // Promotes all demoted changes to the normal queue. Returns true if it has
  // promoted any changes.
  bool PromoteDemotedChanges();

  // Called by FileSyncService at the startup time to restore last dirty changes
  // left after the last shutdown (if any).
  SyncStatusCode Initialize(storage::FileSystemContext* file_system_context);

  // Resets all the changes recorded for the given |origin| and |type|.
  // TODO(kinuko,nhiroki): Ideally this should be automatically called in
  // DeleteFileSystem via QuotaUtil::DeleteOriginDataOnFileThread.
  void ResetForFileSystem(const GURL& origin, storage::FileSystemType type);

  // This method is (exceptionally) thread-safe.
  int64_t num_changes() const {
    base::AutoLock lock(num_changes_lock_);
    return num_changes_;
  }

 private:
  class TrackerDB;
  friend class CannedSyncableFileSystem;
  friend class LocalFileChangeTrackerTest;
  friend class LocalFileSyncContext;
  friend class LocalFileSyncContextTest;
  friend class SyncableFileSystemTest;

  struct ChangeInfo {
    ChangeInfo();
    ~ChangeInfo();
    FileChangeList change_list;
    int64_t change_seq;
  };

  typedef std::map<storage::FileSystemURL,
                   ChangeInfo,
                   storage::FileSystemURL::Comparator> FileChangeMap;
  typedef std::map<int64_t, storage::FileSystemURL> ChangeSeqMap;

  void UpdateNumChanges();

  // This does mostly same as calling GetNextChangedURLs with max_url=0
  // except that it returns urls in set rather than in deque.
  // Used only in testings.
  void GetAllChangedURLs(storage::FileSystemURLSet* urls);

  // Used only in testings.
  void DropAllChanges();

  // Database related methods.
  SyncStatusCode MarkDirtyOnDatabase(const storage::FileSystemURL& url);
  SyncStatusCode ClearDirtyOnDatabase(const storage::FileSystemURL& url);

  SyncStatusCode CollectLastDirtyChanges(
      storage::FileSystemContext* file_system_context);
  void RecordChange(const storage::FileSystemURL& url,
                    const FileChange& change);

  static void RecordChangeToChangeMaps(const storage::FileSystemURL& url,
                                       const FileChange& change,
                                       int change_seq,
                                       FileChangeMap* changes,
                                       ChangeSeqMap* change_seqs);

  void ResetForURL(const storage::FileSystemURL& url,
                   int change_seq,
                   leveldb::WriteBatch* batch);

  bool initialized_;

  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  FileChangeMap changes_;
  ChangeSeqMap change_seqs_;

  FileChangeMap mirror_changes_;  // For mirrors.
  FileChangeMap demoted_changes_;  // For demoted changes.

  std::unique_ptr<TrackerDB> tracker_db_;

  // Change sequence number. Briefly gives a hint about the order of changes,
  // but they are updated when a new change comes on the same file (as
  // well as Drive's changestamps).
  int64_t current_change_seq_number_;

  // This can be accessed on any threads (with num_changes_lock_).
  int64_t num_changes_;
  mutable base::Lock num_changes_lock_;
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_LOCAL_LOCAL_FILE_CHANGE_TRACKER_H_
