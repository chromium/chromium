// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/local/syncable_file_operation_runner.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/sync_file_system/local/canned_syncable_file_system.h"
#include "chrome/browser/sync_file_system/local/local_file_change_tracker.h"
#include "chrome/browser/sync_file_system/local/local_file_sync_context.h"
#include "chrome/browser/sync_file_system/local/local_file_sync_status.h"
#include "chrome/browser/sync_file_system/local/sync_file_system_backend.h"
#include "chrome/browser/sync_file_system/local/syncable_file_system_operation.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/test/mock_blob_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/leveldb_chrome.h"

using base::File;
using storage::FileSystemOperation;
using storage::FileSystemURL;
using storage::ScopedTextBlob;

namespace sync_file_system {

namespace {
const std::string kParent = "foo";
const std::string kFile   = "foo/file";
const std::string kDir    = "foo/dir";
const std::string kChild  = "foo/dir/bar";
const std::string kOther  = "bar";
}  // namespace

class SyncableFileOperationRunnerTest : public testing::Test {
 protected:
  typedef FileSystemOperation::StatusCallback StatusCallback;

  // Use the current thread as IO thread so that we can directly call
  // operations in the tests.
  SyncableFileOperationRunnerTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP),
        in_memory_env_(
            leveldb_chrome::NewMemEnv("SyncableFileOperationRunnerTest")),
        file_system_(GURL("http://example.com"),
                     in_memory_env_.get(),
                     base::ThreadTaskRunnerHandle::Get().get(),
                     base::ThreadTaskRunnerHandle::Get().get()),
        callback_count_(0),
        write_status_(File::FILE_ERROR_FAILED),
        write_bytes_(0),
        write_complete_(false) {}

  void SetUp() override {
    ASSERT_TRUE(dir_.CreateUniqueTempDir());

    file_system_.SetUp(CannedSyncableFileSystem::QUOTA_ENABLED);
    sync_context_ =
        new LocalFileSyncContext(dir_.GetPath(), in_memory_env_.get(),
                                 base::ThreadTaskRunnerHandle::Get().get(),
                                 base::ThreadTaskRunnerHandle::Get().get());
    ASSERT_EQ(
        SYNC_STATUS_OK,
        file_system_.MaybeInitializeFileSystemContext(sync_context_.get()));

    ASSERT_EQ(File::FILE_OK, file_system_.OpenFileSystem());
    ASSERT_EQ(File::FILE_OK,
              file_system_.CreateDirectory(URL(kParent)));
  }

  void TearDown() override {
    if (sync_context_.get())
      sync_context_->ShutdownOnUIThread();
    sync_context_ = nullptr;

    file_system_.TearDown();
    RevokeSyncableFileSystem();
  }

  FileSystemURL URL(const std::string& path) {
    return file_system_.URL(path);
  }

  LocalFileSyncStatus* sync_status() {
    return file_system_.backend()->sync_context()->sync_status();
  }

  void ResetCallbackStatus() {
    write_status_ = File::FILE_ERROR_FAILED;
    write_bytes_ = 0;
    write_complete_ = false;
    callback_count_ = 0;
  }

  StatusCallback ExpectStatus(const base::Location& location,
                              File::Error expect) {
    return base::Bind(&SyncableFileOperationRunnerTest::DidFinish,
                      weak_factory_.GetWeakPtr(), location, expect);
  }

  FileSystemOperation::WriteCallback GetWriteCallback(
      const base::Location& location) {
    return base::Bind(&SyncableFileOperationRunnerTest::DidWrite,
                      weak_factory_.GetWeakPtr(), location);
  }

  void DidWrite(const base::Location& location,
                File::Error status,
                int64_t bytes,
                bool complete) {
    SCOPED_TRACE(testing::Message() << location.ToString());
    write_status_ = status;
    write_bytes_ += bytes;
    write_complete_ = complete;
    ++callback_count_;
  }

  void DidFinish(const base::Location& location,
                 File::Error expect,
                 File::Error status) {
    SCOPED_TRACE(testing::Message() << location.ToString());
    EXPECT_EQ(expect, status);
    ++callback_count_;
  }

  bool CreateTempFile(base::FilePath* path) {
    return base::CreateTemporaryFileInDir(dir_.GetPath(), path);
  }

  content::BrowserTaskEnvironment task_environment_;

  base::ScopedTempDir dir_;
  std::unique_ptr<leveldb::Env> in_memory_env_;

  CannedSyncableFileSystem file_system_;
  scoped_refptr<LocalFileSyncContext> sync_context_;

  int callback_count_;
  File::Error write_status_;
  size_t write_bytes_;
  bool write_complete_;

  storage::BlobStorageContext blob_storage_context_;

 private:
  base::WeakPtrFactory<SyncableFileOperationRunnerTest> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SyncableFileOperationRunnerTest);
};

TEST_F(SyncableFileOperationRunnerTest, SimpleQueue) {
  sync_status()->StartSyncing(URL(kFile));
  ASSERT_FALSE(sync_status()->IsWritable(URL(kFile)));

  // The URL is in syncing so the write operations won't run.
  ResetCallbackStatus();
  file_system_.operation_runner()->CreateFile(
      URL(kFile), false /* exclusive */,
      ExpectStatus(FROM_HERE, File::FILE_OK));
  file_system_.operation_runner()->Truncate(
      URL(kFile), 1,
      ExpectStatus(FROM_HERE, File::FILE_OK));
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(0, callback_count_);

  // Read operations are not blocked (and are executed before queued ones).
  file_system_.operation_runner()->FileExists(
      URL(kFile), ExpectStatus(FROM_HERE, File::FILE_ERROR_NOT_FOUND));
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(1, callback_count_);

  // End syncing (to enable write).
  sync_status()->EndSyncing(URL(kFile));
  ASSERT_TRUE(sync_status()->IsWritable(URL(kFile)));

  ResetCallbackStatus();
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(2, callback_count_);

  // Now the file must have been created and updated.
  ResetCallbackStatus();
  file_system_.operation_runner()->FileExists(
      URL(kFile), ExpectStatus(FROM_HERE, File::FILE_OK));
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(1, callback_count_);
}

TEST_F(SyncableFileOperationRunnerTest, WriteToParentAndChild) {
  // First create the kDir directory and kChild in the dir.
  EXPECT_EQ(File::FILE_OK, file_system_.CreateDirectory(URL(kDir)));
  EXPECT_EQ(File::FILE_OK, file_system_.CreateFile(URL(kChild)));

  // Start syncing the kDir directory.
  sync_status()->StartSyncing(URL(kDir));
  ASSERT_FALSE(sync_status()->IsWritable(URL(kDir)));

  // Writes to kParent and kChild should be all queued up.
  ResetCallbackStatus();
  file_system_.operation_runner()->Truncate(
      URL(kChild), 1, ExpectStatus(FROM_HERE, File::FILE_OK));
  file_system_.operation_runner()->Remove(
      URL(kParent), true /* recursive */,
      ExpectStatus(FROM_HERE, File::FILE_OK));
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(0, callback_count_);

  // Read operations are not blocked (and are executed before queued ones).
  file_system_.operation_runner()->DirectoryExists(
      URL(kDir), ExpectStatus(FROM_HERE, File::FILE_OK));
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(1, callback_count_);

  // Writes to unrelated files must succeed as well.
  ResetCallbackStatus();
  file_system_.operation_runner()->CreateDirectory(
      URL(kOther), false /* exclusive */, false /* recursive */,
      ExpectStatus(FROM_HERE, File::FILE_OK));
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(1, callback_count_);

  // End syncing (to enable write).
  sync_status()->EndSyncing(URL(kDir));
  ASSERT_TRUE(sync_status()->IsWritable(URL(kDir)));

  ResetCallbackStatus();
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(2, callback_count_);
}

TEST_F(SyncableFileOperationRunnerTest, CopyAndMove) {
  // First create the kDir directory and kChild in the dir.
  EXPECT_EQ(File::FILE_OK, file_system_.CreateDirectory(URL(kDir)));
  EXPECT_EQ(File::FILE_OK, file_system_.CreateFile(URL(kChild)));

  // Start syncing the kParent directory.
  sync_status()->StartSyncing(URL(kParent));

  // Copying kDir to other directory should succeed, while moving would fail
  // (since the source directory is in syncing).
  ResetCallbackStatus();
  file_system_.operation_runner()->Copy(
      URL(kDir), URL("dest-copy"), storage::FileSystemOperation::OPTION_NONE,
      storage::FileSystemOperation::ERROR_BEHAVIOR_ABORT,
      storage::FileSystemOperationRunner::CopyProgressCallback(),
      ExpectStatus(FROM_HERE, File::FILE_OK));
  file_system_.operation_runner()->Move(
      URL(kDir),
      URL("dest-move"),
      storage::FileSystemOperation::OPTION_NONE,
      ExpectStatus(FROM_HERE, File::FILE_OK));
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(1, callback_count_);

  // Only "dest-copy1" should exist.
  EXPECT_EQ(File::FILE_OK,
            file_system_.DirectoryExists(URL("dest-copy")));
  EXPECT_EQ(File::FILE_ERROR_NOT_FOUND,
            file_system_.DirectoryExists(URL("dest-move")));

  // Start syncing the "dest-copy2" directory.
  sync_status()->StartSyncing(URL("dest-copy2"));

  // Now the destination is also locked copying kDir should be queued.
  ResetCallbackStatus();
  file_system_.operation_runner()->Copy(
      URL(kDir), URL("dest-copy2"), storage::FileSystemOperation::OPTION_NONE,
      storage::FileSystemOperation::ERROR_BEHAVIOR_ABORT,
      storage::FileSystemOperationRunner::CopyProgressCallback(),
      ExpectStatus(FROM_HERE, File::FILE_OK));
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(0, callback_count_);

  // Finish syncing the "dest-copy2" directory to unlock Copy.
  sync_status()->EndSyncing(URL("dest-copy2"));
  ResetCallbackStatus();
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(1, callback_count_);

  // Now we should have "dest-copy2".
  EXPECT_EQ(File::FILE_OK,
            file_system_.DirectoryExists(URL("dest-copy2")));

  // Finish syncing the kParent to unlock Move.
  sync_status()->EndSyncing(URL(kParent));
  ResetCallbackStatus();
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(1, callback_count_);

  // Now we should have "dest-move".
  EXPECT_EQ(File::FILE_OK,
            file_system_.DirectoryExists(URL("dest-move")));
}

TEST_F(SyncableFileOperationRunnerTest, Write) {
  EXPECT_EQ(File::FILE_OK, file_system_.CreateFile(URL(kFile)));
  const std::string kData("Lorem ipsum.");
  ScopedTextBlob blob(&blob_storage_context_, "blob:foo", kData);

  sync_status()->StartSyncing(URL(kFile));

  ResetCallbackStatus();
  file_system_.operation_runner()->Write(
      URL(kFile), blob.GetBlobDataHandle(), 0, GetWriteCallback(FROM_HERE));
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(0, callback_count_);

  sync_status()->EndSyncing(URL(kFile));
  ResetCallbackStatus();

  while (!write_complete_)
    content::RunAllTasksUntilIdle();

  EXPECT_EQ(File::FILE_OK, write_status_);
  EXPECT_EQ(kData.size(), write_bytes_);
  EXPECT_TRUE(write_complete_);
}

TEST_F(SyncableFileOperationRunnerTest, QueueAndCancel) {
  sync_status()->StartSyncing(URL(kFile));
  ASSERT_FALSE(sync_status()->IsWritable(URL(kFile)));

  ResetCallbackStatus();
  file_system_.operation_runner()->CreateFile(
      URL(kFile), false /* exclusive */,
      ExpectStatus(FROM_HERE, File::FILE_ERROR_ABORT));
  file_system_.operation_runner()->Truncate(
      URL(kFile), 1,
      ExpectStatus(FROM_HERE, File::FILE_ERROR_ABORT));
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(0, callback_count_);

  ResetCallbackStatus();

  // This shouldn't crash nor leak memory.
  sync_context_->ShutdownOnUIThread();
  sync_context_ = nullptr;
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(2, callback_count_);
}

// Test if CopyInForeignFile runs cooperatively with other Sync operations.
TEST_F(SyncableFileOperationRunnerTest, CopyInForeignFile) {
  const std::string kTestData("test data");

  base::FilePath temp_path;
  ASSERT_TRUE(CreateTempFile(&temp_path));
  ASSERT_EQ(static_cast<int>(kTestData.size()),
            base::WriteFile(
                temp_path, kTestData.data(), kTestData.size()));

  sync_status()->StartSyncing(URL(kFile));
  ASSERT_FALSE(sync_status()->IsWritable(URL(kFile)));

  // The URL is in syncing so CopyIn (which is a write operation) won't run.
  ResetCallbackStatus();
  file_system_.operation_runner()->CopyInForeignFile(
      temp_path, URL(kFile),
      ExpectStatus(FROM_HERE, File::FILE_OK));
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(0, callback_count_);

  // End syncing (to enable write).
  sync_status()->EndSyncing(URL(kFile));
  ASSERT_TRUE(sync_status()->IsWritable(URL(kFile)));

  ResetCallbackStatus();
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(1, callback_count_);

  // Now the file must have been created and have the same content as temp_path.
  // TODO(mek): AdaptCallbackForRepeating is needed here because
  // CannedSyncableFileSystem hasn't switched to OnceCallback yet.
  ResetCallbackStatus();
  file_system_.DoVerifyFile(
      URL(kFile), kTestData,
      base::AdaptCallbackForRepeating(ExpectStatus(FROM_HERE, File::FILE_OK)));
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(1, callback_count_);
}

TEST_F(SyncableFileOperationRunnerTest, Cancel) {
  // Prepare a file.
  file_system_.operation_runner()->CreateFile(
      URL(kFile), false /* exclusive */,
      ExpectStatus(FROM_HERE, File::FILE_OK));
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(1, callback_count_);

  // Run Truncate and immediately cancel. This shouldn't crash.
  ResetCallbackStatus();
  storage::FileSystemOperationRunner::OperationID id =
      file_system_.operation_runner()->Truncate(
          URL(kFile), 10, ExpectStatus(FROM_HERE, File::FILE_OK));
  file_system_.operation_runner()->Cancel(
      id, ExpectStatus(FROM_HERE, File::FILE_ERROR_INVALID_OPERATION));
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(2, callback_count_);
}

}  // namespace sync_file_system
