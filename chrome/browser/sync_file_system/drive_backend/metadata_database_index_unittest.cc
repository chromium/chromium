// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/metadata_database_index.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "chrome/browser/sync_file_system/drive_backend/drive_backend_constants.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_test_util.h"
#include "chrome/browser/sync_file_system/drive_backend/leveldb_wrapper.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/leveldb_chrome.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"

namespace sync_file_system {
namespace drive_backend {

namespace {

const int64_t kSyncRootTrackerID = 1;
const int64_t kAppRootTrackerID = 2;
const int64_t kFileTrackerID = 3;
const int64_t kPlaceholderTrackerID = 4;

std::unique_ptr<DatabaseContents> CreateTestDatabaseContents() {
  std::unique_ptr<DatabaseContents> contents(new DatabaseContents);

  std::unique_ptr<FileMetadata> sync_root_metadata =
      test_util::CreateFolderMetadata("sync_root_folder_id",
                                      "Chrome Syncable FileSystem");
  std::unique_ptr<FileTracker> sync_root_tracker = test_util::CreateTracker(
      *sync_root_metadata, kSyncRootTrackerID, nullptr);

  std::unique_ptr<FileMetadata> app_root_metadata =
      test_util::CreateFolderMetadata("app_root_folder_id", "app_id");
  std::unique_ptr<FileTracker> app_root_tracker = test_util::CreateTracker(
      *app_root_metadata, kAppRootTrackerID, sync_root_tracker.get());
  app_root_tracker->set_app_id("app_id");
  app_root_tracker->set_tracker_kind(TRACKER_KIND_APP_ROOT);

  std::unique_ptr<FileMetadata> file_metadata =
      test_util::CreateFileMetadata("file_id", "file", "file_md5");
  std::unique_ptr<FileTracker> file_tracker = test_util::CreateTracker(
      *file_metadata, kFileTrackerID, app_root_tracker.get());

  std::unique_ptr<FileTracker> placeholder_tracker =
      test_util::CreatePlaceholderTracker(
          "unsynced_file_id", kPlaceholderTrackerID, app_root_tracker.get());

  contents->file_metadata.push_back(std::move(sync_root_metadata));
  contents->file_trackers.push_back(std::move(sync_root_tracker));
  contents->file_metadata.push_back(std::move(app_root_metadata));
  contents->file_trackers.push_back(std::move(app_root_tracker));
  contents->file_metadata.push_back(std::move(file_metadata));
  contents->file_trackers.push_back(std::move(file_tracker));
  contents->file_trackers.push_back(std::move(placeholder_tracker));
  return contents;
}

}  // namespace

class MetadataDatabaseIndexTest : public testing::Test {
 public:
  void SetUp() override {
    in_memory_env_ = leveldb_chrome::NewMemEnv("MetadataDatabaseIndexTest");
    InitializeLevelDB();

    contents_ = CreateTestDatabaseContents();
    index_ = MetadataDatabaseIndex::CreateForTesting(contents_.get(),
                                                     db_.get());
  }

  MetadataDatabaseIndex* index() { return index_.get(); }

 private:
  void InitializeLevelDB() {
    std::unique_ptr<leveldb::DB> db;
    leveldb_env::Options options;
    options.create_if_missing = true;
    options.max_open_files = 0;  // Use minimum.
    options.env = in_memory_env_.get();
    leveldb::Status status = leveldb_env::OpenDB(options, "", &db);
    ASSERT_TRUE(status.ok());

    db_ = std::make_unique<LevelDBWrapper>(std::move(db));
  }

  std::unique_ptr<DatabaseContents> contents_;
  std::unique_ptr<MetadataDatabaseIndex> index_;

  std::unique_ptr<leveldb::Env> in_memory_env_;
  std::unique_ptr<LevelDBWrapper> db_;
};

TEST_F(MetadataDatabaseIndexTest, GetEntryTest) {
  FileTracker tracker;
  EXPECT_FALSE(index()->GetFileTracker(kInvalidTrackerID, nullptr));
  ASSERT_TRUE(index()->GetFileTracker(kFileTrackerID, &tracker));
  EXPECT_EQ(kFileTrackerID, tracker.tracker_id());
  EXPECT_EQ("file_id", tracker.file_id());

  FileMetadata metadata;
  EXPECT_FALSE(index()->GetFileMetadata(std::string(), nullptr));
  ASSERT_TRUE(index()->GetFileMetadata("file_id", &metadata));
  EXPECT_EQ("file_id", metadata.file_id());
}

TEST_F(MetadataDatabaseIndexTest, IndexLookUpTest) {
  TrackerIDSet trackers = index()->GetFileTrackerIDsByFileID("file_id");
  EXPECT_EQ(1u, trackers.size());
  EXPECT_TRUE(trackers.has_active());
  EXPECT_EQ(kFileTrackerID, trackers.active_tracker());

  int64_t app_root_tracker_id = index()->GetAppRootTracker("app_id");
  EXPECT_EQ(kAppRootTrackerID, app_root_tracker_id);

  trackers = index()->GetFileTrackerIDsByParentAndTitle(
      app_root_tracker_id, "file");
  EXPECT_EQ(1u, trackers.size());
  EXPECT_TRUE(trackers.has_active());
  EXPECT_EQ(kFileTrackerID, trackers.active_tracker());

  EXPECT_TRUE(index()->PickMultiTrackerFileID().empty());
  EXPECT_EQ(kInvalidTrackerID,
            index()->PickMultiBackingFilePath().parent_id);
  EXPECT_EQ(kPlaceholderTrackerID, index()->PickDirtyTracker());
}

TEST_F(MetadataDatabaseIndexTest, UpdateTest) {
  EXPECT_FALSE(index()->IsDemotedDirtyTracker(kPlaceholderTrackerID));
  index()->DemoteDirtyTracker(kPlaceholderTrackerID);
  EXPECT_TRUE(index()->IsDemotedDirtyTracker(kPlaceholderTrackerID));
  EXPECT_EQ(kInvalidTrackerID, index()->PickDirtyTracker());
  index()->PromoteDemotedDirtyTrackers();
  EXPECT_EQ(kPlaceholderTrackerID, index()->PickDirtyTracker());

  FileMetadata metadata;
  ASSERT_TRUE(index()->GetFileMetadata("file_id", &metadata));
  FileTracker app_root_tracker;
  ASSERT_TRUE(index()->GetFileTracker(kAppRootTrackerID, &app_root_tracker));

  int64_t new_tracker_id = 100;
  std::unique_ptr<FileTracker> new_tracker =
      test_util::CreateTracker(metadata, new_tracker_id, &app_root_tracker);
  new_tracker->set_active(false);
  index()->StoreFileTracker(std::move(new_tracker));

  EXPECT_EQ("file_id", index()->PickMultiTrackerFileID());
  EXPECT_EQ(ParentIDAndTitle(kAppRootTrackerID, std::string("file")),
            index()->PickMultiBackingFilePath());

  index()->RemoveFileMetadata("file_id");
  index()->RemoveFileTracker(kFileTrackerID);

  EXPECT_FALSE(index()->GetFileMetadata("file_id", nullptr));
  EXPECT_FALSE(index()->GetFileTracker(kFileTrackerID, nullptr));
}

}  // namespace drive_backend
}  // namespace sync_file_system
