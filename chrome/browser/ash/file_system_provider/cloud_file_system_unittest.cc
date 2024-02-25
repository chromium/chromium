// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/cloud_file_system.h"

#include "base/files/file_path.h"
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

class FileSystemProviderCloudFileSystemTest : public testing::Test {
 protected:
  FileSystemProviderCloudFileSystemTest() = default;
  ~FileSystemProviderCloudFileSystemTest() override = default;

  void SetUp() override { profile_ = std::make_unique<TestingProfile>(); }

  std::unique_ptr<CloudFileSystem> CreateCloudFileSystem(
      bool with_content_cache) {
    const base::FilePath mount_path = util::GetMountPath(
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
            with_content_cache ? CacheType::LRU : CacheType::NONE);
    std::unique_ptr<ProvidedFileSystemInterface> provided_file_system =
        std::make_unique<FakeProvidedFileSystem>(*file_system_info.get());
    std::unique_ptr<ContentCache> content_cache =
        std::make_unique<ContentCache>();
    return std::make_unique<CloudFileSystem>(
        std::move(provided_file_system),
        with_content_cache ? content_cache.get() : nullptr);
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
};

// Test that there always exists a self-added recursive watcher on root when
// there is a ContentCache.
TEST_F(FileSystemProviderCloudFileSystemTest,
       WatcherOnRootIsAddedWhenContentCacheExists) {
  std::unique_ptr<CloudFileSystem> cloud_file_system =
      CreateCloudFileSystem(/*with_content_cache=*/true);

  // Expect recursive root watcher added.
  EXPECT_THAT(cloud_file_system->GetWatchers(),
              Pointee(ElementsAre(Pair(
                  _, AllOf(Field(&Watcher::entry_path, base::FilePath("/")),
                           Field(&Watcher::recursive, IsTrue()))))));
}

// Test that there is not a recursive watcher on root when there isn't a
// ContentCache.
TEST_F(FileSystemProviderCloudFileSystemTest,
       WatcherOnRootIsNotAddedWhenContentCacheDoesNotExist) {
  std::unique_ptr<CloudFileSystem> cloud_file_system =
      CreateCloudFileSystem(/*with_content_cache=*/false);

  // Expect no watchers are added.
  EXPECT_THAT(cloud_file_system->GetWatchers(), Pointee(IsEmpty()));
}

}  // namespace
}  // namespace ash::file_system_provider
