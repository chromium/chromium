// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/register_app_task.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/format_macros.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_constants.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_util.h"
#include "chrome/browser/sync_file_system/drive_backend/fake_drive_service_helper.h"
#include "chrome/browser/sync_file_system/drive_backend/leveldb_wrapper.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.pb.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine_context.h"
#include "chrome/browser/sync_file_system/sync_file_system_test_util.h"
#include "components/drive/drive_uploader.h"
#include "components/drive/service/fake_drive_service.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/drive/drive_api_parser.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/leveldb_chrome.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"

namespace sync_file_system {
namespace drive_backend {

namespace {
const int64_t kSyncRootTrackerID = 100;
}  // namespace

class RegisterAppTaskTest : public testing::Test {
 public:
  RegisterAppTaskTest()
      : next_file_id_(1000),
        next_tracker_id_(10000) {}

  RegisterAppTaskTest(const RegisterAppTaskTest&) = delete;
  RegisterAppTaskTest& operator=(const RegisterAppTaskTest&) = delete;

  ~RegisterAppTaskTest() override {}

  void SetUp() override {
    ASSERT_TRUE(database_dir_.CreateUniqueTempDir());
    in_memory_env_ = leveldb_chrome::NewMemEnv("RegisterAppTaskTest");

    std::unique_ptr<drive::FakeDriveService> fake_drive_service(
        new drive::FakeDriveService);
    std::unique_ptr<drive::DriveUploaderInterface> drive_uploader(
        new drive::DriveUploader(
            fake_drive_service.get(),
            base::SingleThreadTaskRunner::GetCurrentDefault(),
            mojo::NullRemote()));

    fake_drive_service_helper_ = std::make_unique<FakeDriveServiceHelper>(
        fake_drive_service.get(), drive_uploader.get(), kSyncRootFolderTitle);

    context_ = std::make_unique<SyncEngineContext>(
        std::move(fake_drive_service), std::move(drive_uploader),
        nullptr /* task_logger */,
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        base::SingleThreadTaskRunner::GetCurrentDefault());

    ASSERT_EQ(google_apis::HTTP_CREATED,
              fake_drive_service_helper_->AddOrphanedFolder(
                  kSyncRootFolderTitle, &sync_root_folder_id_));
  }

  void TearDown() override {
    context_.reset();
    base::RunLoop().RunUntilIdle();
  }

 protected:
  std::unique_ptr<LevelDBWrapper> OpenLevelDB() {
    std::unique_ptr<leveldb::DB> db;
    leveldb_env::Options options;
    options.create_if_missing = true;
    options.env = in_memory_env_.get();
    leveldb::Status status = leveldb_env::OpenDB(
        options, database_dir_.GetPath().AsUTF8Unsafe(), &db);
    EXPECT_TRUE(status.ok());
    return std::make_unique<LevelDBWrapper>(std::move(db));
  }

  void SetUpInitialData(LevelDBWrapper* db) {
    ServiceMetadata service_metadata;
    service_metadata.set_largest_change_id(100);
    service_metadata.set_sync_root_tracker_id(kSyncRootTrackerID);
    service_metadata.set_next_tracker_id(next_tracker_id_);

    FileDetails sync_root_details;
    sync_root_details.set_title(kSyncRootFolderTitle);
    sync_root_details.set_file_kind(FILE_KIND_FOLDER);
    sync_root_details.set_change_id(1);

    FileMetadata sync_root_metadata;
    sync_root_metadata.set_file_id(sync_root_folder_id_);
    *sync_root_metadata.mutable_details() = sync_root_details;

    FileTracker sync_root_tracker;
    sync_root_tracker.set_tracker_id(service_metadata.sync_root_tracker_id());
    sync_root_tracker.set_parent_tracker_id(0);
    sync_root_tracker.set_file_id(sync_root_metadata.file_id());
    sync_root_tracker.set_tracker_kind(TRACKER_KIND_REGULAR);
    *sync_root_tracker.mutable_synced_details() = sync_root_details;
    sync_root_tracker.set_active(true);

    db->Put(kDatabaseVersionKey, base::NumberToString(kCurrentDatabaseVersion));
    PutServiceMetadataToDB(service_metadata, db);
    PutFileMetadataToDB(sync_root_metadata, db);
    PutFileTrackerToDB(sync_root_tracker, db);
    EXPECT_TRUE(db->Commit().ok());
  }

  void CreateMetadataDatabase(std::unique_ptr<LevelDBWrapper> db) {
    ASSERT_TRUE(db);
    ASSERT_FALSE(context_->GetMetadataDatabase());
    std::unique_ptr<MetadataDatabase> metadata_db;
    ASSERT_EQ(
        SYNC_STATUS_OK,
        MetadataDatabase::CreateForTesting(
            std::move(db), true /* enable_on_disk_index */, &metadata_db));
    context_->SetMetadataDatabase(std::move(metadata_db));
  }

  SyncStatusCode RunRegisterAppTask(const std::string& app_id) {
    RegisterAppTask task(context_.get(), app_id);
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    task.RunExclusive(CreateResultReceiver(&status));
    base::RunLoop().RunUntilIdle();
    return status;
  }

  void SetUpRegisteredAppRoot(
      const std::string& app_id,
      LevelDBWrapper* db) {
    FileDetails details;
    details.set_title(app_id);
    details.set_file_kind(FILE_KIND_FOLDER);
    details.add_parent_folder_ids(sync_root_folder_id_);

    FileMetadata metadata;
    metadata.set_file_id(GenerateFileID());
    *metadata.mutable_details() = details;

    FileTracker tracker;
    tracker.set_parent_tracker_id(kSyncRootTrackerID);
    tracker.set_tracker_id(next_tracker_id_++);
    tracker.set_file_id(metadata.file_id());
    tracker.set_tracker_kind(TRACKER_KIND_APP_ROOT);
    tracker.set_app_id(app_id);
    *tracker.mutable_synced_details() = details;
    tracker.set_active(true);

    PutFileMetadataToDB(metadata, db);
    PutFileTrackerToDB(tracker, db);
    EXPECT_TRUE(db->Commit().ok());
  }

  void SetUpUnregisteredAppRoot(const std::string& app_id,
                                LevelDBWrapper* db) {
    FileDetails details;
    details.set_title(app_id);
    details.set_file_kind(FILE_KIND_FOLDER);
    details.add_parent_folder_ids(sync_root_folder_id_);

    FileMetadata metadata;
    metadata.set_file_id(GenerateFileID());
    *metadata.mutable_details() = details;

    FileTracker tracker;
    tracker.set_parent_tracker_id(kSyncRootTrackerID);
    tracker.set_tracker_id(next_tracker_id_++);
    tracker.set_file_id(metadata.file_id());
    tracker.set_tracker_kind(TRACKER_KIND_REGULAR);
    *tracker.mutable_synced_details() = details;
    tracker.set_active(false);

    PutFileMetadataToDB(metadata, db);
    PutFileTrackerToDB(tracker, db);
    EXPECT_TRUE(db->Commit().ok());
  }

  size_t CountRegisteredAppRoot() {
    std::vector<std::string> app_ids;
    context_->GetMetadataDatabase()->GetRegisteredAppIDs(&app_ids);
    return app_ids.size();
  }

  bool IsAppRegistered(const std::string& app_id) {
    TrackerIDSet trackers;
    if (!context_->GetMetadataDatabase()->FindTrackersByParentAndTitle(
            kSyncRootTrackerID, app_id, &trackers))
      return false;
    return trackers.has_active();
  }

  size_t CountRemoteFileInSyncRoot() {
    std::vector<std::unique_ptr<google_apis::FileResource>> files;
    EXPECT_EQ(google_apis::HTTP_SUCCESS,
              fake_drive_service_helper_->ListFilesInFolder(
                  sync_root_folder_id_, &files));
    return files.size();
  }

  bool GetAppRootFolderID(const std::string& app_id,
                          std::string* app_root_folder_id) {
    TrackerIDSet files;
    if (!context_->GetMetadataDatabase()->FindTrackersByParentAndTitle(
            kSyncRootTrackerID, app_id, &files) ||
        !files.has_active())
      return false;

    FileTracker app_root_tracker;
    EXPECT_TRUE(context_->GetMetadataDatabase()->FindTrackerByTrackerID(
        files.active_tracker(), &app_root_tracker));
    *app_root_folder_id = app_root_tracker.file_id();
    return true;
  }

  bool HasRemoteAppRoot(const std::string& app_id) {
    std::string app_root_folder_id;
    if (!GetAppRootFolderID(app_id, &app_root_folder_id))
      return false;

    std::unique_ptr<google_apis::FileResource> entry;
    if (google_apis::HTTP_SUCCESS !=
        fake_drive_service_helper_->GetFileResource(app_root_folder_id, &entry))
      return false;

    return !entry->labels().is_trashed();
  }

  bool VerifyRemoteAppRootVisibility(const std::string& app_id) {
    std::string app_root_folder_id;
    if (!GetAppRootFolderID(app_id, &app_root_folder_id))
      return false;

    google_apis::drive::FileVisibility visibility;
    if (google_apis::HTTP_SUCCESS !=
        fake_drive_service_helper_->GetFileVisibility(
            app_root_folder_id, &visibility))
      return false;
    if (visibility != google_apis::drive::FILE_VISIBILITY_PRIVATE)
      return false;

    return true;
  }

 private:
  std::string GenerateFileID() {
    return base::StringPrintf("file_id_%" PRId64, next_file_id_++);
  }

  std::unique_ptr<leveldb::Env> in_memory_env_;

  std::string sync_root_folder_id_;

  int64_t next_file_id_;
  int64_t next_tracker_id_;

  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir database_dir_;

  std::unique_ptr<SyncEngineContext> context_;
  std::unique_ptr<FakeDriveServiceHelper> fake_drive_service_helper_;
};

TEST_F(RegisterAppTaskTest, AlreadyRegistered) {
  std::unique_ptr<LevelDBWrapper> db = OpenLevelDB();
  ASSERT_TRUE(db);
  SetUpInitialData(db.get());

  const std::string kAppID = "app_id";
  SetUpRegisteredAppRoot(kAppID, db.get());

  CreateMetadataDatabase(std::move(db));
  EXPECT_EQ(SYNC_STATUS_OK, RunRegisterAppTask(kAppID));

  EXPECT_EQ(1u, CountRegisteredAppRoot());
  EXPECT_TRUE(IsAppRegistered(kAppID));
}

TEST_F(RegisterAppTaskTest, CreateAppFolder) {
  std::unique_ptr<LevelDBWrapper> db = OpenLevelDB();
  ASSERT_TRUE(db);
  SetUpInitialData(db.get());

  const std::string kAppID = "app_id";
  CreateMetadataDatabase(std::move(db));
  RunRegisterAppTask(kAppID);

  EXPECT_EQ(1u, CountRegisteredAppRoot());
  EXPECT_TRUE(IsAppRegistered(kAppID));

  EXPECT_EQ(1u, CountRemoteFileInSyncRoot());
  EXPECT_TRUE(HasRemoteAppRoot(kAppID));
  EXPECT_TRUE(VerifyRemoteAppRootVisibility(kAppID));
}

TEST_F(RegisterAppTaskTest, RegisterExistingFolder) {
  std::unique_ptr<LevelDBWrapper> db = OpenLevelDB();
  ASSERT_TRUE(db);
  SetUpInitialData(db.get());

  const std::string kAppID = "app_id";
  SetUpUnregisteredAppRoot(kAppID, db.get());

  CreateMetadataDatabase(std::move(db));
  RunRegisterAppTask(kAppID);

  EXPECT_EQ(1u, CountRegisteredAppRoot());
  EXPECT_TRUE(IsAppRegistered(kAppID));
}

TEST_F(RegisterAppTaskTest, RegisterExistingFolder_MultipleCandidate) {
  std::unique_ptr<LevelDBWrapper> db = OpenLevelDB();
  ASSERT_TRUE(db);
  SetUpInitialData(db.get());

  const std::string kAppID = "app_id";
  SetUpUnregisteredAppRoot(kAppID, db.get());
  SetUpUnregisteredAppRoot(kAppID, db.get());

  CreateMetadataDatabase(std::move(db));
  RunRegisterAppTask(kAppID);

  EXPECT_EQ(1u, CountRegisteredAppRoot());
  EXPECT_TRUE(IsAppRegistered(kAppID));
}

}  // namespace drive_backend
}  // namespace sync_file_system
