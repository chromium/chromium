// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/macros.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/sync_file_system/local/canned_syncable_file_system.h"
#include "chrome/browser/sync_file_system/local/local_file_change_tracker.h"
#include "chrome/browser/sync_file_system/local/local_file_sync_context.h"
#include "chrome/browser/sync_file_system/local/sync_file_system_backend.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_context.h"
#include "storage/browser/file_system/isolated_context.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/test/async_file_test_helper.h"
#include "storage/browser/test/sandbox_file_system_test_helper.h"
#include "storage/common/file_system/file_system_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/leveldb_chrome.h"

using content::SandboxFileSystemTestHelper;
using storage::FileSystemContext;
using storage::FileSystemOperationContext;
using storage::FileSystemURL;
using storage::FileSystemURLSet;
using storage::QuotaManager;

namespace sync_file_system {

class SyncableFileSystemTest : public testing::Test {
 public:
  SyncableFileSystemTest()
      : in_memory_env_(leveldb_chrome::NewMemEnv("SyncableFileSystemTest")),
        file_system_(GURL("http://example.com/"),
                     in_memory_env_.get(),
                     base::ThreadTaskRunnerHandle::Get().get(),
                     base::ThreadTaskRunnerHandle::Get().get()) {}

  void SetUp() override {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    file_system_.SetUp(CannedSyncableFileSystem::QUOTA_ENABLED);

    sync_context_ =
        new LocalFileSyncContext(data_dir_.GetPath(), in_memory_env_.get(),
                                 base::ThreadTaskRunnerHandle::Get().get(),
                                 base::ThreadTaskRunnerHandle::Get().get());
    ASSERT_EQ(
        sync_file_system::SYNC_STATUS_OK,
        file_system_.MaybeInitializeFileSystemContext(sync_context_.get()));
  }

  void TearDown() override {
    if (sync_context_.get())
      sync_context_->ShutdownOnUIThread();
    sync_context_ = nullptr;

    file_system_.TearDown();

    // Make sure we don't leave the external filesystem.
    // (CannedSyncableFileSystem::TearDown does not do this as there may be
    // multiple syncable file systems registered for the name)
    RevokeSyncableFileSystem();
  }

 protected:
  void VerifyAndClearChange(const FileSystemURL& url,
                            const FileChange& expected_change) {
    SCOPED_TRACE(testing::Message() << url.DebugString() <<
                 " expecting:" << expected_change.DebugString());
    // Get the changes for URL and verify.
    FileChangeList changes;
    change_tracker()->GetChangesForURL(url, &changes);
    ASSERT_EQ(1U, changes.size());
    SCOPED_TRACE(testing::Message() << url.DebugString() <<
                 " actual:" << changes.DebugString());
    EXPECT_EQ(expected_change, changes.front());

    // Clear the URL from the change tracker.
    change_tracker()->ClearChangesForURL(url);
  }

  FileSystemURL URL(const std::string& path) {
    return file_system_.URL(path);
  }

  FileSystemContext* file_system_context() {
    return file_system_.file_system_context();
  }

  LocalFileChangeTracker* change_tracker() {
    return file_system_.backend()->change_tracker();
  }

  base::ScopedTempDir data_dir_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<leveldb::Env> in_memory_env_;
  CannedSyncableFileSystem file_system_;

 private:
  scoped_refptr<LocalFileSyncContext> sync_context_;

  base::WeakPtrFactory<SyncableFileSystemTest> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SyncableFileSystemTest);
};

// Brief combined testing. Just see if all the sandbox feature works.
TEST_F(SyncableFileSystemTest, SyncableLocalSandboxCombined) {
  // Opens a syncable file system.
  EXPECT_EQ(base::File::FILE_OK,
            file_system_.OpenFileSystem());

  // Do some operations.
  EXPECT_EQ(base::File::FILE_OK,
            file_system_.CreateDirectory(URL("dir")));
  EXPECT_EQ(base::File::FILE_OK,
            file_system_.CreateFile(URL("dir/foo")));

  const int64_t kOriginalQuota = QuotaManager::kSyncableStorageDefaultHostQuota;

  const int64_t kQuota = 12345 * 1024;
  QuotaManager::kSyncableStorageDefaultHostQuota = kQuota;
  int64_t usage, quota;
  EXPECT_EQ(blink::mojom::QuotaStatusCode::kOk,
            file_system_.GetUsageAndQuota(&usage, &quota));

  // Returned quota must be what we overrode. Usage must be greater than 0
  // as creating a file or directory consumes some space.
  EXPECT_EQ(kQuota, quota);
  EXPECT_GT(usage, 0);

  // Truncate to extend an existing file and see if the usage reflects it.
  const int64_t kFileSizeToExtend = 333;
  EXPECT_EQ(base::File::FILE_OK,
            file_system_.CreateFile(URL("dir/foo")));

  EXPECT_EQ(base::File::FILE_OK,
            file_system_.TruncateFile(URL("dir/foo"), kFileSizeToExtend));

  int64_t new_usage;
  EXPECT_EQ(blink::mojom::QuotaStatusCode::kOk,
            file_system_.GetUsageAndQuota(&new_usage, &quota));
  EXPECT_EQ(kFileSizeToExtend, new_usage - usage);

  // Shrink the quota to the current usage, try to extend the file further
  // and see if it fails.
  QuotaManager::kSyncableStorageDefaultHostQuota = new_usage;
  EXPECT_EQ(base::File::FILE_ERROR_NO_SPACE,
            file_system_.TruncateFile(URL("dir/foo"), kFileSizeToExtend + 1));

  usage = new_usage;
  EXPECT_EQ(blink::mojom::QuotaStatusCode::kOk,
            file_system_.GetUsageAndQuota(&new_usage, &quota));
  EXPECT_EQ(usage, new_usage);

  // Deletes the file system.
  EXPECT_EQ(base::File::FILE_OK,
            file_system_.DeleteFileSystem());

  // Now the usage must be zero.
  EXPECT_EQ(blink::mojom::QuotaStatusCode::kOk,
            file_system_.GetUsageAndQuota(&usage, &quota));
  EXPECT_EQ(0, usage);

  // Restore the system default quota.
  QuotaManager::kSyncableStorageDefaultHostQuota = kOriginalQuota;
}

// Combined testing with LocalFileChangeTracker.
TEST_F(SyncableFileSystemTest, ChangeTrackerSimple) {
  EXPECT_EQ(base::File::FILE_OK,
            file_system_.OpenFileSystem());

  const char kPath0[] = "dir a";
  const char kPath1[] = "dir a/dir";   // child of kPath0
  const char kPath2[] = "dir a/file";  // child of kPath0
  const char kPath3[] = "dir b";

  // Do some operations.
  EXPECT_EQ(base::File::FILE_OK,
            file_system_.CreateDirectory(URL(kPath0)));  // Creates a dir.
  EXPECT_EQ(base::File::FILE_OK,
            file_system_.CreateDirectory(URL(kPath1)));  // Creates another.
  EXPECT_EQ(base::File::FILE_OK,
            file_system_.CreateFile(URL(kPath2)));       // Creates a file.
  EXPECT_EQ(base::File::FILE_OK,
            file_system_.TruncateFile(URL(kPath2), 1));  // Modifies the file.
  EXPECT_EQ(base::File::FILE_OK,
            file_system_.TruncateFile(URL(kPath2), 2));  // Modifies it again.

  FileSystemURLSet urls;
  file_system_.GetChangedURLsInTracker(&urls);

  EXPECT_EQ(3U, urls.size());
  EXPECT_TRUE(base::Contains(urls, URL(kPath0)));
  EXPECT_TRUE(base::Contains(urls, URL(kPath1)));
  EXPECT_TRUE(base::Contains(urls, URL(kPath2)));

  VerifyAndClearChange(URL(kPath0),
                       FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                                  sync_file_system::SYNC_FILE_TYPE_DIRECTORY));
  VerifyAndClearChange(URL(kPath1),
                       FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                                  sync_file_system::SYNC_FILE_TYPE_DIRECTORY));
  VerifyAndClearChange(URL(kPath2),
                       FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                                  sync_file_system::SYNC_FILE_TYPE_FILE));

  // Creates and removes a same directory.
  EXPECT_EQ(base::File::FILE_OK,
            file_system_.CreateDirectory(URL(kPath3)));
  EXPECT_EQ(base::File::FILE_OK,
            file_system_.Remove(URL(kPath3), false /* recursive */));

  // The changes will be offset.
  urls.clear();
  file_system_.GetChangedURLsInTracker(&urls);
  EXPECT_TRUE(urls.empty());

  // Recursively removes the kPath0 directory.
  EXPECT_EQ(base::File::FILE_OK,
            file_system_.Remove(URL(kPath0), true /* recursive */));

  urls.clear();
  file_system_.GetChangedURLsInTracker(&urls);

  // kPath0 and its all chidren (kPath1 and kPath2) must have been deleted.
  EXPECT_EQ(3U, urls.size());
  EXPECT_TRUE(base::Contains(urls, URL(kPath0)));
  EXPECT_TRUE(base::Contains(urls, URL(kPath1)));
  EXPECT_TRUE(base::Contains(urls, URL(kPath2)));

  VerifyAndClearChange(URL(kPath0),
                       FileChange(FileChange::FILE_CHANGE_DELETE,
                                  sync_file_system::SYNC_FILE_TYPE_DIRECTORY));
  VerifyAndClearChange(URL(kPath1),
                       FileChange(FileChange::FILE_CHANGE_DELETE,
                                  sync_file_system::SYNC_FILE_TYPE_DIRECTORY));
  VerifyAndClearChange(URL(kPath2),
                       FileChange(FileChange::FILE_CHANGE_DELETE,
                                  sync_file_system::SYNC_FILE_TYPE_FILE));
}

}  // namespace sync_file_system
