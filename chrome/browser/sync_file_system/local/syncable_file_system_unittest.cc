// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/containers/contains.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/test_future.h"
#include "chrome/browser/sync_file_system/local/canned_syncable_file_system.h"
#include "chrome/browser/sync_file_system/local/local_file_change_tracker.h"
#include "chrome/browser/sync_file_system/local/local_file_sync_context.h"
#include "chrome/browser/sync_file_system/local/sync_file_system_backend.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_context.h"
#include "storage/browser/file_system/file_system_quota_client.h"
#include "storage/browser/file_system/isolated_context.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/test/async_file_test_helper.h"
#include "storage/browser/test/sandbox_file_system_test_helper.h"
#include "storage/browser/test/test_file_system_context.h"
#include "storage/common/file_system/file_system_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "third_party/leveldatabase/leveldb_chrome.h"

using storage::FileSystemContext;
using storage::FileSystemOperationContext;
using storage::FileSystemURL;
using storage::FileSystemURLSet;
using storage::QuotaManager;
using storage::SandboxFileSystemTestHelper;

namespace sync_file_system {

class SyncableFileSystemTest : public testing::Test {
 public:
  SyncableFileSystemTest()
      : in_memory_env_(leveldb_chrome::NewMemEnv("SyncableFileSystemTest")),
        file_system_(GURL("http://example.com/"),
                     in_memory_env_.get(),
                     base::SingleThreadTaskRunner::GetCurrentDefault().get(),
                     base::SingleThreadTaskRunner::GetCurrentDefault().get()) {}

  SyncableFileSystemTest(const SyncableFileSystemTest&) = delete;
  SyncableFileSystemTest& operator=(const SyncableFileSystemTest&) = delete;

  void SetUp() override {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    file_system_.SetUp();
    quota_client_ = std::make_unique<storage::FileSystemQuotaClient>(
        file_system_.file_system_context());

    sync_context_ = new LocalFileSyncContext(
        data_dir_.GetPath(), in_memory_env_.get(),
        base::SingleThreadTaskRunner::GetCurrentDefault().get(),
        base::SingleThreadTaskRunner::GetCurrentDefault().get());
    ASSERT_EQ(
        sync_file_system::SYNC_STATUS_OK,
        file_system_.MaybeInitializeFileSystemContext(sync_context_.get()));
  }

  void TearDown() override {
    if (sync_context_.get())
      sync_context_->ShutdownOnUIThread();
    sync_context_ = nullptr;

    quota_client_.reset();
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

  storage::QuotaErrorOr<storage::BucketLocator> GetOrCreateBucket(
      const std::string& name) {
    base::test::TestFuture<storage::QuotaErrorOr<storage::BucketInfo>> future;
    file_system_.quota_manager()->UpdateOrCreateBucket({storage_key(), name},
                                                       future.GetCallback());
    return future.Take().transform(&storage::BucketInfo::ToBucketLocator);
  }

  int64_t GetBucketUsage(const storage::BucketLocator& bucket) {
    base::test::TestFuture<int64_t> future;
    quota_client_->GetBucketUsage(bucket, future.GetCallback());
    return future.Get();
  }

  void DeleteBucketData(const storage::BucketLocator& bucket) {
    base::test::TestFuture<blink::mojom::QuotaStatusCode> future;
    quota_client_->DeleteBucketData(bucket, future.GetCallback());
    ASSERT_EQ(future.Get(), blink::mojom::QuotaStatusCode::kOk);
  }

  void DeleteFileSystem(storage::FileSystemType fs_type) {
    base::test::TestFuture<base::File::Error> future;
    file_system_context()->DeleteFileSystem(storage_key(), fs_type,
                                            future.GetCallback());
    ASSERT_EQ(future.Get(), base::File::Error::FILE_OK);
  }

  blink::StorageKey storage_key() {
    return blink::StorageKey::CreateFirstParty(
        url::Origin::Create(file_system_.origin()));
  }

  FileSystemURL URL(storage::FileSystemType fs_type, const std::string& path) {
    return file_system_context()->CreateCrackedFileSystemURL(
        storage_key(), fs_type, base::FilePath().AppendASCII(path));
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
  std::unique_ptr<storage::FileSystemQuotaClient> quota_client_;
  CannedSyncableFileSystem file_system_;

 private:
  scoped_refptr<LocalFileSyncContext> sync_context_;

  base::WeakPtrFactory<SyncableFileSystemTest> weak_factory_{this};
};

// Brief combined testing. Just see if all the sandbox feature works.
TEST_F(SyncableFileSystemTest, SyncableLocalSandboxCombined) {
  // Opens a syncable file system.
  EXPECT_EQ(base::File::FILE_OK, file_system_.OpenFileSystem());

  // Create files for kFileSystemTypeSyncable.
  storage::AsyncFileTestHelper::CreateDirectory(
      file_system_context(), URL(storage::kFileSystemTypeSyncable, "dir"));
  FileSystemURL sync_url = URL(storage::kFileSystemTypeSyncable, "dir/foo");
  storage::AsyncFileTestHelper::CreateFile(file_system_context(), sync_url);

  // Create files for kFileSystemTypeTemporary.
  storage::AsyncFileTestHelper::CreateDirectory(
      file_system_context(), URL(storage::kFileSystemTypeTemporary, "dir"));
  FileSystemURL temp_url = URL(storage::kFileSystemTypeTemporary, "dir/foo");
  storage::AsyncFileTestHelper::CreateFile(file_system_context(), temp_url);

  const int64_t kDefaultQuota = 12345 * 1024;
  file_system_.quota_manager()->SetQuota(storage_key(), kDefaultQuota);

  ASSERT_OK_AND_ASSIGN(auto bucket,
                       GetOrCreateBucket(storage::kDefaultBucketName));

  // Changes in the syncable files should be reflected in the temporary bucket.
  int64_t usage = GetBucketUsage(bucket);
  EXPECT_GT(usage, 0);

  // Truncating existing syncable file should update usage for the temporary
  // bucket.
  const int64_t kFileSizeToExtend = 333;
  storage::AsyncFileTestHelper::TruncateFile(file_system_context(), sync_url,
                                             kFileSizeToExtend);
  int64_t new_usage = GetBucketUsage(bucket);
  EXPECT_EQ(kFileSizeToExtend, new_usage - usage);

  // Shrinking the quota to the current usage, should make extending the file
  // further fail.
  file_system_.quota_manager()->SetQuota(storage_key(), new_usage);

  EXPECT_EQ(base::File::FILE_ERROR_NO_SPACE,
            storage::AsyncFileTestHelper::TruncateFile(
                file_system_context(), sync_url, kFileSizeToExtend + 1));
  usage = new_usage;
  new_usage = GetBucketUsage(bucket);
  EXPECT_EQ(new_usage, usage);

  // Must delete both file types to delete all temporary bucket storage.
  DeleteFileSystem(storage::kFileSystemTypeTemporary);
  EXPECT_GT(GetBucketUsage(bucket), 0);
  DeleteFileSystem(storage::kFileSystemTypeSyncable);
  EXPECT_EQ(GetBucketUsage(bucket), 0);

  // Restore the system default quota.
  file_system_.quota_manager()->SetQuota(storage_key(), kDefaultQuota);
}

TEST_F(SyncableFileSystemTest, BucketDeletion) {
  EXPECT_EQ(base::File::FILE_OK, file_system_.OpenFileSystem());

  // Create files for kFileSystemTypeSyncable.
  storage::AsyncFileTestHelper::CreateDirectory(
      file_system_context(), URL(storage::kFileSystemTypeSyncable, "dir"));
  FileSystemURL sync_url = URL(storage::kFileSystemTypeSyncable, "dir/foo");
  storage::AsyncFileTestHelper::CreateFile(file_system_context(), sync_url);

  // Create files for kFileSystemTypeTemporary.
  storage::AsyncFileTestHelper::CreateDirectory(
      file_system_context(), URL(storage::kFileSystemTypeTemporary, "dir"));
  FileSystemURL temp_url = URL(storage::kFileSystemTypeTemporary, "dir/foo");
  storage::AsyncFileTestHelper::CreateFile(file_system_context(), temp_url);

  ASSERT_OK_AND_ASSIGN(auto bucket,
                       GetOrCreateBucket(storage::kDefaultBucketName));

  EXPECT_GT(GetBucketUsage(bucket), 0);

  // Deleting the temporary bucket is enough to delete files for both
  // kFileSystemTypeTemporary and kFileSystemTypeSyncable.
  DeleteBucketData(bucket);
  EXPECT_FALSE(storage::AsyncFileTestHelper::FileExists(
      file_system_context(), temp_url,
      storage::AsyncFileTestHelper::kDontCheckSize));
  EXPECT_FALSE(storage::AsyncFileTestHelper::FileExists(
      file_system_context(), sync_url,
      storage::AsyncFileTestHelper::kDontCheckSize));
}

// Combined testing with LocalFileChangeTracker.
TEST_F(SyncableFileSystemTest, ChangeTrackerSimple) {
  EXPECT_EQ(base::File::FILE_OK, file_system_.OpenFileSystem());

  const auto path0 = URL(storage::kFileSystemTypeSyncable, "dir a");
  const auto path1 = URL(storage::kFileSystemTypeSyncable, "dir a/dir");
  const auto path2 = URL(storage::kFileSystemTypeSyncable, "dir a/file");
  const auto path3 = URL(storage::kFileSystemTypeSyncable, "dir b");

  EXPECT_EQ(base::File::FILE_OK, file_system_.CreateDirectory(path0));
  EXPECT_EQ(base::File::FILE_OK, file_system_.CreateDirectory(path1));
  EXPECT_EQ(base::File::FILE_OK, file_system_.CreateFile(path2));
  EXPECT_EQ(base::File::FILE_OK, file_system_.TruncateFile(path2, 1));
  EXPECT_EQ(base::File::FILE_OK, file_system_.TruncateFile(path2, 2));

  FileSystemURLSet urls;
  file_system_.GetChangedURLsInTracker(&urls);

  EXPECT_EQ(3U, urls.size());
  EXPECT_TRUE(base::Contains(urls, path0));
  EXPECT_TRUE(base::Contains(urls, path1));
  EXPECT_TRUE(base::Contains(urls, path2));

  VerifyAndClearChange(path0,
                       FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                                  sync_file_system::SYNC_FILE_TYPE_DIRECTORY));
  VerifyAndClearChange(path1,
                       FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                                  sync_file_system::SYNC_FILE_TYPE_DIRECTORY));
  VerifyAndClearChange(path2,
                       FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                                  sync_file_system::SYNC_FILE_TYPE_FILE));

  // Creates and removes the same directory.
  EXPECT_EQ(base::File::FILE_OK, file_system_.CreateDirectory(path3));
  EXPECT_EQ(base::File::FILE_OK, file_system_.Remove(path3,
                                                     /*recursive=*/false));

  // The changes will be offset.
  urls.clear();
  file_system_.GetChangedURLsInTracker(&urls);
  EXPECT_TRUE(urls.empty());

  // Recursively removes the `path0` directory.
  EXPECT_EQ(base::File::FILE_OK,
            file_system_.Remove(path0, /*recursive=*/true));

  urls.clear();
  file_system_.GetChangedURLsInTracker(&urls);

  // `path0` and its children (`path1` and `path2`) should be deleted.
  EXPECT_EQ(3U, urls.size());
  EXPECT_TRUE(base::Contains(urls, path0));
  EXPECT_TRUE(base::Contains(urls, path1));
  EXPECT_TRUE(base::Contains(urls, path2));

  VerifyAndClearChange(path0,
                       FileChange(FileChange::FILE_CHANGE_DELETE,
                                  sync_file_system::SYNC_FILE_TYPE_DIRECTORY));
  VerifyAndClearChange(path1,
                       FileChange(FileChange::FILE_CHANGE_DELETE,
                                  sync_file_system::SYNC_FILE_TYPE_DIRECTORY));
  VerifyAndClearChange(path2,
                       FileChange(FileChange::FILE_CHANGE_DELETE,
                                  sync_file_system::SYNC_FILE_TYPE_FILE));
}

}  // namespace sync_file_system
