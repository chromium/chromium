// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_METADATA_DATABASE_INDEX_INTERFACE_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_METADATA_DATABASE_INDEX_INTERFACE_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

namespace sync_file_system {
namespace drive_backend {

class FileMetadata;
class FileTracker;
class TrackerIDSet;

struct ParentIDAndTitle {
  int64_t parent_id;
  std::string title;

  ParentIDAndTitle();
  ParentIDAndTitle(int64_t parent_id, const std::string& title);
};

bool operator==(const ParentIDAndTitle& left, const ParentIDAndTitle& right);
bool operator<(const ParentIDAndTitle& left, const ParentIDAndTitle& right);

// Interface class to maintain indexes of MetadataDatabase.
class MetadataDatabaseIndexInterface {
 public:
  MetadataDatabaseIndexInterface() {}

  MetadataDatabaseIndexInterface(const MetadataDatabaseIndexInterface&) =
      delete;
  MetadataDatabaseIndexInterface& operator=(
      const MetadataDatabaseIndexInterface&) = delete;

  virtual ~MetadataDatabaseIndexInterface() {}

  // Removes unreachable items.
  virtual void RemoveUnreachableItems() = 0;

  // Returns true if FileMetadata identified by |file_id| exists.
  // If |metadata| is not NULL, the FileMetadata is copied to it.
  virtual bool GetFileMetadata(
      const std::string& file_id, FileMetadata* metadata) const = 0;

  // Returns true if FileTracker identified by |tracker_id| exists.
  // If |tracker| is not NULL, the FileTracker is copied to it.
  virtual bool GetFileTracker(int64_t tracker_id,
                              FileTracker* tracker) const = 0;

  // Stores |metadata| and updates indexes.
  // This overwrites existing FileMetadata for the same |file_id|.
  virtual void StoreFileMetadata(std::unique_ptr<FileMetadata> metadata) = 0;

  // Stores |tracker| and updates indexes.
  // This overwrites existing FileTracker for the same |tracker_id|.
  virtual void StoreFileTracker(std::unique_ptr<FileTracker> tracker) = 0;

  // Removes FileMetadata identified by |file_id| from indexes.
  virtual void RemoveFileMetadata(const std::string& file_id) = 0;

  // Removes FileTracker identified by |tracker_id| from indexes.
  virtual void RemoveFileTracker(int64_t tracker_id) = 0;

  // Returns a set of FileTracker that have |file_id| as its own.
  virtual TrackerIDSet GetFileTrackerIDsByFileID(
      const std::string& file_id) const = 0;

  // Returns an app-root tracker identified by |app_id|.  Returns 0 if not
  // found.
  virtual int64_t GetAppRootTracker(const std::string& app_id) const = 0;

  // Returns a set of FileTracker that have |parent_tracker_id| and |title|.
  virtual TrackerIDSet GetFileTrackerIDsByParentAndTitle(
      int64_t parent_tracker_id,
      const std::string& title) const = 0;

  virtual std::vector<int64_t> GetFileTrackerIDsByParent(
      int64_t parent_tracker_id) const = 0;

  // Returns the |file_id| of a file that has multiple trackers.
  virtual std::string PickMultiTrackerFileID() const = 0;

  // Returns a pair of |parent_tracker_id| and |title| that has multiple file
  // at the path.
  virtual ParentIDAndTitle PickMultiBackingFilePath() const = 0;

  // Returns a FileTracker whose |dirty| is set and which isn't demoted.
  // Returns 0 if not found.
  virtual int64_t PickDirtyTracker() const = 0;

  // Demotes a dirty tracker.
  virtual void DemoteDirtyTracker(int64_t tracker_id) = 0;

  virtual bool HasDemotedDirtyTracker() const = 0;
  virtual bool IsDemotedDirtyTracker(int64_t tracker_id) const = 0;

  // Promotes single demoted dirty tracker to a normal dirty tracker.
  virtual void PromoteDemotedDirtyTracker(int64_t tracker_id) = 0;

  // Promotes all demoted dirty trackers to normal dirty trackers.
  // Returns true if any tracker was promoted.
  virtual bool PromoteDemotedDirtyTrackers() = 0;

  virtual size_t CountDirtyTracker() const = 0;
  virtual size_t CountFileMetadata() const = 0;
  virtual size_t CountFileTracker() const = 0;

  virtual void SetSyncRootRevalidated() const = 0;
  virtual void SetSyncRootTrackerID(int64_t sync_root_id) const = 0;
  virtual void SetLargestChangeID(int64_t largest_change_id) const = 0;
  virtual void SetNextTrackerID(int64_t next_tracker_id) const = 0;
  virtual bool IsSyncRootRevalidated() const = 0;
  virtual int64_t GetSyncRootTrackerID() const = 0;
  virtual int64_t GetLargestChangeID() const = 0;
  virtual int64_t GetNextTrackerID() const = 0;
  virtual std::vector<std::string> GetRegisteredAppIDs() const = 0;
  virtual std::vector<int64_t> GetAllTrackerIDs() const = 0;
  virtual std::vector<std::string> GetAllMetadataIDs() const = 0;
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_METADATA_DATABASE_INDEX_INTERFACE_H_
