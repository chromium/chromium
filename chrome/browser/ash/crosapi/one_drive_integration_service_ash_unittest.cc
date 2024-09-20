// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/one_drive_integration_service_ash.h"

#include "base/test/bind.h"
#include "chrome/browser/ash/file_manager/file_manager_test_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_manager/volume_manager_factory.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/disks/fake_disk_mount_manager.h"
#include "chromeos/crosapi/mojom/one_drive_integration_service.mojom.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crosapi {

namespace {

constexpr char kProfileName[] = "user@gmail.com";
constexpr char kGaiaId[] = "1234567890";

const base::FilePath kOneDrivePath(file_manager::util::kFuseBoxMediaPath);

}  // namespace

class MockOneDriveMountObserver : public crosapi::mojom::OneDriveMountObserver {
 public:
  MockOneDriveMountObserver() = default;
  ~MockOneDriveMountObserver() override = default;

  mojo::PendingRemote<crosapi::mojom::OneDriveMountObserver>
  BindAndGetRemote() {
    DCHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }

  mojo::Receiver<crosapi::mojom::OneDriveMountObserver>& GetReceiver() {
    return receiver_;
  }

  mojo::Receiver<crosapi::mojom::OneDriveMountObserver> receiver_{this};

  MOCK_METHOD1(OnOneDriveMountPointPathChanged,
               void(const base::FilePath& path));
};

class TestOneDriveIntegrationServiceAsh : public OneDriveIntegrationServiceAsh {
 public:
  TestOneDriveIntegrationServiceAsh() = default;
  ~TestOneDriveIntegrationServiceAsh() override = default;

  void OnProvidedFileSystemMount(
      const ash::file_system_provider::ProvidedFileSystemInfo& file_system_info,
      ash::file_system_provider::MountContext context,
      base::File::Error error) override {
    OneDriveIntegrationServiceAsh::OnProvidedFileSystemMount(file_system_info,
                                                             context, error);
  }

  void OnProvidedFileSystemUnmount(
      const ash::file_system_provider::ProvidedFileSystemInfo& file_system_info,
      base::File::Error error) override {
    OneDriveIntegrationServiceAsh::OnProvidedFileSystemUnmount(file_system_info,
                                                               error);
  }
};

class OneDriveIntegrationServiceAshTest : public testing::Test {
 public:
  OneDriveIntegrationServiceAshTest() = default;
  ~OneDriveIntegrationServiceAshTest() override = default;

 protected:
  void SetUp() override {
    testing::Test::SetUp();
    fake_user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile(kProfileName);
    const AccountId account_id =
        AccountId::FromUserEmailGaiaId(profile_->GetProfileUserName(), kGaiaId);
    fake_user_manager_->AddUser(account_id);
    fake_user_manager_->LoginUser(account_id);
    DCHECK(ash::ProfileHelper::IsPrimaryProfile(profile_));

    file_manager::VolumeManagerFactory::GetInstance()->SetTestingFactory(
        profile_,
        base::BindLambdaForTesting([this](content::BrowserContext* context) {
          return std::unique_ptr<KeyedService>(
              std::make_unique<file_manager::VolumeManager>(
                  Profile::FromBrowserContext(context), nullptr, nullptr,
                  &disk_mount_manager_, nullptr,
                  file_manager::VolumeManager::GetMtpStorageInfoCallback()));
        }));
  }

  void AddOneDriveFuseboxVolume() {
    auto* fake_provided_fs =
        file_manager::test::MountFakeProvidedFileSystemOneDrive(profile_);

    file_manager::VolumeManager* const volume_manager =
        file_manager::VolumeManager::Get(profile_);
    ASSERT_TRUE(volume_manager);

    std::unique_ptr<file_manager::Volume> volume =
        file_manager::Volume::CreateForProvidedFileSystem(
            fake_provided_fs->GetFileSystemInfo(),
            file_manager::MountContext::MOUNT_CONTEXT_USER, kOneDrivePath);

    volume_manager->AddVolumeForTesting(std::move(volume));
  }

  content::BrowserTaskEnvironment task_environment_;
  ash::disks::FakeDiskMountManager disk_mount_manager_;
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  // Externally owned raw pointers. Owned by TestingProfileManager.
  raw_ptr<TestingProfile> profile_;

  testing::StrictMock<MockOneDriveMountObserver> mock_observer_;
  TestOneDriveIntegrationServiceAsh one_drive_service_ash_;
};

TEST_F(OneDriveIntegrationServiceAshTest, NotifyObserverOnMountChanges) {
  // OnOneDriveMountPointPathChanged should be called with empty file path since
  // OneDrive isn't mounted.
  EXPECT_CALL(mock_observer_,
              OnOneDriveMountPointPathChanged(base::FilePath()));
  one_drive_service_ash_.AddOneDriveMountObserver(
      mock_observer_.BindAndGetRemote());
  mock_observer_.GetReceiver().FlushForTesting();

  // OnOneDriveMountPointPathChanged should be called with OneDrive path when
  // it's mounted.
  EXPECT_CALL(mock_observer_, OnOneDriveMountPointPathChanged(kOneDrivePath));
  AddOneDriveFuseboxVolume();
  auto file_system_info = ash::file_system_provider::ProvidedFileSystemInfo(
      extension_misc::kODFSExtensionId,
      ash::file_system_provider::MountOptions(), kOneDrivePath,
      /*configurable=*/true,
      /*watchable=*/true, extensions::FileSystemProviderSource::SOURCE_NETWORK,
      ash::file_system_provider::IconSet(),
      ash::file_system_provider::CacheType::NONE);
  one_drive_service_ash_.OnProvidedFileSystemMount(
      file_system_info,
      ash::file_system_provider::MountContext::MOUNT_CONTEXT_USER,
      base::File::Error::FILE_OK);
  mock_observer_.GetReceiver().FlushForTesting();

  // OnOneDriveMountPointPathChanged should be called with empty path when
  // OneDrive is unmounted.
  EXPECT_CALL(mock_observer_,
              OnOneDriveMountPointPathChanged(base::FilePath()));
  one_drive_service_ash_.OnProvidedFileSystemUnmount(
      file_system_info, base::File::Error::FILE_OK);
  mock_observer_.GetReceiver().FlushForTesting();
}

TEST_F(OneDriveIntegrationServiceAshTest, NotNotifyObserverOnMountError) {
  // OnOneDriveMountPointPathChanged should be called with OneDrive path since
  // OneDrive is mounted.
  AddOneDriveFuseboxVolume();
  EXPECT_CALL(mock_observer_, OnOneDriveMountPointPathChanged(kOneDrivePath));
  one_drive_service_ash_.AddOneDriveMountObserver(
      mock_observer_.BindAndGetRemote());
  mock_observer_.GetReceiver().FlushForTesting();

  // OnOneDriveMountPointPathChanged shouldn't be called since OneDrive mount
  // has an error.
  auto file_system_info = ash::file_system_provider::ProvidedFileSystemInfo(
      extension_misc::kODFSExtensionId,
      ash::file_system_provider::MountOptions(), kOneDrivePath,
      /*configurable=*/true,
      /*watchable=*/true, extensions::FileSystemProviderSource::SOURCE_NETWORK,
      ash::file_system_provider::IconSet(),
      ash::file_system_provider::CacheType::NONE);
  one_drive_service_ash_.OnProvidedFileSystemMount(
      file_system_info,
      ash::file_system_provider::MountContext::MOUNT_CONTEXT_USER,
      base::File::Error::FILE_ERROR_FAILED);
  mock_observer_.GetReceiver().FlushForTesting();

  // OnOneDriveMountPointPathChanged shouldn't be called since OneDrive unmount
  // has an error.
  one_drive_service_ash_.OnProvidedFileSystemUnmount(
      file_system_info, base::File::Error::FILE_ERROR_INVALID_OPERATION);
  mock_observer_.GetReceiver().FlushForTesting();
}

TEST_F(OneDriveIntegrationServiceAshTest, NotNotifyObserverOnUnrelatedMounts) {
  // OnOneDriveMountPointPathChanged should be called with OneDrive path since
  // OneDrive is mounted.
  AddOneDriveFuseboxVolume();
  EXPECT_CALL(mock_observer_, OnOneDriveMountPointPathChanged(kOneDrivePath));
  one_drive_service_ash_.AddOneDriveMountObserver(
      mock_observer_.BindAndGetRemote());
  mock_observer_.GetReceiver().FlushForTesting();

  // OnOneDriveMountPointPathChanged shouldn't be called since the mount event
  // doesn't have ODFS extension ID.
  auto file_system_info = ash::file_system_provider::ProvidedFileSystemInfo(
      extension_misc::kDeskApiExtensionId,
      ash::file_system_provider::MountOptions(), kOneDrivePath,
      /*configurable=*/true,
      /*watchable=*/true, extensions::FileSystemProviderSource::SOURCE_NETWORK,
      ash::file_system_provider::IconSet(),
      ash::file_system_provider::CacheType::NONE);
  one_drive_service_ash_.OnProvidedFileSystemMount(
      file_system_info,
      ash::file_system_provider::MountContext::MOUNT_CONTEXT_USER,
      base::File::Error::FILE_OK);
  mock_observer_.GetReceiver().FlushForTesting();
}

}  // namespace crosapi
