// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/file_system_provider/content_cache/content_cache_impl.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
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

using base::test::RunClosure;
using base::test::TestFuture;
using testing::_;
using testing::AllOf;
using testing::ElementsAre;
using testing::Field;
using testing::Key;
using testing::Pair;

// The default chunk size that is requested via the `FileStreamReader`.
constexpr int kDefaultChunkSize = 512;

class MockContentCacheObserver : public ContentCache::Observer {
 public:
  MOCK_METHOD(void,
              OnItemEvicted,
              (const base::FilePath& fsp_path),
              (override));

  MOCK_METHOD(void,
              OnItemRemovedFromDisk,
              (const base::FilePath& fsp_path, int64_t bytes_removed),
              (override));
};

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

    content_cache_->AddObserver(&content_cache_observer_);
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

  OpenedCloudFile WriteFileToCache(const base::FilePath& path,
                                   const std::string& version_tag,
                                   int chunk_size) {
    OpenedCloudFile file(path, OpenFileMode::OPEN_FILE_MODE_READ, ++request_id_,
                         version_tag, chunk_size);

    // Create a buffer and ensure there is data in the buffer before writing it
    // to disk.
    scoped_refptr<net::IOBufferWithSize> buffer =
        InitializeBufferWithRandBytes(chunk_size);
    EXPECT_EQ(WriteBytesToContentCache(file, buffer, /*offset=*/0, chunk_size),
              base::File::FILE_OK);

    return file;
  }

  OpenedCloudFile ReadFileFromCache(const base::FilePath& path,
                                    const std::string& version_tag,
                                    int chunk_size) {
    OpenedCloudFile file(path, OpenFileMode::OPEN_FILE_MODE_READ, ++request_id_,
                         version_tag, chunk_size);
    scoped_refptr<net::IOBufferWithSize> buffer =
        base::MakeRefCounted<net::IOBufferWithSize>(kDefaultChunkSize);
    EXPECT_THAT(
        ReadBytesFromContentCache(file, buffer, /*offset=*/0, chunk_size),
        Pair(chunk_size, base::File::FILE_OK));

    return file;
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

  std::unique_ptr<std::optional<ContextDatabase::Item>> GetItemById(
      int64_t item_id) {
    TestFuture<std::unique_ptr<std::optional<ContextDatabase::Item>>>
        db_get_item_future;
    db_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          return context_db_->GetItemById(item_id);
        }),
        db_get_item_future.GetCallback());
    return db_get_item_future.Take();
  }

  void AddItemToDbAndDisk(const base::FilePath& fsp_path,
                          const std::string& version_tag,
                          base::Time time) {
    int64_t inserted_id = AddItemToDb(fsp_path, version_tag, time);
    EXPECT_TRUE(base::WriteFile(
        temp_dir_.GetPath().Append(base::NumberToString(inserted_id)),
        base::RandBytesAsString(kDefaultChunkSize)));
  }

  base::File::Error WriteBytesToContentCache(
      const OpenedCloudFile& file,
      scoped_refptr<net::IOBuffer> buffer,
      int64_t offset,
      int length) {
    TestFuture<base::File::Error> future;
    content_cache_->WriteBytes(file, buffer, offset, length,
                               future.GetCallback());
    return future.Get();
  }

  std::pair<int, base::File::Error> ReadBytesFromContentCache(
      const OpenedCloudFile& file,
      scoped_refptr<net::IOBuffer> buffer,
      int64_t offset,
      int length) {
    TestFuture<int, bool, base::File::Error> future;
    content_cache_->ReadBytes(file, buffer, offset, length,
                              future.GetRepeatingCallback());
    EXPECT_TRUE(future.Wait());
    return std::make_pair(future.Get<int>(), future.Get<base::File::Error>());
  }

  std::unique_ptr<base::RunLoop> CreateItemRemovedRunLoop(
      const base::FilePath& fsp_path,
      int64_t bytes_removed) {
    auto run_loop = std::make_unique<base::RunLoop>();
    EXPECT_CALL(content_cache_observer_,
                OnItemRemovedFromDisk(fsp_path, bytes_removed))
        .WillOnce(RunClosure(run_loop->QuitClosure()));
    return run_loop;
  }

  int request_id_ = 0;

  scoped_refptr<base::SequencedTaskRunner> db_task_runner_;
  base::WeakPtr<ContextDatabase> context_db_;
  std::unique_ptr<ContentCacheImpl> content_cache_;
  MockContentCacheObserver content_cache_observer_;
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(FileSystemProviderContentCacheImplTest, WriteBytes) {
  // Perform initial write to cache of length 512 bytes.
  OpenedCloudFile file =
      WriteFileToCache(base::FilePath("random-path"),
                       /*version_tag=*/"versionA", kDefaultChunkSize);

  // The first write to the disk should use the database ID as the file name.
  base::FilePath path = temp_dir_.GetPath().Append("1");
  EXPECT_TRUE(base::PathExists(path));
  EXPECT_THAT(content_cache_->GetCachedFilePaths(),
              ElementsAre(base::FilePath("random-path")));

  // Contiguous writes should be allowed if re-using the same request ID (which
  // is stored in the `OpenedCloudFile` returned from above).
  scoped_refptr<net::IOBuffer> buffer = InitializeBufferWithRandBytes(64);
  EXPECT_EQ(WriteBytesToContentCache(file, buffer,
                                     /*offset=*/kDefaultChunkSize, 64),
            base::File::FILE_OK);

  // Ensure the file on disk is the correct size.
  base::File::Info info;
  EXPECT_TRUE(base::GetFileInfo(path, &info));
  EXPECT_EQ(info.size, kDefaultChunkSize + 64);
}

TEST_F(FileSystemProviderContentCacheImplTest,
       WriteBytesShouldFailWithEmptyVersionTag) {
  OpenedCloudFile file(base::FilePath("not-in-cache"),
                       OpenFileMode::OPEN_FILE_MODE_READ, ++request_id_,
                       /*version_tag=*/"", kDefaultChunkSize);
  EXPECT_EQ(WriteBytesToContentCache(file, /*buffer=*/nullptr,
                                     /*offset=*/0, kDefaultChunkSize),
            base::File::FILE_ERROR_INVALID_OPERATION);
}

TEST_F(FileSystemProviderContentCacheImplTest,
       WriteBytesShouldFailWithDifferentVersionTag) {
  const base::FilePath fsp_path("random-path");

  // Perform initial write to cache of length 512 bytes.
  WriteFileToCache(fsp_path, "versionA", kDefaultChunkSize);

  // Try to write to the same file from offset 512 but with a different
  // version_tag.
  OpenedCloudFile file(fsp_path, OpenFileMode::OPEN_FILE_MODE_READ,
                       ++request_id_, "versionB", kDefaultChunkSize);
  EXPECT_EQ(
      WriteBytesToContentCache(file, /*buffer=*/nullptr,
                               /*offset=*/kDefaultChunkSize, kDefaultChunkSize),
      base::File::FILE_ERROR_NOT_FOUND);
}

TEST_F(FileSystemProviderContentCacheImplTest,
       WriteBytesShouldFailIfNonContiguousChunk) {
  const base::FilePath fsp_path("random-path");
  const std::string version_tag("versionA");

  // Perform initial write to cache of length 512 bytes.
  WriteFileToCache(fsp_path, version_tag, kDefaultChunkSize);

  // Try to write to the same file but from offset 1024 which leaves a 512 byte
  // hole in the file, not supported.
  OpenedCloudFile file(fsp_path, OpenFileMode::OPEN_FILE_MODE_READ,
                       ++request_id_, version_tag, kDefaultChunkSize);
  EXPECT_EQ(WriteBytesToContentCache(file, /*buffer=*/nullptr,
                                     /*offset=*/1024, kDefaultChunkSize),
            base::File::FILE_ERROR_INVALID_OPERATION);
}

TEST_F(FileSystemProviderContentCacheImplTest,
       WriteBytesShouldFailIfBytesAlreadyExist) {
  const base::FilePath fsp_path("random-path");
  const std::string version_tag("versionA");

  // Perform initial write to cache of length 512.
  WriteFileToCache(fsp_path, version_tag, kDefaultChunkSize);

  // Try to write to the same file with the exact same version_tag and byte
  // range, this should return false.
  OpenedCloudFile file(fsp_path, OpenFileMode::OPEN_FILE_MODE_READ,
                       ++request_id_, version_tag, kDefaultChunkSize);
  EXPECT_EQ(WriteBytesToContentCache(file, /*buffer=*/nullptr,
                                     /*offset=*/0, kDefaultChunkSize),
            base::File::FILE_ERROR_INVALID_OPERATION);
}

TEST_F(FileSystemProviderContentCacheImplTest,
       WriteBytesShouldFailIfMultipleWritersAttemptToWriteToStartOfNewFile) {
  OpenedCloudFile file(base::FilePath("random-path"),
                       OpenFileMode::OPEN_FILE_MODE_READ, ++request_id_,
                       /*version_tag=*/"versionA", kDefaultChunkSize * 2);
  // First write attempt to start of file.  Eventually creates a file on disk.
  TestFuture<base::File::Error> future;
  scoped_refptr<net::IOBufferWithSize> buffer =
      InitializeBufferWithRandBytes(kDefaultChunkSize * 2);
  content_cache_->WriteBytes(file, buffer, /*offset=*/0, kDefaultChunkSize,
                             future.GetCallback());

  // Second write attempt to start of file. This will be attempted before the
  // write that is made above has created a path on disk. And the second write
  // will not create a path on disk as this is only done on new cache entries.
  EXPECT_EQ(WriteBytesToContentCache(file, /*buffer=*/nullptr,
                                     /*offset=*/0, kDefaultChunkSize),
            base::File::FILE_ERROR_NOT_FOUND);

  // Allow the first write to continue. It should succeed.
  EXPECT_EQ(future.Get(), base::File::FILE_OK);
}

TEST_F(FileSystemProviderContentCacheImplTest,
       WriteBytesShouldFailIfMultipleContiguousWritersAttemptToWriteToNewFile) {
  OpenedCloudFile file(base::FilePath("random-path"),
                       OpenFileMode::OPEN_FILE_MODE_READ, ++request_id_,
                       /*version_tag=*/"versionA", kDefaultChunkSize * 2);
  // First write attempt.  Eventually creates a file on disk.
  TestFuture<base::File::Error> future;
  scoped_refptr<net::IOBufferWithSize> buffer =
      InitializeBufferWithRandBytes(kDefaultChunkSize * 2);
  content_cache_->WriteBytes(file, buffer, /*offset=*/0, kDefaultChunkSize,
                             future.GetCallback());

  // Second write attempt to contiguous chunk. This will be attempted before the
  // write that is made above, so this should fail as offset writes are not
  // allowed.
  EXPECT_EQ(
      WriteBytesToContentCache(file, /*buffer=*/nullptr,
                               /*offset=*/kDefaultChunkSize, kDefaultChunkSize),
      base::File::FILE_ERROR_INVALID_OPERATION);

  // Allow the first write to continue. It should succeed.
  EXPECT_EQ(future.Get(), base::File::FILE_OK);
}

TEST_F(FileSystemProviderContentCacheImplTest,
       WriteBytesShouldFailIfMultipleWritersAttemptToWriteToExistingFile) {
  // Perform initial write to cache of length 512 bytes. Creates a file on disk.
  OpenedCloudFile file =
      WriteFileToCache(base::FilePath("random-path"),
                       /*version_tag=*/"versionA", kDefaultChunkSize);

  // Write attempt to second chunk.
  TestFuture<base::File::Error> future;
  scoped_refptr<net::IOBufferWithSize> buffer =
      InitializeBufferWithRandBytes(kDefaultChunkSize * 2);
  content_cache_->WriteBytes(file, buffer, /*offset=*/kDefaultChunkSize,
                             kDefaultChunkSize, future.GetCallback());

  // This will be attempted before the write that is made above, so this should
  // fail as the first write is in the middle of writing.
  EXPECT_EQ(
      WriteBytesToContentCache(file, /*buffer=*/nullptr,
                               /*offset=*/kDefaultChunkSize, kDefaultChunkSize),
      base::File::FILE_ERROR_IN_USE);

  // Allow the first write to continue. It should succeed.
  EXPECT_EQ(future.Get(), base::File::FILE_OK);
}

TEST_F(FileSystemProviderContentCacheImplTest,
       WriteBytesForNewFileShouldEvictOldestFileFirst) {
  content_cache_->SetMaxCacheItems(2);

  // Inserts file into cache with size `kDefaultChunkSize`. 1 space left.
  base::FilePath random_path1("random-path1");
  OpenedCloudFile file1 =
      WriteFileToCache(random_path1,
                       /*version_tag=*/"versionA", kDefaultChunkSize);
  content_cache_->CloseFile(file1);
  // Inserts another file into cache that is `kDefaultChunkSize` * 2. 0 spaces
  // left.
  OpenedCloudFile file2 =
      WriteFileToCache(base::FilePath("random-path2"),
                       /*version_tag=*/"versionA", kDefaultChunkSize * 2);
  content_cache_->CloseFile(file2);

  EXPECT_THAT(content_cache_->GetCachedFilePaths(),
              ElementsAre(base::FilePath("random-path2"), random_path1));

  // Expect third insertion to succeed and evict and remove the oldest file
  // (i.e. `random-path1`).
  std::unique_ptr<base::RunLoop> run_loop = CreateItemRemovedRunLoop(
      random_path1, /*bytes_removed*/ kDefaultChunkSize);
  EXPECT_CALL(content_cache_observer_, OnItemEvicted(random_path1));
  OpenedCloudFile file3 =
      WriteFileToCache(base::FilePath("random-path3"),
                       /*version_tag=*/"versionA", kDefaultChunkSize * 3);
  run_loop->Run();
}

TEST_F(FileSystemProviderContentCacheImplTest,
       ReadBytesShouldReturnNotFoundOnFirstRead) {
  OpenedCloudFile file(base::FilePath("not-in-cache"),
                       OpenFileMode::OPEN_FILE_MODE_READ, ++request_id_,
                       /*version_tag=*/"", kDefaultChunkSize);
  EXPECT_THAT(ReadBytesFromContentCache(file, /*buffer=*/nullptr,
                                        /*offset=*/0, kDefaultChunkSize),
              Pair(-1, base::File::FILE_ERROR_NOT_FOUND));
}

TEST_F(FileSystemProviderContentCacheImplTest,
       ReadBytesShouldReturnNotFoundIfInitialWriteNotFinished) {
  OpenedCloudFile file(base::FilePath("random-path"),
                       OpenFileMode::OPEN_FILE_MODE_READ, ++request_id_,
                       /*version_tag=*/"versionA", kDefaultChunkSize * 2);
  TestFuture<base::File::Error> future;
  scoped_refptr<net::IOBufferWithSize> buffer =
      InitializeBufferWithRandBytes(kDefaultChunkSize * 2);
  content_cache_->WriteBytes(file, buffer, /*offset=*/0, kDefaultChunkSize,
                             future.GetCallback());

  // This will be attempted before the write that is made above, so this should
  // fail as the first 512 byte chunk has not been written yet.
  EXPECT_THAT(ReadBytesFromContentCache(file, /*buffer=*/nullptr,
                                        /*offset=*/0, kDefaultChunkSize),
              Pair(-1, base::File::FILE_ERROR_NOT_FOUND));

  // Allow the first write to continue. It should succeed.
  EXPECT_EQ(future.Get(), base::File::FILE_OK);
}

TEST_F(FileSystemProviderContentCacheImplTest,
       ReadBytesShouldReturnNotFoundIfVersionTagMismatch) {
  // Write to cache a file with `versionA`.
  const base::FilePath fsp_path("random-path");
  WriteFileToCache(fsp_path, "versionA", kDefaultChunkSize);

  // Attempt to read from the cache the same file with `versionB`.
  OpenedCloudFile file(fsp_path, OpenFileMode::OPEN_FILE_MODE_READ,
                       ++request_id_,
                       /*version_tag=*/"versionB", kDefaultChunkSize);
  EXPECT_THAT(ReadBytesFromContentCache(file, /*buffer=*/nullptr,
                                        /*offset=*/0, kDefaultChunkSize),
              Pair(-1, base::File::FILE_ERROR_NOT_FOUND));
}

TEST_F(FileSystemProviderContentCacheImplTest,
       ReadBytesShouldSucceedIfRequestedBytesAreAtEOF) {
  const base::FilePath fsp_path("random-path");
  const std::string version_tag("versionA");
  WriteFileToCache(fsp_path, version_tag, kDefaultChunkSize);

  OpenedCloudFile file(fsp_path, OpenFileMode::OPEN_FILE_MODE_READ,
                       ++request_id_, version_tag, kDefaultChunkSize);
  scoped_refptr<net::IOBufferWithSize> buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(kDefaultChunkSize);

  // The file in the cloud has 512 bytes, however, the reader is attempting to
  // get another 512 bytes starting from 512. Readers expect the `bytes_read` to
  // return with 0 to indicate EOF, follow up requests should honor this.
  EXPECT_THAT(ReadBytesFromContentCache(file, buffer,
                                        /*offset=*/512, kDefaultChunkSize),
              Pair(0, base::File::FILE_OK));
}

TEST_F(FileSystemProviderContentCacheImplTest,
       ReadBytesShouldSucceedIfExactBytesAreAvailable) {
  const base::FilePath fsp_path("random-path");
  const std::string version_tag("versionA");
  WriteFileToCache(fsp_path, version_tag, kDefaultChunkSize);

  OpenedCloudFile file(fsp_path, OpenFileMode::OPEN_FILE_MODE_READ,
                       ++request_id_, version_tag, kDefaultChunkSize);
  scoped_refptr<net::IOBufferWithSize> buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(kDefaultChunkSize);
  EXPECT_THAT(ReadBytesFromContentCache(file, buffer,
                                        /*offset=*/0, kDefaultChunkSize),
              Pair(kDefaultChunkSize, base::File::FILE_OK));
}

TEST_F(
    FileSystemProviderContentCacheImplTest,
    ReadBytesShouldSucceedIfRequestIsForLengthThatExceedsKnownSizeOnDiskAndCloud) {
  const base::FilePath fsp_path("random-path");
  const std::string version_tag("versionA");
  WriteFileToCache(fsp_path, version_tag, /*chunk_size=*/49);

  OpenedCloudFile file(fsp_path, OpenFileMode::OPEN_FILE_MODE_READ,
                       ++request_id_, version_tag, 49);
  scoped_refptr<net::IOBufferWithSize> buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(kDefaultChunkSize);

  // First request is made for a file that is 49 bytes big but the request is
  // for `kDefaultChunkSize` instead.
  EXPECT_THAT(ReadBytesFromContentCache(file, buffer,
                                        /*offset=*/0, kDefaultChunkSize),
              Pair(49, base::File::FILE_OK));

  // The client then requests from the previous offset and the same length, we
  // want to avoid sending this to the FSP as we have all this data available.
  EXPECT_THAT(ReadBytesFromContentCache(file, buffer,
                                        /*offset=*/49, kDefaultChunkSize),
              Pair(0, base::File::FILE_OK));
}

TEST_F(FileSystemProviderContentCacheImplTest,
       ReadBytesShouldReturnNotFoundIfOnlyHalfTheFileIsOnDisk) {
  const base::FilePath fsp_path("random-path");
  const std::string version_tag("versionA");
  // Stage a file with `kDefaultChunkSize` bytes in the cache. For the purposes
  // of this test, let's assume that this file is actually of size 640 bytes
  // (i.e. we have 512 bytes in cache and 128 bytes unretrieved from the cloud,
  // possibly from a previously interrupted write to the cache).
  WriteFileToCache(fsp_path, version_tag, kDefaultChunkSize);

  OpenedCloudFile file(fsp_path, OpenFileMode::OPEN_FILE_MODE_READ,
                       ++request_id_, version_tag, kDefaultChunkSize + 128);
  scoped_refptr<net::IOBufferWithSize> buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(kDefaultChunkSize);
  // First request is made for a file that is `kDefaultChunkSize` bytes.
  EXPECT_THAT(ReadBytesFromContentCache(file, buffer,
                                        /*offset=*/0, kDefaultChunkSize),
              Pair(kDefaultChunkSize, base::File::FILE_OK));

  // The client then requests from the previous offset but the remaining length,
  // this should return false to ensure the next request must be made from the
  // underlying FSP.
  EXPECT_THAT(ReadBytesFromContentCache(file, /*buffer=*/nullptr,
                                        /*offset=*/kDefaultChunkSize, 128),
              Pair(-1, base::File::FILE_ERROR_NOT_FOUND));
}

TEST_F(FileSystemProviderContentCacheImplTest,
       ReadBytesShouldFailIfOnlyHalfTheFileIsOnDiskWithDifferentChunkSizes) {
  const base::FilePath fsp_path("random-path");
  const std::string version_tag("versionA");
  // Stage a file with 576 bytes in the cache. For the purposes
  // of this test, let's assume that this file is actually of size 640 bytes
  // (i.e. we have 576 bytes in cache and 64 bytes unretrieved from the cloud,
  // possibly from a previously interrupted write to the cache).
  WriteFileToCache(fsp_path, version_tag, 576);

  OpenedCloudFile file(fsp_path, OpenFileMode::OPEN_FILE_MODE_READ,
                       ++request_id_, version_tag, 640);
  scoped_refptr<net::IOBufferWithSize> buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(kDefaultChunkSize);
  TestFuture<int, bool, base::File::Error> future;

  // First request is made for a file that is `kDefaultChunkSize` bytes.
  EXPECT_THAT(ReadBytesFromContentCache(file, buffer,
                                        /*offset=*/0, kDefaultChunkSize),
              Pair(kDefaultChunkSize, base::File::FILE_OK));

  // The client then requests from the previous offset but the remaining length,
  // this should return true but only 64 bytes should be returned.
  EXPECT_THAT(
      ReadBytesFromContentCache(file, buffer,
                                /*offset=*/kDefaultChunkSize, /*length=*/128),
      Pair(64, base::File::FILE_OK));

  // The follow up request for the remaining bytes should return
  // `base::File::FILE_ERROR_NOT_FOUND` to indicate to the `CloudFileSystem`
  // that the request must be delegated to the underlying FSP.
  EXPECT_THAT(ReadBytesFromContentCache(file, /*buffer=*/nullptr,
                                        /*offset=*/kDefaultChunkSize + 64,
                                        /*length=*/64),
              Pair(-1, base::File::FILE_ERROR_NOT_FOUND));
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
                       ++request_id_, version_tag, kDefaultChunkSize);
  scoped_refptr<net::IOBufferWithSize> buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(kDefaultChunkSize);
  EXPECT_THAT(
      ReadBytesFromContentCache(file, buffer,
                                /*offset=*/0, /*length=*/kDefaultChunkSize),
      Pair(kDefaultChunkSize, base::File::FILE_OK));
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
                       OpenFileMode::OPEN_FILE_MODE_READ, ++request_id_,
                       /*version_tag=*/"versionA", kDefaultChunkSize);
  EXPECT_THAT(
      ReadBytesFromContentCache(file, /*buffer=*/nullptr,
                                /*offset=*/0, /*length=*/kDefaultChunkSize),
      Pair(-1, base::File::FILE_ERROR_NOT_FOUND));
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
                       ++request_id_, version_tag, kDefaultChunkSize);
  EXPECT_THAT(
      ReadBytesFromContentCache(file, /*buffer=*/nullptr,
                                /*offset=*/0, /*length=*/kDefaultChunkSize),
      Pair(-1, base::File::FILE_ERROR_NOT_FOUND));

  // Expect `std::nullopt` is returned as the item is not found.
  std::unique_ptr<std::optional<ContextDatabase::Item>> item =
      GetItemById(inserted_id);
  EXPECT_TRUE(item);
  EXPECT_FALSE(item->has_value());
}

TEST_F(FileSystemProviderContentCacheImplTest, EvictedFileRemovedUponClosing) {
  // Inserts file into cache with size `kDefaultChunkSize`.
  int64_t random_path1_size = kDefaultChunkSize;
  base::FilePath random_path1("random-path1");
  OpenedCloudFile file =
      WriteFileToCache(random_path1, "versionA", random_path1_size);

  EXPECT_CALL(content_cache_observer_, OnItemEvicted(random_path1));
  content_cache_->Evict(random_path1);

  // Close the evicted file and expect it gets removed.
  std::unique_ptr<base::RunLoop> run_loop = CreateItemRemovedRunLoop(
      random_path1, /*bytes_removed*/ random_path1_size);
  content_cache_->CloseFile(file);
  run_loop->Run();
}

TEST_F(FileSystemProviderContentCacheImplTest,
       NonEvictedFileNotRemovedUponClosing) {
  // Inserts file into cache with size `kDefaultChunkSize`.
  int64_t random_path1_size = kDefaultChunkSize;
  base::FilePath random_path1("random-path1");
  OpenedCloudFile file =
      WriteFileToCache(random_path1, "versionA", random_path1_size);

  // Close the file and expect it doesn't removed.
  EXPECT_CALL(
      content_cache_observer_,
      OnItemRemovedFromDisk(random_path1, /*bytes_removed*/ kDefaultChunkSize))
      .Times(0);
  content_cache_->CloseFile(file);
}

TEST_F(FileSystemProviderContentCacheImplTest,
       EvictingNonExistentFileDoesNothing) {
  // Attempt to evict a non-existent.
  EXPECT_CALL(content_cache_observer_, OnItemEvicted(_)).Times(0);
  EXPECT_CALL(content_cache_observer_, OnItemRemovedFromDisk(_, _)).Times(0);
  content_cache_->Evict(base::FilePath("random-path1"));
}

TEST_F(FileSystemProviderContentCacheImplTest,
       FilesRemovedUponEvictingWhenNotOpened) {
  // Inserts two files into cache.
  int64_t random_path1_size = kDefaultChunkSize;
  base::FilePath random_path1("random-path1");
  OpenedCloudFile file1 =
      WriteFileToCache(random_path1, "versionA", random_path1_size);
  content_cache_->CloseFile(file1);

  int64_t random_path2_size = kDefaultChunkSize * 2;
  base::FilePath random_path2("random-path2");
  OpenedCloudFile file2 =
      WriteFileToCache(random_path2, "versionA", random_path2_size);
  content_cache_->CloseFile(file2);

  // Expect that evicting the files will remove them.
  std::unique_ptr<base::RunLoop> run_loop1 = CreateItemRemovedRunLoop(
      random_path1, /*bytes_removed*/ random_path1_size);
  std::unique_ptr<base::RunLoop> run_loop2 = CreateItemRemovedRunLoop(
      random_path2, /*bytes_removed*/ random_path2_size);
  EXPECT_CALL(content_cache_observer_, OnItemEvicted(random_path1));
  content_cache_->Evict(random_path1);
  EXPECT_CALL(content_cache_observer_, OnItemEvicted(random_path2));
  content_cache_->Evict(random_path2);

  run_loop1->Run();
  run_loop2->Run();
}

TEST_F(FileSystemProviderContentCacheImplTest,
       FilesRemovedUponResizeWhenNotOpened) {
  // Inserts two files into cache.
  int64_t random_path1_size = kDefaultChunkSize;
  base::FilePath random_path1("random-path1");
  OpenedCloudFile file1 =
      WriteFileToCache(random_path1, "versionA", random_path1_size);
  content_cache_->CloseFile(file1);

  int64_t random_path2_size = kDefaultChunkSize * 2;
  base::FilePath random_path2("random-path2");
  OpenedCloudFile file2 =
      WriteFileToCache(random_path2, "versionA", random_path2_size);
  content_cache_->CloseFile(file2);

  // Expect that resizing the cache to size 0 will remove all the files.
  std::unique_ptr<base::RunLoop> run_loop1 = CreateItemRemovedRunLoop(
      random_path1, /*bytes_removed*/ random_path1_size);
  std::unique_ptr<base::RunLoop> run_loop2 = CreateItemRemovedRunLoop(
      random_path2, /*bytes_removed*/ random_path2_size);
  EXPECT_CALL(content_cache_observer_, OnItemEvicted(random_path1));
  EXPECT_CALL(content_cache_observer_, OnItemEvicted(random_path2));
  content_cache_->SetMaxCacheItems(0);

  run_loop1->Run();
  run_loop2->Run();
}

TEST_F(FileSystemProviderContentCacheImplTest, OldestFilesAreEvictedOnResize) {
  content_cache_->SetMaxCacheItems(3);

  // Inserts file into cache with size `kDefaultChunkSize`. 2 spaces left.
  int64_t random_path1_size = kDefaultChunkSize;
  base::FilePath random_path1("random-path1");
  OpenedCloudFile file1 =
      WriteFileToCache(random_path1, "versionA", random_path1_size);
  // Inserts another file into cache that is `kDefaultChunkSize` * 2. 1 space
  // left.
  int64_t random_path2_size = kDefaultChunkSize * 2;
  base::FilePath random_path2("random-path2");
  OpenedCloudFile file2 =
      WriteFileToCache(random_path2, "versionA", random_path2_size);
  // Inserts another file into cache that is `kDefaultChunkSize` * 4. 0 spaces
  // left.
  int64_t random_path3_size = kDefaultChunkSize * 4;
  base::FilePath random_path3("random-path3");
  OpenedCloudFile file3 =
      WriteFileToCache(random_path3, "versionA", random_path3_size);

  // Resize the cache to only have 1 spot, the `random-path1` and `random-path2`
  // entries (least-recently used) should be evicted since there are no files
  // already evicted.
  EXPECT_CALL(content_cache_observer_, OnItemEvicted(random_path1));
  EXPECT_CALL(content_cache_observer_, OnItemEvicted(random_path2));
  content_cache_->SetMaxCacheItems(1);

  // Close the evicted files and expect they get removed.
  std::unique_ptr<base::RunLoop> run_loop1 = CreateItemRemovedRunLoop(
      random_path1, /*bytes_removed*/ random_path1_size);
  content_cache_->CloseFile(file1);
  std::unique_ptr<base::RunLoop> run_loop2 = CreateItemRemovedRunLoop(
      random_path2, /*bytes_removed*/ random_path2_size);
  content_cache_->CloseFile(file2);

  // Closing the non-evicted file should not remove it.
  EXPECT_CALL(
      content_cache_observer_,
      OnItemRemovedFromDisk(random_path3, /*bytes_removed*/ random_path3_size))
      .Times(0);
  content_cache_->CloseFile(file3);

  run_loop1->Run();
  run_loop2->Run();
}

TEST_F(FileSystemProviderContentCacheImplTest,
       AlreadyEvictedFilesAndOldestFilesAreEvictedOnResize) {
  content_cache_->SetMaxCacheItems(4);

  // Inserts file into cache with size `kDefaultChunkSize`. 3 spaces left.
  int64_t random_path1_size = kDefaultChunkSize;
  base::FilePath random_path1("random-path1");
  OpenedCloudFile file1 =
      WriteFileToCache(random_path1, "versionA", random_path1_size);
  // Inserts another file into cache that is `kDefaultChunkSize` * 2. 2 spaces
  // left.
  int64_t random_path2_size = kDefaultChunkSize * 2;
  base::FilePath random_path2("random-path2");
  OpenedCloudFile file2 =
      WriteFileToCache(random_path2, "versionA", random_path2_size);
  // Inserts another file into cache that is `kDefaultChunkSize` * 3. 1 space
  // left.
  int64_t random_path3_size = kDefaultChunkSize * 2;
  base::FilePath random_path3("random-path3");
  OpenedCloudFile file3 =
      WriteFileToCache(random_path3, "versionA", random_path3_size);
  // Inserts another file into cache that is `kDefaultChunkSize` * 4. 0 spaces
  // left.
  int64_t random_path4_size = kDefaultChunkSize * 4;
  base::FilePath random_path4("random-path4");
  OpenedCloudFile file4 =
      WriteFileToCache(random_path4, "versionA", random_path4_size);

  // Evict the least and most-recently-used file.
  EXPECT_CALL(content_cache_observer_, OnItemEvicted(random_path1));
  content_cache_->Evict(random_path1);
  EXPECT_CALL(content_cache_observer_, OnItemEvicted(random_path4));
  content_cache_->Evict(random_path4);

  // Resize the cache to only have 1 spot, the `random-path4` and `random-path1`
  // entries are already evicted so only one more file (`random-path2` the
  // least-recently used and not already evicted) is evicted.
  EXPECT_CALL(content_cache_observer_, OnItemEvicted(random_path2));
  content_cache_->SetMaxCacheItems(1);

  // Close the evicted files and expect they get removed.
  std::unique_ptr<base::RunLoop> run_loop1 = CreateItemRemovedRunLoop(
      random_path1, /*bytes_removed*/ random_path1_size);
  content_cache_->CloseFile(file1);
  // Close the evicted file and expect it gets removed.
  std::unique_ptr<base::RunLoop> run_loop2 = CreateItemRemovedRunLoop(
      random_path2, /*bytes_removed*/ random_path2_size);
  content_cache_->CloseFile(file2);
  std::unique_ptr<base::RunLoop> run_loop4 = CreateItemRemovedRunLoop(
      random_path4, /*bytes_removed*/ random_path4_size);
  content_cache_->CloseFile(file4);

  // Closing the non-evicted file should not remove it.
  EXPECT_CALL(
      content_cache_observer_,
      OnItemRemovedFromDisk(random_path3, /*bytes_removed*/ random_path3_size))
      .Times(0);
  content_cache_->CloseFile(file3);

  run_loop1->Run();
  run_loop2->Run();
  run_loop4->Run();
}

TEST_F(FileSystemProviderContentCacheImplTest, EvictCanBeCalledMultipleTimes) {
  // Add 1 item to the cache via a Write FSP request. `CloseFile()` to ensure
  // the eviction+removal isn't blocked.
  OpenedCloudFile file =
      WriteFileToCache(base::FilePath("/a.txt"), "versionA", kDefaultChunkSize);
  content_cache_->CloseFile(file);

  // Run Evict twice but the file should only be evicted and removed once.
  std::unique_ptr<base::RunLoop> run_loop = CreateItemRemovedRunLoop(
      file.file_path, /*bytes_removed*/ kDefaultChunkSize);
  EXPECT_CALL(content_cache_observer_, OnItemEvicted(file.file_path));
  content_cache_->Evict(file.file_path);

  EXPECT_CALL(content_cache_observer_, OnItemEvicted(_)).Times(0);
  content_cache_->Evict(file.file_path);

  run_loop->Run();
}

TEST_F(FileSystemProviderContentCacheImplTest,
       NotifyEvictsDeletedFilesAndFilesWithDifferentVersionTagsFromCache) {
  // Insert files into cache.
  const std::string version_tagA("versionA");
  const base::FilePath fsp_path1("random-path1");
  WriteFileToCache(fsp_path1, version_tagA, kDefaultChunkSize);
  const base::FilePath fsp_path2("random-path2");
  WriteFileToCache(fsp_path2, version_tagA, kDefaultChunkSize);
  const base::FilePath fsp_path3("random-path3");
  WriteFileToCache(fsp_path3, version_tagA, kDefaultChunkSize);

  // The files now exists in the cache.
  EXPECT_THAT(content_cache_->GetCachedFilePaths(),
              ElementsAre(fsp_path3, fsp_path2, fsp_path1));

  auto changes = std::make_unique<ProvidedFileSystemObserver::Changes>();
  // Deleted file.
  changes->emplace_back(
      fsp_path1, storage::WatcherManager::ChangeType::DELETED,
      std::make_unique<ash::file_system_provider::CloudFileInfo>(version_tagA));
  // Regular file. This change will be ignored.
  changes->emplace_back(
      fsp_path1, storage::WatcherManager::ChangeType::CHANGED,
      std::make_unique<ash::file_system_provider::CloudFileInfo>(version_tagA));
  // File with different version tag.
  changes->emplace_back(
      fsp_path3, storage::WatcherManager::ChangeType::CHANGED,
      std::make_unique<ash::file_system_provider::CloudFileInfo>("versionB"));

  // Notify of the file changes.
  EXPECT_CALL(content_cache_observer_, OnItemEvicted(fsp_path1));
  EXPECT_CALL(content_cache_observer_, OnItemEvicted(fsp_path3));
  content_cache_->Notify(*changes);

  // The deleted file and file with different version tag are now evicted from
  // the cache.
  EXPECT_THAT(content_cache_->GetCachedFilePaths(), ElementsAre(fsp_path2));
}

TEST_F(FileSystemProviderContentCacheImplTest,
       ObservedVersionTagEvictsFileWithDifferentVersionTagFromCache) {
  // Insert file into cache.
  const base::FilePath fsp_path("random-path1");
  WriteFileToCache(fsp_path, "versionA", kDefaultChunkSize);

  // The file now exists in the cache.
  EXPECT_THAT(content_cache_->GetCachedFilePaths(), ElementsAre(fsp_path));

  // Expect that file is deleted on observing that the version changed.
  EXPECT_CALL(content_cache_observer_, OnItemEvicted(fsp_path));
  content_cache_->ObservedVersionTag(fsp_path, "versionB");

  EXPECT_EQ(content_cache_->GetCachedFilePaths().size(), 0U);
}

TEST_F(FileSystemProviderContentCacheImplTest,
       ExistingRequestsShouldSucceedWhenFileEvictedButNotRemoved) {
  // Initiate write and read requests for a file.
  const base::FilePath fsp_path("random-path");
  const std::string version_tag("versionA");
  OpenedCloudFile write_file =
      WriteFileToCache(fsp_path, version_tag, kDefaultChunkSize);
  OpenedCloudFile read_file =
      ReadFileFromCache(fsp_path, version_tag, kDefaultChunkSize);

  // The file now exists in the cache.
  EXPECT_THAT(content_cache_->GetCachedFilePaths(), ElementsAre(fsp_path));

  EXPECT_CALL(content_cache_observer_, OnItemEvicted(fsp_path));
  content_cache_->Evict(fsp_path);

  // The file is now evicted from the cache.
  EXPECT_EQ(content_cache_->GetCachedFilePaths().size(), 0U);

  // Can still make write and read requests with the same request id.
  scoped_refptr<net::IOBufferWithSize> write_buffer =
      InitializeBufferWithRandBytes(kDefaultChunkSize);
  EXPECT_EQ(
      WriteBytesToContentCache(write_file, write_buffer,
                               /*offset=*/kDefaultChunkSize, kDefaultChunkSize),
      base::File::FILE_OK);
  scoped_refptr<net::IOBufferWithSize> read_buffer =
      InitializeBufferWithRandBytes(kDefaultChunkSize);
  EXPECT_THAT(ReadBytesFromContentCache(read_file, read_buffer,
                                        /*offset=*/kDefaultChunkSize,
                                        kDefaultChunkSize),
              Pair(kDefaultChunkSize, base::File::FILE_OK));

  // Close the write request. This does not remove the file.
  content_cache_->CloseFile(write_file);

  // Can still make read requests with the same request id.
  EXPECT_THAT(ReadBytesFromContentCache(read_file, read_buffer,
                                        /*offset=*/kDefaultChunkSize,
                                        kDefaultChunkSize),
              Pair(kDefaultChunkSize, base::File::FILE_OK));

  // Close the read request. This removes the evicted file since there are no
  // more existing requests.
  std::unique_ptr<base::RunLoop> run_loop = CreateItemRemovedRunLoop(
      fsp_path, /*bytes_removed*/ kDefaultChunkSize * 2);
  content_cache_->CloseFile(read_file);
  run_loop->Run();
}

TEST_F(FileSystemProviderContentCacheImplTest,
       NewRequestsShouldFailWhenFileEvictedButNotRemoved) {
  // Initiate write request for a file.
  const base::FilePath fsp_path("random-path");
  const std::string version_tag("versionA");
  OpenedCloudFile write_file1 =
      WriteFileToCache(fsp_path, version_tag, kDefaultChunkSize);

  // The file now exists in the cache.
  EXPECT_THAT(content_cache_->GetCachedFilePaths(), ElementsAre(fsp_path));

  EXPECT_CALL(content_cache_observer_, OnItemEvicted(fsp_path));
  content_cache_->Evict(fsp_path);

  // The file is now evicted from the cache.
  EXPECT_EQ(content_cache_->GetCachedFilePaths().size(), 0U);

  // Cannot make new write or read requests with a new request id when the file
  // is in its evicted state.
  OpenedCloudFile write_file2(fsp_path, OpenFileMode::OPEN_FILE_MODE_READ,
                              ++request_id_, version_tag, kDefaultChunkSize);
  scoped_refptr<net::IOBufferWithSize> buffer =
      InitializeBufferWithRandBytes(kDefaultChunkSize);
  EXPECT_EQ(WriteBytesToContentCache(write_file2, buffer,
                                     /*offset=*/0, kDefaultChunkSize),
            base::File::FILE_ERROR_INVALID_OPERATION);
  EXPECT_EQ(
      WriteBytesToContentCache(write_file2, buffer,
                               /*offset=*/kDefaultChunkSize, kDefaultChunkSize),
      base::File::FILE_ERROR_NOT_FOUND);
  OpenedCloudFile read_file(fsp_path, OpenFileMode::OPEN_FILE_MODE_READ,
                            ++request_id_, version_tag, kDefaultChunkSize);

  EXPECT_THAT(ReadBytesFromContentCache(read_file, /*buffer=*/nullptr,
                                        /*offset=*/0, kDefaultChunkSize),
              Pair(-1, base::File::FILE_ERROR_NOT_FOUND));

  // No new replacement file is added to the cache.
  EXPECT_EQ(content_cache_->GetCachedFilePaths().size(), 0U);

  // Close original write request and expect file gets removed.
  std::unique_ptr<base::RunLoop> run_loop =
      CreateItemRemovedRunLoop(fsp_path, /*bytes_removed*/ kDefaultChunkSize);
  content_cache_->CloseFile(write_file1);
  run_loop->Run();

  // Now can write to and read from the file with new request ids.
  EXPECT_EQ(WriteBytesToContentCache(write_file2, buffer,
                                     /*offset=*/0, kDefaultChunkSize),
            base::File::FILE_OK);
  EXPECT_EQ(
      WriteBytesToContentCache(write_file2, buffer,
                               /*offset=*/kDefaultChunkSize, kDefaultChunkSize),
      base::File::FILE_OK);
  scoped_refptr<net::IOBufferWithSize> read_buffer =
      InitializeBufferWithRandBytes(kDefaultChunkSize);
  EXPECT_THAT(ReadBytesFromContentCache(read_file, read_buffer,
                                        /*offset=*/kDefaultChunkSize,
                                        kDefaultChunkSize),
              Pair(kDefaultChunkSize, base::File::FILE_OK));

  // The file exists in the cache again.
  EXPECT_THAT(content_cache_->GetCachedFilePaths(), ElementsAre(fsp_path));
}

}  // namespace
}  // namespace ash::file_system_provider
