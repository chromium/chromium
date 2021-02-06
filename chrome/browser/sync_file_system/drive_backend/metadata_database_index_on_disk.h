// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_METADATA_DATABASE_INDEX_ON_DISK_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_METADATA_DATABASE_INDEX_ON_DISK_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database_index_interface.h"
#include "chrome/browser/sync_file_system/drive_backend/tracker_id_set.h"

namespace sync_file_system {
namespace drive_backend {

class FileMetadata;
class FileTracker;
class LevelDBWrapper;
class ServiceMetadata;
// TODO(peria): Migrate implementation of ParentIDAndTitle structure from
//     metadata_database_index.{cc,h} to here, on removing the files.
struct ParentIDAndTitle;

// Maintains indexes of MetadataDatabase on disk.
class MetadataDatabaseIndexOnDisk : public MetadataDatabaseIndexInterface {
 public:
  static std::unique_ptr<MetadataDatabaseIndexOnDisk> Create(
      LevelDBWrapper* db);

  ~MetadataDatabaseIndexOnDisk() override;

  // MetadataDatabaseIndexInterface overrides.
  void RemoveUnreachableItems() override;
  bool GetFileMetadata(const std::string& file_id,
                       FileMetadata* metadata) const override;
  bool GetFileTracker(int64_t tracker_id, FileTracker* tracker) const override;
  void StoreFileMetadata(std::unique_ptr<FileMetadata> metadata) override;
  void StoreFileTracker(std::unique_ptr<FileTracker> tracker) override;
  void RemoveFileMetadata(const std::string& file_id) override;
  void RemoveFileTracker(int64_t tracker_id) override;
  TrackerIDSet GetFileTrackerIDsByFileID(
      const std::string& file_id) const override;
  int64_t GetAppRootTracker(const std::string& app_id) const override;
  TrackerIDSet GetFileTrackerIDsByParentAndTitle(
      int64_t parent_tracker_id,
      const std::string& title) const override;
  std::vector<int64_t> GetFileTrackerIDsByParent(
      int64_t parent_tracker_id) const override;
  std::string PickMultiTrackerFileID() const override;
  ParentIDAndTitle PickMultiBackingFilePath() const override;
  int64_t PickDirtyTracker() const override;
  void DemoteDirtyTracker(int64_t tracker_id) override;
  bool HasDemotedDirtyTracker() const override;
  bool IsDemotedDirtyTracker(int64_t tracker_id) const override;
  void PromoteDemotedDirtyTracker(int64_t tracker_id) override;
  bool PromoteDemotedDirtyTrackers() override;
  size_t CountDirtyTracker() const override;
  size_t CountFileMetadata() const override;
  size_t CountFileTracker() const override;
  void SetSyncRootRevalidated() const override;
  void SetSyncRootTrackerID(int64_t sync_root_id) const override;
  void SetLargestChangeID(int64_t largest_change_id) const override;
  void SetNextTrackerID(int64_t next_tracker_id) const override;
  bool IsSyncRootRevalidated() const override;
  int64_t GetSyncRootTrackerID() const override;
  int64_t GetLargestChangeID() const override;
  int64_t GetNextTrackerID() const override;
  std::vector<std::string> GetRegisteredAppIDs() const override;
  std::vector<int64_t> GetAllTrackerIDs() const override;
  std::vector<std::string> GetAllMetadataIDs() const override;

  // Builds on-disk indexes from FileTracker entries on disk.
  // Returns the number of newly added entries for indexing.
  int64_t BuildTrackerIndexes();

  // Deletes entries used for indexes on on-disk database.
  // Returns the number of the deleted entries.
  int64_t DeleteTrackerIndexes();

  LevelDBWrapper* GetDBForTesting();

 private:
  enum NumEntries {
    NONE,      // No entries are found.
    SINGLE,    // One entry is found.
    MULTIPLE,  // Two or more entires are found.
  };

  explicit MetadataDatabaseIndexOnDisk(LevelDBWrapper* db);

  // Maintain indexes from AppIDs to tracker IDs.
  void AddToAppIDIndex(const FileTracker& new_tracker);
  void UpdateInAppIDIndex(const FileTracker& old_tracker,
                          const FileTracker& new_tracker);
  void RemoveFromAppIDIndex(const FileTracker& tracker);

  // Maintain indexes from remote file IDs to tracker ID sets.
  void AddToFileIDIndexes(const FileTracker& new_tracker);
  void UpdateInFileIDIndexes(const FileTracker& old_tracker,
                             const FileTracker& new_tracker);
  void RemoveFromFileIDIndexes(const FileTracker& tracker);

  // Maintain indexes from path indexes to tracker ID sets
  void AddToPathIndexes(const FileTracker& new_tracker);
  void UpdateInPathIndexes(const FileTracker& old_tracker,
                           const FileTracker& new_tracker);
  void RemoveFromPathIndexes(const FileTracker& tracker);

  // Maintain dirty tracker IDs.
  void AddToDirtyTrackerIndexes(const FileTracker& new_tracker);
  void UpdateInDirtyTrackerIndexes(const FileTracker& old_tracker,
                                   const FileTracker& new_tracker);
  void RemoveFromDirtyTrackerIndexes(const FileTracker& tracker);

  // Returns a TrackerIDSet built from IDs which are found with given key
  // and key prefix.
  TrackerIDSet GetTrackerIDSetByPrefix(
      const std::string& active_tracker_key,
      const std::string& key_prefix) const;


  // Simulate behavior of TrackerIDSet class.

  // Adds an entry with |key_prefix| and tracker ID of |tracker|.  If |tracker|
  // is active, an entry for |active_key| is added.
  void AddToTrackerIDSetWithPrefix(
      const std::string& active_tracker_key,
      const std::string& key_prefix,
      const FileTracker& tracker);

  // Returns true if |tracker_id| is removed successfully.  If no other entries
  // are stored with |key_prefix|, the entry for |active_key| is also removed.
  bool EraseInTrackerIDSetWithPrefix(const std::string& active_tracker_key,
                                     const std::string& key_prefix,
                                     int64_t tracker_id);

  // Adds an entry for |active_key| on DB, if |tracker_id| has an entry with
  // |key_prefix|.
  void ActivateInTrackerIDSetWithPrefix(const std::string& active_tracker_key,
                                        const std::string& key_prefix,
                                        int64_t tracker_id);

  // Removes an entry for |active_key| on DB, if the value of |active_key| key
  // is |tracker_id|.
  void DeactivateInTrackerIDSetWithPrefix(const std::string& active_tracker_key,
                                          const std::string& key_prefix,
                                          int64_t tracker_id);

  // Checks if |db_| has an entry whose key is |key|.
  bool DBHasKey(const std::string& key) const;

  // Returns the number of dirty trackers, actually counting them.
  size_t CountDirtyTrackerInternal() const;

  // Returns the number of entries starting with |prefix| in NumEntries format.
  // Entries for |ignored_id| are not counted in.
  NumEntries CountWithPrefix(const std::string& prefix, int64_t ignored_id);

  // Deletes entries whose keys start from |prefix|.
  void DeleteKeyStartsWith(const std::string& prefix);

  LevelDBWrapper* db_;  // Not owned.
  std::unique_ptr<ServiceMetadata> service_metadata_;

  size_t num_dirty_trackers_;

  DISALLOW_COPY_AND_ASSIGN(MetadataDatabaseIndexOnDisk);
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_METADATA_DATABASE_INDEX_ON_DISK_H_
