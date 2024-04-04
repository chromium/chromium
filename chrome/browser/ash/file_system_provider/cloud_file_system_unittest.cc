// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/cloud_file_system.h"

#include "base/base64.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/file_system_provider/content_cache/cache_manager.h"
#include "chrome/browser/ash/file_system_provider/content_cache/content_cache.h"
#include "chrome/browser/ash/file_system_provider/fake_provided_file_system.h"
#include "chrome/browser/ash/file_system_provider/mount_path_util.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_interface.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/io_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::file_system_provider {
namespace {

using base::test::IsNotNullCallback;
using base::test::RunOnceCallback;
using base::test::TestFuture;
using testing::_;
using testing::Field;
using testing::IsEmpty;
using testing::IsTrue;
using testing::Return;

using OpenFileFuture =
    TestFuture<int, base::File::Error, std::unique_ptr<CloudFileInfo>>;
using ReadFileFuture = TestFuture<int, bool, base::File::Error>;
using FileErrorFuture = TestFuture<base::File::Error>;

const char kExtensionId[] = "mbflcebpggnecokmikipoihdbecnjfoj";
const char kFileSystemId[] = "cloud-fs-id";
const char kDisplayName[] = "Cloud FS";

class MockCacheManager : public CacheManager {
 public:
  MOCK_METHOD(void,
              InitializeForProvider,
              (const ProvidedFileSystemInfo& file_system_info,
               FileErrorOrContentCacheCallback callback),
              (override));
  MOCK_METHOD(void,
              UninitializeForProvider,
              (const ProvidedFileSystemInfo& file_system_info),
              (override));
  MOCK_METHOD(void, AddObserver, (Observer * observer), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer * observer), (override));
};

class MockContentCache : public ContentCache {
 public:
  MOCK_METHOD(bool,
              StartReadBytes,
              (const OpenedCloudFile& file,
               net::IOBuffer* buffer,
               int64_t offset,
               int length,
               ProvidedFileSystemInterface::ReadChunkReceivedCallback callback),
              (override));
  MOCK_METHOD(bool,
              StartWriteBytes,
              (const OpenedCloudFile& file,
               net::IOBuffer* buffer,
               int64_t offset,
               int length,
               FileErrorCallback callback),
              (override));
};

class FileSystemProviderCloudFileSystemTest : public testing::Test,
                                              public CacheManager::Observer {
 protected:
  FileSystemProviderCloudFileSystemTest() = default;
  ~FileSystemProviderCloudFileSystemTest() override = default;

  void SetUp() override { profile_ = std::make_unique<TestingProfile>(); }

  const ProvidedFileSystemInfo GetFileSystemInfo(bool with_mock_cache_manager) {
    MountOptions mount_options;
    mount_options.file_system_id = kFileSystemId;
    mount_options.display_name = kDisplayName;
    mount_options.supports_notify_tag = true;
    mount_options.writable = true;
    const base::FilePath mount_path = util::GetMountPath(
        profile_.get(), ProviderId::CreateFromExtensionId(kExtensionId),
        kFileSystemId);
    return ProvidedFileSystemInfo(
        kExtensionId, mount_options, mount_path,
        /*configurable=*/false,
        /*watchable=*/true, extensions::SOURCE_NETWORK, IconSet(),
        with_mock_cache_manager ? CacheType::LRU : CacheType::NONE);
  }

  // Creates a CloudFileSystem which wraps a FakeProvidedFileSystem.
  std::unique_ptr<CloudFileSystem> CreateCloudFileSystem(
      bool with_mock_cache_manager) {
    std::unique_ptr<FakeProvidedFileSystem> provided_file_system =
        std::make_unique<FakeProvidedFileSystem>(
            GetFileSystemInfo(with_mock_cache_manager));
    // Start the CloudFileSystem initialisation.
    std::unique_ptr<CloudFileSystem> cloud_file_system =
        std::make_unique<CloudFileSystem>(
            std::move(provided_file_system),
            with_mock_cache_manager ? &mock_cache_manager_ : nullptr);
    return cloud_file_system;
  }

  MockCacheManager mock_cache_manager_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
};

// Test that there always exists a self-added recursive watcher on root when
// there is a CacheManager.
TEST_F(FileSystemProviderCloudFileSystemTest,
       WatcherOnRootIsAddedWhenCacheManagerExists) {
  EXPECT_CALL(mock_cache_manager_,
              InitializeForProvider(
                  GetFileSystemInfo(/*with_mock_cache_manager=*/true), _))
      .Times(1);
  std::unique_ptr<CloudFileSystem> cloud_file_system =
      CreateCloudFileSystem(/*with_mock_cache_manager=*/true);

  // Expect recursive root watcher added.
  EXPECT_THAT(cloud_file_system->GetWatchers(),
              Pointee(ElementsAre(Pair(
                  _, AllOf(Field(&Watcher::entry_path, base::FilePath("/")),
                           Field(&Watcher::recursive, IsTrue()))))));
}

// Test that there is not a recursive watcher on root when there isn't a
// CacheManager.
TEST_F(FileSystemProviderCloudFileSystemTest,
       WatcherOnRootIsNotAddedWhenCacheManagerDoesNotExist) {
  EXPECT_CALL(mock_cache_manager_, InitializeForProvider(_, _)).Times(0);
  std::unique_ptr<CloudFileSystem> cloud_file_system =
      CreateCloudFileSystem(/*with_mock_cache_manager=*/false);

  // Expect no watchers are added.
  EXPECT_THAT(cloud_file_system->GetWatchers(), Pointee(IsEmpty()));
}

TEST_F(FileSystemProviderCloudFileSystemTest, FirstReadFileWritesToCache) {
  std::unique_ptr<MockContentCache> mock_content_cache =
      std::make_unique<MockContentCache>();
  MockContentCache* cache_ptr = mock_content_cache.get();
  EXPECT_CALL(mock_cache_manager_,
              InitializeForProvider(_, IsNotNullCallback()))
      .WillOnce(RunOnceCallback<1>(std::move(mock_content_cache)));
  std::unique_ptr<CloudFileSystem> cloud_file_system =
      CreateCloudFileSystem(/*with_mock_cache_manager=*/true);

  // Open the `kFakeFilePath` file to stage it in the `FakeProvidedFileSystem`.
  OpenFileFuture open_file_future;
  cloud_file_system->OpenFile(base::FilePath(kFakeFilePath),
                              OPEN_FILE_MODE_READ,
                              open_file_future.GetCallback());
  EXPECT_EQ(open_file_future.Get<base::File::Error>(), base::File::FILE_OK);
  int file_handle = open_file_future.Get<int>();

  scoped_refptr<net::IOBuffer> buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(1);

  // Set the first read bytes to return false, this indicates that the data is
  // not cached in the content cache.
  EXPECT_CALL(*cache_ptr, StartReadBytes(_, buffer.get(), /*offset=*/0,
                                         /*length=*/1, IsNotNullCallback()))
      .WillOnce(Return(false));

  // Set the first write bytes to return successfully, this indicates the post
  // FSP stream to disk succeeded.
  EXPECT_CALL(*cache_ptr, StartWriteBytes(_, buffer.get(), /*offset=*/0,
                                          /*length=*/1, IsNotNullCallback()))
      .WillOnce([](const OpenedCloudFile& file, net::IOBuffer* buffer,
                   int64_t offset, int length, FileErrorCallback callback) {
        std::move(callback).Run(base::File::FILE_OK);
        return true;
      });

  // Read the file and assert that is works fine.
  ReadFileFuture read_file_future;
  cloud_file_system->ReadFile(file_handle, buffer.get(), /*offset=*/0,
                              /*length=*/1,
                              read_file_future.GetRepeatingCallback());
  EXPECT_EQ(read_file_future.Get<int>(), 1);
  EXPECT_EQ(read_file_future.Get<base::File::Error>(), base::File::FILE_OK);

  // Close the file and assert that it successfully closed.
  FileErrorFuture close_file_future;
  cloud_file_system->CloseFile(file_handle, close_file_future.GetCallback());
  EXPECT_EQ(close_file_future.Get(), base::File::FILE_OK);
}

}  // namespace
}  // namespace ash::file_system_provider
