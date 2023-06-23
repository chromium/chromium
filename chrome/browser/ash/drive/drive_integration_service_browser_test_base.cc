// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/drive/drive_integration_service_browser_test_base.h"
#include "base/files/file_util.h"
#include "chrome/browser/ash/drive/drivefs_test_support.h"
#include "chrome/browser/profiles/profile.h"

namespace drive {

DriveIntegrationServiceBrowserTestBase::
    DriveIntegrationServiceBrowserTestBase() = default;

DriveIntegrationServiceBrowserTestBase::
    ~DriveIntegrationServiceBrowserTestBase() = default;

drivefs::FakeDriveFs*
DriveIntegrationServiceBrowserTestBase::GetFakeDriveFsForProfile(
    Profile* profile) {
  return &fake_drivefs_helpers_[profile]->fake_drivefs();
}

drive::DriveIntegrationService*
DriveIntegrationServiceBrowserTestBase::CreateDriveIntegrationService(
    Profile* profile) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath mount_path = profile->GetPath().Append("drivefs");
  fake_drivefs_helpers_[profile] =
      std::make_unique<drive::FakeDriveFsHelper>(profile, mount_path);
  auto* integration_service = new drive::DriveIntegrationService(
      profile, std::string(), mount_path,
      fake_drivefs_helpers_[profile]->CreateFakeDriveFsListenerFactory());
  return integration_service;
}

void DriveIntegrationServiceBrowserTestBase::InitTestFileMountRoot(
    Profile* profile) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_EQ(test_file_mount_root_mappings_.end(),
            test_file_mount_root_mappings_.find(profile));

  const base::FilePath drive_fs_mount_path =
      DriveIntegrationServiceFactory::FindForProfile(profile)
          ->GetMountPointPath();
  base::ScopedTempDir temp_dir;
  EXPECT_TRUE(temp_dir.CreateUniqueTempDirUnderPath(drive_fs_mount_path));
  test_file_mount_root_mappings_.emplace(profile, std::move(temp_dir));
}

void DriveIntegrationServiceBrowserTestBase::AddDriveFileWithRelativePath(
    Profile* profile,
    const std::string& drive_file_id,
    const base::FilePath& directory_path,
    base::FilePath* new_file_relative_path,
    base::FilePath* new_file_absolute_path) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  // `directory_path` should be a relative path.
  EXPECT_FALSE(directory_path.IsAbsolute());

  auto iter = test_file_mount_root_mappings_.find(profile);
  if (test_file_mount_root_mappings_.end() == iter) {
    ADD_FAILURE() << "the test file mount root has not been initialized yet. "
                     "Have you called InitTestFileMountRoot()?";
  }

  const std::vector<base::FilePath::StringType> components =
      directory_path.GetComponents();
  base::FilePath absolute_directory_path = iter->second.GetPath();

  // Create the missing directories.
  for (const auto& directory : components) {
    absolute_directory_path = absolute_directory_path.Append(directory);
    if (!base::DirectoryExists(absolute_directory_path))
      base::CreateDirectory(absolute_directory_path);
  }

  base::FilePath absolute_file_path;
  base::CreateTemporaryFileInDir(absolute_directory_path, &absolute_file_path);

  // Calculate the relative path from the drive file system root.
  const base::FilePath fs_mount_path =
      DriveIntegrationServiceFactory::FindForProfile(profile)
          ->GetMountPointPath();
  base::FilePath relative_to_drive_fs_mount("/");
  fs_mount_path.AppendRelativePath(absolute_file_path,
                                   &relative_to_drive_fs_mount);

  // Update the drive file metadata.
  drivefs::FakeMetadata metadata;
  metadata.path = relative_to_drive_fs_mount;
  metadata.mime_type = "text/plain";
  metadata.doc_id = drive_file_id;
  GetFakeDriveFsForProfile(profile)->SetMetadata(std::move(metadata));

  // Update the relative/absolute paths to the generated file.
  if (new_file_relative_path)
    *new_file_relative_path = relative_to_drive_fs_mount;
  if (new_file_absolute_path)
    *new_file_absolute_path = absolute_file_path;
}

bool DriveIntegrationServiceBrowserTestBase::SetUpUserDataDirectory() {
  return drive::SetUpUserDataDirectoryForDriveFsTest();
}

void DriveIntegrationServiceBrowserTestBase::
    SetUpInProcessBrowserTestFixture() {
  create_drive_integration_service_ = base::BindRepeating(
      &DriveIntegrationServiceBrowserTestBase::CreateDriveIntegrationService,
      base::Unretained(this));
  service_factory_for_test_ = std::make_unique<
      drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>(
      &create_drive_integration_service_);
}

}  // namespace drive
