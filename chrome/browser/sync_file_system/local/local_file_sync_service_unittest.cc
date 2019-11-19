// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <vector>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/sync_file_system/file_change.h"
#include "chrome/browser/sync_file_system/local/canned_syncable_file_system.h"
#include "chrome/browser/sync_file_system/local/local_file_change_tracker.h"
#include "chrome/browser/sync_file_system/local/local_file_sync_context.h"
#include "chrome/browser/sync_file_system/local/local_file_sync_service.h"
#include "chrome/browser/sync_file_system/local/local_file_sync_status.h"
#include "chrome/browser/sync_file_system/local/mock_sync_status_observer.h"
#include "chrome/browser/sync_file_system/local/sync_file_system_backend.h"
#include "chrome/browser/sync_file_system/mock_local_change_processor.h"
#include "chrome/browser/sync_file_system/sync_file_metadata.h"
#include "chrome/browser/sync_file_system/sync_file_system_test_util.h"
#include "chrome/browser/sync_file_system/sync_status_code.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "storage/browser/file_system/file_system_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/leveldb_chrome.h"

using content::BrowserThread;
using storage::FileSystemURL;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::InvokeWithoutArgs;
using ::testing::StrictMock;

namespace sync_file_system {

namespace {

const char kOrigin[] = "http://example.com";

void DidPrepareForProcessRemoteChange(const base::Location& where,
                                      const base::Closure& oncompleted,
                                      SyncStatusCode expected_status,
                                      const SyncFileMetadata& expected_metadata,
                                      SyncStatusCode status,
                                      const SyncFileMetadata& metadata,
                                      const FileChangeList& changes) {
  SCOPED_TRACE(testing::Message() << where.ToString());
  ASSERT_EQ(expected_status, status);
  ASSERT_EQ(expected_metadata.file_type, metadata.file_type);
  ASSERT_EQ(expected_metadata.size, metadata.size);
  ASSERT_TRUE(changes.empty());
  oncompleted.Run();
}

void OnSyncCompleted(const base::Location& where,
                     const base::Closure& oncompleted,
                     SyncStatusCode expected_status,
                     const FileSystemURL& expected_url,
                     SyncStatusCode status,
                     const FileSystemURL& url) {
  SCOPED_TRACE(testing::Message() << where.ToString());
  ASSERT_EQ(expected_status, status);
  ASSERT_EQ(expected_url, url);
  oncompleted.Run();
}

void OnGetFileMetadata(const base::Location& where,
                       const base::Closure& oncompleted,
                       SyncStatusCode* status_out,
                       SyncFileMetadata* metadata_out,
                       SyncStatusCode status,
                       const SyncFileMetadata& metadata) {
  SCOPED_TRACE(testing::Message() << where.ToString());
  *status_out = status;
  *metadata_out = metadata;
  oncompleted.Run();
}

ACTION_P(MockStatusCallback, status) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                base::BindOnce(arg4, status));
}

ACTION_P2(MockStatusCallbackAndRecordChange, status, changes) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                base::BindOnce(arg4, status));
  changes->push_back(arg0);
}

}  // namespace

class LocalFileSyncServiceTest
    : public testing::Test,
      public LocalFileSyncService::Observer {
 protected:
  LocalFileSyncServiceTest()
      : task_environment_(content::BrowserTaskEnvironment::REAL_IO_THREAD),
        num_changes_(0) {}

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    in_memory_env_ = leveldb_chrome::NewMemEnv("LocalFileSyncServiceTest");

    file_system_.reset(new CannedSyncableFileSystem(
        GURL(kOrigin), in_memory_env_.get(),
        base::CreateSingleThreadTaskRunner({BrowserThread::IO}),
        base::CreateSingleThreadTaskRunner(
            {base::ThreadPool(), base::MayBlock()})));

    local_service_ = LocalFileSyncService::CreateForTesting(
        &profile_, in_memory_env_.get());

    file_system_->SetUp(CannedSyncableFileSystem::QUOTA_ENABLED);

    base::RunLoop run_loop;
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    local_service_->MaybeInitializeFileSystemContext(
        GURL(kOrigin), file_system_->file_system_context(),
        AssignAndQuitCallback(&run_loop, &status));
    run_loop.Run();

    local_service_->AddChangeObserver(this);

    EXPECT_EQ(base::File::FILE_OK, file_system_->OpenFileSystem());

    file_system_->backend()->sync_context()->
        set_mock_notify_changes_duration_in_sec(0);
  }

  void TearDown() override {
    local_service_->Shutdown();
    file_system_->TearDown();
    RevokeSyncableFileSystem();

    base::ThreadPoolInstance::Get()->FlushForTesting();
    content::RunAllPendingInMessageLoop(BrowserThread::IO);
  }

  // LocalChangeObserver overrides.
  void OnLocalChangeAvailable(int64_t num_changes) override {
    num_changes_ = num_changes;
  }

  void PrepareForProcessRemoteChange(
      const FileSystemURL& url,
      const base::Location& where,
      SyncStatusCode expected_status,
      const SyncFileMetadata& expected_metadata) {
    base::RunLoop run_loop;
    local_service_->PrepareForProcessRemoteChange(
        url,
        base::Bind(&DidPrepareForProcessRemoteChange,
                   where,
                   run_loop.QuitClosure(),
                   expected_status,
                   expected_metadata));
    run_loop.Run();
  }

  SyncStatusCode ApplyRemoteChange(const FileChange& change,
                                   const base::FilePath& local_path,
                                   const FileSystemURL& url) {
    SyncStatusCode sync_status = SYNC_STATUS_UNKNOWN;
    {
      base::RunLoop run_loop;
      local_service_->ApplyRemoteChange(
          change, local_path, url,
          AssignAndQuitCallback(&run_loop, &sync_status));
      run_loop.Run();
    }
    {
      base::RunLoop run_loop;
      local_service_->FinalizeRemoteSync(
          url,
          sync_status == SYNC_STATUS_OK,
          run_loop.QuitClosure());
      run_loop.Run();
    }
    return sync_status;
  }

  int64_t GetNumChangesInTracker() const {
    return file_system_->backend()->change_tracker()->num_changes();
  }

  content::BrowserTaskEnvironment task_environment_;

  base::ScopedTempDir temp_dir_;
  std::unique_ptr<leveldb::Env> in_memory_env_;
  TestingProfile profile_;

  std::unique_ptr<CannedSyncableFileSystem> file_system_;
  std::unique_ptr<LocalFileSyncService> local_service_;

  int64_t num_changes_;
};

// More complete tests for PrepareForProcessRemoteChange and ApplyRemoteChange
// are also in content_unittest:LocalFileSyncContextTest.
TEST_F(LocalFileSyncServiceTest, RemoteSyncStepsSimple) {
  const FileSystemURL kFile(file_system_->URL("file"));
  const FileSystemURL kDir(file_system_->URL("dir"));
  const char kTestFileData[] = "0123456789";
  const int kTestFileDataSize = static_cast<int>(base::size(kTestFileData) - 1);

  base::FilePath local_path;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &local_path));
  ASSERT_EQ(kTestFileDataSize,
            base::WriteFile(local_path, kTestFileData, kTestFileDataSize));

  // Run PrepareForProcessRemoteChange for kFile.
  SyncFileMetadata expected_metadata;
  expected_metadata.file_type = SYNC_FILE_TYPE_UNKNOWN;
  expected_metadata.size = 0;
  PrepareForProcessRemoteChange(kFile, FROM_HERE,
                                SYNC_STATUS_OK,
                                expected_metadata);

  // Run ApplyRemoteChange for kFile.
  FileChange change(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                    SYNC_FILE_TYPE_FILE);
  EXPECT_EQ(SYNC_STATUS_OK,
            ApplyRemoteChange(change, local_path, kFile));

  // Verify the file is synced.
  EXPECT_EQ(base::File::FILE_OK,
            file_system_->VerifyFile(kFile, kTestFileData));

  // Run PrepareForProcessRemoteChange for kDir.
  PrepareForProcessRemoteChange(kDir, FROM_HERE,
                                SYNC_STATUS_OK,
                                expected_metadata);

  // Run ApplyRemoteChange for kDir.
  change = FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                      SYNC_FILE_TYPE_DIRECTORY);
  EXPECT_EQ(SYNC_STATUS_OK,
            ApplyRemoteChange(change, base::FilePath(), kDir));

  // Verify the directory.
  EXPECT_EQ(base::File::FILE_OK,
            file_system_->DirectoryExists(kDir));

  // Run PrepareForProcessRemoteChange and ApplyRemoteChange for
  // kDir once again for deletion.
  expected_metadata.file_type = SYNC_FILE_TYPE_DIRECTORY;
  expected_metadata.size = 0;
  PrepareForProcessRemoteChange(kDir, FROM_HERE,
                                SYNC_STATUS_OK,
                                expected_metadata);

  change = FileChange(FileChange::FILE_CHANGE_DELETE, SYNC_FILE_TYPE_UNKNOWN);
  EXPECT_EQ(SYNC_STATUS_OK, ApplyRemoteChange(change, base::FilePath(), kDir));

  // Now the directory must have deleted.
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND,
            file_system_->DirectoryExists(kDir));
}

TEST_F(LocalFileSyncServiceTest, LocalChangeObserver) {
  const FileSystemURL kFile(file_system_->URL("file"));
  const FileSystemURL kDir(file_system_->URL("dir"));
  const char kTestFileData[] = "0123456789";
  const int kTestFileDataSize = static_cast<int>(base::size(kTestFileData) - 1);

  EXPECT_EQ(base::File::FILE_OK, file_system_->CreateFile(kFile));

  EXPECT_EQ(1, num_changes_);

  EXPECT_EQ(base::File::FILE_OK, file_system_->CreateDirectory(kDir));
  EXPECT_EQ(kTestFileDataSize,
            file_system_->WriteString(kFile, kTestFileData));

  EXPECT_EQ(2, num_changes_);
}

#if defined(OS_WIN)
// Flaky: http://crbug.com/171487
#define MAYBE_LocalChangeObserverMultipleContexts\
    DISABLED_LocalChangeObserverMultipleContexts
#else
#define MAYBE_LocalChangeObserverMultipleContexts\
    LocalChangeObserverMultipleContexts
#endif

TEST_F(LocalFileSyncServiceTest, MAYBE_LocalChangeObserverMultipleContexts) {
  const char kOrigin2[] = "http://foo";
  CannedSyncableFileSystem file_system2(
      GURL(kOrigin2), in_memory_env_.get(),
      base::CreateSingleThreadTaskRunner({BrowserThread::IO}),
      base::CreateSingleThreadTaskRunner(
          {base::ThreadPool(), base::MayBlock()}));
  file_system2.SetUp(CannedSyncableFileSystem::QUOTA_ENABLED);

  base::RunLoop run_loop;
  SyncStatusCode status = SYNC_STATUS_UNKNOWN;
  local_service_->MaybeInitializeFileSystemContext(
      GURL(kOrigin2), file_system2.file_system_context(),
      AssignAndQuitCallback(&run_loop, &status));
  run_loop.Run();

  EXPECT_EQ(base::File::FILE_OK, file_system2.OpenFileSystem());
  file_system2.backend()->sync_context()->
      set_mock_notify_changes_duration_in_sec(0);

  const FileSystemURL kFile1(file_system_->URL("file1"));
  const FileSystemURL kFile2(file_system_->URL("file2"));
  const FileSystemURL kFile3(file_system2.URL("file3"));
  const FileSystemURL kFile4(file_system2.URL("file4"));

  EXPECT_EQ(base::File::FILE_OK, file_system_->CreateFile(kFile1));
  EXPECT_EQ(base::File::FILE_OK, file_system_->CreateFile(kFile2));
  EXPECT_EQ(base::File::FILE_OK, file_system2.CreateFile(kFile3));
  EXPECT_EQ(base::File::FILE_OK, file_system2.CreateFile(kFile4));

  EXPECT_EQ(4, num_changes_);

  file_system2.TearDown();
}

TEST_F(LocalFileSyncServiceTest, ProcessLocalChange_CreateFile) {
  const FileSystemURL kFile(file_system_->URL("foo"));
  const char kTestFileData[] = "0123456789";
  const int kTestFileDataSize = static_cast<int>(base::size(kTestFileData) - 1);

  base::RunLoop run_loop;

  // We should get called OnSyncEnabled and OnWriteEnabled on kFile.
  // (OnWriteEnabled is called because we release lock before returning
  // from ApplyLocalChange)
  StrictMock<MockSyncStatusObserver> status_observer;
  EXPECT_CALL(status_observer, OnSyncEnabled(kFile)).Times(AtLeast(1));
  EXPECT_CALL(status_observer, OnWriteEnabled(kFile)).Times(AtLeast(0));
  file_system_->AddSyncStatusObserver(&status_observer);

  // Creates and writes into a file.
  EXPECT_EQ(base::File::FILE_OK, file_system_->CreateFile(kFile));
  EXPECT_EQ(kTestFileDataSize,
            file_system_->WriteString(kFile, std::string(kTestFileData)));

  // Retrieve the expected file info.
  base::File::Info info;
  base::FilePath platform_path;
  EXPECT_EQ(base::File::FILE_OK,
            file_system_->GetMetadataAndPlatformPath(
                kFile, &info, &platform_path));

  ASSERT_FALSE(info.is_directory);
  ASSERT_EQ(kTestFileDataSize, info.size);

  SyncFileMetadata metadata;
  metadata.file_type = SYNC_FILE_TYPE_FILE;
  metadata.size = info.size;
  metadata.last_modified = info.last_modified;

  // The local_change_processor's ApplyLocalChange should be called once
  // with ADD_OR_UPDATE change for TYPE_FILE.
  StrictMock<MockLocalChangeProcessor> local_change_processor;
  const FileChange change(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                          SYNC_FILE_TYPE_FILE);
  EXPECT_CALL(local_change_processor,
              ApplyLocalChange(change, _, metadata, kFile, _))
      .WillOnce(MockStatusCallback(SYNC_STATUS_OK));

  local_service_->SetLocalChangeProcessor(&local_change_processor);
  local_service_->ProcessLocalChange(
      base::Bind(&OnSyncCompleted, FROM_HERE, run_loop.QuitClosure(),
                 SYNC_STATUS_OK, kFile));

  run_loop.Run();

  file_system_->RemoveSyncStatusObserver(&status_observer);

  EXPECT_EQ(0, GetNumChangesInTracker());
}

TEST_F(LocalFileSyncServiceTest, ProcessLocalChange_CreateAndRemoveFile) {
  const FileSystemURL kFile(file_system_->URL("foo"));

  base::RunLoop run_loop;

  // We should get called OnSyncEnabled and possibly OnWriteEnabled (depends
  // on timing) on kFile.
  StrictMock<MockSyncStatusObserver> status_observer;
  EXPECT_CALL(status_observer, OnSyncEnabled(kFile)).Times(AtLeast(1));
  EXPECT_CALL(status_observer, OnWriteEnabled(kFile)).Times(AtLeast(0));
  file_system_->AddSyncStatusObserver(&status_observer);

  // Creates and then deletes a file.
  EXPECT_EQ(base::File::FILE_OK, file_system_->CreateFile(kFile));
  EXPECT_EQ(base::File::FILE_OK, file_system_->Remove(kFile, false));

  // The local_change_processor's ApplyLocalChange should be called once
  // with DELETE change for TYPE_FILE.
  // The file will NOT exist in the remote side and the processor might
  // return SYNC_FILE_ERROR_NOT_FOUND (as mocked).
  StrictMock<MockLocalChangeProcessor> local_change_processor;
  const FileChange change(FileChange::FILE_CHANGE_DELETE, SYNC_FILE_TYPE_FILE);
  EXPECT_CALL(local_change_processor, ApplyLocalChange(change, _, _, kFile, _))
      .WillOnce(MockStatusCallback(SYNC_FILE_ERROR_NOT_FOUND));

  // The sync should succeed anyway.
  local_service_->SetLocalChangeProcessor(&local_change_processor);
  local_service_->ProcessLocalChange(
      base::Bind(&OnSyncCompleted, FROM_HERE, run_loop.QuitClosure(),
                 SYNC_STATUS_OK, kFile));

  run_loop.Run();

  file_system_->RemoveSyncStatusObserver(&status_observer);

  EXPECT_EQ(0, GetNumChangesInTracker());
}

TEST_F(LocalFileSyncServiceTest, ProcessLocalChange_CreateAndRemoveDirectory) {
  const FileSystemURL kDir(file_system_->URL("foo"));

  base::RunLoop run_loop;

  // OnSyncEnabled is expected to be called at least or more than once.
  StrictMock<MockSyncStatusObserver> status_observer;
  EXPECT_CALL(status_observer, OnSyncEnabled(kDir)).Times(AtLeast(1));
  file_system_->AddSyncStatusObserver(&status_observer);

  // Creates and then deletes a directory.
  EXPECT_EQ(base::File::FILE_OK, file_system_->CreateDirectory(kDir));
  EXPECT_EQ(base::File::FILE_OK, file_system_->Remove(kDir, false));

  // The local_change_processor's ApplyLocalChange should never be called.
  StrictMock<MockLocalChangeProcessor> local_change_processor;

  local_service_->SetLocalChangeProcessor(&local_change_processor);
  local_service_->ProcessLocalChange(
      base::Bind(&OnSyncCompleted, FROM_HERE, run_loop.QuitClosure(),
                 SYNC_STATUS_NO_CHANGE_TO_SYNC, FileSystemURL()));

  run_loop.Run();

  file_system_->RemoveSyncStatusObserver(&status_observer);

  EXPECT_EQ(0, GetNumChangesInTracker());
}

TEST_F(LocalFileSyncServiceTest, ProcessLocalChange_MultipleChanges) {
  const FileSystemURL kPath(file_system_->URL("foo"));
  const FileSystemURL kOther(file_system_->URL("bar"));

  base::RunLoop run_loop;

  // We should get called OnSyncEnabled and OnWriteEnabled on kPath and
  // OnSyncEnabled on kOther.
  StrictMock<MockSyncStatusObserver> status_observer;
  EXPECT_CALL(status_observer, OnSyncEnabled(kPath)).Times(AtLeast(1));
  EXPECT_CALL(status_observer, OnSyncEnabled(kOther)).Times(AtLeast(1));
  file_system_->AddSyncStatusObserver(&status_observer);

  // Creates a file, delete the file and creates a directory with the same
  // name.
  EXPECT_EQ(base::File::FILE_OK, file_system_->CreateFile(kPath));
  EXPECT_EQ(base::File::FILE_OK, file_system_->Remove(kPath, false));
  EXPECT_EQ(base::File::FILE_OK, file_system_->CreateDirectory(kPath));

  // Creates one more file.
  EXPECT_EQ(base::File::FILE_OK, file_system_->CreateFile(kOther));

  // The local_change_processor's ApplyLocalChange will be called
  // twice for FILE_TYPE and FILE_DIRECTORY.
  StrictMock<MockLocalChangeProcessor> local_change_processor;
  std::vector<FileChange> changes;
  EXPECT_CALL(local_change_processor, ApplyLocalChange(_, _, _, kPath, _))
      .Times(2)
      .WillOnce(MockStatusCallbackAndRecordChange(SYNC_STATUS_OK, &changes))
      .WillOnce(MockStatusCallbackAndRecordChange(SYNC_STATUS_OK, &changes));
  local_service_->SetLocalChangeProcessor(&local_change_processor);

  // OnWriteEnabled will be notified on kPath (in multi-threaded this
  // could be delayed, so AtLeast(0)).
  EXPECT_CALL(status_observer, OnWriteEnabled(kPath)).Times(AtLeast(0));

  local_service_->ProcessLocalChange(
      base::Bind(&OnSyncCompleted, FROM_HERE, run_loop.QuitClosure(),
                 SYNC_STATUS_OK, kPath));

  run_loop.Run();

  EXPECT_EQ(2U, changes.size());
  EXPECT_EQ(FileChange(FileChange::FILE_CHANGE_DELETE, SYNC_FILE_TYPE_FILE),
            changes[0]);
  EXPECT_EQ(FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                       SYNC_FILE_TYPE_DIRECTORY),
            changes[1]);

  file_system_->RemoveSyncStatusObserver(&status_observer);

  // We have one more change for kOther.
  EXPECT_EQ(1, GetNumChangesInTracker());
}

TEST_F(LocalFileSyncServiceTest, ProcessLocalChange_GetLocalMetadata) {
  const FileSystemURL kURL(file_system_->URL("foo"));
  const base::Time kTime = base::Time::FromDoubleT(333);
  const int kSize = 555;

  base::RunLoop run_loop;

  // Creates a file.
  EXPECT_EQ(base::File::FILE_OK, file_system_->CreateFile(kURL));
  EXPECT_EQ(base::File::FILE_OK, file_system_->TruncateFile(kURL, kSize));
  EXPECT_EQ(base::File::FILE_OK,
            file_system_->TouchFile(kURL, base::Time(), kTime));

  SyncStatusCode status = SYNC_STATUS_UNKNOWN;
  SyncFileMetadata metadata;
  local_service_->GetLocalFileMetadata(
      kURL,
      base::Bind(&OnGetFileMetadata, FROM_HERE, run_loop.QuitClosure(),
                 &status, &metadata));

  run_loop.Run();

  EXPECT_EQ(SYNC_STATUS_OK, status);
  EXPECT_EQ(kTime, metadata.last_modified);
  EXPECT_EQ(kSize, metadata.size);
}

TEST_F(LocalFileSyncServiceTest, RecordFakeChange) {
  const FileSystemURL kURL(file_system_->URL("foo"));

  // Create a file and reset the changes (as preparation).
  EXPECT_EQ(base::File::FILE_OK, file_system_->CreateFile(kURL));
  file_system_->ClearChangeForURLInTracker(kURL);

  EXPECT_EQ(0, GetNumChangesInTracker());

  storage::FileSystemURLSet urlset;
  file_system_->GetChangedURLsInTracker(&urlset);
  EXPECT_TRUE(urlset.empty());

  const FileChange change(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                          SYNC_FILE_TYPE_FILE);

  // Call RecordFakeLocalChange to add an ADD_OR_UPDATE change.
  {
    base::RunLoop run_loop;
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    local_service_->RecordFakeLocalChange(
        kURL, change, AssignAndQuitCallback(&run_loop, &status));
    run_loop.Run();
    EXPECT_EQ(SYNC_STATUS_OK, status);
  }

  EXPECT_EQ(1, GetNumChangesInTracker());
  file_system_->GetChangedURLsInTracker(&urlset);
  EXPECT_EQ(1U, urlset.size());
  EXPECT_TRUE(urlset.find(kURL) != urlset.end());

  // Next local sync should pick up the recorded change.
  StrictMock<MockLocalChangeProcessor> local_change_processor;
  std::vector<FileChange> changes;
  EXPECT_CALL(local_change_processor, ApplyLocalChange(_, _, _, kURL, _))
      .WillOnce(MockStatusCallbackAndRecordChange(SYNC_STATUS_OK, &changes));
  {
    base::RunLoop run_loop;
    local_service_->SetLocalChangeProcessor(&local_change_processor);
    local_service_->ProcessLocalChange(
        base::Bind(&OnSyncCompleted, FROM_HERE, run_loop.QuitClosure(),
                   SYNC_STATUS_OK, kURL));
    run_loop.Run();
  }

  EXPECT_EQ(1U, changes.size());
  EXPECT_EQ(change, changes[0]);
}

// TODO(kinuko): Add tests for multiple file changes and multiple
// FileSystemContexts.

// Unit test for OriginChangeMap ---------------------------------------------

class OriginChangeMapTest : public testing::Test {
 protected:
  OriginChangeMapTest() {}
  ~OriginChangeMapTest() override {}

  bool NextOriginToProcess(GURL* origin) {
    return map_.NextOriginToProcess(origin);
  }

  int64_t GetTotalChangeCount() const { return map_.GetTotalChangeCount(); }

  void SetOriginChangeCount(const GURL& origin, int64_t changes) {
    map_.SetOriginChangeCount(origin, changes);
  }

  void SetOriginEnabled(const GURL& origin, bool enabled) {
    map_.SetOriginEnabled(origin, enabled);
  }

  LocalFileSyncService::OriginChangeMap map_;
};

TEST_F(OriginChangeMapTest, Basic) {
  const GURL kOrigin1("chrome-extension://foo");
  const GURL kOrigin2("chrome-extension://bar");
  const GURL kOrigin3("chrome-extension://baz");

  ASSERT_EQ(0, GetTotalChangeCount());

  SetOriginChangeCount(kOrigin1, 1);
  SetOriginChangeCount(kOrigin2, 2);

  ASSERT_EQ(1 + 2, GetTotalChangeCount());

  SetOriginChangeCount(kOrigin3, 4);

  ASSERT_EQ(1 + 2 + 4, GetTotalChangeCount());

  const GURL kOrigins[] = { kOrigin1, kOrigin2, kOrigin3 };
  std::set<GURL> all_origins;
  all_origins.insert(kOrigins, kOrigins + base::size(kOrigins));

  GURL origin;
  while (!all_origins.empty()) {
    ASSERT_TRUE(NextOriginToProcess(&origin));
    ASSERT_TRUE(base::Contains(all_origins, origin));
    all_origins.erase(origin);
  }

  // Set kOrigin2's change count 0.
  SetOriginChangeCount(kOrigin2, 0);
  ASSERT_EQ(1 + 4, GetTotalChangeCount());

  // kOrigin2 won't return this time.
  all_origins.insert(kOrigin1);
  all_origins.insert(kOrigin3);
  while (!all_origins.empty()) {
    ASSERT_TRUE(NextOriginToProcess(&origin));
    ASSERT_TRUE(base::Contains(all_origins, origin));
    all_origins.erase(origin);
  }

  // Calling NextOriginToProcess() again will just return
  // the same set of origins (as far as we don't change the
  // change count).
  all_origins.insert(kOrigin1);
  all_origins.insert(kOrigin3);
  while (!all_origins.empty()) {
    ASSERT_TRUE(NextOriginToProcess(&origin));
    ASSERT_TRUE(base::Contains(all_origins, origin));
    all_origins.erase(origin);
  }

  // Set kOrigin2's change count 8.
  SetOriginChangeCount(kOrigin2, 8);
  ASSERT_EQ(1 + 4 + 8, GetTotalChangeCount());

  all_origins.insert(kOrigins, kOrigins + base::size(kOrigins));
  while (!all_origins.empty()) {
    ASSERT_TRUE(NextOriginToProcess(&origin));
    ASSERT_TRUE(base::Contains(all_origins, origin));
    all_origins.erase(origin);
  }
}

TEST_F(OriginChangeMapTest, WithDisabled) {
  const GURL kOrigin1("chrome-extension://foo");
  const GURL kOrigin2("chrome-extension://bar");
  const GURL kOrigin3("chrome-extension://baz");
  const GURL kOrigins[] = { kOrigin1, kOrigin2, kOrigin3 };

  ASSERT_EQ(0, GetTotalChangeCount());

  SetOriginChangeCount(kOrigin1, 1);
  SetOriginChangeCount(kOrigin2, 2);
  SetOriginChangeCount(kOrigin3, 4);

  ASSERT_EQ(1 + 2 + 4, GetTotalChangeCount());

  std::set<GURL> all_origins;
  all_origins.insert(kOrigins, kOrigins + base::size(kOrigins));

  GURL origin;
  while (!all_origins.empty()) {
    ASSERT_TRUE(NextOriginToProcess(&origin));
    ASSERT_TRUE(base::Contains(all_origins, origin));
    all_origins.erase(origin);
  }

  SetOriginEnabled(kOrigin2, false);
  ASSERT_EQ(1 + 4, GetTotalChangeCount());

  // kOrigin2 won't return this time.
  all_origins.insert(kOrigin1);
  all_origins.insert(kOrigin3);
  while (!all_origins.empty()) {
    ASSERT_TRUE(NextOriginToProcess(&origin));
    ASSERT_TRUE(base::Contains(all_origins, origin));
    all_origins.erase(origin);
  }

  // kOrigin1 and kOrigin2 are now disabled.
  SetOriginEnabled(kOrigin1, false);
  ASSERT_EQ(4, GetTotalChangeCount());

  ASSERT_TRUE(NextOriginToProcess(&origin));
  ASSERT_EQ(kOrigin3, origin);

  // Re-enable kOrigin2.
  SetOriginEnabled(kOrigin2, true);
  ASSERT_EQ(2 + 4, GetTotalChangeCount());

  // kOrigin1 won't return this time.
  all_origins.insert(kOrigin2);
  all_origins.insert(kOrigin3);
  while (!all_origins.empty()) {
    ASSERT_TRUE(NextOriginToProcess(&origin));
    ASSERT_TRUE(base::Contains(all_origins, origin));
    all_origins.erase(origin);
  }
}

}  // namespace sync_file_system
