// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/content_cache/content_cache_impl.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/rand_util.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/file_system_provider/content_cache/cache_file_context.h"
#include "chrome/browser/ash/file_system_provider/content_cache/context_database.h"
#include "chrome/browser/ash/file_system_provider/opened_cloud_file.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_interface.h"
#include "net/base/io_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::file_system_provider {
namespace {

using base::test::TestFuture;
using testing::ElementsAre;
using testing::Key;

// The default chunk size that is requested via the `FileStreamReader`.
constexpr int kDefaultChunkSize = 512;

}  // namespace

class FileSystemProviderContentCacheImplTest : public testing::Test {
 protected:
  FileSystemProviderContentCacheImplTest() = default;
  ~FileSystemProviderContentCacheImplTest() override = default;

  void SetUp() override {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());

    // Initialize a `ContextDatabase` in memory on a blocking task runner.
    std::unique_ptr<ContextDatabase> context_db =
        std::make_unique<ContextDatabase>(base::FilePath());
    context_db_ = context_db->GetWeakPtr();
    db_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
    BoundContextDatabase db(db_task_runner_, std::move(context_db));
    TestFuture<bool> future;
    db.AsyncCall(&ContextDatabase::Initialize).Then(future.GetCallback());
    EXPECT_TRUE(future.Get());

    content_cache_ = std::make_unique<ContentCacheImpl>(
        temp_dir_.GetPath(), std::move(db), /*max_cache_size=*/500);
  }

  void TearDown() override { content_cache_.reset(); }

  scoped_refptr<net::IOBufferWithSize> InitializeBufferWithRandBytes(int size) {
    scoped_refptr<net::IOBufferWithSize> buffer =
        base::MakeRefCounted<net::IOBufferWithSize>(size);
    std::vector<uint8_t> rand_bytes = base::RandBytesAsVector(size);
    for (int i = 0; i < size; i++) {
      buffer->data()[i] = rand_bytes[i];
    }
    return buffer;
  }

  void WriteFileToCache(const base::FilePath& path,
                        const std::string& version_tag) {
    OpenedCloudFile file(path, OpenFileMode::OPEN_FILE_MODE_READ, version_tag);

    // Create a buffer and ensure there is data in the buffer before writing it
    // to disk.
    scoped_refptr<net::IOBufferWithSize> buffer =
        InitializeBufferWithRandBytes(kDefaultChunkSize);
    TestFuture<base::File::Error> future;
    EXPECT_TRUE(content_cache_->StartWriteBytes(file, buffer.get(),
                                                /*offset=*/0, kDefaultChunkSize,
                                                future.GetCallback()));
    EXPECT_EQ(future.Get(), base::File::FILE_OK);
  }

  int64_t AddItemToDb(const base::FilePath& fsp_path,
                      const std::string& version_tag,
                      base::Time accessed_time) {
    int64_t inserted_id = -1;
    TestFuture<bool> db_add_item_future;
    db_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          return context_db_->AddItem(fsp_path, version_tag, accessed_time,
                                      &inserted_id);
        }),
        db_add_item_future.GetCallback());
    EXPECT_TRUE(db_add_item_future.Get());
    EXPECT_GE(inserted_id, 1);
    return inserted_id;
  }

  ContextDatabase::Item GetItemById(int64_t item_id) {
    ContextDatabase::Item item;
    TestFuture<bool> db_get_item_future;
    db_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          return context_db_->GetItemById(item_id, item);
        }),
        db_get_item_future.GetCallback());
    EXPECT_TRUE(db_get_item_future.Get());
    return item;
  }

  void AddItemToDbAndDisk(const base::FilePath& fsp_path,
                          const std::string& version_tag,
                          base::Time time) {
    int64_t inserted_id = AddItemToDb(fsp_path, version_tag, time);
    EXPECT_TRUE(base::WriteFile(
        temp_dir_.GetPath().Append(base::NumberToString(inserted_id)),
        base::RandBytesAsString(kDefaultChunkSize)));
  }

  scoped_refptr<base::SequencedTaskRunner> db_task_runner_;
  base::WeakPtr<ContextDatabase> context_db_;
  std::unique_ptr<ContentCacheImpl> content_cache_;
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(FileSystemProviderContentCacheImplTest, StartWriteBytes) {
  // Perform initial write to cache of length 512 bytes.
  WriteFileToCache(base::FilePath("random-path"), /*version_tag=*/"versionA");

  // The first write to the disk should use the database ID as the file name.
  EXPECT_TRUE(base::PathExists(temp_dir_.GetPath().Append("1")));
}

TEST_F(FileSystemProviderContentCacheImplTest,
       StartWriteBytesShouldFailWithEmptyVersionTag) {
  OpenedCloudFile file(base::FilePath("not-in-cache"),
                       OpenFileMode::OPEN_FILE_MODE_READ, /*version_tag=*/"");
  EXPECT_FALSE(content_cache_->StartWriteBytes(file, /*buffer=*/nullptr,
                                               /*offset=*/0, kDefaultChunkSize,
                                               base::DoNothing()));
}

TEST_F(FileSystemProviderContentCacheImplTest,
       StartWriteBytesShouldFailIfNonContiguousChunk) {
  const base::FilePath fsp_path("random-path");
  const std::string version_tag("versionA");

  // Perform initial write to cache of length 512 bytes.
  WriteFileToCache(fsp_path, version_tag);

  // Try to write to the same file but from offset 1024 which leaves a 512 byte
  // hole in the file, not supported.
  OpenedCloudFile file(fsp_path, OpenFileMode::OPEN_FILE_MODE_READ,
                       version_tag);
  EXPECT_FALSE(content_cache_->StartWriteBytes(
      file, /*buffer=*/nullptr,
      /*offset=*/1024, kDefaultChunkSize, base::DoNothing()));
}

TEST_F(FileSystemProviderContentCacheImplTest,
       StartWriteBytesShouldFailIfBytesAlreadyExist) {
  const base::FilePath fsp_path("random-path");
  const std::string version_tag("versionA");

  // Perform initial write to cache of length 512.
  WriteFileToCache(fsp_path, version_tag);

  // Try to write to the same file with the exact same version_tag and byte
  // range, this should return false.
  OpenedCloudFile file(fsp_path, OpenFileMode::OPEN_FILE_MODE_READ,
                       version_tag);
  EXPECT_FALSE(content_cache_->StartWriteBytes(file, /*buffer=*/nullptr,
                                               /*offset=*/0, kDefaultChunkSize,
                                               base::DoNothing()));
}

TEST_F(FileSystemProviderContentCacheImplTest,
       StartWriteBytesShouldFailIfMultipleWritersAttemptToWriteAtOnce) {
  OpenedCloudFile file(base::FilePath("random-path"),
                       OpenFileMode::OPEN_FILE_MODE_READ,
                       /*version_tag=*/"versionA");
  TestFuture<base::File::Error> future;
  scoped_refptr<net::IOBufferWithSize> buffer =
      InitializeBufferWithRandBytes(kDefaultChunkSize * 2);
  EXPECT_TRUE(content_cache_->StartWriteBytes(file, buffer.get(), /*offset=*/0,
                                              kDefaultChunkSize,
                                              future.GetCallback()));

  // This attempt will be attempted before the `WriteBytesBlocking` call that is
  // made above, so this should fail as the first 512 byte chunk has not been
  // written yet.
  EXPECT_FALSE(content_cache_->StartWriteBytes(
      file, buffer.get(),
      /*offset=*/512, kDefaultChunkSize, base::DoNothing()));

  // Initial file write should succeed.
  EXPECT_EQ(future.Get(), base::File::FILE_OK);
}

TEST_F(FileSystemProviderContentCacheImplTest,
       StartWriteBytesForNewFileShouldFailIfCacheFull) {
  content_cache_->SetMaxCacheSize(2);

  // Inserts file into cache. 1 space left.
  WriteFileToCache(base::FilePath("random-path1"), /*version_tag=*/"versionA");
  // Inserts another file into cache. 0 spaces left.
  WriteFileToCache(base::FilePath("random-path2"), /*version_tag=*/"versionA");

  // Expect third insertion to fail.
  OpenedCloudFile file3(base::FilePath("random-path3"),
                        OpenFileMode::OPEN_FILE_MODE_READ,
                        /*version_tag=*/"versionA");
  EXPECT_FALSE(content_cache_->StartWriteBytes(file3, /*buffer=*/nullptr,
                                               /*offset=*/0, kDefaultChunkSize,
                                               base::DoNothing()));

  // Should still be able to write to existing files in the cache.
  OpenedCloudFile file1(base::FilePath("random-path1"),
                        OpenFileMode::OPEN_FILE_MODE_READ,
                        /*version_tag=*/"versionA");
  TestFuture<base::File::Error> future;
  scoped_refptr<net::IOBufferWithSize> buffer =
      InitializeBufferWithRandBytes(kDefaultChunkSize);
  EXPECT_TRUE(content_cache_->StartWriteBytes(file1, buffer.get(),
                                              /*offset=*/512, kDefaultChunkSize,
                                              future.GetCallback()));
  // Contiguous file write should succeed.
  EXPECT_EQ(future.Get(), base::File::FILE_OK);
}

TEST_F(FileSystemProviderContentCacheImplTest,
       StartReadBytesShouldFailOnFirstRead) {
  OpenedCloudFile file(base::FilePath("not-in-cache"),
                       OpenFileMode::OPEN_FILE_MODE_READ, /*version_tag=*/"");
  EXPECT_FALSE(content_cache_->StartReadBytes(file, /*buffer=*/nullptr,
                                              /*offset=*/0, kDefaultChunkSize,
                                              base::DoNothing()));
}

TEST_F(FileSystemProviderContentCacheImplTest,
       StartReadBytesShouldFailIfVersionTagMismatch) {
  // Write to cache a file with `versionA`.
  const base::FilePath fsp_path("random-path");
  WriteFileToCache(fsp_path, "versionA");

  // Attempt to read from the cache the same file with `versionB`.
  OpenedCloudFile file(fsp_path, OpenFileMode::OPEN_FILE_MODE_READ,
                       /*version_tag=*/"versionB");
  EXPECT_FALSE(content_cache_->StartReadBytes(file, /*buffer=*/nullptr,
                                              /*offset=*/0, kDefaultChunkSize,
                                              base::DoNothing()));
}

TEST_F(FileSystemProviderContentCacheImplTest,
       StartReadBytesShouldFailIfBytesRequestedArentAvailable) {
  const base::FilePath fsp_path("random-path");
  const std::string version_tag("versionA");
  WriteFileToCache(fsp_path, version_tag);

  OpenedCloudFile file(fsp_path, OpenFileMode::OPEN_FILE_MODE_READ,
                       version_tag);
  EXPECT_FALSE(content_cache_->StartReadBytes(file, /*buffer=*/nullptr,
                                              /*offset=*/512, kDefaultChunkSize,
                                              base::DoNothing()));
}

TEST_F(FileSystemProviderContentCacheImplTest,
       StartReadBytesShouldSucceedIfBytesAreAvailable) {
  const base::FilePath fsp_path("random-path");
  const std::string version_tag("versionA");
  WriteFileToCache(fsp_path, version_tag);

  OpenedCloudFile file(fsp_path, OpenFileMode::OPEN_FILE_MODE_READ,
                       version_tag);
  scoped_refptr<net::IOBufferWithSize> buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(kDefaultChunkSize);
  TestFuture<int, bool, base::File::Error> future;
  EXPECT_TRUE(content_cache_->StartReadBytes(file, buffer.get(), /*offset=*/0,
                                             kDefaultChunkSize,
                                             future.GetRepeatingCallback()));
  EXPECT_EQ(future.Get<0>(), kDefaultChunkSize);
}

TEST_F(FileSystemProviderContentCacheImplTest,
       FilesOnDiskAndInDbAreSuccessfullyAddedToCache) {
  const base::FilePath fsp_path("/test.txt");
  const std::string version_tag("versionA");
  AddItemToDbAndDisk(fsp_path, version_tag, base::Time::Now());

  TestFuture<void> future;
  content_cache_->LoadFromDisk(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  OpenedCloudFile file(fsp_path, OpenFileMode::OPEN_FILE_MODE_READ,
                       version_tag);
  scoped_refptr<net::IOBufferWithSize> buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(kDefaultChunkSize);
  TestFuture<int, bool, base::File::Error> read_bytes_future;
  EXPECT_TRUE(content_cache_->StartReadBytes(
      file, buffer.get(), /*offset=*/0, kDefaultChunkSize,
      read_bytes_future.GetRepeatingCallback()));
  EXPECT_EQ(read_bytes_future.Get<0>(), kDefaultChunkSize);
  EXPECT_EQ(read_bytes_future.Get<2>(), base::File::FILE_OK);
}

TEST_F(FileSystemProviderContentCacheImplTest,
       FilesOnDiskAndInDbAreInitializedInTheDatabaseAccessedTimeOrder) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(content_cache_->sequence_checker_);

  // This is the oldest accessed item on disk, we should order this last in the
  // LRU cache.
  base::Time older_time;
  EXPECT_TRUE(base::Time::FromString("17 Apr 2024 10:00 GMT", &older_time));
  AddItemToDbAndDisk(base::FilePath("/a.txt"), "versionA", older_time);

  // This is the most recently used item on disk, this should be ordered first.
  base::Time newer_time;
  EXPECT_TRUE(base::Time::FromString("17 Apr 2024 11:00 GMT", &newer_time));
  AddItemToDbAndDisk(base::FilePath("/b.txt"), "versionA", newer_time);

  TestFuture<void> future;
  content_cache_->LoadFromDisk(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  // TODO(b/328679426): Once eviction logic has been created, we can assert on
  // this. For now let's inspect the underlying `lru_cache_` instead.
  EXPECT_THAT(content_cache_->lru_cache_,
              ElementsAre(Key(base::FilePath("/b.txt")),
                          Key(base::FilePath("/a.txt"))));
}

TEST_F(FileSystemProviderContentCacheImplTest,
       FilesOnDiskWithNoSqlEntryAreRemoved) {
  // Only write the file to disk, don't write it into the database.
  base::FilePath path_on_disk =
      temp_dir_.GetPath().Append(base::NumberToString(1));
  base::WriteFile(path_on_disk, base::RandBytesAsString(kDefaultChunkSize));

  // Files that aren't integer file names are ignored.
  // TODO(b/335548274): Should we remove these files from disk, or ignore them.
  base::WriteFile(temp_dir_.GetPath().Append("unknown"),
                  base::RandBytesAsString(kDefaultChunkSize));

  TestFuture<void> future;
  content_cache_->LoadFromDisk(future.GetCallback());
  ASSERT_TRUE(future.Wait());
  EXPECT_FALSE(base::PathExists(path_on_disk));

  OpenedCloudFile file(base::FilePath("/test.txt"),
                       OpenFileMode::OPEN_FILE_MODE_READ,
                       /*version_tag=*/"versionA");
  EXPECT_FALSE(content_cache_->StartReadBytes(file, /*buffer=*/nullptr,
                                              /*offset=*/0, kDefaultChunkSize,
                                              base::DoNothing()));
}

TEST_F(FileSystemProviderContentCacheImplTest,
       FilesNotOnDiskButOnlyOnSqlAreRemoved) {
  const base::FilePath fsp_path("/test.txt");
  const std::string version_tag("versionA");
  int64_t inserted_id = AddItemToDb(fsp_path, version_tag, base::Time::Now());

  TestFuture<void> future;
  content_cache_->LoadFromDisk(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  OpenedCloudFile file(fsp_path, OpenFileMode::OPEN_FILE_MODE_READ,
                       version_tag);
  EXPECT_FALSE(content_cache_->StartReadBytes(file, /*buffer=*/nullptr,
                                              /*offset=*/0, kDefaultChunkSize,
                                              base::DoNothing()));

  ContextDatabase::Item item = GetItemById(inserted_id);
  EXPECT_FALSE(item.item_exists);
}

}  // namespace ash::file_system_provider
