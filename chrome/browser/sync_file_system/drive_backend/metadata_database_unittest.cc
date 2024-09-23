// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"

#include <stdint.h>

#include <unordered_map>
#include <utility>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_constants.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_test_util.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_util.h"
#include "chrome/browser/sync_file_system/drive_backend/leveldb_wrapper.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.pb.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database_index.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database_index_interface.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database_index_on_disk.h"
#include "chrome/browser/sync_file_system/sync_file_system_test_util.h"
#include "google_apis/drive/drive_api_parser.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/leveldb_chrome.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

#define FPL(a) FILE_PATH_LITERAL(a)

namespace sync_file_system {
namespace drive_backend {

namespace {

typedef MetadataDatabase::FileIDList FileIDList;

const int64_t kInitialChangeID = 1234;
const int64_t kSyncRootTrackerID = 100;
const char kSyncRootFolderID[] = "sync_root_folder_id";

// This struct is used to setup initial state of the database in the test and
// also used to match to the modified content of the database as the
// expectation.
struct TrackedFile {
  // Holds the latest remote metadata which may be not-yet-synced to |tracker|.
  FileMetadata metadata;
  FileTracker tracker;

  // Implies the file should not in the database.
  bool should_be_absent;

  // Implies the file should have a tracker in the database but should have no
  // metadata.
  bool tracker_only;

  TrackedFile() : should_be_absent(false), tracker_only(false) {}
};

void ExpectEquivalentServiceMetadata(
    const MetadataDatabaseIndexInterface* left,
    const MetadataDatabaseIndexInterface* right) {
  EXPECT_EQ(left->GetLargestChangeID(), right->GetLargestChangeID());
  EXPECT_EQ(left->GetSyncRootTrackerID(), right->GetSyncRootTrackerID());
  EXPECT_EQ(left->GetNextTrackerID(), right->GetNextTrackerID());
}

void ExpectEquivalent(const FileMetadata* left, const FileMetadata* right) {
  if (!left) {
    ASSERT_FALSE(right);
    return;
  }
  ASSERT_TRUE(right);
  test_util::ExpectEquivalentMetadata(*left, *right);
}

void ExpectEquivalent(const FileTracker* left, const FileTracker* right) {
  if (!left) {
    ASSERT_FALSE(right);
    return;
  }
  ASSERT_TRUE(right);
  test_util::ExpectEquivalentTrackers(*left, *right);
}

void ExpectEquivalent(int64_t left, int64_t right) {
  EXPECT_EQ(left, right);
}

template <typename Container>
void ExpectEquivalentMaps(const Container& left, const Container& right);

template <typename Key, typename Value>
void ExpectEquivalent(const std::map<Key, Value>& left,
                      const std::map<Key, Value>& right) {
  ExpectEquivalentMaps(left, right);
}

template <typename Key, typename Value>
void ExpectEquivalent(const std::unordered_map<Key, Value>& left,
                      const std::unordered_map<Key, Value>& right) {
  // Convert from a hash container to an ordered container for comparison.
  ExpectEquivalentMaps(std::map<Key, Value>(left.begin(), left.end()),
                       std::map<Key, Value>(right.begin(), right.end()));
}

template <typename Key, typename Value>
void ExpectEquivalent(
    const std::unordered_map<Key, std::unique_ptr<Value>>& left,
    const std::unordered_map<Key, std::unique_ptr<Value>>& right) {
  // Convert from a hash container to an ordered container for comparison.
  std::map<Key, Value*> left_ordered;
  std::map<Key, Value*> right_ordered;
  for (const auto& item : left)
    left_ordered[item.first] = item.second.get();
  for (const auto& item : right)
    right_ordered[item.first] = item.second.get();

  ExpectEquivalentMaps(left_ordered, right_ordered);
}

template <typename Container>
void ExpectEquivalentSets(const Container& left, const Container& right);

template <typename Value, typename Comparator>
void ExpectEquivalent(const std::set<Value, Comparator>& left,
                      const std::set<Value, Comparator>& right) {
  return ExpectEquivalentSets(left, right);
}

template <typename Value>
void ExpectEquivalent(const std::unordered_set<Value>& left,
                      const std::unordered_set<Value>& right) {
  // Convert from a hash container to an ordered container for comparison.
  return ExpectEquivalentSets(std::set<Value>(left.begin(), left.end()),
                              std::set<Value>(right.begin(), right.end()));
}

void ExpectEquivalent(const TrackerIDSet& left,
                      const TrackerIDSet& right) {
  {
    SCOPED_TRACE("Expect equivalent active_tracker");
    EXPECT_EQ(left.active_tracker(), right.active_tracker());
  }
  ExpectEquivalent(left.tracker_set(), right.tracker_set());
}

template <typename Container>
void ExpectEquivalentMaps(const Container& left, const Container& right) {
  ASSERT_EQ(left.size(), right.size());

  auto left_itr = left.begin();
  auto right_itr = right.begin();
  while (left_itr != left.end()) {
    EXPECT_EQ(left_itr->first, right_itr->first);
    ExpectEquivalent(left_itr->second, right_itr->second);
    ++left_itr;
    ++right_itr;
  }
}

template <typename Container>
void ExpectEquivalentSets(const Container& left, const Container& right) {
  ASSERT_EQ(left.size(), right.size());

  auto left_itr = left.begin();
  auto right_itr = right.begin();
  while (left_itr != left.end()) {
    ExpectEquivalent(*left_itr, *right_itr);
    ++left_itr;
    ++right_itr;
  }
}

base::FilePath CreateNormalizedPath(const base::FilePath::StringType& path) {
  return base::FilePath(path).NormalizePathSeparators();
}

}  // namespace

class MetadataDatabaseTest : public testing::TestWithParam<bool> {
 public:
  MetadataDatabaseTest()
      : current_change_id_(kInitialChangeID),
        next_tracker_id_(kSyncRootTrackerID + 1),
        next_file_id_number_(1),
        next_md5_sequence_number_(1) {}

  MetadataDatabaseTest(const MetadataDatabaseTest&) = delete;
  MetadataDatabaseTest& operator=(const MetadataDatabaseTest&) = delete;

  virtual ~MetadataDatabaseTest() {}

  void SetUp() override {
    ASSERT_TRUE(database_dir_.CreateUniqueTempDir());
    in_memory_env_ = leveldb_chrome::NewMemEnv("MetadataDatabaseTest");
  }

  void TearDown() override { DropDatabase(); }

 protected:
  std::string GenerateFileID() {
    return "file_id_" + base::NumberToString(next_file_id_number_++);
  }

  int64_t GetTrackerIDByFileID(const std::string& file_id) {
    TrackerIDSet trackers;
    if (metadata_database_->FindTrackersByFileID(file_id, &trackers)) {
      EXPECT_FALSE(trackers.empty());
      return *trackers.begin();
    }
    return 0;
  }

  bool enable_on_disk_index() const { return GetParam(); }

  leveldb::Env* in_memory_env() const { return in_memory_env_.get(); }

  const base::FilePath& DatabasePath() const { return database_dir_.GetPath(); }

  SyncStatusCode InitializeMetadataDatabase() {
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    metadata_database_ = MetadataDatabase::CreateInternal(
        DatabasePath(), in_memory_env_.get(), enable_on_disk_index(), &status);
    return status;
  }

  void DropDatabase() {
    metadata_database_.reset();
    base::RunLoop().RunUntilIdle();
  }

  void SetUpDatabaseByTrackedFiles(const TrackedFile** tracked_files,
                                   int size) {
    std::unique_ptr<LevelDBWrapper> db = InitializeLevelDB();
    ASSERT_TRUE(db);

    for (int i = 0; i < size; ++i) {
      const TrackedFile* file = tracked_files[i];
      if (file->should_be_absent)
        continue;
      if (!file->tracker_only)
        EXPECT_TRUE(PutFileToDB(db.get(), file->metadata).ok());
      EXPECT_TRUE(PutTrackerToDB(db.get(), file->tracker).ok());
    }
  }

  void VerifyTrackedFile(const TrackedFile& file) {
    if (!file.should_be_absent) {
      if (file.tracker_only) {
        EXPECT_FALSE(metadata_database()->FindFileByFileID(
            file.metadata.file_id(), nullptr));
      } else {
        VerifyFile(file.metadata);
      }
      VerifyTracker(file.tracker);
      return;
    }

    EXPECT_FALSE(metadata_database()->FindFileByFileID(
        file.metadata.file_id(), nullptr));
    EXPECT_FALSE(metadata_database()->FindTrackerByTrackerID(
        file.tracker.tracker_id(), nullptr));
  }

  void VerifyTrackedFiles(const TrackedFile** tracked_files, int size) {
    for (int i = 0; i < size; ++i)
      VerifyTrackedFile(*tracked_files[i]);
  }

  MetadataDatabase* metadata_database() { return metadata_database_.get(); }

  std::unique_ptr<LevelDBWrapper> InitializeLevelDB() {
    std::unique_ptr<leveldb::DB> db;
    leveldb_env::Options options;
    options.create_if_missing = true;
    options.max_open_files = 0;  // Use minimum.
    options.env = in_memory_env_.get();
    leveldb::Status status =
        leveldb_env::OpenDB(options, DatabasePath().AsUTF8Unsafe(), &db);
    EXPECT_TRUE(status.ok());

    std::unique_ptr<LevelDBWrapper> wrapper(new LevelDBWrapper(std::move(db)));

    wrapper->Put(kDatabaseVersionKey, base::NumberToString(3));
    SetUpServiceMetadata(wrapper.get());

    return wrapper;
  }

  void SetUpServiceMetadata(LevelDBWrapper* db) {
    ServiceMetadata service_metadata;
    service_metadata.set_largest_change_id(kInitialChangeID);
    service_metadata.set_sync_root_tracker_id(kSyncRootTrackerID);
    service_metadata.set_next_tracker_id(next_tracker_id_);
    PutServiceMetadataToDB(service_metadata, db);
    EXPECT_TRUE(db->Commit().ok());
  }

  FileMetadata CreateSyncRootMetadata() {
    FileMetadata sync_root;
    sync_root.set_file_id(kSyncRootFolderID);
    FileDetails* details = sync_root.mutable_details();
    details->set_title(kSyncRootFolderTitle);
    details->set_file_kind(FILE_KIND_FOLDER);
    details->set_change_id(current_change_id_);
    return sync_root;
  }

  FileMetadata CreateFileMetadata(const FileMetadata& parent,
                                  const std::string& title) {
    FileMetadata file;
    file.set_file_id(GenerateFileID());
    FileDetails* details = file.mutable_details();
    details->add_parent_folder_ids(parent.file_id());
    details->set_title(title);
    details->set_file_kind(FILE_KIND_FILE);
    details->set_md5("md5_value_" +
                     base::NumberToString(next_md5_sequence_number_++));
    details->set_change_id(current_change_id_);
    return file;
  }

  FileMetadata CreateFolderMetadata(const FileMetadata& parent,
                                    const std::string& title) {
    FileMetadata folder;
    folder.set_file_id(GenerateFileID());
    FileDetails* details = folder.mutable_details();
    details->add_parent_folder_ids(parent.file_id());
    details->set_title(title);
    details->set_file_kind(FILE_KIND_FOLDER);
    details->set_change_id(current_change_id_);
    return folder;
  }

  FileTracker CreateSyncRootTracker(const FileMetadata& sync_root) {
    FileTracker sync_root_tracker;
    sync_root_tracker.set_tracker_id(kSyncRootTrackerID);
    sync_root_tracker.set_parent_tracker_id(0);
    sync_root_tracker.set_file_id(sync_root.file_id());
    sync_root_tracker.set_dirty(false);
    sync_root_tracker.set_active(true);
    sync_root_tracker.set_needs_folder_listing(false);
    *sync_root_tracker.mutable_synced_details() = sync_root.details();
    return sync_root_tracker;
  }

  FileTracker CreateTracker(const FileTracker& parent_tracker,
                            const FileMetadata& file) {
    FileTracker tracker;
    tracker.set_tracker_id(next_tracker_id_++);
    tracker.set_parent_tracker_id(parent_tracker.tracker_id());
    tracker.set_file_id(file.file_id());
    tracker.set_app_id(parent_tracker.app_id());
    tracker.set_tracker_kind(TRACKER_KIND_REGULAR);
    tracker.set_dirty(false);
    tracker.set_active(true);
    tracker.set_needs_folder_listing(false);
    *tracker.mutable_synced_details() = file.details();
    return tracker;
  }

  TrackedFile CreateTrackedSyncRoot() {
    TrackedFile sync_root;
    sync_root.metadata = CreateSyncRootMetadata();
    sync_root.tracker = CreateSyncRootTracker(sync_root.metadata);
    return sync_root;
  }

  TrackedFile CreateTrackedAppRoot(const TrackedFile& sync_root,
                                   const std::string& app_id) {
    TrackedFile app_root(CreateTrackedFolder(sync_root, app_id));
    app_root.tracker.set_app_id(app_id);
    app_root.tracker.set_tracker_kind(TRACKER_KIND_APP_ROOT);
    return app_root;
  }

  TrackedFile CreateTrackedFile(const TrackedFile& parent,
                                const std::string& title) {
    TrackedFile file;
    file.metadata = CreateFileMetadata(parent.metadata, title);
    file.tracker = CreateTracker(parent.tracker, file.metadata);
    return file;
  }

  TrackedFile CreateTrackedFolder(const TrackedFile& parent,
                                  const std::string& title) {
    TrackedFile folder;
    folder.metadata = CreateFolderMetadata(parent.metadata, title);
    folder.tracker = CreateTracker(parent.tracker, folder.metadata);
    return folder;
  }

  std::unique_ptr<google_apis::FileResource> CreateFileResourceFromMetadata(
      const FileMetadata& file) {
    std::unique_ptr<google_apis::FileResource> file_resource(
        new google_apis::FileResource);
    for (int i = 0; i < file.details().parent_folder_ids_size(); ++i) {
      google_apis::ParentReference parent;
      parent.set_file_id(file.details().parent_folder_ids(i));
      file_resource->mutable_parents()->push_back(parent);
    }

    file_resource->set_file_id(file.file_id());
    file_resource->set_title(file.details().title());
    if (file.details().file_kind() == FILE_KIND_FOLDER) {
      file_resource->set_mime_type("application/vnd.google-apps.folder");
    } else if (file.details().file_kind() == FILE_KIND_FILE) {
      file_resource->set_mime_type("text/plain");
      file_resource->set_file_size(0);
    } else {
      file_resource->set_mime_type("application/vnd.google-apps.document");
    }
    file_resource->set_md5_checksum(file.details().md5());
    file_resource->set_etag(file.details().etag());
    file_resource->set_created_date(base::Time::FromInternalValue(
        file.details().creation_time()));
    file_resource->set_modified_date(base::Time::FromInternalValue(
        file.details().modification_time()));

    return file_resource;
  }

  std::unique_ptr<google_apis::ChangeResource> CreateChangeResourceFromMetadata(
      const FileMetadata& file) {
    std::unique_ptr<google_apis::ChangeResource> change(
        new google_apis::ChangeResource);
    change->set_change_id(file.details().change_id());
    change->set_type(google_apis::ChangeResource::FILE);
    change->set_file_id(file.file_id());
    change->set_deleted(file.details().missing());
    if (change->is_deleted())
      return change;

    change->set_file(CreateFileResourceFromMetadata(file));
    return change;
  }

  void ApplyRenameChangeToMetadata(const std::string& new_title,
                                   FileMetadata* file) {
    FileDetails* details = file->mutable_details();
    details->set_title(new_title);
    details->set_change_id(++current_change_id_);
  }

  void ApplyReorganizeChangeToMetadata(const std::string& new_parent,
                                       FileMetadata* file) {
    FileDetails* details = file->mutable_details();
    details->clear_parent_folder_ids();
    details->add_parent_folder_ids(new_parent);
    details->set_change_id(++current_change_id_);
  }

  void ApplyContentChangeToMetadata(FileMetadata* file) {
    FileDetails* details = file->mutable_details();
    details->set_md5("md5_value_" +
                     base::NumberToString(next_md5_sequence_number_++));
    details->set_change_id(++current_change_id_);
  }

  void ApplyNoopChangeToMetadata(FileMetadata* file) {
    file->mutable_details()->set_change_id(++current_change_id_);
  }

  void PushToChangeList(
      std::unique_ptr<google_apis::ChangeResource> change,
      std::vector<std::unique_ptr<google_apis::ChangeResource>>* changes) {
    changes->push_back(std::move(change));
  }

  leveldb::Status PutFileToDB(LevelDBWrapper* db, const FileMetadata& file) {
    PutFileMetadataToDB(file, db);
    return db->Commit();
  }

  leveldb::Status PutTrackerToDB(LevelDBWrapper* db,
                                 const FileTracker& tracker) {
    PutFileTrackerToDB(tracker, db);
    return db->Commit();
  }

  void VerifyReloadConsistencyForOnMemory(MetadataDatabaseIndex* index1,
                                          MetadataDatabaseIndex* index2) {
    ExpectEquivalentServiceMetadata(index1, index2);
    {
      SCOPED_TRACE("Expect equivalent metadata_by_id_ contents.");
      ExpectEquivalent(index1->metadata_by_id_, index2->metadata_by_id_);
    }
    {
      SCOPED_TRACE("Expect equivalent tracker_by_id_ contents.");
      ExpectEquivalent(index1->tracker_by_id_, index2->tracker_by_id_);
    }
    {
      SCOPED_TRACE("Expect equivalent trackers_by_file_id_ contents.");
      ExpectEquivalent(index1->trackers_by_file_id_,
                       index2->trackers_by_file_id_);
    }
    {
      SCOPED_TRACE("Expect equivalent app_root_by_app_id_ contents.");
      ExpectEquivalent(index1->app_root_by_app_id_,
                       index2->app_root_by_app_id_);
    }
    {
      SCOPED_TRACE("Expect equivalent trackers_by_parent_and_title_ contents.");
      ExpectEquivalent(index1->trackers_by_parent_and_title_,
                       index2->trackers_by_parent_and_title_);
    }
    {
      SCOPED_TRACE("Expect equivalent dirty_trackers_ contents.");
      ExpectEquivalent(index1->dirty_trackers_, index2->dirty_trackers_);
    }
  }

  void VerifyReloadConsistencyForOnDisk(
      MetadataDatabaseIndexOnDisk* index1,
      MetadataDatabaseIndexOnDisk* index2) {
    ExpectEquivalentServiceMetadata(index1, index2);
    std::unique_ptr<LevelDBWrapper::Iterator> itr1 =
        index1->GetDBForTesting()->NewIterator();
    std::unique_ptr<LevelDBWrapper::Iterator> itr2 =
        index2->GetDBForTesting()->NewIterator();
    for (itr1->SeekToFirst(), itr2->SeekToFirst();
         itr1->Valid() && itr2->Valid();
         itr1->Next(), itr2->Next()) {
      EXPECT_EQ(itr1->key().ToString(), itr2->key().ToString());
      EXPECT_EQ(itr1->value().ToString(), itr2->value().ToString());
    }
    EXPECT_TRUE(!itr1->Valid());
    EXPECT_TRUE(!itr2->Valid());
  }

  void VerifyReloadConsistency() {
    std::unique_ptr<MetadataDatabase> metadata_database_2;
    ASSERT_EQ(SYNC_STATUS_OK, MetadataDatabase::CreateForTesting(
                                  std::move(metadata_database_->db_),
                                  metadata_database_->enable_on_disk_index_,
                                  &metadata_database_2));
    metadata_database_->db_ = std::move(metadata_database_2->db_);

    MetadataDatabaseIndexInterface* index1 = metadata_database_->index_.get();
    MetadataDatabaseIndexInterface* index2 = metadata_database_2->index_.get();
    if (enable_on_disk_index()) {
      VerifyReloadConsistencyForOnDisk(
          static_cast<MetadataDatabaseIndexOnDisk*>(index1),
          static_cast<MetadataDatabaseIndexOnDisk*>(index2));
    } else {
      VerifyReloadConsistencyForOnMemory(
          static_cast<MetadataDatabaseIndex*>(index1),
          static_cast<MetadataDatabaseIndex*>(index2));
    }
  }

  void VerifyFile(const FileMetadata& file) {
    FileMetadata file_in_metadata_database;
    ASSERT_TRUE(metadata_database()->FindFileByFileID(
        file.file_id(), &file_in_metadata_database));

    SCOPED_TRACE("Expect equivalent " + file.file_id());
    ExpectEquivalent(&file, &file_in_metadata_database);
  }

  void VerifyTracker(const FileTracker& tracker) {
    FileTracker tracker_in_metadata_database;
    ASSERT_TRUE(metadata_database()->FindTrackerByTrackerID(
        tracker.tracker_id(), &tracker_in_metadata_database));

    SCOPED_TRACE("Expect equivalent tracker[" +
                 base::NumberToString(tracker.tracker_id()) + "]");
    ExpectEquivalent(&tracker, &tracker_in_metadata_database);
  }

  SyncStatusCode RegisterApp(const std::string& app_id,
                             const std::string& folder_id) {
    return metadata_database_->RegisterApp(app_id, folder_id);
  }

  SyncStatusCode DisableApp(const std::string& app_id) {
    return metadata_database_->DisableApp(app_id);
  }

  SyncStatusCode EnableApp(const std::string& app_id) {
    return metadata_database_->EnableApp(app_id);
  }

  SyncStatusCode UnregisterApp(const std::string& app_id) {
    return metadata_database_->UnregisterApp(app_id);
  }

  SyncStatusCode UpdateByChangeList(
      std::vector<std::unique_ptr<google_apis::ChangeResource>> changes) {
    return metadata_database_->UpdateByChangeList(current_change_id_,
                                                  std::move(changes));
  }

  SyncStatusCode PopulateFolder(const std::string& folder_id,
                                const FileIDList& listed_children) {
    return metadata_database_->PopulateFolderByChildList(
        folder_id, listed_children);
  }

  SyncStatusCode UpdateTracker(const FileTracker& tracker) {
    return metadata_database_->UpdateTracker(
        tracker.tracker_id(), tracker.synced_details());
  }

  SyncStatusCode PopulateInitialData(
      int64_t largest_change_id,
      const google_apis::FileResource& sync_root_folder,
      const std::vector<std::unique_ptr<google_apis::FileResource>>&
          app_root_folders) {
    return metadata_database_->PopulateInitialData(
        largest_change_id, sync_root_folder, app_root_folders);
  }

  void ResetTrackerID(FileTracker* tracker) {
    tracker->set_tracker_id(GetTrackerIDByFileID(tracker->file_id()));
  }

  int64_t current_change_id() const { return current_change_id_; }

 private:
  base::ScopedTempDir database_dir_;
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<leveldb::Env> in_memory_env_;
  std::unique_ptr<MetadataDatabase> metadata_database_;

  int64_t current_change_id_;
  int64_t next_tracker_id_;
  int64_t next_file_id_number_;
  int64_t next_md5_sequence_number_;
};

INSTANTIATE_TEST_SUITE_P(MetadataDatabaseTestWithIndexesOnDisk,
                         MetadataDatabaseTest,
                         ::testing::Values(true, false));

TEST_P(MetadataDatabaseTest, InitializationTest_Empty) {
  EXPECT_EQ(SYNC_STATUS_OK, InitializeMetadataDatabase());
  DropDatabase();
  EXPECT_EQ(SYNC_STATUS_OK, InitializeMetadataDatabase());

  DropDatabase();

  std::unique_ptr<LevelDBWrapper> db = InitializeLevelDB();
  db->Put(kServiceMetadataKey, "Unparsable string");
  EXPECT_TRUE(db->Commit().ok());
  db.reset();

  EXPECT_EQ(SYNC_STATUS_OK, InitializeMetadataDatabase());
}

TEST_P(MetadataDatabaseTest, InitializationTest_SimpleTree) {
  TrackedFile sync_root(CreateTrackedSyncRoot());
  TrackedFile app_root(CreateTrackedFolder(sync_root, "app_id"));
  app_root.tracker.set_app_id(app_root.metadata.details().title());
  app_root.tracker.set_tracker_kind(TRACKER_KIND_APP_ROOT);

  TrackedFile file(CreateTrackedFile(app_root, "file"));
  TrackedFile folder(CreateTrackedFolder(app_root, "folder"));
  TrackedFile file_in_folder(CreateTrackedFile(folder, "file_in_folder"));
  TrackedFile orphaned_file(CreateTrackedFile(sync_root, "orphaned_file"));
  orphaned_file.metadata.mutable_details()->clear_parent_folder_ids();
  orphaned_file.tracker.set_parent_tracker_id(0);

  const TrackedFile* tracked_files[] = {
    &sync_root, &app_root, &file, &folder, &file_in_folder, &orphaned_file
  };

  SetUpDatabaseByTrackedFiles(tracked_files, std::size(tracked_files));
  EXPECT_EQ(SYNC_STATUS_OK, InitializeMetadataDatabase());

  orphaned_file.should_be_absent = true;
  VerifyTrackedFiles(tracked_files, std::size(tracked_files));
}

TEST_P(MetadataDatabaseTest, AppManagementTest) {
  TrackedFile sync_root(CreateTrackedSyncRoot());
  TrackedFile app_root(CreateTrackedFolder(sync_root, "app_id"));
  app_root.tracker.set_app_id(app_root.metadata.details().title());
  app_root.tracker.set_tracker_kind(TRACKER_KIND_APP_ROOT);

  TrackedFile file(CreateTrackedFile(app_root, "file"));
  TrackedFile folder(CreateTrackedFolder(sync_root, "folder"));
  folder.tracker.set_active(false);

  const TrackedFile* tracked_files[] = {
    &sync_root, &app_root, &file, &folder,
  };
  SetUpDatabaseByTrackedFiles(tracked_files, std::size(tracked_files));
  EXPECT_EQ(SYNC_STATUS_OK, InitializeMetadataDatabase());
  VerifyTrackedFiles(tracked_files, std::size(tracked_files));

  folder.tracker.set_app_id("foo");
  EXPECT_EQ(SYNC_STATUS_OK, RegisterApp(
      folder.tracker.app_id(), folder.metadata.file_id()));
  folder.tracker.set_tracker_kind(TRACKER_KIND_APP_ROOT);
  folder.tracker.set_active(true);
  folder.tracker.set_dirty(true);
  folder.tracker.set_needs_folder_listing(true);
  VerifyTrackedFile(folder);
  VerifyReloadConsistency();

  EXPECT_EQ(SYNC_STATUS_OK, DisableApp(folder.tracker.app_id()));
  folder.tracker.set_tracker_kind(TRACKER_KIND_DISABLED_APP_ROOT);
  VerifyTrackedFile(folder);
  VerifyReloadConsistency();

  EXPECT_EQ(SYNC_STATUS_OK, EnableApp(folder.tracker.app_id()));
  folder.tracker.set_tracker_kind(TRACKER_KIND_APP_ROOT);
  VerifyTrackedFile(folder);
  VerifyReloadConsistency();

  EXPECT_EQ(SYNC_STATUS_OK, UnregisterApp(folder.tracker.app_id()));
  folder.tracker.set_app_id(std::string());
  folder.tracker.set_tracker_kind(TRACKER_KIND_REGULAR);
  folder.tracker.set_active(false);
  VerifyTrackedFile(folder);
  VerifyReloadConsistency();

  EXPECT_EQ(SYNC_STATUS_OK, UnregisterApp(app_root.tracker.app_id()));
  app_root.tracker.set_app_id(std::string());
  app_root.tracker.set_tracker_kind(TRACKER_KIND_REGULAR);
  app_root.tracker.set_active(false);
  app_root.tracker.set_dirty(true);
  file.should_be_absent = true;
  VerifyTrackedFile(app_root);
  VerifyTrackedFile(file);
  VerifyReloadConsistency();
}

TEST_P(MetadataDatabaseTest, BuildPathTest) {
  FileMetadata sync_root(CreateSyncRootMetadata());
  FileTracker sync_root_tracker(CreateSyncRootTracker(sync_root));

  FileMetadata app_root(CreateFolderMetadata(sync_root, "app_id"));
  FileTracker app_root_tracker(
      CreateTracker(sync_root_tracker, app_root));
  app_root_tracker.set_app_id(app_root.details().title());
  app_root_tracker.set_tracker_kind(TRACKER_KIND_APP_ROOT);

  FileMetadata folder(CreateFolderMetadata(app_root, "folder"));
  FileTracker folder_tracker(CreateTracker(app_root_tracker, folder));

  FileMetadata file(CreateFileMetadata(folder, "file"));
  FileTracker file_tracker(CreateTracker(folder_tracker, file));

  FileMetadata inactive_folder(CreateFolderMetadata(app_root, "folder"));
  FileTracker inactive_folder_tracker(CreateTracker(app_root_tracker,
                                                    inactive_folder));
  inactive_folder_tracker.set_active(false);

  {
    std::unique_ptr<LevelDBWrapper> db = InitializeLevelDB();
    ASSERT_TRUE(db);

    EXPECT_TRUE(PutFileToDB(db.get(), sync_root).ok());
    EXPECT_TRUE(PutTrackerToDB(db.get(), sync_root_tracker).ok());
    EXPECT_TRUE(PutFileToDB(db.get(), app_root).ok());
    EXPECT_TRUE(PutTrackerToDB(db.get(), app_root_tracker).ok());
    EXPECT_TRUE(PutFileToDB(db.get(), folder).ok());
    EXPECT_TRUE(PutTrackerToDB(db.get(), folder_tracker).ok());
    EXPECT_TRUE(PutFileToDB(db.get(), file).ok());
    EXPECT_TRUE(PutTrackerToDB(db.get(), file_tracker).ok());
  }

  EXPECT_EQ(SYNC_STATUS_OK, InitializeMetadataDatabase());

  base::FilePath path;
  EXPECT_FALSE(metadata_database()->BuildPathForTracker(
      sync_root_tracker.tracker_id(), &path));
  EXPECT_TRUE(metadata_database()->BuildPathForTracker(
      app_root_tracker.tracker_id(), &path));
  EXPECT_EQ(base::FilePath(FPL("/")).NormalizePathSeparators(), path);
  EXPECT_TRUE(metadata_database()->BuildPathForTracker(
      file_tracker.tracker_id(), &path));
  EXPECT_EQ(base::FilePath(FPL("/folder/file")).NormalizePathSeparators(),
            path);
}

TEST_P(MetadataDatabaseTest, FindNearestActiveAncestorTest) {
  const std::string kAppID = "app_id";

  FileMetadata sync_root(CreateSyncRootMetadata());
  FileTracker sync_root_tracker(CreateSyncRootTracker(sync_root));

  FileMetadata app_root(CreateFolderMetadata(sync_root, kAppID));
  FileTracker app_root_tracker(
      CreateTracker(sync_root_tracker, app_root));
  app_root_tracker.set_app_id(app_root.details().title());
  app_root_tracker.set_tracker_kind(TRACKER_KIND_APP_ROOT);

  // Create directory structure like this: "/folder1/folder2/file"
  FileMetadata folder1(CreateFolderMetadata(app_root, "folder1"));
  FileTracker folder_tracker1(CreateTracker(app_root_tracker, folder1));
  FileMetadata folder2(CreateFolderMetadata(folder1, "folder2"));
  FileTracker folder_tracker2(CreateTracker(folder_tracker1, folder2));
  FileMetadata file(CreateFileMetadata(folder2, "file"));
  FileTracker file_tracker(CreateTracker(folder_tracker2, file));

  FileMetadata inactive_folder(CreateFolderMetadata(app_root, "folder1"));
  FileTracker inactive_folder_tracker(CreateTracker(app_root_tracker,
                                                    inactive_folder));
  inactive_folder_tracker.set_active(false);

  {
    std::unique_ptr<LevelDBWrapper> db = InitializeLevelDB();
    ASSERT_TRUE(db);

    EXPECT_TRUE(PutFileToDB(db.get(), sync_root).ok());
    EXPECT_TRUE(PutTrackerToDB(db.get(), sync_root_tracker).ok());
    EXPECT_TRUE(PutFileToDB(db.get(), app_root).ok());
    EXPECT_TRUE(PutTrackerToDB(db.get(), app_root_tracker).ok());
    EXPECT_TRUE(PutFileToDB(db.get(), folder1).ok());
    EXPECT_TRUE(PutTrackerToDB(db.get(), folder_tracker1).ok());
    EXPECT_TRUE(PutFileToDB(db.get(), folder2).ok());
    EXPECT_TRUE(PutTrackerToDB(db.get(), folder_tracker2).ok());
    EXPECT_TRUE(PutFileToDB(db.get(), file).ok());
    EXPECT_TRUE(PutTrackerToDB(db.get(), file_tracker).ok());
    EXPECT_TRUE(PutFileToDB(db.get(), inactive_folder).ok());
    EXPECT_TRUE(PutTrackerToDB(db.get(), inactive_folder_tracker).ok());
  }

  EXPECT_EQ(SYNC_STATUS_OK, InitializeMetadataDatabase());

  {
    base::FilePath path;
    FileTracker tracker;
    EXPECT_FALSE(metadata_database()->FindNearestActiveAncestor(
        "non_registered_app_id",
        CreateNormalizedPath(FPL("folder1/folder2/file")),
        &tracker, &path));
  }

  {
    base::FilePath path;
    FileTracker tracker;
    EXPECT_TRUE(metadata_database()->FindNearestActiveAncestor(
        kAppID, CreateNormalizedPath(FPL("")), &tracker, &path));
    EXPECT_EQ(app_root_tracker.tracker_id(), tracker.tracker_id());
    EXPECT_EQ(CreateNormalizedPath(FPL("")), path);
  }

  {
    base::FilePath path;
    FileTracker tracker;
    EXPECT_TRUE(metadata_database()->FindNearestActiveAncestor(
        kAppID, CreateNormalizedPath(FPL("folder1/folder2")),
        &tracker, &path));
    EXPECT_EQ(folder_tracker2.tracker_id(), tracker.tracker_id());
    EXPECT_EQ(CreateNormalizedPath(FPL("folder1/folder2")), path);
  }

  {
    base::FilePath path;
    FileTracker tracker;
    EXPECT_TRUE(metadata_database()->FindNearestActiveAncestor(
        kAppID, CreateNormalizedPath(FPL("folder1/folder2/file")),
        &tracker, &path));
    EXPECT_EQ(file_tracker.tracker_id(), tracker.tracker_id());
    EXPECT_EQ(CreateNormalizedPath(FPL("folder1/folder2/file")), path);
  }

  {
    base::FilePath path;
    FileTracker tracker;
    EXPECT_TRUE(metadata_database()->FindNearestActiveAncestor(
        kAppID,
        CreateNormalizedPath(FPL("folder1/folder2/folder3/folder4/file")),
        &tracker, &path));
    EXPECT_EQ(folder_tracker2.tracker_id(), tracker.tracker_id());
    EXPECT_EQ(CreateNormalizedPath(FPL("folder1/folder2")), path);
  }

  {
    base::FilePath path;
    FileTracker tracker;
    EXPECT_TRUE(metadata_database()->FindNearestActiveAncestor(
        kAppID, CreateNormalizedPath(FPL("folder1/folder2/file/folder4/file")),
        &tracker, &path));
    EXPECT_EQ(folder_tracker2.tracker_id(), tracker.tracker_id());
    EXPECT_EQ(CreateNormalizedPath(FPL("folder1/folder2")), path);
  }
}

TEST_P(MetadataDatabaseTest, UpdateByChangeListTest) {
  TrackedFile sync_root(CreateTrackedSyncRoot());
  TrackedFile app_root(CreateTrackedFolder(sync_root, "app_id"));
  TrackedFile disabled_app_root(CreateTrackedFolder(sync_root, "disabled_app"));
  TrackedFile file(CreateTrackedFile(app_root, "file"));
  TrackedFile renamed_file(CreateTrackedFile(app_root, "to be renamed"));
  TrackedFile folder(CreateTrackedFolder(app_root, "folder"));
  TrackedFile reorganized_file(
      CreateTrackedFile(app_root, "to be reorganized"));
  TrackedFile updated_file(
      CreateTrackedFile(app_root, "to be updated"));
  TrackedFile noop_file(CreateTrackedFile(app_root, "has noop change"));
  TrackedFile new_file(CreateTrackedFile(app_root, "to be added later"));
  new_file.should_be_absent = true;

  const TrackedFile* tracked_files[] = {
    &sync_root, &app_root, &disabled_app_root,
    &file, &renamed_file, &folder, &reorganized_file, &updated_file, &noop_file,
    &new_file,
  };

  SetUpDatabaseByTrackedFiles(tracked_files, std::size(tracked_files));
  EXPECT_EQ(SYNC_STATUS_OK, InitializeMetadataDatabase());

  ApplyRenameChangeToMetadata("renamed", &renamed_file.metadata);
  ApplyReorganizeChangeToMetadata(folder.metadata.file_id(),
                                  &reorganized_file.metadata);
  ApplyContentChangeToMetadata(&updated_file.metadata);

  // Update change ID.
  ApplyNoopChangeToMetadata(&noop_file.metadata);

  std::vector<std::unique_ptr<google_apis::ChangeResource>> changes;
  PushToChangeList(
      CreateChangeResourceFromMetadata(renamed_file.metadata), &changes);
  PushToChangeList(
      CreateChangeResourceFromMetadata(reorganized_file.metadata), &changes);
  PushToChangeList(
      CreateChangeResourceFromMetadata(updated_file.metadata), &changes);
  PushToChangeList(
      CreateChangeResourceFromMetadata(noop_file.metadata), &changes);
  PushToChangeList(
      CreateChangeResourceFromMetadata(new_file.metadata), &changes);
  EXPECT_EQ(SYNC_STATUS_OK, UpdateByChangeList(std::move(changes)));

  renamed_file.tracker.set_dirty(true);
  reorganized_file.tracker.set_dirty(true);
  updated_file.tracker.set_dirty(true);
  noop_file.tracker.set_dirty(true);
  new_file.tracker.mutable_synced_details()->set_missing(true);
  new_file.tracker.mutable_synced_details()->clear_md5();
  new_file.tracker.set_active(false);
  new_file.tracker.set_dirty(true);
  ResetTrackerID(&new_file.tracker);
  EXPECT_NE(0, new_file.tracker.tracker_id());

  new_file.should_be_absent = false;

  VerifyTrackedFiles(tracked_files, std::size(tracked_files));
  VerifyReloadConsistency();
}

TEST_P(MetadataDatabaseTest, PopulateFolderTest_RegularFolder) {
  TrackedFile sync_root(CreateTrackedSyncRoot());
  TrackedFile app_root(CreateTrackedAppRoot(sync_root, "app_id"));
  app_root.tracker.set_app_id(app_root.metadata.details().title());

  TrackedFile folder_to_populate(
      CreateTrackedFolder(app_root, "folder_to_populate"));
  folder_to_populate.tracker.set_needs_folder_listing(true);
  folder_to_populate.tracker.set_dirty(true);

  TrackedFile known_file(CreateTrackedFile(folder_to_populate, "known_file"));
  TrackedFile new_file(CreateTrackedFile(folder_to_populate, "new_file"));
  new_file.should_be_absent = true;

  const TrackedFile* tracked_files[] = {
    &sync_root, &app_root, &folder_to_populate, &known_file, &new_file
  };

  SetUpDatabaseByTrackedFiles(tracked_files, std::size(tracked_files));
  EXPECT_EQ(SYNC_STATUS_OK, InitializeMetadataDatabase());
  VerifyTrackedFiles(tracked_files, std::size(tracked_files));

  FileIDList listed_children;
  listed_children.push_back(known_file.metadata.file_id());
  listed_children.push_back(new_file.metadata.file_id());

  EXPECT_EQ(SYNC_STATUS_OK,
            PopulateFolder(folder_to_populate.metadata.file_id(),
                           listed_children));

  folder_to_populate.tracker.set_dirty(false);
  folder_to_populate.tracker.set_needs_folder_listing(false);
  ResetTrackerID(&new_file.tracker);
  new_file.tracker.set_dirty(true);
  new_file.tracker.set_active(false);
  new_file.tracker.clear_synced_details();
  new_file.should_be_absent = false;
  new_file.tracker_only = true;
  VerifyTrackedFiles(tracked_files, std::size(tracked_files));
  VerifyReloadConsistency();
}

TEST_P(MetadataDatabaseTest, PopulateFolderTest_InactiveFolder) {
  TrackedFile sync_root(CreateTrackedSyncRoot());
  TrackedFile app_root(CreateTrackedAppRoot(sync_root, "app_id"));

  TrackedFile inactive_folder(CreateTrackedFolder(app_root, "inactive_folder"));
  inactive_folder.tracker.set_active(false);
  inactive_folder.tracker.set_dirty(true);

  TrackedFile new_file(
      CreateTrackedFile(inactive_folder, "file_in_inactive_folder"));
  new_file.should_be_absent = true;

  const TrackedFile* tracked_files[] = {
    &sync_root, &app_root, &inactive_folder, &new_file,
  };

  SetUpDatabaseByTrackedFiles(tracked_files, std::size(tracked_files));
  EXPECT_EQ(SYNC_STATUS_OK, InitializeMetadataDatabase());
  VerifyTrackedFiles(tracked_files, std::size(tracked_files));

  FileIDList listed_children;
  listed_children.push_back(new_file.metadata.file_id());

  EXPECT_EQ(SYNC_STATUS_OK,
            PopulateFolder(inactive_folder.metadata.file_id(),
                           listed_children));
  VerifyTrackedFiles(tracked_files, std::size(tracked_files));
  VerifyReloadConsistency();
}

TEST_P(MetadataDatabaseTest, PopulateFolderTest_DisabledAppRoot) {
  TrackedFile sync_root(CreateTrackedSyncRoot());
  TrackedFile disabled_app_root(
      CreateTrackedAppRoot(sync_root, "disabled_app"));
  disabled_app_root.tracker.set_dirty(true);
  disabled_app_root.tracker.set_needs_folder_listing(true);

  TrackedFile known_file(CreateTrackedFile(disabled_app_root, "known_file"));
  TrackedFile file(CreateTrackedFile(disabled_app_root, "file"));
  file.should_be_absent = true;

  const TrackedFile* tracked_files[] = {
    &sync_root, &disabled_app_root, &disabled_app_root, &known_file, &file,
  };

  SetUpDatabaseByTrackedFiles(tracked_files, std::size(tracked_files));
  EXPECT_EQ(SYNC_STATUS_OK, InitializeMetadataDatabase());
  VerifyTrackedFiles(tracked_files, std::size(tracked_files));

  FileIDList disabled_app_children;
  disabled_app_children.push_back(file.metadata.file_id());
  EXPECT_EQ(SYNC_STATUS_OK, PopulateFolder(
      disabled_app_root.metadata.file_id(), disabled_app_children));
  ResetTrackerID(&file.tracker);
  file.tracker.clear_synced_details();
  file.tracker.set_dirty(true);
  file.tracker.set_active(false);
  file.should_be_absent = false;
  file.tracker_only = true;

  disabled_app_root.tracker.set_dirty(false);
  disabled_app_root.tracker.set_needs_folder_listing(false);
  VerifyTrackedFiles(tracked_files, std::size(tracked_files));
  VerifyReloadConsistency();
}

// TODO(tzik): Fix expectation and re-enable this test.
TEST_P(MetadataDatabaseTest, DISABLED_UpdateTrackerTest) {
  TrackedFile sync_root(CreateTrackedSyncRoot());
  TrackedFile app_root(CreateTrackedAppRoot(sync_root, "app_root"));
  TrackedFile file(CreateTrackedFile(app_root, "file"));
  file.tracker.set_dirty(true);
  file.metadata.mutable_details()->set_title("renamed file");

  TrackedFile inactive_file(CreateTrackedFile(app_root, "inactive_file"));
  inactive_file.tracker.set_active(false);
  inactive_file.tracker.set_dirty(true);
  inactive_file.metadata.mutable_details()->set_title("renamed inactive file");
  inactive_file.metadata.mutable_details()->set_md5("modified_md5");

  TrackedFile new_conflict(CreateTrackedFile(app_root, "new conflict file"));
  new_conflict.tracker.set_dirty(true);
  new_conflict.metadata.mutable_details()->set_title("renamed file");

  const TrackedFile* tracked_files[] = {
    &sync_root, &app_root, &file, &inactive_file, &new_conflict
  };

  SetUpDatabaseByTrackedFiles(tracked_files, std::size(tracked_files));
  EXPECT_EQ(SYNC_STATUS_OK, InitializeMetadataDatabase());
  VerifyTrackedFiles(tracked_files, std::size(tracked_files));
  VerifyReloadConsistency();

  *file.tracker.mutable_synced_details() = file.metadata.details();
  file.tracker.set_dirty(false);
  EXPECT_EQ(SYNC_STATUS_OK, UpdateTracker(file.tracker));
  VerifyTrackedFiles(tracked_files, std::size(tracked_files));
  VerifyReloadConsistency();

  *inactive_file.tracker.mutable_synced_details() =
       inactive_file.metadata.details();
  inactive_file.tracker.set_dirty(false);
  inactive_file.tracker.set_active(true);
  EXPECT_EQ(SYNC_STATUS_OK, UpdateTracker(inactive_file.tracker));
  VerifyTrackedFiles(tracked_files, std::size(tracked_files));
  VerifyReloadConsistency();

  *new_conflict.tracker.mutable_synced_details() =
       new_conflict.metadata.details();
  new_conflict.tracker.set_dirty(false);
  new_conflict.tracker.set_active(true);
  file.tracker.set_dirty(true);
  file.tracker.set_active(false);
  EXPECT_EQ(SYNC_STATUS_OK, UpdateTracker(new_conflict.tracker));
  VerifyTrackedFiles(tracked_files, std::size(tracked_files));
  VerifyReloadConsistency();
}

TEST_P(MetadataDatabaseTest, PopulateInitialDataTest) {
  TrackedFile sync_root(CreateTrackedSyncRoot());
  TrackedFile app_root(CreateTrackedFolder(sync_root, "app_root"));
  app_root.tracker.set_active(false);

  const TrackedFile* tracked_files[] = {
    &sync_root, &app_root
  };

  std::unique_ptr<google_apis::FileResource> sync_root_folder(
      CreateFileResourceFromMetadata(sync_root.metadata));
  std::unique_ptr<google_apis::FileResource> app_root_folder(
      CreateFileResourceFromMetadata(app_root.metadata));

  std::vector<std::unique_ptr<google_apis::FileResource>> app_root_folders;
  app_root_folders.push_back(std::move(app_root_folder));

  EXPECT_EQ(SYNC_STATUS_OK, InitializeMetadataDatabase());
  EXPECT_EQ(SYNC_STATUS_OK, PopulateInitialData(
      current_change_id(),
      *sync_root_folder,
      app_root_folders));

  ResetTrackerID(&sync_root.tracker);
  ResetTrackerID(&app_root.tracker);
  app_root.tracker.set_parent_tracker_id(sync_root.tracker.tracker_id());

  VerifyTrackedFiles(tracked_files, std::size(tracked_files));
  VerifyReloadConsistency();
}

TEST_P(MetadataDatabaseTest, DumpFiles) {
  TrackedFile sync_root(CreateTrackedSyncRoot());
  TrackedFile app_root(CreateTrackedAppRoot(sync_root, "app_id"));
  app_root.tracker.set_app_id(app_root.metadata.details().title());

  TrackedFile folder_0(CreateTrackedFolder(app_root, "folder_0"));
  TrackedFile file_0(CreateTrackedFile(folder_0, "file_0"));

  const TrackedFile* tracked_files[] = {&sync_root, &app_root, &folder_0,
                                        &file_0};

  SetUpDatabaseByTrackedFiles(tracked_files, std::size(tracked_files));
  EXPECT_EQ(SYNC_STATUS_OK, InitializeMetadataDatabase());
  VerifyTrackedFiles(tracked_files, std::size(tracked_files));

  base::Value::List files =
      metadata_database()->DumpFiles(app_root.tracker.app_id());
  ASSERT_EQ(2u, files.size());

  const std::string* str;
  const base::Value& folder_val = files[0];
  ASSERT_TRUE(folder_val.is_dict());
  const base::Value::Dict& folder = folder_val.GetDict();
  str = folder.FindString("title");
  EXPECT_TRUE(str && *str == "folder_0");
  str = folder.FindString("type");
  EXPECT_TRUE(str && *str == "folder");
  EXPECT_TRUE(folder.contains("details"));

  const base::Value& file_val = files[1];
  ASSERT_TRUE(file_val.is_dict());
  const base::Value::Dict& file = file_val.GetDict();
  str = file.FindString("title");
  EXPECT_TRUE(str && *str == "file_0");
  str = file.FindString("type");
  EXPECT_TRUE(str && *str == "file");
  EXPECT_TRUE(file.contains("details"));
}

TEST_P(MetadataDatabaseTest, ClearDatabase) {
  const bool db_on_disk = enable_on_disk_index();
  leveldb::Env* env = db_on_disk ? leveldb::Env::Default() : in_memory_env();
  std::vector<std::string> children;
  EXPECT_TRUE(env->GetChildren(DatabasePath().AsUTF8Unsafe(), &children).ok());
  EXPECT_EQ(children.size(), 0ul);

  SyncStatusCode status = SYNC_STATUS_UNKNOWN;
  std::unique_ptr<MetadataDatabase> metadata_database =
      MetadataDatabase::CreateInternal(DatabasePath(), env,
                                       enable_on_disk_index(), &status);
  ASSERT_EQ(SYNC_STATUS_OK, status);
  EXPECT_TRUE(env->GetChildren(DatabasePath().AsUTF8Unsafe(), &children).ok());
  EXPECT_GT(children.size(), 0ul);

  MetadataDatabase::ClearDatabase(std::move(metadata_database));

  if (db_on_disk) {
    EXPECT_FALSE(base::PathExists(DatabasePath()));
  } else {
    EXPECT_TRUE(base::PathExists(DatabasePath()));
  }
}

}  // namespace drive_backend
}  // namespace sync_file_system
