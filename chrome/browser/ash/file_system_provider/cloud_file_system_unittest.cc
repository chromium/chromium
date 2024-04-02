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

class MockCacheManager : public CacheManager {
 public:
  MOCK_METHOD(void,
              InitializeForProvider,
              (const base::FilePath& provider_folder_name,
               FileErrorOrContentCacheCallback callback),
              (override));
  MOCK_METHOD(void,
              UninitializeForProvider,
              (const base::FilePath& provider_folder_name),
              (override));
  MOCK_METHOD(void, AddObserver, (Observer * observer), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer * observer), (override));
};

class FileSystemProviderCloudFileSystemTest : public testing::Test,
                                              public CacheManager::Observer {
 protected:
  FileSystemProviderCloudFileSystemTest() = default;
  ~FileSystemProviderCloudFileSystemTest() override = default;

  void SetUp() override { profile_ = std::make_unique<TestingProfile>(); }

  const base::FilePath GetProviderMountPath() {
    return util::GetMountPath(profile_.get(),
                              ProviderId::CreateFromExtensionId(kExtensionId),
                              kFileSystemId);
  }

  // Creates a CloudFileSystem which wraps a FakeProvidedFileSystem.
  std::unique_ptr<CloudFileSystem> CreateCloudFileSystem(
      bool with_mock_cache_manager) {
    MountOptions mount_options;
    mount_options.file_system_id = kFileSystemId;
    mount_options.display_name = kDisplayName;
    mount_options.supports_notify_tag = true;
    mount_options.writable = true;
    std::unique_ptr<ProvidedFileSystemInfo> file_system_info =
        std::make_unique<ProvidedFileSystemInfo>(
            kExtensionId, mount_options, GetProviderMountPath(),
            /*configurable=*/false,
            /*watchable=*/true, extensions::SOURCE_NETWORK, IconSet(),
            with_mock_cache_manager ? CacheType::LRU : CacheType::NONE);
    std::unique_ptr<FakeProvidedFileSystem> provided_file_system =
        std::make_unique<FakeProvidedFileSystem>(*file_system_info.get());
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
              InitializeForProvider(GetProviderMountPath().BaseName(), _))
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

}  // namespace
}  // namespace ash::file_system_provider
