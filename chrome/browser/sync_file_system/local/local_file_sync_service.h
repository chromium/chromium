// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_LOCAL_LOCAL_FILE_SYNC_SERVICE_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_LOCAL_LOCAL_FILE_SYNC_SERVICE_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/sync_file_system/local/local_origin_change_observer.h"
#include "chrome/browser/sync_file_system/remote_change_processor.h"
#include "chrome/browser/sync_file_system/sync_callbacks.h"
#include "chrome/browser/sync_file_system/sync_status_code.h"

class GURL;
class Profile;

namespace storage {
class FileSystemContext;
}

namespace leveldb {
class Env;
}

namespace storage {
class ScopedFile;
}

namespace sync_file_system {

class FileChange;
class LocalChangeProcessor;
class LocalFileSyncContext;
struct LocalFileSyncInfo;

// Maintains local file change tracker and sync status.
// Owned by SyncFileSystemService (which is a per-profile object).
class LocalFileSyncService final : public RemoteChangeProcessor,
                                   public LocalOriginChangeObserver {
 public:
  typedef base::RepeatingCallback<LocalChangeProcessor*(const GURL& origin)>
      GetLocalChangeProcessorCallback;

  class Observer {
   public:
    Observer() {}

    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;

    virtual ~Observer() {}

    // This is called when there're one or more local changes available.
    // |pending_changes_hint| indicates the pending queue length to help sync
    // scheduling but the value may not be accurately reflect the real-time
    // value.
    virtual void OnLocalChangeAvailable(int64_t pending_changes_hint) = 0;
  };

  typedef base::OnceCallback<void(SyncStatusCode status,
                                  bool has_pending_changes)>
      HasPendingLocalChangeCallback;

  static std::unique_ptr<LocalFileSyncService> Create(Profile* profile);
  static std::unique_ptr<LocalFileSyncService> CreateForTesting(
      Profile* profile,
      leveldb::Env* env_override);

  LocalFileSyncService(const LocalFileSyncService&) = delete;
  LocalFileSyncService& operator=(const LocalFileSyncService&) = delete;

  ~LocalFileSyncService() override;

  void Shutdown();

  void MaybeInitializeFileSystemContext(
      const GURL& app_origin,
      storage::FileSystemContext* file_system_context,
      SyncStatusCallback callback);

  void AddChangeObserver(Observer* observer);

  // Registers |url| to wait until sync is enabled for |url|.
  // |on_syncable_callback| is to be called when |url| becomes syncable
  // (i.e. when we have no pending writes and the file is successfully locked
  // for sync).
  // Calling this method again while this already has another URL waiting
  // for sync will overwrite the previously registered URL.
  void RegisterURLForWaitingSync(const storage::FileSystemURL& url,
                                 base::OnceClosure on_syncable_callback);

  // Synchronize one (or a set of) local change(s) to the remote server
  // using local_change_processor given by SetLocalChangeProcessor().
  // |processor| must have same or longer lifetime than this service.
  // It is invalid to call this method before calling SetLocalChangeProcessor().
  void ProcessLocalChange(SyncFileCallback callback);

  // Sets a local change processor. The value is ignored if
  // SetLocalChangeProcessorCallback() is called separately.
  // Either this or SetLocalChangeProcessorCallback() must be called before
  // any ProcessLocalChange().
  void SetLocalChangeProcessor(LocalChangeProcessor* local_change_processor);

  // Sets a closure which gets a local change processor for the given origin.
  // Note that once this is called it overrides the direct processor setting
  // done by SetLocalChangeProcessor().
  // Either this or SetLocalChangeProcessor() must be called before any
  // ProcessLocalChange().
  //
  // TODO(kinuko): Remove this method once we stop using multiple backends
  // (crbug.com/324215), or deprecate the other if we keep doing so.
  void SetLocalChangeProcessorCallback(
      GetLocalChangeProcessorCallback get_local_change_processor);

  // Returns true via |callback| if the given file |url| has local pending
  // changes.
  void HasPendingLocalChanges(const storage::FileSystemURL& url,
                              HasPendingLocalChangeCallback callback);

  void PromoteDemotedChanges(base::RepeatingClosure callback);

  // Returns the metadata of a remote file pointed by |url|.
  virtual void GetLocalFileMetadata(const storage::FileSystemURL& url,
                                    SyncFileMetadataCallback callback);

  // RemoteChangeProcessor overrides.
  void PrepareForProcessRemoteChange(const storage::FileSystemURL& url,
                                     PrepareChangeCallback callback) override;
  void ApplyRemoteChange(const FileChange& change,
                         const base::FilePath& local_path,
                         const storage::FileSystemURL& url,
                         SyncStatusCallback callback) override;
  void FinalizeRemoteSync(const storage::FileSystemURL& url,
                          bool clear_local_changes,
                          base::OnceClosure completion_callback) override;
  void RecordFakeLocalChange(const storage::FileSystemURL& url,
                             const FileChange& change,
                             SyncStatusCallback callback) override;

  // LocalOriginChangeObserver override.
  void OnChangesAvailableInOrigins(const std::set<GURL>& origins) override;

  // Called when a particular origin (app) is disabled/enabled while
  // the service is running. This may be called for origins/apps that
  // are not initialized for the service.
  void SetOriginEnabled(const GURL& origin, bool enabled);

 private:
  typedef std::map<GURL, raw_ptr<storage::FileSystemContext, CtnExperimental>>
      OriginToContext;
  friend class OriginChangeMapTest;

  class OriginChangeMap {
   public:
    typedef std::map<GURL, int64_t> Map;

    OriginChangeMap();
    ~OriginChangeMap();

    // Sets |origin| to the next origin to process. (For now we simply apply
    // round-robin to pick the next origin to avoid starvation.)
    // Returns false if no origins to process.
    bool NextOriginToProcess(GURL* origin);

    int64_t GetTotalChangeCount() const;

    // Update change_count_map_ for |origin|.
    void SetOriginChangeCount(const GURL& origin, int64_t changes);

    void SetOriginEnabled(const GURL& origin, bool enabled);

   private:
    // Per-origin changes (cached info, could be stale).
    Map change_count_map_;
    Map::iterator next_;

    // Holds a set of disabled (but initialized) origins.
    std::set<GURL> disabled_origins_;
  };

  LocalFileSyncService(Profile* profile, leveldb::Env* env_override);

  void DidInitializeFileSystemContext(
      const GURL& app_origin,
      storage::FileSystemContext* file_system_context,
      SyncStatusCallback callback,
      SyncStatusCode status);
  void DidInitializeForRemoteSync(
      const storage::FileSystemURL& url,
      storage::FileSystemContext* file_system_context,
      PrepareChangeCallback callback,
      SyncStatusCode status);

  // Callback for ApplyRemoteChange.
  void DidApplyRemoteChange(SyncStatusCallback callback, SyncStatusCode status);

  // Callbacks for ProcessLocalChange.
  void DidGetFileForLocalSync(SyncFileCallback callback,
                              SyncStatusCode status,
                              const LocalFileSyncInfo& sync_file_info,
                              storage::ScopedFile snapshot);
  void ProcessNextChangeForURL(SyncFileCallback callback,
                               storage::ScopedFile snapshot,
                               const LocalFileSyncInfo& sync_file_info,
                               const FileChange& last_change,
                               const FileChangeList& changes,
                               SyncStatusCode status);

  // A thin wrapper of get_local_change_processor_.
  LocalChangeProcessor* GetLocalChangeProcessor(
      const storage::FileSystemURL& url);

  raw_ptr<Profile> profile_;

  scoped_refptr<LocalFileSyncContext> sync_context_;

  // Origin to context map. (Assuming that as far as we're in the same
  // profile single origin wouldn't belong to multiple FileSystemContexts.)
  OriginToContext origin_to_contexts_;

  // Origins which have pending changes but have not been initialized yet.
  // (Used only for handling dirty files left in the local tracker database
  // after a restart.)
  std::set<GURL> pending_origins_with_changes_;

  OriginChangeMap origin_change_map_;

  raw_ptr<LocalChangeProcessor> local_change_processor_;
  GetLocalChangeProcessorCallback get_local_change_processor_;

  base::ObserverList<Observer>::UncheckedAndDanglingUntriaged change_observers_;
  base::WeakPtrFactory<LocalFileSyncService> weak_ptr_factory_{this};
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_LOCAL_LOCAL_FILE_SYNC_SERVICE_H_
