// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_manager.h"

#include <memory>
#include <optional>

#include "ash/constants/ash_features.h"
#include "base/barrier_closure.h"
#include "base/files/scoped_temp_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/ash/crostini/ansible/ansible_management_service.h"
#include "chrome/browser/ash/crostini/ansible/ansible_management_test_helper.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/crostini/crostini_simple_types.h"
#include "chrome/browser/ash/crostini/crostini_test_util.h"
#include "chrome/browser/ash/crostini/crostini_types.mojom-shared.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/crostini/fake_crostini_features.h"
#include "chrome/browser/ash/guest_os/guest_os_pref_names.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker.h"
#include "chrome/browser/ash/guest_os/guest_os_share_path.h"
#include "chrome/browser/ash/guest_os/guest_os_share_path_factory.h"
#include "chrome/browser/ash/guest_os/public/guest_os_service.h"
#include "chrome/browser/ash/guest_os/public/guest_os_service_factory.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/browser/ui/views/crostini/crostini_ansible_software_config_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/browser_process_platform_part_test_api_chromeos.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/anomaly_detector/anomaly_detector_client.h"
#include "chromeos/ash/components/dbus/anomaly_detector/fake_anomaly_detector_client.h"
#include "chromeos/ash/components/dbus/chunneld/chunneld_client.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_service.pb.h"
#include "chromeos/ash/components/dbus/cicerone/fake_cicerone_client.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/concierge/fake_concierge_client.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"
#include "chromeos/ash/components/dbus/seneschal/seneschal_client.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/dbus/vm_concierge/concierge_service.pb.h"
#include "chromeos/ash/components/disks/mock_disk_mount_manager.h"
#include "components/account_id/account_id.h"
#include "components/component_updater/ash/fake_component_manager_ash.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/cpp/test/fake_usb_device_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crostini {
using base::test::TestFuture;

namespace {

const char kVmName[] = "vm_name";
const char kContainerName[] = "container_name";
const char kPackageID[] = "package;1;;";
constexpr int64_t kDiskSizeBytes = 4ll * 1024 * 1024 * 1024;  // 4 GiB
const char kTerminaKernelVersion[] =
    "4.19.56-05556-gca219a5b1086 #3 SMP PREEMPT Mon Jul 1 14:36:38 CEST 2019";
const char kCrostiniCorruptionHistogram[] = "Crostini.FilesystemCorruption";
constexpr auto kLongTime = base::Days(10);

class TestRestartObserver : public CrostiniManager::RestartObserver {
 public:
  void OnStageStarted(mojom::InstallerState stage) override {
    stages.push_back(stage);
  }
  std::vector<mojom::InstallerState> stages;
};

}  // namespace

class CrostiniManagerTest : public testing::Test {
 public:
  void SendVmStoppedSignal() {
    vm_tools::concierge::VmStoppedSignal signal;
    signal.set_name(kVmName);
    signal.set_owner_id("test");
    crostini_manager_->OnVmStopped(signal);
  }

  void CreateDiskImageFailureCallback(base::OnceClosure closure,
                                      CrostiniResult result,
                                      const base::FilePath& file_path) {
    EXPECT_EQ(fake_concierge_client_->create_disk_image_call_count(), 0);
    EXPECT_EQ(result, CrostiniResult::CLIENT_ERROR);
    std::move(closure).Run();
  }

  void CreateDiskImageSuccessCallback(base::OnceClosure closure,
                                      CrostiniResult result,
                                      const base::FilePath& file_path) {
    EXPECT_GE(fake_concierge_client_->create_disk_image_call_count(), 1);
    EXPECT_EQ(result, CrostiniResult::SUCCESS);
    std::move(closure).Run();
  }

  base::ScopedFD TestFileDescriptor() {
    base::File file(base::FilePath("/dev/null"),
                    base::File::FLAG_OPEN | base::File::FLAG_WRITE);
    base::ScopedFD fd(file.TakePlatformFile());
    return fd;
  }

  void AttachUsbDeviceCallback(base::OnceClosure closure,
                               bool expected_success,
                               bool success,
                               uint8_t guest_port) {
    EXPECT_GE(fake_concierge_client_->attach_usb_device_call_count(), 1);
    EXPECT_EQ(expected_success, success);
    std::move(closure).Run();
  }

  void DetachUsbDeviceCallback(base::OnceClosure closure,
                               bool expected_called,
                               bool expected_success,
                               bool success) {
    EXPECT_EQ(fake_concierge_client_->detach_usb_device_call_count(),
              expected_called);
    EXPECT_EQ(expected_success, success);
    std::move(closure).Run();
  }

  void EnsureTerminaInstalled() {
    TestFuture<CrostiniResult> result_future;
    crostini_manager()->InstallTermina(result_future.GetCallback());
    EXPECT_EQ(result_future.Get(), CrostiniResult::SUCCESS);
  }

  CrostiniManagerTest()
      : task_environment_(content::BrowserTaskEnvironment::REAL_IO_THREAD,
                          base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        local_state_(std::make_unique<ScopedTestingLocalState>(
            TestingBrowserProcess::GetGlobal())),
        browser_part_(g_browser_process->platform_part()) {
    ash::AnomalyDetectorClient::InitializeFake();
    ash::ChunneldClient::InitializeFake();
    ash::CiceroneClient::InitializeFake();
    ash::ConciergeClient::InitializeFake();
    ash::DebugDaemonClient::InitializeFake();
    ash::SeneschalClient::InitializeFake();
    fake_cicerone_client_ = ash::FakeCiceroneClient::Get();
    fake_concierge_client_ = ash::FakeConciergeClient::Get();
    fake_anomaly_detector_client_ =
        static_cast<ash::FakeAnomalyDetectorClient*>(
            ash::AnomalyDetectorClient::Get());
  }

  CrostiniManagerTest(const CrostiniManagerTest&) = delete;
  CrostiniManagerTest& operator=(const CrostiniManagerTest&) = delete;

  ~CrostiniManagerTest() override {
    ash::AnomalyDetectorClient::Shutdown();
    ash::SeneschalClient::Shutdown();
    ash::DebugDaemonClient::Shutdown();
    ash::ConciergeClient::Shutdown();
    ash::CiceroneClient::Shutdown();
    ash::ChunneldClient::Shutdown();
  }

  void SetUp() override {
    component_manager_ =
        base::MakeRefCounted<component_updater::FakeComponentManagerAsh>();
    component_manager_->set_supported_components({"cros-termina"});
    component_manager_->ResetComponentState(
        "cros-termina",
        component_updater::FakeComponentManagerAsh::ComponentInfo(
            component_updater::ComponentManagerAsh::Error::NONE,
            base::FilePath("/install/path"), base::FilePath("/mount/path")));
    browser_part_.InitializeComponentManager(component_manager_);
    ash::DlcserviceClient::InitializeFake();

    scoped_feature_list_.InitWithFeatures(
        {features::kCrostini, ash::features::kCrostiniMultiContainer}, {});
    fake_user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());
    profile_ = std::make_unique<TestingProfile>();
    crostini_manager_ = CrostiniManager::GetForProfile(profile_.get());

    // Login user for crostini, link gaia for DriveFS.
    AccountId account_id = AccountId::FromUserEmailGaiaId(
        profile()->GetProfileUserName(), "12345");
    fake_user_manager_->AddUser(account_id);
    fake_user_manager_->LoginUser(account_id);

    mojo::Remote<device::mojom::UsbDeviceManager> fake_usb_manager;
    fake_usb_manager_.AddReceiver(
        fake_usb_manager.BindNewPipeAndPassReceiver());

    g_browser_process->platform_part()
        ->InitializeSchedulerConfigurationManager();

    TestingBrowserProcess::GetGlobal()->SetSystemNotificationHelper(
        std::make_unique<SystemNotificationHelper>());

    vm_tools::cicerone::OsRelease os_release;
    base::HistogramTester histogram_tester{};
    os_release.set_pretty_name("Debian GNU/Linux 12 (bookworm)");
    os_release.set_version_id("12");
    os_release.set_id("debian");
    fake_cicerone_client_->set_lxd_container_os_release(os_release);
  }

  void TearDown() override {
    g_browser_process->platform_part()->ShutdownSchedulerConfigurationManager();
    crostini_manager_->Shutdown();
    profile_.reset();
    fake_user_manager_.Reset();
    ash::DlcserviceClient::Shutdown();
    browser_part_.ShutdownComponentManager();
    component_manager_.reset();
  }

 protected:
  Profile* profile() { return profile_.get(); }
  CrostiniManager* crostini_manager() { return crostini_manager_; }
  const guest_os::GuestId& container_id() { return container_id_; }

  raw_ptr<ash::FakeCiceroneClient, DanglingUntriaged> fake_cicerone_client_;
  raw_ptr<ash::FakeConciergeClient, DanglingUntriaged> fake_concierge_client_;
  raw_ptr<ash::FakeAnomalyDetectorClient, DanglingUntriaged>
      fake_anomaly_detector_client_;

  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<CrostiniManager, DanglingUntriaged> crostini_manager_;
  const guest_os::GuestId container_id_ =
      guest_os::GuestId(kCrostiniDefaultVmType, kVmName, kContainerName);
  device::FakeUsbDeviceManager fake_usb_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;

 private:
  std::unique_ptr<ScopedTestingLocalState> local_state_;
  scoped_refptr<component_updater::FakeComponentManagerAsh> component_manager_;
  BrowserProcessPlatformPartTestApi browser_part_;
};

TEST_F(CrostiniManagerTest, CreateDiskImageEmptyNameError) {
  TestFuture<CrostiniResult, const base::FilePath&> result_future;

  crostini_manager()->CreateDiskImage(
      "", vm_tools::concierge::STORAGE_CRYPTOHOME_ROOT, kDiskSizeBytes,
      result_future.GetCallback());
  EXPECT_TRUE(result_future.Wait());

  EXPECT_EQ(fake_concierge_client_->create_disk_image_call_count(), 0);
  EXPECT_EQ(result_future.Get<0>(), CrostiniResult::CLIENT_ERROR);
}

TEST_F(CrostiniManagerTest, CreateDiskImageStorageLocationError) {
  TestFuture<CrostiniResult, const base::FilePath&> result_future;

  crostini_manager()->CreateDiskImage(
      kVmName,
      vm_tools::concierge::StorageLocation_INT_MIN_SENTINEL_DO_NOT_USE_,
      kDiskSizeBytes, result_future.GetCallback());
  EXPECT_TRUE(result_future.Wait());

  EXPECT_EQ(fake_concierge_client_->create_disk_image_call_count(), 0);
  EXPECT_EQ(result_future.Get<0>(), CrostiniResult::CLIENT_ERROR);
}

TEST_F(CrostiniManagerTest, CreateDiskImageSuccess) {
  TestFuture<CrostiniResult, const base::FilePath&> result_future;

  crostini_manager()->CreateDiskImage(
      kVmName, vm_tools::concierge::STORAGE_CRYPTOHOME_ROOT, kDiskSizeBytes,
      result_future.GetCallback());
  EXPECT_TRUE(result_future.Wait());

  EXPECT_GE(fake_concierge_client_->create_disk_image_call_count(), 1);
  EXPECT_EQ(result_future.Get<0>(), CrostiniResult::SUCCESS);
}

TEST_F(CrostiniManagerTest, StartTerminaVmNameError) {
  TestFuture<bool> success_future;

  const base::FilePath& disk_path = base::FilePath("unused");
  crostini_manager()->StartTerminaVm("", disk_path, 0,
                                     success_future.GetCallback());

  EXPECT_FALSE(success_future.Get());
  EXPECT_EQ(fake_concierge_client_->start_vm_call_count(), 0);
}

TEST_F(CrostiniManagerTest, StartTerminaVmAnomalyDetectorNotConnectedError) {
  TestFuture<bool> success_future;
  const base::FilePath& disk_path = base::FilePath("unused");

  fake_anomaly_detector_client_->set_guest_file_corruption_signal_connected(
      false);
  crostini_manager()->StartTerminaVm(kVmName, disk_path, 0,
                                     success_future.GetCallback());

  EXPECT_FALSE(success_future.Get());
  EXPECT_EQ(fake_concierge_client_->start_vm_call_count(), 0);
}

TEST_F(CrostiniManagerTest, StartTerminaVmDiskPathError) {
  TestFuture<bool> success_future;
  const base::FilePath& disk_path = base::FilePath();

  crostini_manager()->StartTerminaVm(kVmName, disk_path, 0,
                                     success_future.GetCallback());

  EXPECT_FALSE(success_future.Get());
  EXPECT_EQ(fake_concierge_client_->start_vm_call_count(), 0);
}

TEST_F(CrostiniManagerTest, StartTerminaVmMountError) {
  base::HistogramTester histogram_tester{};
  const base::FilePath& disk_path = base::FilePath("unused");

  vm_tools::concierge::StartVmResponse response;
  response.set_status(vm_tools::concierge::VM_STATUS_FAILURE);
  response.set_mount_result(vm_tools::concierge::StartVmResponse::FAILURE);
  fake_concierge_client_->set_start_vm_response(response);

  EnsureTerminaInstalled();
  TestFuture<bool> success_future;
  crostini_manager()->StartTerminaVm(kVmName, disk_path, 0,
                                     success_future.GetCallback());

  EXPECT_FALSE(success_future.Get());
  EXPECT_GE(fake_concierge_client_->start_vm_call_count(), 1);
  histogram_tester.ExpectUniqueSample(kCrostiniCorruptionHistogram,
                                      CorruptionStates::MOUNT_FAILED, 1);
}

TEST_F(CrostiniManagerTest, StartTerminaVmMountErrorThenSuccess) {
  base::HistogramTester histogram_tester{};
  const base::FilePath& disk_path = base::FilePath("unused");

  vm_tools::concierge::StartVmResponse response;
  response.set_status(vm_tools::concierge::VM_STATUS_STARTING);
  response.set_mount_result(
      vm_tools::concierge::StartVmResponse::PARTIAL_DATA_LOSS);
  fake_concierge_client_->set_start_vm_response(response);

  EnsureTerminaInstalled();
  TestFuture<bool> result_future;
  crostini_manager()->StartTerminaVm(kVmName, disk_path, 0,
                                     result_future.GetCallback());

  EXPECT_TRUE(result_future.Get());
  EXPECT_GE(fake_concierge_client_->start_vm_call_count(), 1);
  histogram_tester.ExpectUniqueSample(kCrostiniCorruptionHistogram,
                                      CorruptionStates::MOUNT_ROLLED_BACK, 1);
}

TEST_F(CrostiniManagerTest, StartTerminaVmSuccess) {
  base::HistogramTester histogram_tester{};
  const base::FilePath& disk_path = base::FilePath("unused");

  EnsureTerminaInstalled();
  TestFuture<bool> result_future;
  crostini_manager()->StartTerminaVm(kVmName, disk_path, 0,
                                     result_future.GetCallback());

  EXPECT_TRUE(result_future.Get());
  EXPECT_GE(fake_concierge_client_->start_vm_call_count(), 1);
  histogram_tester.ExpectTotalCount(kCrostiniCorruptionHistogram, 0);
}

TEST_F(CrostiniManagerTest, StartTerminaVmLowDiskNotification) {
  const base::FilePath& disk_path = base::FilePath("unused");
  NotificationDisplayServiceTester notification_service(nullptr);
  vm_tools::concierge::StartVmResponse response;

  response.set_free_bytes(0);
  response.set_free_bytes_has_value(true);
  response.set_success(true);
  response.set_status(::vm_tools::concierge::VmStatus::VM_STATUS_RUNNING);
  fake_concierge_client_->set_start_vm_response(response);

  EnsureTerminaInstalled();
  TestFuture<bool> result_future;
  crostini_manager()->StartTerminaVm(DefaultContainerId().vm_name, disk_path, 0,
                                     result_future.GetCallback());

  EXPECT_TRUE(result_future.Get());
  EXPECT_GE(fake_concierge_client_->start_vm_call_count(), 1);
  auto notification = notification_service.GetNotification("crostini_low_disk");
  EXPECT_NE(std::nullopt, notification);
}

TEST_F(CrostiniManagerTest,
       StartTerminaVmLowDiskNotificationNotShownIfNoValue) {
  const base::FilePath& disk_path = base::FilePath("unused");
  NotificationDisplayServiceTester notification_service(nullptr);
  vm_tools::concierge::StartVmResponse response;

  response.set_free_bytes(1234);
  response.set_free_bytes_has_value(false);
  response.set_success(true);
  response.set_status(::vm_tools::concierge::VmStatus::VM_STATUS_RUNNING);
  fake_concierge_client_->set_start_vm_response(response);

  EnsureTerminaInstalled();
  TestFuture<bool> result_future;
  crostini_manager()->StartTerminaVm(DefaultContainerId().vm_name, disk_path, 0,
                                     result_future.GetCallback());

  EXPECT_TRUE(result_future.Get());
  EXPECT_GE(fake_concierge_client_->start_vm_call_count(), 1);
  auto notification = notification_service.GetNotification("crostini_low_disk");
  EXPECT_EQ(std::nullopt, notification);
}

TEST_F(CrostiniManagerTest, OnStartTremplinRecordsRunningVm) {
  const base::FilePath& disk_path = base::FilePath("unused");
  const std::string owner_id = CryptohomeIdForProfile(profile());

  // Start the Vm.
  EnsureTerminaInstalled();
  TestFuture<bool> result_future;
  crostini_manager()->StartTerminaVm(kVmName, disk_path, 0,
                                     result_future.GetCallback());

  // Check that the Vm start is not recorded until tremplin starts.
  EXPECT_FALSE(crostini_manager()->IsVmRunning(kVmName));

  EXPECT_TRUE(result_future.Get());
  EXPECT_TRUE(crostini_manager()->IsVmRunning(kVmName));
}

TEST_F(CrostiniManagerTest, OnStartTremplinHappensEarlier) {
  fake_concierge_client_->set_send_tremplin_started_signal_delay(
      base::Milliseconds(1));
  fake_concierge_client_->set_send_start_vm_response_delay(
      base::Milliseconds(10));
  const base::FilePath& disk_path = base::FilePath("unused");
  const std::string owner_id = CryptohomeIdForProfile(profile());

  // Start the Vm.
  EnsureTerminaInstalled();
  TestFuture<bool> result_future;
  crostini_manager()->StartTerminaVm(kVmName, disk_path, 0,
                                     result_future.GetCallback());

  // Check that the Vm start is not recorded until tremplin starts.
  EXPECT_FALSE(crostini_manager()->IsVmRunning(kVmName));

  EXPECT_TRUE(result_future.Get());
  EXPECT_TRUE(crostini_manager()->IsVmRunning(kVmName));
}

TEST_F(CrostiniManagerTest, StopVmNameError) {
  TestFuture<CrostiniResult> result_future;

  crostini_manager()->StopVm("", result_future.GetCallback());

  EXPECT_EQ(result_future.Get(), CrostiniResult::CLIENT_ERROR);
  EXPECT_EQ(fake_concierge_client_->stop_vm_call_count(), 0);
}

TEST_F(CrostiniManagerTest, StopVmSuccess) {
  TestFuture<CrostiniResult> result_future;

  crostini_manager()->StopVm(kVmName, result_future.GetCallback());

  EXPECT_EQ(result_future.Get(), CrostiniResult::SUCCESS);
  EXPECT_GE(fake_concierge_client_->stop_vm_call_count(), 1);
}

TEST_F(CrostiniManagerTest, InstallLinuxPackageRootAccessError) {
  FakeCrostiniFeatures crostini_features;

  crostini_features.set_root_access_allowed(false);
  TestFuture<CrostiniResult> result_future;
  crostini_manager()->InstallLinuxPackage(container_id(), "/tmp/package.deb",
                                          result_future.GetCallback());

  EXPECT_EQ(result_future.Get(), CrostiniResult::INSTALL_LINUX_PACKAGE_FAILED);
}

TEST_F(CrostiniManagerTest, InstallLinuxPackageSignalNotConnectedError) {
  fake_cicerone_client_->set_install_linux_package_progress_signal_connected(
      false);
  TestFuture<CrostiniResult> result_future;
  crostini_manager()->InstallLinuxPackage(container_id(), "/tmp/package.deb",
                                          result_future.GetCallback());

  EXPECT_EQ(result_future.Get(), CrostiniResult::INSTALL_LINUX_PACKAGE_FAILED);
}

TEST_F(CrostiniManagerTest, InstallLinuxPackageSignalSuccess) {
  vm_tools::cicerone::InstallLinuxPackageResponse response;

  response.set_status(vm_tools::cicerone::InstallLinuxPackageResponse::STARTED);
  fake_cicerone_client_->set_install_linux_package_response(response);
  TestFuture<CrostiniResult> result_future;
  crostini_manager()->InstallLinuxPackage(container_id(), "/tmp/package.deb",
                                          result_future.GetCallback());

  EXPECT_EQ(result_future.Get(), CrostiniResult::SUCCESS);
}

TEST_F(CrostiniManagerTest, InstallLinuxPackageSignalFailure) {
  vm_tools::cicerone::InstallLinuxPackageResponse response;
  std::string failure_reason = "Unit tests can't install Linux packages!";

  response.set_status(vm_tools::cicerone::InstallLinuxPackageResponse::FAILED);
  response.set_failure_reason(failure_reason);
  fake_cicerone_client_->set_install_linux_package_response(response);
  TestFuture<CrostiniResult> result_future;
  crostini_manager()->InstallLinuxPackage(container_id(), "/tmp/package.deb",
                                          result_future.GetCallback());

  EXPECT_EQ(result_future.Get(), CrostiniResult::INSTALL_LINUX_PACKAGE_FAILED);
}

TEST_F(CrostiniManagerTest, InstallLinuxPackageSignalOperationBlocked) {
  vm_tools::cicerone::InstallLinuxPackageResponse response;
  response.set_status(
      vm_tools::cicerone::InstallLinuxPackageResponse::INSTALL_ALREADY_ACTIVE);
  fake_cicerone_client_->set_install_linux_package_response(response);

  TestFuture<CrostiniResult> result_future;
  crostini_manager()->InstallLinuxPackage(container_id(), "/tmp/package.deb",
                                          result_future.GetCallback());

  EXPECT_EQ(result_future.Get(),
            CrostiniResult::BLOCKING_OPERATION_ALREADY_ACTIVE);
}

TEST_F(CrostiniManagerTest, UninstallPackageOwningFileSignalNotConnectedError) {
  fake_cicerone_client_->set_uninstall_package_progress_signal_connected(false);
  TestFuture<CrostiniResult> result_future;
  crostini_manager()->UninstallPackageOwningFile(container_id(), "emacs",
                                                 result_future.GetCallback());

  EXPECT_EQ(result_future.Get(), CrostiniResult::UNINSTALL_PACKAGE_FAILED);
}

TEST_F(CrostiniManagerTest, UninstallPackageOwningFileSignalSuccess) {
  vm_tools::cicerone::UninstallPackageOwningFileResponse response;

  response.set_status(
      vm_tools::cicerone::UninstallPackageOwningFileResponse::STARTED);
  fake_cicerone_client_->set_uninstall_package_owning_file_response(response);
  TestFuture<CrostiniResult> result_future;
  crostini_manager()->UninstallPackageOwningFile(container_id(), "emacs",
                                                 result_future.GetCallback());

  EXPECT_EQ(result_future.Get(), CrostiniResult::SUCCESS);
}

TEST_F(CrostiniManagerTest, UninstallPackageOwningFileSignalFailure) {
  vm_tools::cicerone::UninstallPackageOwningFileResponse response;
  response.set_status(
      vm_tools::cicerone::UninstallPackageOwningFileResponse::FAILED);
  response.set_failure_reason("Didn't feel like it");
  fake_cicerone_client_->set_uninstall_package_owning_file_response(response);

  TestFuture<CrostiniResult> result_future;
  crostini_manager()->UninstallPackageOwningFile(container_id(), "emacs",
                                                 result_future.GetCallback());

  EXPECT_EQ(result_future.Get(), CrostiniResult::UNINSTALL_PACKAGE_FAILED);
}

TEST_F(CrostiniManagerTest, UninstallPackageOwningFileSignalOperationBlocked) {
  vm_tools::cicerone::UninstallPackageOwningFileResponse response;
  response.set_status(vm_tools::cicerone::UninstallPackageOwningFileResponse::
                          BLOCKING_OPERATION_IN_PROGRESS);
  fake_cicerone_client_->set_uninstall_package_owning_file_response(response);

  TestFuture<CrostiniResult> result_future;
  crostini_manager()->UninstallPackageOwningFile(container_id(), "emacs",
                                                 result_future.GetCallback());

  EXPECT_EQ(result_future.Get(),
            CrostiniResult::BLOCKING_OPERATION_ALREADY_ACTIVE);
}

TEST_F(CrostiniManagerTest, RegisterCreateOptions) {
  guest_os::AddContainerToPrefs(profile_.get(), crostini::DefaultContainerId(),
                                {});
  CrostiniManager::RestartOptions options;
  options.container_username = "penguininadesert";
  options.ansible_playbook = base::FilePath("pob.yaml");
  options.disk_size_bytes = 9001;
  options.image_server_url = "https://suspiciouswebsite.com";
  options.image_alias = "nothingtoseehereofficer";

  EXPECT_TRUE(crostini_manager()->RegisterCreateOptions(
      crostini::DefaultContainerId(), options));
}

TEST_F(CrostiniManagerTest, RegisterCreateOptions_FalseWhenExists) {
  guest_os::AddContainerToPrefs(profile_.get(), crostini::DefaultContainerId(),
                                {});
  CrostiniManager::RestartOptions options;
  options.container_username = "penguininadesert";
  options.ansible_playbook = base::FilePath("pob.yaml");
  options.disk_size_bytes = 9001;
  options.image_server_url = "https://suspiciouswebsite.com";
  options.image_alias = "nothingtoseehereofficer";

  EXPECT_TRUE(crostini_manager()->RegisterCreateOptions(
      crostini::DefaultContainerId(), options));
  EXPECT_FALSE(crostini_manager()->RegisterCreateOptions(
      crostini::DefaultContainerId(), options));
}

TEST_F(CrostiniManagerTest, SetCreateOptionsUsed) {
  guest_os::AddContainerToPrefs(profile_.get(), crostini::DefaultContainerId(),
                                {});
  CrostiniManager::RestartOptions options;

  options.container_username = "penguininadesert";
  options.ansible_playbook = base::FilePath("pob.yaml");
  options.disk_size_bytes = 9001;
  options.image_server_url = "https://suspiciouswebsite.com";
  options.image_alias = "nothingtoseehereofficer";
  EXPECT_TRUE(crostini_manager()->RegisterCreateOptions(
      crostini::DefaultContainerId(), options));
  crostini_manager()->SetCreateOptionsUsed(crostini::DefaultContainerId());

  const base::Value* create_options = guest_os::GetContainerPrefValue(
      profile_.get(), crostini::DefaultContainerId(),
      guest_os::prefs::kContainerCreateOptions);
  ASSERT_NE(create_options, nullptr);

  EXPECT_TRUE(
      create_options->GetDict().FindBool(prefs::kCrostiniCreateOptionsUsedKey));
}

TEST_F(CrostiniManagerTest, FetchCreateOptions_MergesSharePaths) {
  guest_os::AddContainerToPrefs(profile_.get(), crostini::DefaultContainerId(),
                                {});
  CrostiniManager::RestartOptions options;
  options.share_paths = {base::FilePath("ah"), base::FilePath("ah"),
                         base::FilePath("ah"), base::FilePath("ah")};
  options.container_username = "penguininadesert";
  options.ansible_playbook = base::FilePath("pob.yaml");
  options.disk_size_bytes = 9001;
  options.image_server_url = "https://suspiciouswebsite.com";
  options.image_alias = "nothingtoseehereofficer";

  EXPECT_TRUE(crostini_manager()->RegisterCreateOptions(
      crostini::DefaultContainerId(), options));

  CrostiniManager::RestartOptions options2;
  options2.share_paths = {base::FilePath("oh")};
  EXPECT_FALSE(crostini_manager()->FetchCreateOptions(
      crostini::DefaultContainerId(), &options2));
  EXPECT_TRUE(options.container_username == options2.container_username);
  EXPECT_THAT(
      options2.share_paths,
      testing::ContainerEq(std::vector<base::FilePath>(
          {base::FilePath("oh"), base::FilePath("ah"), base::FilePath("ah"),
           base::FilePath("ah"), base::FilePath("ah")})));
  EXPECT_TRUE(options.ansible_playbook == options2.ansible_playbook);
  EXPECT_TRUE(options.disk_size_bytes == options2.disk_size_bytes);
  EXPECT_TRUE(options.image_server_url == options2.image_server_url);
  EXPECT_TRUE(options.image_alias == options2.image_alias);
}

TEST_F(CrostiniManagerTest, FetchCreateOptions_FalseWhenUnused) {
  guest_os::AddContainerToPrefs(profile_.get(), crostini::DefaultContainerId(),
                                {});
  CrostiniManager::RestartOptions options;
  options.container_username = "penguininadesert";
  options.ansible_playbook = base::FilePath("pob.yaml");
  options.disk_size_bytes = 9001;
  options.image_server_url = "https://suspiciouswebsite.com";
  options.image_alias = "nothingtoseehereofficer";

  EXPECT_TRUE(crostini_manager()->RegisterCreateOptions(
      crostini::DefaultContainerId(), options));

  CrostiniManager::RestartOptions options2;
  EXPECT_FALSE(crostini_manager()->FetchCreateOptions(
      crostini::DefaultContainerId(), &options2));
  EXPECT_TRUE(options.container_username == options2.container_username);
  EXPECT_TRUE(options.ansible_playbook == options2.ansible_playbook);
  EXPECT_TRUE(options.disk_size_bytes == options2.disk_size_bytes);
  EXPECT_TRUE(options.image_server_url == options2.image_server_url);
  EXPECT_TRUE(options.image_alias == options2.image_alias);
}

TEST_F(CrostiniManagerTest, FetchCreateOptions_TrueWhenUsed) {
  guest_os::AddContainerToPrefs(profile_.get(), crostini::DefaultContainerId(),
                                {});
  CrostiniManager::RestartOptions options;
  options.container_username = "penguininadesert";
  options.ansible_playbook = base::FilePath("pob.yaml");
  options.disk_size_bytes = 9001;
  options.image_server_url = "https://suspiciouswebsite.com";
  options.image_alias = "nothingtoseehereofficer";

  EXPECT_TRUE(crostini_manager()->RegisterCreateOptions(
      crostini::DefaultContainerId(), options));

  crostini_manager()->SetCreateOptionsUsed(crostini::DefaultContainerId());

  CrostiniManager::RestartOptions options2;
  EXPECT_TRUE(crostini_manager()->FetchCreateOptions(
      crostini::DefaultContainerId(), &options2));
  EXPECT_TRUE(options.container_username == options2.container_username);
  EXPECT_TRUE(options.ansible_playbook == options2.ansible_playbook);
  EXPECT_TRUE(options.disk_size_bytes == options2.disk_size_bytes);
  EXPECT_TRUE(options.image_server_url == options2.image_server_url);
  EXPECT_TRUE(options.image_alias == options2.image_alias);
}

class CrostiniManagerRestartTest : public CrostiniManagerTest,
                                   public CrostiniManager::RestartObserver {
 public:
  void SetUp() override {
    CrostiniManagerTest::SetUp();

    // Most tests are for a non-initial install, so return by default that a
    // disk already exists to avoid extra error logs.
    SetCreateDiskImageResponse(vm_tools::concierge::DISK_STATUS_EXISTS);
  }

  // CrostiniManager::RestartObserver
  void OnStageStarted(mojom::InstallerState stage) override {
    on_stage_started_.Run(stage);
  }

 protected:
  // Convenience functions that forward the restart request to the
  // CrostiniManager
  CrostiniManager::RestartId RestartCrostini(
      guest_os::GuestId container_id,
      CrostiniManager::CrostiniResultCallback callback,
      RestartObserver* observer = nullptr) {
    return crostini_manager()->RestartCrostini(container_id,
                                               std::move(callback), observer);
  }

  CrostiniManager::RestartId RestartCrostiniWithOptions(
      guest_os::GuestId container_id,
      CrostiniManager::RestartOptions options,
      CrostiniManager::CrostiniResultCallback callback,
      RestartObserver* observer = nullptr) {
    return crostini_manager()->RestartCrostiniWithOptions(
        container_id, std::move(options), std::move(callback), observer);
  }

  void RunUntilState(mojom::InstallerState target_state) {
    base::RunLoop run_loop;
    on_stage_started_ =
        base::BindLambdaForTesting([&](mojom::InstallerState state) {
          if (state == target_state) {
            run_loop.Quit();
          }
        });
    run_loop.Run();
  }

  void ExpectRestarterUmaCount(int count, bool is_install = false) {
    histogram_tester_.ExpectTotalCount("Crostini.Restarter.Started", count);
    if (is_install) {
      histogram_tester_.ExpectTotalCount("Crostini.RestarterResult.Installer",
                                         count);
    } else {
      histogram_tester_.ExpectTotalCount("Crostini.RestarterResult", count);
    }
    histogram_tester_.ExpectTotalCount("Crostini.Installer.Started", 0);
  }

  void SetCreateDiskImageResponse(vm_tools::concierge::DiskImageStatus status) {
    vm_tools::concierge::CreateDiskImageResponse response;
    response.set_status(status);
    response.set_disk_path("foo");
    fake_concierge_client_->set_create_disk_image_response(response);
  }

  const CrostiniManager::RestartId uninitialized_id_ =
      CrostiniManager::kUninitializedRestartId;

  raw_ptr<ash::disks::MockDiskMountManager> disk_mount_manager_mock_;
  base::HistogramTester histogram_tester_{};

  base::RepeatingCallback<void(mojom::InstallerState)> on_stage_started_ =
      base::DoNothing();
};

TEST_F(CrostiniManagerRestartTest, RestartSuccess) {
  TestFuture<CrostiniResult> result_future;
  RestartCrostini(container_id(), result_future.GetCallback(), this);

  EXPECT_EQ(result_future.Get(), CrostiniResult::SUCCESS);

  EXPECT_GE(fake_concierge_client_->create_disk_image_call_count(), 1);
  EXPECT_GE(fake_concierge_client_->start_vm_call_count(), 1);

  auto req = fake_cicerone_client_->get_setup_lxd_container_user_request();
  EXPECT_EQ(req.container_username(),
            DefaultContainerUserNameForProfile(profile()));

  ExpectRestarterUmaCount(1);
  histogram_tester_.ExpectTotalCount("Crostini.RestarterTimeInState2.Start", 1);
  histogram_tester_.ExpectTotalCount(
      "Crostini.RestarterTimeInState2.StartContainer", 1);
  histogram_tester_.ExpectBucketCount(
      "Crostini.SetUpLxdContainerUser.UnknownResult", false, 1);
}

TEST_F(CrostiniManagerRestartTest, CrostiniNotAllowed) {
  FakeCrostiniFeatures crostini_features;
  crostini_features.set_is_allowed_now(false);
  TestFuture<CrostiniResult> result_future;
  CrostiniManager::RestartId restart_id =
      RestartCrostini(container_id(), result_future.GetCallback(), this);

  EXPECT_EQ(result_future.Get(), CrostiniResult::NOT_ALLOWED);

  EXPECT_FALSE(crostini_manager()->IsRestartPending(restart_id));
  histogram_tester_.ExpectBucketCount("Crostini.RestarterResult",
                                      CrostiniResult::NOT_ALLOWED, 1);
}

TEST_F(CrostiniManagerRestartTest, UncleanRestartReportsMetricToUncleanBucket) {
  crostini_manager()->SetUncleanStartupForTesting(true);
  TestFuture<CrostiniResult> result_future;
  RestartCrostini(container_id(), result_future.GetCallback(), this);

  EXPECT_EQ(result_future.Get(), CrostiniResult::SUCCESS);

  EXPECT_GE(fake_concierge_client_->create_disk_image_call_count(), 1);
  EXPECT_GE(fake_concierge_client_->start_vm_call_count(), 1);

  histogram_tester_.ExpectTotalCount("Crostini.Restarter.Started", 1);
  histogram_tester_.ExpectTotalCount("Crostini.RestarterResult", 1);
  histogram_tester_.ExpectTotalCount("Crostini.Installer.Started", 0);
}

TEST_F(CrostiniManagerRestartTest, RestartDelayAndSuccessWhenVmStopping) {
  crostini_manager()->AddStoppingVmForTesting(kVmName);
  on_stage_started_ =
      base::BindLambdaForTesting([this](mojom::InstallerState state) {
        if (state == mojom::InstallerState::kStart) {
          // This tells the restarter that the vm has stopped, and it should
          // resume the restarting process.
          SendVmStoppedSignal();
        }
      });

  TestFuture<CrostiniResult> result_future;
  RestartCrostini(container_id(), result_future.GetCallback(), this);

  EXPECT_EQ(result_future.Get(), CrostiniResult::SUCCESS);

  EXPECT_GE(fake_concierge_client_->create_disk_image_call_count(), 1);
  EXPECT_GE(fake_concierge_client_->start_vm_call_count(), 1);

  ExpectRestarterUmaCount(1);
}

TEST_F(CrostiniManagerRestartTest, RestartSuccessWithOptions) {
  CrostiniManager::RestartOptions options;
  options.container_username = "helloworld";
  TestFuture<CrostiniResult> result_future;
  RestartCrostiniWithOptions(container_id(), std::move(options),
                             result_future.GetCallback(), this);

  EXPECT_EQ(result_future.Get(), CrostiniResult::SUCCESS);

  EXPECT_GE(fake_concierge_client_->create_disk_image_call_count(), 1);
  EXPECT_GE(fake_concierge_client_->start_vm_call_count(), 1);
  auto req = fake_cicerone_client_->get_setup_lxd_container_user_request();
  EXPECT_EQ(req.container_username(), "helloworld");
  ExpectRestarterUmaCount(1);
}

TEST_F(CrostiniManagerRestartTest, CancelDuringCreateDiskImage) {
  TestFuture<CrostiniResult> result_future;
  CrostiniManager::RestartId restart_id =
      RestartCrostini(container_id(), result_future.GetCallback(), this);
  RunUntilState(mojom::InstallerState::kCreateDiskImage);

  EXPECT_EQ(fake_concierge_client_->create_disk_image_call_count(), 1);

  crostini_manager()->CancelRestartCrostini(restart_id);
  EXPECT_EQ(result_future.Get(), CrostiniResult::RESTART_REQUEST_CANCELLED);

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(crostini_manager()->HasRestarterForTesting(container_id()));
  EXPECT_EQ(fake_concierge_client_->start_vm_call_count(), 0);
  ExpectRestarterUmaCount(1);
}

TEST_F(CrostiniManagerRestartTest, CancelDuringStartTerminaVm) {
  TestFuture<CrostiniResult> result_future;
  CrostiniManager::RestartId restart_id =
      RestartCrostini(container_id(), result_future.GetCallback(), this);
  RunUntilState(mojom::InstallerState::kStartTerminaVm);

  EXPECT_EQ(fake_concierge_client_->start_vm_call_count(), 1);

  crostini_manager()->CancelRestartCrostini(restart_id);
  EXPECT_EQ(result_future.Get(), CrostiniResult::RESTART_REQUEST_CANCELLED);

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(crostini_manager()->HasRestarterForTesting(container_id()));
  EXPECT_EQ(fake_cicerone_client_->start_lxd_count(), 0);
}

TEST_F(CrostiniManagerRestartTest, CancelDuringStartLxd) {
  TestFuture<CrostiniResult> result_future;
  CrostiniManager::RestartId restart_id =
      RestartCrostini(container_id(), result_future.GetCallback(), this);
  RunUntilState(mojom::InstallerState::kStartLxd);

  EXPECT_EQ(fake_cicerone_client_->start_lxd_count(), 1);

  crostini_manager()->CancelRestartCrostini(restart_id);
  EXPECT_EQ(result_future.Get(), CrostiniResult::RESTART_REQUEST_CANCELLED);

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(crostini_manager()->HasRestarterForTesting(container_id()));
  EXPECT_EQ(fake_cicerone_client_->create_lxd_container_count(), 0);
}

TEST_F(CrostiniManagerRestartTest, CancelDuringCreateContainer) {
  TestFuture<CrostiniResult> result_future;
  CrostiniManager::RestartId restart_id =
      RestartCrostini(container_id(), result_future.GetCallback(), this);
  RunUntilState(mojom::InstallerState::kCreateContainer);

  EXPECT_EQ(fake_cicerone_client_->create_lxd_container_count(), 1);

  crostini_manager()->CancelRestartCrostini(restart_id);
  EXPECT_EQ(result_future.Get(), CrostiniResult::RESTART_REQUEST_CANCELLED);

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(crostini_manager()->HasRestarterForTesting(container_id()));
  EXPECT_EQ(fake_cicerone_client_->setup_lxd_container_user_count(), 0);
}

TEST_F(CrostiniManagerRestartTest, CancelDuringSetupContainer) {
  TestFuture<CrostiniResult> result_future;
  CrostiniManager::RestartId restart_id =
      RestartCrostini(container_id(), result_future.GetCallback(), this);
  RunUntilState(mojom::InstallerState::kSetupContainer);

  EXPECT_EQ(fake_cicerone_client_->setup_lxd_container_user_count(), 1);

  crostini_manager()->CancelRestartCrostini(restart_id);
  EXPECT_EQ(result_future.Get(), CrostiniResult::RESTART_REQUEST_CANCELLED);

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(crostini_manager()->HasRestarterForTesting(container_id()));
  EXPECT_EQ(fake_cicerone_client_->start_lxd_container_count(), 0);
}

TEST_F(CrostiniManagerRestartTest, CancelDuringStartContainer) {
  TestFuture<CrostiniResult> result_future;
  CrostiniManager::RestartId restart_id =
      RestartCrostini(container_id(), result_future.GetCallback(), this);
  RunUntilState(mojom::InstallerState::kStartContainer);

  EXPECT_EQ(fake_cicerone_client_->start_lxd_container_count(), 1);

  crostini_manager()->CancelRestartCrostini(restart_id);
  EXPECT_EQ(result_future.Get(), CrostiniResult::RESTART_REQUEST_CANCELLED);

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(crostini_manager()->HasRestarterForTesting(container_id()));
  EXPECT_FALSE(fake_cicerone_client_->configure_for_arc_sideload_called());
}

TEST_F(CrostiniManagerRestartTest, TimeoutWaitingForRestarterStart) {
  crostini_manager_->AddStoppingVmForTesting(container_id().vm_name);

  TestFuture<CrostiniResult> result_future;
  RestartCrostini(container_id(), result_future.GetCallback(), this);
  task_environment_.FastForwardBy(kLongTime);
  task_environment_.RunUntilIdle();

  EXPECT_EQ(result_future.Get(), CrostiniResult::START_TIMED_OUT);

  EXPECT_EQ(fake_concierge_client_->create_disk_image_call_count(), 0);
  ExpectRestarterUmaCount(1);
}

TEST_F(CrostiniManagerRestartTest, TimeoutDuringComponentLoadedFirstInstall) {
  crostini_manager()->SetInstallTerminaNeverCompletesForTesting(true);

  TestFuture<CrostiniResult> result_future;
  CrostiniManager::RestartOptions options;
  options.restart_source = RestartSource::kInstaller;
  RestartCrostiniWithOptions(container_id(), std::move(options),
                             result_future.GetCallback());
  EXPECT_FALSE(result_future.IsReady());
  task_environment_.FastForwardBy(base::Minutes(30));
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(result_future.IsReady());
  task_environment_.FastForwardBy(kLongTime);
  task_environment_.RunUntilIdle();

  EXPECT_EQ(result_future.Get(),
            CrostiniResult::INSTALL_IMAGE_LOADER_TIMED_OUT);

  EXPECT_EQ(fake_concierge_client_->create_disk_image_call_count(), 0);
  EXPECT_FALSE(
      profile_->GetPrefs()->GetBoolean(crostini::prefs::kCrostiniEnabled));
  ExpectRestarterUmaCount(1, /*is_install=*/true);
  histogram_tester_.ExpectTotalCount(
      "Crostini.RestarterTimeInState2.InstallImageLoader", 1);
  histogram_tester_.ExpectTotalCount(
      "Crostini.RestarterTimeInState2.CreateDiskImage", 0);
}

TEST_F(CrostiniManagerRestartTest,
       TimeoutDuringComponentLoadedAlreadyInstalled) {
  crostini_manager()->SetInstallTerminaNeverCompletesForTesting(true);

  TestFuture<CrostiniResult> result_future;
  RestartCrostini(container_id(), result_future.GetCallback(), this);
  task_environment_.FastForwardBy(base::Minutes(30));
  task_environment_.RunUntilIdle();

  EXPECT_EQ(result_future.Get(),
            CrostiniResult::INSTALL_IMAGE_LOADER_TIMED_OUT);

  EXPECT_EQ(fake_concierge_client_->create_disk_image_call_count(), 0);
  EXPECT_FALSE(
      profile_->GetPrefs()->GetBoolean(crostini::prefs::kCrostiniEnabled));
  ExpectRestarterUmaCount(1);
  histogram_tester_.ExpectTotalCount(
      "Crostini.RestarterTimeInState2.InstallImageLoader", 1);
  histogram_tester_.ExpectTotalCount(
      "Crostini.RestarterTimeInState2.CreateDiskImage", 0);
}

TEST_F(CrostiniManagerRestartTest, TimeoutDuringCreateDiskImage) {
  fake_concierge_client_->set_send_create_disk_image_response_delay(
      base::TimeDelta::Max());

  TestFuture<CrostiniResult> result_future;
  RestartCrostini(container_id(), result_future.GetCallback(), this);
  task_environment_.FastForwardBy(kLongTime);
  task_environment_.RunUntilIdle();

  EXPECT_EQ(result_future.Get(), CrostiniResult::CREATE_DISK_IMAGE_TIMED_OUT);

  EXPECT_GE(fake_concierge_client_->create_disk_image_call_count(), 1);
  EXPECT_EQ(fake_concierge_client_->start_vm_call_count(), 0);
  ExpectRestarterUmaCount(1);
  histogram_tester_.ExpectTotalCount(
      "Crostini.RestarterTimeInState2.CreateDiskImage", 1);
  histogram_tester_.ExpectTotalCount(
      "Crostini.RestarterTimeInState2.StartTerminaVm", 0);
}

TEST_F(CrostiniManagerRestartTest, UnexpectedTransitionsRecorded) {
  fake_concierge_client_->set_send_create_disk_image_response_delay(
      base::TimeDelta::Max());
  CrostiniManager::RestartId restart_id =
      RestartCrostini(container_id(), base::DoNothing(), this);
  // Run until we're sitting in the CreateDiskImage step that'll never return,
  // then try triggering a different transition.
  base::RunLoop().RunUntilIdle();
  crostini_manager()->CallRestarterStartLxdContainerFinishedForTesting(
      restart_id, CrostiniResult::CONTAINER_START_FAILED);
  histogram_tester_.ExpectUniqueSample("Crostini.InvalidStateTransition",
                                       mojom::InstallerState::kStartContainer,
                                       1);
}

TEST_F(CrostiniManagerRestartTest, TimeoutDuringStartVm) {
  fake_concierge_client_->set_send_start_vm_response_delay(
      base::TimeDelta::Max());

  TestFuture<CrostiniResult> result_future;
  RestartCrostini(container_id(), result_future.GetCallback(), this);
  task_environment_.FastForwardBy(kLongTime);
  task_environment_.RunUntilIdle();

  EXPECT_EQ(result_future.Get(), CrostiniResult::START_TERMINA_VM_TIMED_OUT);

  EXPECT_GE(fake_concierge_client_->create_disk_image_call_count(), 1);
  EXPECT_GE(fake_concierge_client_->start_vm_call_count(), 1);
  ExpectRestarterUmaCount(1);
}

TEST_F(CrostiniManagerRestartTest, TimeoutWaitingForVmStarted) {
  fake_concierge_client_->set_send_tremplin_started_signal_delay(
      base::TimeDelta::Max());
  vm_tools::concierge::StartVmResponse response;
  response.set_status(vm_tools::concierge::VmStatus::VM_STATUS_STARTING);
  fake_concierge_client_->set_start_vm_response(response);

  TestFuture<CrostiniResult> result_future;
  RestartCrostini(container_id(), result_future.GetCallback(), this);
  task_environment_.FastForwardBy(kLongTime);
  task_environment_.RunUntilIdle();

  EXPECT_EQ(result_future.Get(), CrostiniResult::START_TERMINA_VM_TIMED_OUT);

  EXPECT_GE(fake_concierge_client_->create_disk_image_call_count(), 1);
  EXPECT_GE(fake_concierge_client_->start_vm_call_count(), 1);
  ExpectRestarterUmaCount(1);
}

TEST_F(CrostiniManagerRestartTest, TimeoutDuringStartLxd) {
  fake_cicerone_client_->set_send_start_lxd_response_delay(
      base::TimeDelta::Max());

  TestFuture<CrostiniResult> result_future;
  RestartCrostini(container_id(), result_future.GetCallback(), this);
  task_environment_.FastForwardBy(kLongTime);
  task_environment_.RunUntilIdle();

  EXPECT_EQ(result_future.Get(), CrostiniResult::START_LXD_TIMED_OUT);

  EXPECT_GE(fake_concierge_client_->create_disk_image_call_count(), 1);
  EXPECT_GE(fake_concierge_client_->start_vm_call_count(), 1);
  ExpectRestarterUmaCount(1);
}

TEST_F(CrostiniManagerRestartTest, TimeoutWaitingForLxdStarted) {
  vm_tools::cicerone::StartLxdResponse response;
  response.set_status(vm_tools::cicerone::StartLxdResponse::STARTING);
  fake_cicerone_client_->set_start_lxd_response(response);

  TestFuture<CrostiniResult> result_future;
  RestartCrostini(container_id(), result_future.GetCallback(), this);
  task_environment_.FastForwardBy(kLongTime);
  task_environment_.RunUntilIdle();

  EXPECT_EQ(result_future.Get(), CrostiniResult::START_LXD_TIMED_OUT);

  EXPECT_GE(fake_concierge_client_->create_disk_image_call_count(), 1);
  EXPECT_GE(fake_concierge_client_->start_vm_call_count(), 1);
  ExpectRestarterUmaCount(1);
}

TEST_F(CrostiniManagerRestartTest, SameVmDifferentContainerStartsLxdCorrectly) {
  vm_tools::cicerone::StartLxdResponse response;
  response.set_status(vm_tools::cicerone::StartLxdResponse::STARTING);
  fake_cicerone_client_->set_start_lxd_response(response);

  TestFuture<CrostiniResult> result_future_1;
  RestartCrostini(container_id(), result_future_1.GetCallback(), this);

  auto container_id_2 =
      guest_os::GuestId(kCrostiniDefaultVmType, kVmName, "other-container");
  TestFuture<CrostiniResult> result_future_2;
  RestartCrostini(container_id_2, result_future_2.GetCallback(), this);

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(result_future_1.IsReady());
  EXPECT_FALSE(result_future_2.IsReady());

  vm_tools::cicerone::StartLxdProgressSignal signal;
  signal.set_owner_id(CryptohomeIdForProfile(profile()));
  signal.set_vm_name(kVmName);
  signal.set_status(vm_tools::cicerone::StartLxdProgressSignal::STARTED);
  crostini_manager()->OnStartLxdProgress(signal);

  EXPECT_EQ(result_future_1.Get(), CrostiniResult::SUCCESS);
  EXPECT_EQ(result_future_2.Get(), CrostiniResult::SUCCESS);
}

TEST_F(CrostiniManagerRestartTest, TimeoutDuringCreateContainer) {
  fake_cicerone_client_->set_send_create_lxd_container_response_delay(
      base::TimeDelta::Max());

  TestFuture<CrostiniResult> result_future;
  RestartCrostini(container_id(), result_future.GetCallback(), this);
  task_environment_.FastForwardBy(kLongTime);
  task_environment_.RunUntilIdle();

  EXPECT_EQ(result_future.Get(), CrostiniResult::CREATE_CONTAINER_TIMED_OUT);
  ExpectRestarterUmaCount(1);
}

TEST_F(CrostiniManagerRestartTest, TimeoutWaitingForContainerCreated) {
  fake_cicerone_client_->set_send_notify_lxd_container_created_signal_delay(
      base::TimeDelta::Max());

  TestFuture<CrostiniResult> result_future;
  RestartCrostini(container_id(), result_future.GetCallback(), this);
  task_environment_.FastForwardBy(kLongTime);
  task_environment_.RunUntilIdle();

  EXPECT_EQ(result_future.Get(), CrostiniResult::CREATE_CONTAINER_TIMED_OUT);

  EXPECT_GE(fake_concierge_client_->create_disk_image_call_count(), 1);
  EXPECT_GE(fake_concierge_client_->start_vm_call_count(), 1);
  ExpectRestarterUmaCount(1);
}

TEST_F(CrostiniManagerRestartTest, HeartbeatKeepsCreateContainerFromTimingOut) {
  fake_cicerone_client_->set_send_notify_lxd_container_created_signal_delay(
      base::TimeDelta::Max());
  vm_tools::cicerone::LxdContainerDownloadingSignal signal;
  signal.set_container_name(container_id().container_name);
  signal.set_vm_name(container_id().vm_name);
  signal.set_owner_id(CryptohomeIdForProfile(profile()));

  TestFuture<CrostiniResult> result_future;
  RestartCrostini(container_id(), result_future.GetCallback(), this);

  task_environment_.FastForwardBy(base::Minutes(4));
  crostini_manager_->OnLxdContainerDownloading(signal);
  task_environment_.FastForwardBy(base::Minutes(4));
  EXPECT_FALSE(result_future.IsReady());

  task_environment_.FastForwardBy(base::Minutes(6));
  EXPECT_TRUE(result_future.IsReady());

  EXPECT_EQ(result_future.Get(), CrostiniResult::CREATE_CONTAINER_TIMED_OUT);

  EXPECT_GE(fake_concierge_client_->create_disk_image_call_count(), 1);
  EXPECT_GE(fake_concierge_client_->start_vm_call_count(), 1);
  ExpectRestarterUmaCount(1);
}

TEST_F(CrostiniManagerRestartTest, RestartFinishesOnContainerCreatedError) {
  fake_cicerone_client_->set_lxd_container_created_signal_status(
      vm_tools::cicerone::LxdContainerCreatedSignal::UNKNOWN);

  TestFuture<CrostiniResult> result_future;
  RestartCrostini(DefaultContainerId(), result_future.GetCallback(), this);

  EXPECT_EQ(result_future.Get(), CrostiniResult::UNKNOWN_ERROR);

  // This pref entry is currently retained for the default container id and
  // removed for other containers.
  EXPECT_GE(
      guest_os::GetContainers(profile_.get(), guest_os::VmType::TERMINA).size(),
      1uL);
  EXPECT_GE(fake_concierge_client_->create_disk_image_call_count(), 1);
  EXPECT_GE(fake_concierge_client_->start_vm_call_count(), 1);
  ExpectRestarterUmaCount(1);
}

TEST_F(CrostiniManagerRestartTest,
       SuccessfulCreateCancelContainerCreatedKeepsPrefs) {
  TestFuture<CrostiniResult> restart_future;
  RestartCrostini(container_id(), restart_future.GetCallback());
  EXPECT_EQ(restart_future.Get(), CrostiniResult::SUCCESS);
  EXPECT_GE(
      guest_os::GetContainers(profile_.get(), guest_os::VmType::TERMINA).size(),
      1uL);

  TestFuture<CrostiniResult> stop_future;
  crostini_manager()->StopLxdContainer(container_id(),
                                       stop_future.GetCallback());
  EXPECT_EQ(stop_future.Get(), CrostiniResult::SUCCESS);

  TestFuture<CrostiniResult> failed_restart_future;
  fake_cicerone_client_->set_lxd_container_created_signal_status(
      vm_tools::cicerone::LxdContainerCreatedSignal::UNKNOWN);
  RestartCrostini(container_id(), failed_restart_future.GetCallback());
  EXPECT_EQ(failed_restart_future.Get(), CrostiniResult::UNKNOWN_ERROR);

  // Expect container wasn't removed from prefs.
  EXPECT_GE(
      guest_os::GetContainers(profile_.get(), guest_os::VmType::TERMINA).size(),
      1uL);
}

TEST_F(CrostiniManagerRestartTest, TimeoutDuringContainerSetup) {
  fake_cicerone_client_->set_send_set_up_lxd_container_user_response_delay(
      base::TimeDelta::Max());

  TestFuture<CrostiniResult> result_future;
  RestartCrostini(container_id(), result_future.GetCallback(), this);
  task_environment_.FastForwardBy(kLongTime);
  task_environment_.RunUntilIdle();

  EXPECT_EQ(result_future.Get(), CrostiniResult::SETUP_CONTAINER_TIMED_OUT);

  EXPECT_GE(fake_concierge_client_->create_disk_image_call_count(), 1);
  EXPECT_GE(fake_concierge_client_->start_vm_call_count(), 1);
  ExpectRestarterUmaCount(1);
}

TEST_F(CrostiniManagerRestartTest, TimeoutDuringStartContainer) {
  fake_cicerone_client_->set_send_start_lxd_container_response_delay(
      base::TimeDelta::Max());

  TestFuture<CrostiniResult> result_future;
  RestartCrostini(container_id(), result_future.GetCallback(), this);
  task_environment_.FastForwardBy(kLongTime);
  task_environment_.RunUntilIdle();

  EXPECT_EQ(result_future.Get(), CrostiniResult::START_CONTAINER_TIMED_OUT);
  ExpectRestarterUmaCount(1);
}

TEST_F(CrostiniManagerRestartTest, TimeoutWaitingForContainerStarted) {
  fake_cicerone_client_->set_send_container_started_signal_delay(
      base::TimeDelta::Max());

  TestFuture<CrostiniResult> result_future;
  RestartCrostini(container_id(), result_future.GetCallback(), this);
  task_environment_.FastForwardBy(kLongTime);
  task_environment_.RunUntilIdle();

  EXPECT_EQ(result_future.Get(), CrostiniResult::START_CONTAINER_TIMED_OUT);
  ExpectRestarterUmaCount(1);
}

TEST_F(CrostiniManagerRestartTest,
       HeartbeatKeepsContainerStartedFromTimingOut) {
  fake_cicerone_client_->set_send_container_started_signal_delay(
      base::TimeDelta::Max());
  vm_tools::cicerone::LxdContainerStartingSignal signal;
  signal.set_container_name(container_id().container_name);
  signal.set_vm_name(container_id().vm_name);
  signal.set_owner_id(CryptohomeIdForProfile(profile()));
  signal.set_status(vm_tools::cicerone::LxdContainerStartingSignal::STARTING);

  TestFuture<CrostiniResult> result_future;
  RestartCrostini(container_id(), result_future.GetCallback(), this);

  task_environment_.FastForwardBy(base::Minutes(7));
  crostini_manager_->OnLxdContainerStarting(signal);
  task_environment_.FastForwardBy(base::Minutes(7));
  EXPECT_FALSE(result_future.IsReady());

  task_environment_.FastForwardBy(base::Minutes(2));
  EXPECT_TRUE(result_future.IsReady());

  EXPECT_EQ(result_future.Get(), CrostiniResult::START_CONTAINER_TIMED_OUT);

  EXPECT_GE(fake_concierge_client_->create_disk_image_call_count(), 1);
  EXPECT_GE(fake_concierge_client_->start_vm_call_count(), 1);
  ExpectRestarterUmaCount(1);
}

TEST_F(CrostiniManagerRestartTest, CancelThenStopVm) {
  // This test checks that CrostiniRestarter can correctly handle VM shutdowns
  // after being cancelled.

  TestFuture<CrostiniResult> result_future;
  CrostiniManager::RestartId restart_id =
      RestartCrostini(container_id(), result_future.GetCallback(), this);
  RunUntilState(mojom::InstallerState::kCreateContainer);

  EXPECT_FALSE(result_future.IsReady());
  crostini_manager()->CancelRestartCrostini(restart_id);

  EXPECT_EQ(result_future.Get(), CrostiniResult::RESTART_REQUEST_CANCELLED);

  // Make sure the Restarter hasn't been destroyed yet, otherwise the shutdown
  // below won't actually test the code path we want it to.
  EXPECT_TRUE(crostini_manager()->HasRestarterForTesting(container_id()));

  vm_tools::concierge::VmStoppedSignal signal;
  signal.set_owner_id(CryptohomeIdForProfile(profile()));
  signal.set_name(kVmName);
  crostini_manager()->OnVmStopped(signal);

  EXPECT_FALSE(crostini_manager()->HasRestarterForTesting(container_id()));
}

TEST_F(CrostiniManagerRestartTest, CancelFinishedRestartIsSafe) {
  TestFuture<CrostiniResult> result_future;
  CrostiniManager::RestartId restart_id =
      RestartCrostini(container_id(), result_future.GetCallback());

  EXPECT_EQ(result_future.Get(), CrostiniResult::SUCCESS);
  EXPECT_FALSE(crostini_manager()->IsRestartPending(restart_id));

  crostini_manager()->CancelRestartCrostini(restart_id);
  base::RunLoop().RunUntilIdle();
  // Just make sure nothing crashes.
}

TEST_F(CrostiniManagerRestartTest, DoubleCancelIsSafe) {
  TestFuture<CrostiniResult> result_future;
  CrostiniManager::RestartId restart_id =
      RestartCrostini(container_id(), result_future.GetCallback());

  crostini_manager()->CancelRestartCrostini(restart_id);
  EXPECT_FALSE(crostini_manager()->IsRestartPending(restart_id));
  EXPECT_TRUE(result_future.IsReady());
  EXPECT_EQ(result_future.Get(), CrostiniResult::RESTART_REQUEST_CANCELLED);

  crostini_manager()->CancelRestartCrostini(restart_id);
}

TEST_F(CrostiniManagerRestartTest, MultiRestartAllowed) {
  CrostiniManager::RestartId id1, id2, id3;

  TestFuture<CrostiniResult> result_future_1, result_future_2, result_future_3;
  id1 = RestartCrostini(container_id(), result_future_1.GetCallback());
  id2 = RestartCrostini(container_id(), result_future_2.GetCallback());
  id3 = RestartCrostini(container_id(), result_future_3.GetCallback());

  EXPECT_TRUE(crostini_manager()->IsRestartPending(id1));
  EXPECT_TRUE(crostini_manager()->IsRestartPending(id2));
  EXPECT_TRUE(crostini_manager()->IsRestartPending(id3));

  EXPECT_EQ(result_future_1.Get(), CrostiniResult::SUCCESS);
  EXPECT_EQ(result_future_2.Get(), CrostiniResult::SUCCESS);
  EXPECT_EQ(result_future_3.Get(), CrostiniResult::SUCCESS);

  EXPECT_GE(fake_concierge_client_->create_disk_image_call_count(), 1);
  EXPECT_GE(fake_concierge_client_->start_vm_call_count(), 1);

  EXPECT_FALSE(crostini_manager()->IsRestartPending(id1));
  EXPECT_FALSE(crostini_manager()->IsRestartPending(id2));
  EXPECT_FALSE(crostini_manager()->IsRestartPending(id3));
  ExpectRestarterUmaCount(3);
}

TEST_F(CrostiniManagerRestartTest, FailureWithMultipleRestarts) {
  // When multiple restarts are running, a failure in the first should cause
  // the others to fail immediately.

  vm_tools::concierge::StartVmResponse response;
  response.set_status(vm_tools::concierge::VmStatus::VM_STATUS_FAILURE);
  fake_concierge_client_->set_start_vm_response(response);

  CrostiniManager::RestartId id1, id2, id3;
  TestFuture<CrostiniResult> result_future_1, result_future_2, result_future_3;
  id1 = RestartCrostini(container_id(), result_future_1.GetCallback());
  id2 = RestartCrostini(container_id(), result_future_2.GetCallback());
  id3 = RestartCrostini(container_id(), result_future_3.GetCallback());

  EXPECT_EQ(result_future_1.Get(), CrostiniResult::VM_START_FAILED);
  EXPECT_TRUE(result_future_2.IsReady());
  EXPECT_TRUE(result_future_3.IsReady());
  EXPECT_EQ(result_future_2.Get(), CrostiniResult::VM_START_FAILED);
  EXPECT_EQ(result_future_3.Get(), CrostiniResult::VM_START_FAILED);

  EXPECT_EQ(1, fake_concierge_client_->start_vm_call_count());
  EXPECT_FALSE(crostini_manager()->IsRestartPending(id1));
  EXPECT_FALSE(crostini_manager()->IsRestartPending(id2));
  EXPECT_FALSE(crostini_manager()->IsRestartPending(id3));
}

TEST_F(CrostiniManagerRestartTest, InstallHistogramEntries) {
  // When the first request is tagged as RestartSource::kInstaller, we should
  // log a single result to Crostini.RestarterResult.Installer and no results
  // to Crostini.RestartResult even if there were additional requests.

  SetCreateDiskImageResponse(vm_tools::concierge::DISK_STATUS_CREATED);

  vm_tools::concierge::StartVmResponse response;
  response.set_status(vm_tools::concierge::VmStatus::VM_STATUS_FAILURE);
  fake_concierge_client_->set_start_vm_response(response);

  TestFuture<CrostiniResult> result_future_1, result_future_2;
  CrostiniManager::RestartOptions options1;
  options1.restart_source = RestartSource::kInstaller;
  RestartCrostiniWithOptions(container_id(), std::move(options1),
                             result_future_1.GetCallback());
  RestartCrostini(container_id(), result_future_2.GetCallback());

  EXPECT_EQ(result_future_1.Get(), CrostiniResult::VM_START_FAILED);
  EXPECT_EQ(result_future_2.Get(), CrostiniResult::VM_START_FAILED);

  histogram_tester_.ExpectBucketCount("Crostini.RestarterResult.Installer",
                                      CrostiniResult::VM_START_FAILED, 1);
  histogram_tester_.ExpectTotalCount("Crostini.RestarterResult", 0);

  // Likewise for RestartSource::kMultiContainerCreation
  TestFuture<CrostiniResult> result_future_3, result_future_4;
  guest_os::GuestId container_id2("termina", "banana");
  CrostiniManager::RestartOptions options2;
  options2.restart_source = RestartSource::kMultiContainerCreation;
  RestartCrostiniWithOptions(container_id2, std::move(options2),
                             result_future_3.GetCallback());
  RestartCrostini(container_id2, result_future_4.GetCallback());

  EXPECT_EQ(result_future_3.Get(), CrostiniResult::VM_START_FAILED);
  EXPECT_EQ(result_future_4.Get(), CrostiniResult::VM_START_FAILED);

  histogram_tester_.ExpectBucketCount(
      "Crostini.RestarterResult.MultiContainerCreation",
      CrostiniResult::VM_START_FAILED, 1);
  histogram_tester_.ExpectTotalCount("Crostini.RestarterResult.Installer", 1);
  histogram_tester_.ExpectTotalCount("Crostini.RestarterResult", 0);
}

TEST_F(CrostiniManagerRestartTest, OsReleaseSetCorrectly) {
  vm_tools::cicerone::OsRelease os_release;
  base::HistogramTester histogram_tester{};
  os_release.set_pretty_name("Debian GNU/Linux 12 (bookworm)");
  os_release.set_version_id("12");
  os_release.set_id("debian");
  fake_cicerone_client_->set_lxd_container_os_release(os_release);

  TestFuture<CrostiniResult> result_future;
  RestartCrostini(container_id(), result_future.GetCallback(), this);
  EXPECT_EQ(result_future.Get(), CrostiniResult::SUCCESS);

  const auto* stored_os_release =
      crostini_manager()->GetContainerOsRelease(container_id());
  EXPECT_NE(stored_os_release, nullptr);
  // Sadly, we can't use MessageDifferencer here because we're using the LITE
  // API in our protos.
  EXPECT_EQ(os_release.SerializeAsString(),
            stored_os_release->SerializeAsString());
  histogram_tester.ExpectUniqueSample("Crostini.ContainerOsVersion",
                                      ContainerOsVersion::kDebianBookworm, 1);

  // The data for this container should also be stored in prefs.
  const base::Value* os_release_pref_value = GetContainerPrefValue(
      profile(), container_id(), guest_os::prefs::kContainerOsVersionKey);
  EXPECT_NE(os_release_pref_value, nullptr);
  EXPECT_EQ(os_release_pref_value->GetInt(),
            static_cast<int>(ContainerOsVersion::kDebianBookworm));
}

TEST_F(CrostiniManagerRestartTest, RestartThenUninstall) {
  TestFuture<CrostiniResult> restart_future;
  CrostiniManager::RestartId restart_id =
      RestartCrostini(container_id(), restart_future.GetCallback(), this);

  EXPECT_TRUE(crostini_manager()->IsRestartPending(restart_id));

  TestFuture<CrostiniResult> uninstall_future;
  crostini_manager()->RemoveCrostini(kVmName, uninstall_future.GetCallback());

  EXPECT_FALSE(crostini_manager()->IsRestartPending(restart_id));

  EXPECT_EQ(uninstall_future.Get(), CrostiniResult::SUCCESS);
  EXPECT_EQ(restart_future.Get(), CrostiniResult::RESTART_ABORTED);
  ExpectRestarterUmaCount(1);
}

TEST_F(CrostiniManagerRestartTest, RestartMultipleThenUninstall) {
  CrostiniManager::RestartId id1, id2, id3;
  TestFuture<CrostiniResult> restart_future_1, restart_future_2,
      restart_future_3;
  id1 = RestartCrostini(container_id(), restart_future_1.GetCallback());
  id2 = RestartCrostini(container_id(), restart_future_2.GetCallback());
  id3 = RestartCrostini(container_id(), restart_future_3.GetCallback());

  EXPECT_TRUE(crostini_manager()->IsRestartPending(id1));
  EXPECT_TRUE(crostini_manager()->IsRestartPending(id2));
  EXPECT_TRUE(crostini_manager()->IsRestartPending(id3));

  TestFuture<CrostiniResult> uninstall_future;
  crostini_manager()->RemoveCrostini(kVmName, uninstall_future.GetCallback());

  EXPECT_FALSE(crostini_manager()->IsRestartPending(id1));
  EXPECT_FALSE(crostini_manager()->IsRestartPending(id2));
  EXPECT_FALSE(crostini_manager()->IsRestartPending(id3));

  EXPECT_EQ(uninstall_future.Get(), CrostiniResult::SUCCESS);

  EXPECT_EQ(restart_future_1.Get(), CrostiniResult::RESTART_ABORTED);
  EXPECT_EQ(restart_future_2.Get(), CrostiniResult::RESTART_ABORTED);
  EXPECT_EQ(restart_future_3.Get(), CrostiniResult::RESTART_ABORTED);
  ExpectRestarterUmaCount(3);
}

TEST_F(CrostiniManagerRestartTest, UninstallWithRestarterTimeout) {
  fake_concierge_client_->set_send_start_vm_response_delay(
      base::TimeDelta::Max());
  RestartCrostini(container_id(), base::DoNothing(), this);
  RunUntilState(mojom::InstallerState::kStartTerminaVm);

  // In the kStartTerminaVm state now. Start an uninstall and then wait for
  // the timeout to be hit.

  TestFuture<CrostiniResult> uninstall_future;
  crostini_manager()->RemoveCrostini(kVmName, uninstall_future.GetCallback());

  task_environment_.FastForwardBy(kLongTime);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(uninstall_future.Get(), CrostiniResult::SUCCESS);
}

TEST_F(CrostiniManagerRestartTest, UninstallThenRestart) {
  // Install crostini first so that the uninstaller doesn't terminate before we
  // can call the installer again
  TestFuture<CrostiniResult> restart_future_1;
  RestartCrostini(container_id(), restart_future_1.GetCallback());
  EXPECT_EQ(restart_future_1.Get(), CrostiniResult::SUCCESS);

  TestFuture<CrostiniResult> uninstall_future;
  crostini_manager()->RemoveCrostini(kVmName, uninstall_future.GetCallback());

  TestFuture<CrostiniResult> restart_future_2;
  CrostiniManager::RestartId restart_id =
      RestartCrostini(container_id(), restart_future_2.GetCallback());

  // Restarting during uninstallation is not allowed and immediately fails.
  EXPECT_EQ(uninitialized_id_, restart_id);
  EXPECT_TRUE(restart_future_2.IsReady());
  EXPECT_EQ(restart_future_2.Get(),
            CrostiniResult::CROSTINI_UNINSTALLER_RUNNING);

  EXPECT_FALSE(uninstall_future.IsReady());
  EXPECT_EQ(uninstall_future.Get(), CrostiniResult::SUCCESS);
}

TEST_F(CrostiniManagerRestartTest, VmStoppedDuringRestart) {
  fake_cicerone_client_->set_send_container_started_signal_delay(
      base::TimeDelta::Max());
  TestFuture<CrostiniResult> result_future;
  RestartCrostini(container_id(), result_future.GetCallback(), this);

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(result_future.IsReady());

  vm_tools::concierge::VmStoppedSignal vm_stopped_signal;
  vm_stopped_signal.set_owner_id(CryptohomeIdForProfile(profile()));
  vm_stopped_signal.set_name(kVmName);
  crostini_manager()->OnVmStopped(vm_stopped_signal);
  EXPECT_EQ(result_future.Get(), CrostiniResult::RESTART_FAILED_VM_STOPPED);
}

TEST_F(CrostiniManagerRestartTest, RestartTriggersArcSideloadIfEnabled) {
  ash::SessionManagerClient::InitializeFake();
  ash::FakeSessionManagerClient::Get()->set_adb_sideload_enabled(true);

  vm_tools::cicerone::ConfigureForArcSideloadResponse fake_response;
  fake_response.set_status(
      vm_tools::cicerone::ConfigureForArcSideloadResponse::SUCCEEDED);
  fake_cicerone_client_->set_enable_arc_sideload_response(fake_response);

  TestFuture<CrostiniResult> result_future;
  RestartCrostini(container_id(), result_future.GetCallback());
  EXPECT_EQ(result_future.Get(), CrostiniResult::SUCCESS);

  // ConfigureForArcSideload() is called asynchronously.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(fake_cicerone_client_->configure_for_arc_sideload_called());
}

TEST_F(CrostiniManagerRestartTest, RestartDoesNotTriggerArcSideloadIfDisabled) {
  ash::SessionManagerClient::InitializeFake();
  ash::FakeSessionManagerClient::Get()->set_adb_sideload_enabled(false);

  vm_tools::cicerone::ConfigureForArcSideloadResponse fake_response;
  fake_response.set_status(
      vm_tools::cicerone::ConfigureForArcSideloadResponse::SUCCEEDED);
  fake_cicerone_client_->set_enable_arc_sideload_response(fake_response);

  TestFuture<CrostiniResult> result_future;
  RestartCrostini(container_id(), result_future.GetCallback());
  EXPECT_EQ(result_future.Get(), CrostiniResult::SUCCESS);

  // ConfigureForArcSideload() is called asynchronously.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(fake_cicerone_client_->configure_for_arc_sideload_called());
}

TEST_F(CrostiniManagerRestartTest, RestartWhileShuttingDown) {
  RestartCrostini(container_id(), base::DoNothing(), this);
  // crostini_manager() is destructed during test teardown, mimicking the effect
  // of shutting down chrome while a restart is running.
}

TEST_F(CrostiniManagerRestartTest, AllObservers) {
  TestFuture<CrostiniResult> result_future_1, result_future_2;

  TestRestartObserver observer2;
  int observer1_count = 0;
  on_stage_started_ =
      base::BindLambdaForTesting([&](mojom::InstallerState state) {
        ++observer1_count;
        if (state == mojom::InstallerState::kStartTerminaVm) {
          // Add a second Restarter with observer while first is starting.
          RestartCrostini(container_id(), result_future_2.GetCallback(),
                          &observer2);
        }
      });

  RestartCrostini(container_id(), result_future_1.GetCallback(), this);

  EXPECT_EQ(result_future_1.Get(), CrostiniResult::SUCCESS);
  EXPECT_EQ(result_future_2.Get(), CrostiniResult::SUCCESS);
  EXPECT_EQ(8, observer1_count);
  EXPECT_EQ(5u, observer2.stages.size());
}

TEST_F(CrostiniManagerRestartTest, StartVmOnly) {
  TestRestartObserver observer;
  CrostiniManager::RestartOptions options;
  options.start_vm_only = true;

  TestFuture<CrostiniResult> result_future;
  RestartCrostiniWithOptions(container_id(), std::move(options),
                             result_future.GetCallback(), &observer);

  EXPECT_EQ(result_future.Get(), CrostiniResult::SUCCESS);

  EXPECT_EQ(std::vector<crostini::mojom::InstallerState>({
                crostini::mojom::InstallerState::kStart,
                crostini::mojom::InstallerState::kInstallImageLoader,
                crostini::mojom::InstallerState::kCreateDiskImage,
                crostini::mojom::InstallerState::kStartTerminaVm,
            }),
            observer.stages);
}

TEST_F(CrostiniManagerRestartTest, StartVmOnlyThenFullRestart) {
  TestRestartObserver observer1;
  TestRestartObserver observer2;
  CrostiniManager::RestartOptions options;
  options.start_vm_only = true;

  TestFuture<CrostiniResult> result_future_1, result_future_2;

  RestartCrostiniWithOptions(container_id(), std::move(options),
                             result_future_1.GetCallback(), &observer1);

  RestartCrostini(container_id(), result_future_2.GetCallback(), &observer2);

  EXPECT_EQ(result_future_1.Get(), CrostiniResult::SUCCESS);
  EXPECT_FALSE(result_future_2.IsReady());

  EXPECT_EQ(result_future_2.Get(), CrostiniResult::SUCCESS);
  EXPECT_EQ(std::vector<crostini::mojom::InstallerState>({
                crostini::mojom::InstallerState::kStart,
                crostini::mojom::InstallerState::kInstallImageLoader,
                crostini::mojom::InstallerState::kCreateDiskImage,
                crostini::mojom::InstallerState::kStartTerminaVm,
            }),
            observer1.stages);
  EXPECT_EQ(std::vector<crostini::mojom::InstallerState>({
                crostini::mojom::InstallerState::kCreateDiskImage,
                crostini::mojom::InstallerState::kStartTerminaVm,
                crostini::mojom::InstallerState::kStartLxd,
                crostini::mojom::InstallerState::kCreateContainer,
                crostini::mojom::InstallerState::kSetupContainer,
                crostini::mojom::InstallerState::kStartContainer,
            }),
            observer2.stages);
}

TEST_F(CrostiniManagerRestartTest, FullRestartThenStartVmOnly) {
  TestRestartObserver observer1;
  TestRestartObserver observer2;
  TestFuture<CrostiniResult> result_future_1, result_future_2;

  RestartCrostini(container_id(), result_future_1.GetCallback(), &observer1);

  CrostiniManager::RestartOptions options;
  options.start_vm_only = true;
  RestartCrostiniWithOptions(container_id(), std::move(options),
                             result_future_2.GetCallback(), &observer2);

  EXPECT_EQ(result_future_2.Get(), CrostiniResult::SUCCESS);
  EXPECT_FALSE(result_future_1.IsReady());
  EXPECT_EQ(std::vector<crostini::mojom::InstallerState>({
                crostini::mojom::InstallerState::kCreateDiskImage,
                crostini::mojom::InstallerState::kStartTerminaVm,
            }),
            observer2.stages);

  EXPECT_EQ(result_future_1.Get(), CrostiniResult::SUCCESS);
  EXPECT_EQ(std::vector<crostini::mojom::InstallerState>({
                crostini::mojom::InstallerState::kStart,
                crostini::mojom::InstallerState::kInstallImageLoader,
                crostini::mojom::InstallerState::kCreateDiskImage,
                crostini::mojom::InstallerState::kStartTerminaVm,
                crostini::mojom::InstallerState::kStartLxd,
                crostini::mojom::InstallerState::kCreateContainer,
                crostini::mojom::InstallerState::kSetupContainer,
                crostini::mojom::InstallerState::kStartContainer,
            }),
            observer1.stages);
}

TEST_F(CrostiniManagerRestartTest, StartVmOnlyTwice) {
  TestRestartObserver observer1;
  TestRestartObserver observer2;
  TestFuture<CrostiniResult> result_future_1, result_future_2;

  CrostiniManager::RestartOptions options1;
  options1.start_vm_only = true;
  RestartCrostiniWithOptions(container_id(), std::move(options1),
                             result_future_1.GetCallback(), &observer1);
  CrostiniManager::RestartOptions options2;
  options2.start_vm_only = true;
  RestartCrostiniWithOptions(container_id(), std::move(options2),
                             result_future_2.GetCallback(), &observer2);

  EXPECT_EQ(result_future_1.Get(), CrostiniResult::SUCCESS);
  EXPECT_EQ(result_future_2.Get(), CrostiniResult::SUCCESS);

  EXPECT_EQ(std::vector<crostini::mojom::InstallerState>({
                crostini::mojom::InstallerState::kStart,
                crostini::mojom::InstallerState::kInstallImageLoader,
                crostini::mojom::InstallerState::kCreateDiskImage,
                crostini::mojom::InstallerState::kStartTerminaVm,
            }),
            observer1.stages);
  EXPECT_EQ(std::vector<crostini::mojom::InstallerState>({
                crostini::mojom::InstallerState::kCreateDiskImage,
                crostini::mojom::InstallerState::kStartTerminaVm,
            }),
            observer2.stages);
}

TEST_F(CrostiniManagerRestartTest, StopAfterLxdAvailableThenFullRestart) {
  TestRestartObserver observer1;
  TestRestartObserver observer2;
  TestFuture<CrostiniResult> result_future_1, result_future_2;

  CrostiniManager::RestartOptions options;
  options.stop_after_lxd_available = true;
  RestartCrostiniWithOptions(container_id(), std::move(options),
                             result_future_1.GetCallback(), &observer1);
  RestartCrostini(container_id(), result_future_2.GetCallback(), &observer2);

  EXPECT_EQ(result_future_1.Get(), CrostiniResult::SUCCESS);
  EXPECT_EQ(result_future_2.Get(), CrostiniResult::SUCCESS);

  EXPECT_EQ(std::vector<crostini::mojom::InstallerState>({
                crostini::mojom::InstallerState::kStart,
                crostini::mojom::InstallerState::kInstallImageLoader,
                crostini::mojom::InstallerState::kCreateDiskImage,
                crostini::mojom::InstallerState::kStartTerminaVm,
                crostini::mojom::InstallerState::kStartLxd,
            }),
            observer1.stages);
  EXPECT_EQ(std::vector<crostini::mojom::InstallerState>({
                crostini::mojom::InstallerState::kCreateDiskImage,
                crostini::mojom::InstallerState::kStartTerminaVm,
                crostini::mojom::InstallerState::kStartLxd,
                crostini::mojom::InstallerState::kCreateContainer,
                crostini::mojom::InstallerState::kSetupContainer,
                crostini::mojom::InstallerState::kStartContainer,
            }),
            observer2.stages);
}

TEST_F(CrostiniManagerRestartTest, UninstallUnregistersContainers) {
  auto* terminal_registry =
      guest_os::GuestOsServiceFactory::GetForProfile(profile_.get())
          ->TerminalProviderRegistry();
  auto* mount_registry =
      guest_os::GuestOsServiceFactory::GetForProfile(profile_.get())
          ->MountProviderRegistry();
  auto* share_service =
      guest_os::GuestOsSharePathFactory::GetForProfile(profile_.get());

  TestFuture<CrostiniResult> restart_result;
  RestartCrostini(container_id(), restart_result.GetCallback());
  EXPECT_EQ(restart_result.Get(), CrostiniResult::SUCCESS);

  EXPECT_GT(terminal_registry->List().size(), 0u);
  EXPECT_GT(mount_registry->List().size(), 0u);
  EXPECT_GT(share_service->ListGuests().size(), 0u);

  TestFuture<CrostiniResult> uninstall_result;
  crostini_manager()->RemoveCrostini(kVmName, uninstall_result.GetCallback());
  EXPECT_EQ(uninstall_result.Get(), CrostiniResult::SUCCESS);

  EXPECT_EQ(terminal_registry->List().size(), 0u);
  EXPECT_EQ(mount_registry->List().size(), 0u);
  EXPECT_EQ(share_service->ListGuests().size(), 0u);
}

TEST_F(CrostiniManagerRestartTest,
       DeleteUnregistersContainersWhenDoesNotExist) {
  auto* terminal_registry =
      guest_os::GuestOsServiceFactory::GetForProfile(profile_.get())
          ->TerminalProviderRegistry();
  auto* mount_registry =
      guest_os::GuestOsServiceFactory::GetForProfile(profile_.get())
          ->MountProviderRegistry();
  auto* share_service =
      guest_os::GuestOsSharePathFactory::GetForProfile(profile_.get());
  vm_tools::cicerone::DeleteLxdContainerResponse response;
  response.set_status(
      vm_tools::cicerone::DeleteLxdContainerResponse::DOES_NOT_EXIST);
  fake_cicerone_client_->set_delete_lxd_container_response_(response);

  TestFuture<CrostiniResult> restart_result;
  RestartCrostini(container_id(), restart_result.GetCallback());
  EXPECT_EQ(restart_result.Get(), CrostiniResult::SUCCESS);

  EXPECT_GT(terminal_registry->List().size(), 0u);
  EXPECT_GT(mount_registry->List().size(), 0u);
  EXPECT_GT(share_service->ListGuests().size(), 0u);

  TestFuture<bool> delete_result;
  crostini_manager()->DeleteLxdContainer(container_id(),
                                         delete_result.GetCallback());
  EXPECT_EQ(delete_result.Get(), true);

  EXPECT_EQ(terminal_registry->List().size(), 0u);
  EXPECT_EQ(mount_registry->List().size(), 0u);
  EXPECT_EQ(share_service->ListGuests().size(), 0u);
}

TEST_F(CrostiniManagerRestartTest, DeleteUnregistersContainers) {
  auto* terminal_registry =
      guest_os::GuestOsServiceFactory::GetForProfile(profile_.get())
          ->TerminalProviderRegistry();
  auto* mount_registry =
      guest_os::GuestOsServiceFactory::GetForProfile(profile_.get())
          ->MountProviderRegistry();
  auto* share_service =
      guest_os::GuestOsSharePathFactory::GetForProfile(profile_.get());

  TestFuture<CrostiniResult> restart_result;
  RestartCrostini(container_id(), restart_result.GetCallback());
  EXPECT_EQ(restart_result.Get(), CrostiniResult::SUCCESS);

  EXPECT_GT(terminal_registry->List().size(), 0u);
  EXPECT_GT(mount_registry->List().size(), 0u);
  EXPECT_GT(share_service->ListGuests().size(), 0u);

  vm_tools::cicerone::LxdContainerDeletedSignal signal;
  signal.set_vm_name(container_id().vm_name);
  signal.set_container_name(container_id().container_name);
  signal.set_owner_id(CryptohomeIdForProfile(profile()));
  signal.set_status(vm_tools::cicerone::LxdContainerDeletedSignal::DELETED);
  crostini_manager()->OnLxdContainerDeleted(signal);

  EXPECT_EQ(terminal_registry->List().size(), 0u);
  EXPECT_EQ(mount_registry->List().size(), 0u);
  EXPECT_EQ(share_service->ListGuests().size(), 0u);
}

class CrostiniManagerEnterpriseReportingTest
    : public CrostiniManagerRestartTest {
 public:
  void SetUp() override {
    CrostiniManagerRestartTest::SetUp();

    // Enable Crostini reporting:
    profile()->GetPrefs()->SetBoolean(prefs::kReportCrostiniUsageEnabled, true);
  }

  void TearDown() override {
    ash::disks::DiskMountManager::Shutdown();
    CrostiniManagerRestartTest::TearDown();
  }
};

TEST_F(CrostiniManagerEnterpriseReportingTest,
       LogKernelVersionForEnterpriseReportingSuccess) {
  // Set success response for retrieving enterprise reporting info:
  vm_tools::concierge::GetVmEnterpriseReportingInfoResponse response;
  response.set_success(true);
  response.set_vm_kernel_version(kTerminaKernelVersion);
  fake_concierge_client_->set_get_vm_enterprise_reporting_info_response(
      response);

  TestFuture<CrostiniResult> result_future;
  RestartCrostini(DefaultContainerId(), result_future.GetCallback());
  EXPECT_EQ(result_future.Get(), CrostiniResult::SUCCESS);

  EXPECT_GE(fake_concierge_client_->create_disk_image_call_count(), 1);
  EXPECT_GE(fake_concierge_client_->start_vm_call_count(), 1);
  EXPECT_TRUE(
      fake_concierge_client_->get_vm_enterprise_reporting_info_call_count());
  EXPECT_EQ(kTerminaKernelVersion,
            profile()->GetPrefs()->GetString(
                crostini::prefs::kCrostiniLastLaunchTerminaKernelVersion));
}

TEST_F(CrostiniManagerEnterpriseReportingTest,
       LogKernelVersionForEnterpriseReportingFailure) {
  // Set error response for retrieving enterprise reporting info:
  vm_tools::concierge::GetVmEnterpriseReportingInfoResponse response;
  response.set_success(false);
  response.set_failure_reason("Don't feel like it");
  fake_concierge_client_->set_get_vm_enterprise_reporting_info_response(
      response);

  TestFuture<CrostiniResult> result_future;
  RestartCrostini(DefaultContainerId(), result_future.GetCallback());
  EXPECT_EQ(result_future.Get(), CrostiniResult::SUCCESS);

  EXPECT_GE(fake_concierge_client_->create_disk_image_call_count(), 1);
  EXPECT_GE(fake_concierge_client_->start_vm_call_count(), 1);
  EXPECT_TRUE(
      fake_concierge_client_->get_vm_enterprise_reporting_info_call_count());
  // In case of an error, the pref should be (re)set to the empty string:
  EXPECT_TRUE(
      profile()
          ->GetPrefs()
          ->GetString(crostini::prefs::kCrostiniLastLaunchTerminaKernelVersion)
          .empty());
}

TEST_F(CrostiniManagerTest, ExportDiskImageFailure) {
  base::ScopedTempFile export_path;
  EXPECT_TRUE(export_path.Create());
  TestFuture<CrostiniResult> result_future;
  EXPECT_EQ(fake_concierge_client_->export_disk_image_call_count(), 0);

  vm_tools::concierge::ExportDiskImageResponse failure_response;
  failure_response.set_status(vm_tools::concierge::DISK_STATUS_FAILED);
  fake_concierge_client_->set_export_disk_image_response(failure_response);

  crostini_manager()->ExportDiskImage(container_id(), "my_cool_user_id_hash",
                                      export_path.path(), false,
                                      result_future.GetCallback());

  EXPECT_EQ(fake_concierge_client_->export_disk_image_call_count(), 1);
  EXPECT_EQ(result_future.Get<0>(), CrostiniResult::DISK_IMAGE_FAILED);
}

TEST_F(CrostiniManagerTest, ExportDiskImageNoSpaceFailure) {
  base::ScopedTempFile export_path;
  EXPECT_TRUE(export_path.Create());
  TestFuture<CrostiniResult> result_future;
  EXPECT_EQ(fake_concierge_client_->export_disk_image_call_count(), 0);

  vm_tools::concierge::DiskImageStatusResponse progress_signal;
  progress_signal.set_status(vm_tools::concierge::DISK_STATUS_IN_PROGRESS);
  progress_signal.set_progress(50);
  vm_tools::concierge::DiskImageStatusResponse no_space_signal;
  no_space_signal.set_status(vm_tools::concierge::DISK_STATUS_NOT_ENOUGH_SPACE);
  std::vector<vm_tools::concierge::DiskImageStatusResponse> signals;
  signals.emplace_back(progress_signal);
  signals.emplace_back(no_space_signal);
  fake_concierge_client_->set_disk_image_status_signals(signals);

  crostini_manager()->ExportDiskImage(container_id(), "my_cool_user_id_hash",
                                      export_path.path(), false,
                                      result_future.GetCallback());

  EXPECT_EQ(fake_concierge_client_->export_disk_image_call_count(), 1);
  EXPECT_EQ(result_future.Get<0>(), CrostiniResult::DISK_IMAGE_FAILED_NO_SPACE);
}

TEST_F(CrostiniManagerTest, ExportDiskImageSuccess) {
  base::ScopedTempFile export_path;
  EXPECT_TRUE(export_path.Create());
  TestFuture<CrostiniResult> result_future;
  EXPECT_EQ(fake_concierge_client_->export_disk_image_call_count(), 0);

  vm_tools::concierge::DiskImageStatusResponse progress_signal;
  progress_signal.set_status(vm_tools::concierge::DISK_STATUS_IN_PROGRESS);
  progress_signal.set_progress(50);
  vm_tools::concierge::DiskImageStatusResponse done_signal;
  done_signal.set_status(vm_tools::concierge::DISK_STATUS_CREATED);
  std::vector<vm_tools::concierge::DiskImageStatusResponse> signals;
  signals.emplace_back(progress_signal);
  signals.emplace_back(done_signal);
  fake_concierge_client_->set_disk_image_status_signals(signals);

  crostini_manager()->ExportDiskImage(container_id(), "my_cool_user_id_hash",
                                      export_path.path(), false,
                                      result_future.GetCallback());

  EXPECT_EQ(fake_concierge_client_->export_disk_image_call_count(), 1);
  EXPECT_EQ(result_future.Get<0>(), CrostiniResult::SUCCESS);
}

TEST_F(CrostiniManagerTest, ImportDiskImageFailure) {
  base::ScopedTempFile import_path;
  EXPECT_TRUE(import_path.Create());
  TestFuture<CrostiniResult> result_future;
  EXPECT_EQ(fake_concierge_client_->import_disk_image_call_count(), 0);

  vm_tools::concierge::ImportDiskImageResponse failure_response;
  failure_response.set_status(vm_tools::concierge::DISK_STATUS_FAILED);
  fake_concierge_client_->set_import_disk_image_response(failure_response);

  crostini_manager()->ImportDiskImage(container_id(), "my_cool_user_id_hash",
                                      import_path.path(),
                                      result_future.GetCallback());

  EXPECT_EQ(fake_concierge_client_->import_disk_image_call_count(), 1);
  EXPECT_EQ(result_future.Get<0>(), CrostiniResult::DISK_IMAGE_FAILED);
}

TEST_F(CrostiniManagerTest, ImportDiskImageNoSpaceFailure) {
  base::ScopedTempFile import_path;
  EXPECT_TRUE(import_path.Create());
  TestFuture<CrostiniResult> result_future;
  EXPECT_EQ(fake_concierge_client_->import_disk_image_call_count(), 0);

  vm_tools::concierge::DiskImageStatusResponse progress_signal;
  progress_signal.set_status(vm_tools::concierge::DISK_STATUS_IN_PROGRESS);
  progress_signal.set_progress(50);
  vm_tools::concierge::DiskImageStatusResponse no_space_signal;
  no_space_signal.set_status(vm_tools::concierge::DISK_STATUS_NOT_ENOUGH_SPACE);
  std::vector<vm_tools::concierge::DiskImageStatusResponse> signals;
  signals.emplace_back(progress_signal);
  signals.emplace_back(no_space_signal);
  fake_concierge_client_->set_disk_image_status_signals(signals);

  vm_tools::concierge::ImportDiskImageResponse response;
  response.set_status(vm_tools::concierge::DISK_STATUS_IN_PROGRESS);
  ash::FakeConciergeClient::Get()->set_import_disk_image_response(response);

  crostini_manager()->ImportDiskImage(container_id(), "my_cool_user_id_hash",
                                      import_path.path(),
                                      result_future.GetCallback());

  EXPECT_EQ(fake_concierge_client_->import_disk_image_call_count(), 1);
  EXPECT_EQ(result_future.Get<0>(), CrostiniResult::DISK_IMAGE_FAILED_NO_SPACE);
}

TEST_F(CrostiniManagerTest, ImportDiskImageSuccess) {
  base::ScopedTempFile import_path;
  EXPECT_TRUE(import_path.Create());
  TestFuture<CrostiniResult> result_future;
  EXPECT_EQ(fake_concierge_client_->import_disk_image_call_count(), 0);

  vm_tools::concierge::DiskImageStatusResponse progress_signal;
  progress_signal.set_status(vm_tools::concierge::DISK_STATUS_IN_PROGRESS);
  progress_signal.set_progress(50);
  vm_tools::concierge::DiskImageStatusResponse done_signal;
  done_signal.set_status(vm_tools::concierge::DISK_STATUS_CREATED);
  std::vector<vm_tools::concierge::DiskImageStatusResponse> signals;
  signals.emplace_back(progress_signal);
  signals.emplace_back(done_signal);
  fake_concierge_client_->set_disk_image_status_signals(signals);

  vm_tools::concierge::ImportDiskImageResponse response;
  response.set_status(vm_tools::concierge::DISK_STATUS_IN_PROGRESS);
  ash::FakeConciergeClient::Get()->set_import_disk_image_response(response);

  crostini_manager()->ImportDiskImage(container_id(), "my_cool_user_id_hash",
                                      import_path.path(),
                                      result_future.GetCallback());

  EXPECT_EQ(fake_concierge_client_->import_disk_image_call_count(), 1);
  EXPECT_EQ(result_future.Get<0>(), CrostiniResult::SUCCESS);
}

TEST_F(CrostiniManagerTest, ExportContainerSuccess) {
  uint64_t container_size = 123;
  uint64_t exported_size = 456;

  TestFuture<CrostiniResult, uint64_t, uint64_t> result_future;
  crostini_manager()->ExportLxdContainer(container_id(),
                                         base::FilePath("export_path"),
                                         result_future.GetCallback());

  // Send signals, STREAMING, DONE.
  vm_tools::cicerone::ExportLxdContainerProgressSignal signal;
  signal.set_owner_id(CryptohomeIdForProfile(profile()));
  signal.set_vm_name(kVmName);
  signal.set_container_name(kContainerName);
  signal.set_status(
      vm_tools::cicerone::
          ExportLxdContainerProgressSignal_Status_EXPORTING_STREAMING);
  fake_cicerone_client_->NotifyExportLxdContainerProgress(signal);

  signal.set_status(
      vm_tools::cicerone::ExportLxdContainerProgressSignal_Status_DONE);
  signal.set_input_bytes_streamed(container_size);
  signal.set_bytes_exported(exported_size);
  fake_cicerone_client_->NotifyExportLxdContainerProgress(signal);

  EXPECT_EQ(result_future.Get<0>(), CrostiniResult::SUCCESS);
  EXPECT_EQ(result_future.Get<1>(), container_size);
  EXPECT_EQ(result_future.Get<2>(), exported_size);
}

TEST_F(CrostiniManagerTest, ExportContainerFailInProgress) {
  uint64_t container_size = 123;
  uint64_t exported_size = 456;

  // 1st call succeeds.
  TestFuture<CrostiniResult, uint64_t, uint64_t> result_future;
  crostini_manager()->ExportLxdContainer(container_id(),
                                         base::FilePath("export_path"),
                                         result_future.GetCallback());

  // 2nd call fails since 1st call is in progress.
  TestFuture<CrostiniResult, uint64_t, uint64_t> result_future2;
  crostini_manager()->ExportLxdContainer(container_id(),
                                         base::FilePath("export_path"),
                                         result_future2.GetCallback());

  // Send signal to indicate 1st call is done.
  vm_tools::cicerone::ExportLxdContainerProgressSignal signal;
  signal.set_owner_id(CryptohomeIdForProfile(profile()));
  signal.set_vm_name(kVmName);
  signal.set_container_name(kContainerName);
  signal.set_status(
      vm_tools::cicerone::ExportLxdContainerProgressSignal_Status_DONE);
  signal.set_input_bytes_streamed(123);
  signal.set_bytes_exported(456);
  fake_cicerone_client_->NotifyExportLxdContainerProgress(signal);

  EXPECT_EQ(result_future.Get<0>(), CrostiniResult::SUCCESS);
  EXPECT_EQ(result_future.Get<1>(), container_size);
  EXPECT_EQ(result_future.Get<2>(), exported_size);

  EXPECT_EQ(result_future2.Get<0>(),
            CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED);
  EXPECT_EQ(result_future2.Get<1>(), 0u);
  EXPECT_EQ(result_future2.Get<2>(), 0u);
}

TEST_F(CrostiniManagerTest, ExportContainerFailFromSignal) {
  uint64_t container_size = 123;
  uint64_t exported_size = 456;

  TestFuture<CrostiniResult, uint64_t, uint64_t> result_future;
  crostini_manager()->ExportLxdContainer(container_id(),
                                         base::FilePath("export_path"),
                                         result_future.GetCallback());

  // Send signal with FAILED.
  vm_tools::cicerone::ExportLxdContainerProgressSignal signal;
  signal.set_owner_id(CryptohomeIdForProfile(profile()));
  signal.set_vm_name(kVmName);
  signal.set_container_name(kContainerName);
  signal.set_status(
      vm_tools::cicerone::ExportLxdContainerProgressSignal_Status_FAILED);
  signal.set_input_bytes_streamed(container_size);
  signal.set_bytes_exported(exported_size);
  fake_cicerone_client_->NotifyExportLxdContainerProgress(signal);

  EXPECT_EQ(result_future.Get<0>(),
            CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED);
  EXPECT_EQ(result_future.Get<1>(), container_size);
  EXPECT_EQ(result_future.Get<2>(), exported_size);
}

TEST_F(CrostiniManagerTest, ExportContainerFailOnVmStop) {
  crostini_manager()->AddRunningVmForTesting(kVmName);
  TestFuture<CrostiniResult, uint64_t, uint64_t> result_future;
  crostini_manager()->ExportLxdContainer(container_id(),
                                         base::FilePath("export_path"),
                                         result_future.GetCallback());
  crostini_manager()->StopVm(kVmName, base::DoNothing());

  EXPECT_EQ(result_future.Get<0>(),
            CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED_VM_STOPPED);
  EXPECT_EQ(result_future.Get<1>(), 0u);
  EXPECT_EQ(result_future.Get<2>(), 0u);
}

TEST_F(CrostiniManagerTest, ImportContainerSuccess) {
  TestFuture<CrostiniResult> result_future;
  crostini_manager()->ImportLxdContainer(container_id(),
                                         base::FilePath("import_path"),
                                         result_future.GetCallback());

  // Send signals, UPLOAD, UNPACK, DONE.
  vm_tools::cicerone::ImportLxdContainerProgressSignal signal;
  signal.set_owner_id(CryptohomeIdForProfile(profile()));
  signal.set_vm_name(kVmName);
  signal.set_container_name(kContainerName);
  signal.set_status(
      vm_tools::cicerone::
          ImportLxdContainerProgressSignal_Status_IMPORTING_UPLOAD);
  fake_cicerone_client_->NotifyImportLxdContainerProgress(signal);

  signal.set_status(
      vm_tools::cicerone::
          ImportLxdContainerProgressSignal_Status_IMPORTING_UNPACK);
  fake_cicerone_client_->NotifyImportLxdContainerProgress(signal);

  signal.set_status(
      vm_tools::cicerone::ImportLxdContainerProgressSignal_Status_DONE);
  fake_cicerone_client_->NotifyImportLxdContainerProgress(signal);

  EXPECT_EQ(result_future.Get(), CrostiniResult::SUCCESS);
}

TEST_F(CrostiniManagerTest, ImportContainerFailInProgress) {
  // 1st call succeeds.
  TestFuture<CrostiniResult> result_future;
  crostini_manager()->ImportLxdContainer(container_id(),
                                         base::FilePath("import_path"),
                                         result_future.GetCallback());

  // 2nd call fails since 1st call is in progress.
  TestFuture<CrostiniResult> result_future2;
  crostini_manager()->ImportLxdContainer(container_id(),
                                         base::FilePath("import_path"),
                                         result_future2.GetCallback());

  // Send signal to indicate 1st call is done.
  vm_tools::cicerone::ImportLxdContainerProgressSignal signal;
  signal.set_owner_id(CryptohomeIdForProfile(profile()));
  signal.set_vm_name(kVmName);
  signal.set_container_name(kContainerName);
  signal.set_status(
      vm_tools::cicerone::ImportLxdContainerProgressSignal_Status_DONE);
  fake_cicerone_client_->NotifyImportLxdContainerProgress(signal);

  EXPECT_EQ(result_future.Get(), CrostiniResult::SUCCESS);
  EXPECT_EQ(result_future2.Get(),
            CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED);
}

TEST_F(CrostiniManagerTest, ImportContainerFailArchitecture) {
  TestFuture<CrostiniResult> result_future;
  crostini_manager()->ImportLxdContainer(container_id(),
                                         base::FilePath("import_path"),
                                         result_future.GetCallback());

  // Send signal with FAILED_ARCHITECTURE.
  vm_tools::cicerone::ImportLxdContainerProgressSignal signal;
  signal.set_owner_id(CryptohomeIdForProfile(profile()));
  signal.set_vm_name(kVmName);
  signal.set_container_name(kContainerName);
  signal.set_status(
      vm_tools::cicerone::
          ImportLxdContainerProgressSignal_Status_FAILED_ARCHITECTURE);
  signal.set_architecture_device("archdev");
  signal.set_architecture_container("archcont");
  fake_cicerone_client_->NotifyImportLxdContainerProgress(signal);

  EXPECT_EQ(result_future.Get(),
            CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED_ARCHITECTURE);
}

TEST_F(CrostiniManagerTest, ImportContainerFailFromSignal) {
  TestFuture<CrostiniResult> result_future;
  crostini_manager()->ImportLxdContainer(container_id(),
                                         base::FilePath("import_path"),
                                         result_future.GetCallback());

  // Send signal with FAILED.
  vm_tools::cicerone::ImportLxdContainerProgressSignal signal;
  signal.set_owner_id(CryptohomeIdForProfile(profile()));
  signal.set_vm_name(kVmName);
  signal.set_container_name(kContainerName);
  signal.set_status(
      vm_tools::cicerone::ImportLxdContainerProgressSignal_Status_FAILED);
  fake_cicerone_client_->NotifyImportLxdContainerProgress(signal);

  EXPECT_EQ(result_future.Get(),
            CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED);
}

TEST_F(CrostiniManagerTest, ImportContainerFailOnVmStop) {
  TestFuture<CrostiniResult> result_future;

  crostini_manager()->AddRunningVmForTesting(kVmName);
  crostini_manager()->ImportLxdContainer(container_id(),
                                         base::FilePath("import_path"),
                                         result_future.GetCallback());
  crostini_manager()->StopVm(kVmName, base::DoNothing());

  EXPECT_EQ(result_future.Get(),
            CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED_VM_STOPPED);
}

TEST_F(CrostiniManagerTest, InstallLinuxPackageFromAptSignalNotConnectedError) {
  fake_cicerone_client_->set_install_linux_package_progress_signal_connected(
      false);
  TestFuture<CrostiniResult> result_future;
  crostini_manager()->InstallLinuxPackageFromApt(container_id(), kPackageID,
                                                 result_future.GetCallback());

  EXPECT_EQ(result_future.Get(), CrostiniResult::INSTALL_LINUX_PACKAGE_FAILED);
}

TEST_F(CrostiniManagerTest, InstallLinuxPackageFromAptSignalSuccess) {
  vm_tools::cicerone::InstallLinuxPackageResponse response;

  response.set_status(vm_tools::cicerone::InstallLinuxPackageResponse::STARTED);
  fake_cicerone_client_->set_install_linux_package_response(response);
  TestFuture<CrostiniResult> result_future;
  crostini_manager()->InstallLinuxPackageFromApt(container_id(), kPackageID,
                                                 result_future.GetCallback());

  EXPECT_EQ(result_future.Get(), CrostiniResult::SUCCESS);
}

TEST_F(CrostiniManagerTest, InstallLinuxPackageFromAptSignalFailure) {
  vm_tools::cicerone::InstallLinuxPackageResponse response;

  response.set_status(vm_tools::cicerone::InstallLinuxPackageResponse::FAILED);
  response.set_failure_reason(
      "Unit tests can't install Linux package from apt!");
  fake_cicerone_client_->set_install_linux_package_response(response);
  TestFuture<CrostiniResult> result_future;
  crostini_manager()->InstallLinuxPackageFromApt(container_id(), kPackageID,
                                                 result_future.GetCallback());

  EXPECT_EQ(result_future.Get(), CrostiniResult::INSTALL_LINUX_PACKAGE_FAILED);
}

TEST_F(CrostiniManagerTest, InstallLinuxPackageFromAptSignalOperationBlocked) {
  vm_tools::cicerone::InstallLinuxPackageResponse response;

  response.set_status(
      vm_tools::cicerone::InstallLinuxPackageResponse::INSTALL_ALREADY_ACTIVE);
  fake_cicerone_client_->set_install_linux_package_response(response);
  TestFuture<CrostiniResult> result_future;
  crostini_manager()->InstallLinuxPackageFromApt(container_id(), kPackageID,
                                                 result_future.GetCallback());

  EXPECT_EQ(result_future.Get(),
            CrostiniResult::BLOCKING_OPERATION_ALREADY_ACTIVE);
}

TEST_F(CrostiniManagerTest, InstallerStatusInitiallyFalse) {
  EXPECT_FALSE(
      crostini_manager()->GetCrostiniDialogStatus(DialogType::INSTALLER));
}

TEST_F(CrostiniManagerTest, StartContainerSuccess) {
  TestFuture<CrostiniResult> result_future;

  crostini_manager()->StartLxdContainer(container_id(),
                                        result_future.GetCallback());

  EXPECT_EQ(result_future.Get(), CrostiniResult::SUCCESS);
}

TEST_F(CrostiniManagerTest, StopContainerSuccess) {
  TestFuture<CrostiniResult> result_future;

  crostini_manager()->StopLxdContainer(container_id(),
                                       result_future.GetCallback());

  EXPECT_EQ(result_future.Get(), CrostiniResult::SUCCESS);
}

TEST_F(CrostiniManagerTest, FileSystemCorruptionSignal) {
  base::HistogramTester histogram_tester{};

  anomaly_detector::GuestFileCorruptionSignal signal;
  fake_anomaly_detector_client_->NotifyGuestFileCorruption(signal);

  histogram_tester.ExpectUniqueSample(kCrostiniCorruptionHistogram,
                                      CorruptionStates::OTHER_CORRUPTION, 1);
}

TEST_F(CrostiniManagerTest, StartLxdSuccess) {
  TestFuture<CrostiniResult> result_future;

  crostini_manager()->StartLxd(kVmName, result_future.GetCallback());

  EXPECT_EQ(result_future.Get(), CrostiniResult::SUCCESS);
}

class CrostiniManagerAnsibleInfraTest : public CrostiniManagerRestartTest {
 public:
  void SetUp() override {
    CrostiniManagerTest::SetUp();
    mock_ansible_management_service_ =
        AnsibleManagementTestHelper::SetUpMockAnsibleManagementService(
            profile_.get());
    ansible_management_test_helper_ =
        std::make_unique<AnsibleManagementTestHelper>(profile_.get());
    ansible_management_test_helper_->SetUpAnsibleInfra();
    SetUpViewsEnvironmentForTesting();
  }

  void TearDown() override {
    base::RunLoop().RunUntilIdle();

    TearDownViewsEnvironmentForTesting();

    ansible_management_test_helper_.reset();
    CrostiniManagerTest::TearDown();
  }

 protected:
  MockAnsibleManagementService* mock_ansible_management_service() {
    return mock_ansible_management_service_;
  }

  std::unique_ptr<AnsibleManagementTestHelper> ansible_management_test_helper_;
  raw_ptr<MockAnsibleManagementService, DanglingUntriaged>
      mock_ansible_management_service_;
};

TEST_F(CrostiniManagerAnsibleInfraTest, StartContainerFailure) {
  EXPECT_CALL(*mock_ansible_management_service(), ConfigureContainer).Times(1);
  ON_CALL(*mock_ansible_management_service(), ConfigureContainer)
      .WillByDefault([](const guest_os::GuestId& container_id,
                        base::FilePath playbook,
                        base::OnceCallback<void(bool success)> callback) {
        std::move(callback).Run(false);
      });

  CrostiniManager::RestartOptions ansible_restart;
  ansible_restart.ansible_playbook = profile_->GetPrefs()->GetFilePath(
      prefs::kCrostiniAnsiblePlaybookFilePath);

  TestFuture<CrostiniResult> result_future;
  RestartCrostiniWithOptions(DefaultContainerId(), std::move(ansible_restart),
                             result_future.GetCallback(), this);

  EXPECT_EQ(CrostiniResult::CONTAINER_CONFIGURATION_FAILED,
            result_future.Get());
}

TEST_F(CrostiniManagerAnsibleInfraTest, StartContainerSuccess) {
  EXPECT_CALL(*mock_ansible_management_service(), ConfigureContainer).Times(1);
  ON_CALL(*mock_ansible_management_service(), ConfigureContainer)
      .WillByDefault([](const guest_os::GuestId& container_id,
                        base::FilePath playbook,
                        base::OnceCallback<void(bool success)> callback) {
        std::move(callback).Run(true);
      });

  CrostiniManager::RestartOptions ansible_restart;
  ansible_restart.ansible_playbook = profile_->GetPrefs()->GetFilePath(
      prefs::kCrostiniAnsiblePlaybookFilePath);

  TestFuture<CrostiniResult> result_future;
  RestartCrostiniWithOptions(DefaultContainerId(), std::move(ansible_restart),
                             result_future.GetCallback(), this);

  EXPECT_EQ(CrostiniResult::SUCCESS, result_future.Get());
}

class CrostiniManagerUpgradeContainerTest
    : public CrostiniManagerTest,
      public UpgradeContainerProgressObserver {
 public:
  void SetUp() override {
    CrostiniManagerTest::SetUp();
    progress_signal_.set_owner_id(CryptohomeIdForProfile(profile()));
    progress_signal_.set_vm_name(kVmName);
    progress_signal_.set_container_name(kContainerName);
    progress_run_loop_ = std::make_unique<base::RunLoop>();
    crostini_manager()->AddUpgradeContainerProgressObserver(this);
  }

  void TearDown() override {
    crostini_manager()->RemoveUpgradeContainerProgressObserver(this);
    CrostiniManagerTest::TearDown();
  }

  void RunUntilUpgradeDone(UpgradeContainerProgressStatus final_status) {
    final_status_ = final_status;
    progress_run_loop_->Run();
  }

  void SendProgressSignal() {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&ash::FakeCiceroneClient::NotifyUpgradeContainerProgress,
                       base::Unretained(fake_cicerone_client_),
                       progress_signal_));
  }

 protected:
  // UpgradeContainerProgressObserver
  void OnUpgradeContainerProgress(
      const guest_os::GuestId& container_id,
      UpgradeContainerProgressStatus status,
      const std::vector<std::string>& messages) override {
    if (status == final_status_) {
      progress_run_loop_->Quit();
    }
  }

  guest_os::GuestId container_id_ =
      guest_os::GuestId(kCrostiniDefaultVmType, kVmName, kContainerName);

  UpgradeContainerProgressStatus final_status_ =
      UpgradeContainerProgressStatus::FAILED;

  vm_tools::cicerone::UpgradeContainerProgressSignal progress_signal_;
  // must be created on UI thread
  std::unique_ptr<base::RunLoop> progress_run_loop_;
};

TEST_F(CrostiniManagerUpgradeContainerTest, UpgradeContainerSuccess) {
  TestFuture<CrostiniResult> result_future;
  crostini_manager()->UpgradeContainer(container_id_, ContainerVersion::BUSTER,
                                       result_future.GetCallback());

  EXPECT_EQ(CrostiniResult::SUCCESS, result_future.Get());

  progress_signal_.set_status(
      vm_tools::cicerone::UpgradeContainerProgressSignal::SUCCEEDED);

  SendProgressSignal();
  RunUntilUpgradeDone(UpgradeContainerProgressStatus::SUCCEEDED);
}

TEST_F(CrostiniManagerUpgradeContainerTest, CancelUpgradeContainerSuccess) {
  TestFuture<CrostiniResult> result_future;
  crostini_manager()->UpgradeContainer(container_id_, ContainerVersion::BUSTER,
                                       result_future.GetCallback());

  progress_signal_.set_status(
      vm_tools::cicerone::UpgradeContainerProgressSignal::IN_PROGRESS);

  SendProgressSignal();
  EXPECT_EQ(CrostiniResult::SUCCESS, result_future.Get());

  TestFuture<CrostiniResult> result_future2;
  crostini_manager()->CancelUpgradeContainer(container_id_,
                                             result_future2.GetCallback());

  EXPECT_EQ(CrostiniResult::SUCCESS, result_future2.Get());
}

}  // namespace crostini
