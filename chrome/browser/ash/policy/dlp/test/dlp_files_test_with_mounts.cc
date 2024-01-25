// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/test/dlp_files_test_with_mounts.h"

#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/fake_crostini_features.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/fileapi/file_system_backend.h"
#include "chrome/browser/ash/policy/dlp/files_policy_notification_manager_factory.h"
#include "chrome/browser/ash/policy/dlp/test/files_policy_notification_manager_test_utils.h"
#include "chrome/browser/ash/policy/dlp/test/mock_files_policy_notification_manager.h"
#include "chrome/browser/enterprise/data_controls/dlp_reporting_manager.h"
#include "chrome/browser/policy/messaging_layer/public/report_client_test_util.h"
#include "chromeos/ash/components/dbus/chunneld/chunneld_client.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/seneschal/seneschal_client.h"
#include "chromeos/dbus/dlp/dlp_client.h"
#include "components/drive/drive_pref_names.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/test_file_system_context.h"

namespace policy {

DlpFilesTestWithMounts::DlpFilesTestWithMounts() = default;
DlpFilesTestWithMounts::~DlpFilesTestWithMounts() = default;

void DlpFilesTestWithMounts::MountExternalComponents() {
  // Register Android.
  ASSERT_TRUE(mount_points_->RegisterFileSystem(
      file_manager::util::GetAndroidFilesMountPointName(),
      storage::kFileSystemTypeLocal, storage::FileSystemMountOption(),
      base::FilePath(file_manager::util::GetAndroidFilesPath())));

  // Register Removable Media.
  ASSERT_TRUE(mount_points_->RegisterFileSystem(
      ash::kSystemMountNameRemovable, storage::kFileSystemTypeLocal,
      storage::FileSystemMountOption(),
      base::FilePath(file_manager::util::kRemovableMediaPath)));

  // Setup for Crostini.
  crostini::FakeCrostiniFeatures crostini_features;
  crostini_features.set_is_allowed_now(true);
  crostini_features.set_enabled(true);

  ash::ChunneldClient::InitializeFake();
  ash::CiceroneClient::InitializeFake();
  ash::ConciergeClient::InitializeFake();
  ash::SeneschalClient::InitializeFake();

  crostini::CrostiniManager* crostini_manager =
      crostini::CrostiniManager::GetForProfile(profile_.get());
  ASSERT_TRUE(crostini_manager);
  crostini_manager->AddRunningVmForTesting(crostini::kCrostiniDefaultVmName);
  crostini_manager->AddRunningContainerForTesting(
      crostini::kCrostiniDefaultVmName,
      crostini::ContainerInfo(crostini::kCrostiniDefaultContainerName,
                              "testuser", "/home/testuser", "PLACEHOLDER_IP"));
  // Register Crostini.
  ASSERT_TRUE(mount_points_->RegisterFileSystem(
      file_manager::util::GetCrostiniMountPointName(profile_.get()),
      storage::kFileSystemTypeLocal, storage::FileSystemMountOption(),
      file_manager::util::GetCrostiniMountDirectory(profile_.get())));

  // Setup for DriveFS.
  profile_->GetPrefs()->SetString(drive::prefs::kDriveFsProfileSalt, "a");
  drive::DriveIntegrationServiceFactory::GetForProfile(profile_.get())
      ->SetEnabled(true);
  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::GetForProfile(profile_.get());
  ASSERT_TRUE(integration_service);
  base::FilePath mount_point_drive = integration_service->GetMountPointPath();
  // Register DriveFS.
  ASSERT_TRUE(mount_points_->RegisterFileSystem(
      mount_point_drive.BaseName().value(), storage::kFileSystemTypeLocal,
      storage::FileSystemMountOption(), mount_point_drive));
}

void DlpFilesTestWithMounts::SetUp() {
  DlpFilesTestBase::SetUp();
  ASSERT_TRUE(rules_manager_);
  files_controller_ =
      std::make_unique<DlpFilesControllerAsh>(*rules_manager_, profile_.get());

  event_storage_ = files_controller_->GetEventStorageForTesting();
  DCHECK(event_storage_);

  task_runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  event_storage_->SetTaskRunnerForTesting(task_runner_);

  // Reporting test environment needs to be created before the browser
  // creation is completed.
  reporting_test_enviroment_ =
      reporting::ReportingClient::TestEnvironment::CreateWithStorageModule();

  reporting_manager_ = std::make_unique<data_controls::DlpReportingManager>();
  SetReportQueueForReportingManager(
      reporting_manager_.get(), events_,
      base::SequencedTaskRunner::GetCurrentDefault());
  ON_CALL(*rules_manager_, GetReportingManager)
      .WillByDefault(::testing::Return(reporting_manager_.get()));

  // Set FilesPolicyNotificationManager.
  policy::FilesPolicyNotificationManagerFactory::GetInstance()
      ->SetTestingFactory(
          profile_.get(),
          base::BindRepeating(
              &DlpFilesTestWithMounts::SetFilesPolicyNotificationManager,
              base::Unretained(this)));

  ASSERT_TRUE(
      policy::FilesPolicyNotificationManagerFactory::GetForBrowserContext(
          profile_.get()));
  ASSERT_TRUE(fpnm_);

  chromeos::DlpClient::InitializeFake();
  chromeos::DlpClient::Get()->GetTestInterface()->SetIsAlive(true);

  my_files_dir_ =
      file_manager::util::GetMyFilesFolderForProfile(profile_.get());
  ASSERT_TRUE(base::CreateDirectory(my_files_dir_));
  file_system_context_ =
      storage::CreateFileSystemContextForTesting(nullptr, my_files_dir_);
  my_files_dir_url_ =
      CreateFileSystemURL(kTestStorageKey, my_files_dir_.value());

  ASSERT_TRUE(files_controller_);
  files_controller_->SetFileSystemContextForTesting(file_system_context_.get());

  mount_points_ = storage::ExternalMountPoints::GetSystemInstance();
  ASSERT_TRUE(mount_points_);
  mount_points_->RevokeAllFileSystems();
}

void DlpFilesTestWithMounts::TearDown() {
  event_storage_ = nullptr;
  files_controller_.reset();
  DlpFilesTestBase::TearDown();
  reporting_manager_.reset();

  reporting_test_enviroment_.reset();

  if (chromeos::DlpClient::Get()) {
    chromeos::DlpClient::Shutdown();
  }

  if (ash::ChunneldClient::Get()) {
    ash::ChunneldClient::Shutdown();
  }

  if (ash::CiceroneClient::Get()) {
    ash::CiceroneClient::Shutdown();
  }

  if (ash::ConciergeClient::Get()) {
    ash::ConciergeClient::Shutdown();
  }

  if (ash::SeneschalClient::Get()) {
    ash::SeneschalClient::Shutdown();
  }

  storage::ExternalMountPoints::GetSystemInstance()->RevokeAllFileSystems();
}

std::unique_ptr<KeyedService>
DlpFilesTestWithMounts::SetFilesPolicyNotificationManager(
    content::BrowserContext* context) {
  auto fpnm =
      std::make_unique<testing::StrictMock<MockFilesPolicyNotificationManager>>(
          profile_.get());
  fpnm_ = fpnm.get();

  return fpnm;
}

}  // namespace policy
