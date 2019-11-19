// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/metadata_database_index_on_disk.h"

#include <unordered_set>

#include "base/format_macros.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_constants.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_util.h"
#include "chrome/browser/sync_file_system/drive_backend/leveldb_wrapper.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.pb.h"
#include "chrome/browser/sync_file_system/logger.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"

// LevelDB database schema
// =======================
//
// NOTE
// - Entries are sorted by keys.
// - int64_t value is serialized as a string by base::NumberToString().
// - ServiceMetadata, FileMetadata, and FileTracker values are serialized
//   as a string by SerializeToString() of protocol buffers.
//
// Version 4:
//   # Version of this schema
//   key: "VERSION"
//   value: "4"
//
//   # Metadata of the SyncFS service (compatible with version 3)
//   key: "SERVICE"
//   value: <ServiceMetadata 'service_metadata'>
//
//   # Metadata of remote files (compatible with version 3)
//   key: "FILE: " + <string 'file_id'>
//   value: <FileMetadata 'metadata'>
//
//   # Trackers of remote file updates (compatible with version 3)
//   key: "TRACKER: " + <int64_t 'tracker_id'>
//   value: <FileTracker 'tracker'>
//
//   # Index from App ID to the tracker ID
//   key: "APP_ROOT: " + <string 'app_id'>
//   value: <int64_t 'app_root_tracker_id'>
//
//   # Index from file ID to the active tracker ID
//   key: "ACTIVE_FILE: " + <string 'file_id'>
//   value: <int64_t 'active_tracker_id'>
//
//   # Index from file ID to a tracker ID
//   key: "TRACKER_FILE: " + <string 'file_id'> + '\x00' +
//        <int64_t'tracker_id'>
//   value: <empty>
//
//   # Tracker IDs; a file metadata linked to multiple tracker IDs.
//   key: "MULTI_FILE: " + <int64_t 'tracker_id'>
//   value: <empty>
//
//   # Index from the parent tracker ID and the title to the active tracker ID
//   key: "ACTIVE_PATH: " + <int64_t 'parent_tracker_id'> +
//        '\x00' + <string 'title'>
//   value: <int64_t 'active_tracker_id'>
//
//   # Index from the parent tracker ID and the title to a tracker ID
//   key: "TRACKER_PATH: " + <int64_t 'parent_tracker_id'> +
//        '\x00' + <string 'title'> + '\x00' + <int64_t 'tracker_id'>
//   value: <empty>
//
//   # Tracker IDs; a parent tracker ID and a title figure multiple tracker IDs
//   key: "MULTI_PATH: " + <int64_t 'tracker_id'>
//   value: <empty>
//
//   # Dirty tracker IDs
//   key: "DIRTY: " + <int64_t 'dirty_tracker_id'>
//   value: <empty>
//
//   # Demoted dirty tracker IDs
//   key: "DEMOTED_DIRTY: " + <int64_t 'demoted_dirty_tracker_id'>
//   value: <empty>
//
//   # Timestamp when the last validation ran
//   key: "LAST_VALID"
//   value: <time_t 'last_valid_time'>

namespace sync_file_system {
namespace drive_backend {

namespace {

std::string GenerateAppRootIDByAppIDKey(const std::string& app_id) {
  return kAppRootIDByAppIDKeyPrefix + app_id;
}

std::string GenerateActiveTrackerIDByFileIDKey(const std::string& file_id) {
  return kActiveTrackerIDByFileIDKeyPrefix + file_id;
}

std::string GenerateTrackerIDByFileIDKeyPrefix(const std::string& file_id) {
  std::ostringstream oss;
  oss << kTrackerIDByFileIDKeyPrefix << file_id << '\0';
  return oss.str();
}

std::string GenerateMultiTrackerKey(const std::string& file_id) {
  return kMultiTrackerByFileIDKeyPrefix + file_id;
}

std::string GenerateActiveTrackerIDByParentAndTitleKey(
    int64_t parent_id,
    const std::string& title) {
  std::ostringstream oss;
  oss << kActiveTrackerIDByParentAndTitleKeyPrefix << parent_id
      << '\0' << title;
  return oss.str();
}

std::string GenerateTrackerIDByParentAndTitleKeyPrefix(
    int64_t parent_id,
    const std::string& title) {
  std::ostringstream oss;
  oss << kTrackerIDByParentAndTitleKeyPrefix << parent_id << '\0'
      << title << '\0';
  return oss.str();
}

std::string GenerateTrackerIDsByParentIDKeyPrefix(int64_t parent_id) {
  std::ostringstream oss;
  oss << kTrackerIDByParentAndTitleKeyPrefix << parent_id << '\0';
  return oss.str();
}

std::string GenerateMultiBackingParentAndTitleKey(int64_t parent_id,
                                                  const std::string& title) {
  std::ostringstream oss;
  oss << kMultiBackingParentAndTitleKeyPrefix << parent_id << '\0'
      << title;
  return oss.str();
}

std::string GenerateDirtyIDKey(int64_t tracker_id) {
  return kDirtyIDKeyPrefix + base::NumberToString(tracker_id);
}

std::string GenerateDemotedDirtyIDKey(int64_t tracker_id) {
  return kDemotedDirtyIDKeyPrefix + base::NumberToString(tracker_id);
}

void RemoveUnreachableItemsFromDB(LevelDBWrapper* db,
                                  int64_t sync_root_tracker_id) {
  DCHECK(db);

  typedef std::map<int64_t, std::set<int64_t>> ChildTrackersByParent;
  ChildTrackersByParent trackers_by_parent;
  {
    // Set up links from parent tracker to child trackers.
    std::set<int64_t> inactive_trackers;
    std::unique_ptr<LevelDBWrapper::Iterator> itr = db->NewIterator();
    for (itr->Seek(kFileTrackerKeyPrefix); itr->Valid(); itr->Next()) {
      if (!RemovePrefix(itr->key().ToString(), kFileTrackerKeyPrefix, nullptr))
        break;

      std::unique_ptr<FileTracker> tracker(new FileTracker);
      if (!tracker->ParseFromString(itr->value().ToString())) {
        util::Log(logging::LOG_WARNING, FROM_HERE,
                  "Failed to parse a Tracker");
        continue;
      }

      int64_t parent_tracker_id = tracker->parent_tracker_id();
      int64_t tracker_id = tracker->tracker_id();
      trackers_by_parent[parent_tracker_id].insert(tracker_id);
      if (!tracker->active())
        inactive_trackers.insert(tracker_id);
    }

    // Drop links from inactive trackers.
    for (auto iter = inactive_trackers.begin(); iter != inactive_trackers.end();
         ++iter) {
      trackers_by_parent.erase(*iter);
    }
  }

  // Traverse tracker tree from sync-root.
  std::set<int64_t> visited_trackers;
  {
    std::vector<int64_t> pending;
    if (sync_root_tracker_id != kInvalidTrackerID)
      pending.push_back(sync_root_tracker_id);

    while (!pending.empty()) {
      int64_t tracker_id = pending.back();
      DCHECK_NE(kInvalidTrackerID, tracker_id);
      pending.pop_back();

      if (!visited_trackers.insert(tracker_id).second) {
        NOTREACHED();
        continue;
      }

      AppendContents(
          LookUpMap(trackers_by_parent, tracker_id, std::set<int64_t>()),
          &pending);
    }
  }

  // Delete all unreachable trackers, and list all |file_id| referred by
  // remained trackers.
  std::unordered_set<std::string> referred_file_ids;
  {
    std::unique_ptr<LevelDBWrapper::Iterator> itr = db->NewIterator();
    for (itr->Seek(kFileTrackerKeyPrefix); itr->Valid(); itr->Next()) {
      if (!RemovePrefix(itr->key().ToString(), kFileTrackerKeyPrefix, nullptr))
        break;

      std::unique_ptr<FileTracker> tracker(new FileTracker);
      if (!tracker->ParseFromString(itr->value().ToString())) {
        util::Log(logging::LOG_WARNING, FROM_HERE,
                  "Failed to parse a Tracker");
        continue;
      }

      if (base::Contains(visited_trackers, tracker->tracker_id())) {
        referred_file_ids.insert(tracker->file_id());
      } else {
        PutFileTrackerDeletionToDB(tracker->tracker_id(), db);
      }
    }
  }

  // Delete all unreferred metadata.
  {
    std::unique_ptr<LevelDBWrapper::Iterator> itr = db->NewIterator();
    for (itr->Seek(kFileMetadataKeyPrefix); itr->Valid(); itr->Next()) {
      if (!RemovePrefix(itr->key().ToString(), kFileMetadataKeyPrefix, nullptr))
        break;

      std::unique_ptr<FileMetadata> metadata(new FileMetadata);
      if (!metadata->ParseFromString(itr->value().ToString())) {
        util::Log(logging::LOG_WARNING, FROM_HERE,
                  "Failed to parse a Tracker");
        continue;
      }

      if (!base::Contains(referred_file_ids, metadata->file_id()))
        PutFileMetadataDeletionToDB(metadata->file_id(), db);
    }
  }
}

}  // namespace

// static
std::unique_ptr<MetadataDatabaseIndexOnDisk>
MetadataDatabaseIndexOnDisk::Create(LevelDBWrapper* db) {
  DCHECK(db);

  std::unique_ptr<ServiceMetadata> service_metadata =
      InitializeServiceMetadata(db);
  if (!service_metadata)
    return nullptr;

  PutVersionToDB(kDatabaseOnDiskVersion, db);
  RemoveUnreachableItemsFromDB(db, service_metadata->sync_root_tracker_id());
  std::unique_ptr<MetadataDatabaseIndexOnDisk> index(
      new MetadataDatabaseIndexOnDisk(db));

  return index;
}

MetadataDatabaseIndexOnDisk::~MetadataDatabaseIndexOnDisk() {}

void MetadataDatabaseIndexOnDisk::RemoveUnreachableItems() {
  RemoveUnreachableItemsFromDB(
      db_, service_metadata_->sync_root_tracker_id());
  DeleteTrackerIndexes();
  BuildTrackerIndexes();
}

bool MetadataDatabaseIndexOnDisk::GetFileMetadata(
    const std::string& file_id, FileMetadata* metadata) const {
  const std::string key = kFileMetadataKeyPrefix + file_id;
  std::string value;
  leveldb::Status status = db_->Get(key, &value);

  if (status.IsNotFound())
    return false;

  if (!status.ok()) {
    util::Log(logging::LOG_WARNING, FROM_HERE,
              "LevelDB error (%s) in getting FileMetadata for ID: %s",
              status.ToString().c_str(),
              file_id.c_str());
    return false;
  }

  FileMetadata tmp_metadata;
  if (!tmp_metadata.ParseFromString(value)) {
    util::Log(logging::LOG_WARNING, FROM_HERE,
              "Failed to parse a FileMetadata for ID: %s",
              file_id.c_str());
    return false;
  }
  if (metadata)
    metadata->CopyFrom(tmp_metadata);

  return true;
}

bool MetadataDatabaseIndexOnDisk::GetFileTracker(int64_t tracker_id,
                                                 FileTracker* tracker) const {
  const std::string key =
      kFileTrackerKeyPrefix + base::NumberToString(tracker_id);
  std::string value;
  leveldb::Status status = db_->Get(key, &value);

  if (status.IsNotFound())
    return false;

  if (!status.ok()) {
    util::Log(logging::LOG_WARNING, FROM_HERE,
              "LevelDB error (%s) in getting FileTracker for ID: %" PRId64,
              status.ToString().c_str(),
              tracker_id);
    return false;
  }

  FileTracker tmp_tracker;
  if (!tmp_tracker.ParseFromString(value)) {
    util::Log(logging::LOG_WARNING, FROM_HERE,
              "Failed to parse a Tracker for ID: %" PRId64,
              tracker_id);
    return false;
  }
  if (tracker)
    tracker->CopyFrom(tmp_tracker);

  return true;
}

void MetadataDatabaseIndexOnDisk::StoreFileMetadata(
    std::unique_ptr<FileMetadata> metadata) {
  DCHECK(metadata);
  PutFileMetadataToDB(*metadata, db_);
}

void MetadataDatabaseIndexOnDisk::StoreFileTracker(
    std::unique_ptr<FileTracker> tracker) {
  DCHECK(tracker);

  int64_t tracker_id = tracker->tracker_id();
  FileTracker old_tracker;
  if (!GetFileTracker(tracker_id, &old_tracker)) {
    DVLOG(3) << "Adding new tracker: " << tracker->tracker_id()
             << " " << GetTrackerTitle(*tracker);
    AddToAppIDIndex(*tracker);
    AddToFileIDIndexes(*tracker);
    AddToPathIndexes(*tracker);
    AddToDirtyTrackerIndexes(*tracker);
  } else {
    DVLOG(3) << "Updating tracker: " << tracker->tracker_id()
             << " " << GetTrackerTitle(*tracker);
    UpdateInAppIDIndex(old_tracker, *tracker);
    UpdateInFileIDIndexes(old_tracker, *tracker);
    UpdateInPathIndexes(old_tracker, *tracker);
    UpdateInDirtyTrackerIndexes(old_tracker, *tracker);
  }

  PutFileTrackerToDB(*tracker, db_);
}

void MetadataDatabaseIndexOnDisk::RemoveFileMetadata(
    const std::string& file_id) {
  PutFileMetadataDeletionToDB(file_id, db_);
}

void MetadataDatabaseIndexOnDisk::RemoveFileTracker(int64_t tracker_id) {
  FileTracker tracker;
  if (!GetFileTracker(tracker_id, &tracker)) {
    NOTREACHED();
    return;
  }

  DVLOG(1) << "Removing tracker: "
           << tracker.tracker_id() << " " << GetTrackerTitle(tracker);
  RemoveFromAppIDIndex(tracker);
  RemoveFromFileIDIndexes(tracker);
  RemoveFromPathIndexes(tracker);
  RemoveFromDirtyTrackerIndexes(tracker);

  PutFileTrackerDeletionToDB(tracker_id, db_);
}

TrackerIDSet MetadataDatabaseIndexOnDisk::GetFileTrackerIDsByFileID(
    const std::string& file_id) const {
  return GetTrackerIDSetByPrefix(
      GenerateActiveTrackerIDByFileIDKey(file_id),
      GenerateTrackerIDByFileIDKeyPrefix(file_id));
}

int64_t MetadataDatabaseIndexOnDisk::GetAppRootTracker(
    const std::string& app_id) const {
  const std::string key = GenerateAppRootIDByAppIDKey(app_id);
  std::string value;
  leveldb::Status status = db_->Get(key, &value);

  if (status.IsNotFound())
    return kInvalidTrackerID;

  if (!status.ok()) {
    util::Log(logging::LOG_WARNING, FROM_HERE,
              "LevelDB error (%s) in getting AppRoot for AppID: %s",
              status.ToString().c_str(),
              app_id.c_str());
    return kInvalidTrackerID;
  }

  int64_t root_id;
  if (!base::StringToInt64(value, &root_id)) {
    util::Log(logging::LOG_WARNING, FROM_HERE,
              "Failed to parse a root ID (%s) for an App ID: %s",
              value.c_str(),
              app_id.c_str());
    return kInvalidTrackerID;
  }

  return root_id;
}

TrackerIDSet MetadataDatabaseIndexOnDisk::GetFileTrackerIDsByParentAndTitle(
    int64_t parent_tracker_id,
    const std::string& title) const {
  return GetTrackerIDSetByPrefix(
      GenerateActiveTrackerIDByParentAndTitleKey(parent_tracker_id, title),
      GenerateTrackerIDByParentAndTitleKeyPrefix(parent_tracker_id, title));
}

std::vector<int64_t> MetadataDatabaseIndexOnDisk::GetFileTrackerIDsByParent(
    int64_t parent_id) const {
  std::vector<int64_t> result;

  const std::string prefix = GenerateTrackerIDsByParentIDKeyPrefix(parent_id);
  std::unique_ptr<LevelDBWrapper::Iterator> itr(db_->NewIterator());
  for (itr->Seek(prefix); itr->Valid(); itr->Next()) {
    const std::string& key(itr->key().ToString());
    std::string title_and_id;
    if (!RemovePrefix(key, prefix, &title_and_id))
      break;

    size_t pos = title_and_id.rfind('\0');
    DCHECK(pos != std::string::npos);

    int64_t tracker_id;
    if (!base::StringToInt64(title_and_id.substr(pos + 1), &tracker_id))
      continue;
    result.push_back(tracker_id);
  }
  return result;
}

std::string MetadataDatabaseIndexOnDisk::PickMultiTrackerFileID() const {
  std::unique_ptr<LevelDBWrapper::Iterator> itr(db_->NewIterator());
  itr->Seek(kMultiTrackerByFileIDKeyPrefix);
  if (!itr->Valid())
    return std::string();

  std::string file_id;
  if (!RemovePrefix(itr->key().ToString(),
                    kMultiTrackerByFileIDKeyPrefix, &file_id))
    return std::string();

  return file_id;
}

ParentIDAndTitle MetadataDatabaseIndexOnDisk::PickMultiBackingFilePath() const {
  std::unique_ptr<LevelDBWrapper::Iterator> itr(db_->NewIterator());
  itr->Seek(kMultiBackingParentAndTitleKeyPrefix);
  if (!itr->Valid())
    return ParentIDAndTitle();

  std::string value;
  if (!RemovePrefix(itr->key().ToString(),
                    kMultiBackingParentAndTitleKeyPrefix, &value))
    return ParentIDAndTitle();

  size_t pos = value.find('\0');  // '\0' is a separator.
  if (pos == std::string::npos)
    return ParentIDAndTitle();

  int64_t parent_id;
  return base::StringToInt64(value.substr(0, pos), &parent_id) ?
      ParentIDAndTitle(parent_id, value.substr(pos + 1)) : ParentIDAndTitle();
}

int64_t MetadataDatabaseIndexOnDisk::PickDirtyTracker() const {
  std::unique_ptr<LevelDBWrapper::Iterator> itr(db_->NewIterator());
  itr->Seek(kDirtyIDKeyPrefix);
  if (!itr->Valid())
    return kInvalidTrackerID;

  std::string id_str;
  if (!RemovePrefix(itr->key().ToString(), kDirtyIDKeyPrefix, &id_str))
    return kInvalidTrackerID;

  int64_t tracker_id;
  if (!base::StringToInt64(id_str, &tracker_id))
    return kInvalidTrackerID;

  return tracker_id;
}

void MetadataDatabaseIndexOnDisk::DemoteDirtyTracker(int64_t tracker_id) {
  const std::string key = GenerateDirtyIDKey(tracker_id);

  std::string value;
  leveldb::Status status = db_->Get(key, &value);
  if (status.IsNotFound())
    return;
  if (!status.ok()) {
    util::Log(logging::LOG_WARNING, FROM_HERE,
              "LevelDB error (%s) in getting a dirty tracker for ID: %" PRId64,
              status.ToString().c_str(),
              tracker_id);
    return;
  }

  db_->Delete(key);
  db_->Put(GenerateDemotedDirtyIDKey(tracker_id), std::string());
  --num_dirty_trackers_;
}

bool MetadataDatabaseIndexOnDisk::HasDemotedDirtyTracker() const {
  std::unique_ptr<LevelDBWrapper::Iterator> itr(db_->NewIterator());
  itr->Seek(kDemotedDirtyIDKeyPrefix);
  if (!itr->Valid())
    return false;
  return base::StartsWith(itr->key().ToString(), kDemotedDirtyIDKeyPrefix,
                          base::CompareCase::SENSITIVE);
}

bool MetadataDatabaseIndexOnDisk::IsDemotedDirtyTracker(
    int64_t tracker_id) const {
  return DBHasKey(GenerateDemotedDirtyIDKey(tracker_id));
}

void MetadataDatabaseIndexOnDisk::PromoteDemotedDirtyTracker(
    int64_t tracker_id) {
  std::string demoted_key = GenerateDemotedDirtyIDKey(tracker_id);

  std::string empty;
  if (db_->Get(demoted_key, &empty).ok()) {
    db_->Delete(demoted_key);
    db_->Put(GenerateDirtyIDKey(tracker_id), std::string());
    ++num_dirty_trackers_;
  }
}

bool MetadataDatabaseIndexOnDisk::PromoteDemotedDirtyTrackers() {
  bool promoted = false;
  std::unique_ptr<LevelDBWrapper::Iterator> itr(db_->NewIterator());
  for (itr->Seek(kDemotedDirtyIDKeyPrefix); itr->Valid(); itr->Next()) {
    std::string id_str;
    if (!RemovePrefix(itr->key().ToString(), kDemotedDirtyIDKeyPrefix, &id_str))
      break;

    int64_t tracker_id;
    if (!base::StringToInt64(id_str, &tracker_id))
      continue;

    db_->Delete(itr->key().ToString());
    db_->Put(GenerateDirtyIDKey(tracker_id), std::string());
    ++num_dirty_trackers_;
    promoted = true;
  }
  return promoted;
}

size_t MetadataDatabaseIndexOnDisk::CountDirtyTracker() const {
  return num_dirty_trackers_;
}

size_t MetadataDatabaseIndexOnDisk::CountFileMetadata() const {
  // TODO(peria): Cache the number of FileMetadata in the DB.
  size_t count = 0;
  std::unique_ptr<LevelDBWrapper::Iterator> itr(db_->NewIterator());
  for (itr->Seek(kFileMetadataKeyPrefix); itr->Valid(); itr->Next()) {
    if (!base::StartsWith(itr->key().ToString(), kFileMetadataKeyPrefix,
                          base::CompareCase::SENSITIVE))
      break;
    ++count;
  }
  return count;
}

size_t MetadataDatabaseIndexOnDisk::CountFileTracker() const {
  // TODO(peria): Cache the number of FileTracker in the DB.
  size_t count = 0;
  std::unique_ptr<LevelDBWrapper::Iterator> itr(db_->NewIterator());
  for (itr->Seek(kFileTrackerKeyPrefix); itr->Valid(); itr->Next()) {
    if (!base::StartsWith(itr->key().ToString(), kFileTrackerKeyPrefix,
                          base::CompareCase::SENSITIVE))
      break;
    ++count;
  }
  return count;
}

void MetadataDatabaseIndexOnDisk::SetSyncRootRevalidated() const {
  service_metadata_->set_sync_root_revalidated(true);
  PutServiceMetadataToDB(*service_metadata_, db_);
}

void MetadataDatabaseIndexOnDisk::SetSyncRootTrackerID(
    int64_t sync_root_id) const {
  service_metadata_->set_sync_root_tracker_id(sync_root_id);
  PutServiceMetadataToDB(*service_metadata_, db_);
}

void MetadataDatabaseIndexOnDisk::SetLargestChangeID(
    int64_t largest_change_id) const {
  service_metadata_->set_largest_change_id(largest_change_id);
  PutServiceMetadataToDB(*service_metadata_, db_);
}

void MetadataDatabaseIndexOnDisk::SetNextTrackerID(
    int64_t next_tracker_id) const {
  service_metadata_->set_next_tracker_id(next_tracker_id);
  PutServiceMetadataToDB(*service_metadata_, db_);
}

bool MetadataDatabaseIndexOnDisk::IsSyncRootRevalidated() const {
  return service_metadata_->has_sync_root_revalidated() &&
      service_metadata_->sync_root_revalidated();
}

int64_t MetadataDatabaseIndexOnDisk::GetSyncRootTrackerID() const {
  if (!service_metadata_->has_sync_root_tracker_id())
    return kInvalidTrackerID;
  return service_metadata_->sync_root_tracker_id();
}

int64_t MetadataDatabaseIndexOnDisk::GetLargestChangeID() const {
  if (!service_metadata_->has_largest_change_id())
    return kInvalidTrackerID;
  return service_metadata_->largest_change_id();
}

int64_t MetadataDatabaseIndexOnDisk::GetNextTrackerID() const {
  if (!service_metadata_->has_next_tracker_id())
    return kInvalidTrackerID;
  return service_metadata_->next_tracker_id();
}

std::vector<std::string>
MetadataDatabaseIndexOnDisk::GetRegisteredAppIDs() const {
  std::vector<std::string> result;
  std::unique_ptr<LevelDBWrapper::Iterator> itr(db_->NewIterator());
  for (itr->Seek(kAppRootIDByAppIDKeyPrefix); itr->Valid(); itr->Next()) {
    std::string id;
    if (!RemovePrefix(itr->key().ToString(), kAppRootIDByAppIDKeyPrefix, &id))
      break;
    result.push_back(id);
  }
  return result;
}

std::vector<int64_t> MetadataDatabaseIndexOnDisk::GetAllTrackerIDs() const {
  std::vector<int64_t> tracker_ids;
  std::unique_ptr<LevelDBWrapper::Iterator> itr(db_->NewIterator());
  for (itr->Seek(kFileTrackerKeyPrefix); itr->Valid(); itr->Next()) {
    std::string id_str;
    if (!RemovePrefix(itr->key().ToString(), kFileTrackerKeyPrefix, &id_str))
      break;

    int64_t tracker_id;
    if (!base::StringToInt64(id_str, &tracker_id))
      continue;
    tracker_ids.push_back(tracker_id);
  }
  return tracker_ids;
}

std::vector<std::string>
MetadataDatabaseIndexOnDisk::GetAllMetadataIDs() const {
  std::vector<std::string> file_ids;
  std::unique_ptr<LevelDBWrapper::Iterator> itr(db_->NewIterator());
  for (itr->Seek(kFileMetadataKeyPrefix); itr->Valid(); itr->Next()) {
    std::string file_id;
    if (!RemovePrefix(itr->key().ToString(), kFileMetadataKeyPrefix, &file_id))
      break;
    file_ids.push_back(file_id);
  }
  return file_ids;
}

int64_t MetadataDatabaseIndexOnDisk::BuildTrackerIndexes() {
  int64_t num_puts_before = db_->num_puts();

  std::unique_ptr<LevelDBWrapper::Iterator> itr(db_->NewIterator());
  for (itr->Seek(kFileTrackerKeyPrefix); itr->Valid(); itr->Next()) {
    if (!RemovePrefix(itr->key().ToString(), kFileTrackerKeyPrefix, nullptr))
      break;

    FileTracker tracker;
    if (!tracker.ParseFromString(itr->value().ToString())) {
      util::Log(logging::LOG_WARNING, FROM_HERE,
                "Failed to parse a Tracker");
      continue;
    }

    AddToAppIDIndex(tracker);
    AddToFileIDIndexes(tracker);
    AddToPathIndexes(tracker);
    AddToDirtyTrackerIndexes(tracker);
  }

  return db_->num_puts() - num_puts_before;
}

int64_t MetadataDatabaseIndexOnDisk::DeleteTrackerIndexes() {
  const char* kIndexPrefixes[] = {
    kAppRootIDByAppIDKeyPrefix, kActiveTrackerIDByFileIDKeyPrefix,
    kTrackerIDByFileIDKeyPrefix, kMultiTrackerByFileIDKeyPrefix,
    kActiveTrackerIDByParentAndTitleKeyPrefix,
    kTrackerIDByParentAndTitleKeyPrefix, kMultiBackingParentAndTitleKeyPrefix,
    kDirtyIDKeyPrefix, kDemotedDirtyIDKeyPrefix
  };

  int64_t num_deletes_before = db_->num_deletes();
  for (size_t i = 0; i < base::size(kIndexPrefixes); ++i)
    DeleteKeyStartsWith(kIndexPrefixes[i]);
  num_dirty_trackers_ = 0;
  return db_->num_deletes() - num_deletes_before;
}

LevelDBWrapper* MetadataDatabaseIndexOnDisk::GetDBForTesting() {
  return db_;
}

MetadataDatabaseIndexOnDisk::MetadataDatabaseIndexOnDisk(LevelDBWrapper* db)
    : db_(db),
      num_dirty_trackers_(0) {
  // TODO(peria): Add UMA to measure the number of FileMetadata, FileTracker,
  //    and AppRootId.
  service_metadata_ = InitializeServiceMetadata(db_);

  // Check if index is valid, if no validations run in 7 days.
  const int64_t kThresholdToValidateInDays = 7;

  int64_t last_check_time = 0;
  std::string value;
  if (db_->Get(kLastValidationTimeKey, &value).ok())
    base::StringToInt64(value, &last_check_time);
  base::TimeDelta since_last_check =
      base::Time::Now() - base::Time::FromInternalValue(last_check_time);
  int64_t since_last_check_in_days = since_last_check.InDays();
  if (since_last_check_in_days >= kThresholdToValidateInDays ||
      since_last_check_in_days < 0) {
    // TODO(peria): Add UMA to check if the number of deleted entries and the
    // number of built entries are different or not.
    DeleteTrackerIndexes();
    BuildTrackerIndexes();
    db_->Put(kLastValidationTimeKey,
             base::NumberToString(base::Time::Now().ToInternalValue()));
  } else {
    num_dirty_trackers_ = CountDirtyTrackerInternal();
  }
}

void MetadataDatabaseIndexOnDisk::AddToAppIDIndex(const FileTracker& tracker) {
  if (!IsAppRoot(tracker)) {
    DVLOG(3) << "  Tracker for " << tracker.file_id() << " is not an App root.";
    return;
  }

  DVLOG(1) << "  Add to App root by App ID: " << tracker.app_id();

  const std::string db_key = GenerateAppRootIDByAppIDKey(tracker.app_id());
  DCHECK(tracker.active());
  DCHECK(!DBHasKey(db_key));
  db_->Put(db_key, base::NumberToString(tracker.tracker_id()));
}

void MetadataDatabaseIndexOnDisk::UpdateInAppIDIndex(
    const FileTracker& old_tracker,
    const FileTracker& new_tracker) {
  DCHECK_EQ(old_tracker.tracker_id(), new_tracker.tracker_id());

  if (IsAppRoot(old_tracker) && !IsAppRoot(new_tracker)) {
    DCHECK(old_tracker.active());
    DCHECK(!new_tracker.active());
    const std::string key = GenerateAppRootIDByAppIDKey(old_tracker.app_id());
    DCHECK(DBHasKey(key));

    DVLOG(1) << "  Remove from App root by App ID: " << old_tracker.app_id();
    db_->Delete(key);
  } else if (!IsAppRoot(old_tracker) && IsAppRoot(new_tracker)) {
    DCHECK(!old_tracker.active());
    DCHECK(new_tracker.active());
    const std::string key = GenerateAppRootIDByAppIDKey(new_tracker.app_id());
    DCHECK(!DBHasKey(key));

    DVLOG(1) << "  Add to App root by App ID: " << new_tracker.app_id();
    db_->Put(key, base::NumberToString(new_tracker.tracker_id()));
  }
}

void MetadataDatabaseIndexOnDisk::RemoveFromAppIDIndex(
    const FileTracker& tracker) {
  if (!IsAppRoot(tracker)) {
    DVLOG(3) << "  Tracker for " << tracker.file_id() << " is not an App root.";
    return;
  }

  DCHECK(tracker.active());
  const std::string key = GenerateAppRootIDByAppIDKey(tracker.app_id());
  DCHECK(DBHasKey(key));

  DVLOG(1) << "  Remove from App root by App ID: " << tracker.app_id();
  db_->Delete(key);
}

void MetadataDatabaseIndexOnDisk::AddToFileIDIndexes(
    const FileTracker& new_tracker) {
  const std::string& file_id = new_tracker.file_id();

  DVLOG(1) << "  Add to trackers by file ID: " << file_id;
  const std::string prefix = GenerateTrackerIDByFileIDKeyPrefix(file_id);
  AddToTrackerIDSetWithPrefix(
      GenerateActiveTrackerIDByFileIDKey(file_id), prefix, new_tracker);

  const std::string multi_tracker_key = GenerateMultiTrackerKey(file_id);
  if (!DBHasKey(multi_tracker_key) &&
      CountWithPrefix(prefix, new_tracker.tracker_id()) != NONE) {
    DVLOG(1) << "  Add to multi-tracker file IDs: " << file_id;
    db_->Put(multi_tracker_key, std::string());
  }
}

void MetadataDatabaseIndexOnDisk::UpdateInFileIDIndexes(
    const FileTracker& old_tracker,
    const FileTracker& new_tracker) {
  DCHECK_EQ(old_tracker.tracker_id(), new_tracker.tracker_id());
  DCHECK_EQ(old_tracker.file_id(), new_tracker.file_id());

  const std::string& file_id = new_tracker.file_id();
  const std::string prefix = GenerateTrackerIDByFileIDKeyPrefix(file_id);
  DCHECK(DBHasKey(prefix + base::NumberToString(new_tracker.tracker_id())));

  if (old_tracker.active() && !new_tracker.active()) {
    DeactivateInTrackerIDSetWithPrefix(
        GenerateActiveTrackerIDByFileIDKey(file_id), prefix,
        new_tracker.tracker_id());
  } else if (!old_tracker.active() && new_tracker.active()) {
    ActivateInTrackerIDSetWithPrefix(
        GenerateActiveTrackerIDByFileIDKey(file_id), prefix,
        new_tracker.tracker_id());
  }
}

void MetadataDatabaseIndexOnDisk::RemoveFromFileIDIndexes(
    const FileTracker& tracker) {
  const std::string& file_id = tracker.file_id();
  const std::string prefix =
      GenerateTrackerIDByFileIDKeyPrefix(file_id);

  if (!EraseInTrackerIDSetWithPrefix(
          GenerateActiveTrackerIDByFileIDKey(file_id),
          prefix, tracker.tracker_id()))
    return;

  DVLOG(1) << "  Remove from trackers by file ID: " << tracker.tracker_id();

  const std::string multi_key = GenerateMultiTrackerKey(file_id);
  if (DBHasKey(multi_key) &&
      CountWithPrefix(prefix, tracker.tracker_id()) != MULTIPLE) {
    DVLOG(1) << "  Remove from multi-tracker file IDs: " << file_id;
    db_->Delete(multi_key);
  }
}

void MetadataDatabaseIndexOnDisk::AddToPathIndexes(
    const FileTracker& new_tracker) {
  int64_t parent_id = new_tracker.parent_tracker_id();
  std::string title = GetTrackerTitle(new_tracker);

  DVLOG(1) << "  Add to trackers by parent and title: "
           << parent_id << " " << title;

  const std::string prefix =
      GenerateTrackerIDByParentAndTitleKeyPrefix(parent_id, title);
  if (!title.empty()) {
    std::unique_ptr<LevelDBWrapper::Iterator> itr(db_->NewIterator());
    for (itr->Seek(prefix); itr->Valid(); itr->Next()) {
      std::string id_str;
      if (!RemovePrefix(itr->key().ToString(), prefix, &id_str))
        break;

      int64_t tracker_id;
      if (!base::StringToInt64(id_str, &tracker_id))
        continue;
      if (tracker_id == new_tracker.tracker_id()) {
        NOTREACHED();
        continue;
      }

      const std::string multi_key =
          GenerateMultiBackingParentAndTitleKey(parent_id, title);
      DVLOG_IF(1, !DBHasKey(multi_key))
          << "  Add to multi backing file paths: " << parent_id << " " << title;
      db_->Put(multi_key, std::string());
      break;
    }
  }

  AddToTrackerIDSetWithPrefix(
      GenerateActiveTrackerIDByParentAndTitleKey(parent_id, title),
      prefix, new_tracker);
}

void MetadataDatabaseIndexOnDisk::UpdateInPathIndexes(
    const FileTracker& old_tracker,
    const FileTracker& new_tracker) {
  DCHECK_EQ(old_tracker.tracker_id(), new_tracker.tracker_id());
  DCHECK_EQ(old_tracker.parent_tracker_id(), new_tracker.parent_tracker_id());
  DCHECK(GetTrackerTitle(old_tracker) == GetTrackerTitle(new_tracker) ||
         !old_tracker.has_synced_details());

  int64_t tracker_id = new_tracker.tracker_id();
  int64_t parent_id = new_tracker.parent_tracker_id();
  const std::string old_title = GetTrackerTitle(old_tracker);
  const std::string title = GetTrackerTitle(new_tracker);

  if (old_title != title) {
    const std::string old_prefix =
        GenerateTrackerIDByParentAndTitleKeyPrefix(parent_id, old_title);
    EraseInTrackerIDSetWithPrefix(
        GenerateActiveTrackerIDByParentAndTitleKey(parent_id, old_title),
        old_prefix, tracker_id);

    if (!old_title.empty() &&
        CountWithPrefix(old_prefix, tracker_id) != MULTIPLE) {
      const std::string old_multi_backing_key =
          GenerateMultiBackingParentAndTitleKey(parent_id, old_title);
      DVLOG_IF(1, DBHasKey(old_multi_backing_key))
          << "  Remove from multi backing file paths: "
          << parent_id << " " << old_title;
      db_->Delete(old_multi_backing_key);
    }

    DVLOG(1) << "  Add to trackers by parent and title: "
             << parent_id << " " << title;

    const std::string prefix =
        GenerateTrackerIDByParentAndTitleKeyPrefix(parent_id, title);
    AddToTrackerIDSetWithPrefix(
        GenerateActiveTrackerIDByParentAndTitleKey(parent_id, title),
        prefix, new_tracker);

    if (!title.empty() && CountWithPrefix(prefix, tracker_id) != NONE) {
      const std::string multi_backing_key =
          GenerateMultiBackingParentAndTitleKey(parent_id, title);
      DVLOG_IF(1, !DBHasKey(multi_backing_key))
          << "  Add to multi backing file_paths: "
          << parent_id << " " << title;
      db_->Put(multi_backing_key, std::string());
    }

    return;
  }

  const std::string active_tracker_key =
      GenerateActiveTrackerIDByParentAndTitleKey(parent_id, title);
  const std::string prefix =
      GenerateTrackerIDByParentAndTitleKeyPrefix(parent_id, title);
  if (old_tracker.active() && !new_tracker.active()) {
    DeactivateInTrackerIDSetWithPrefix(
        active_tracker_key, prefix, tracker_id);
  } else if (!old_tracker.active() && new_tracker.active()) {
    ActivateInTrackerIDSetWithPrefix(
        active_tracker_key, prefix, tracker_id);
  }
}

void MetadataDatabaseIndexOnDisk::RemoveFromPathIndexes(
    const FileTracker& tracker) {
  int64_t tracker_id = tracker.tracker_id();
  int64_t parent_id = tracker.parent_tracker_id();
  std::string title = GetTrackerTitle(tracker);

  DVLOG(1) << "  Remove from trackers by parent and title: "
           << parent_id << " " << title;

  const std::string active_tracker_key =
      GenerateActiveTrackerIDByParentAndTitleKey(parent_id, title);
  const std::string key_prefix =
      GenerateTrackerIDByParentAndTitleKeyPrefix(parent_id, title);
  if (!EraseInTrackerIDSetWithPrefix(
          active_tracker_key, key_prefix, tracker_id))
    return;

  const std::string multi_key =
      GenerateMultiBackingParentAndTitleKey(parent_id, title);
  if (!title.empty() && DBHasKey(multi_key) &&
      CountWithPrefix(key_prefix, tracker_id) != MULTIPLE) {
    DVLOG(1) << "  Remove from multi backing file paths: "
             << parent_id << " " << title;
    db_->Delete(multi_key);
  }
}

void MetadataDatabaseIndexOnDisk::AddToDirtyTrackerIndexes(
    const FileTracker& new_tracker) {
  const std::string dirty_key = GenerateDirtyIDKey(new_tracker.tracker_id());
  DCHECK(!DBHasKey(dirty_key));
  DCHECK(!DBHasKey(GenerateDemotedDirtyIDKey(new_tracker.tracker_id())));

  if (new_tracker.dirty()) {
    DVLOG(1) << "  Add to dirty tracker IDs: " << new_tracker.tracker_id();
    db_->Put(dirty_key, std::string());
    ++num_dirty_trackers_;
  }
}

void MetadataDatabaseIndexOnDisk::UpdateInDirtyTrackerIndexes(
    const FileTracker& old_tracker,
    const FileTracker& new_tracker) {
  DCHECK_EQ(old_tracker.tracker_id(), new_tracker.tracker_id());

  int64_t tracker_id = new_tracker.tracker_id();
  const std::string dirty_key = GenerateDirtyIDKey(tracker_id);
  const std::string demoted_key = GenerateDemotedDirtyIDKey(tracker_id);
  if (old_tracker.dirty() && !new_tracker.dirty()) {
    DCHECK(DBHasKey(dirty_key) || DBHasKey(demoted_key));

    DVLOG(1) << "  Remove from dirty trackers IDs: " << tracker_id;

    if (DBHasKey(dirty_key))
      --num_dirty_trackers_;
    db_->Delete(dirty_key);
    db_->Delete(demoted_key);
  } else if (!old_tracker.dirty() && new_tracker.dirty()) {
    DCHECK(!DBHasKey(dirty_key));
    DCHECK(!DBHasKey(demoted_key));

    DVLOG(1) << "  Add to dirty tracker IDs: " << tracker_id;

    db_->Put(dirty_key, std::string());
    ++num_dirty_trackers_;
  }
}

void MetadataDatabaseIndexOnDisk::RemoveFromDirtyTrackerIndexes(
    const FileTracker& tracker) {
  if (tracker.dirty()) {
    int64_t tracker_id = tracker.tracker_id();
    const std::string dirty_key = GenerateDirtyIDKey(tracker_id);
    const std::string demoted_key = GenerateDemotedDirtyIDKey(tracker_id);
    DCHECK(DBHasKey(dirty_key) || DBHasKey(demoted_key));

    DVLOG(1) << "  Remove from dirty tracker IDs: " << tracker_id;
    if (DBHasKey(dirty_key))
      --num_dirty_trackers_;
    db_->Delete(dirty_key);
    db_->Delete(demoted_key);
  }
}

TrackerIDSet MetadataDatabaseIndexOnDisk::GetTrackerIDSetByPrefix(
    const std::string& active_tracker_key,
    const std::string& ids_prefix) const {
  TrackerIDSet trackers;

  // Seek IDs.
  std::unique_ptr<LevelDBWrapper::Iterator> itr(db_->NewIterator());
  for (itr->Seek(ids_prefix); itr->Valid(); itr->Next()) {
    const std::string& key(itr->key().ToString());
    std::string id_str;
    if (!RemovePrefix(key, ids_prefix, &id_str))
      break;

    int64_t tracker_id;
    if (!base::StringToInt64(id_str, &tracker_id))
      continue;
    trackers.InsertInactiveTracker(tracker_id);
  }

  // Set an active tracker ID, if available.
  std::string value;
  leveldb::Status status = db_->Get(active_tracker_key, &value);
  int64_t active_tracker;
  if (status.ok() && base::StringToInt64(value, &active_tracker)) {
    DCHECK_NE(kInvalidTrackerID, active_tracker);
    trackers.Activate(active_tracker);
  }

  return trackers;
}

void MetadataDatabaseIndexOnDisk::AddToTrackerIDSetWithPrefix(
    const std::string& active_tracker_key, const std::string& key_prefix,
    const FileTracker& tracker) {
  DCHECK(tracker.tracker_id());

  const std::string id_str = base::NumberToString(tracker.tracker_id());
  db_->Put(key_prefix + id_str, std::string());
  if (tracker.active())
    db_->Put(active_tracker_key, id_str);
}

bool MetadataDatabaseIndexOnDisk::EraseInTrackerIDSetWithPrefix(
    const std::string& active_tracker_key,
    const std::string& key_prefix,
    int64_t tracker_id) {
  std::string value;
  const std::string del_key = key_prefix + base::NumberToString(tracker_id);
  leveldb::Status status = db_->Get(del_key, &value);
  if (status.IsNotFound())
    return false;

  db_->Delete(del_key);

  status = db_->Get(active_tracker_key, &value);
  int64_t active_tracker_id;
  if (status.ok() && base::StringToInt64(value, &active_tracker_id) &&
      active_tracker_id == tracker_id) {
    db_->Delete(active_tracker_key);
  }

  return true;
}

void MetadataDatabaseIndexOnDisk::ActivateInTrackerIDSetWithPrefix(
    const std::string& active_tracker_key,
    const std::string& key_prefix,
    int64_t tracker_id) {
  DCHECK(DBHasKey(key_prefix + base::NumberToString(tracker_id)));

  std::string value;
  leveldb::Status status = db_->Get(active_tracker_key, &value);
  int64_t active_tracker_id = kInvalidTrackerID;
  if (status.IsNotFound() ||
      (status.ok() && base::StringToInt64(value, &active_tracker_id))) {
    DCHECK(active_tracker_id != tracker_id);
    db_->Put(active_tracker_key, base::NumberToString(tracker_id));
  }
}

void MetadataDatabaseIndexOnDisk::DeactivateInTrackerIDSetWithPrefix(
    const std::string& active_tracker_key,
    const std::string& key_prefix,
    int64_t tracker_id) {
  DCHECK(DBHasKey(key_prefix + base::NumberToString(tracker_id)));

  std::string value;
  leveldb::Status status = db_->Get(active_tracker_key, &value);
  int64_t active_tracker_id;
  if (status.ok() && base::StringToInt64(value, &active_tracker_id)) {
    DCHECK(active_tracker_id == tracker_id);
    db_->Delete(active_tracker_key);
  }
}

bool MetadataDatabaseIndexOnDisk::DBHasKey(const std::string& key) const {
  std::unique_ptr<LevelDBWrapper::Iterator> itr(db_->NewIterator());
  itr->Seek(key);
  return itr->Valid() && (itr->key() == key);
}

size_t MetadataDatabaseIndexOnDisk::CountDirtyTrackerInternal() const {
  size_t num_dirty_trackers = 0;

  std::unique_ptr<LevelDBWrapper::Iterator> itr(db_->NewIterator());
  for (itr->Seek(kDirtyIDKeyPrefix); itr->Valid(); itr->Next()) {
    if (!base::StartsWith(itr->key().ToString(), kDirtyIDKeyPrefix,
                          base::CompareCase::SENSITIVE))
      break;
    ++num_dirty_trackers;
  }

  return num_dirty_trackers;
}

MetadataDatabaseIndexOnDisk::NumEntries
MetadataDatabaseIndexOnDisk::CountWithPrefix(const std::string& prefix,
                                             int64_t ignored_id) {
  const std::string ignored = base::NumberToString(ignored_id);

  size_t count = 0;
  std::unique_ptr<LevelDBWrapper::Iterator> itr(db_->NewIterator());
  for (itr->Seek(prefix); itr->Valid() && count <= 1; itr->Next()) {
    std::string value;
    if (!RemovePrefix(itr->key().ToString(), prefix, &value))
      break;
    if (value == ignored)
      continue;

    ++count;
  }

  if (count >= 2)
    return MULTIPLE;
  return count == 0 ? NONE : SINGLE;
}

void MetadataDatabaseIndexOnDisk::DeleteKeyStartsWith(
    const std::string& prefix) {
  std::unique_ptr<LevelDBWrapper::Iterator> itr(db_->NewIterator());
  for (itr->Seek(prefix); itr->Valid();) {
    const std::string key = itr->key().ToString();
    if (!base::StartsWith(key, prefix, base::CompareCase::SENSITIVE))
      break;
    itr->Delete();
  }
}

}  // namespace drive_backend
}  // namespace sync_file_system
