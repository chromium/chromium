// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/conflict_resolver.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_constants.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_test_util.h"
#include "chrome/browser/sync_file_system/drive_backend/fake_drive_service_helper.h"
#include "chrome/browser/sync_file_system/drive_backend/fake_drive_uploader.h"
#include "chrome/browser/sync_file_system/drive_backend/list_changes_task.h"
#include "chrome/browser/sync_file_system/drive_backend/local_to_remote_syncer.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"
#include "chrome/browser/sync_file_system/drive_backend/remote_to_local_syncer.h"
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
#include "google_apis/drive/drive_api_parser.h"
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

class ConflictResolverTest : public testing::Test {
 public:
  typedef FakeRemoteChangeProcessor::URLToFileChangesMap URLToFileChangesMap;

  ConflictResolverTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}
  ~ConflictResolverTest() override {}

  void SetUp() override {
    ASSERT_TRUE(database_dir_.CreateUniqueTempDir());
    in_memory_env_ = leveldb_chrome::NewMemEnv("ConflictResolverTest");

    std::unique_ptr<FakeDriveServiceWrapper> fake_drive_service(
        new FakeDriveServiceWrapper);
    std::unique_ptr<drive::DriveUploaderInterface> drive_uploader(
        new FakeDriveUploader(fake_drive_service.get()));
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
        base::WeakPtr<SyncTaskManager::Client>(),
        10 /* maximum_background_task */, base::ThreadTaskRunnerHandle::Get()));
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
        base::Bind(&ConflictResolverTest::DidInitializeMetadataDatabase,
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

  void RegisterApp(const std::string& app_id,
                   const std::string& app_root_folder_id) {
    SyncStatusCode status = context_->GetMetadataDatabase()->RegisterApp(
        app_id, app_root_folder_id);
    EXPECT_EQ(SYNC_STATUS_OK, status);
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

  void CreateLocalFile(const storage::FileSystemURL& url) {
    remote_change_processor_->UpdateLocalFileMetadata(
        url, FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                        SYNC_FILE_TYPE_FILE));
  }

  google_apis::DriveApiErrorCode AddFileToFolder(
      const std::string& parent_folder_id,
      const std::string& file_id) {
    google_apis::DriveApiErrorCode error = google_apis::DRIVE_OTHER_ERROR;
    context_->GetDriveService()->AddResourceToDirectory(
        parent_folder_id, file_id,
        CreateResultReceiver(&error));
    base::RunLoop().RunUntilIdle();
    return error;
  }

  int CountParents(const std::string& file_id) {
    std::unique_ptr<google_apis::FileResource> entry;
    EXPECT_EQ(google_apis::HTTP_SUCCESS,
              fake_drive_helper_->GetFileResource(file_id, &entry));
    return entry->parents().size();
  }

  SyncStatusCode RunRemoteToLocalSyncer() {
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    std::unique_ptr<RemoteToLocalSyncer> syncer(
        new RemoteToLocalSyncer(context_.get()));
    syncer->RunPreflight(SyncTaskToken::CreateForTesting(
        CreateResultReceiver(&status)));
    base::RunLoop().RunUntilIdle();
    return status;
  }

  SyncStatusCode RunLocalToRemoteSyncer(const storage::FileSystemURL& url,
                                        const FileChange& file_change) {
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    base::FilePath local_path = base::FilePath(FILE_PATH_LITERAL("dummy"));
    if (file_change.IsAddOrUpdate())
      CreateTemporaryFileInDir(database_dir_.GetPath(), &local_path);
    std::unique_ptr<LocalToRemoteSyncer> syncer(new LocalToRemoteSyncer(
        context_.get(),
        SyncFileMetadata(file_change.file_type(), 0, base::Time()), file_change,
        local_path, url));
    syncer->RunPreflight(SyncTaskToken::CreateForTesting(
        CreateResultReceiver(&status)));
    base::RunLoop().RunUntilIdle();
    if (status == SYNC_STATUS_OK)
      remote_change_processor_->ClearLocalChanges(url);
    return status;
  }

  void RunRemoteToLocalSyncerUntilIdle() {
    const int kRetryLimit = 100;
    SyncStatusCode status;
    int retry_count = 0;
    MetadataDatabase* metadata_database = context_->GetMetadataDatabase();
    do {
      if (retry_count++ > kRetryLimit)
        break;
      status = RunRemoteToLocalSyncer();
    } while (status == SYNC_STATUS_OK ||
             status == SYNC_STATUS_RETRY ||
             metadata_database->PromoteDemotedTrackers());
    EXPECT_EQ(SYNC_STATUS_NO_CHANGE_TO_SYNC, status);
  }

  SyncStatusCode RunConflictResolver() {
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    ConflictResolver resolver(context_.get());
    resolver.RunPreflight(SyncTaskToken::CreateForTesting(
        CreateResultReceiver(&status)));
    base::RunLoop().RunUntilIdle();
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

  std::vector<std::unique_ptr<google_apis::FileResource>>
  GetResourceEntriesForParentAndTitle(const std::string& parent_folder_id,
                                      const std::string& title) {
    std::vector<std::unique_ptr<google_apis::FileResource>> entries;
    EXPECT_EQ(google_apis::HTTP_SUCCESS,
              fake_drive_helper_->SearchByTitle(
                  parent_folder_id, title, &entries));
    return entries;
  }

  void VerifyConflictResolution(
      const std::string& parent_folder_id,
      const std::string& title,
      const std::string& primary_file_id,
      test_util::FileResourceKind kind) {
    std::vector<std::unique_ptr<google_apis::FileResource>> entries;
    EXPECT_EQ(google_apis::HTTP_SUCCESS,
              fake_drive_helper_->SearchByTitle(
                  parent_folder_id, title, &entries));
    ASSERT_EQ(1u, entries.size());
    EXPECT_EQ(primary_file_id, entries[0]->file_id());
    EXPECT_EQ(kind, test_util::GetFileResourceKind(*entries[0]));
  }

  void VerifyLocalChangeConsistency(
      const URLToFileChangesMap& expected_changes) {
    remote_change_processor_->VerifyConsistency(expected_changes);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir database_dir_;
  std::unique_ptr<leveldb::Env> in_memory_env_;

  std::unique_ptr<SyncEngineContext> context_;
  std::unique_ptr<FakeDriveServiceHelper> fake_drive_helper_;
  std::unique_ptr<FakeRemoteChangeProcessor> remote_change_processor_;

  std::unique_ptr<SyncTaskManager> sync_task_manager_;

  DISALLOW_COPY_AND_ASSIGN(ConflictResolverTest);
};

TEST_F(ConflictResolverTest, NoFileToBeResolved) {
  const GURL kOrigin("chrome-extension://example");
  const std::string sync_root = CreateSyncRoot();
  const std::string app_root = CreateRemoteFolder(sync_root, kOrigin.host());
  InitializeMetadataDatabase();
  RegisterApp(kOrigin.host(), app_root);
  RunRemoteToLocalSyncerUntilIdle();

  EXPECT_EQ(SYNC_STATUS_NO_CONFLICT, RunConflictResolver());
}

TEST_F(ConflictResolverTest, ResolveConflict_Files) {
  const GURL kOrigin("chrome-extension://example");
  const std::string sync_root = CreateSyncRoot();
  const std::string app_root = CreateRemoteFolder(sync_root, kOrigin.host());
  InitializeMetadataDatabase();
  RegisterApp(kOrigin.host(), app_root);
  RunRemoteToLocalSyncerUntilIdle();

  const std::string kTitle = "foo";
  CreateRemoteFile(app_root, kTitle, "data1");
  CreateRemoteFile(app_root, kTitle, "data2");
  CreateRemoteFile(app_root, kTitle, "data3");
  const std::string primary = CreateRemoteFile(app_root, kTitle, "data4");
  EXPECT_EQ(SYNC_STATUS_OK, ListChanges());
  RunRemoteToLocalSyncerUntilIdle();

  std::vector<std::unique_ptr<google_apis::FileResource>> entries =
      GetResourceEntriesForParentAndTitle(app_root, kTitle);
  ASSERT_EQ(4u, entries.size());

  // Only primary file should survive.
  EXPECT_EQ(SYNC_STATUS_OK, RunConflictResolver());
  VerifyConflictResolution(app_root, kTitle, primary,
                           test_util::RESOURCE_KIND_FILE);
}

TEST_F(ConflictResolverTest, ResolveConflict_Folders) {
  const GURL kOrigin("chrome-extension://example");
  const std::string sync_root = CreateSyncRoot();
  const std::string app_root = CreateRemoteFolder(sync_root, kOrigin.host());
  InitializeMetadataDatabase();
  RegisterApp(kOrigin.host(), app_root);
  RunRemoteToLocalSyncerUntilIdle();

  const std::string kTitle = "foo";
  const std::string primary = CreateRemoteFolder(app_root, kTitle);
  CreateRemoteFolder(app_root, kTitle);
  CreateRemoteFolder(app_root, kTitle);
  CreateRemoteFolder(app_root, kTitle);
  EXPECT_EQ(SYNC_STATUS_OK, ListChanges());
  RunRemoteToLocalSyncerUntilIdle();

  std::vector<std::unique_ptr<google_apis::FileResource>> entries =
      GetResourceEntriesForParentAndTitle(app_root, kTitle);
  ASSERT_EQ(4u, entries.size());

  // Only primary file should survive.
  EXPECT_EQ(SYNC_STATUS_OK, RunConflictResolver());
  VerifyConflictResolution(app_root, kTitle, primary,
                           test_util::RESOURCE_KIND_FOLDER);
}

TEST_F(ConflictResolverTest, ResolveConflict_FilesAndFolders) {
  const GURL kOrigin("chrome-extension://example");
  const std::string sync_root = CreateSyncRoot();
  const std::string app_root = CreateRemoteFolder(sync_root, kOrigin.host());
  InitializeMetadataDatabase();
  RegisterApp(kOrigin.host(), app_root);
  RunRemoteToLocalSyncerUntilIdle();

  const std::string kTitle = "foo";
  CreateRemoteFile(app_root, kTitle, "data");
  const std::string primary = CreateRemoteFolder(app_root, kTitle);
  CreateRemoteFile(app_root, kTitle, "data2");
  CreateRemoteFolder(app_root, kTitle);
  EXPECT_EQ(SYNC_STATUS_OK, ListChanges());
  RunRemoteToLocalSyncerUntilIdle();

  std::vector<std::unique_ptr<google_apis::FileResource>> entries =
      GetResourceEntriesForParentAndTitle(app_root, kTitle);
  ASSERT_EQ(4u, entries.size());

  // Only primary file should survive.
  EXPECT_EQ(SYNC_STATUS_OK, RunConflictResolver());
  VerifyConflictResolution(app_root, kTitle, primary,
                           test_util::RESOURCE_KIND_FOLDER);
}

TEST_F(ConflictResolverTest, ResolveConflict_RemoteFolderOnLocalFile) {
  const GURL kOrigin("chrome-extension://example");
  const std::string sync_root = CreateSyncRoot();
  const std::string app_root = CreateRemoteFolder(sync_root, kOrigin.host());
  InitializeMetadataDatabase();
  RegisterApp(kOrigin.host(), app_root);
  RunRemoteToLocalSyncerUntilIdle();

  const std::string kTitle = "foo";
  storage::FileSystemURL kURL = URL(kOrigin, kTitle);

  // Create a file on local and sync it.
  CreateLocalFile(kURL);
  RunLocalToRemoteSyncer(
      kURL,
      FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE, SYNC_FILE_TYPE_FILE));

  // Create a folder on remote and sync it.
  const std::string primary = CreateRemoteFolder(app_root, kTitle);
  EXPECT_EQ(SYNC_STATUS_OK, ListChanges());
  RunRemoteToLocalSyncerUntilIdle();

  std::vector<std::unique_ptr<google_apis::FileResource>> entries =
      GetResourceEntriesForParentAndTitle(app_root, kTitle);
  ASSERT_EQ(2u, entries.size());

  // Run conflict resolver. Only primary file should survive.
  EXPECT_EQ(SYNC_STATUS_OK, RunConflictResolver());
  VerifyConflictResolution(app_root, kTitle, primary,
                           test_util::RESOURCE_KIND_FOLDER);

  // Continue to run remote-to-local sync.
  EXPECT_EQ(SYNC_STATUS_OK, ListChanges());
  RunRemoteToLocalSyncerUntilIdle();

  // Verify that the local side has been synced to the same state
  // (i.e. file deletion and folder creation).
  URLToFileChangesMap expected_changes;
  expected_changes[kURL].push_back(
      FileChange(FileChange::FILE_CHANGE_DELETE,
                 SYNC_FILE_TYPE_UNKNOWN));
  expected_changes[kURL].push_back(
      FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                 SYNC_FILE_TYPE_DIRECTORY));
  VerifyLocalChangeConsistency(expected_changes);
}

TEST_F(ConflictResolverTest, ResolveConflict_RemoteNestedFolderOnLocalFile) {
  const GURL kOrigin("chrome-extension://example");
  const std::string sync_root = CreateSyncRoot();
  const std::string app_root = CreateRemoteFolder(sync_root, kOrigin.host());
  InitializeMetadataDatabase();
  RegisterApp(kOrigin.host(), app_root);
  RunRemoteToLocalSyncerUntilIdle();

  const std::string kTitle = "foo";
  storage::FileSystemURL kURL = URL(kOrigin, kTitle);

  // Create a file on local and sync it.
  CreateLocalFile(kURL);
  RunLocalToRemoteSyncer(
      kURL,
      FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE, SYNC_FILE_TYPE_FILE));

  // Create a folder and subfolder in it on remote, and sync it.
  const std::string primary = CreateRemoteFolder(app_root, kTitle);
  CreateRemoteFolder(primary, "nested");
  EXPECT_EQ(SYNC_STATUS_OK, ListChanges());
  RunRemoteToLocalSyncerUntilIdle();

  std::vector<std::unique_ptr<google_apis::FileResource>> entries =
      GetResourceEntriesForParentAndTitle(app_root, kTitle);
  ASSERT_EQ(2u, entries.size());

  // Run conflict resolver. Only primary file should survive.
  EXPECT_EQ(SYNC_STATUS_OK, RunConflictResolver());
  VerifyConflictResolution(app_root, kTitle, primary,
                           test_util::RESOURCE_KIND_FOLDER);

  // Continue to run remote-to-local sync.
  EXPECT_EQ(SYNC_STATUS_OK, ListChanges());
  RunRemoteToLocalSyncerUntilIdle();

  // Verify that the local side has been synced to the same state
  // (i.e. file deletion and folders creation).
  URLToFileChangesMap expected_changes;
  expected_changes[kURL].push_back(
      FileChange(FileChange::FILE_CHANGE_DELETE,
                 SYNC_FILE_TYPE_UNKNOWN));
  expected_changes[kURL].push_back(
      FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                 SYNC_FILE_TYPE_DIRECTORY));
  expected_changes[URL(kOrigin, "foo/nested")].push_back(
      FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                 SYNC_FILE_TYPE_DIRECTORY));
  VerifyLocalChangeConsistency(expected_changes);
}

TEST_F(ConflictResolverTest, ResolveMultiParents_File) {
  const GURL kOrigin("chrome-extension://example");
  const std::string sync_root = CreateSyncRoot();
  const std::string app_root = CreateRemoteFolder(sync_root, kOrigin.host());
  InitializeMetadataDatabase();
  RegisterApp(kOrigin.host(), app_root);
  RunRemoteToLocalSyncerUntilIdle();

  const std::string primary = CreateRemoteFolder(app_root, "primary");
  const std::string file = CreateRemoteFile(primary, "file", "data");
  ASSERT_EQ(google_apis::HTTP_SUCCESS,
            AddFileToFolder(CreateRemoteFolder(app_root, "nonprimary1"), file));
  ASSERT_EQ(google_apis::HTTP_SUCCESS,
            AddFileToFolder(CreateRemoteFolder(app_root, "nonprimary2"), file));
  ASSERT_EQ(google_apis::HTTP_SUCCESS,
            AddFileToFolder(CreateRemoteFolder(app_root, "nonprimary3"), file));

  EXPECT_EQ(SYNC_STATUS_OK, ListChanges());
  RunRemoteToLocalSyncerUntilIdle();

  EXPECT_EQ(4, CountParents(file));

  EXPECT_EQ(SYNC_STATUS_OK, RunConflictResolver());

  EXPECT_EQ(1, CountParents(file));
}

TEST_F(ConflictResolverTest, ResolveMultiParents_Folder) {
  const GURL kOrigin("chrome-extension://example");
  const std::string sync_root = CreateSyncRoot();
  const std::string app_root = CreateRemoteFolder(sync_root, kOrigin.host());
  InitializeMetadataDatabase();
  RegisterApp(kOrigin.host(), app_root);
  RunRemoteToLocalSyncerUntilIdle();

  const std::string primary = CreateRemoteFolder(app_root, "primary");
  const std::string file = CreateRemoteFolder(primary, "folder");
  ASSERT_EQ(google_apis::HTTP_SUCCESS,
            AddFileToFolder(CreateRemoteFolder(app_root, "nonprimary1"), file));
  ASSERT_EQ(google_apis::HTTP_SUCCESS,
            AddFileToFolder(CreateRemoteFolder(app_root, "nonprimary2"), file));
  ASSERT_EQ(google_apis::HTTP_SUCCESS,
            AddFileToFolder(CreateRemoteFolder(app_root, "nonprimary3"), file));

  EXPECT_EQ(SYNC_STATUS_OK, ListChanges());
  RunRemoteToLocalSyncerUntilIdle();

  EXPECT_EQ(4, CountParents(file));

  EXPECT_EQ(SYNC_STATUS_OK, RunConflictResolver());

  EXPECT_EQ(1, CountParents(file));
}

}  // namespace drive_backend
}  // namespace sync_file_system
