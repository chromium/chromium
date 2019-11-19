// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/list_changes_task.h"

#include <stddef.h>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/format_macros.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_constants.h"
#include "chrome/browser/sync_file_system/drive_backend/fake_drive_service_helper.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"
#include "chrome/browser/sync_file_system/drive_backend/register_app_task.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine_context.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine_initializer.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_task_manager.h"
#include "chrome/browser/sync_file_system/sync_file_system_test_util.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/drive/drive_api_parser.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/leveldb_chrome.h"

namespace sync_file_system {
namespace drive_backend {

namespace {

const char kAppID[] = "app_id";
const char kUnregisteredAppID[] = "app_id unregistered";

}  // namespace

class ListChangesTaskTest : public testing::Test {
 public:
  ListChangesTaskTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}
  ~ListChangesTaskTest() override {}

  void SetUp() override {
    ASSERT_TRUE(database_dir_.CreateUniqueTempDir());
    in_memory_env_ = leveldb_chrome::NewMemEnv("ListChangesTaskTest");

    std::unique_ptr<drive::FakeDriveService> fake_drive_service(
        new drive::FakeDriveService);

    std::unique_ptr<drive::DriveUploaderInterface> drive_uploader(
        new drive::DriveUploader(fake_drive_service.get(),
                                 base::ThreadTaskRunnerHandle::Get(),
                                 mojo::NullRemote()));

    fake_drive_service_helper_.reset(
        new FakeDriveServiceHelper(fake_drive_service.get(),
                                   drive_uploader.get(),
                                   kSyncRootFolderTitle));

    sync_task_manager_.reset(new SyncTaskManager(
        base::WeakPtr<SyncTaskManager::Client>(),
        10 /* maximum_background_task */, base::ThreadTaskRunnerHandle::Get()));
    sync_task_manager_->Initialize(SYNC_STATUS_OK);

    context_.reset(new SyncEngineContext(
        std::move(fake_drive_service), std::move(drive_uploader),
        nullptr /* task_logger */, base::ThreadTaskRunnerHandle::Get(),
        base::ThreadTaskRunnerHandle::Get()));

    SetUpRemoteFolders();

    InitializeMetadataDatabase();
    RegisterApp(kAppID);
  }

  void TearDown() override {
    sync_task_manager_.reset();
    context_.reset();
    base::RunLoop().RunUntilIdle();
  }

 protected:
  SyncStatusCode RunTask(std::unique_ptr<SyncTask> sync_task) {
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    sync_task_manager_->ScheduleSyncTask(FROM_HERE, std::move(sync_task),
                                         SyncTaskManager::PRIORITY_MED,
                                         CreateResultReceiver(&status));
    base::RunLoop().RunUntilIdle();
    return status;
  }

  size_t CountDirtyTracker() {
    return context_->GetMetadataDatabase()->CountDirtyTracker();
  }

  FakeDriveServiceHelper* fake_drive_service_helper() {
    return fake_drive_service_helper_.get();
  }

  void SetUpChangesInFolder(const std::string& folder_id) {
    std::string new_file_id;
    ASSERT_EQ(google_apis::HTTP_SUCCESS,
              fake_drive_service_helper()->AddFile(
                  folder_id, "new file", "file contents", &new_file_id));
    std::string same_name_file_id;
    ASSERT_EQ(google_apis::HTTP_SUCCESS,
              fake_drive_service_helper()->AddFile(
                  folder_id, "new file", "file contents",
                  &same_name_file_id));

    std::string new_folder_id;
    ASSERT_EQ(google_apis::HTTP_CREATED,
              fake_drive_service_helper()->AddFolder(
                  folder_id, "new folder", &new_folder_id));

    std::string modified_file_id;
    ASSERT_EQ(google_apis::HTTP_SUCCESS,
              fake_drive_service_helper()->AddFile(
                  folder_id, "modified file", "file content",
                  &modified_file_id));
    ASSERT_EQ(google_apis::HTTP_SUCCESS,
              fake_drive_service_helper()->UpdateFile(
                  modified_file_id, "modified file content"));


    std::string deleted_file_id;
    ASSERT_EQ(google_apis::HTTP_SUCCESS,
              fake_drive_service_helper()->AddFile(
                  folder_id, "trashed file", "file content",
                  &deleted_file_id));
    ASSERT_EQ(google_apis::HTTP_NO_CONTENT,
              fake_drive_service_helper()->DeleteResource(deleted_file_id));
  }

  std::string root_resource_id() {
    return context_->GetDriveService()->GetRootResourceId();
  }

  std::string app_root_folder_id() {
    return app_root_folder_id_;
  }

  std::string unregistered_app_root_folder_id() {
    return unregistered_app_root_folder_id_;
  }

  SyncEngineContext* GetSyncEngineContext() {
    return context_.get();
  }

 private:
  void SetUpRemoteFolders() {
    ASSERT_EQ(google_apis::HTTP_CREATED,
              fake_drive_service_helper_->AddOrphanedFolder(
                  kSyncRootFolderTitle, &sync_root_folder_id_));
    ASSERT_EQ(google_apis::HTTP_CREATED,
              fake_drive_service_helper_->AddFolder(
                  sync_root_folder_id_, kAppID, &app_root_folder_id_));
    ASSERT_EQ(google_apis::HTTP_CREATED,
              fake_drive_service_helper_->AddFolder(
                  sync_root_folder_id_, kUnregisteredAppID,
                  &unregistered_app_root_folder_id_));
  }

  void InitializeMetadataDatabase() {
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    SyncEngineInitializer* initializer = new SyncEngineInitializer(
        context_.get(), database_dir_.GetPath(), in_memory_env_.get());

    sync_task_manager_->ScheduleSyncTask(
        FROM_HERE, std::unique_ptr<SyncTask>(initializer),
        SyncTaskManager::PRIORITY_MED,
        base::Bind(&ListChangesTaskTest::DidInitializeMetadataDatabase,
                   base::Unretained(this), initializer, &status));

    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(SYNC_STATUS_OK, status);
  }

  void DidInitializeMetadataDatabase(SyncEngineInitializer* initializer,
                                     SyncStatusCode* status_out,
                                     SyncStatusCode status) {
    context_->SetMetadataDatabase(initializer->PassMetadataDatabase());
    *status_out = status;
  }

  void RegisterApp(const std::string& app_id) {
    EXPECT_EQ(SYNC_STATUS_OK,
              RunTask(std::unique_ptr<SyncTask>(
                  new RegisterAppTask(context_.get(), app_id))));
  }

  std::unique_ptr<leveldb::Env> in_memory_env_;

  std::string sync_root_folder_id_;
  std::string app_root_folder_id_;
  std::string unregistered_app_root_folder_id_;

  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir database_dir_;

  std::unique_ptr<SyncEngineContext> context_;
  std::unique_ptr<FakeDriveServiceHelper> fake_drive_service_helper_;

  std::unique_ptr<SyncTaskManager> sync_task_manager_;

  DISALLOW_COPY_AND_ASSIGN(ListChangesTaskTest);
};

TEST_F(ListChangesTaskTest, NoChange) {
  size_t num_dirty_trackers = CountDirtyTracker();

  EXPECT_EQ(SYNC_STATUS_NO_CHANGE_TO_SYNC,
            RunTask(std::unique_ptr<SyncTask>(
                new ListChangesTask(GetSyncEngineContext()))));

  EXPECT_EQ(num_dirty_trackers, CountDirtyTracker());
}

TEST_F(ListChangesTaskTest, UnrelatedChange) {
  size_t num_dirty_trackers = CountDirtyTracker();

  SetUpChangesInFolder(root_resource_id());
  SetUpChangesInFolder(unregistered_app_root_folder_id());

  EXPECT_EQ(SYNC_STATUS_OK, RunTask(std::unique_ptr<SyncTask>(
                                new ListChangesTask(GetSyncEngineContext()))));

  EXPECT_EQ(num_dirty_trackers, CountDirtyTracker());
}

TEST_F(ListChangesTaskTest, UnderTrackedFolder) {
  size_t num_dirty_trackers = CountDirtyTracker();

  SetUpChangesInFolder(app_root_folder_id());

  EXPECT_EQ(SYNC_STATUS_OK, RunTask(std::unique_ptr<SyncTask>(
                                new ListChangesTask(GetSyncEngineContext()))));

  EXPECT_EQ(num_dirty_trackers + 4, CountDirtyTracker());
}

TEST_F(ListChangesTaskTest, TeamDriveChangeInChangeList) {
  size_t num_dirty_trackers = CountDirtyTracker();

  SetUpChangesInFolder(app_root_folder_id());

  // Adding a team drive will return a TeamDriveResource entry when the
  // change list is retrieved.
  fake_drive_service_helper()->AddTeamDrive("team_drive_id", "team_drive_name");

  EXPECT_EQ(SYNC_STATUS_OK, RunTask(std::unique_ptr<SyncTask>(
                                new ListChangesTask(GetSyncEngineContext()))));

  EXPECT_EQ(num_dirty_trackers + 4, CountDirtyTracker());
}

}  // namespace drive_backend
}  // namespace sync_file_system
