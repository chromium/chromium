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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::file_system_provider {
namespace {

using base::FilePath;
using base::test::RunClosure;
using base::test::TestFuture;
using FileErrorOrContentCache =
    base::FileErrorOr<std::unique_ptr<ContentCache>>;
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
  }

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

}  // namespace
}  // namespace ash::file_system_provider
