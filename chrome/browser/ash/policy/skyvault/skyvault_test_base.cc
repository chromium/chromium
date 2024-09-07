// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/skyvault_test_base.h"

#include <string>

#include "base/base_paths.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/policy/skyvault/local_files_migration_constants.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "storage/browser/file_system/external_mount_points.h"

namespace policy::local_user_files {

base::FilePath GetTestFilePath(const std::string& file_name) {
  // Get the path to file manager's test data directory.
  base::FilePath source_dir;
  CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_dir));
  base::FilePath test_data_dir = source_dir.AppendASCII("chrome")
                                     .AppendASCII("test")
                                     .AppendASCII("data")
                                     .AppendASCII("chromeos")
                                     .AppendASCII("file_manager");
  return test_data_dir.Append(base::FilePath::FromUTF8Unsafe(file_name));
}

void SkyvaultTestBase::TearDown() {
  InProcessBrowserTest::TearDown();
  storage::ExternalMountPoints::GetSystemInstance()->RevokeAllFileSystems();
}

void SkyvaultTestBase::SetUpMyFiles() {
  my_files_dir_ = GetMyFilesPath(browser()->profile());
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::CreateDirectory(my_files_dir_));
  }
  std::string mount_point_name =
      file_manager::util::GetDownloadsMountPointName(profile());
  storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
      mount_point_name);
  CHECK(storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
      mount_point_name, storage::kFileSystemTypeLocal,
      storage::FileSystemMountOption(), my_files_dir_));
  file_manager::VolumeManager::Get(profile())
      ->RegisterDownloadsDirectoryForTesting(my_files_dir_);
}

base::FilePath SkyvaultTestBase::CreateTestDir(
    const std::string& test_dir_name,
    const base::FilePath& parent_dir) {
  const base::FilePath dir_path = parent_dir.AppendASCII(test_dir_name);
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    CHECK(base::CreateDirectory(dir_path));
    CHECK(base::PathExists(dir_path));
  }

  return dir_path;
}

base::FilePath SkyvaultTestBase::CopyTestFile(
    const std::string& test_file_name,
    const base::FilePath& parent_dir) {
  const base::FilePath copied_file_path =
      parent_dir.AppendASCII(test_file_name);

  const base::FilePath test_file_path = GetTestFilePath(test_file_name);
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    CHECK(base::CopyFile(test_file_path, copied_file_path));
    CHECK(base::PathExists(copied_file_path));
  }

  return copied_file_path;
}

void SkyvaultOneDriveTest::SetUpODFS() {
  provided_file_system_ =
      file_manager::test::MountFakeProvidedFileSystemOneDrive(profile());
}

void SkyvaultOneDriveTest::CheckPathExistsOnODFS(const base::FilePath& path) {
  ASSERT_TRUE(provided_file_system_);
  base::test::TestFuture<
      std::unique_ptr<ash::file_system_provider::EntryMetadata>,
      base::File::Error>
      future;
  provided_file_system_->GetMetadata(path, {}, future.GetCallback());
  EXPECT_EQ(base::File::Error::FILE_OK, future.Get<base::File::Error>());
}

void SkyvaultOneDriveTest::CheckPathNotFoundOnODFS(const base::FilePath& path) {
  ASSERT_TRUE(provided_file_system_);
  base::test::TestFuture<
      std::unique_ptr<ash::file_system_provider::EntryMetadata>,
      base::File::Error>
      future;
  provided_file_system_->GetMetadata(path, {}, future.GetCallback());
  EXPECT_EQ(base::File::Error::FILE_ERROR_NOT_FOUND,
            future.Get<base::File::Error>());
}

SkyvaultGoogleDriveTest::SkyvaultGoogleDriveTest() {
  EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
  drive_mount_point_ = temp_dir_.GetPath().Append("drivefs");
  drive_root_dir_ = drive_mount_point_.AppendASCII("root");
}

SkyvaultGoogleDriveTest::~SkyvaultGoogleDriveTest() = default;

void SkyvaultGoogleDriveTest::SetUpInProcessBrowserTestFixture() {
  // Setup drive integration service.
  create_drive_integration_service_ = base::BindRepeating(
      &SkyvaultGoogleDriveTest::CreateDriveIntegrationService,
      base::Unretained(this));
  service_factory_for_test_ = std::make_unique<
      drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>(
      &create_drive_integration_service_);
}

void SkyvaultGoogleDriveTest::SetUpOnMainThread() {
  SetDriveConnectionStatusForTesting(ConnectionStatus::kConnected);
  SkyvaultTestBase::SetUpOnMainThread();
}

void SkyvaultGoogleDriveTest::SetUp() {
  InProcessBrowserTest::SetUp();
  // Create the Google Drive root directory
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::CreateDirectory(drive_root_dir_));
  }
}

void SkyvaultGoogleDriveTest::TearDown() {
  SkyvaultTestBase::TearDown();
  storage::ExternalMountPoints::GetSystemInstance()->RevokeAllFileSystems();
}

void SkyvaultGoogleDriveTest::TearDownOnMainThread() {
  RemoveObservers();
  SkyvaultTestBase::TearDownOnMainThread();
}

void SkyvaultGoogleDriveTest::SetUpObservers() {
  // Subscribe to IOTasks updates to track the copy/move to Drive progress.
  file_manager::VolumeManager::Get(profile())
      ->io_task_controller()
      ->AddObserver(this);
}

void SkyvaultGoogleDriveTest::RemoveObservers() {
  file_manager::VolumeManager::Get(profile())
      ->io_task_controller()
      ->RemoveObserver(this);
}

base::FilePath SkyvaultGoogleDriveTest::SetUpSourceFile(
    const std::string& test_file_name,
    base::FilePath source_path) {
  base::FilePath source_file_path = CopyTestFile(test_file_name, source_path);

  base::FilePath local_relative_path;
  GetMyFilesPath(browser()->profile())
      .AppendRelativePath(source_file_path, &local_relative_path);
  FileInfo info(test_file_name, local_relative_path);
  // Check that the source file exists at the intended source location and is
  // not in Drive.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::PathExists(source_file_path));
    CheckPathNotFoundOnDrive(observed_relative_drive_path(info));
  }

  source_files_.emplace(source_file_path, std::move(info));
  return source_file_path;
}

void SkyvaultGoogleDriveTest::CheckPathExistsOnDrive(
    const base::FilePath& path) {
  drive_integration_service()->GetDriveFsInterface()->GetMetadata(
      path, base::BindOnce(&SkyvaultGoogleDriveTest::OnGetMetadataExpectSuccess,
                           base::Unretained(this)));
  base::ScopedAllowBlockingForTesting allow_blocking;
  Wait();
}
void SkyvaultGoogleDriveTest::CheckPathNotFoundOnDrive(
    const base::FilePath& path) {
  drive_integration_service()->GetDriveFsInterface()->GetMetadata(
      path,
      base::BindOnce(&SkyvaultGoogleDriveTest::OnGetMetadataExpectNotFound,
                     base::Unretained(this)));
  base::ScopedAllowBlockingForTesting allow_blocking;
  Wait();
}

void SkyvaultGoogleDriveTest::Wait() {
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_FALSE(run_loop_);
  run_loop_ = std::make_unique<base::RunLoop>();
  run_loop_->Run();
  run_loop_ = nullptr;
}

void SkyvaultGoogleDriveTest::EndWait() {
  ASSERT_TRUE(run_loop_);
  run_loop_->Quit();
}

DriveIntegrationService* SkyvaultGoogleDriveTest::CreateDriveIntegrationService(
    Profile* profile) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  fake_drivefs_helpers_[profile] =
      std::make_unique<file_manager::test::FakeSimpleDriveFsHelper>(
          profile, drive_mount_point_);
  return new DriveIntegrationService(
      profile, "", drive_mount_point_,
      fake_drivefs_helpers_[profile]->CreateFakeDriveFsListenerFactory());
}

void SkyvaultGoogleDriveTest::OnGetMetadataExpectSuccess(
    drive::FileError error,
    drivefs::mojom::FileMetadataPtr metadata) {
  EXPECT_EQ(drive::FILE_ERROR_OK, error);
  EndWait();
}

void SkyvaultGoogleDriveTest::OnGetMetadataExpectNotFound(
    drive::FileError error,
    drivefs::mojom::FileMetadataPtr metadata) {
  EXPECT_EQ(drive::FILE_ERROR_NOT_FOUND, error);
  EndWait();
}

}  // namespace policy::local_user_files
