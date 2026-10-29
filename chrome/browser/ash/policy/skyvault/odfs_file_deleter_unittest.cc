// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/odfs_file_deleter.h"

#include <memory>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_manager/volume_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::cloud_upload {

namespace {
constexpr char kMountPointName[] = "odfs_deleter_test_mount";
}  // namespace

class OdfsFileDeleterTest : public BrowserWithTestWindowTest {
 public:
  OdfsFileDeleterTest() = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_path_ = temp_dir_.GetPath().AppendASCII("test_capture.png");
    ASSERT_TRUE(base::WriteFile(file_path_, "test content"));
  }

  void TearDown() override {
    storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
        kMountPointName);
    BrowserWithTestWindowTest::TearDown();
  }

 protected:
  void RegisterMountPoint() {
    ASSERT_TRUE(
        storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
            kMountPointName, storage::kFileSystemTypeLocal,
            storage::FileSystemMountOption(), temp_dir_.GetPath()));
  }

  void SetUpVolumeManager() {
    file_manager::VolumeManagerFactory::GetInstance()->SetTestingFactory(
        profile(),
        base::BindLambdaForTesting([](content::BrowserContext* context) {
          return std::unique_ptr<KeyedService>(
              std::make_unique<file_manager::VolumeManager>(
                  TestingBrowserProcess::GetGlobal()->local_state(),
                  Profile::FromBrowserContext(context),
                  /*drive_integration_service=*/nullptr,
                  /*power_manager_client=*/nullptr,
                  ash::disks::DiskMountManager::GetInstance(),
                  /*file_system_provider_service=*/nullptr,
                  file_manager::VolumeManager::GetMtpStorageInfoCallback()));
        }));
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath file_path_;
};

TEST_F(OdfsFileDeleterTest, DeleteFailsOnInvalidFileSystemUrl) {
  base::test::TestFuture<bool> future;
  OdfsFileDeleter::Delete(file_path_, future.GetCallback());
  EXPECT_FALSE(future.Get());

  OdfsFileDeleter::Delete(file_path_, base::NullCallback());
}

TEST_F(OdfsFileDeleterTest, DeleteFailsWhenNoVolumeManager) {
  RegisterMountPoint();

  base::test::TestFuture<bool> future;
  OdfsFileDeleter::Delete(file_path_, future.GetCallback());
  EXPECT_FALSE(future.Get());

  OdfsFileDeleter::Delete(file_path_, base::NullCallback());
}

TEST_F(OdfsFileDeleterTest, DeleteFailsWhenFileDoesNotExist) {
  RegisterMountPoint();
  SetUpVolumeManager();

  const base::FilePath non_existent_path =
      temp_dir_.GetPath().AppendASCII("non_existent.png");

  base::test::TestFuture<bool> future;
  OdfsFileDeleter::Delete(non_existent_path, future.GetCallback());
  EXPECT_FALSE(future.Get());

  OdfsFileDeleter::Delete(non_existent_path, base::NullCallback());
  task_environment()->RunUntilIdle();
}

TEST_F(OdfsFileDeleterTest, DeleteSucceeds) {
  RegisterMountPoint();
  SetUpVolumeManager();

  base::test::TestFuture<bool> future;
  OdfsFileDeleter::Delete(file_path_, future.GetCallback());
  EXPECT_TRUE(future.Get());
  EXPECT_FALSE(base::PathExists(file_path_));
}

}  // namespace ash::cloud_upload
