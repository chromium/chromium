// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/content_cache/content_cache_impl.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/rand_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/file_system_provider/content_cache/context_database.h"
#include "chrome/browser/ash/file_system_provider/opened_cloud_file.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_interface.h"
#include "net/base/io_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::file_system_provider {
namespace {

using base::test::TestFuture;

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
    BoundContextDatabase db(
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
             base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}),
        std::move(context_db));
    TestFuture<bool> future;
    db.AsyncCall(&ContextDatabase::Initialize).Then(future.GetCallback());
    EXPECT_TRUE(future.Get());

    content_cache_ =
        std::make_unique<ContentCacheImpl>(temp_dir_.GetPath(), std::move(db));
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

  std::unique_ptr<ContentCache> content_cache_;
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

}  // namespace
}  // namespace ash::file_system_provider
