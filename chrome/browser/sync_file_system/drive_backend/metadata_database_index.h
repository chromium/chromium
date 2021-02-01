// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_METADATA_DATABASE_INDEX_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_METADATA_DATABASE_INDEX_H_

#include <stddef.h>
#include <stdint.h>

#include <functional>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "base/hash/hash.h"
#include "base/macros.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database_index_interface.h"
#include "chrome/browser/sync_file_system/drive_backend/tracker_id_set.h"

namespace sync_file_system {
namespace drive_backend {

class FileMetadata;
class FileTracker;
class LevelDBWrapper;
class ServiceMetadata;

}  // namespace drive_backend
}  // namespace sync_file_system

namespace std {

template<> struct hash<sync_file_system::drive_backend::ParentIDAndTitle> {
  std::size_t operator()(
      const sync_file_system::drive_backend::ParentIDAndTitle& v) const {
    return base::HashInts64(v.parent_id, hash<std::string>()(v.title));
  }
};

}  // namespace std

namespace sync_file_system {
namespace drive_backend {

struct DatabaseContents {
  DatabaseContents();
  ~DatabaseContents();
  std::vector<std::unique_ptr<FileMetadata>> file_metadata;
  std::vector<std::unique_ptr<FileTracker>> file_trackers;
};

// Maintains indexes of MetadataDatabase on memory.
class MetadataDatabaseIndex : public MetadataDatabaseIndexInterface {
 public:
  ~MetadataDatabaseIndex() override;

  static std::unique_ptr<MetadataDatabaseIndex> Create(LevelDBWrapper* db);
  static std::unique_ptr<MetadataDatabaseIndex> CreateForTesting(
      DatabaseContents* contents,
      LevelDBWrapper* db);

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

 private:
  typedef std::unordered_map<std::string, std::unique_ptr<FileMetadata>>
      MetadataByID;
  typedef std::unordered_map<int64_t, std::unique_ptr<FileTracker>> TrackerByID;
  typedef std::unordered_map<std::string, TrackerIDSet> TrackerIDsByFileID;
  typedef std::unordered_map<std::string, TrackerIDSet> TrackerIDsByTitle;
  typedef std::map<int64_t, TrackerIDsByTitle> TrackerIDsByParentAndTitle;
  typedef std::unordered_map<std::string, int64_t> TrackerIDByAppID;
  typedef std::unordered_set<std::string> FileIDSet;
  typedef std::unordered_set<ParentIDAndTitle> PathSet;
  typedef std::set<int64_t> DirtyTrackers;

  friend class MetadataDatabaseTest;

  explicit MetadataDatabaseIndex(LevelDBWrapper* db);
  void Initialize(std::unique_ptr<ServiceMetadata> service_metadata,
                  DatabaseContents* contents);

  // Maintains |app_root_by_app_id_|.
  void AddToAppIDIndex(const FileTracker& new_tracker);
  void UpdateInAppIDIndex(const FileTracker& old_tracker,
                          const FileTracker& new_tracker);
  void RemoveFromAppIDIndex(const FileTracker& tracker);

  // Maintains |trackers_by_file_id_| and |multi_tracker_file_ids_|.
  void AddToFileIDIndexes(const FileTracker& new_tracker);
  void UpdateInFileIDIndexes(const FileTracker& old_tracker,
                             const FileTracker& new_tracker);
  void RemoveFromFileIDIndexes(const FileTracker& tracker);

  // Maintains |trackers_by_parent_and_title_| and |multi_backing_file_paths_|.
  void AddToPathIndexes(const FileTracker& new_tracker);
  void UpdateInPathIndexes(const FileTracker& old_tracker,
                           const FileTracker& new_tracker1);
  void RemoveFromPathIndexes(const FileTracker& tracker);

  // Maintains |dirty_trackers_| and |demoted_dirty_trackers_|.
  void AddToDirtyTrackerIndexes(const FileTracker& new_tracker);
  void UpdateInDirtyTrackerIndexes(const FileTracker& old_tracker,
                                   const FileTracker& new_tracker);
  void RemoveFromDirtyTrackerIndexes(const FileTracker& tracker);

  std::unique_ptr<ServiceMetadata> service_metadata_;
  LevelDBWrapper* db_;  // Not owned

  MetadataByID metadata_by_id_;
  TrackerByID tracker_by_id_;

  TrackerIDByAppID app_root_by_app_id_;

  TrackerIDsByFileID trackers_by_file_id_;
  FileIDSet multi_tracker_file_ids_;

  TrackerIDsByParentAndTitle trackers_by_parent_and_title_;
  PathSet multi_backing_file_paths_;

  DirtyTrackers dirty_trackers_;
  DirtyTrackers demoted_dirty_trackers_;

  DISALLOW_COPY_AND_ASSIGN(MetadataDatabaseIndex);
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_METADATA_DATABASE_INDEX_H_
