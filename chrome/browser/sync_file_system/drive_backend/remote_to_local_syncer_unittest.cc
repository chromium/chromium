// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/remote_to_local_syncer.h"

#include <map>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_constants.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_test_util.h"
#include "chrome/browser/sync_file_system/drive_backend/fake_drive_service_helper.h"
#include "chrome/browser/sync_file_system/drive_backend/list_changes_task.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine_context.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine_initializer.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_task_manager.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_task_token.h"
#include "chrome/browser/sync_file_system/fake_remote_change_processor.h"
#include "chrome/browser/sync_file_system/sync_file_system_test_util.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "components/drive/drive_uploader.h"
#include "components/drive/service/fake_drive_service.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/drive/drive_api_error_codes.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/leveldb_chrome.h"

namespace sync_file_system {
namespace drive_backend {

namespace {

storage::FileSystemURL URL(const GURL& origin, const std::string& path) {
  return CreateSyncableFileSystemURL(
      origin, base::FilePath::FromUTF8Unsafe(path));
}

}  // namespace

class RemoteToLocalSyncerTest : public testing::Test {
 public:
  typedef FakeRemoteChangeProcessor::URLToFileChangesMap URLToFileChangesMap;

  RemoteToLocalSyncerTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}
  ~RemoteToLocalSyncerTest() override {}

  void SetUp() override {
    ASSERT_TRUE(database_dir_.CreateUniqueTempDir());
    in_memory_env_ = leveldb_chrome::NewMemEnv("RemoteToLocalSyncerTest");

    std::unique_ptr<drive::FakeDriveService> fake_drive_service(
        new drive::FakeDriveService);

    std::unique_ptr<drive::DriveUploaderInterface> drive_uploader(
        new drive::DriveUploader(fake_drive_service.get(),
                                 base::ThreadTaskRunnerHandle::Get().get(),
                                 mojo::NullRemote()));
    fake_drive_helper_.reset(
        new FakeDriveServiceHelper(fake_drive_service.get(),
                                   drive_uploader.get(),
                                   kSyncRootFolderTitle));
    remote_change_processor_.reset(new FakeRemoteChangeProcessor);

    context_.reset(new SyncEngineContext(
        std::move(fake_drive_service), std::move(drive_uploader),
        nullptr /* task_logger */, base::ThreadTaskRunnerHandle::Get(),
        base::ThreadTaskRunnerHandle::Get()));
    context_->SetRemoteChangeProcessor(remote_change_processor_.get());

    RegisterSyncableFileSystem();

    sync_task_manager_.reset(new SyncTaskManager(
        base::WeakPtr<SyncTaskManager::Client>(), 10 /* max_parallel_task */,
        base::ThreadTaskRunnerHandle::Get()));
    sync_task_manager_->Initialize(SYNC_STATUS_OK);
  }

  void TearDown() override {
    sync_task_manager_.reset();
    RevokeSyncableFileSystem();
    fake_drive_helper_.reset();
    context_.reset();
    base::RunLoop().RunUntilIdle();
  }

  void InitializeMetadataDatabase() {
    SyncEngineInitializer* initializer = new SyncEngineInitializer(
        context_.get(), database_dir_.GetPath(), in_memory_env_.get());
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    sync_task_manager_->ScheduleSyncTask(
        FROM_HERE, std::unique_ptr<SyncTask>(initializer),
        SyncTaskManager::PRIORITY_MED,
        base::Bind(&RemoteToLocalSyncerTest::DidInitializeMetadataDatabase,
                   base::Unretained(this), initializer, &status));

    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(SYNC_STATUS_OK, status);
  }

  void DidInitializeMetadataDatabase(SyncEngineInitializer* initializer,
                                     SyncStatusCode* status_out,
                                     SyncStatusCode status) {
    *status_out = status;
    context_->SetMetadataDatabase(initializer->PassMetadataDatabase());
  }


  void RegisterApp(const std::string& app_id,
                   const std::string& app_root_folder_id) {
    SyncStatusCode status = context_->GetMetadataDatabase()->RegisterApp(
        app_id, app_root_folder_id);
    EXPECT_EQ(SYNC_STATUS_OK, status);
  }

  MetadataDatabase* GetMetadataDatabase() {
    return context_->GetMetadataDatabase();
  }

 protected:
  std::string CreateSyncRoot() {
    std::string sync_root_folder_id;
    EXPECT_EQ(google_apis::HTTP_CREATED,
              fake_drive_helper_->AddOrphanedFolder(
                  kSyncRootFolderTitle, &sync_root_folder_id));
    return sync_root_folder_id;
  }

  std::string CreateRemoteFolder(const std::string& parent_folder_id,
                                 const std::string& title) {
    std::string folder_id;
    EXPECT_EQ(google_apis::HTTP_CREATED,
              fake_drive_helper_->AddFolder(
                  parent_folder_id, title, &folder_id));
    return folder_id;
  }

  std::string CreateRemoteFile(const std::string& parent_folder_id,
                               const std::string& title,
                               const std::string& content) {
    std::string file_id;
    EXPECT_EQ(google_apis::HTTP_SUCCESS,
              fake_drive_helper_->AddFile(
                  parent_folder_id, title, content, &file_id));
    return file_id;
  }

  void DeleteRemoteFile(const std::string& file_id) {
    EXPECT_EQ(google_apis::HTTP_NO_CONTENT,
              fake_drive_helper_->DeleteResource(file_id));
  }

  void CreateLocalFolder(const storage::FileSystemURL& url) {
    remote_change_processor_->UpdateLocalFileMetadata(
        url, FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                        SYNC_FILE_TYPE_DIRECTORY));
  }

  void CreateLocalFile(const storage::FileSystemURL& url) {
    remote_change_processor_->UpdateLocalFileMetadata(
        url, FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                        SYNC_FILE_TYPE_FILE));
  }

  SyncStatusCode RunSyncer() {
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    std::unique_ptr<RemoteToLocalSyncer> syncer(
        new RemoteToLocalSyncer(context_.get()));
    syncer->RunPreflight(SyncTaskToken::CreateForTesting(
        CreateResultReceiver(&status)));
    base::RunLoop().RunUntilIdle();
    return status;
  }

  SyncStatusCode RunSyncerUntilIdle() {
    const int kRetryLimit = 100;
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    int count = 0;
    do {
      if (count++ > kRetryLimit)
        return status;
      status = RunSyncer();
    } while (status == SYNC_STATUS_OK ||
             status == SYNC_STATUS_RETRY);
    return status;
  }

  SyncStatusCode RunSyncerAndPromoteUntilIdle() {
    const int kRetryLimit = 100;
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    MetadataDatabase* metadata_database = context_->GetMetadataDatabase();
    int count = 0;
    do {
      if (count++ > kRetryLimit)
        return status;
      status = RunSyncer();
    } while (status == SYNC_STATUS_OK ||
             status == SYNC_STATUS_RETRY ||
             metadata_database->PromoteDemotedTrackers());
    return status;
  }

  SyncStatusCode ListChanges() {
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    sync_task_manager_->ScheduleSyncTask(
        FROM_HERE,
        std::unique_ptr<SyncTask>(new ListChangesTask(context_.get())),
        SyncTaskManager::PRIORITY_MED, CreateResultReceiver(&status));
    base::RunLoop().RunUntilIdle();
    return status;
  }

  void AppendExpectedChange(const storage::FileSystemURL& url,
                            FileChange::ChangeType change_type,
                            SyncFileType file_type) {
    expected_changes_[url].push_back(FileChange(change_type, file_type));
  }

  void VerifyConsistency() {
    remote_change_processor_->VerifyConsistency(expected_changes_);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir database_dir_;
  std::unique_ptr<leveldb::Env> in_memory_env_;

  std::unique_ptr<SyncEngineContext> context_;
  std::unique_ptr<FakeDriveServiceHelper> fake_drive_helper_;
  std::unique_ptr<FakeRemoteChangeProcessor> remote_change_processor_;

  std::unique_ptr<SyncTaskManager> sync_task_manager_;

  URLToFileChangesMap expected_changes_;

  DISALLOW_COPY_AND_ASSIGN(RemoteToLocalSyncerTest);
};

TEST_F(RemoteToLocalSyncerTest, AddNewFile) {
  const GURL kOrigin("chrome-extension://example");
  const std::string sync_root = CreateSyncRoot();
  const std::string app_root = CreateRemoteFolder(sync_root, kOrigin.host());
  InitializeMetadataDatabase();
  RegisterApp(kOrigin.host(), app_root);

  const std::string folder1 = CreateRemoteFolder(app_root, "folder1");
  const std::string file1 = CreateRemoteFile(app_root, "file1", "data1");
  const std::string folder2 = CreateRemoteFolder(folder1, "folder2");
  const std::string file2 = CreateRemoteFile(folder1, "file2", "data2");

  EXPECT_EQ(SYNC_STATUS_NO_CHANGE_TO_SYNC, RunSyncerAndPromoteUntilIdle());

  // Create expected changes.
  // TODO(nhiroki): Clean up creating URL part.
  AppendExpectedChange(URL(kOrigin, "folder1"),
                       FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                       SYNC_FILE_TYPE_DIRECTORY);
  AppendExpectedChange(URL(kOrigin, "file1"),
                       FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                       SYNC_FILE_TYPE_FILE);
  AppendExpectedChange(URL(kOrigin, "folder1/folder2"),
                       FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                       SYNC_FILE_TYPE_DIRECTORY);
  AppendExpectedChange(URL(kOrigin, "folder1/file2"),
                       FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                       SYNC_FILE_TYPE_FILE);

  VerifyConsistency();

  EXPECT_FALSE(GetMetadataDatabase()->HasDirtyTracker());
}

TEST_F(RemoteToLocalSyncerTest, DeleteFile) {
  const GURL kOrigin("chrome-extension://example");
  const std::string sync_root = CreateSyncRoot();
  const std::string app_root = CreateRemoteFolder(sync_root, kOrigin.host());
  InitializeMetadataDatabase();
  RegisterApp(kOrigin.host(), app_root);

  const std::string folder = CreateRemoteFolder(app_root, "folder");
  const std::string file = CreateRemoteFile(app_root, "file", "data");

  AppendExpectedChange(URL(kOrigin, "folder"),
                       FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                       SYNC_FILE_TYPE_DIRECTORY);
  AppendExpectedChange(URL(kOrigin, "file"),
                       FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                       SYNC_FILE_TYPE_FILE);

  EXPECT_EQ(SYNC_STATUS_NO_CHANGE_TO_SYNC, RunSyncerAndPromoteUntilIdle());
  VerifyConsistency();

  DeleteRemoteFile(folder);
  DeleteRemoteFile(file);

  AppendExpectedChange(URL(kOrigin, "folder"),
                       FileChange::FILE_CHANGE_DELETE,
                       SYNC_FILE_TYPE_UNKNOWN);
  AppendExpectedChange(URL(kOrigin, "file"),
                       FileChange::FILE_CHANGE_DELETE,
                       SYNC_FILE_TYPE_UNKNOWN);

  EXPECT_EQ(SYNC_STATUS_OK, ListChanges());
  EXPECT_EQ(SYNC_STATUS_NO_CHANGE_TO_SYNC, RunSyncerUntilIdle());
  VerifyConsistency();

  EXPECT_FALSE(GetMetadataDatabase()->HasDirtyTracker());
}

TEST_F(RemoteToLocalSyncerTest, DeleteNestedFiles) {
  const GURL kOrigin("chrome-extension://example");
  const std::string sync_root = CreateSyncRoot();
  const std::string app_root = CreateRemoteFolder(sync_root, kOrigin.host());
  InitializeMetadataDatabase();
  RegisterApp(kOrigin.host(), app_root);

  const std::string folder1 = CreateRemoteFolder(app_root, "folder1");
  const std::string file1 = CreateRemoteFile(app_root, "file1", "data1");
  const std::string folder2 = CreateRemoteFolder(folder1, "folder2");
  const std::string file2 = CreateRemoteFile(folder1, "file2", "data2");

  AppendExpectedChange(URL(kOrigin, "folder1"),
                       FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                       SYNC_FILE_TYPE_DIRECTORY);
  AppendExpectedChange(URL(kOrigin, "file1"),
                       FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                       SYNC_FILE_TYPE_FILE);
  AppendExpectedChange(URL(kOrigin, "folder1/folder2"),
                       FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                       SYNC_FILE_TYPE_DIRECTORY);
  AppendExpectedChange(URL(kOrigin, "folder1/file2"),
                       FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                       SYNC_FILE_TYPE_FILE);

  EXPECT_EQ(SYNC_STATUS_NO_CHANGE_TO_SYNC, RunSyncerAndPromoteUntilIdle());
  VerifyConsistency();

  DeleteRemoteFile(folder1);

  AppendExpectedChange(URL(kOrigin, "folder1"),
                       FileChange::FILE_CHANGE_DELETE,
                       SYNC_FILE_TYPE_UNKNOWN);
  // Changes for descendant files ("folder2" and "file2") should be ignored.

  EXPECT_EQ(SYNC_STATUS_OK, ListChanges());
  EXPECT_EQ(SYNC_STATUS_NO_CHANGE_TO_SYNC, RunSyncerUntilIdle());
  VerifyConsistency();

  EXPECT_FALSE(GetMetadataDatabase()->HasDirtyTracker());
}

TEST_F(RemoteToLocalSyncerTest, Conflict_CreateFileOnFolder) {
  const GURL kOrigin("chrome-extension://example");
  const std::string sync_root = CreateSyncRoot();
  const std::string app_root = CreateRemoteFolder(sync_root, kOrigin.host());
  InitializeMetadataDatabase();
  RegisterApp(kOrigin.host(), app_root);

  CreateLocalFolder(URL(kOrigin, "folder"));
  CreateRemoteFile(app_root, "folder", "data");

  // Folder-File conflict happens. File creation should be ignored.

  EXPECT_EQ(SYNC_STATUS_OK, ListChanges());
  EXPECT_EQ(SYNC_STATUS_NO_CHANGE_TO_SYNC, RunSyncerUntilIdle());
  VerifyConsistency();

  // Tracker for the remote file should has low priority.
  EXPECT_FALSE(GetMetadataDatabase()->GetDirtyTracker(nullptr));
  EXPECT_TRUE(GetMetadataDatabase()->HasDemotedDirtyTracker());
}

TEST_F(RemoteToLocalSyncerTest, Conflict_CreateFolderOnFile) {
  const GURL kOrigin("chrome-extension://example");
  const std::string sync_root = CreateSyncRoot();
  const std::string app_root = CreateRemoteFolder(sync_root, kOrigin.host());
  InitializeMetadataDatabase();
  RegisterApp(kOrigin.host(), app_root);

  EXPECT_EQ(SYNC_STATUS_NO_CHANGE_TO_SYNC, RunSyncerUntilIdle());
  VerifyConsistency();

  CreateLocalFile(URL(kOrigin, "file"));
  CreateRemoteFolder(app_root, "file");

  // File-Folder conflict happens. Folder should override the existing file.
  AppendExpectedChange(URL(kOrigin, "file"),
                       FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                       SYNC_FILE_TYPE_DIRECTORY);

  EXPECT_EQ(SYNC_STATUS_OK, ListChanges());
  EXPECT_EQ(SYNC_STATUS_NO_CHANGE_TO_SYNC, RunSyncerUntilIdle());
  VerifyConsistency();

  EXPECT_FALSE(GetMetadataDatabase()->HasDirtyTracker());
}

TEST_F(RemoteToLocalSyncerTest, Conflict_CreateFolderOnFolder) {
  const GURL kOrigin("chrome-extension://example");
  const std::string sync_root = CreateSyncRoot();
  const std::string app_root = CreateRemoteFolder(sync_root, kOrigin.host());
  InitializeMetadataDatabase();
  RegisterApp(kOrigin.host(), app_root);

  CreateLocalFolder(URL(kOrigin, "folder"));
  CreateRemoteFolder(app_root, "folder");

  // Folder-Folder conflict happens. Folder creation should be ignored.

  EXPECT_EQ(SYNC_STATUS_OK, ListChanges());
  EXPECT_EQ(SYNC_STATUS_NO_CHANGE_TO_SYNC, RunSyncerUntilIdle());
  VerifyConsistency();

  EXPECT_FALSE(GetMetadataDatabase()->HasDirtyTracker());
}

TEST_F(RemoteToLocalSyncerTest, Conflict_CreateFileOnFile) {
  const GURL kOrigin("chrome-extension://example");
  const std::string sync_root = CreateSyncRoot();
  const std::string app_root = CreateRemoteFolder(sync_root, kOrigin.host());
  InitializeMetadataDatabase();
  RegisterApp(kOrigin.host(), app_root);

  CreateLocalFile(URL(kOrigin, "file"));
  CreateRemoteFile(app_root, "file", "data");

  // File-File conflict happens. File creation should be ignored.

  EXPECT_EQ(SYNC_STATUS_OK, ListChanges());
  EXPECT_EQ(SYNC_STATUS_NO_CHANGE_TO_SYNC, RunSyncerUntilIdle());
  VerifyConsistency();

  // Tracker for the remote file should be lowered.
  EXPECT_FALSE(GetMetadataDatabase()->GetDirtyTracker(nullptr));
  EXPECT_TRUE(GetMetadataDatabase()->HasDemotedDirtyTracker());
}

TEST_F(RemoteToLocalSyncerTest, Conflict_CreateNestedFolderOnFile) {
  const GURL kOrigin("chrome-extension://example");
  const std::string sync_root = CreateSyncRoot();
  const std::string app_root = CreateRemoteFolder(sync_root, kOrigin.host());
  InitializeMetadataDatabase();
  RegisterApp(kOrigin.host(), app_root);

  EXPECT_EQ(SYNC_STATUS_NO_CHANGE_TO_SYNC, RunSyncerUntilIdle());
  VerifyConsistency();

  const std::string folder = CreateRemoteFolder(app_root, "folder");
  CreateLocalFile(URL(kOrigin, "/folder"));
  CreateRemoteFile(folder, "file", "data");

  // File-Folder conflict happens. Folder should override the existing file.
  AppendExpectedChange(URL(kOrigin, "/folder"),
                       FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                       SYNC_FILE_TYPE_DIRECTORY);

  EXPECT_EQ(SYNC_STATUS_OK, ListChanges());
  EXPECT_EQ(SYNC_STATUS_NO_CHANGE_TO_SYNC, RunSyncerUntilIdle());
  VerifyConsistency();
}

TEST_F(RemoteToLocalSyncerTest, AppRootDeletion) {
  const GURL kOrigin("chrome-extension://example");
  const std::string sync_root = CreateSyncRoot();
  const std::string app_root = CreateRemoteFolder(sync_root, kOrigin.host());
  InitializeMetadataDatabase();
  RegisterApp(kOrigin.host(), app_root);

  EXPECT_EQ(SYNC_STATUS_NO_CHANGE_TO_SYNC, RunSyncerUntilIdle());
  VerifyConsistency();

  DeleteRemoteFile(app_root);

  AppendExpectedChange(URL(kOrigin, "/"),
                       FileChange::FILE_CHANGE_DELETE,
                       SYNC_FILE_TYPE_UNKNOWN);

  EXPECT_EQ(SYNC_STATUS_OK, ListChanges());
  EXPECT_EQ(SYNC_STATUS_NO_CHANGE_TO_SYNC, RunSyncerUntilIdle());
  VerifyConsistency();

  // SyncEngine will re-register the app and resurrect the app root later.
}

}  // namespace drive_backend
}  // namespace sync_file_system
