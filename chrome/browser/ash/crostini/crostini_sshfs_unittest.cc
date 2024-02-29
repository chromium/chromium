// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_sshfs.h"
#include "build/build_config.h"

#include <memory>

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/crostini_test_helper.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_manager/volume_manager_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/chunneld/chunneld_client.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/cros_disks/cros_disks_client.h"
#include "chromeos/ash/components/dbus/seneschal/seneschal_client.h"
#include "chromeos/ash/components/dbus/vm_applications/apps.pb.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#include "chromeos/ash/components/disks/mock_disk_mount_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::ash::disks::DiskMountManager;
using testing::_;

namespace {
const char* kCrostiniMetricMountResultBackground =
    "Crostini.Sshfs.Mount.Result.Background";
const char* kCrostiniMetricMountResultUserVisible =
    "Crostini.Sshfs.Mount.Result.UserVisible";
const char* kCrostiniMetricMountTimeTaken = "Crostini.Sshfs.Mount.TimeTaken";
const char* kCrostiniMetricUnmount = "Crostini.Sshfs.Unmount.Result";
const char* kCrostiniMetricUnmountTimeTaken =
    "Crostini.Sshfs.Unmount.TimeTaken";
// Creates a new VolumeManager for tests.
// By default, VolumeManager KeyedService is null for testing.
std::unique_ptr<KeyedService> BuildVolumeManager(
    content::BrowserContext* context) {
  return std::make_unique<file_manager::VolumeManager>(
      Profile::FromBrowserContext(context),
      nullptr /* drive_integration_service */,
      nullptr /* power_manager_client */, DiskMountManager::GetInstance(),
      nullptr /* file_system_provider_service */,
      file_manager::VolumeManager::GetMtpStorageInfoCallback());
}
}  // namespace

namespace crostini {

class CrostiniSshfsHelperTest : public testing::Test {
 public:
  CrostiniSshfsHelperTest() {
    ash::ChunneldClient::InitializeFake();
    ash::CiceroneClient::InitializeFake();
    ash::ConciergeClient::InitializeFake();
    ash::SeneschalClient::InitializeFake();
    profile_ = std::make_unique<TestingProfile>();
    crostini_test_helper_ =
        std::make_unique<CrostiniTestHelper>(profile_.get());
    // DiskMountManager::InitializeForTesting takes ownership and works with
    // a raw pointer, hence the new with no matching delete.
    disk_manager_ = new ash::disks::MockDiskMountManager;
    crostini_sshfs_ = std::make_unique<CrostiniSshfs>(profile_.get());
    file_manager::VolumeManagerFactory::GetInstance()->SetTestingFactory(
        profile_.get(), base::BindRepeating(&BuildVolumeManager));

    DiskMountManager::InitializeForTesting(disk_manager_);

    std::string known_hosts = base::Base64Encode("[hostname]:2222 pubkey");
    std::string identity = base::Base64Encode("privkey");
  }

  CrostiniSshfsHelperTest(const CrostiniSshfsHelperTest&) = delete;
  CrostiniSshfsHelperTest& operator=(const CrostiniSshfsHelperTest&) = delete;

  ~CrostiniSshfsHelperTest() override {
    storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
        kMountName);
    crostini_sshfs_.reset();
    crostini_test_helper_.reset();
    profile_.reset();
    file_manager::VolumeManagerFactory::GetInstance()->SetTestingFactory(
        profile_.get(), BrowserContextKeyedServiceFactory::TestingFactory{});
    DiskMountManager::Shutdown();
    ash::SeneschalClient::Shutdown();
    ash::ConciergeClient::Shutdown();
    ash::CiceroneClient::Shutdown();
    ash::ChunneldClient::Shutdown();
  }

  TestingProfile* profile() { return profile_.get(); }

  base::test::TaskEnvironment* task_environment() { return &task_environment_; }

  void SetUp() override {}

  void TearDown() override {}

 protected:
  void NotifyMountEvent(
      const std::string& source_path,
      const std::string& source_format,
      const std::string& mount_label,
      const std::vector<std::string>& mount_options,
      ash::MountType type,
      ash::MountAccessMode access_mode,
      ash::disks::DiskMountManager::MountPathCallback callback) {
    auto event = DiskMountManager::MountEvent::MOUNTING;
    auto code = ash::MountError::kSuccess;
    DiskMountManager::MountPoint info{"sftp://3:1234",
                                      "/media/fuse/" + kMountName,
                                      ash::MountType::kNetworkStorage};
    disk_manager_->NotifyMountEvent(event, code, info);
    std::move(callback).Run(code, info);
  }

  void ExpectMountCalls(int n) {
    EXPECT_CALL(*disk_manager_, MountPath("sftp://3:1234", "", kMountName,
                                          default_mount_options_,
                                          ash::MountType::kNetworkStorage,
                                          ash::MountAccessMode::kReadWrite, _))
        .Times(n)
        .WillRepeatedly(
            Invoke(this, &CrostiniSshfsHelperTest::NotifyMountEvent));
  }

  void SetContainerRunning(guest_os::GuestId container) {
    auto* manager = CrostiniManager::GetForProfile(profile());
    ContainerInfo info(container.container_name, "username", "homedir",
                       "1.2.3.4", 1234);
    manager->AddRunningVmForTesting(container.vm_name, 3);
    manager->AddRunningContainerForTesting(container.vm_name, info);
  }

  content::BrowserTaskEnvironment task_environment_;
  raw_ptr<ash::disks::MockDiskMountManager, DanglingUntriaged> disk_manager_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<CrostiniTestHelper> crostini_test_helper_;
  const std::string kMountName = "crostini_test_termina_penguin";
  std::vector<std::string> default_mount_options_;
  std::unique_ptr<file_manager::VolumeManager> volume_manager_;
  std::unique_ptr<CrostiniSshfs> crostini_sshfs_;
  raw_ptr<CrostiniManager> crostini_manager_;
  base::HistogramTester histogram_tester{};
};

TEST_F(CrostiniSshfsHelperTest, MountDiskMountsDisk) {
  SetContainerRunning(DefaultContainerId());
  ExpectMountCalls(1);
  bool result = false;

  crostini_sshfs_->MountCrostiniFiles(
      DefaultContainerId(),
      base::BindLambdaForTesting([&result](bool res) { result = res; }), true);
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(result);
  base::FilePath path;
  EXPECT_TRUE(
      storage::ExternalMountPoints::GetSystemInstance()->GetRegisteredPath(
          kMountName, &path));
  EXPECT_EQ(base::FilePath("/media/fuse/" + kMountName), path);
  histogram_tester.ExpectUniqueSample(
      kCrostiniMetricMountResultBackground,
      CrostiniSshfs::CrostiniSshfsResult::kSuccess, 1);
  histogram_tester.ExpectTotalCount(kCrostiniMetricMountTimeTaken, 1);
}

TEST_F(CrostiniSshfsHelperTest, FailsIfContainerNotRunning) {
  bool result = false;
  EXPECT_CALL(*disk_manager_, MountPath).Times(0);

  crostini_sshfs_->MountCrostiniFiles(
      DefaultContainerId(),
      base::BindLambdaForTesting([&result](bool res) { result = res; }), false);
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(result);
  histogram_tester.ExpectUniqueSample(
      kCrostiniMetricMountResultUserVisible,
      CrostiniSshfs::CrostiniSshfsResult::kContainerNotRunning, 1);
  histogram_tester.ExpectTotalCount(kCrostiniMetricMountTimeTaken, 1);
}

TEST_F(CrostiniSshfsHelperTest, OnlyDefaultContainerSupported) {
  auto not_default =
      guest_os::GuestId(kCrostiniDefaultVmType, "vm_name", "container_name");
  SetContainerRunning(not_default);
  EXPECT_CALL(*disk_manager_, MountPath).Times(0);

  bool result = false;
  crostini_sshfs_->MountCrostiniFiles(
      not_default,
      base::BindLambdaForTesting([&result](bool res) { result = res; }), false);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(result);
  histogram_tester.ExpectUniqueSample(
      kCrostiniMetricMountResultUserVisible,
      CrostiniSshfs::CrostiniSshfsResult::kNotDefaultContainer, 1);
  histogram_tester.ExpectTotalCount(kCrostiniMetricMountTimeTaken, 1);
}

TEST_F(CrostiniSshfsHelperTest, RecordBackgroundMetricIfBackground) {
  auto not_default =
      guest_os::GuestId(kCrostiniDefaultVmType, "vm_name", "container_name");
  SetContainerRunning(not_default);
  EXPECT_CALL(*disk_manager_, MountPath).Times(0);

  bool result = false;
  crostini_sshfs_->MountCrostiniFiles(not_default, base::DoNothing(), true);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(result);
  histogram_tester.ExpectUniqueSample(
      kCrostiniMetricMountResultBackground,
      CrostiniSshfs::CrostiniSshfsResult::kNotDefaultContainer, 1);
  histogram_tester.ExpectTotalCount(kCrostiniMetricMountResultUserVisible, 0);
}

TEST_F(CrostiniSshfsHelperTest, MultipleCallsAreQueuedAndOnlyMountOnce) {
  SetContainerRunning(DefaultContainerId());

  ExpectMountCalls(1);
  int successes = 0;
  crostini_sshfs_->MountCrostiniFiles(
      DefaultContainerId(),
      base::BindLambdaForTesting(
          [&successes](bool result) { successes += result; }),
      false);
  crostini_sshfs_->MountCrostiniFiles(
      DefaultContainerId(),
      base::BindLambdaForTesting(
          [&successes](bool result) { successes += result; }),
      false);
  task_environment_.RunUntilIdle();

  EXPECT_EQ(successes, 2);
  base::FilePath path;
  EXPECT_TRUE(
      storage::ExternalMountPoints::GetSystemInstance()->GetRegisteredPath(
          kMountName, &path));
  EXPECT_EQ(base::FilePath("/media/fuse/" + kMountName), path);
  histogram_tester.ExpectUniqueSample(
      kCrostiniMetricMountResultUserVisible,
      CrostiniSshfs::CrostiniSshfsResult::kSuccess, 2);
  histogram_tester.ExpectTotalCount(kCrostiniMetricMountTimeTaken, 2);
}

TEST_F(CrostiniSshfsHelperTest, CanRemountAfterUnmount) {
  SetContainerRunning(DefaultContainerId());
  ExpectMountCalls(2);
  EXPECT_CALL(*disk_manager_, UnmountPath)
      .WillOnce(testing::Invoke(
          [this](const std::string& mount_path,
                 DiskMountManager::UnmountPathCallback callback) {
            EXPECT_EQ(mount_path, "/media/fuse/" + kMountName);
            std::move(callback).Run(ash::MountError::kSuccess);
          }));

  crostini_sshfs_->MountCrostiniFiles(
      DefaultContainerId(),
      base::BindLambdaForTesting([](bool res) { EXPECT_TRUE(res); }), false);
  task_environment_.RunUntilIdle();
  crostini_sshfs_->UnmountCrostiniFiles(
      DefaultContainerId(),
      base::BindLambdaForTesting([](bool res) { EXPECT_TRUE(res); }));
  task_environment_.RunUntilIdle();
  crostini_sshfs_->MountCrostiniFiles(
      DefaultContainerId(),
      base::BindLambdaForTesting([](bool res) { EXPECT_TRUE(res); }), false);
  task_environment_.RunUntilIdle();

  base::FilePath path;
  EXPECT_TRUE(
      storage::ExternalMountPoints::GetSystemInstance()->GetRegisteredPath(
          kMountName, &path));
  EXPECT_EQ(base::FilePath("/media/fuse/" + kMountName), path);
  histogram_tester.ExpectUniqueSample(
      kCrostiniMetricMountResultUserVisible,
      CrostiniSshfs::CrostiniSshfsResult::kSuccess, 2);
  histogram_tester.ExpectTotalCount(kCrostiniMetricMountTimeTaken, 2);
  histogram_tester.ExpectUniqueSample(kCrostiniMetricUnmount, true, 1);
  histogram_tester.ExpectTotalCount(kCrostiniMetricUnmountTimeTaken, 1);
}

TEST_F(CrostiniSshfsHelperTest, ContainerShutdownClearsMountStatus) {
  SetContainerRunning(DefaultContainerId());
  ExpectMountCalls(2);
  crostini_sshfs_->MountCrostiniFiles(
      DefaultContainerId(),
      base::BindLambdaForTesting([](bool res) { EXPECT_TRUE(res); }), false);
  task_environment_.RunUntilIdle();
  crostini_sshfs_->OnContainerShutdown(DefaultContainerId());
  task_environment_.RunUntilIdle();
  crostini_sshfs_->MountCrostiniFiles(
      DefaultContainerId(),
      base::BindLambdaForTesting([](bool res) { EXPECT_TRUE(res); }), true);
  task_environment_.RunUntilIdle();

  base::FilePath path;
  EXPECT_TRUE(
      storage::ExternalMountPoints::GetSystemInstance()->GetRegisteredPath(
          kMountName, &path));
  EXPECT_EQ(base::FilePath("/media/fuse/" + kMountName), path);
}

}  // namespace crostini
