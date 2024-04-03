// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/trash_unittest_base.h"

#include "base/files/file_util.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/trash_common_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_manager/volume_manager_factory.h"
#include "chromeos/ash/components/dbus/chunneld/chunneld_client.h"
#include "chromeos/ash/components/dbus/cros_disks/cros_disks_client.h"
#include "chromeos/ash/components/dbus/seneschal/seneschal_client.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/test_file_system_context.h"

namespace file_manager::io_task {

TrashBaseTest::TrashBaseTest() = default;

TrashBaseTest::~TrashBaseTest() = default;

void TrashBaseTest::SetUp() {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

  // Pass in a mock factory method that sets up a fake
  // DriveIntegrationService. This ensures the enabled paths contain the drive
  // path.
  create_drive_integration_service_ = base::BindRepeating(
      &TrashBaseTest::CreateDriveIntegrationService, base::Unretained(this));
  service_factory_for_test_ = std::make_unique<
      drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>(
      &create_drive_integration_service_);
  fake_user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());
  // Create the profile and add it to the user manager for DriveFS.
  profile_ =
      std::make_unique<TestingProfile>(base::FilePath(temp_dir_.GetPath()));
  AccountId account_id =
      AccountId::FromUserEmailGaiaId(profile_->GetProfileUserName(), "12345");
  fake_user_manager_->AddUserWithAffiliationAndTypeAndProfile(
      account_id, /*is_affiliated=*/false, user_manager::UserType::kRegular,
      profile_.get());
  fake_user_manager_->LoginUser(account_id, true);

  file_system_context_ =
      storage::CreateFileSystemContextForTesting(nullptr, temp_dir_.GetPath());
  my_files_dir_ = temp_dir_.GetPath().Append("MyFiles");
  storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
      file_manager::util::GetDownloadsMountPointName(profile_.get()),
      storage::kFileSystemTypeLocal, storage::FileSystemMountOption(),
      my_files_dir_);

  // Create Downloads inside the `temp_dir_` which will implicitly create the
  // `my_files_dir_.
  downloads_dir_ = my_files_dir_.Append("Downloads");
  ASSERT_TRUE(base::CreateDirectory(downloads_dir_));

  ash::ChunneldClient::InitializeFake();
  ash::CiceroneClient::InitializeFake();
  ash::ConciergeClient::InitializeFake();
  ash::CrosDisksClient::InitializeFake();
  ash::SeneschalClient::InitializeFake();

  // Ensure Crostini is setup correctly.
  crostini_manager_ = crostini::CrostiniManager::GetForProfile(profile_.get());
  crostini_manager_->AddRunningVmForTesting(crostini::kCrostiniDefaultVmName);
  crostini_manager_->AddRunningContainerForTesting(
      crostini::kCrostiniDefaultVmName,
      crostini::ContainerInfo(crostini::kCrostiniDefaultContainerName,
                              "testuser", "/remote/mount", "PLACEHOLDER_IP"));

  crostini_dir_ = temp_dir_.GetPath().Append("crostini");
  ASSERT_TRUE(base::CreateDirectory(crostini_dir_));

  VolumeManagerFactory::GetInstance()->SetTestingFactory(
      profile_.get(),
      base::BindLambdaForTesting([this](content::BrowserContext* context) {
        return std::unique_ptr<KeyedService>(std::make_unique<VolumeManager>(
            Profile::FromBrowserContext(context), nullptr, nullptr,
            &disk_mount_manager_, nullptr,
            VolumeManager::GetMtpStorageInfoCallback()));
      }));
  crostini_remote_mount_ = base::FilePath("/remote/mount");
  auto* volume_manager = VolumeManager::Get(profile_.get());
  volume_manager->AddVolumeForTesting(
      Volume::CreateForSshfsCrostini(crostini_dir_, crostini_remote_mount_));
}

void TrashBaseTest::TearDown() {
  // Ensure any previously registered mount points for Downloads are revoked.
  storage::ExternalMountPoints::GetSystemInstance()->RevokeAllFileSystems();
  profile_.reset();
  fake_user_manager_.Reset();
  ash::SeneschalClient::Shutdown();
  ash::CrosDisksClient::Shutdown();
  ash::ConciergeClient::Shutdown();
  ash::CiceroneClient::Shutdown();
  ash::ChunneldClient::Shutdown();
}

drive::DriveIntegrationService* TrashBaseTest::CreateDriveIntegrationService(
    Profile* profile) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath mount_point = temp_dir_.GetPath().Append("drivefs");
  fake_drivefs_helper_ =
      std::make_unique<drive::FakeDriveFsHelper>(profile, mount_point);
  integration_service_ = new drive::DriveIntegrationService(
      profile, "", mount_point,
      fake_drivefs_helper_->CreateFakeDriveFsListenerFactory());
  return integration_service_;
}

storage::FileSystemURL TrashBaseTest::CreateFileSystemURL(
    const base::FilePath& absolute_path) {
  std::string relative_path = absolute_path.value();

  // URLs in test are required to be relative to the `base_path_`. This ensures
  // all FileSystemURLs in test are made relative.
  EXPECT_TRUE(file_manager::util::ReplacePrefix(
      &relative_path, temp_dir_.GetPath().AsEndingWithSeparator().value(), ""));

  return file_system_context_->CreateCrackedFileSystemURL(
      kTestStorageKey, storage::kFileSystemTypeTest,
      base::FilePath::FromUTF8Unsafe(relative_path));
}

const base::FilePath TrashBaseTest::GenerateInfoPath(
    const std::string& file_name) {
  return trash::GenerateTrashPath(
      downloads_dir_.Append(trash::kTrashFolderName), trash::kInfoFolderName,
      file_name);
}

const base::FilePath TrashBaseTest::GenerateFilesPath(
    const std::string& file_name) {
  return trash::GenerateTrashPath(
      downloads_dir_.Append(trash::kTrashFolderName), trash::kFilesFolderName,
      file_name);
}

const std::string TrashBaseTest::CreateTrashInfoContentsFromPath(
    const base::FilePath& file_path,
    const base::FilePath& base_path,
    const base::FilePath& prefix_path) {
  std::string relative_restore_path = file_path.value();
  EXPECT_TRUE(file_manager::util::ReplacePrefix(
      &relative_restore_path, base_path.AsEndingWithSeparator().value(), ""));

  base::FilePath prefix = (prefix_path.IsAbsolute())
                              ? prefix_path
                              : base::FilePath("/").Append(prefix_path);

  return base::StrCat(
      {"[Trash Info]\nPath=",
       base::EscapePath(prefix.AsEndingWithSeparator().value()),
       base::EscapePath(relative_restore_path), "\nDeletionDate=",
       base::TimeFormatAsIso8601(base::Time::UnixEpoch()), "\n"});
}

const std::string TrashBaseTest::CreateTrashInfoContentsFromPath(
    const base::FilePath& file_path) {
  return CreateTrashInfoContentsFromPath(file_path, my_files_dir_,
                                         base::FilePath("/"));
}

bool TrashBaseTest::EnsureTrashDirectorySetup(
    const base::FilePath& parent_path) {
  base::FilePath trash_path = parent_path.Append(trash::kTrashFolderName);
  if (!base::CreateDirectory(trash_path.Append(trash::kInfoFolderName))) {
    return false;
  }
  if (!base::CreateDirectory(trash_path.Append(trash::kFilesFolderName))) {
    return false;
  }
  return true;
}

}  // namespace file_manager::io_task
