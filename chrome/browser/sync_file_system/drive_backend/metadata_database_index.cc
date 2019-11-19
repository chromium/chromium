// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/metadata_database_index.h"

#include <tuple>
#include <unordered_set>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_constants.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_util.h"
#include "chrome/browser/sync_file_system/drive_backend/leveldb_wrapper.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.pb.h"
#include "chrome/browser/sync_file_system/logger.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

// LevelDB database schema
// =======================
//
// NOTE
// - Entries are sorted by keys.
// - int64_t value is serialized as a string by base::NumberToString().
// - ServiceMetadata, FileMetadata, and FileTracker values are serialized
//   as a string by SerializeToString() of protocol buffers.
//
// Version 3
//   # Version of this schema
//   key: "VERSION"
//   value: "3"
//
//   # Metadata of the SyncFS service
//   key: "SERVICE"
//   value: <ServiceMetadata 'service_metadata'>
//
//   # Metadata of remote files
//   key: "FILE: " + <string 'file_id'>
//   value: <FileMetadata 'metadata'>
//
//   # Trackers of local file updates
//   key: "TRACKER: " + <int64_t 'tracker_id'>
//   value: <FileTracker 'tracker'>

namespace sync_file_system {
namespace drive_backend {

ParentIDAndTitle::ParentIDAndTitle() : parent_id(0) {}
ParentIDAndTitle::ParentIDAndTitle(int64_t parent_id, const std::string& title)
    : parent_id(parent_id), title(title) {}

bool operator==(const ParentIDAndTitle& left, const ParentIDAndTitle& right) {
  return left.parent_id == right.parent_id && left.title == right.title;
}

bool operator<(const ParentIDAndTitle& left, const ParentIDAndTitle& right) {
  return std::tie(left.parent_id, left.title) <
         std::tie(right.parent_id, right.title);
}

DatabaseContents::DatabaseContents() {}

DatabaseContents::~DatabaseContents() {}

namespace {

template <typename Container>
typename Container::mapped_type FindItem(
    const Container& container,
    const typename Container::key_type& key) {
  auto found = container.find(key);
  if (found == container.end())
    return typename Container::mapped_type();
  return found->second;
}

void ReadDatabaseContents(LevelDBWrapper* db, DatabaseContents* contents) {
  DCHECK(db);
  DCHECK(contents);

  std::unique_ptr<LevelDBWrapper::Iterator> itr(db->NewIterator());
  for (itr->SeekToFirst(); itr->Valid(); itr->Next()) {
    std::string key = itr->key().ToString();
    std::string value = itr->value().ToString();

    std::string file_id;
    if (RemovePrefix(key, kFileMetadataKeyPrefix, &file_id)) {
      std::unique_ptr<FileMetadata> metadata(new FileMetadata);
      if (!metadata->ParseFromString(itr->value().ToString())) {
        util::Log(logging::LOG_WARNING, FROM_HERE,
                  "Failed to parse a FileMetadata");
        continue;
      }

      contents->file_metadata.push_back(std::move(metadata));
      continue;
    }

    std::string tracker_id_str;
    if (RemovePrefix(key, kFileTrackerKeyPrefix, &tracker_id_str)) {
      int64_t tracker_id = 0;
      if (!base::StringToInt64(tracker_id_str, &tracker_id)) {
        util::Log(logging::LOG_WARNING, FROM_HERE,
                  "Failed to parse TrackerID");
        continue;
      }

      std::unique_ptr<FileTracker> tracker(new FileTracker);
      if (!tracker->ParseFromString(itr->value().ToString())) {
        util::Log(logging::LOG_WARNING, FROM_HERE,
                  "Failed to parse a Tracker");
        continue;
      }
      contents->file_trackers.push_back(std::move(tracker));
      continue;
    }
  }
}

void RemoveUnreachableItemsFromDB(DatabaseContents* contents,
                                  int64_t sync_root_tracker_id,
                                  LevelDBWrapper* db) {
  typedef std::map<int64_t, std::set<int64_t>> ChildTrackersByParent;
  ChildTrackersByParent trackers_by_parent;

  // Set up links from parent tracker to child trackers.
  for (size_t i = 0; i < contents->file_trackers.size(); ++i) {
    const FileTracker& tracker = *contents->file_trackers[i];
    int64_t parent_tracker_id = tracker.parent_tracker_id();
    int64_t tracker_id = tracker.tracker_id();

    trackers_by_parent[parent_tracker_id].insert(tracker_id);
  }

  // Drop links from inactive trackers.
  for (size_t i = 0; i < contents->file_trackers.size(); ++i) {
    const FileTracker& tracker = *contents->file_trackers[i];

    if (!tracker.active())
      trackers_by_parent.erase(tracker.tracker_id());
  }

  std::vector<int64_t> pending;
  if (sync_root_tracker_id != kInvalidTrackerID)
    pending.push_back(sync_root_tracker_id);

  // Traverse tracker tree from sync-root.
  std::set<int64_t> visited_trackers;
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

  // Delete all unreachable trackers.
  std::vector<std::unique_ptr<FileTracker>> reachable_trackers;
  for (size_t i = 0; i < contents->file_trackers.size(); ++i) {
    std::unique_ptr<FileTracker>& tracker = contents->file_trackers[i];
    if (base::Contains(visited_trackers, tracker->tracker_id())) {
      reachable_trackers.push_back(std::move(tracker));
    } else {
      PutFileTrackerDeletionToDB(tracker->tracker_id(), db);
    }
  }
  contents->file_trackers = std::move(reachable_trackers);

  // List all |file_id| referred by a tracker.
  std::unordered_set<std::string> referred_file_ids;
  for (size_t i = 0; i < contents->file_trackers.size(); ++i)
    referred_file_ids.insert(contents->file_trackers[i]->file_id());

  // Delete all unreferred metadata.
  std::vector<std::unique_ptr<FileMetadata>> referred_file_metadata;
  for (size_t i = 0; i < contents->file_metadata.size(); ++i) {
    std::unique_ptr<FileMetadata>& metadata = contents->file_metadata[i];
    if (base::Contains(referred_file_ids, metadata->file_id())) {
      referred_file_metadata.push_back(std::move(metadata));
    } else {
      PutFileMetadataDeletionToDB(metadata->file_id(), db);
    }
  }
  contents->file_metadata = std::move(referred_file_metadata);
}

}  // namespace

// static
std::unique_ptr<MetadataDatabaseIndex> MetadataDatabaseIndex::Create(
    LevelDBWrapper* db) {
  DCHECK(db);

  std::unique_ptr<ServiceMetadata> service_metadata =
      InitializeServiceMetadata(db);
  if (!service_metadata)
    return std::unique_ptr<MetadataDatabaseIndex>();

  DatabaseContents contents;
  PutVersionToDB(kCurrentDatabaseVersion, db);
  ReadDatabaseContents(db, &contents);
  RemoveUnreachableItemsFromDB(&contents,
                               service_metadata->sync_root_tracker_id(),
                               db);

  std::unique_ptr<MetadataDatabaseIndex> index(new MetadataDatabaseIndex(db));
  index->Initialize(std::move(service_metadata), &contents);
  return index;
}

// static
std::unique_ptr<MetadataDatabaseIndex> MetadataDatabaseIndex::CreateForTesting(
    DatabaseContents* contents,
    LevelDBWrapper* db) {
  std::unique_ptr<MetadataDatabaseIndex> index(new MetadataDatabaseIndex(db));
  index->Initialize(base::WrapUnique(new ServiceMetadata), contents);
  return index;
}

void MetadataDatabaseIndex::Initialize(
    std::unique_ptr<ServiceMetadata> service_metadata,
    DatabaseContents* contents) {
  service_metadata_ = std::move(service_metadata);

  for (size_t i = 0; i < contents->file_metadata.size(); ++i)
    StoreFileMetadata(std::move(contents->file_metadata[i]));
  contents->file_metadata.clear();

  for (size_t i = 0; i < contents->file_trackers.size(); ++i)
    StoreFileTracker(std::move(contents->file_trackers[i]));
  contents->file_trackers.clear();

  UMA_HISTOGRAM_COUNTS_1M("SyncFileSystem.MetadataNumber",
                          metadata_by_id_.size());
  UMA_HISTOGRAM_COUNTS_1M("SyncFileSystem.TrackerNumber",
                          tracker_by_id_.size());
  UMA_HISTOGRAM_COUNTS_100("SyncFileSystem.RegisteredAppNumber",
                           app_root_by_app_id_.size());
}

MetadataDatabaseIndex::MetadataDatabaseIndex(LevelDBWrapper* db) : db_(db) {}
MetadataDatabaseIndex::~MetadataDatabaseIndex() {}

void MetadataDatabaseIndex::RemoveUnreachableItems() {
  // Do nothing. MetadataDatabaseIndex is behind a private flag and will be
  // removed soon.
  // TODO(crbug.com/568008): Remove MetadataDatabaseIndex.
}

bool MetadataDatabaseIndex::GetFileMetadata(
    const std::string& file_id, FileMetadata* metadata) const {
  auto it = metadata_by_id_.find(file_id);
  if (it == metadata_by_id_.end())
    return false;
  FileMetadata* identified = it->second.get();
  if (metadata)
    metadata->CopyFrom(*identified);
  return true;
}

bool MetadataDatabaseIndex::GetFileTracker(int64_t tracker_id,
                                           FileTracker* tracker) const {
  auto it = tracker_by_id_.find(tracker_id);
  if (it == tracker_by_id_.end())
    return false;
  FileTracker* identified = it->second.get();
  if (tracker)
    tracker->CopyFrom(*identified);
  return true;
}

void MetadataDatabaseIndex::StoreFileMetadata(
    std::unique_ptr<FileMetadata> metadata) {
  PutFileMetadataToDB(*metadata.get(), db_);
  if (!metadata) {
    NOTREACHED();
    return;
  }

  std::string file_id = metadata->file_id();
  metadata_by_id_[file_id] = std::move(metadata);
}

void MetadataDatabaseIndex::StoreFileTracker(
    std::unique_ptr<FileTracker> tracker) {
  PutFileTrackerToDB(*tracker.get(), db_);
  if (!tracker) {
    NOTREACHED();
    return;
  }

  int64_t tracker_id = tracker->tracker_id();
  auto old_tracker_it = tracker_by_id_.find(tracker_id);

  if (old_tracker_it == tracker_by_id_.end()) {
    DVLOG(3) << "Adding new tracker: " << tracker->tracker_id()
             << " " << GetTrackerTitle(*tracker);

    AddToAppIDIndex(*tracker);
    AddToPathIndexes(*tracker);
    AddToFileIDIndexes(*tracker);
    AddToDirtyTrackerIndexes(*tracker);
  } else {
    DVLOG(3) << "Updating tracker: " << tracker->tracker_id()
             << " " << GetTrackerTitle(*tracker);

    FileTracker* old_tracker = old_tracker_it->second.get();
    UpdateInAppIDIndex(*old_tracker, *tracker);
    UpdateInPathIndexes(*old_tracker, *tracker);
    UpdateInFileIDIndexes(*old_tracker, *tracker);
    UpdateInDirtyTrackerIndexes(*old_tracker, *tracker);
  }

  tracker_by_id_[tracker_id] = std::move(tracker);
}

void MetadataDatabaseIndex::RemoveFileMetadata(const std::string& file_id) {
  PutFileMetadataDeletionToDB(file_id, db_);
  metadata_by_id_.erase(file_id);
}

void MetadataDatabaseIndex::RemoveFileTracker(int64_t tracker_id) {
  PutFileTrackerDeletionToDB(tracker_id, db_);

  auto tracker_it = tracker_by_id_.find(tracker_id);
  if (tracker_it == tracker_by_id_.end()) {
    NOTREACHED();
    return;
  }
  FileTracker* tracker = tracker_it->second.get();

  DVLOG(3) << "Removing tracker: "
           << tracker->tracker_id() << " " << GetTrackerTitle(*tracker);

  RemoveFromAppIDIndex(*tracker);
  RemoveFromPathIndexes(*tracker);
  RemoveFromFileIDIndexes(*tracker);
  RemoveFromDirtyTrackerIndexes(*tracker);

  tracker_by_id_.erase(tracker_id);
}

TrackerIDSet MetadataDatabaseIndex::GetFileTrackerIDsByFileID(
    const std::string& file_id) const {
  return FindItem(trackers_by_file_id_, file_id);
}

int64_t MetadataDatabaseIndex::GetAppRootTracker(
    const std::string& app_id) const {
  return FindItem(app_root_by_app_id_, app_id);
}

TrackerIDSet MetadataDatabaseIndex::GetFileTrackerIDsByParentAndTitle(
    int64_t parent_tracker_id,
    const std::string& title) const {
  auto found = trackers_by_parent_and_title_.find(parent_tracker_id);
  if (found == trackers_by_parent_and_title_.end())
    return TrackerIDSet();
  return FindItem(found->second, title);
}

std::vector<int64_t> MetadataDatabaseIndex::GetFileTrackerIDsByParent(
    int64_t parent_tracker_id) const {
  std::vector<int64_t> result;
  auto found = trackers_by_parent_and_title_.find(parent_tracker_id);
  if (found == trackers_by_parent_and_title_.end())
    return result;

  for (auto itr = found->second.begin(); itr != found->second.end(); ++itr) {
    result.insert(result.end(), itr->second.begin(), itr->second.end());
  }

  return result;
}

std::string MetadataDatabaseIndex::PickMultiTrackerFileID() const {
  if (multi_tracker_file_ids_.empty())
    return std::string();
  return *multi_tracker_file_ids_.begin();
}

ParentIDAndTitle MetadataDatabaseIndex::PickMultiBackingFilePath() const {
  if (multi_backing_file_paths_.empty())
    return ParentIDAndTitle(kInvalidTrackerID, std::string());
  return *multi_backing_file_paths_.begin();
}

int64_t MetadataDatabaseIndex::PickDirtyTracker() const {
  if (dirty_trackers_.empty())
    return kInvalidTrackerID;
  return *dirty_trackers_.begin();
}

void MetadataDatabaseIndex::DemoteDirtyTracker(int64_t tracker_id) {
  if (dirty_trackers_.erase(tracker_id))
    demoted_dirty_trackers_.insert(tracker_id);
}

bool MetadataDatabaseIndex::HasDemotedDirtyTracker() const {
  return !demoted_dirty_trackers_.empty();
}

bool MetadataDatabaseIndex::IsDemotedDirtyTracker(int64_t tracker_id) const {
  return demoted_dirty_trackers_.find(tracker_id) !=
      demoted_dirty_trackers_.end();
}

void MetadataDatabaseIndex::PromoteDemotedDirtyTracker(int64_t tracker_id) {
  if (demoted_dirty_trackers_.erase(tracker_id) == 1)
    dirty_trackers_.insert(tracker_id);
}

bool MetadataDatabaseIndex::PromoteDemotedDirtyTrackers() {
  bool promoted = !demoted_dirty_trackers_.empty();
  dirty_trackers_.insert(demoted_dirty_trackers_.begin(),
                         demoted_dirty_trackers_.end());
  demoted_dirty_trackers_.clear();
  return promoted;
}

size_t MetadataDatabaseIndex::CountDirtyTracker() const {
  return dirty_trackers_.size();
}

size_t MetadataDatabaseIndex::CountFileMetadata() const {
  return metadata_by_id_.size();
}

size_t MetadataDatabaseIndex::CountFileTracker() const {
  return tracker_by_id_.size();
}

void MetadataDatabaseIndex::SetSyncRootRevalidated() const {
  service_metadata_->set_sync_root_revalidated(true);
  PutServiceMetadataToDB(*service_metadata_, db_);
}

void MetadataDatabaseIndex::SetSyncRootTrackerID(int64_t sync_root_id) const {
  service_metadata_->set_sync_root_tracker_id(sync_root_id);
  PutServiceMetadataToDB(*service_metadata_, db_);
}

void MetadataDatabaseIndex::SetLargestChangeID(
    int64_t largest_change_id) const {
  service_metadata_->set_largest_change_id(largest_change_id);
  PutServiceMetadataToDB(*service_metadata_, db_);
}

void MetadataDatabaseIndex::SetNextTrackerID(int64_t next_tracker_id) const {
  service_metadata_->set_next_tracker_id(next_tracker_id);
  PutServiceMetadataToDB(*service_metadata_, db_);
}

bool MetadataDatabaseIndex::IsSyncRootRevalidated() const {
  return service_metadata_->has_sync_root_revalidated() &&
      service_metadata_->sync_root_revalidated();
}

int64_t MetadataDatabaseIndex::GetSyncRootTrackerID() const {
  if (!service_metadata_->has_sync_root_tracker_id())
    return kInvalidTrackerID;
  return service_metadata_->sync_root_tracker_id();
}

int64_t MetadataDatabaseIndex::GetLargestChangeID() const {
  if (!service_metadata_->has_largest_change_id())
    return kInvalidTrackerID;
  return service_metadata_->largest_change_id();
}

int64_t MetadataDatabaseIndex::GetNextTrackerID() const {
  if (!service_metadata_->has_next_tracker_id()) {
    NOTREACHED();
    return kInvalidTrackerID;
  }
  return service_metadata_->next_tracker_id();
}

std::vector<std::string> MetadataDatabaseIndex::GetRegisteredAppIDs() const {
  std::vector<std::string> result;
  result.reserve(app_root_by_app_id_.size());
  for (const auto& pair : app_root_by_app_id_)
    result.push_back(pair.first);
  return result;
}

std::vector<int64_t> MetadataDatabaseIndex::GetAllTrackerIDs() const {
  std::vector<int64_t> result;
  for (const auto& pair : tracker_by_id_)
    result.push_back(pair.first);
  return result;
}

std::vector<std::string> MetadataDatabaseIndex::GetAllMetadataIDs() const {
  std::vector<std::string> result;
  for (const auto& pair : metadata_by_id_)
    result.push_back(pair.first);
  return result;
}

void MetadataDatabaseIndex::AddToAppIDIndex(
    const FileTracker& new_tracker) {
  if (!IsAppRoot(new_tracker))
    return;

  DVLOG(3) << "  Add to app_root_by_app_id_: " << new_tracker.app_id();

  DCHECK(new_tracker.active());
  DCHECK(!base::Contains(app_root_by_app_id_, new_tracker.app_id()));
  app_root_by_app_id_[new_tracker.app_id()] = new_tracker.tracker_id();
}

void MetadataDatabaseIndex::UpdateInAppIDIndex(
    const FileTracker& old_tracker,
    const FileTracker& new_tracker) {
  DCHECK_EQ(old_tracker.tracker_id(), new_tracker.tracker_id());

  if (IsAppRoot(old_tracker) && !IsAppRoot(new_tracker)) {
    DCHECK(old_tracker.active());
    DCHECK(!new_tracker.active());
    DCHECK(base::Contains(app_root_by_app_id_, old_tracker.app_id()));

    DVLOG(3) << "  Remove from app_root_by_app_id_: " << old_tracker.app_id();

    app_root_by_app_id_.erase(old_tracker.app_id());
  } else if (!IsAppRoot(old_tracker) && IsAppRoot(new_tracker)) {
    DCHECK(!old_tracker.active());
    DCHECK(new_tracker.active());
    DCHECK(!base::Contains(app_root_by_app_id_, new_tracker.app_id()));

    DVLOG(3) << "  Add to app_root_by_app_id_: " << new_tracker.app_id();

    app_root_by_app_id_[new_tracker.app_id()] = new_tracker.tracker_id();
  }
}

void MetadataDatabaseIndex::RemoveFromAppIDIndex(
    const FileTracker& tracker) {
  if (IsAppRoot(tracker)) {
    DCHECK(tracker.active());
    DCHECK(base::Contains(app_root_by_app_id_, tracker.app_id()));

    DVLOG(3) << "  Remove from app_root_by_app_id_: " << tracker.app_id();

    app_root_by_app_id_.erase(tracker.app_id());
  }
}

void MetadataDatabaseIndex::AddToFileIDIndexes(
    const FileTracker& new_tracker) {
  DVLOG(3) << "  Add to trackers_by_file_id_: " << new_tracker.file_id();

  trackers_by_file_id_[new_tracker.file_id()].Insert(new_tracker);

  if (trackers_by_file_id_[new_tracker.file_id()].size() > 1) {
    DVLOG_IF(3, !base::Contains(multi_tracker_file_ids_, new_tracker.file_id()))
        << "  Add to multi_tracker_file_ids_: " << new_tracker.file_id();
    multi_tracker_file_ids_.insert(new_tracker.file_id());
  }
}

void MetadataDatabaseIndex::UpdateInFileIDIndexes(
    const FileTracker& old_tracker,
    const FileTracker& new_tracker) {
  DCHECK_EQ(old_tracker.tracker_id(), new_tracker.tracker_id());
  DCHECK_EQ(old_tracker.file_id(), new_tracker.file_id());

  std::string file_id = new_tracker.file_id();
  DCHECK(base::Contains(trackers_by_file_id_, file_id));

  if (old_tracker.active() && !new_tracker.active())
    trackers_by_file_id_[file_id].Deactivate(new_tracker.tracker_id());
  else if (!old_tracker.active() && new_tracker.active())
    trackers_by_file_id_[file_id].Activate(new_tracker.tracker_id());
}

void MetadataDatabaseIndex::RemoveFromFileIDIndexes(
    const FileTracker& tracker) {
  auto found = trackers_by_file_id_.find(tracker.file_id());
  if (found == trackers_by_file_id_.end()) {
    NOTREACHED();
    return;
  }

  DVLOG(3) << "  Remove from trackers_by_file_id_: "
           << tracker.tracker_id();
  found->second.Erase(tracker.tracker_id());

  if (trackers_by_file_id_[tracker.file_id()].size() <= 1) {
    DVLOG_IF(3, base::Contains(multi_tracker_file_ids_, tracker.file_id()))
        << "  Remove from multi_tracker_file_ids_: " << tracker.file_id();
    multi_tracker_file_ids_.erase(tracker.file_id());
  }

  if (found->second.empty())
    trackers_by_file_id_.erase(found);
}

void MetadataDatabaseIndex::AddToPathIndexes(
    const FileTracker& new_tracker) {
  int64_t parent = new_tracker.parent_tracker_id();
  std::string title = GetTrackerTitle(new_tracker);

  DVLOG(3) << "  Add to trackers_by_parent_and_title_: "
           << parent << " " << title;

  trackers_by_parent_and_title_[parent][title].Insert(new_tracker);

  if (trackers_by_parent_and_title_[parent][title].size() > 1 &&
      !title.empty()) {
    DVLOG_IF(3, !base::Contains(multi_backing_file_paths_,
                                ParentIDAndTitle(parent, title)))
        << "  Add to multi_backing_file_paths_: " << parent << " " << title;
    multi_backing_file_paths_.insert(ParentIDAndTitle(parent, title));
  }
}

void MetadataDatabaseIndex::UpdateInPathIndexes(
    const FileTracker& old_tracker,
    const FileTracker& new_tracker) {
  DCHECK_EQ(old_tracker.tracker_id(), new_tracker.tracker_id());
  DCHECK_EQ(old_tracker.parent_tracker_id(), new_tracker.parent_tracker_id());
  DCHECK(GetTrackerTitle(old_tracker) == GetTrackerTitle(new_tracker) ||
         !old_tracker.has_synced_details());

  int64_t tracker_id = new_tracker.tracker_id();
  int64_t parent = new_tracker.parent_tracker_id();
  std::string old_title = GetTrackerTitle(old_tracker);
  std::string title = GetTrackerTitle(new_tracker);

  TrackerIDsByTitle* trackers_by_title = &trackers_by_parent_and_title_[parent];

  if (old_title != title) {
    auto found = trackers_by_title->find(old_title);
    if (found != trackers_by_title->end()) {
      DVLOG(3) << "  Remove from trackers_by_parent_and_title_: "
             << parent << " " << old_title;

      found->second.Erase(tracker_id);
      if (found->second.empty())
        trackers_by_title->erase(found);
    } else {
      NOTREACHED();
    }

    DVLOG(3) << "  Add to trackers_by_parent_and_title_: "
             << parent << " " << title;

    (*trackers_by_title)[title].Insert(new_tracker);

    if (trackers_by_parent_and_title_[parent][old_title].size() <= 1 &&
        !old_title.empty()) {
      DVLOG_IF(3, base::Contains(multi_backing_file_paths_,
                                 ParentIDAndTitle(parent, old_title)))
          << "  Remove from multi_backing_file_paths_: " << parent << " "
          << old_title;
      multi_backing_file_paths_.erase(ParentIDAndTitle(parent, old_title));
    }

    if (trackers_by_parent_and_title_[parent][title].size() > 1 &&
        !title.empty()) {
      DVLOG_IF(3, !base::Contains(multi_backing_file_paths_,
                                  ParentIDAndTitle(parent, title)))
          << "  Add to multi_backing_file_paths_: " << parent << " " << title;
      multi_backing_file_paths_.insert(ParentIDAndTitle(parent, title));
    }

    return;
  }

  if (old_tracker.active() && !new_tracker.active())
    trackers_by_parent_and_title_[parent][title].Deactivate(tracker_id);
  else if (!old_tracker.active() && new_tracker.active())
    trackers_by_parent_and_title_[parent][title].Activate(tracker_id);
}

void MetadataDatabaseIndex::RemoveFromPathIndexes(
    const FileTracker& tracker) {
  int64_t tracker_id = tracker.tracker_id();
  int64_t parent = tracker.parent_tracker_id();
  std::string title = GetTrackerTitle(tracker);

  DCHECK(base::Contains(trackers_by_parent_and_title_, parent));
  DCHECK(base::Contains(trackers_by_parent_and_title_[parent], title));

  DVLOG(3) << "  Remove from trackers_by_parent_and_title_: "
           << parent << " " << title;

  trackers_by_parent_and_title_[parent][title].Erase(tracker_id);

  if (trackers_by_parent_and_title_[parent][title].size() <= 1 &&
      !title.empty()) {
    DVLOG_IF(3, base::Contains(multi_backing_file_paths_,
                               ParentIDAndTitle(parent, title)))
        << "  Remove from multi_backing_file_paths_: " << parent << " "
        << title;
    multi_backing_file_paths_.erase(ParentIDAndTitle(parent, title));
  }

  if (trackers_by_parent_and_title_[parent][title].empty()) {
    trackers_by_parent_and_title_[parent].erase(title);
    if (trackers_by_parent_and_title_[parent].empty())
      trackers_by_parent_and_title_.erase(parent);
  }
}

void MetadataDatabaseIndex::AddToDirtyTrackerIndexes(
    const FileTracker& new_tracker) {
  DCHECK(!base::Contains(dirty_trackers_, new_tracker.tracker_id()));
  DCHECK(!base::Contains(demoted_dirty_trackers_, new_tracker.tracker_id()));

  if (new_tracker.dirty()) {
    DVLOG(3) << "  Add to dirty_trackers_: " << new_tracker.tracker_id();
    dirty_trackers_.insert(new_tracker.tracker_id());
  }
}

void MetadataDatabaseIndex::UpdateInDirtyTrackerIndexes(
    const FileTracker& old_tracker,
    const FileTracker& new_tracker) {
  DCHECK_EQ(old_tracker.tracker_id(), new_tracker.tracker_id());

  int64_t tracker_id = new_tracker.tracker_id();
  if (old_tracker.dirty() && !new_tracker.dirty()) {
    DCHECK(base::Contains(dirty_trackers_, tracker_id) ||
           base::Contains(demoted_dirty_trackers_, tracker_id));

    DVLOG(3) << "  Remove from dirty_trackers_: " << tracker_id;

    dirty_trackers_.erase(tracker_id);
    demoted_dirty_trackers_.erase(tracker_id);
  } else if (!old_tracker.dirty() && new_tracker.dirty()) {
    DCHECK(!base::Contains(dirty_trackers_, tracker_id));
    DCHECK(!base::Contains(demoted_dirty_trackers_, tracker_id));

    DVLOG(3) << "  Add to dirty_trackers_: " << tracker_id;

    dirty_trackers_.insert(tracker_id);
  }
}

void MetadataDatabaseIndex::RemoveFromDirtyTrackerIndexes(
    const FileTracker& tracker) {
  if (tracker.dirty()) {
    int64_t tracker_id = tracker.tracker_id();
    DCHECK(base::Contains(dirty_trackers_, tracker_id) ||
           base::Contains(demoted_dirty_trackers_, tracker_id));

    DVLOG(3) << "  Remove from dirty_trackers_: " << tracker_id;
    dirty_trackers_.erase(tracker_id);

    demoted_dirty_trackers_.erase(tracker_id);
  }
}

}  // namespace drive_backend
}  // namespace sync_file_system
