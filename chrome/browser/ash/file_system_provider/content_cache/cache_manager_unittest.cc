// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/content_cache/cache_manager.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::file_system_provider {
namespace {

using base::FilePath;
using base::test::TestFuture;
using FileError = base::File::Error;

class FileSystemProviderCacheManagerTest : public testing::Test {
 protected:
  FileSystemProviderCacheManagerTest() = default;
  ~FileSystemProviderCacheManagerTest() override = default;

  void SetUp() override {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    profile_dir_ = temp_dir_.GetPath();
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  base::FilePath profile_dir_;
};

TEST_F(FileSystemProviderCacheManagerTest, EmptyProviderIdFailsInitialization) {
  CacheManager cache_manager(profile_dir_);
  TestFuture<FileError> future;
  cache_manager.InitializeForProvider(FilePath(""), future.GetCallback());
  EXPECT_EQ(future.Get(), FileError::FILE_ERROR_INVALID_URL);
  EXPECT_FALSE(base::PathExists(profile_dir_.Append(kFspContentCacheDirName)));
}

TEST_F(FileSystemProviderCacheManagerTest,
       FspProviderIdCreatedOnInitialization) {
  CacheManager cache_manager(profile_dir_);
  TestFuture<FileError> future;
  cache_manager.InitializeForProvider(FilePath("fsp_id"), future.GetCallback());
  EXPECT_EQ(future.Get(), FileError::FILE_OK);
  EXPECT_TRUE(base::PathExists(
      profile_dir_.Append(kFspContentCacheDirName).Append("fsp_id")));
}

}  // namespace
}  // namespace ash::file_system_provider
