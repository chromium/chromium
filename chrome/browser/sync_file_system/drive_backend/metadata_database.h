// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_METADATA_DATABASE_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_METADATA_DATABASE_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/values.h"
#include "chrome/browser/sync_file_system/drive_backend/tracker_id_set.h"
#include "chrome/browser/sync_file_system/sync_status_code.h"

namespace leveldb {
class Env;
}

namespace google_apis {
class ChangeResource;
class FileResource;
}

namespace base {
class Location;
}

namespace sync_file_system {
namespace drive_backend {

class FileDetails;
class FileMetadata;
class FileTracker;
class LevelDBWrapper;
class MetadataDatabaseIndexInterface;
class ServiceMetadata;

// MetadataDatabase holds and maintains a LevelDB instance and its indexes,
// which holds 1)ServiceMetadata, 2)FileMetadata and 3)FileTracker.
// 1) ServiceMetadata is a singleton in the database which holds information for
//    the backend.
// 2) FileMetadata represents a remote-side file and holds latest known
//    metadata of the remote file.
// 3) FileTracker represents a synced or to-be-synced file and maintains
//    the local-side folder tree.
//
// The term "file" includes files, folders and other resources on Drive.
//
// FileTrackers form a tree structure on the database, which represents the
// FileSystem trees of SyncFileSystem.  The tree has a FileTracker named
// sync-root as its root node, and a set of FileTracker named app-root.  An
// app-root represents a remote folder for an installed Chrome App and holds all
// synced contents for the App.
//
// One FileMetadata is created for each tracked remote file, which is identified
// by FileID.
// One FileTracker is created for every different {parent tracker, FileID} pair,
// excluding non-app-root inactive parent trackers. Multiple trackers may be
// associated to one FileID when the file has multiple parents. Multiple
// trackers may have the same {parent tracker, title} pair when the associated
// remote files have the same title.
//
// Files have following state:
//   - Unknown file
//     - Has a dirty inactive tracker and empty synced_details.
//     - Is initial state of a tracker, only file_id and parent_tracker_id field
//       are known.
//   - Folder
//     - Is either one of sync-root folder, app-root folder or a regular folder.
//     - Sync-root folder holds app-root folders as its direct children, and
//       holds entire SyncFileSystem files as its descentants.  Its tracker
//       should be stored in ServiceMetadata by its tracker_id.
//     - App-root folder holds all files for an application as its descendants.
//   - File
//   - Unsupported file
//     - Represents unsupported files such as hosted documents. Must be
//       inactive.
//
// Invariants:
//   - Any tracker in the database must either:
//     - be sync-root,
//     - have an app-root as its parent tracker, or
//     - have an active tracker as its parent.
//   That is, all trackers must be reachable from sync-root via app-root folders
//   and active trackers.
//
//   - Any active tracker must either:
//     - have |needs_folder_listing| flag and dirty flag, or
//     - have all children at the stored largest change ID.
//
//   - If multiple trackers have the same parent tracker and same title, they
//     must not have same |file_id|, and at most one of them may be active.
//   - If multiple trackers have the same |file_id|, at most one of them may be
//     active.
//
class MetadataDatabase {
 public:
  typedef std::vector<std::string> FileIDList;

  enum ActivationStatus {
    ACTIVATION_PENDING,
    ACTIVATION_FAILED_ANOTHER_ACTIVE_TRACKER,
  };

  enum UpdateOption {
    UPDATE_TRACKER_FOR_UNSYNCED_FILE,
    UPDATE_TRACKER_FOR_SYNCED_FILE,
  };

  // The entry point of the MetadataDatabase for production code.
  // If |env_override| is non-NULL, internal LevelDB uses |env_override| instead
  // of leveldb::Env::Default().  Use leveldb::MemEnv in test code for faster
  // testing.
  static std::unique_ptr<MetadataDatabase> Create(
      const base::FilePath& database_path,
      leveldb::Env* env_override,
      SyncStatusCode* status);
  static std::unique_ptr<MetadataDatabase> CreateInternal(
      const base::FilePath& database_path,
      leveldb::Env* env_override,
      bool enable_on_disk_index,
      SyncStatusCode* status);
  static SyncStatusCode CreateForTesting(
      std::unique_ptr<LevelDBWrapper> db,
      bool enable_on_disk_index,
      std::unique_ptr<MetadataDatabase>* metadata_database_out);

  MetadataDatabase(const MetadataDatabase&) = delete;
  MetadataDatabase& operator=(const MetadataDatabase&) = delete;

  ~MetadataDatabase();

  static void ClearDatabase(
      std::unique_ptr<MetadataDatabase> metadata_database);

  int64_t GetLargestFetchedChangeID() const;
  int64_t GetSyncRootTrackerID() const;

  // Returns true if the client should check if the sync root is still valid.
  bool NeedsSyncRootRevalidation() const;

  bool HasSyncRoot() const;

  // Returns all file metadata for the given |app_id|.
  base::Value::List DumpFiles(const std::string& app_id);

  // Returns all database data.
  base::Value::List DumpDatabase();

  // TODO(tzik): Move GetLargestKnownChangeID() to private section, and hide its
  // handling in the class, instead of letting user do.
  //
  // Gets / updates the largest known change ID.
  // The largest known change ID is on-memory and not persist over restart.
  // This is supposed to use when a task fetches ChangeList in parallel to other
  // operation.  When a task starts fetching paged ChangeList one by one, it
  // should update the largest known change ID on the first round and background
  // remaining fetch job.
  // Then, when other tasks that update FileMetadata by UpdateByFileResource,
  // it should use largest known change ID as the |change_id| that prevents
  // FileMetadata from overwritten by ChangeList.
  // Also if other tasks try to update a remote resource whose change is not yet
  // retrieved the task should fail due to etag check, so we should be fine.
  int64_t GetLargestKnownChangeID() const;
  void UpdateLargestKnownChangeID(int64_t change_id);

  // Populates empty database with initial data.
  // Adds a file metadata and a file tracker for |sync_root_folder|, and adds
  // file metadata and file trackers for each |app_root_folders|.
  // Newly added tracker for |sync_root_folder| is active and non-dirty.
  // Newly added trackers for |app_root_folders| are inactive and non-dirty.
  // Trackers for |app_root_folders| are not yet registered as app-roots, but
  // are ready to register.
  SyncStatusCode PopulateInitialData(
      int64_t largest_change_id,
      const google_apis::FileResource& sync_root_folder,
      const std::vector<std::unique_ptr<google_apis::FileResource>>&
          app_root_folders);

  // Returns true if the folder associated to |app_id| is enabled.
  bool IsAppEnabled(const std::string& app_id) const;

  // Registers existing folder as the app-root for |app_id|.  The folder
  // must be an inactive folder that does not yet associated to any App.
  // This method associates the folder with |app_id| and activates it.
  SyncStatusCode RegisterApp(const std::string& app_id,
                             const std::string& folder_id);

  // Inactivates the folder associated to the app to disable |app_id|.
  // Does nothing if |app_id| is already disabled.
  SyncStatusCode DisableApp(const std::string& app_id);

  // Activates the folder associated to |app_id| to enable |app_id|.
  // Does nothing if |app_id| is already enabled.
  SyncStatusCode EnableApp(const std::string& app_id);

  // Unregisters the folder as the app-root for |app_id|.  If |app_id| does not
  // exist, does nothing.  The folder is left as an inactive regular folder.
  // Note that the inactivation drops all descendant files since they are no
  // longer reachable from sync-root via active folder or app-root.
  SyncStatusCode UnregisterApp(const std::string& app_id);

  // Finds the app-root folder for |app_id|.  Returns true if exists.
  // Copies the result to |tracker| if it is non-NULL.
  bool FindAppRootTracker(const std::string& app_id,
                          FileTracker* tracker) const;

  // Finds the file identified by |file_id|.  Returns true if the file is found.
  // Copies the metadata identified by |file_id| into |file| if exists and
  // |file| is non-NULL.
  bool FindFileByFileID(const std::string& file_id, FileMetadata* file) const;

  // Finds the tracker identified by |tracker_id|.  Returns true if the tracker
  // is found.
  // Copies the tracker identified by |tracker_id| into |tracker| if exists and
  // |tracker| is non-NULL.
  bool FindTrackerByTrackerID(int64_t tracker_id, FileTracker* tracker) const;

  // Finds the trackers tracking |file_id|.  Returns true if the trackers are
  // found.
  bool FindTrackersByFileID(const std::string& file_id,
                            TrackerIDSet* trackers) const;

  // Finds the set of trackers whose parent's tracker ID is |parent_tracker_id|,
  // and who has |title| as its title in the synced_details.
  // Copies the tracker set to |trackers| if it is non-NULL.
  // Returns true if the trackers are found.
  bool FindTrackersByParentAndTitle(int64_t parent_tracker_id,
                                    const std::string& title,
                                    TrackerIDSet* trackers) const;

  // Builds the file path for the given tracker.  Returns true on success.
  // |path| can be NULL.
  // The file path is relative to app-root and have a leading path separator.
  bool BuildPathForTracker(int64_t tracker_id, base::FilePath* path) const;

  // Builds the file path for the given tracker for display purpose.
  // This may return a path ending with '<unknown>' if the given tracker does
  // not have title information (yet). This may return an empty path.
  base::FilePath BuildDisplayPathForTracker(const FileTracker& tracker) const;

  // Returns false if no registered app exists associated to |app_id|.
  // If |full_path| is active, assigns the tracker of |full_path| to |tracker|.
  // Otherwise, assigns the nearest active ancestor to |full_path| to |tracker|.
  // Also, assigns the full path of |tracker| to |path|.
  bool FindNearestActiveAncestor(const std::string& app_id,
                                 const base::FilePath& full_path,
                                 FileTracker* tracker,
                                 base::FilePath* path) const;

  // Updates database by |changes|.
  // Marks each tracker for modified file as dirty and adds new trackers if
  // needed.
  SyncStatusCode UpdateByChangeList(
      int64_t largest_change_id,
      std::vector<std::unique_ptr<google_apis::ChangeResource>> changes);

  // Updates database by |resource|.
  // Marks each tracker for modified file as dirty and adds new trackers if
  // needed.
  SyncStatusCode UpdateByFileResource(
      const google_apis::FileResource& resource);
  SyncStatusCode UpdateByFileResourceList(
      std::vector<std::unique_ptr<google_apis::FileResource>> resources);

  SyncStatusCode UpdateByDeletedRemoteFile(const std::string& file_id);
  SyncStatusCode UpdateByDeletedRemoteFileList(const FileIDList& file_ids);

  // Adds new FileTracker and FileMetadata.  The database must not have
  // |resource| beforehand.
  // The newly added tracker under |parent_tracker_id| is active and non-dirty.
  // Deactivates existing active tracker if exists that has the same title and
  // parent_tracker to the newly added tracker.
  SyncStatusCode ReplaceActiveTrackerWithNewResource(
      int64_t parent_tracker_id,
      const google_apis::FileResource& resource);

  // Adds |child_file_ids| to |folder_id| as its children.
  // This method affects the active tracker only.
  // If the tracker has no further change to sync, unmarks its dirty flag.
  SyncStatusCode PopulateFolderByChildList(const std::string& folder_id,
                                           const FileIDList& child_file_ids);

  // Updates |synced_details| of the tracker with |updated_details|.
  SyncStatusCode UpdateTracker(int64_t tracker_id,
                               const FileDetails& updated_details);

  // Activates a tracker identified by |parent_tracker_id| and |file_id| if the
  // tracker can be activated without inactivating other trackers that have the
  // same |file_id| but different paths.
  // If |file_id| has another active tracker, the function returns
  // ACTIVATION_FAILED_ANOTHER_ACTIVE_TRACKER and does not invoke |callback|.
  // If there is another active tracker that has the same path but different
  // |file_id|, inactivates the tracker.
  // In success case, returns ACTIVATION_PENDING and invokes |callback| upon
  // completion.
  //
  // The tracker to be activated must:
  //  - have a tracked metadata in the database,
  //  - have |synced_details| with valid |title|.
  ActivationStatus TryActivateTracker(int64_t parent_tracker_id,
                                      const std::string& file_id,
                                      SyncStatusCode* status);

  // Changes the priority of the tracker to low.
  void DemoteTracker(int64_t tracker_id);
  bool PromoteDemotedTrackers();
  void PromoteDemotedTracker(int64_t tracker_id);

  // Returns true if there is a normal priority dirty tracker.
  // Assigns the dirty tracker if exists and |tracker| is non-NULL.
  bool GetDirtyTracker(FileTracker* tracker) const;

  // Returns true if there is a low priority dirty tracker.
  bool HasDemotedDirtyTracker() const;

  bool HasDirtyTracker() const;
  size_t CountDirtyTracker() const;
  size_t CountFileMetadata() const;
  size_t CountFileTracker() const;

  bool GetMultiParentFileTrackers(std::string* file_id,
                                  TrackerIDSet* trackers);
  bool GetConflictingTrackers(TrackerIDSet* trackers);

  // Sets |app_ids| to a list of all registered app ids.
  void GetRegisteredAppIDs(std::vector<std::string>* app_ids);

  // Clears dirty flag of trackers that can be cleared without external
  // interactien.
  SyncStatusCode SweepDirtyTrackers(const std::vector<std::string>& file_ids);

 private:
  friend class MetadataDatabaseTest;

  MetadataDatabase(const base::FilePath& database_path,
                   bool enable_on_disk_index,
                   leveldb::Env* env_override);
  SyncStatusCode Initialize();

  // Database manipulation methods.
  void RegisterTrackerAsAppRoot(const std::string& app_id, int64_t tracker_id);

  void CreateTrackerForParentAndFileID(const FileTracker& parent_tracker,
                                       const std::string& file_id);
  void CreateTrackerForParentAndFileMetadata(const FileTracker& parent_tracker,
                                             const FileMetadata& file_metadata,
                                             UpdateOption option);
  void CreateTrackerInternal(const FileTracker& parent_tracker,
                             const std::string& file_id,
                             const FileDetails* details,
                             UpdateOption option);

  void MaybeAddTrackersForNewFile(const FileMetadata& file,
                                  UpdateOption option);

  int64_t IncrementTrackerID();

  bool CanActivateTracker(const FileTracker& tracker);
  bool ShouldKeepDirty(const FileTracker& tracker) const;

  bool HasDisabledAppRoot(const FileTracker& tracker) const;
  bool HasActiveTrackerForFileID(const std::string& file_id) const;
  bool HasActiveTrackerForPath(int64_t parent_tracker,
                               const std::string& title) const;

  void RemoveUnneededTrackersForMissingFile(const std::string& file_id);
  void UpdateByFileMetadata(const base::Location& from_where,
                            std::unique_ptr<FileMetadata> file,
                            UpdateOption option);

  SyncStatusCode WriteToDatabase();

  bool HasNewerFileMetadata(const std::string& file_id, int64_t change_id);

  base::Value::List DumpTrackers();
  base::Value::List DumpMetadata();

  void AttachSyncRoot(const google_apis::FileResource& sync_root_folder);
  void AttachInitialAppRoot(const google_apis::FileResource& app_root_folder);

  void ForceActivateTrackerByPath(int64_t parent_tracker_id,
                                  const std::string& title,
                                  const std::string& file_id);

  bool CanClearDirty(const FileTracker& tracker);

  base::FilePath database_path_;
  raw_ptr<leveldb::Env> env_override_;
  std::unique_ptr<LevelDBWrapper> db_;

  bool enable_on_disk_index_;

  int64_t largest_known_change_id_;

  std::unique_ptr<MetadataDatabaseIndexInterface> index_;

  base::WeakPtrFactory<MetadataDatabase> weak_ptr_factory_{this};
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_METADATA_DATABASE_H_
