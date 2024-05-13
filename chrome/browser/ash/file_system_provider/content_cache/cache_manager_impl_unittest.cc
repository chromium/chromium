// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/content_cache/cache_manager_impl.h"

#include "base/base64.h"
#include "base/files/file_error_or.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chromeos/ash/components/dbus/spaced/fake_spaced_client.h"
#include "chromeos/ash/components/dbus/spaced/spaced_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::file_system_provider {

using base::FilePath;
using base::test::RunClosure;
using base::test::TestFuture;
using FileErrorOrContentCache =
    base::FileErrorOr<std::unique_ptr<ContentCache>>;
using testing::AllOf;
using testing::Field;
using testing::IsFalse;
using testing::IsTrue;
using testing::Property;

const ProvidedFileSystemInfo kFileSystemInfo(
    "abc",
    MountOptions("fsp_id", "display name"),
    base::FilePath("/file_system/fsp_id"),
    /*configurable=*/false,
    /*watchable=*/true,
    extensions::SOURCE_FILE,
    IconSet());
const ProvidedFileSystemInfo kEmptyFileSystemInfo("",
                                                  MountOptions("", ""),
                                                  base::FilePath(),
                                                  /*configurable=*/false,
                                                  /*watchable=*/true,
                                                  extensions::SOURCE_FILE,
                                                  IconSet());

class MockCacheManagerObserver : public CacheManager::Observer {
 public:
  MOCK_METHOD(void,
              OnProviderUninitialized,
              (const base::FilePath base64_encoded_provider_folder_name,
               base::File::Error result),
              (override));
};

class FileSystemProviderCacheManagerImplTest : public testing::Test {
 protected:
  FileSystemProviderCacheManagerImplTest() = default;
  ~FileSystemProviderCacheManagerImplTest() override = default;

  void SetUp() override {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    profile_dir_ = temp_dir_.GetPath();

    ash::SpacedClient::InitializeFake();
  }

  void TearDown() override { ash::SpacedClient::Shutdown(); }

  const base::FilePath GetProviderMountPath(const std::string& fsp_id) {
    return profile_dir_.Append(kFspContentCacheDirName)
        .Append(base::Base64Encode(fsp_id));
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  base::FilePath profile_dir_;
};

TEST_F(FileSystemProviderCacheManagerImplTest,
       EmptyProviderIdFailsInitialization) {
  CacheManagerImpl cache_manager(profile_dir_);
  TestFuture<FileErrorOrContentCache> future;
  cache_manager.InitializeForProvider(kEmptyFileSystemInfo,
                                      future.GetCallback());
  EXPECT_THAT(future.Get(), Property(&FileErrorOrContentCache::error,
                                     base::File::FILE_ERROR_INVALID_URL));
  EXPECT_FALSE(base::PathExists(profile_dir_.Append(kFspContentCacheDirName)));
  EXPECT_FALSE(cache_manager.IsProviderInitialized(kEmptyFileSystemInfo));
}

TEST_F(FileSystemProviderCacheManagerImplTest,
       FspProviderIdCreatedOnInitializationAndDeletedOnUninitialization) {
  CacheManagerImpl cache_manager(profile_dir_);
  MockCacheManagerObserver observer;
  cache_manager.AddObserver(&observer);
  TestFuture<FileErrorOrContentCache> future;
  // Expect successful initialization.
  cache_manager.InitializeForProvider(kFileSystemInfo, future.GetCallback());
  EXPECT_THAT(future.Get(),
              Property(&FileErrorOrContentCache::has_value, IsTrue()));
  EXPECT_TRUE(base::PathExists(GetProviderMountPath("fsp_id")));
  EXPECT_TRUE(cache_manager.IsProviderInitialized(kFileSystemInfo));

  // Expect successful uninitialization.
  base::RunLoop run_loop;
  EXPECT_CALL(observer, OnProviderUninitialized(
                            base::FilePath(base::Base64Encode("fsp_id")),
                            base::File::FILE_OK))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  cache_manager.UninitializeForProvider(kFileSystemInfo);
  run_loop.Run();
  EXPECT_FALSE(base::PathExists(GetProviderMountPath("fsp_id")));
  EXPECT_FALSE(cache_manager.IsProviderInitialized(kFileSystemInfo));
}

TEST_F(FileSystemProviderCacheManagerImplTest,
       NonExistantFspProviderHasUnsucessfulUninitialization) {
  CacheManagerImpl cache_manager(profile_dir_);
  MockCacheManagerObserver observer;
  cache_manager.AddObserver(&observer);

  EXPECT_FALSE(cache_manager.IsProviderInitialized(kFileSystemInfo));

  // Expect unsuccessful uninitialization of a non-existent provider.
  EXPECT_CALL(observer, OnProviderUninitialized(
                            base::FilePath(base::Base64Encode("fsp_id")),
                            base::File::FILE_ERROR_NOT_FOUND));
  cache_manager.UninitializeForProvider(kFileSystemInfo);
}

TEST_F(FileSystemProviderCacheManagerImplTest, OnSpaceUpdate) {
  FakeSpacedClient::Get()->set_connected(true);
  CacheManagerImpl cache_manager(profile_dir_);
  MockCacheManagerObserver observer;
  cache_manager.AddObserver(&observer);
  TestFuture<FileErrorOrContentCache> first_cache_future;
  // Expect successful initialization.
  cache_manager.InitializeForProvider(kFileSystemInfo,
                                      first_cache_future.GetCallback());
  EXPECT_THAT(first_cache_future.Get(),
              Property(&FileErrorOrContentCache::has_value, IsTrue()));

  ash::FakeSpacedClient* spaced_client = ash::FakeSpacedClient::Get();
  using Size = ContentCache::SizeInfo;
  ash::SpacedClient::Observer::SpaceEvent event;

  // Initial free space bytes event sets the max bytes on disk for the content
  // cache, capped up to 5GB.
  event.set_free_space_bytes(int64_t(10) << 30);
  cache_manager.OnSpaceUpdate(event);
  const std::unique_ptr<ContentCache>& first_cache =
      first_cache_future.Get().value();
  EXPECT_THAT(first_cache->GetSize(),
              Field(&Size::max_bytes_on_disk, kMaxAllowedCacheSizeInBytes));

  // When the free space drops, the max size is appropriately reduced.
  event.set_free_space_bytes(int64_t(5) << 30);
  cache_manager.OnSpaceUpdate(event);
  EXPECT_THAT(first_cache->GetSize(),
              Field(&Size::max_bytes_on_disk, int64_t(3) << 30));

  // When the free space increases, it will max out at 5GB.
  event.set_free_space_bytes(int64_t(100) << 30);
  cache_manager.OnSpaceUpdate(event);
  EXPECT_THAT(first_cache->GetSize(),
              Field(&Size::max_bytes_on_disk, kMaxAllowedCacheSizeInBytes));

  TestFuture<FileErrorOrContentCache> second_cache_future;
  auto new_file_system = ProvidedFileSystemInfo(
      "second_fsp", MountOptions("fsp_id_2", "display name"),
      base::FilePath("/file_system/fsp_id_2"),
      /*configurable=*/false,
      /*watchable=*/true, extensions::SOURCE_FILE, IconSet());
  spaced_client->set_free_disk_space(int64_t(100) << 30);
  cache_manager.InitializeForProvider(new_file_system,
                                      second_cache_future.GetCallback());
  EXPECT_THAT(second_cache_future.Get(),
              Property(&FileErrorOrContentCache::has_value, IsTrue()));

  // The last update was 100GB, so we expect the max cache size to be 5GB for
  // both caches.
  const std::unique_ptr<ContentCache>& second_cache =
      second_cache_future.Get().value();
  EXPECT_THAT(first_cache->GetSize(),
              Field(&Size::max_bytes_on_disk, kMaxAllowedCacheSizeInBytes));
  EXPECT_THAT(second_cache->GetSize(),
              Field(&Size::max_bytes_on_disk, kMaxAllowedCacheSizeInBytes));

  // Update the free space to 5GB and the free space for both the caches should
  // be 1.5GB for each.
  event.set_free_space_bytes(int64_t(5) << 30);
  cache_manager.OnSpaceUpdate(event);
  spaced_client->set_free_disk_space(int64_t(5) << 30);
  EXPECT_THAT(first_cache->GetSize(),
              Field(&Size::max_bytes_on_disk, int64_t(1536) << 20));
  EXPECT_THAT(second_cache->GetSize(),
              Field(&Size::max_bytes_on_disk, int64_t(1536) << 20));

  // Uninitializing a provider should result in a resize of the existing cache.
  base::RunLoop run_loop;
  EXPECT_CALL(observer, OnProviderUninitialized(
                            base::FilePath(base::Base64Encode("fsp_id")),
                            base::File::FILE_OK))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  cache_manager.UninitializeForProvider(kFileSystemInfo);
  run_loop.Run();
  EXPECT_FALSE(cache_manager.IsProviderInitialized(kFileSystemInfo));
  EXPECT_THAT(second_cache->GetSize(),
              Field(&Size::max_bytes_on_disk, int64_t(3) << 30));
}

}  // namespace ash::file_system_provider
