// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/containers/stack.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "chrome/browser/sync_file_system/drive_backend/callback_helper.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_constants.h"
#include "chrome/browser/sync_file_system/drive_backend/fake_drive_service_helper.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.pb.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine_context.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_worker.h"
#include "chrome/browser/sync_file_system/local/canned_syncable_file_system.h"
#include "chrome/browser/sync_file_system/local/local_file_sync_context.h"
#include "chrome/browser/sync_file_system/local/local_file_sync_service.h"
#include "chrome/browser/sync_file_system/local/sync_file_system_backend.h"
#include "chrome/browser/sync_file_system/sync_file_system_test_util.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/drive/drive_uploader.h"
#include "components/drive/service/fake_drive_service.h"
#include "components/drive/service/test_util.h"
#include "components/services/filesystem/public/mojom/types.mojom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/extension.h"
#include "google_apis/drive/drive_api_parser.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "storage/browser/file_system/file_system_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/leveldb_chrome.h"

#define FPL(a) FILE_PATH_LITERAL(a)

namespace sync_file_system {
namespace drive_backend {

typedef storage::FileSystemOperation::FileEntryList FileEntryList;

namespace {

template <typename T>
void SetValueAndCallClosure(base::OnceClosure closure, T* arg_out, T arg) {
  *arg_out = std::forward<T>(arg);
  std::move(closure).Run();
}

void SetSyncStatusAndUrl(base::OnceClosure closure,
                         SyncStatusCode* status_out,
                         storage::FileSystemURL* url_out,
                         SyncStatusCode status,
                         const storage::FileSystemURL& url) {
  *status_out = status;
  *url_out = url;
  std::move(closure).Run();
}

}  // namespace

class DriveBackendSyncTest : public testing::Test,
                             public LocalFileSyncService::Observer,
                             public RemoteFileSyncService::Observer {
 public:
  DriveBackendSyncTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP),
        pending_remote_changes_(0),
        pending_local_changes_(0) {}

  DriveBackendSyncTest(const DriveBackendSyncTest&) = delete;
  DriveBackendSyncTest& operator=(const DriveBackendSyncTest&) = delete;

  ~DriveBackendSyncTest() override {}

  void SetUp() override {
    ASSERT_TRUE(base_dir_.CreateUniqueTempDir());
    in_memory_env_ = leveldb_chrome::NewMemEnv("DriveBackendSyncTest");

    io_task_runner_ = content::GetIOThreadTaskRunner({});
    worker_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
    file_task_runner_ = io_task_runner_;
    scoped_refptr<base::SequencedTaskRunner> drive_task_runner =
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

    RegisterSyncableFileSystem();
    local_sync_service_ =
        LocalFileSyncService::CreateForTesting(&profile_, in_memory_env_.get());
    local_sync_service_->AddChangeObserver(this);

    std::unique_ptr<drive::FakeDriveService> drive_service(
        new drive::FakeDriveService);
    drive_service->Initialize(CoreAccountId::FromGaiaId("account_id"));
    ASSERT_TRUE(drive::test_util::SetUpTestEntries(drive_service.get()));

    std::unique_ptr<drive::DriveUploaderInterface> uploader(
        new drive::DriveUploader(drive_service.get(), file_task_runner_.get(),
                                 mojo::NullRemote()));

    fake_drive_service_helper_ = std::make_unique<FakeDriveServiceHelper>(
        drive_service.get(), uploader.get(), kSyncRootFolderTitle);

    remote_sync_service_.reset(new SyncEngine(
        base::SingleThreadTaskRunner::GetCurrentDefault(),  // ui_task_runner
        worker_task_runner_.get(), drive_task_runner.get(), base_dir_.GetPath(),
        nullptr,  // task_logger
        nullptr,  // notification_manager
        nullptr,  // extension_service
        nullptr,  // extension_registry
        nullptr,  // identity_manager
        nullptr,  // url_loader_factory
        nullptr,  // drive_service
        in_memory_env_.get()));
    remote_sync_service_->AddServiceObserver(this);
    remote_sync_service_->InitializeForTesting(std::move(drive_service),
                                               std::move(uploader),
                                               nullptr /* sync_worker */);
    remote_sync_service_->SetSyncEnabled(true);

    local_sync_service_->SetLocalChangeProcessor(remote_sync_service_.get());
    remote_sync_service_->SetRemoteChangeProcessor(local_sync_service_.get());
  }

  void TearDown() override {
    for (auto itr = file_systems_.begin(); itr != file_systems_.end(); ++itr) {
      itr->second->TearDown();
      delete itr->second;
    }
    file_systems_.clear();

    local_sync_service_->Shutdown();

    fake_drive_service_helper_.reset();
    local_sync_service_.reset();
    remote_sync_service_.reset();

    base::ThreadPoolInstance::Get()->FlushForTesting();
    RevokeSyncableFileSystem();
  }

  void OnRemoteChangeQueueUpdated(int64_t pending_changes_hint) override {
    pending_remote_changes_ = pending_changes_hint;
  }

  void OnLocalChangeAvailable(int64_t pending_changes_hint) override {
    pending_local_changes_ = pending_changes_hint;
  }

 protected:
  storage::FileSystemURL CreateURL(const std::string& app_id,
                                   const base::FilePath::StringType& path) {
    return CreateURL(app_id, base::FilePath(path));
  }

  storage::FileSystemURL CreateURL(const std::string& app_id,
                                   const base::FilePath& path) {
    GURL origin = extensions::Extension::GetBaseURLFromExtensionId(app_id);
    return CreateSyncableFileSystemURL(origin, path);
  }

  bool GetAppRootFolderID(const std::string& app_id, std::string* folder_id) {
    base::RunLoop run_loop;
    bool success = false;
    FileTracker tracker;
    worker_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&MetadataDatabase::FindAppRootTracker,
                       base::Unretained(metadata_database()), app_id, &tracker),
        base::BindOnce(&SetValueAndCallClosure<bool>, run_loop.QuitClosure(),
                       &success));
    run_loop.Run();
    if (!success)
      return false;
    *folder_id = tracker.file_id();
    return true;
  }

  std::string GetFileIDByPath(const std::string& app_id,
                              const base::FilePath::StringType& path) {
    return GetFileIDByPath(app_id, base::FilePath(path));
  }

  std::string GetFileIDByPath(const std::string& app_id,
                              const base::FilePath& path) {
    base::RunLoop run_loop;
    bool success = false;
    FileTracker tracker;
    base::FilePath result_path;
    base::FilePath normalized_path = path.NormalizePathSeparators();
    worker_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&MetadataDatabase::FindNearestActiveAncestor,
                       base::Unretained(metadata_database()), app_id,
                       normalized_path, &tracker, &result_path),
        base::BindOnce(&SetValueAndCallClosure<bool>, run_loop.QuitClosure(),
                       &success));
    run_loop.Run();
    EXPECT_TRUE(success);
    EXPECT_EQ(normalized_path, result_path);
    return tracker.file_id();
  }

  SyncStatusCode RegisterApp(const std::string& app_id) {
    GURL origin = extensions::Extension::GetBaseURLFromExtensionId(app_id);
    if (!base::Contains(file_systems_, app_id)) {
      CannedSyncableFileSystem* file_system = new CannedSyncableFileSystem(
          origin, in_memory_env_.get(), io_task_runner_.get(),
          file_task_runner_.get());
      file_system->SetUp();

      SyncStatusCode status = SYNC_STATUS_UNKNOWN;
      base::RunLoop run_loop;
      local_sync_service_->MaybeInitializeFileSystemContext(
          origin, file_system->file_system_context(),
          base::BindOnce(&SetValueAndCallClosure<SyncStatusCode>,
                         run_loop.QuitClosure(), &status));
      run_loop.Run();
      EXPECT_EQ(SYNC_STATUS_OK, status);

      file_system->backend()
          ->sync_context()
          ->set_mock_notify_changes_duration_in_sec(0);

      EXPECT_EQ(base::File::FILE_OK, file_system->OpenFileSystem());
      file_systems_[app_id] = file_system;
    }

    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    base::RunLoop run_loop;
    remote_sync_service_->RegisterOrigin(
        origin, base::BindOnce(&SetValueAndCallClosure<SyncStatusCode>,
                               run_loop.QuitClosure(), &status));
    run_loop.Run();
    return status;
  }

  void AddLocalFolder(const std::string& app_id,
                      const base::FilePath::StringType& path) {
    ASSERT_TRUE(base::Contains(file_systems_, app_id));
    EXPECT_EQ(base::File::FILE_OK,
              file_systems_[app_id]->CreateDirectory(CreateURL(app_id, path)));
  }

  void AddOrUpdateLocalFile(const std::string& app_id,
                            const base::FilePath::StringType& path,
                            const std::string& content) {
    storage::FileSystemURL url(CreateURL(app_id, path));
    ASSERT_TRUE(base::Contains(file_systems_, app_id));
    EXPECT_EQ(base::File::FILE_OK, file_systems_[app_id]->CreateFile(url));
    int64_t bytes_written = file_systems_[app_id]->WriteString(url, content);
    EXPECT_EQ(static_cast<int64_t>(content.size()), bytes_written);
    base::RunLoop().RunUntilIdle();
  }

  void UpdateLocalFile(const std::string& app_id,
                       const base::FilePath::StringType& path,
                       const std::string& content) {
    ASSERT_TRUE(base::Contains(file_systems_, app_id));
    int64_t bytes_written =
        file_systems_[app_id]->WriteString(CreateURL(app_id, path), content);
    EXPECT_EQ(static_cast<int64_t>(content.size()), bytes_written);
    base::RunLoop().RunUntilIdle();
  }

  void RemoveLocal(const std::string& app_id,
                   const base::FilePath::StringType& path) {
    ASSERT_TRUE(base::Contains(file_systems_, app_id));
    EXPECT_EQ(base::File::FILE_OK,
              file_systems_[app_id]->Remove(CreateURL(app_id, path),
                                            true /* recursive */));
    base::RunLoop().RunUntilIdle();
  }

  SyncStatusCode ProcessLocalChange() {
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    storage::FileSystemURL url;
    base::RunLoop run_loop;
    local_sync_service_->ProcessLocalChange(base::BindOnce(
        &SetSyncStatusAndUrl, run_loop.QuitClosure(), &status, &url));
    run_loop.Run();
    return status;
  }

  SyncStatusCode ProcessRemoteChange() {
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    storage::FileSystemURL url;
    base::RunLoop run_loop;
    remote_sync_service_->ProcessRemoteChange(base::BindOnce(
        &SetSyncStatusAndUrl, run_loop.QuitClosure(), &status, &url));
    run_loop.Run();
    return status;
  }

  int64_t GetLargestChangeID() {
    std::unique_ptr<google_apis::AboutResource> about_resource;
    EXPECT_EQ(google_apis::HTTP_SUCCESS,
              fake_drive_service_helper()->GetAboutResource(&about_resource));
    if (!about_resource)
      return 0;
    return about_resource->largest_change_id();
  }

  void FetchRemoteChanges() {
    remote_sync_service_->OnNotificationTimerFired();
    WaitForIdleWorker();
  }

  SyncStatusCode ProcessChangesUntilDone() {
    int task_limit = 100;
    SyncStatusCode local_sync_status;
    SyncStatusCode remote_sync_status;
    while (true) {
      base::RunLoop().RunUntilIdle();
      WaitForIdleWorker();

      if (!task_limit--)
        return SYNC_STATUS_ABORT;

      local_sync_status = ProcessLocalChange();
      if (local_sync_status != SYNC_STATUS_OK &&
          local_sync_status != SYNC_STATUS_NO_CHANGE_TO_SYNC &&
          local_sync_status != SYNC_STATUS_FILE_BUSY)
        return local_sync_status;

      remote_sync_status = ProcessRemoteChange();
      if (remote_sync_status != SYNC_STATUS_OK &&
          remote_sync_status != SYNC_STATUS_NO_CHANGE_TO_SYNC &&
          remote_sync_status != SYNC_STATUS_FILE_BUSY)
        return remote_sync_status;

      if (local_sync_status == SYNC_STATUS_NO_CHANGE_TO_SYNC &&
          remote_sync_status == SYNC_STATUS_NO_CHANGE_TO_SYNC) {
        {
          base::RunLoop run_loop;
          remote_sync_service_->PromoteDemotedChanges(run_loop.QuitClosure());
          run_loop.Run();
        }

        {
          base::RunLoop run_loop;
          local_sync_service_->PromoteDemotedChanges(run_loop.QuitClosure());
          run_loop.Run();
        }

        if (pending_remote_changes_ || pending_local_changes_)
          continue;

        base::RunLoop run_loop;
        int64_t largest_fetched_change_id = -1;
        worker_task_runner_->PostTaskAndReplyWithResult(
            FROM_HERE,
            base::BindOnce(&MetadataDatabase::GetLargestFetchedChangeID,
                           base::Unretained(metadata_database())),
            base::BindOnce(&SetValueAndCallClosure<int64_t>,
                           run_loop.QuitClosure(), &largest_fetched_change_id));
        run_loop.Run();
        if (largest_fetched_change_id != GetLargestChangeID()) {
          FetchRemoteChanges();
          continue;
        }
        break;
      }
    }
    return SYNC_STATUS_OK;
  }

  // Verifies local and remote files/folders are consistent.
  // This function checks:
  //  - Each registered origin has corresponding remote folder.
  //  - Each local file/folder has corresponding remote one.
  //  - Each remote file/folder has corresponding local one.
  // TODO(tzik): Handle conflict case. i.e. allow remote file has different
  // file content if the corresponding local file conflicts to it.
  void VerifyConsistency() {
    std::string sync_root_folder_id;
    google_apis::ApiErrorCode error =
        fake_drive_service_helper_->GetSyncRootFolderID(&sync_root_folder_id);
    if (sync_root_folder_id.empty()) {
      EXPECT_EQ(google_apis::HTTP_NOT_FOUND, error);
      EXPECT_TRUE(file_systems_.empty());
      return;
    }
    EXPECT_EQ(google_apis::HTTP_SUCCESS, error);

    std::vector<std::unique_ptr<google_apis::FileResource>> remote_entries;
    EXPECT_EQ(google_apis::HTTP_SUCCESS,
              fake_drive_service_helper_->ListFilesInFolder(sync_root_folder_id,
                                                            &remote_entries));
    std::map<std::string, const google_apis::FileResource*> app_root_by_title;
    for (const auto& remote_entry : remote_entries) {
      EXPECT_FALSE(base::Contains(app_root_by_title, remote_entry->title()));
      app_root_by_title[remote_entry->title()] = remote_entry.get();
    }

    for (std::map<std::string, raw_ptr<CannedSyncableFileSystem,
                                       CtnExperimental>>::const_iterator itr =
             file_systems_.begin();
         itr != file_systems_.end(); ++itr) {
      const std::string& app_id = itr->first;
      SCOPED_TRACE(testing::Message() << "Verifying app: " << app_id);
      CannedSyncableFileSystem* file_system = itr->second;
      ASSERT_TRUE(base::Contains(app_root_by_title, app_id));
      VerifyConsistencyForFolder(app_id, base::FilePath(),
                                 app_root_by_title[app_id]->file_id(),
                                 file_system);
    }
  }

  void VerifyConsistencyForFolder(const std::string& app_id,
                                  const base::FilePath& path,
                                  const std::string& folder_id,
                                  CannedSyncableFileSystem* file_system) {
    SCOPED_TRACE(testing::Message() << "Verifying folder: " << path.value());

    std::vector<std::unique_ptr<google_apis::FileResource>> remote_entries;
    EXPECT_EQ(google_apis::HTTP_SUCCESS,
              fake_drive_service_helper_->ListFilesInFolder(folder_id,
                                                            &remote_entries));
    std::map<std::string, const google_apis::FileResource*>
        remote_entry_by_title;
    for (size_t i = 0; i < remote_entries.size(); ++i) {
      google_apis::FileResource* remote_entry = remote_entries[i].get();
      EXPECT_FALSE(base::Contains(remote_entry_by_title, remote_entry->title()))
          << "title: " << remote_entry->title();
      remote_entry_by_title[remote_entry->title()] = remote_entry;
    }

    storage::FileSystemURL url(CreateURL(app_id, path));
    FileEntryList local_entries;
    EXPECT_EQ(base::File::FILE_OK,
              file_system->ReadDirectory(url, &local_entries));
    for (const auto& local_entry : local_entries) {
      storage::FileSystemURL entry_url(
          CreateURL(app_id, path.Append(local_entry.name)));
      std::string title =
          storage::VirtualPath::BaseName(entry_url.path()).AsUTF8Unsafe();
      SCOPED_TRACE(testing::Message() << "Verifying entry: " << title);

      ASSERT_TRUE(base::Contains(remote_entry_by_title, title));
      const google_apis::FileResource& remote_entry =
          *remote_entry_by_title[title];
      if (local_entry.type == filesystem::mojom::FsFileType::DIRECTORY) {
        ASSERT_TRUE(remote_entry.IsDirectory());
        VerifyConsistencyForFolder(app_id, entry_url.path(),
                                   remote_entry.file_id(), file_system);
      } else {
        ASSERT_FALSE(remote_entry.IsDirectory());
        VerifyConsistencyForFile(app_id, entry_url.path(),
                                 remote_entry.file_id(), file_system);
      }
      remote_entry_by_title.erase(title);
    }

    EXPECT_TRUE(remote_entry_by_title.empty());
  }

  void VerifyConsistencyForFile(const std::string& app_id,
                                const base::FilePath& path,
                                const std::string& file_id,
                                CannedSyncableFileSystem* file_system) {
    storage::FileSystemURL url(CreateURL(app_id, path));
    std::string file_content;
    EXPECT_EQ(google_apis::HTTP_SUCCESS,
              fake_drive_service_helper_->ReadFile(file_id, &file_content));
    EXPECT_EQ(base::File::FILE_OK, file_system->VerifyFile(url, file_content));
  }

  size_t CountApp() { return file_systems_.size(); }

  size_t CountLocalFile(const std::string& app_id) {
    if (!base::Contains(file_systems_, app_id))
      return 0;

    CannedSyncableFileSystem* file_system = file_systems_[app_id];
    base::stack<base::FilePath> folders;
    folders.push(base::FilePath());  // root folder

    size_t result = 1;
    while (!folders.empty()) {
      storage::FileSystemURL url(CreateURL(app_id, folders.top()));
      folders.pop();

      FileEntryList entries;
      EXPECT_EQ(base::File::FILE_OK, file_system->ReadDirectory(url, &entries));
      for (const auto& entry : entries) {
        ++result;
        if (entry.type == filesystem::mojom::FsFileType::DIRECTORY)
          folders.push(url.path().Append(entry.name));
      }
    }

    return result;
  }

  void VerifyLocalFile(const std::string& app_id,
                       const base::FilePath::StringType& path,
                       const std::string& content) {
    SCOPED_TRACE(testing::Message()
                 << "Verifying local file: "
                 << "app_id = " << app_id << ", path = " << path);
    ASSERT_TRUE(base::Contains(file_systems_, app_id));
    EXPECT_EQ(base::File::FILE_OK, file_systems_[app_id]->VerifyFile(
                                       CreateURL(app_id, path), content));
  }

  void VerifyLocalFolder(const std::string& app_id,
                         const base::FilePath::StringType& path) {
    SCOPED_TRACE(testing::Message()
                 << "Verifying local file: "
                 << "app_id = " << app_id << ", path = " << path);
    ASSERT_TRUE(base::Contains(file_systems_, app_id));
    EXPECT_EQ(base::File::FILE_OK,
              file_systems_[app_id]->DirectoryExists(CreateURL(app_id, path)));
  }

  size_t CountMetadata() {
    size_t count = 0;
    base::RunLoop run_loop;
    worker_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&MetadataDatabase::CountFileMetadata,
                       base::Unretained(metadata_database())),
        base::BindOnce(&SetValueAndCallClosure<size_t>, run_loop.QuitClosure(),
                       &count));
    run_loop.Run();
    return count;
  }

  size_t CountTracker() {
    size_t count = 0;
    base::RunLoop run_loop;
    worker_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&MetadataDatabase::CountFileTracker,
                       base::Unretained(metadata_database())),
        base::BindOnce(&SetValueAndCallClosure<size_t>, run_loop.QuitClosure(),
                       &count));
    run_loop.Run();
    return count;
  }

  drive::FakeDriveService* fake_drive_service() {
    return static_cast<drive::FakeDriveService*>(
        remote_sync_service_->drive_service_.get());
  }

  FakeDriveServiceHelper* fake_drive_service_helper() {
    return fake_drive_service_helper_.get();
  }

  void WaitForIdleWorker() {
    base::RunLoop run_loop;
    worker_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &SyncWorker::CallOnIdleForTesting, base::Unretained(sync_worker()),
            RelayCallbackToCurrentThread(FROM_HERE, run_loop.QuitClosure())));
    run_loop.Run();
  }

 private:
  SyncWorker* sync_worker() {
    return static_cast<SyncWorker*>(remote_sync_service_->sync_worker_.get());
  }

  // MetadataDatabase is normally used on the worker thread.
  // Use this only when there is no task running on the worker.
  MetadataDatabase* metadata_database() {
    return sync_worker()->context_->metadata_database_.get();
  }

  content::BrowserTaskEnvironment task_environment_;

  base::ScopedTempDir base_dir_;
  std::unique_ptr<leveldb::Env> in_memory_env_;
  TestingProfile profile_;

  std::unique_ptr<SyncEngine> remote_sync_service_;
  std::unique_ptr<LocalFileSyncService> local_sync_service_;

  int64_t pending_remote_changes_;
  int64_t pending_local_changes_;

  std::unique_ptr<FakeDriveServiceHelper> fake_drive_service_helper_;
  std::map<std::string, raw_ptr<CannedSyncableFileSystem, CtnExperimental>>
      file_systems_;

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> worker_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner_;
};

TEST_F(DriveBackendSyncTest, LocalToRemoteBasicTest) {
  std::string app_id = "example";

  RegisterApp(app_id);
  AddOrUpdateLocalFile(app_id, FPL("file"), "abcde");

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  EXPECT_EQ(1u, CountApp());
  EXPECT_EQ(2u, CountLocalFile(app_id));
  VerifyLocalFile(app_id, FPL("file"), "abcde");

  EXPECT_EQ(3u, CountMetadata());
  EXPECT_EQ(3u, CountTracker());
}

TEST_F(DriveBackendSyncTest, RemoteToLocalBasicTest) {
  std::string app_id = "example";
  RegisterApp(app_id);

  std::string app_root_folder_id;
  EXPECT_TRUE(GetAppRootFolderID(app_id, &app_root_folder_id));

  std::string file_id;
  EXPECT_EQ(google_apis::HTTP_SUCCESS,
            fake_drive_service_helper()->AddFile(app_root_folder_id, "file",
                                                 "abcde", &file_id));

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  EXPECT_EQ(1u, CountApp());
  EXPECT_EQ(2u, CountLocalFile(app_id));
  VerifyLocalFile(app_id, FPL("file"), "abcde");

  EXPECT_EQ(3u, CountMetadata());
  EXPECT_EQ(3u, CountTracker());
}

TEST_F(DriveBackendSyncTest, LocalFileUpdateTest) {
  std::string app_id = "example";
  const base::FilePath::StringType kPath(FPL("file"));

  RegisterApp(app_id);
  AddOrUpdateLocalFile(app_id, kPath, "abcde");

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  UpdateLocalFile(app_id, kPath, "1234567890");

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  EXPECT_EQ(1u, CountApp());
  EXPECT_EQ(2u, CountLocalFile(app_id));
  VerifyLocalFile(app_id, FPL("file"), "1234567890");

  EXPECT_EQ(3u, CountMetadata());
  EXPECT_EQ(3u, CountTracker());
}

TEST_F(DriveBackendSyncTest, RemoteFileUpdateTest) {
  std::string app_id = "example";

  RegisterApp(app_id);
  std::string remote_file_id;
  std::string app_root_folder_id;
  EXPECT_TRUE(GetAppRootFolderID(app_id, &app_root_folder_id));
  EXPECT_EQ(google_apis::HTTP_SUCCESS,
            fake_drive_service_helper()->AddFile(app_root_folder_id, "file",
                                                 "abcde", &remote_file_id));

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  EXPECT_EQ(google_apis::HTTP_SUCCESS, fake_drive_service_helper()->UpdateFile(
                                           remote_file_id, "1234567890"));

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  EXPECT_EQ(1u, CountApp());
  EXPECT_EQ(2u, CountLocalFile(app_id));
  VerifyLocalFile(app_id, FPL("file"), "1234567890");

  EXPECT_EQ(3u, CountMetadata());
  EXPECT_EQ(3u, CountTracker());
}

TEST_F(DriveBackendSyncTest, LocalFileDeletionTest) {
  std::string app_id = "example";
  const base::FilePath::StringType path(FPL("file"));

  RegisterApp(app_id);
  AddOrUpdateLocalFile(app_id, path, "abcde");

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  RemoveLocal(app_id, path);

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  EXPECT_EQ(1u, CountApp());
  EXPECT_EQ(1u, CountLocalFile(app_id));

  EXPECT_EQ(2u, CountMetadata());
  EXPECT_EQ(2u, CountTracker());
}

TEST_F(DriveBackendSyncTest, RemoteFileDeletionTest) {
  std::string app_id = "example";
  const base::FilePath::StringType path(FPL("file"));

  RegisterApp(app_id);
  AddOrUpdateLocalFile(app_id, path, "abcde");

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  std::string file_id = GetFileIDByPath(app_id, path);
  EXPECT_EQ(google_apis::HTTP_NO_CONTENT,
            fake_drive_service_helper()->DeleteResource(file_id));

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  EXPECT_EQ(1u, CountApp());
  EXPECT_EQ(1u, CountLocalFile(app_id));

  EXPECT_EQ(2u, CountMetadata());
  EXPECT_EQ(2u, CountTracker());
}

TEST_F(DriveBackendSyncTest, RemoteRenameTest) {
  std::string app_id = "example";
  const base::FilePath::StringType path(FPL("file"));

  RegisterApp(app_id);
  AddOrUpdateLocalFile(app_id, path, "abcde");

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  std::string file_id = GetFileIDByPath(app_id, path);
  EXPECT_EQ(
      google_apis::HTTP_SUCCESS,
      fake_drive_service_helper()->RenameResource(file_id, "renamed_file"));

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  EXPECT_EQ(1u, CountApp());
  EXPECT_EQ(2u, CountLocalFile(app_id));
  VerifyLocalFile(app_id, FPL("renamed_file"), "abcde");

  EXPECT_EQ(3u, CountMetadata());
  EXPECT_EQ(3u, CountTracker());
}

TEST_F(DriveBackendSyncTest, RemoteRenameAndRevertTest) {
  std::string app_id = "example";
  const base::FilePath::StringType path(FPL("file"));

  RegisterApp(app_id);
  AddOrUpdateLocalFile(app_id, path, "abcde");

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  std::string file_id = GetFileIDByPath(app_id, path);
  EXPECT_EQ(
      google_apis::HTTP_SUCCESS,
      fake_drive_service_helper()->RenameResource(file_id, "renamed_file"));

  FetchRemoteChanges();

  EXPECT_EQ(google_apis::HTTP_SUCCESS,
            fake_drive_service_helper()->RenameResource(
                file_id, base::FilePath(path).AsUTF8Unsafe()));

  FetchRemoteChanges();

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  EXPECT_EQ(1u, CountApp());
  EXPECT_EQ(2u, CountLocalFile(app_id));
  VerifyLocalFile(app_id, FPL("file"), "abcde");

  EXPECT_EQ(3u, CountMetadata());
  EXPECT_EQ(3u, CountTracker());
}

TEST_F(DriveBackendSyncTest, ReorganizeToOtherFolder) {
  std::string app_id = "example";
  const base::FilePath::StringType path(FPL("file"));

  RegisterApp(app_id);
  AddLocalFolder(app_id, FPL("folder_src"));
  AddLocalFolder(app_id, FPL("folder_dest"));
  AddOrUpdateLocalFile(app_id, FPL("folder_src/file"), "abcde");

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  std::string file_id = GetFileIDByPath(app_id, FPL("folder_src/file"));
  std::string src_folder_id = GetFileIDByPath(app_id, FPL("folder_src"));
  std::string dest_folder_id = GetFileIDByPath(app_id, FPL("folder_dest"));
  EXPECT_EQ(google_apis::HTTP_NO_CONTENT,
            fake_drive_service_helper()->RemoveResourceFromDirectory(
                src_folder_id, file_id));
  EXPECT_EQ(google_apis::HTTP_SUCCESS,
            fake_drive_service_helper()->AddResourceToDirectory(dest_folder_id,
                                                                file_id));

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  EXPECT_EQ(1u, CountApp());
  EXPECT_EQ(4u, CountLocalFile(app_id));
  VerifyLocalFolder(app_id, FPL("folder_dest"));
  VerifyLocalFile(app_id, FPL("folder_dest/file"), "abcde");

  EXPECT_EQ(5u, CountMetadata());
  EXPECT_EQ(5u, CountTracker());
}

TEST_F(DriveBackendSyncTest, ReorganizeToOtherApp) {
  std::string src_app_id = "src_app";
  std::string dest_app_id = "dest_app";

  RegisterApp(src_app_id);
  RegisterApp(dest_app_id);

  AddLocalFolder(src_app_id, FPL("folder_src"));
  AddLocalFolder(dest_app_id, FPL("folder_dest"));
  AddOrUpdateLocalFile(src_app_id, FPL("folder_src/file"), "abcde");

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  std::string file_id = GetFileIDByPath(src_app_id, FPL("folder_src/file"));
  std::string src_folder_id = GetFileIDByPath(src_app_id, FPL("folder_src"));
  std::string dest_folder_id = GetFileIDByPath(dest_app_id, FPL("folder_dest"));
  EXPECT_EQ(google_apis::HTTP_NO_CONTENT,
            fake_drive_service_helper()->RemoveResourceFromDirectory(
                src_folder_id, file_id));
  EXPECT_EQ(google_apis::HTTP_SUCCESS,
            fake_drive_service_helper()->AddResourceToDirectory(dest_folder_id,
                                                                file_id));

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  EXPECT_EQ(2u, CountApp());
  EXPECT_EQ(2u, CountLocalFile(src_app_id));
  EXPECT_EQ(3u, CountLocalFile(dest_app_id));
  VerifyLocalFile(dest_app_id, FPL("folder_dest/file"), "abcde");

  EXPECT_EQ(6u, CountMetadata());
  EXPECT_EQ(6u, CountTracker());
}

TEST_F(DriveBackendSyncTest, ReorganizeToUnmanagedArea) {
  std::string app_id = "example";

  RegisterApp(app_id);

  AddLocalFolder(app_id, FPL("folder_src"));
  AddOrUpdateLocalFile(app_id, FPL("folder_src/file_orphaned"), "abcde");
  AddOrUpdateLocalFile(app_id, FPL("folder_src/file_under_sync_root"), "123");
  AddOrUpdateLocalFile(app_id, FPL("folder_src/file_under_drive_root"), "hoge");

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  std::string file_orphaned_id =
      GetFileIDByPath(app_id, FPL("folder_src/file_orphaned"));
  std::string file_under_sync_root_id =
      GetFileIDByPath(app_id, FPL("folder_src/file_under_sync_root"));
  std::string file_under_drive_root_id =
      GetFileIDByPath(app_id, FPL("folder_src/file_under_drive_root"));

  std::string folder_id = GetFileIDByPath(app_id, FPL("folder_src"));
  std::string sync_root_folder_id;
  EXPECT_EQ(
      google_apis::HTTP_SUCCESS,
      fake_drive_service_helper()->GetSyncRootFolderID(&sync_root_folder_id));

  EXPECT_EQ(google_apis::HTTP_NO_CONTENT,
            fake_drive_service_helper()->RemoveResourceFromDirectory(
                folder_id, file_orphaned_id));
  EXPECT_EQ(google_apis::HTTP_NO_CONTENT,
            fake_drive_service_helper()->RemoveResourceFromDirectory(
                folder_id, file_under_sync_root_id));
  EXPECT_EQ(google_apis::HTTP_NO_CONTENT,
            fake_drive_service_helper()->RemoveResourceFromDirectory(
                folder_id, file_under_drive_root_id));

  EXPECT_EQ(google_apis::HTTP_SUCCESS,
            fake_drive_service_helper()->AddResourceToDirectory(
                sync_root_folder_id, file_under_sync_root_id));
  EXPECT_EQ(
      google_apis::HTTP_SUCCESS,
      fake_drive_service_helper()->AddResourceToDirectory(
          fake_drive_service()->GetRootResourceId(), file_under_drive_root_id));

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  EXPECT_EQ(1u, CountApp());
  EXPECT_EQ(2u, CountLocalFile(app_id));

  EXPECT_EQ(4u, CountMetadata());
  EXPECT_EQ(4u, CountTracker());
}

TEST_F(DriveBackendSyncTest, ReorganizeToMultipleParents) {
  std::string app_id = "example";

  RegisterApp(app_id);

  AddLocalFolder(app_id, FPL("parent1"));
  AddLocalFolder(app_id, FPL("parent2"));
  AddOrUpdateLocalFile(app_id, FPL("parent1/file"), "abcde");

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  std::string file_id = GetFileIDByPath(app_id, FPL("parent1/file"));
  std::string parent2_folder_id = GetFileIDByPath(app_id, FPL("parent2"));
  EXPECT_EQ(google_apis::HTTP_SUCCESS,
            fake_drive_service_helper()->AddResourceToDirectory(
                parent2_folder_id, file_id));

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  EXPECT_EQ(1u, CountApp());
  EXPECT_EQ(4u, CountLocalFile(app_id));
  VerifyLocalFolder(app_id, FPL("parent1"));
  VerifyLocalFolder(app_id, FPL("parent2"));
  VerifyLocalFile(app_id, FPL("parent1/file"), "abcde");

  EXPECT_EQ(5u, CountMetadata());
  EXPECT_EQ(5u, CountTracker());
}

TEST_F(DriveBackendSyncTest, ReorganizeAndRevert) {
  std::string app_id = "example";

  RegisterApp(app_id);

  AddLocalFolder(app_id, FPL("folder"));
  AddLocalFolder(app_id, FPL("folder_temp"));
  AddOrUpdateLocalFile(app_id, FPL("folder/file"), "abcde");

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  std::string file_id = GetFileIDByPath(app_id, FPL("folder/file"));
  std::string folder_id = GetFileIDByPath(app_id, FPL("folder"));
  std::string folder_temp_id = GetFileIDByPath(app_id, FPL("folder_temp"));
  EXPECT_EQ(google_apis::HTTP_NO_CONTENT,
            fake_drive_service_helper()->RemoveResourceFromDirectory(folder_id,
                                                                     file_id));
  EXPECT_EQ(google_apis::HTTP_SUCCESS,
            fake_drive_service_helper()->AddResourceToDirectory(folder_temp_id,
                                                                file_id));

  FetchRemoteChanges();

  EXPECT_EQ(google_apis::HTTP_NO_CONTENT,
            fake_drive_service_helper()->RemoveResourceFromDirectory(
                folder_temp_id, file_id));
  EXPECT_EQ(
      google_apis::HTTP_SUCCESS,
      fake_drive_service_helper()->AddResourceToDirectory(folder_id, file_id));

  FetchRemoteChanges();

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  EXPECT_EQ(1u, CountApp());
  EXPECT_EQ(4u, CountLocalFile(app_id));
  VerifyLocalFolder(app_id, FPL("folder"));
  VerifyLocalFile(app_id, FPL("folder/file"), "abcde");

  EXPECT_EQ(5u, CountMetadata());
  EXPECT_EQ(5u, CountTracker());
}

TEST_F(DriveBackendSyncTest, ConflictTest_ConflictTest_AddFolder_AddFolder) {
  std::string app_id = "example";

  RegisterApp(app_id);
  std::string app_root_folder_id = GetFileIDByPath(app_id, FPL(""));

  AddLocalFolder(app_id, FPL("conflict_to_pending_remote"));
  AddLocalFolder(app_id, FPL("conflict_to_existing_remote"));

  std::string remote_folder_id;
  EXPECT_EQ(
      google_apis::HTTP_CREATED,
      fake_drive_service_helper()->AddFolder(
          app_root_folder_id, "conflict_to_pending_remote", &remote_folder_id));

  FetchRemoteChanges();

  EXPECT_EQ(google_apis::HTTP_CREATED,
            fake_drive_service_helper()->AddFolder(
                app_root_folder_id, "conflict_to_existing_remote",
                &remote_folder_id));

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  EXPECT_EQ(1u, CountApp());
  EXPECT_EQ(3u, CountLocalFile(app_id));
  VerifyLocalFolder(app_id, FPL("conflict_to_pending_remote"));
  VerifyLocalFolder(app_id, FPL("conflict_to_existing_remote"));

  EXPECT_EQ(4u, CountMetadata());
  EXPECT_EQ(4u, CountTracker());
}

TEST_F(DriveBackendSyncTest, ConflictTest_AddFolder_DeleteFolder) {
  std::string app_id = "example";

  RegisterApp(app_id);

  AddLocalFolder(app_id, FPL("conflict_to_pending_remote"));
  AddLocalFolder(app_id, FPL("conflict_to_existing_remote"));

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  // Test body starts from here.
  RemoveLocal(app_id, FPL("conflict_to_pending_remote"));
  AddLocalFolder(app_id, FPL("conflict_to_pending_remote"));
  RemoveLocal(app_id, FPL("conflict_to_existing_remote"));
  AddLocalFolder(app_id, FPL("conflict_to_existing_remote"));

  EXPECT_EQ(google_apis::HTTP_NO_CONTENT,
            fake_drive_service_helper()->DeleteResource(
                GetFileIDByPath(app_id, FPL("conflict_to_pending_remote"))));

  FetchRemoteChanges();

  EXPECT_EQ(google_apis::HTTP_NO_CONTENT,
            fake_drive_service_helper()->DeleteResource(
                GetFileIDByPath(app_id, FPL("conflict_to_existing_remote"))));

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  EXPECT_EQ(1u, CountApp());
  EXPECT_EQ(2u, CountLocalFile(app_id));
  VerifyLocalFolder(app_id, FPL("conflict_to_pending_remote"));

  EXPECT_EQ(3u, CountMetadata());
  EXPECT_EQ(3u, CountTracker());
}

TEST_F(DriveBackendSyncTest, ConflictTest_AddFolder_AddFile) {
  std::string app_id = "example";

  RegisterApp(app_id);
  std::string app_root_folder_id = GetFileIDByPath(app_id, FPL(""));

  AddLocalFolder(app_id, FPL("conflict_to_pending_remote"));
  AddLocalFolder(app_id, FPL("conflict_to_existing_remote"));

  std::string file_id;
  EXPECT_EQ(
      google_apis::HTTP_SUCCESS,
      fake_drive_service_helper()->AddFile(
          app_root_folder_id, "conflict_to_pending_remote", "foo", &file_id));
  EXPECT_EQ(google_apis::HTTP_SUCCESS,
            fake_drive_service_helper()->UpdateModificationTime(
                file_id, base::Time::Now() + base::Days(1)));

  FetchRemoteChanges();

  EXPECT_EQ(
      google_apis::HTTP_SUCCESS,
      fake_drive_service_helper()->AddFile(
          app_root_folder_id, "conflict_to_existing_remote", "foo", &file_id));
  EXPECT_EQ(google_apis::HTTP_SUCCESS,
            fake_drive_service_helper()->UpdateModificationTime(
                file_id, base::Time::Now() + base::Days(1)));

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  EXPECT_EQ(1u, CountApp());
  EXPECT_EQ(3u, CountLocalFile(app_id));
  VerifyLocalFolder(app_id, FPL("conflict_to_pending_remote"));
  VerifyLocalFolder(app_id, FPL("conflict_to_existing_remote"));

  EXPECT_EQ(4u, CountMetadata());
  EXPECT_EQ(4u, CountTracker());
}

TEST_F(DriveBackendSyncTest, ConflictTest_AddFolder_DeleteFile) {
  std::string app_id = "example";

  RegisterApp(app_id);
  std::string app_root_folder_id = GetFileIDByPath(app_id, FPL(""));

  AddOrUpdateLocalFile(app_id, FPL("conflict_to_pending_remote"), "foo");
  AddOrUpdateLocalFile(app_id, FPL("conflict_to_existing_remote"), "bar");

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  // Test body starts from here.
  RemoveLocal(app_id, FPL("conflict_to_pending_remote"));
  AddLocalFolder(app_id, FPL("conflict_to_pending_remote"));

  RemoveLocal(app_id, FPL("conflict_to_existing_remote"));
  AddLocalFolder(app_id, FPL("conflict_to_existing_remote"));

  EXPECT_EQ(google_apis::HTTP_NO_CONTENT,
            fake_drive_service_helper()->DeleteResource(
                GetFileIDByPath(app_id, FPL("conflict_to_pending_remote"))));

  FetchRemoteChanges();

  EXPECT_EQ(google_apis::HTTP_NO_CONTENT,
            fake_drive_service_helper()->DeleteResource(
                GetFileIDByPath(app_id, FPL("conflict_to_existing_remote"))));

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  EXPECT_EQ(1u, CountApp());
  EXPECT_EQ(3u, CountLocalFile(app_id));
  VerifyLocalFolder(app_id, FPL("conflict_to_pending_remote"));
  VerifyLocalFolder(app_id, FPL("conflict_to_existing_remote"));

  EXPECT_EQ(4u, CountMetadata());
  EXPECT_EQ(4u, CountTracker());
}

TEST_F(DriveBackendSyncTest, ConflictTest_DeleteFolder_AddFolder) {
  std::string app_id = "example";

  RegisterApp(app_id);
  std::string app_root_folder_id = GetFileIDByPath(app_id, FPL(""));
  AddLocalFolder(app_id, FPL("conflict_to_pending_remote"));
  AddLocalFolder(app_id, FPL("conflict_to_existing_remote"));

  RemoveLocal(app_id, FPL("conflict_to_pending_remote"));
  RemoveLocal(app_id, FPL("conflict_to_existing_remote"));

  std::string file_id;
  EXPECT_EQ(google_apis::HTTP_CREATED,
            fake_drive_service_helper()->AddFolder(
                app_root_folder_id, "conflict_to_pending_remote", &file_id));

  FetchRemoteChanges();

  EXPECT_EQ(google_apis::HTTP_CREATED,
            fake_drive_service_helper()->AddFolder(
                app_root_folder_id, "conflict_to_existing_remote", nullptr));

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  EXPECT_EQ(1u, CountApp());
  EXPECT_EQ(3u, CountLocalFile(app_id));
  VerifyLocalFolder(app_id, FPL("conflict_to_pending_remote"));
  VerifyLocalFolder(app_id, FPL("conflict_to_existing_remote"));

  EXPECT_EQ(4u, CountMetadata());
  EXPECT_EQ(4u, CountTracker());
}

TEST_F(DriveBackendSyncTest, ConflictTest_DeleteFolder_DeleteFolder) {
  std::string app_id = "example";

  RegisterApp(app_id);
  std::string app_root_folder_id = GetFileIDByPath(app_id, FPL(""));

  AddLocalFolder(app_id, FPL("conflict_to_pending_remote"));
  AddLocalFolder(app_id, FPL("conflict_to_existing_remote"));

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  // Test body starts from here.
  RemoveLocal(app_id, FPL("conflict_to_pending_remote"));
  RemoveLocal(app_id, FPL("conflict_to_existing_remote"));

  EXPECT_EQ(google_apis::HTTP_NO_CONTENT,
            fake_drive_service_helper()->DeleteResource(
                GetFileIDByPath(app_id, FPL("conflict_to_pending_remote"))));

  FetchRemoteChanges();

  EXPECT_EQ(google_apis::HTTP_NO_CONTENT,
            fake_drive_service_helper()->DeleteResource(
                GetFileIDByPath(app_id, FPL("conflict_to_existing_remote"))));

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  EXPECT_EQ(1u, CountApp());
  EXPECT_EQ(1u, CountLocalFile(app_id));

  EXPECT_EQ(2u, CountMetadata());
  EXPECT_EQ(2u, CountTracker());
}

TEST_F(DriveBackendSyncTest, ConflictTest_DeleteFolder_AddFile) {
  std::string app_id = "example";

  RegisterApp(app_id);
  std::string app_root_folder_id = GetFileIDByPath(app_id, FPL(""));

  AddLocalFolder(app_id, FPL("conflict_to_pending_remote"));
  AddLocalFolder(app_id, FPL("conflict_to_existing_remote"));
  RemoveLocal(app_id, FPL("conflict_to_pending_remote"));
  RemoveLocal(app_id, FPL("conflict_to_existing_remote"));

  EXPECT_EQ(
      google_apis::HTTP_SUCCESS,
      fake_drive_service_helper()->AddFile(
          app_root_folder_id, "conflict_to_pending_remote", "foo", nullptr));

  FetchRemoteChanges();

  EXPECT_EQ(
      google_apis::HTTP_SUCCESS,
      fake_drive_service_helper()->AddFile(
          app_root_folder_id, "conflict_to_existing_remote", "bar", nullptr));

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  EXPECT_EQ(1u, CountApp());
  EXPECT_EQ(3u, CountLocalFile(app_id));
  VerifyLocalFile(app_id, FPL("conflict_to_pending_remote"), "foo");
  VerifyLocalFile(app_id, FPL("conflict_to_existing_remote"), "bar");

  EXPECT_EQ(4u, CountMetadata());
  EXPECT_EQ(4u, CountTracker());
}

TEST_F(DriveBackendSyncTest, ConflictTest_DeleteFolder_DeleteFile) {
  std::string app_id = "example";

  RegisterApp(app_id);
  std::string app_root_folder_id = GetFileIDByPath(app_id, FPL(""));
  AddOrUpdateLocalFile(app_id, FPL("conflict_to_pending_remote"), "foo");
  AddOrUpdateLocalFile(app_id, FPL("conflict_to_existing_remote"), "bar");

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  // Test body starts from here.
  RemoveLocal(app_id, FPL("conflict_to_pending_remote"));
  RemoveLocal(app_id, FPL("conflict_to_existing_remote"));

  EXPECT_EQ(google_apis::HTTP_NO_CONTENT,
            fake_drive_service_helper()->DeleteResource(
                GetFileIDByPath(app_id, FPL("conflict_to_pending_remote"))));

  FetchRemoteChanges();

  EXPECT_EQ(google_apis::HTTP_NO_CONTENT,
            fake_drive_service_helper()->DeleteResource(
                GetFileIDByPath(app_id, FPL("conflict_to_existing_remote"))));

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  EXPECT_EQ(1u, CountApp());
  EXPECT_EQ(1u, CountLocalFile(app_id));

  EXPECT_EQ(2u, CountMetadata());
  EXPECT_EQ(2u, CountTracker());
}

TEST_F(DriveBackendSyncTest, ConflictTest_AddFile_AddFolder) {
  std::string app_id = "example";

  RegisterApp(app_id);
  std::string app_root_folder_id = GetFileIDByPath(app_id, FPL(""));

  AddOrUpdateLocalFile(app_id, FPL("conflict_to_pending_remote"), "foo");
  AddOrUpdateLocalFile(app_id, FPL("conflict_to_existing_remote"), "bar");

  std::string file_id;
  EXPECT_EQ(google_apis::HTTP_CREATED,
            fake_drive_service_helper()->AddFolder(
                app_root_folder_id, "conflict_to_pending_remote", &file_id));
  EXPECT_EQ(google_apis::HTTP_SUCCESS,
            fake_drive_service_helper()->UpdateModificationTime(
                file_id, base::Time::Now() - base::Days(1)));

  FetchRemoteChanges();

  EXPECT_EQ(google_apis::HTTP_CREATED,
            fake_drive_service_helper()->AddFolder(
                app_root_folder_id, "conflict_to_existing_remote", &file_id));
  EXPECT_EQ(google_apis::HTTP_SUCCESS,
            fake_drive_service_helper()->UpdateModificationTime(
                file_id, base::Time::Now() - base::Days(1)));

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  EXPECT_EQ(1u, CountApp());
  EXPECT_EQ(3u, CountLocalFile(app_id));
  VerifyLocalFolder(app_id, FPL("conflict_to_pending_remote"));
  VerifyLocalFolder(app_id, FPL("conflict_to_existing_remote"));

  EXPECT_EQ(4u, CountMetadata());
  EXPECT_EQ(4u, CountTracker());
}

TEST_F(DriveBackendSyncTest, ConflictTest_AddFile_DeleteFolder) {
  std::string app_id = "example";

  RegisterApp(app_id);
  std::string app_root_folder_id = GetFileIDByPath(app_id, FPL(""));

  AddLocalFolder(app_id, FPL("conflict_to_pending_remote"));
  AddLocalFolder(app_id, FPL("conflict_to_existing_remote"));

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  // Test body starts from here.
  RemoveLocal(app_id, FPL("conflict_to_pending_remote"));
  RemoveLocal(app_id, FPL("conflict_to_existing_remote"));
  AddOrUpdateLocalFile(app_id, FPL("conflict_to_pending_remote"), "foo");
  AddOrUpdateLocalFile(app_id, FPL("conflict_to_existing_remote"), "bar");

  EXPECT_EQ(google_apis::HTTP_NO_CONTENT,
            fake_drive_service_helper()->DeleteResource(
                GetFileIDByPath(app_id, FPL("conflict_to_pending_remote"))));

  FetchRemoteChanges();

  EXPECT_EQ(google_apis::HTTP_NO_CONTENT,
            fake_drive_service_helper()->DeleteResource(
                GetFileIDByPath(app_id, FPL("conflict_to_existing_remote"))));

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  EXPECT_EQ(1u, CountApp());
  EXPECT_EQ(3u, CountLocalFile(app_id));
  VerifyLocalFile(app_id, FPL("conflict_to_pending_remote"), "foo");
  VerifyLocalFile(app_id, FPL("conflict_to_existing_remote"), "bar");

  EXPECT_EQ(4u, CountMetadata());
  EXPECT_EQ(4u, CountTracker());
}

TEST_F(DriveBackendSyncTest, ConflictTest_AddFile_AddFile) {
  std::string app_id = "example";

  RegisterApp(app_id);

  std::string app_root_folder_id = GetFileIDByPath(app_id, FPL(""));
  AddOrUpdateLocalFile(app_id, FPL("conflict_to_pending_remote"), "hoge");
  AddOrUpdateLocalFile(app_id, FPL("conflict_to_existing_remote"), "fuga");

  std::string file_id;
  EXPECT_EQ(
      google_apis::HTTP_SUCCESS,
      fake_drive_service_helper()->AddFile(
          app_root_folder_id, "conflict_to_pending_remote", "foo", &file_id));
  EXPECT_EQ(google_apis::HTTP_SUCCESS,
            fake_drive_service_helper()->UpdateModificationTime(
                file_id, base::Time::Now() + base::Days(1)));

  FetchRemoteChanges();

  EXPECT_EQ(
      google_apis::HTTP_SUCCESS,
      fake_drive_service_helper()->AddFile(
          app_root_folder_id, "conflict_to_existing_remote", "bar", &file_id));
  EXPECT_EQ(google_apis::HTTP_SUCCESS,
            fake_drive_service_helper()->UpdateModificationTime(
                file_id, base::Time::Now() + base::Days(1)));

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  EXPECT_EQ(1u, CountApp());
  EXPECT_EQ(3u, CountLocalFile(app_id));
  VerifyLocalFile(app_id, FPL("conflict_to_pending_remote"), "foo");
  VerifyLocalFile(app_id, FPL("conflict_to_existing_remote"), "bar");

  EXPECT_EQ(4u, CountMetadata());
  EXPECT_EQ(4u, CountTracker());
}

TEST_F(DriveBackendSyncTest, ConflictTest_AddFile_DeleteFile) {
  std::string app_id = "example";

  RegisterApp(app_id);

  AddOrUpdateLocalFile(app_id, FPL("conflict_to_pending_remote"), "foo");
  AddOrUpdateLocalFile(app_id, FPL("conflict_to_existing_remote"), "bar");

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  // Test body starts from here.
  RemoveLocal(app_id, FPL("conflict_to_pending_remote"));
  RemoveLocal(app_id, FPL("conflict_to_existing_remote"));
  AddOrUpdateLocalFile(app_id, FPL("conflict_to_pending_remote"), "hoge");
  AddOrUpdateLocalFile(app_id, FPL("conflict_to_existing_remote"), "fuga");

  EXPECT_EQ(google_apis::HTTP_NO_CONTENT,
            fake_drive_service_helper()->DeleteResource(
                GetFileIDByPath(app_id, FPL("conflict_to_pending_remote"))));

  FetchRemoteChanges();

  EXPECT_EQ(google_apis::HTTP_NO_CONTENT,
            fake_drive_service_helper()->DeleteResource(
                GetFileIDByPath(app_id, FPL("conflict_to_existing_remote"))));

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  EXPECT_EQ(1u, CountApp());
  EXPECT_EQ(3u, CountLocalFile(app_id));
  VerifyLocalFile(app_id, FPL("conflict_to_pending_remote"), "hoge");
  VerifyLocalFile(app_id, FPL("conflict_to_existing_remote"), "fuga");

  EXPECT_EQ(4u, CountMetadata());
  EXPECT_EQ(4u, CountTracker());
}

TEST_F(DriveBackendSyncTest, ConflictTest_UpdateFile_DeleteFile) {
  std::string app_id = "example";

  RegisterApp(app_id);

  AddOrUpdateLocalFile(app_id, FPL("conflict_to_pending_remote"), "foo");
  AddOrUpdateLocalFile(app_id, FPL("conflict_to_existing_remote"), "bar");

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  // Test body starts from here.
  AddOrUpdateLocalFile(app_id, FPL("conflict_to_pending_remote"), "hoge");
  AddOrUpdateLocalFile(app_id, FPL("conflict_to_existing_remote"), "fuga");

  EXPECT_EQ(google_apis::HTTP_NO_CONTENT,
            fake_drive_service_helper()->DeleteResource(
                GetFileIDByPath(app_id, FPL("conflict_to_pending_remote"))));

  FetchRemoteChanges();

  EXPECT_EQ(google_apis::HTTP_NO_CONTENT,
            fake_drive_service_helper()->DeleteResource(
                GetFileIDByPath(app_id, FPL("conflict_to_existing_remote"))));

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  EXPECT_EQ(1u, CountApp());
  EXPECT_EQ(3u, CountLocalFile(app_id));
  VerifyLocalFile(app_id, FPL("conflict_to_pending_remote"), "hoge");
  VerifyLocalFile(app_id, FPL("conflict_to_existing_remote"), "fuga");

  EXPECT_EQ(4u, CountMetadata());
  EXPECT_EQ(4u, CountTracker());
}

TEST_F(DriveBackendSyncTest, ConflictTest_DeleteFile_AddFolder) {
  std::string app_id = "example";

  RegisterApp(app_id);

  std::string app_root_folder_id = GetFileIDByPath(app_id, FPL(""));
  AddOrUpdateLocalFile(app_id, FPL("conflict_to_pending_remote"), "foo");
  AddOrUpdateLocalFile(app_id, FPL("conflict_to_existing_remote"), "bar");

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  // Test body starts from here.
  RemoveLocal(app_id, FPL("conflict_to_pending_remote"));
  RemoveLocal(app_id, FPL("conflict_to_existing_remote"));

  EXPECT_EQ(google_apis::HTTP_CREATED,
            fake_drive_service_helper()->AddFolder(
                app_root_folder_id, "conflict_to_pending_remote", nullptr));

  FetchRemoteChanges();

  EXPECT_EQ(google_apis::HTTP_CREATED,
            fake_drive_service_helper()->AddFolder(
                app_root_folder_id, "conflict_to_existing_remote", nullptr));

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  EXPECT_EQ(1u, CountApp());
  EXPECT_EQ(3u, CountLocalFile(app_id));
  VerifyLocalFolder(app_id, FPL("conflict_to_pending_remote"));
  VerifyLocalFolder(app_id, FPL("conflict_to_existing_remote"));

  EXPECT_EQ(4u, CountMetadata());
  EXPECT_EQ(4u, CountTracker());
}

TEST_F(DriveBackendSyncTest, ConflictTest_DeleteFile_DeleteFolder) {
  std::string app_id = "example";

  RegisterApp(app_id);

  AddLocalFolder(app_id, FPL("conflict_to_pending_remote"));
  AddLocalFolder(app_id, FPL("conflict_to_existing_remote"));

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  // Test body starts from here.
  RemoveLocal(app_id, FPL("conflict_to_pending_remote"));
  RemoveLocal(app_id, FPL("conflict_to_existing_remote"));

  AddOrUpdateLocalFile(app_id, FPL("conflict_to_pending_remote"), "foo");
  AddOrUpdateLocalFile(app_id, FPL("conflict_to_existing_remote"), "bar");

  RemoveLocal(app_id, FPL("conflict_to_pending_remote"));
  RemoveLocal(app_id, FPL("conflict_to_existing_remote"));

  EXPECT_EQ(google_apis::HTTP_NO_CONTENT,
            fake_drive_service_helper()->DeleteResource(
                GetFileIDByPath(app_id, FPL("conflict_to_pending_remote"))));

  FetchRemoteChanges();

  EXPECT_EQ(google_apis::HTTP_NO_CONTENT,
            fake_drive_service_helper()->DeleteResource(
                GetFileIDByPath(app_id, FPL("conflict_to_existing_remote"))));

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  EXPECT_EQ(1u, CountApp());
  EXPECT_EQ(1u, CountLocalFile(app_id));

  EXPECT_EQ(2u, CountMetadata());
  EXPECT_EQ(2u, CountTracker());
}

TEST_F(DriveBackendSyncTest, ConflictTest_DeleteFile_AddFile) {
  std::string app_id = "example";

  RegisterApp(app_id);

  std::string app_root_folder_id = GetFileIDByPath(app_id, FPL(""));
  AddOrUpdateLocalFile(app_id, FPL("conflict_to_pending_remote"), "foo");
  AddOrUpdateLocalFile(app_id, FPL("conflict_to_existing_remote"), "bar");
  RemoveLocal(app_id, FPL("conflict_to_pending_remote"));
  RemoveLocal(app_id, FPL("conflict_to_existing_remote"));

  EXPECT_EQ(
      google_apis::HTTP_SUCCESS,
      fake_drive_service_helper()->AddFile(
          app_root_folder_id, "conflict_to_pending_remote", "hoge", nullptr));

  FetchRemoteChanges();

  EXPECT_EQ(
      google_apis::HTTP_SUCCESS,
      fake_drive_service_helper()->AddFile(
          app_root_folder_id, "conflict_to_existing_remote", "fuga", nullptr));

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  EXPECT_EQ(1u, CountApp());
  EXPECT_EQ(3u, CountLocalFile(app_id));
  VerifyLocalFile(app_id, FPL("conflict_to_pending_remote"), "hoge");
  VerifyLocalFile(app_id, FPL("conflict_to_existing_remote"), "fuga");

  EXPECT_EQ(4u, CountMetadata());
  EXPECT_EQ(4u, CountTracker());
}

TEST_F(DriveBackendSyncTest, ConflictTest_DeleteFile_UpdateFile) {
  std::string app_id = "example";

  RegisterApp(app_id);

  std::string app_root_folder_id = GetFileIDByPath(app_id, FPL(""));
  AddOrUpdateLocalFile(app_id, FPL("conflict_to_pending_remote"), "foo");
  AddOrUpdateLocalFile(app_id, FPL("conflict_to_existing_remote"), "bar");

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  // Test body starts from here.
  RemoveLocal(app_id, FPL("conflict_to_pending_remote"));
  RemoveLocal(app_id, FPL("conflict_to_existing_remote"));

  EXPECT_EQ(
      google_apis::HTTP_SUCCESS,
      fake_drive_service_helper()->UpdateFile(
          GetFileIDByPath(app_id, FPL("conflict_to_pending_remote")), "hoge"));

  FetchRemoteChanges();

  EXPECT_EQ(
      google_apis::HTTP_SUCCESS,
      fake_drive_service_helper()->UpdateFile(
          GetFileIDByPath(app_id, FPL("conflict_to_existing_remote")), "fuga"));

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  EXPECT_EQ(1u, CountApp());
  EXPECT_EQ(3u, CountLocalFile(app_id));
  VerifyLocalFile(app_id, FPL("conflict_to_pending_remote"), "hoge");
  VerifyLocalFile(app_id, FPL("conflict_to_existing_remote"), "fuga");

  EXPECT_EQ(4u, CountMetadata());
  EXPECT_EQ(4u, CountTracker());
}

TEST_F(DriveBackendSyncTest, ConflictTest_DeleteFile_DeleteFile) {
  std::string app_id = "example";

  RegisterApp(app_id);

  std::string app_root_folder_id = GetFileIDByPath(app_id, FPL(""));
  AddOrUpdateLocalFile(app_id, FPL("conflict_to_pending_remote"), "foo");
  AddOrUpdateLocalFile(app_id, FPL("conflict_to_existing_remote"), "bar");

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  // Test body starts from here.
  RemoveLocal(app_id, FPL("conflict_to_pending_remote"));
  RemoveLocal(app_id, FPL("conflict_to_existing_remote"));

  EXPECT_EQ(google_apis::HTTP_NO_CONTENT,
            fake_drive_service_helper()->DeleteResource(
                GetFileIDByPath(app_id, FPL("conflict_to_pending_remote"))));

  FetchRemoteChanges();

  EXPECT_EQ(google_apis::HTTP_NO_CONTENT,
            fake_drive_service_helper()->DeleteResource(
                GetFileIDByPath(app_id, FPL("conflict_to_existing_remote"))));

  EXPECT_EQ(SYNC_STATUS_OK, ProcessChangesUntilDone());
  VerifyConsistency();

  EXPECT_EQ(1u, CountApp());
  EXPECT_EQ(1u, CountLocalFile(app_id));

  EXPECT_EQ(2u, CountMetadata());
  EXPECT_EQ(2u, CountTracker());
}

}  // namespace drive_backend
}  // namespace sync_file_system
