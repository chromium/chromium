// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/content_cache/content_cache_impl.h"

#include "base/files/file_path.h"
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
#include "chrome/browser/ash/file_system_provider/content_cache/content_cache.h"
#include "chrome/browser/ash/file_system_provider/content_cache/context_database.h"
#include "chrome/browser/ash/file_system_provider/opened_cloud_file.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_interface.h"
#include "net/base/io_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::file_system_provider {
namespace {

using base::test::TestFuture;
using testing::AllOf;
using testing::ElementsAre;
using testing::Field;
using testing::Key;

// The default chunk size that is requested via the `FileStreamReader`.
constexpr int kDefaultChunkSize = 512;

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
                        const std::string& version_tag,
                        int chunk_size) {
    OpenedCloudFile file(path, OpenFileMode::OPEN_FILE_MODE_READ, version_tag,
                         chunk_size);

    // Create a buffer and ensure there is data in the buffer before writing it
    // to disk.
    scoped_refptr<net::IOBufferWithSize> buffer =
        InitializeBufferWithRandBytes(chunk_size);
    TestFuture<base::File::Error> future;
    EXPECT_TRUE(content_cache_->StartWriteBytes(file, buffer.get(),
                                                /*offset=*/0, chunk_size,
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
  WriteFileToCache(base::FilePath("random-path"), /*version_tag=*/"versionA",
                   kDefaultChunkSize);

  // The first write to the disk should use the database ID as the file name.
  EXPECT_TRUE(base::PathExists(temp_dir_.GetPath().Append("1")));
  EXPECT_THAT(content_cache_->GetCachedFilePaths(),
              ElementsAre(base::FilePath("random-path")));
}

TEST_F(FileSystemProviderContentCacheImplTest,
       StartWriteBytesShouldFailWithEmptyVersionTag) {
  OpenedCloudFile file(base::FilePath("not-in-cache"),
                       OpenFileMode::OPEN_FILE_MODE_READ, /*version_tag=*/"",
                       kDefaultChunkSize);
  EXPECT_FALSE(content_cache_->StartWriteBytes(file, /*buffer=*/nullptr,
                                               /*offset=*/0, kDefaultChunkSize,
                                               base::DoNothing()));
}

TEST_F(FileSystemProviderContentCacheImplTest,
       StartWriteBytesShouldFailIfNonContiguousChunk) {
  const base::FilePath fsp_path("random-path");
  const std::string version_tag("versionA");

  // Perform initial write to cache of length 512 bytes.
  WriteFileToCache(fsp_path, version_tag, kDefaultChunkSize);

  // Try to write to the same file but from offset 1024 which leaves a 512 byte
  // hole in the file, not supported.
  OpenedCloudFile file(fsp_path, OpenFileMode::OPEN_FILE_MODE_READ, version_tag,
                       kDefaultChunkSize);
  EXPECT_FALSE(content_cache_->StartWriteBytes(
      file, /*buffer=*/nullptr,
      /*offset=*/1024, kDefaultChunkSize, base::DoNothing()));
}

TEST_F(FileSystemProviderContentCacheImplTest,
       StartWriteBytesShouldFailIfBytesAlreadyExist) {
  const base::FilePath fsp_path("random-path");
  const std::string version_tag("versionA");

  // Perform initial write to cache of length 512.
  WriteFileToCache(fsp_path, version_tag, kDefaultChunkSize);

  // Try to write to the same file with the exact same version_tag and byte
  // range, this should return false.
  OpenedCloudFile file(fsp_path, OpenFileMode::OPEN_FILE_MODE_READ, version_tag,
                       kDefaultChunkSize);
  EXPECT_FALSE(content_cache_->StartWriteBytes(file, /*buffer=*/nullptr,
                                               /*offset=*/0, kDefaultChunkSize,
                                               base::DoNothing()));
}

TEST_F(FileSystemProviderContentCacheImplTest,
       StartWriteBytesShouldFailIfMultipleWritersAttemptToWriteAtOnce) {
  OpenedCloudFile file(base::FilePath("random-path"),
                       OpenFileMode::OPEN_FILE_MODE_READ,
                       /*version_tag=*/"versionA", kDefaultChunkSize * 2);
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
       StartWriteBytesForNewFileShouldEvictOldestFileFirst) {
  content_cache_->SetMaxCacheItems(2);

  // Inserts file into cache with size `kDefaultChunkSize`. 1 space left.
  WriteFileToCache(base::FilePath("random-path1"), /*version_tag=*/"versionA",
                   kDefaultChunkSize);
  // Inserts another file into cache that is `kDefaultChunkSize` * 2. 0 spaces
  // left.
  WriteFileToCache(base::FilePath("random-path2"), /*version_tag=*/"versionA",
                   kDefaultChunkSize * 2);

  EXPECT_THAT(content_cache_->GetCachedFilePaths(),
              ElementsAre(base::FilePath("random-path2"),
                          base::FilePath("random-path1")));

  // Expect third insertion to succeed, with size of `kDefaultChunkSize` * 3.
  OpenedCloudFile file3(base::FilePath("random-path3"),
                        OpenFileMode::OPEN_FILE_MODE_READ,
                        /*version_tag=*/"versionA", kDefaultChunkSize * 3);
  TestFuture<base::File::Error> future;
  scoped_refptr<net::IOBufferWithSize> buffer =
      InitializeBufferWithRandBytes(kDefaultChunkSize * 3);
  EXPECT_TRUE(content_cache_->StartWriteBytes(file3, buffer.get(),
                                              /*offset=*/0, kDefaultChunkSize,
                                              future.GetCallback()));
  EXPECT_EQ(future.Get(), base::File::FILE_OK);

  // Oldest file (i.e. `random-path1`) should be marked for eviction now and
  // thus `StartWriteBytes` should return false.
  OpenedCloudFile file1(base::FilePath("random-path1"),
                        OpenFileMode::OPEN_FILE_MODE_READ,
                        /*version_tag=*/"versionA", kDefaultChunkSize);
  EXPECT_FALSE(content_cache_->StartWriteBytes(
      file1, /*buffer=*/nullptr,
      /*offset=*/512, kDefaultChunkSize, base::DoNothing()));

  // Evicting items should return the total size of `kDefaultChunkSize` which is
  // the size of `random-path1` as that is the least-recently used item in the
  // cache.
  TestFuture<EvictedItemStats> evict_items_future;
  content_cache_->EvictItems(evict_items_future.GetCallback());
  EXPECT_THAT(
      evict_items_future.Get(),
      AllOf(Field(&EvictedItemStats::num_items, 1),
            Field(&EvictedItemStats::bytes_evicted, kDefaultChunkSize)));
}

TEST_F(FileSystemProviderContentCacheImplTest,
       StartReadBytesShouldFailOnFirstRead) {
  OpenedCloudFile file(base::FilePath("not-in-cache"),
                       OpenFileMode::OPEN_FILE_MODE_READ, /*version_tag=*/"",
                       kDefaultChunkSize);
  EXPECT_FALSE(content_cache_->StartReadBytes(file, /*buffer=*/nullptr,
                                              /*offset=*/0, kDefaultChunkSize,
                                              base::DoNothing()));
}

TEST_F(FileSystemProviderContentCacheImplTest,
       StartReadBytesShouldFailIfVersionTagMismatch) {
  // Write to cache a file with `versionA`.
  const base::FilePath fsp_path("random-path");
  WriteFileToCache(fsp_path, "versionA", kDefaultChunkSize);

  // Attempt to read from the cache the same file with `versionB`.
  OpenedCloudFile file(fsp_path, OpenFileMode::OPEN_FILE_MODE_READ,
                       /*version_tag=*/"versionB", kDefaultChunkSize);
  EXPECT_FALSE(content_cache_->StartReadBytes(file, /*buffer=*/nullptr,
                                              /*offset=*/0, kDefaultChunkSize,
                                              base::DoNothing()));
}

TEST_F(FileSystemProviderContentCacheImplTest,
       StartReadBytesShouldSucceedIfRequestedBytesAreAtEOF) {
  const base::FilePath fsp_path("random-path");
  const std::string version_tag("versionA");
  WriteFileToCache(fsp_path, version_tag, kDefaultChunkSize);

  OpenedCloudFile file(fsp_path, OpenFileMode::OPEN_FILE_MODE_READ, version_tag,
                       kDefaultChunkSize);
  scoped_refptr<net::IOBufferWithSize> buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(kDefaultChunkSize);
  TestFuture<int, bool, base::File::Error> future;

  // The file in the cloud has 512 bytes, however, the reader is attempting to
  // get another 512 bytes starting from 512. Readers expect the `bytes_read` to
  // return with 0 to indicate EOF, follow up requests should honor this.
  EXPECT_TRUE(content_cache_->StartReadBytes(file, /*buffer=*/nullptr,
                                             /*offset=*/512, kDefaultChunkSize,
                                             future.GetRepeatingCallback()));
  EXPECT_EQ(future.Get<int>(), 0);
}

TEST_F(FileSystemProviderContentCacheImplTest,
       StartReadBytesShouldSucceedIfExactBytesAreAvailable) {
  const base::FilePath fsp_path("random-path");
  const std::string version_tag("versionA");
  WriteFileToCache(fsp_path, version_tag, kDefaultChunkSize);

  OpenedCloudFile file(fsp_path, OpenFileMode::OPEN_FILE_MODE_READ, version_tag,
                       kDefaultChunkSize);
  scoped_refptr<net::IOBufferWithSize> buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(kDefaultChunkSize);
  TestFuture<int, bool, base::File::Error> future;
  EXPECT_TRUE(content_cache_->StartReadBytes(file, buffer.get(), /*offset=*/0,
                                             kDefaultChunkSize,
                                             future.GetRepeatingCallback()));
  EXPECT_EQ(future.Get<0>(), kDefaultChunkSize);
}

TEST_F(
    FileSystemProviderContentCacheImplTest,
    StartReadBytesShouldSucceedIfRequestIsForLengthThatExceedsKnownSizeOnDiskAndCloud) {
  const base::FilePath fsp_path("random-path");
  const std::string version_tag("versionA");
  WriteFileToCache(fsp_path, version_tag, /*chunk_size=*/49);

  OpenedCloudFile file(fsp_path, OpenFileMode::OPEN_FILE_MODE_READ, version_tag,
                       49);
  scoped_refptr<net::IOBufferWithSize> buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(kDefaultChunkSize);
  TestFuture<int, bool, base::File::Error> future;

  // First request is made for a file that is 49 bytes big but the request is
  // for `kDefaultChunkSize` instead.
  EXPECT_TRUE(content_cache_->StartReadBytes(file, buffer.get(), /*offset=*/0,
                                             /*length=*/kDefaultChunkSize,
                                             future.GetRepeatingCallback()));
  EXPECT_EQ(future.Get<0>(), 49);
  future.Clear();

  // The client then requests from the previous offset and the same length, we
  // want to avoid sending this to the FSP as we have all this data available.
  EXPECT_TRUE(content_cache_->StartReadBytes(file, buffer.get(), /*offset=*/49,
                                             /*length=*/kDefaultChunkSize,
                                             future.GetRepeatingCallback()));
  EXPECT_EQ(future.Get<0>(), 0);
}

TEST_F(FileSystemProviderContentCacheImplTest,
       StartReadBytesShouldFailIfOnlyHalfTheFileIsOnDisk) {
  const base::FilePath fsp_path("random-path");
  const std::string version_tag("versionA");
  // Stage a file with `kDefaultChunkSize` bytes in the cache. For the purposes
  // of this test, let's assume that this file is actually of size 640 bytes
  // (i.e. we have 512 bytes in cache and 128 bytes unretrieved from the cloud,
  // possibly from a previously interrupted write to the cache).
  WriteFileToCache(fsp_path, version_tag, kDefaultChunkSize);

  OpenedCloudFile file(fsp_path, OpenFileMode::OPEN_FILE_MODE_READ, version_tag,
                       kDefaultChunkSize + 128);
  scoped_refptr<net::IOBufferWithSize> buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(kDefaultChunkSize);
  TestFuture<int, bool, base::File::Error> future;

  // First request is made for a file that is `kDefaultChunkSize` bytes.
  EXPECT_TRUE(content_cache_->StartReadBytes(file, buffer.get(),
                                             /*offset=*/0,
                                             /*length=*/kDefaultChunkSize,
                                             future.GetRepeatingCallback()));
  EXPECT_EQ(future.Get<0>(), kDefaultChunkSize);
  future.Clear();

  // The client then requests from the previous offset but the remaining length,
  // this should return false to ensure the next request can be made from the
  // underlying FSP.
  EXPECT_FALSE(content_cache_->StartReadBytes(file, buffer.get(),
                                              /*offset=*/kDefaultChunkSize,
                                              /*length=*/128,
                                              base::DoNothing()));
}

TEST_F(
    FileSystemProviderContentCacheImplTest,
    StartReadBytesShouldFailIfOnlyHalfTheFileIsOnDiskWithDifferentChunkSizes) {
  const base::FilePath fsp_path("random-path");
  const std::string version_tag("versionA");
  // Stage a file with 576 bytes in the cache. For the purposes
  // of this test, let's assume that this file is actually of size 640 bytes
  // (i.e. we have 576 bytes in cache and 64 bytes unretrieved from the cloud,
  // possibly from a previously interrupted write to the cache).
  WriteFileToCache(fsp_path, version_tag, 576);

  OpenedCloudFile file(fsp_path, OpenFileMode::OPEN_FILE_MODE_READ, version_tag,
                       640);
  scoped_refptr<net::IOBufferWithSize> buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(kDefaultChunkSize);
  TestFuture<int, bool, base::File::Error> future;

  // First request is made for a file that is `kDefaultChunkSize` bytes.
  EXPECT_TRUE(content_cache_->StartReadBytes(file, buffer.get(),
                                             /*offset=*/0,
                                             /*length=*/kDefaultChunkSize,
                                             future.GetRepeatingCallback()));
  EXPECT_EQ(future.Get<0>(), kDefaultChunkSize);
  future.Clear();

  // The client then requests from the previous offset but the remaining length,
  // this should return true but only 64 bytes should be returned.
  EXPECT_TRUE(content_cache_->StartReadBytes(file, buffer.get(),
                                             /*offset=*/kDefaultChunkSize,
                                             /*length=*/128,
                                             future.GetRepeatingCallback()));
  EXPECT_EQ(future.Get<0>(), 64);
  future.Clear();

  // The follow up request for the remaining bytes should return false to
  // indicate to the `CloudFileSystem` that the request must be delegated to the
  // underlying FSP.
  EXPECT_FALSE(content_cache_->StartReadBytes(file, buffer.get(),
                                              /*offset=*/kDefaultChunkSize + 64,
                                              /*length=*/64,
                                              base::DoNothing()));
}

TEST_F(FileSystemProviderContentCacheImplTest,
       FilesOnDiskAndInDbAreSuccessfullyAddedToCache) {
  const base::FilePath fsp_path("/test.txt");
  const std::string version_tag("versionA");
  AddItemToDbAndDisk(fsp_path, version_tag, base::Time::Now());

  TestFuture<void> future;
  content_cache_->LoadFromDisk(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  OpenedCloudFile file(fsp_path, OpenFileMode::OPEN_FILE_MODE_READ, version_tag,
                       kDefaultChunkSize);
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

  EXPECT_THAT(content_cache_->GetCachedFilePaths(),
              ElementsAre(base::FilePath("/b.txt"), base::FilePath("/a.txt")));
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
                       /*version_tag=*/"versionA", kDefaultChunkSize);
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

  OpenedCloudFile file(fsp_path, OpenFileMode::OPEN_FILE_MODE_READ, version_tag,
                       kDefaultChunkSize);
  EXPECT_FALSE(content_cache_->StartReadBytes(file, /*buffer=*/nullptr,
                                              /*offset=*/0, kDefaultChunkSize,
                                              base::DoNothing()));

  ContextDatabase::Item item = GetItemById(inserted_id);
  EXPECT_FALSE(item.item_exists);
}

TEST_F(FileSystemProviderContentCacheImplTest,
       EvictItemsEvictsFilesMarkedForEviction) {
  // Inserts file into cache with size `kDefaultChunkSize`.
  int64_t random_path1_size = kDefaultChunkSize;
  base::FilePath random_path1("random-path1");
  WriteFileToCache(random_path1, "versionA", random_path1_size);

  // Mark file for eviction.
  content_cache_->MarkItemForEviction(random_path1);

  // The items marked for eviction should not be readable again (despite being
  // in the cache).
  OpenedCloudFile file(random_path1, OpenFileMode::OPEN_FILE_MODE_READ,
                       "versionA", kDefaultChunkSize);
  EXPECT_FALSE(content_cache_->StartReadBytes(file, /*buffer=*/nullptr,
                                              /*offset=*/0, kDefaultChunkSize,
                                              base::DoNothing()));

  // Ensure the `EvictItems` returns the correct values.
  TestFuture<EvictedItemStats> evict_items_future;
  content_cache_->EvictItems(evict_items_future.GetCallback());
  EXPECT_THAT(
      evict_items_future.Get(),
      AllOf(Field(&EvictedItemStats::num_items, 1),
            Field(&EvictedItemStats::bytes_evicted, random_path1_size)));
}

TEST_F(FileSystemProviderContentCacheImplTest,
       MarkNonExistentFileForEvictionDoesNothing) {
  // Mark non-existent file for eviction.
  content_cache_->MarkItemForEviction(base::FilePath("random-path1"));

  // Ensure the `EvictItems` does nothing.
  TestFuture<EvictedItemStats> evict_items_future;
  content_cache_->EvictItems(evict_items_future.GetCallback());
  EXPECT_THAT(evict_items_future.Get(),
              AllOf(Field(&EvictedItemStats::num_items, 0),
                    Field(&EvictedItemStats::bytes_evicted, 0)));
}

TEST_F(FileSystemProviderContentCacheImplTest,
       SetMaxCacheItemsShouldEvictOldestFilesOnResize) {
  content_cache_->SetMaxCacheItems(3);

  // Inserts file into cache with size `kDefaultChunkSize`. 2 spaces left.
  int64_t random_path1_size = kDefaultChunkSize;
  base::FilePath random_path1("random-path1");
  WriteFileToCache(random_path1, "versionA", random_path1_size);
  // Inserts another file into cache that is `kDefaultChunkSize` * 2. 1 space
  // left.
  int64_t random_path2_size = kDefaultChunkSize * 2;
  base::FilePath random_path2("random-path2");
  WriteFileToCache(random_path2, "versionA", random_path2_size);
  // Inserts another file into cache that is `kDefaultChunkSize` * 4. 0 spaces
  // left.
  int64_t random_path3_size = kDefaultChunkSize * 4;
  WriteFileToCache(base::FilePath("random-path3"), "versionA",
                   random_path3_size);

  // Resize the cache to only have 1 spot, the `random-path1` and `random-path2`
  // entries (least-recently used) should be evicted since there are no files
  // already marked for eviction.
  content_cache_->SetMaxCacheItems(1);

  // The items marked for eviction should not be readable again (despite being
  // in the cache).
  OpenedCloudFile file1(random_path1, OpenFileMode::OPEN_FILE_MODE_READ,
                        "versionA", random_path1_size);
  EXPECT_FALSE(content_cache_->StartReadBytes(file1, /*buffer=*/nullptr,
                                              /*offset=*/0, kDefaultChunkSize,
                                              base::DoNothing()));
  OpenedCloudFile file2(random_path2, OpenFileMode::OPEN_FILE_MODE_READ,
                        "versionA", random_path2_size);
  EXPECT_FALSE(content_cache_->StartReadBytes(file2, /*buffer=*/nullptr,
                                              /*offset=*/0, kDefaultChunkSize,
                                              base::DoNothing()));

  // Ensure the `EvictItems` returns the correct values.
  TestFuture<EvictedItemStats> evict_items_future;
  content_cache_->EvictItems(evict_items_future.GetCallback());
  EXPECT_THAT(evict_items_future.Get(),
              AllOf(Field(&EvictedItemStats::num_items, 2),
                    Field(&EvictedItemStats::bytes_evicted,
                          random_path1_size + random_path2_size)));
}

TEST_F(
    FileSystemProviderContentCacheImplTest,
    SetMaxCacheItemsShouldEvictFilesMarkedForEvictionBeforeOldestFilesOnResize) {
  content_cache_->SetMaxCacheItems(3);

  // Inserts file into cache with size `kDefaultChunkSize`. 2 spaces left.
  int64_t random_path1_size = kDefaultChunkSize;
  base::FilePath random_path1("random-path1");
  WriteFileToCache(random_path1, "versionA", random_path1_size);
  // Inserts another file into cache that is `kDefaultChunkSize` * 2. 1 space
  // left.
  int64_t random_path2_size = kDefaultChunkSize * 2;
  base::FilePath random_path2("random-path2");
  WriteFileToCache(random_path2, "versionA", random_path2_size);
  // Inserts another file into cache that is `kDefaultChunkSize` * 4. 0 spaces
  // left.
  int64_t random_path3_size = kDefaultChunkSize * 4;
  base::FilePath random_path3("random-path3");
  WriteFileToCache(random_path3, "versionA", random_path3_size);

  // Mark most-recently-used file for eviction.
  content_cache_->MarkItemForEviction(random_path3);

  // Resize the cache to only have 1 spot, the `random-path3` entry
  // is already marked for eviction so only one more file (`random-path1` the
  // least-recently used) is marked for eviction.
  content_cache_->SetMaxCacheItems(1);

  // The items marked for eviction should not be readable again (despite being
  // in the cache).
  OpenedCloudFile file1(random_path1, OpenFileMode::OPEN_FILE_MODE_READ,
                        "versionA", random_path1_size);
  EXPECT_FALSE(content_cache_->StartReadBytes(file1, /*buffer=*/nullptr,
                                              /*offset=*/0, kDefaultChunkSize,
                                              base::DoNothing()));
  OpenedCloudFile file3(random_path3, OpenFileMode::OPEN_FILE_MODE_READ,
                        "versionA", random_path3_size);
  EXPECT_FALSE(content_cache_->StartReadBytes(file3, /*buffer=*/nullptr,
                                              /*offset=*/0, kDefaultChunkSize,
                                              base::DoNothing()));

  // Ensure the `EvictItems` returns the correct values.
  TestFuture<EvictedItemStats> evict_items_future;
  content_cache_->EvictItems(evict_items_future.GetCallback());
  EXPECT_THAT(evict_items_future.Get(),
              AllOf(Field(&EvictedItemStats::num_items, 2),
                    Field(&EvictedItemStats::bytes_evicted,
                          random_path1_size + random_path3_size)));
}

TEST_F(FileSystemProviderContentCacheImplTest,
       EvictItemsCanBeCalledMultipleTimesAndWithNoItems) {
  // Add 2 items to the cache then resize the cache to only allow 1 item (this
  // marks /a.txt for eviction).
  WriteFileToCache(base::FilePath("/a.txt"), "versionA", kDefaultChunkSize);
  WriteFileToCache(base::FilePath("/b.txt"), "versionB", kDefaultChunkSize);
  content_cache_->SetMaxCacheItems(1);

  // Run EvictItems twice, the first invocation will yield on the first eviction
  // of the item from the file system which will allow the second EvictItems to
  // be ran before the first one has completed.
  TestFuture<EvictedItemStats> first_evict_items_future;
  TestFuture<EvictedItemStats> second_evict_items_future;
  content_cache_->EvictItems(first_evict_items_future.GetCallback());
  content_cache_->EvictItems(second_evict_items_future.GetCallback());

  // Ensure the same value comes back on both the callbacks.
  auto evict_items_callback_matcher =
      AllOf(Field(&EvictedItemStats::num_items, 1),
            Field(&EvictedItemStats::bytes_evicted, kDefaultChunkSize));

  EXPECT_THAT(first_evict_items_future.Get(), evict_items_callback_matcher);
  EXPECT_THAT(second_evict_items_future.Get(), evict_items_callback_matcher);

  // If evict is called again, should respond via the callback with 0 items and
  // 0 bytes evicted.
  TestFuture<EvictedItemStats> no_items_evicted_future;
  content_cache_->EvictItems(no_items_evicted_future.GetCallback());
  EXPECT_THAT(no_items_evicted_future.Get(),
              AllOf(Field(&EvictedItemStats::num_items, 0),
                    Field(&EvictedItemStats::bytes_evicted, 0)));
}

}  // namespace
}  // namespace ash::file_system_provider
