// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/sync_engine_initializer.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_constants.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_test_util.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.pb.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine_context.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_task_manager.h"
#include "chrome/browser/sync_file_system/sync_file_system_test_util.h"
#include "components/drive/drive_api_util.h"
#include "components/drive/drive_uploader.h"
#include "components/drive/service/fake_drive_service.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/drive/drive_api_parser.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/leveldb_chrome.h"

namespace sync_file_system {
namespace drive_backend {

namespace {

const int64_t kInitialLargestChangeID = 1234;

}  // namespace

class SyncEngineInitializerTest : public testing::Test {
 public:
  struct TrackedFile {
    std::unique_ptr<google_apis::FileResource> resource;
    FileMetadata metadata;
    FileTracker tracker;
  };

  SyncEngineInitializerTest() {}

  SyncEngineInitializerTest(const SyncEngineInitializerTest&) = delete;
  SyncEngineInitializerTest& operator=(const SyncEngineInitializerTest&) =
      delete;

  ~SyncEngineInitializerTest() override {}

  void SetUp() override {
    ASSERT_TRUE(database_dir_.CreateUniqueTempDir());
    in_memory_env_ = leveldb_chrome::NewMemEnv("SyncEngineInitializerTest");

    std::unique_ptr<drive::FakeDriveService> fake_drive_service(
        new drive::FakeDriveService);
    fake_drive_service_ = fake_drive_service.get();

    sync_context_ = std::make_unique<SyncEngineContext>(
        std::move(fake_drive_service),
        std::unique_ptr<drive::DriveUploaderInterface>(),
        nullptr /* task_logger */,
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        base::SingleThreadTaskRunner::GetCurrentDefault());

    sync_task_manager_ = std::make_unique<SyncTaskManager>(
        base::WeakPtr<SyncTaskManager::Client>(), 1 /* maximum_parallel_task */,
        base::SingleThreadTaskRunner::GetCurrentDefault());
    sync_task_manager_->Initialize(SYNC_STATUS_OK);
  }

  void TearDown() override {
    sync_task_manager_.reset();
    metadata_database_.reset();
    sync_context_.reset();
    base::RunLoop().RunUntilIdle();
  }

  base::FilePath database_path() { return database_dir_.GetPath(); }

  SyncStatusCode RunInitializer() {
    SyncEngineInitializer* initializer = new SyncEngineInitializer(
        sync_context_.get(), database_path(), in_memory_env_.get());
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;

    sync_task_manager_->ScheduleSyncTask(
        FROM_HERE, std::unique_ptr<SyncTask>(initializer),
        SyncTaskManager::PRIORITY_MED,
        base::BindOnce(&SyncEngineInitializerTest::DidRunInitializer,
                       base::Unretained(this), initializer, &status));

    base::RunLoop().RunUntilIdle();
    return status;
  }

  void DidRunInitializer(SyncEngineInitializer* initializer,
                         SyncStatusCode* status_out,
                         SyncStatusCode status) {
    *status_out = status;
    metadata_database_ = initializer->PassMetadataDatabase();
  }

  SyncStatusCode PopulateDatabase(
      const google_apis::FileResource& sync_root,
      const std::vector<std::unique_ptr<google_apis::FileResource>>&
          app_root_list) {
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    std::unique_ptr<MetadataDatabase> database = MetadataDatabase::Create(
        database_path(), in_memory_env_.get(), &status);
    if (status != SYNC_STATUS_OK)
      return status;

    status = database->PopulateInitialData(kInitialLargestChangeID, sync_root,
                                           app_root_list);
    return status;
  }

  std::unique_ptr<google_apis::FileResource> CreateRemoteFolder(
      const std::string& parent_folder_id,
      const std::string& title) {
    google_apis::ApiErrorCode error = google_apis::OTHER_ERROR;
    std::unique_ptr<google_apis::FileResource> entry;
    drive::AddNewDirectoryOptions options;
    options.visibility = google_apis::drive::FILE_VISIBILITY_PRIVATE;
    sync_context_->GetDriveService()->AddNewDirectory(
        parent_folder_id, title, options, CreateResultReceiver(&error, &entry));
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(google_apis::HTTP_CREATED, error);
    return entry;
  }

  std::unique_ptr<google_apis::FileResource> CreateRemoteSyncRoot() {
    std::unique_ptr<google_apis::FileResource> sync_root(
        CreateRemoteFolder(std::string(), kSyncRootFolderTitle));

    for (size_t i = 0; i < sync_root->parents().size(); ++i) {
      google_apis::ApiErrorCode error = google_apis::OTHER_ERROR;
      sync_context_->GetDriveService()->RemoveResourceFromDirectory(
          sync_root->parents()[i].file_id(), sync_root->file_id(),
          CreateResultReceiver(&error));
      base::RunLoop().RunUntilIdle();
      EXPECT_EQ(google_apis::HTTP_NO_CONTENT, error);
    }

    return sync_root;
  }

  std::string GetSyncRootFolderID() {
    int64_t sync_root_tracker_id = metadata_database_->GetSyncRootTrackerID();
    FileTracker sync_root_tracker;
    EXPECT_TRUE(metadata_database_->FindTrackerByTrackerID(sync_root_tracker_id,
                                                           &sync_root_tracker));
    return sync_root_tracker.file_id();
  }

  bool VerifyFolderVisibility(const std::string& folder_id) {
    google_apis::drive::FileVisibility visibility;
    if (google_apis::HTTP_SUCCESS !=
        fake_drive_service_->GetFileVisibility(folder_id, &visibility))
      return false;
    if (visibility != google_apis::drive::FILE_VISIBILITY_PRIVATE)
      return false;
    return true;
  }

  size_t CountTrackersForFile(const std::string& file_id) {
    TrackerIDSet trackers;
    metadata_database_->FindTrackersByFileID(file_id, &trackers);
    return trackers.size();
  }

  bool HasActiveTracker(const std::string& file_id) {
    TrackerIDSet trackers;
    return metadata_database_->FindTrackersByFileID(file_id, &trackers) &&
           trackers.has_active();
  }

  bool HasNoParent(const std::string& file_id) {
    google_apis::ApiErrorCode error = google_apis::OTHER_ERROR;
    std::unique_ptr<google_apis::FileResource> entry;
    sync_context_->GetDriveService()->GetFileResource(
        file_id, CreateResultReceiver(&error, &entry));
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(google_apis::HTTP_SUCCESS, error);
    return entry->parents().empty();
  }

  size_t CountFileMetadata() { return metadata_database_->CountFileMetadata(); }

  size_t CountFileTracker() { return metadata_database_->CountFileTracker(); }

  google_apis::ApiErrorCode AddParentFolder(
      const std::string& new_parent_folder_id,
      const std::string& file_id) {
    google_apis::ApiErrorCode error = google_apis::OTHER_ERROR;
    sync_context_->GetDriveService()->AddResourceToDirectory(
        new_parent_folder_id, file_id, CreateResultReceiver(&error));
    base::RunLoop().RunUntilIdle();
    return error;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir database_dir_;
  std::unique_ptr<leveldb::Env> in_memory_env_;

  std::unique_ptr<MetadataDatabase> metadata_database_;
  std::unique_ptr<SyncTaskManager> sync_task_manager_;
  std::unique_ptr<SyncEngineContext> sync_context_;
  raw_ptr<drive::FakeDriveService, DanglingUntriaged> fake_drive_service_ =
      nullptr;
};

TEST_F(SyncEngineInitializerTest, EmptyDatabase_NoRemoteSyncRoot) {
  EXPECT_EQ(SYNC_STATUS_OK, RunInitializer());

  std::string sync_root_folder_id = GetSyncRootFolderID();
  EXPECT_EQ(1u, CountTrackersForFile(sync_root_folder_id));

  EXPECT_TRUE(HasActiveTracker(sync_root_folder_id));

  EXPECT_EQ(1u, CountFileMetadata());
  EXPECT_EQ(1u, CountFileTracker());
  EXPECT_TRUE(VerifyFolderVisibility(sync_root_folder_id));
}

TEST_F(SyncEngineInitializerTest, EmptyDatabase_RemoteSyncRootExists) {
  std::unique_ptr<google_apis::FileResource> sync_root(CreateRemoteSyncRoot());
  std::unique_ptr<google_apis::FileResource> app_root_1(
      CreateRemoteFolder(sync_root->file_id(), "app-root 1"));
  std::unique_ptr<google_apis::FileResource> app_root_2(
      CreateRemoteFolder(sync_root->file_id(), "app-root 2"));

  EXPECT_EQ(SYNC_STATUS_OK, RunInitializer());

  EXPECT_EQ(1u, CountTrackersForFile(sync_root->file_id()));
  EXPECT_EQ(1u, CountTrackersForFile(app_root_1->file_id()));
  EXPECT_EQ(1u, CountTrackersForFile(app_root_2->file_id()));

  EXPECT_TRUE(HasActiveTracker(sync_root->file_id()));
  EXPECT_FALSE(HasActiveTracker(app_root_1->file_id()));
  EXPECT_FALSE(HasActiveTracker(app_root_2->file_id()));

  EXPECT_EQ(3u, CountFileMetadata());
  EXPECT_EQ(3u, CountFileTracker());
}

TEST_F(SyncEngineInitializerTest, DatabaseAlreadyInitialized) {
  std::unique_ptr<google_apis::FileResource> sync_root(CreateRemoteSyncRoot());
  std::vector<std::unique_ptr<google_apis::FileResource>> app_root_list;
  app_root_list.push_back(
      CreateRemoteFolder(sync_root->file_id(), "app-root 1"));
  app_root_list.push_back(
      CreateRemoteFolder(sync_root->file_id(), "app-root 2"));

  EXPECT_EQ(SYNC_STATUS_OK, PopulateDatabase(*sync_root, app_root_list));

  EXPECT_EQ(SYNC_STATUS_OK, RunInitializer());

  EXPECT_EQ(1u, CountTrackersForFile(sync_root->file_id()));
  EXPECT_EQ(1u, CountTrackersForFile(app_root_list[0]->file_id()));
  EXPECT_EQ(1u, CountTrackersForFile(app_root_list[1]->file_id()));

  EXPECT_TRUE(HasActiveTracker(sync_root->file_id()));
  EXPECT_FALSE(HasActiveTracker(app_root_list[0]->file_id()));
  EXPECT_FALSE(HasActiveTracker(app_root_list[1]->file_id()));

  EXPECT_EQ(3u, CountFileMetadata());
  EXPECT_EQ(3u, CountFileTracker());
}

TEST_F(SyncEngineInitializerTest, EmptyDatabase_MultiCandidate) {
  std::unique_ptr<google_apis::FileResource> sync_root_1(
      CreateRemoteSyncRoot());
  std::unique_ptr<google_apis::FileResource> sync_root_2(
      CreateRemoteSyncRoot());

  EXPECT_EQ(SYNC_STATUS_OK, RunInitializer());

  EXPECT_EQ(1u, CountTrackersForFile(sync_root_1->file_id()));
  EXPECT_EQ(0u, CountTrackersForFile(sync_root_2->file_id()));

  EXPECT_TRUE(HasActiveTracker(sync_root_1->file_id()));
  EXPECT_FALSE(HasActiveTracker(sync_root_2->file_id()));

  EXPECT_EQ(1u, CountFileMetadata());
  EXPECT_EQ(1u, CountFileTracker());
}

TEST_F(SyncEngineInitializerTest, EmptyDatabase_UndetachedRemoteSyncRoot) {
  std::unique_ptr<google_apis::FileResource> sync_root(
      CreateRemoteFolder(std::string(), kSyncRootFolderTitle));
  EXPECT_EQ(SYNC_STATUS_OK, RunInitializer());

  EXPECT_EQ(1u, CountTrackersForFile(sync_root->file_id()));
  EXPECT_TRUE(HasActiveTracker(sync_root->file_id()));

  EXPECT_TRUE(HasNoParent(sync_root->file_id()));

  EXPECT_EQ(1u, CountFileMetadata());
  EXPECT_EQ(1u, CountFileTracker());
}

TEST_F(SyncEngineInitializerTest, EmptyDatabase_MultiparentSyncRoot) {
  std::unique_ptr<google_apis::FileResource> folder(
      CreateRemoteFolder(std::string(), "folder"));
  std::unique_ptr<google_apis::FileResource> sync_root(
      CreateRemoteFolder(std::string(), kSyncRootFolderTitle));
  AddParentFolder(sync_root->file_id(), folder->file_id());

  EXPECT_EQ(SYNC_STATUS_OK, RunInitializer());

  EXPECT_EQ(1u, CountTrackersForFile(sync_root->file_id()));
  EXPECT_TRUE(HasActiveTracker(sync_root->file_id()));

  EXPECT_TRUE(HasNoParent(sync_root->file_id()));

  EXPECT_EQ(1u, CountFileMetadata());
  EXPECT_EQ(1u, CountFileTracker());
}

TEST_F(SyncEngineInitializerTest, EmptyDatabase_FakeRemoteSyncRoot) {
  std::unique_ptr<google_apis::FileResource> folder(
      CreateRemoteFolder(std::string(), "folder"));
  std::unique_ptr<google_apis::FileResource> sync_root(
      CreateRemoteFolder(folder->file_id(), kSyncRootFolderTitle));

  EXPECT_EQ(SYNC_STATUS_OK, RunInitializer());

  EXPECT_EQ(0u, CountTrackersForFile(sync_root->file_id()));
  EXPECT_FALSE(HasNoParent(sync_root->file_id()));

  EXPECT_EQ(1u, CountFileMetadata());
  EXPECT_EQ(1u, CountFileTracker());
}

}  // namespace drive_backend
}  // namespace sync_file_system
