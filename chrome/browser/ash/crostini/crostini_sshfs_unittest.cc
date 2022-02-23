// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_sshfs.h"

#include <memory>

#include "ash/components/disks/disk_mount_manager.h"
#include "ash/components/disks/mock_disk_mount_manager.h"
#include "base/base64.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/crostini_manager_factory.h"
#include "chrome/browser/ash/crostini/crostini_test_helper.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/file_manager/fake_disk_mount_manager.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_manager/volume_manager_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/cicerone/cicerone_client.h"
#include "chromeos/dbus/concierge/concierge_client.h"
#include "chromeos/dbus/concierge/fake_concierge_client.h"
#include "chromeos/dbus/cros_disks/cros_disks_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/seneschal/seneschal_client.h"
#include "chromeos/dbus/vm_applications/apps.pb.h"
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
    chromeos::DBusThreadManager::Initialize();
    chromeos::CiceroneClient::InitializeFake();
    chromeos::ConciergeClient::InitializeFake();
    chromeos::SeneschalClient::InitializeFake();
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

    std::string known_hosts;
    base::Base64Encode("[hostname]:2222 pubkey", &known_hosts);
    std::string identity;
    base::Base64Encode("privkey", &identity);
    default_mount_options_ = {"UserKnownHostsBase64=" + known_hosts,
                              "IdentityBase64=" + identity, "Port=2222"};
    fake_concierge_client_ = chromeos::FakeConciergeClient::Get();
  }

  CrostiniSshfsHelperTest(const CrostiniSshfsHelperTest&) = delete;
  CrostiniSshfsHelperTest& operator=(const CrostiniSshfsHelperTest&) = delete;

  ~CrostiniSshfsHelperTest() override {
    storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
        kMountName);
    file_manager::VolumeManagerFactory::GetInstance()->SetTestingFactory(
        profile_.get(), BrowserContextKeyedServiceFactory::TestingFactory{});
    DiskMountManager::Shutdown();
    crostini_sshfs_.reset();
    crostini_test_helper_.reset();
    profile_.reset();
    chromeos::SeneschalClient::Shutdown();
    chromeos::ConciergeClient::Shutdown();
    chromeos::CiceroneClient::Shutdown();
    chromeos::DBusThreadManager::Shutdown();
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
      chromeos::MountType type,
      chromeos::MountAccessMode access_mode,
      ash::disks::DiskMountManager::MountPathCallback callback) {
    auto event = DiskMountManager::MountEvent::MOUNTING;
    auto code = chromeos::MountError::MOUNT_ERROR_NONE;
    auto info = DiskMountManager::MountPointInfo(
        "sshfs://username@hostname:", "/media/fuse/" + kMountName,
        chromeos::MOUNT_TYPE_NETWORK_STORAGE, ash::disks::MOUNT_CONDITION_NONE);
    disk_manager_->NotifyMountEvent(event, code, info);
    std::move(callback).Run(code, info);
  }

  void ExpectMountCalls(int n) {
    EXPECT_CALL(
        *disk_manager_,
        MountPath("sshfs://username@hostname:", "", kMountName,
                  default_mount_options_, chromeos::MOUNT_TYPE_NETWORK_STORAGE,
                  chromeos::MOUNT_ACCESS_MODE_READ_WRITE, _))
        .Times(n)
        .WillRepeatedly(
            Invoke(this, &CrostiniSshfsHelperTest::NotifyMountEvent));
  }

  void SetContainerRunning(ContainerId container) {
    auto* manager = CrostiniManager::GetForProfile(profile());
    ContainerInfo info(container.container_name, "username", "homedir",
                       "1.2.3.4");
    manager->AddRunningVmForTesting(container.vm_name);
    manager->AddRunningContainerForTesting(container.vm_name, info);
  }

  content::BrowserTaskEnvironment task_environment_;
  ash::disks::MockDiskMountManager* disk_manager_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<CrostiniTestHelper> crostini_test_helper_;
  const std::string kMountName = "crostini_test_termina_penguin";
  std::vector<std::string> default_mount_options_;
  chromeos::FakeConciergeClient* fake_concierge_client_;
  std::unique_ptr<file_manager::VolumeManager> volume_manager_;
  std::unique_ptr<CrostiniSshfs> crostini_sshfs_;
  CrostiniManager* crostini_manager_;
  base::HistogramTester histogram_tester{};
};

TEST_F(CrostiniSshfsHelperTest, MountDiskMountsDisk) {
  SetContainerRunning(ContainerId::GetDefault());
  ExpectMountCalls(1);
  bool result = false;

  crostini_sshfs_->MountCrostiniFiles(
      ContainerId::GetDefault(),
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
      ContainerId::GetDefault(),
      base::BindLambdaForTesting([&result](bool res) { result = res; }), false);
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(result);
  EXPECT_EQ(fake_concierge_client_->get_container_ssh_keys_call_count(), 0);
  histogram_tester.ExpectUniqueSample(
      kCrostiniMetricMountResultUserVisible,
      CrostiniSshfs::CrostiniSshfsResult::kContainerNotRunning, 1);
  histogram_tester.ExpectTotalCount(kCrostiniMetricMountTimeTaken, 1);
}

TEST_F(CrostiniSshfsHelperTest, OnlyDefaultContainerSupported) {
  auto not_default = ContainerId("vm_name", "container_name");
  SetContainerRunning(not_default);
  EXPECT_CALL(*disk_manager_, MountPath).Times(0);

  bool result = false;
  crostini_sshfs_->MountCrostiniFiles(
      not_default,
      base::BindLambdaForTesting([&result](bool res) { result = res; }), false);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(result);
  EXPECT_EQ(fake_concierge_client_->get_container_ssh_keys_call_count(), 0);
  histogram_tester.ExpectUniqueSample(
      kCrostiniMetricMountResultUserVisible,
      CrostiniSshfs::CrostiniSshfsResult::kNotDefaultContainer, 1);
  histogram_tester.ExpectTotalCount(kCrostiniMetricMountTimeTaken, 1);
}

TEST_F(CrostiniSshfsHelperTest, RecordBackgroundMetricIfBackground) {
  auto not_default = ContainerId("vm_name", "container_name");
  SetContainerRunning(not_default);
  EXPECT_CALL(*disk_manager_, MountPath).Times(0);

  bool result = false;
  crostini_sshfs_->MountCrostiniFiles(not_default, base::DoNothing(), true);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(result);
  EXPECT_EQ(fake_concierge_client_->get_container_ssh_keys_call_count(), 0);
  histogram_tester.ExpectUniqueSample(
      kCrostiniMetricMountResultBackground,
      CrostiniSshfs::CrostiniSshfsResult::kNotDefaultContainer, 1);
  histogram_tester.ExpectTotalCount(kCrostiniMetricMountResultUserVisible, 0);
}

TEST_F(CrostiniSshfsHelperTest, MultipleCallsAreQueuedAndOnlyMountOnce) {
  SetContainerRunning(ContainerId::GetDefault());

  ExpectMountCalls(1);
  int successes = 0;
  crostini_sshfs_->MountCrostiniFiles(
      ContainerId::GetDefault(),
      base::BindLambdaForTesting(
          [&successes](bool result) { successes += result; }),
      false);
  crostini_sshfs_->MountCrostiniFiles(
      ContainerId::GetDefault(),
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
  EXPECT_EQ(fake_concierge_client_->get_container_ssh_keys_call_count(), 1);
  histogram_tester.ExpectUniqueSample(
      kCrostiniMetricMountResultUserVisible,
      CrostiniSshfs::CrostiniSshfsResult::kSuccess, 2);
  histogram_tester.ExpectTotalCount(kCrostiniMetricMountTimeTaken, 2);
}

TEST_F(CrostiniSshfsHelperTest, CanRemountAfterUnmount) {
  SetContainerRunning(ContainerId::GetDefault());
  ExpectMountCalls(2);
  EXPECT_CALL(*disk_manager_, UnmountPath)
      .WillOnce(testing::Invoke(
          [this](const std::string& mount_path,
                 DiskMountManager::UnmountPathCallback callback) {
            EXPECT_EQ(mount_path, "/media/fuse/" + kMountName);
            std::move(callback).Run(chromeos::MOUNT_ERROR_NONE);
          }));

  crostini_sshfs_->MountCrostiniFiles(
      ContainerId::GetDefault(),
      base::BindLambdaForTesting([](bool res) { EXPECT_TRUE(res); }), false);
  task_environment_.RunUntilIdle();
  crostini_sshfs_->UnmountCrostiniFiles(
      ContainerId::GetDefault(),
      base::BindLambdaForTesting([](bool res) { EXPECT_TRUE(res); }));
  task_environment_.RunUntilIdle();
  crostini_sshfs_->MountCrostiniFiles(
      ContainerId::GetDefault(),
      base::BindLambdaForTesting([](bool res) { EXPECT_TRUE(res); }), false);
  task_environment_.RunUntilIdle();

  base::FilePath path;
  EXPECT_TRUE(
      storage::ExternalMountPoints::GetSystemInstance()->GetRegisteredPath(
          kMountName, &path));
  EXPECT_EQ(base::FilePath("/media/fuse/" + kMountName), path);
  EXPECT_EQ(fake_concierge_client_->get_container_ssh_keys_call_count(), 2);
  histogram_tester.ExpectUniqueSample(
      kCrostiniMetricMountResultUserVisible,
      CrostiniSshfs::CrostiniSshfsResult::kSuccess, 2);
  histogram_tester.ExpectTotalCount(kCrostiniMetricMountTimeTaken, 2);
  histogram_tester.ExpectUniqueSample(kCrostiniMetricUnmount, true, 1);
  histogram_tester.ExpectTotalCount(kCrostiniMetricUnmountTimeTaken, 1);
}

TEST_F(CrostiniSshfsHelperTest, ContainerShutdownClearsMountStatus) {
  SetContainerRunning(ContainerId::GetDefault());
  ExpectMountCalls(2);
  crostini_sshfs_->MountCrostiniFiles(
      ContainerId::GetDefault(),
      base::BindLambdaForTesting([](bool res) { EXPECT_TRUE(res); }), false);
  task_environment_.RunUntilIdle();
  crostini_sshfs_->OnContainerShutdown(ContainerId::GetDefault());
  task_environment_.RunUntilIdle();
  crostini_sshfs_->MountCrostiniFiles(
      ContainerId::GetDefault(),
      base::BindLambdaForTesting([](bool res) { EXPECT_TRUE(res); }), true);
  task_environment_.RunUntilIdle();

  base::FilePath path;
  EXPECT_TRUE(
      storage::ExternalMountPoints::GetSystemInstance()->GetRegisteredPath(
          kMountName, &path));
  EXPECT_EQ(base::FilePath("/media/fuse/" + kMountName), path);
  EXPECT_EQ(fake_concierge_client_->get_container_ssh_keys_call_count(), 2);
}

}  // namespace crostini
