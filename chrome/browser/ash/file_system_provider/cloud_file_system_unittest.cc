// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/cloud_file_system.h"

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/file_system_provider/content_cache/cache_manager.h"
#include "chrome/browser/ash/file_system_provider/fake_provided_file_system.h"
#include "chrome/browser/ash/file_system_provider/mount_path_util.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Field;
using testing::IsEmpty;
using testing::IsTrue;

namespace ash::file_system_provider {
namespace {
const char kExtensionId[] = "mbflcebpggnecokmikipoihdbecnjfoj";
const char kFileSystemId[] = "cloud-fs-id";
const char kDisplayName[] = "Cloud FS";
const base::FilePath kTestFilePath1 = base::FilePath("/test.txt");

class MockCacheManagerObserver : public CacheManager::Observer {
 public:
  MOCK_METHOD(void,
              OnContentCacheInitializeComplete,
              (const base::FilePath mount_path, base::File::Error result),
              (override));
};

class FileSystemProviderCloudFileSystemTest : public testing::Test,
                                              public CacheManager::Observer {
 protected:
  FileSystemProviderCloudFileSystemTest() = default;
  ~FileSystemProviderCloudFileSystemTest() override = default;

  void SetUp() override { profile_ = std::make_unique<TestingProfile>(); }

  // Creates a CloudFileSystem which wraps a FakeProvidedFileSystem. If
  // `with_cache_manager` is true, wait until the CloudFileSystem's content
  // cache has been initialized.
  std::unique_ptr<CloudFileSystem> CreateCloudFileSystem(
      bool with_cache_manager) {
    base::FilePath mount_path = util::GetMountPath(
        profile_.get(), ProviderId::CreateFromExtensionId(kExtensionId),
        kFileSystemId);
    MountOptions mount_options;
    mount_options.file_system_id = kFileSystemId;
    mount_options.display_name = kDisplayName;
    mount_options.supports_notify_tag = true;
    mount_options.writable = true;
    std::unique_ptr<ProvidedFileSystemInfo> file_system_info =
        std::make_unique<ProvidedFileSystemInfo>(
            kExtensionId, mount_options, mount_path, false /*configurable=*/,
            true /* watchable */, extensions::SOURCE_NETWORK, IconSet(),
            with_cache_manager ? CacheType::LRU : CacheType::NONE);
    std::unique_ptr<FakeProvidedFileSystem> provided_file_system =
        std::make_unique<FakeProvidedFileSystem>(*file_system_info.get());
    std::unique_ptr<CacheManager> cache_manager =
        std::make_unique<CacheManager>(profile_->GetPath());
    // Observe the CacheManager.
    MockCacheManagerObserver observer;
    cache_manager->AddObserver(&observer);
    // Start the CloudFileSystem initialisation.
    std::unique_ptr<CloudFileSystem> cloud_file_system =
        std::make_unique<CloudFileSystem>(
            std::move(provided_file_system),
            with_cache_manager ? cache_manager.get() : nullptr);
    // Wait until the CloudFileSystem content cache has been initialised.
    if (with_cache_manager) {
      base::RunLoop run_loop;
      EXPECT_CALL(observer, OnContentCacheInitializeComplete(
                                mount_path.BaseName(), base::File::FILE_OK))
          .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
      run_loop.Run();
    }
    return cloud_file_system;
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
};

// Test that there always exists a self-added recursive watcher on root when
// there is a CacheManager.
TEST_F(FileSystemProviderCloudFileSystemTest,
       WatcherOnRootIsAddedWhenCacheManagerExists) {
  std::unique_ptr<CloudFileSystem> cloud_file_system =
      CreateCloudFileSystem(/*with_cache_manager=*/true);

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
  std::unique_ptr<CloudFileSystem> cloud_file_system =
      CreateCloudFileSystem(/*with_cache_manager=*/false);

  // Expect no watchers are added.
  EXPECT_THAT(cloud_file_system->GetWatchers(), Pointee(IsEmpty()));
}

// Tests that the first operation ID is created upon file open and can be used
// to close the file.
TEST_F(FileSystemProviderCloudFileSystemTest, OperationId) {
  std::unique_ptr<CloudFileSystem> cloud_file_system =
      CreateCloudFileSystem(/*with_content_cache=*/true);

  // Create a test file.
  using base::test::TestFuture;
  TestFuture<base::File::Error> create_file_future;
  cloud_file_system->CreateFile(kTestFilePath1,
                                create_file_future.GetCallback());
  EXPECT_EQ(create_file_future.Get(), base::File::FILE_OK);

  // The first operation ID generated should be 1.
  int expected_operation_id = 1;

  // Wait for test file to open successfully.
  TestFuture<int, base::File::Error> open_file_future;
  cloud_file_system->OpenFile(kTestFilePath1, OPEN_FILE_MODE_READ,
                              open_file_future.GetCallback());
  EXPECT_EQ(open_file_future.Get<0>(), expected_operation_id);
  EXPECT_EQ(open_file_future.Get<1>(), base::File::FILE_OK);

  // Attempt to close the file with the operation ID.
  TestFuture<base::File::Error> close_file_future;
  cloud_file_system->CloseFile(expected_operation_id,
                               close_file_future.GetCallback());
  EXPECT_EQ(close_file_future.Get(), base::File::FILE_OK);
}

}  // namespace
}  // namespace ash::file_system_provider
