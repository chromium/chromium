// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_manager.h"

#include <memory>

#include "base/base64.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/crostini/ansible/ansible_management_test_helper.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/crostini/crostini_test_util.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/crostini/fake_crostini_features.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/cicerone/cicerone_service.pb.h"
#include "chromeos/dbus/concierge/service.pb.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_cicerone_client.h"
#include "chromeos/dbus/fake_concierge_client.h"
#include "chromeos/disks/mock_disk_mount_manager.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/cpp/test/fake_usb_device_manager.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crostini {

namespace {

const char kVmName[] = "vm_name";
const char kContainerName[] = "container_name";
const char kPackageID[] = "package;1;;";
constexpr int64_t kDiskSizeBytes = 4ll * 1024 * 1024 * 1024;  // 4 GiB
const uint8_t kUsbPort = 0x01;
const char kTerminaKernelVersion[] =
    "4.19.56-05556-gca219a5b1086 #3 SMP PREEMPT Mon Jul 1 14:36:38 CEST 2019";

void ExpectFailure(base::OnceClosure closure, bool success) {
  EXPECT_FALSE(success);
  std::move(closure).Run();
}

void ExpectSuccess(base::OnceClosure closure, bool success) {
  EXPECT_TRUE(success);
  std::move(closure).Run();
}

void ExpectCrostiniResult(base::OnceClosure closure,
                          CrostiniResult expected_result,
                          CrostiniResult result) {
  EXPECT_EQ(expected_result, result);
  std::move(closure).Run();
}

void ExpectCrostiniExportResult(base::OnceClosure closure,
                                CrostiniResult expected_result,
                                uint64_t expected_container_size,
                                uint64_t expected_export_size,
                                CrostiniResult result,
                                uint64_t container_size,
                                uint64_t export_size) {
  EXPECT_EQ(expected_result, result);
  EXPECT_EQ(expected_container_size, container_size);
  EXPECT_EQ(expected_export_size, export_size);
  std::move(closure).Run();
}

}  // namespace

class CrostiniManagerTest : public testing::Test {
 public:
  void CreateDiskImageFailureCallback(
      base::OnceClosure closure,
      bool success,
      vm_tools::concierge::DiskImageStatus status,
      const base::FilePath& file_path) {
    EXPECT_FALSE(fake_concierge_client_->create_disk_image_called());
    EXPECT_FALSE(success);
    EXPECT_EQ(status,
              vm_tools::concierge::DiskImageStatus::DISK_STATUS_UNKNOWN);
    std::move(closure).Run();
  }

  void CreateDiskImageSuccessCallback(
      base::OnceClosure closure,
      bool success,
      vm_tools::concierge::DiskImageStatus status,
      const base::FilePath& file_path) {
    EXPECT_TRUE(fake_concierge_client_->create_disk_image_called());
    EXPECT_TRUE(success);
    std::move(closure).Run();
  }

  void ListVmDisksSuccessCallback(base::OnceClosure closure,
                                  CrostiniResult result,
                                  int64_t total_size) {
    EXPECT_TRUE(fake_concierge_client_->list_vm_disks_called());
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
    EXPECT_TRUE(fake_concierge_client_->attach_usb_device_called());
    EXPECT_EQ(expected_success, success);
    std::move(closure).Run();
  }

  void DetachUsbDeviceCallback(base::OnceClosure closure,
                               bool expected_called,
                               bool expected_success,
                               bool success) {
    EXPECT_EQ(fake_concierge_client_->detach_usb_device_called(),
              expected_called);
    EXPECT_EQ(expected_success, success);
    std::move(closure).Run();
  }

  void ListUsbDevicesCallback(
      base::OnceClosure closure,
      bool expected_success,
      size_t expected_size,
      bool success,
      std::vector<std::pair<std::string, uint8_t>> devices) {
    EXPECT_TRUE(fake_concierge_client_->list_usb_devices_called());
    EXPECT_EQ(expected_success, success);
    EXPECT_EQ(devices.size(), expected_size);
    std::move(closure).Run();
  }

  void GetLinuxPackageInfoFromAptCallback(
      base::OnceClosure closure,
      const LinuxPackageInfo& expected_result,
      const LinuxPackageInfo& result) {
    EXPECT_EQ(result.success, expected_result.success);
    EXPECT_EQ(result.failure_reason, expected_result.failure_reason);
    EXPECT_EQ(result.package_id, expected_result.package_id);
    EXPECT_EQ(result.name, expected_result.name);
    EXPECT_EQ(result.version, expected_result.version);
    EXPECT_EQ(result.summary, expected_result.summary);
    EXPECT_EQ(result.description, expected_result.description);
    std::move(closure).Run();
  }

  CrostiniManagerTest()
      : task_environment_(content::BrowserTaskEnvironment::REAL_IO_THREAD) {
    chromeos::DBusThreadManager::Initialize();
    fake_cicerone_client_ = static_cast<chromeos::FakeCiceroneClient*>(
        chromeos::DBusThreadManager::Get()->GetCiceroneClient());
    fake_concierge_client_ = static_cast<chromeos::FakeConciergeClient*>(
        chromeos::DBusThreadManager::Get()->GetConciergeClient());
  }

  ~CrostiniManagerTest() override { chromeos::DBusThreadManager::Shutdown(); }

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures({features::kCrostini}, {});
    run_loop_ = std::make_unique<base::RunLoop>();
    profile_ = std::make_unique<TestingProfile>();
    crostini_manager_ = CrostiniManager::GetForProfile(profile_.get());

    // Login user for crostini, link gaia for DriveFS.
    auto user_manager = std::make_unique<chromeos::FakeChromeUserManager>();
    AccountId account_id = AccountId::FromUserEmailGaiaId(
        profile()->GetProfileUserName(), "12345");
    user_manager->AddUser(account_id);
    user_manager->LoginUser(account_id);
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(user_manager));

    mojo::Remote<device::mojom::UsbDeviceManager> fake_usb_manager;
    fake_usb_manager_.AddReceiver(
        fake_usb_manager.BindNewPipeAndPassReceiver());
  }

  void TearDown() override {
    scoped_user_manager_.reset();
    crostini_manager_->Shutdown();
    profile_.reset();
    run_loop_.reset();
  }

 protected:
  base::RunLoop* run_loop() { return run_loop_.get(); }
  Profile* profile() { return profile_.get(); }
  CrostiniManager* crostini_manager() { return crostini_manager_; }

  // Owned by chromeos::DBusThreadManager
  chromeos::FakeCiceroneClient* fake_cicerone_client_;
  chromeos::FakeConciergeClient* fake_concierge_client_;

  std::unique_ptr<base::RunLoop>
      run_loop_;  // run_loop_ must be created on the UI thread.
  std::unique_ptr<TestingProfile> profile_;
  CrostiniManager* crostini_manager_;
  device::FakeUsbDeviceManager fake_usb_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  DISALLOW_COPY_AND_ASSIGN(CrostiniManagerTest);
};

TEST_F(CrostiniManagerTest, CreateDiskImageNameError) {
  const base::FilePath& disk_path = base::FilePath("");

  crostini_manager()->CreateDiskImage(
      disk_path, vm_tools::concierge::STORAGE_CRYPTOHOME_ROOT, kDiskSizeBytes,
      base::BindOnce(&CrostiniManagerTest::CreateDiskImageFailureCallback,
                     base::Unretained(this), run_loop()->QuitClosure()));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, CreateDiskImageStorageLocationError) {
  const base::FilePath& disk_path = base::FilePath(kVmName);

  crostini_manager()->CreateDiskImage(
      disk_path,
      vm_tools::concierge::StorageLocation_INT_MIN_SENTINEL_DO_NOT_USE_,
      kDiskSizeBytes,
      base::BindOnce(&CrostiniManagerTest::CreateDiskImageFailureCallback,
                     base::Unretained(this), run_loop()->QuitClosure()));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, CreateDiskImageSuccess) {
  const base::FilePath& disk_path = base::FilePath(kVmName);

  crostini_manager()->CreateDiskImage(
      disk_path, vm_tools::concierge::STORAGE_CRYPTOHOME_ROOT, kDiskSizeBytes,
      base::BindOnce(&CrostiniManagerTest::CreateDiskImageSuccessCallback,
                     base::Unretained(this), run_loop()->QuitClosure()));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, DestroyDiskImageNameError) {
  const base::FilePath& disk_path = base::FilePath("");

  crostini_manager()->DestroyDiskImage(
      disk_path, base::BindOnce(&ExpectFailure, run_loop()->QuitClosure()));
  run_loop()->Run();
  EXPECT_FALSE(fake_concierge_client_->destroy_disk_image_called());
}

TEST_F(CrostiniManagerTest, DestroyDiskImageSuccess) {
  const base::FilePath& disk_path = base::FilePath(kVmName);

  crostini_manager()->DestroyDiskImage(
      disk_path, base::BindOnce(&ExpectSuccess, run_loop()->QuitClosure()));
  run_loop()->Run();
  EXPECT_TRUE(fake_concierge_client_->destroy_disk_image_called());
}

TEST_F(CrostiniManagerTest, ListVmDisksSuccess) {
  crostini_manager()->ListVmDisks(
      base::BindOnce(&CrostiniManagerTest::ListVmDisksSuccessCallback,
                     base::Unretained(this), run_loop()->QuitClosure()));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, StartTerminaVmNameError) {
  const base::FilePath& disk_path = base::FilePath(kVmName);

  crostini_manager()->StartTerminaVm(
      "", disk_path, base::BindOnce(&ExpectFailure, run_loop()->QuitClosure()));
  run_loop()->Run();
  EXPECT_FALSE(fake_concierge_client_->start_termina_vm_called());
}

TEST_F(CrostiniManagerTest, StartTerminaVmDiskPathError) {
  const base::FilePath& disk_path = base::FilePath();

  crostini_manager()->StartTerminaVm(
      kVmName, disk_path,
      base::BindOnce(&ExpectFailure, run_loop()->QuitClosure()));
  run_loop()->Run();
  EXPECT_FALSE(fake_concierge_client_->start_termina_vm_called());
}

TEST_F(CrostiniManagerTest, StartTerminaVmSuccess) {
  const base::FilePath& disk_path = base::FilePath(kVmName);

  crostini_manager()->StartTerminaVm(
      kVmName, disk_path,
      base::BindOnce(&ExpectSuccess, run_loop()->QuitClosure()));
  run_loop()->Run();
  EXPECT_TRUE(fake_concierge_client_->start_termina_vm_called());
}

TEST_F(CrostiniManagerTest, OnStartTremplinRecordsRunningVm) {
  const base::FilePath& disk_path = base::FilePath(kVmName);
  const std::string owner_id = CryptohomeIdForProfile(profile());

  // Start the Vm.
  crostini_manager()->StartTerminaVm(
      kVmName, disk_path,
      base::BindOnce(&ExpectSuccess, run_loop()->QuitClosure()));

  // Check that the Vm start is not recorded until tremplin starts.
  EXPECT_FALSE(crostini_manager()->IsVmRunning(kVmName));
  run_loop()->Run();
  EXPECT_TRUE(crostini_manager()->IsVmRunning(kVmName));
}

TEST_F(CrostiniManagerTest, StopVmNameError) {
  crostini_manager()->StopVm(
      "", base::BindOnce(&ExpectCrostiniResult, run_loop()->QuitClosure(),
                         CrostiniResult::CLIENT_ERROR));
  run_loop()->Run();
  EXPECT_FALSE(fake_concierge_client_->stop_vm_called());
}

TEST_F(CrostiniManagerTest, StopVmSuccess) {
  crostini_manager()->StopVm(
      kVmName, base::BindOnce(&ExpectCrostiniResult, run_loop()->QuitClosure(),
                              CrostiniResult::SUCCESS));
  run_loop()->Run();
  EXPECT_TRUE(fake_concierge_client_->stop_vm_called());
}

TEST_F(CrostiniManagerTest, InstallLinuxPackageRootAccessError) {
  FakeCrostiniFeatures crostini_features;
  crostini_features.set_root_access_allowed(false);
  crostini_manager()->InstallLinuxPackage(
      kVmName, kContainerName, "/tmp/package.deb",
      base::BindOnce(&ExpectCrostiniResult, run_loop()->QuitClosure(),
                     CrostiniResult::INSTALL_LINUX_PACKAGE_FAILED));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, InstallLinuxPackageSignalNotConnectedError) {
  fake_cicerone_client_->set_install_linux_package_progress_signal_connected(
      false);
  crostini_manager()->InstallLinuxPackage(
      kVmName, kContainerName, "/tmp/package.deb",
      base::BindOnce(&ExpectCrostiniResult, run_loop()->QuitClosure(),
                     CrostiniResult::INSTALL_LINUX_PACKAGE_FAILED));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, InstallLinuxPackageSignalSuccess) {
  vm_tools::cicerone::InstallLinuxPackageResponse response;
  response.set_status(vm_tools::cicerone::InstallLinuxPackageResponse::STARTED);
  fake_cicerone_client_->set_install_linux_package_response(response);
  crostini_manager()->InstallLinuxPackage(
      kVmName, kContainerName, "/tmp/package.deb",
      base::BindOnce(&ExpectCrostiniResult, run_loop()->QuitClosure(),
                     CrostiniResult::SUCCESS));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, InstallLinuxPackageSignalFailure) {
  vm_tools::cicerone::InstallLinuxPackageResponse response;
  std::string failure_reason = "Unit tests can't install Linux packages!";
  response.set_status(vm_tools::cicerone::InstallLinuxPackageResponse::FAILED);
  response.set_failure_reason(failure_reason);
  fake_cicerone_client_->set_install_linux_package_response(response);
  crostini_manager()->InstallLinuxPackage(
      kVmName, kContainerName, "/tmp/package.deb",
      base::BindOnce(&ExpectCrostiniResult, run_loop()->QuitClosure(),
                     CrostiniResult::INSTALL_LINUX_PACKAGE_FAILED));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, InstallLinuxPackageSignalOperationBlocked) {
  vm_tools::cicerone::InstallLinuxPackageResponse response;
  response.set_status(
      vm_tools::cicerone::InstallLinuxPackageResponse::INSTALL_ALREADY_ACTIVE);
  fake_cicerone_client_->set_install_linux_package_response(response);
  crostini_manager()->InstallLinuxPackage(
      kVmName, kContainerName, "/tmp/package.deb",
      base::BindOnce(&ExpectCrostiniResult, run_loop()->QuitClosure(),
                     CrostiniResult::BLOCKING_OPERATION_ALREADY_ACTIVE));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, UninstallPackageOwningFileSignalNotConnectedError) {
  fake_cicerone_client_->set_uninstall_package_progress_signal_connected(false);
  crostini_manager()->UninstallPackageOwningFile(
      kVmName, kContainerName, "emacs",
      base::BindOnce(&ExpectCrostiniResult, run_loop()->QuitClosure(),
                     CrostiniResult::UNINSTALL_PACKAGE_FAILED));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, UninstallPackageOwningFileSignalSuccess) {
  vm_tools::cicerone::UninstallPackageOwningFileResponse response;
  response.set_status(
      vm_tools::cicerone::UninstallPackageOwningFileResponse::STARTED);
  fake_cicerone_client_->set_uninstall_package_owning_file_response(response);
  crostini_manager()->UninstallPackageOwningFile(
      kVmName, kContainerName, "emacs",
      base::BindOnce(&ExpectCrostiniResult, run_loop()->QuitClosure(),
                     CrostiniResult::SUCCESS));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, UninstallPackageOwningFileSignalFailure) {
  vm_tools::cicerone::UninstallPackageOwningFileResponse response;
  response.set_status(
      vm_tools::cicerone::UninstallPackageOwningFileResponse::FAILED);
  response.set_failure_reason("Didn't feel like it");
  fake_cicerone_client_->set_uninstall_package_owning_file_response(response);
  crostini_manager()->UninstallPackageOwningFile(
      kVmName, kContainerName, "emacs",
      base::BindOnce(&ExpectCrostiniResult, run_loop()->QuitClosure(),
                     CrostiniResult::UNINSTALL_PACKAGE_FAILED));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, UninstallPackageOwningFileSignalOperationBlocked) {
  vm_tools::cicerone::UninstallPackageOwningFileResponse response;
  response.set_status(vm_tools::cicerone::UninstallPackageOwningFileResponse::
                          BLOCKING_OPERATION_IN_PROGRESS);
  fake_cicerone_client_->set_uninstall_package_owning_file_response(response);
  crostini_manager()->UninstallPackageOwningFile(
      kVmName, kContainerName, "emacs",
      base::BindOnce(&ExpectCrostiniResult, run_loop()->QuitClosure(),
                     CrostiniResult::BLOCKING_OPERATION_ALREADY_ACTIVE));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, AttachUsbDeviceSuccess) {
  vm_tools::concierge::AttachUsbDeviceResponse response;
  response.set_success(true);
  fake_concierge_client_->set_attach_usb_device_response(response);

  auto fake_usb = fake_usb_manager_.CreateAndAddDevice(0, 0);
  auto guid = fake_usb->guid;

  crostini_manager()->AttachUsbDevice(
      kVmName, std::move(fake_usb), TestFileDescriptor(),
      base::BindOnce(&CrostiniManagerTest::AttachUsbDeviceCallback,
                     base::Unretained(this), run_loop()->QuitClosure(),
                     /*expected_success=*/true));
  run_loop()->Run();
  fake_usb_manager_.RemoveDevice(guid);
}

TEST_F(CrostiniManagerTest, AttachUsbDeviceFailure) {
  vm_tools::concierge::AttachUsbDeviceResponse response;
  response.set_success(false);
  fake_concierge_client_->set_attach_usb_device_response(response);

  auto fake_usb = fake_usb_manager_.CreateAndAddDevice(0, 0);
  auto guid = fake_usb->guid;

  crostini_manager()->AttachUsbDevice(
      kVmName, std::move(fake_usb), TestFileDescriptor(),
      base::BindOnce(&CrostiniManagerTest::AttachUsbDeviceCallback,
                     base::Unretained(this), run_loop()->QuitClosure(),
                     /*expected_success=*/false));
  run_loop()->Run();
  fake_usb_manager_.RemoveDevice(guid);
}

TEST_F(CrostiniManagerTest, DetachUsbDeviceSuccess) {
  vm_tools::concierge::AttachUsbDeviceResponse attach_response;
  attach_response.set_success(true);
  fake_concierge_client_->set_attach_usb_device_response(attach_response);

  vm_tools::concierge::DetachUsbDeviceResponse detach_response;
  detach_response.set_success(true);
  fake_concierge_client_->set_detach_usb_device_response(detach_response);

  auto fake_usb = fake_usb_manager_.CreateAndAddDevice(0, 0);
  auto guid = fake_usb->guid;

  auto detach_usb = base::BindOnce(
      &CrostiniManager::DetachUsbDevice, base::Unretained(crostini_manager()),
      kVmName, fake_usb.Clone(), kUsbPort,
      base::BindOnce(&CrostiniManagerTest::DetachUsbDeviceCallback,
                     base::Unretained(this), run_loop()->QuitClosure(), true,
                     /*expected_success=*/true));

  crostini_manager()->AttachUsbDevice(
      kVmName, std::move(fake_usb), TestFileDescriptor(),
      base::BindOnce(&CrostiniManagerTest::AttachUsbDeviceCallback,
                     base::Unretained(this), std::move(detach_usb),
                     /*expected_success=*/true));
  run_loop()->Run();
  fake_usb_manager_.RemoveDevice(guid);
}

TEST_F(CrostiniManagerTest, DetachUsbDeviceFailure) {
  vm_tools::concierge::AttachUsbDeviceResponse attach_response;
  attach_response.set_success(true);
  fake_concierge_client_->set_attach_usb_device_response(attach_response);

  vm_tools::concierge::DetachUsbDeviceResponse detach_response;
  detach_response.set_success(false);
  fake_concierge_client_->set_detach_usb_device_response(detach_response);

  auto fake_usb = fake_usb_manager_.CreateAndAddDevice(0, 0);
  auto guid = fake_usb->guid;

  auto detach_usb = base::BindOnce(
      &CrostiniManager::DetachUsbDevice, base::Unretained(crostini_manager()),
      kVmName, fake_usb.Clone(), kUsbPort,
      base::BindOnce(&CrostiniManagerTest::DetachUsbDeviceCallback,
                     base::Unretained(this), run_loop()->QuitClosure(), true,
                     /*expected_success=*/false));

  crostini_manager()->AttachUsbDevice(
      kVmName, std::move(fake_usb), TestFileDescriptor(),
      base::BindOnce(&CrostiniManagerTest::AttachUsbDeviceCallback,
                     base::Unretained(this), std::move(detach_usb),
                     /*expected_success=*/true));
  run_loop()->Run();
  fake_usb_manager_.RemoveDevice(guid);
}

TEST_F(CrostiniManagerTest, ListUsbDeviceFailure) {
  vm_tools::concierge::ListUsbDeviceResponse response;
  response.set_success(false);
  fake_concierge_client_->set_list_usb_devices_response(response);

  crostini_manager()->ListUsbDevices(
      kVmName, base::BindOnce(&CrostiniManagerTest::ListUsbDevicesCallback,
                              base::Unretained(this), run_loop()->QuitClosure(),
                              /*expected_success=*/false, 0));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, ListUsbDeviceEmptySuccess) {
  vm_tools::concierge::ListUsbDeviceResponse response;
  response.set_success(true);
  fake_concierge_client_->set_list_usb_devices_response(response);

  crostini_manager()->ListUsbDevices(
      kVmName, base::BindOnce(&CrostiniManagerTest::ListUsbDevicesCallback,
                              base::Unretained(this), run_loop()->QuitClosure(),
                              /*expected_success=*/true, 0));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, ListUsbDeviceOne) {
  vm_tools::concierge::AttachUsbDeviceResponse attach_response;
  attach_response.set_success(true);
  attach_response.set_guest_port(1);
  fake_concierge_client_->set_attach_usb_device_response(attach_response);

  auto fake_usb = fake_usb_manager_.CreateAndAddDevice(0, 0);
  auto guid = fake_usb->guid;

  crostini_manager()->AttachUsbDevice(
      kVmName, std::move(fake_usb), TestFileDescriptor(),
      base::BindOnce(&CrostiniManagerTest::AttachUsbDeviceCallback,
                     base::Unretained(this), run_loop()->QuitClosure(),
                     /*expected_success=*/true));
  run_loop()->Run();

  vm_tools::concierge::ListUsbDeviceResponse response;
  response.set_success(true);
  auto* msg = response.add_usb_devices();
  msg->set_guest_port(1);
  fake_concierge_client_->set_list_usb_devices_response(response);

  base::RunLoop run_loop2;
  crostini_manager()->ListUsbDevices(
      kVmName, base::BindOnce(&CrostiniManagerTest::ListUsbDevicesCallback,
                              base::Unretained(this), run_loop2.QuitClosure(),
                              /*expected_success=*/true, 1));
  run_loop2.Run();

  fake_usb_manager_.RemoveDevice(guid);
}

class CrostiniManagerRestartTest : public CrostiniManagerTest,
                                   public CrostiniManager::RestartObserver {
 public:
  void RestartCrostiniCallback(base::OnceClosure closure,
                               CrostiniResult result) {
    restart_crostini_callback_count_++;
    std::move(closure).Run();
  }

  void RemoveCrostiniCallback(base::OnceClosure closure,
                              CrostiniResult result) {
    remove_crostini_callback_count_++;
    std::move(closure).Run();
  }

  // CrostiniManager::RestartObserver
  void OnStageStarted(mojom::InstallerState stage) override {}

  void OnComponentLoaded(CrostiniResult result) override {
    if (abort_on_component_loaded_) {
      Abort();
    }
  }

  void OnConciergeStarted(bool success) override {
    if (abort_on_concierge_started_) {
      Abort();
    }
  }

  void OnDiskImageCreated(bool success,
                          vm_tools::concierge::DiskImageStatus status,
                          int64_t disk_size_available) override {
    if (abort_on_disk_image_created_) {
      Abort();
    }
  }

  void OnVmStarted(bool success) override {
    if (abort_on_vm_started_) {
      Abort();
    }
  }

  void OnContainerDownloading(int32_t download_percent) override {}

  void OnContainerCreated(CrostiniResult result) override {
    if (abort_on_container_created_) {
      Abort();
    }
    if (abort_then_stop_vm_) {
      Abort();
      vm_tools::concierge::VmStoppedSignal signal;
      signal.set_owner_id(CryptohomeIdForProfile(profile()));
      signal.set_name(kVmName);
      crostini_manager()->OnVmStopped(signal);
    }
  }

  void OnContainerStarted(CrostiniResult result) override {
    if (abort_on_container_started_) {
      Abort();
    }
  }

  void OnContainerSetup(bool success) override {
    if (abort_on_container_setup_) {
      Abort();
    }
  }

  void OnSshKeysFetched(bool success) override {
    if (abort_on_ssh_keys_fetched_) {
      Abort();
    }
  }

  void OnContainerMounted(bool success) override {
    if (abort_on_container_mounted_) {
      Abort();
    }
  }

 protected:
  void Abort() {
    crostini_manager()->AbortRestartCrostini(restart_id_,
                                             base::DoNothing::Once());
    run_loop()->Quit();
  }

  void SshfsMount(const std::string& source_path,
                  const std::string& source_format,
                  const std::string& mount_label,
                  const std::vector<std::string>& mount_options,
                  chromeos::MountType type,
                  chromeos::MountAccessMode access_mode) {
    if (abort_on_mount_event_) {
      Abort();
    }
    disk_mount_manager_mock_->NotifyMountEvent(
        chromeos::disks::DiskMountManager::MountEvent::MOUNTING, mount_error_,
        chromeos::disks::DiskMountManager::MountPointInfo(
            source_path, "/media/fuse/" + mount_label,
            chromeos::MountType::MOUNT_TYPE_NETWORK_STORAGE,
            chromeos::disks::MountCondition::MOUNT_CONDITION_NONE));
  }

  CrostiniManager::RestartId restart_id_ =
      CrostiniManager::kUninitializedRestartId;
  const CrostiniManager::RestartId uninitialized_id_ =
      CrostiniManager::kUninitializedRestartId;
  bool abort_on_component_loaded_ = false;
  bool abort_on_concierge_started_ = false;
  bool abort_on_disk_image_created_ = false;
  bool abort_on_vm_started_ = false;
  bool abort_on_container_created_ = false;
  bool abort_on_container_started_ = false;
  bool abort_on_container_setup_ = false;
  bool abort_on_ssh_keys_fetched_ = false;
  bool abort_on_container_mounted_ = false;
  bool abort_then_stop_vm_ = false;

  // Used by SshfsMount().
  bool abort_on_mount_event_ = false;
  chromeos::MountError mount_error_ = chromeos::MountError::MOUNT_ERROR_NONE;

  int restart_crostini_callback_count_ = 0;
  int remove_crostini_callback_count_ = 0;
  chromeos::disks::MockDiskMountManager* disk_mount_manager_mock_;
};

TEST_F(CrostiniManagerRestartTest, RestartSuccess) {
  restart_id_ = crostini_manager()->RestartCrostini(
      kVmName, kContainerName,
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  run_loop()->Run();
  EXPECT_TRUE(fake_concierge_client_->create_disk_image_called());
  EXPECT_TRUE(fake_concierge_client_->start_termina_vm_called());
  // Mount only performed for termina/penguin.
  EXPECT_FALSE(fake_concierge_client_->get_container_ssh_keys_called());
  EXPECT_EQ(1, restart_crostini_callback_count_);

  base::Optional<ContainerInfo> container_info =
      crostini_manager()->GetContainerInfo(kVmName, kContainerName);
  EXPECT_EQ(container_info.value().username,
            DefaultContainerUserNameForProfile(profile()));
}

TEST_F(CrostiniManagerRestartTest, RestartSuccessWithOptions) {
  CrostiniManager::RestartOptions options;
  options.container_username = "helloworld";
  restart_id_ = crostini_manager()->RestartCrostiniWithOptions(
      kVmName, kContainerName, std::move(options),
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  run_loop()->Run();
  EXPECT_TRUE(fake_concierge_client_->create_disk_image_called());
  EXPECT_TRUE(fake_concierge_client_->start_termina_vm_called());
  // Mount only performed for termina/penguin.
  EXPECT_FALSE(fake_concierge_client_->get_container_ssh_keys_called());
  EXPECT_EQ(1, restart_crostini_callback_count_);

  base::Optional<ContainerInfo> container_info =
      crostini_manager()->GetContainerInfo(kVmName, kContainerName);
  EXPECT_EQ(container_info.value().username, "helloworld");
}

TEST_F(CrostiniManagerRestartTest, AbortOnComponentLoaded) {
  abort_on_component_loaded_ = true;
  restart_id_ = crostini_manager()->RestartCrostini(
      kVmName, kContainerName,
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  run_loop()->Run();
  EXPECT_FALSE(
      profile_->GetPrefs()->GetBoolean(crostini::prefs::kCrostiniEnabled));
  EXPECT_FALSE(fake_concierge_client_->create_disk_image_called());
  EXPECT_FALSE(fake_concierge_client_->start_termina_vm_called());
  EXPECT_FALSE(fake_concierge_client_->get_container_ssh_keys_called());
  EXPECT_EQ(0, restart_crostini_callback_count_);
}

TEST_F(CrostiniManagerRestartTest, AbortOnConciergeStarted) {
  abort_on_concierge_started_ = true;
  restart_id_ = crostini_manager()->RestartCrostini(
      kVmName, kContainerName,
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  run_loop()->Run();
  EXPECT_FALSE(fake_concierge_client_->create_disk_image_called());
  EXPECT_FALSE(fake_concierge_client_->start_termina_vm_called());
  EXPECT_FALSE(fake_concierge_client_->get_container_ssh_keys_called());
  EXPECT_EQ(0, restart_crostini_callback_count_);
}

TEST_F(CrostiniManagerRestartTest, AbortOnDiskImageCreated) {
  abort_on_disk_image_created_ = true;
  restart_id_ = crostini_manager()->RestartCrostini(
      kVmName, kContainerName,
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  run_loop()->Run();
  EXPECT_TRUE(fake_concierge_client_->create_disk_image_called());
  EXPECT_FALSE(fake_concierge_client_->start_termina_vm_called());
  EXPECT_FALSE(fake_concierge_client_->get_container_ssh_keys_called());
  EXPECT_EQ(0, restart_crostini_callback_count_);
}

TEST_F(CrostiniManagerRestartTest, AbortOnVmStarted) {
  abort_on_vm_started_ = true;
  restart_id_ = crostini_manager()->RestartCrostini(
      kVmName, kContainerName,
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  run_loop()->Run();
  EXPECT_TRUE(fake_concierge_client_->create_disk_image_called());
  EXPECT_TRUE(fake_concierge_client_->start_termina_vm_called());
  EXPECT_FALSE(fake_concierge_client_->get_container_ssh_keys_called());
  EXPECT_EQ(0, restart_crostini_callback_count_);
}

TEST_F(CrostiniManagerRestartTest, AbortOnContainerCreated) {
  abort_on_container_created_ = true;
  // Use termina/penguin names to allow fetch ssh keys.
  restart_id_ = crostini_manager()->RestartCrostini(
      kCrostiniDefaultVmName, kCrostiniDefaultContainerName,
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  run_loop()->Run();
  EXPECT_TRUE(fake_concierge_client_->create_disk_image_called());
  EXPECT_TRUE(fake_concierge_client_->start_termina_vm_called());
  EXPECT_FALSE(fake_concierge_client_->get_container_ssh_keys_called());
  EXPECT_EQ(0, restart_crostini_callback_count_);
}

TEST_F(CrostiniManagerRestartTest, AbortOnContainerCreatedError) {
  abort_on_container_started_ = true;
  fake_cicerone_client_->set_lxd_container_created_signal_status(
      vm_tools::cicerone::LxdContainerCreatedSignal::UNKNOWN);
  // Use termina/penguin names to allow fetch ssh keys.
  restart_id_ = crostini_manager()->RestartCrostini(
      kCrostiniDefaultVmName, kCrostiniDefaultContainerName,
      base::BindOnce(&ExpectCrostiniResult, run_loop()->QuitClosure(),
                     CrostiniResult::UNKNOWN_ERROR),
      this);
  run_loop()->Run();

  EXPECT_TRUE(fake_concierge_client_->create_disk_image_called());
  EXPECT_TRUE(fake_concierge_client_->start_termina_vm_called());
  EXPECT_FALSE(fake_concierge_client_->get_container_ssh_keys_called());
  EXPECT_EQ(0, restart_crostini_callback_count_);
}

TEST_F(CrostiniManagerRestartTest, AbortOnContainerStarted) {
  abort_on_container_started_ = true;
  // Use termina/penguin names to allow fetch ssh keys.
  restart_id_ = crostini_manager()->RestartCrostini(
      kCrostiniDefaultVmName, kCrostiniDefaultContainerName,
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  run_loop()->Run();
  EXPECT_TRUE(fake_concierge_client_->create_disk_image_called());
  EXPECT_TRUE(fake_concierge_client_->start_termina_vm_called());
  EXPECT_FALSE(fake_concierge_client_->get_container_ssh_keys_called());
  EXPECT_EQ(0, restart_crostini_callback_count_);
}

TEST_F(CrostiniManagerRestartTest, AbortOnContainerSetup) {
  abort_on_container_setup_ = true;
  // Use termina/penguin names to allow fetch ssh keys.
  restart_id_ = crostini_manager()->RestartCrostini(
      kCrostiniDefaultVmName, kCrostiniDefaultContainerName,
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  run_loop()->Run();
  EXPECT_TRUE(fake_concierge_client_->create_disk_image_called());
  EXPECT_TRUE(fake_concierge_client_->start_termina_vm_called());
  EXPECT_FALSE(fake_concierge_client_->get_container_ssh_keys_called());
  EXPECT_EQ(0, restart_crostini_callback_count_);
}

TEST_F(CrostiniManagerRestartTest, AbortOnSshKeysFetched) {
  abort_on_ssh_keys_fetched_ = true;
  // Use termina/penguin names to allow fetch ssh keys.
  restart_id_ = crostini_manager()->RestartCrostini(
      kCrostiniDefaultVmName, kCrostiniDefaultContainerName,
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  run_loop()->Run();
  EXPECT_TRUE(fake_concierge_client_->create_disk_image_called());
  EXPECT_TRUE(fake_concierge_client_->start_termina_vm_called());
  EXPECT_TRUE(fake_concierge_client_->get_container_ssh_keys_called());
  EXPECT_EQ(0, restart_crostini_callback_count_);
}

TEST_F(CrostiniManagerRestartTest, AbortOnContainerMounted) {
  abort_on_container_mounted_ = true;

  disk_mount_manager_mock_ = new chromeos::disks::MockDiskMountManager;
  chromeos::disks::DiskMountManager::InitializeForTesting(
      disk_mount_manager_mock_);
  EXPECT_CALL(*disk_mount_manager_mock_, MountPath)
      .WillOnce(Invoke(
          this, &CrostiniManagerRestartTest_AbortOnContainerMounted_Test::
                    SshfsMount));

  // Use termina/penguin names to allow fetch ssh keys.
  restart_id_ = crostini_manager()->RestartCrostini(
      kCrostiniDefaultVmName, kCrostiniDefaultContainerName,
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  run_loop()->Run();
  EXPECT_TRUE(fake_concierge_client_->create_disk_image_called());
  EXPECT_TRUE(fake_concierge_client_->start_termina_vm_called());
  EXPECT_TRUE(fake_concierge_client_->get_container_ssh_keys_called());
  EXPECT_EQ(0, restart_crostini_callback_count_);

  chromeos::disks::DiskMountManager::Shutdown();
}

TEST_F(CrostiniManagerRestartTest, AbortOnMountEvent) {
  abort_on_mount_event_ = true;

  disk_mount_manager_mock_ = new chromeos::disks::MockDiskMountManager;
  chromeos::disks::DiskMountManager::InitializeForTesting(
      disk_mount_manager_mock_);
  EXPECT_CALL(*disk_mount_manager_mock_, MountPath)
      .WillOnce(Invoke(
          this,
          &CrostiniManagerRestartTest_AbortOnMountEvent_Test::SshfsMount));

  // Use termina/penguin names to allow fetch ssh keys.
  restart_id_ = crostini_manager()->RestartCrostini(
      kCrostiniDefaultVmName, kCrostiniDefaultContainerName,
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  run_loop()->Run();
  EXPECT_TRUE(fake_concierge_client_->create_disk_image_called());
  EXPECT_TRUE(fake_concierge_client_->start_termina_vm_called());
  EXPECT_TRUE(fake_concierge_client_->get_container_ssh_keys_called());
  EXPECT_EQ(0, restart_crostini_callback_count_);

  chromeos::disks::DiskMountManager::Shutdown();
}

TEST_F(CrostiniManagerRestartTest, AbortOnMountEventWithError) {
  mount_error_ = chromeos::MountError::MOUNT_ERROR_INSUFFICIENT_PERMISSIONS;
  abort_on_mount_event_ = true;

  disk_mount_manager_mock_ = new chromeos::disks::MockDiskMountManager;
  chromeos::disks::DiskMountManager::InitializeForTesting(
      disk_mount_manager_mock_);
  EXPECT_CALL(*disk_mount_manager_mock_, MountPath)
      .WillOnce(Invoke(
          this, &CrostiniManagerRestartTest_AbortOnMountEventWithError_Test::
                    SshfsMount));

  // Use termina/penguin names to allow fetch ssh keys.
  restart_id_ = crostini_manager()->RestartCrostini(
      kCrostiniDefaultVmName, kCrostiniDefaultContainerName,
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  run_loop()->Run();
  EXPECT_TRUE(fake_concierge_client_->create_disk_image_called());
  EXPECT_TRUE(fake_concierge_client_->start_termina_vm_called());
  EXPECT_TRUE(fake_concierge_client_->get_container_ssh_keys_called());
  EXPECT_EQ(0, restart_crostini_callback_count_);

  chromeos::disks::DiskMountManager::Shutdown();
}

TEST_F(CrostiniManagerRestartTest, AbortThenStopVm) {
  abort_then_stop_vm_ = true;
  restart_id_ = crostini_manager()->RestartCrostini(
      kVmName, kContainerName,
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  run_loop()->Run();
  EXPECT_TRUE(fake_concierge_client_->create_disk_image_called());
  EXPECT_TRUE(fake_concierge_client_->start_termina_vm_called());
  EXPECT_FALSE(fake_concierge_client_->get_container_ssh_keys_called());
  EXPECT_EQ(0, restart_crostini_callback_count_);
}

TEST_F(CrostiniManagerRestartTest, OnlyMountTerminaPenguin) {
  // Use names other than termina/penguin.  Will not mount sshfs.
  restart_id_ = crostini_manager()->RestartCrostini(
      kVmName, kContainerName,
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  run_loop()->Run();
  EXPECT_TRUE(fake_concierge_client_->create_disk_image_called());
  EXPECT_TRUE(fake_concierge_client_->start_termina_vm_called());
  EXPECT_FALSE(fake_concierge_client_->get_container_ssh_keys_called());
  EXPECT_EQ(1, restart_crostini_callback_count_);
}

TEST_F(CrostiniManagerRestartTest, MultiRestartAllowed) {
  CrostiniManager::RestartId id1, id2, id3;
  id1 = crostini_manager()->RestartCrostini(
      kVmName, kContainerName,
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()));
  id2 = crostini_manager()->RestartCrostini(
      kVmName, kContainerName,
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()));
  id3 = crostini_manager()->RestartCrostini(
      kVmName, kContainerName,
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()));

  EXPECT_TRUE(crostini_manager()->IsRestartPending(id1));
  EXPECT_TRUE(crostini_manager()->IsRestartPending(id2));
  EXPECT_TRUE(crostini_manager()->IsRestartPending(id3));

  run_loop()->Run();
  EXPECT_TRUE(fake_concierge_client_->create_disk_image_called());
  EXPECT_TRUE(fake_concierge_client_->start_termina_vm_called());
  EXPECT_EQ(3, restart_crostini_callback_count_);

  EXPECT_FALSE(crostini_manager()->IsRestartPending(id1));
  EXPECT_FALSE(crostini_manager()->IsRestartPending(id2));
  EXPECT_FALSE(crostini_manager()->IsRestartPending(id3));
}

TEST_F(CrostiniManagerRestartTest, MountForTerminaPenguin) {
  // DiskMountManager mock.  Verify that correct values are received
  // from concierge and passed to DiskMountManager.
  disk_mount_manager_mock_ = new chromeos::disks::MockDiskMountManager;
  chromeos::disks::DiskMountManager::InitializeForTesting(
      disk_mount_manager_mock_);
  disk_mount_manager_mock_->SetupDefaultReplies();
  std::string known_hosts;
  base::Base64Encode("[hostname]:2222 pubkey", &known_hosts);
  std::string identity;
  base::Base64Encode("privkey", &identity);
  std::vector<std::string> mount_options = {
      "UserKnownHostsBase64=" + known_hosts, "IdentityBase64=" + identity,
      "Port=2222"};
  EXPECT_CALL(*disk_mount_manager_mock_,
              MountPath("sshfs://testing_profile@hostname:", "",
                        "crostini_test_termina_penguin", mount_options,
                        chromeos::MOUNT_TYPE_NETWORK_STORAGE,
                        chromeos::MOUNT_ACCESS_MODE_READ_WRITE))
      .WillOnce(Invoke(
          this,
          &CrostiniManagerRestartTest_MountForTerminaPenguin_Test::SshfsMount));

  // Use termina/penguin to perform mount.
  restart_id_ = crostini_manager()->RestartCrostini(
      kCrostiniDefaultVmName, kCrostiniDefaultContainerName,
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()));
  run_loop()->Run();
  EXPECT_TRUE(fake_concierge_client_->create_disk_image_called());
  EXPECT_TRUE(fake_concierge_client_->start_termina_vm_called());
  EXPECT_TRUE(fake_concierge_client_->get_container_ssh_keys_called());
  EXPECT_TRUE(crostini_manager()
                  ->GetContainerInfo(kCrostiniDefaultVmName,
                                     kCrostiniDefaultContainerName)
                  ->sshfs_mounted);
  EXPECT_EQ(1, restart_crostini_callback_count_);
  base::FilePath path;
  EXPECT_TRUE(
      storage::ExternalMountPoints::GetSystemInstance()->GetRegisteredPath(
          "crostini_test_termina_penguin", &path));
  EXPECT_EQ(base::FilePath("/media/fuse/crostini_test_termina_penguin"), path);

  chromeos::disks::DiskMountManager::Shutdown();
}

TEST_F(CrostiniManagerRestartTest, IsContainerRunningFalseIfVmNotStarted) {
  restart_id_ = crostini_manager()->RestartCrostini(
      kVmName, kContainerName,
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  EXPECT_TRUE(crostini_manager()->IsRestartPending(restart_id_));
  run_loop()->Run();

  EXPECT_TRUE(fake_concierge_client_->create_disk_image_called());
  EXPECT_TRUE(fake_concierge_client_->start_termina_vm_called());
  // Mount only performed for termina/penguin.
  EXPECT_FALSE(fake_concierge_client_->get_container_ssh_keys_called());
  EXPECT_EQ(1, restart_crostini_callback_count_);

  EXPECT_TRUE(crostini_manager()->IsVmRunning(kVmName));
  EXPECT_TRUE(crostini_manager()->GetContainerInfo(kVmName, kContainerName));

  // Now call StartTerminaVm again. The default response state is "STARTING",
  // so no container should be considered running.
  const base::FilePath& disk_path = base::FilePath(kVmName);

  base::RunLoop run_loop2;
  crostini_manager()->StartTerminaVm(
      kVmName, disk_path,
      base::BindOnce(&ExpectSuccess, run_loop2.QuitClosure()));
  run_loop2.Run();
  EXPECT_TRUE(fake_concierge_client_->start_termina_vm_called());
  EXPECT_TRUE(crostini_manager()->IsVmRunning(kVmName));
  EXPECT_FALSE(crostini_manager()->GetContainerInfo(kVmName, kContainerName));
}

TEST_F(CrostiniManagerRestartTest, OsReleaseSetCorrectly) {
  vm_tools::cicerone::OsRelease os_release;
  os_release.set_pretty_name("Debian GNU/Linux 10 (buster)");
  fake_cicerone_client_->set_lxd_container_os_release(os_release);

  restart_id_ = crostini_manager()->RestartCrostini(
      kVmName, kContainerName,
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  EXPECT_TRUE(crostini_manager()->IsRestartPending(restart_id_));
  run_loop()->Run();

  const auto* stored_os_release =
      crostini_manager()->GetContainerOsRelease(kVmName, kContainerName);
  EXPECT_NE(stored_os_release, nullptr);
  // Sadly, we can't use MessageDifferencer here because we're using the LITE
  // API in our protos.
  EXPECT_EQ(os_release.SerializeAsString(),
            stored_os_release->SerializeAsString());
}

TEST_F(CrostiniManagerRestartTest, RestartThenUninstall) {
  restart_id_ = crostini_manager()->RestartCrostini(
      kVmName, kContainerName,
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), base::DoNothing::Once()));

  EXPECT_TRUE(crostini_manager()->IsRestartPending(restart_id_));

  crostini_manager()->RemoveCrostini(
      kVmName,
      base::BindOnce(&CrostiniManagerRestartTest::RemoveCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()));

  EXPECT_FALSE(crostini_manager()->IsRestartPending(restart_id_));

  run_loop()->Run();

  // Aborts don't call the restart callback. If that changes, everything that
  // calls RestartCrostini will need to be checked to make sure they handle it
  // in a sensible way.
  EXPECT_EQ(0, restart_crostini_callback_count_);
  EXPECT_EQ(1, remove_crostini_callback_count_);
}

TEST_F(CrostiniManagerRestartTest, RestartMultipleThenUninstall) {
  CrostiniManager::RestartId id1, id2, id3;
  id1 = crostini_manager()->RestartCrostini(
      kVmName, kContainerName,
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), base::DoNothing::Once()));
  id2 = crostini_manager()->RestartCrostini(
      kVmName, kContainerName,
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), base::DoNothing::Once()));
  id3 = crostini_manager()->RestartCrostini(
      kVmName, kContainerName,
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), base::DoNothing::Once()));

  EXPECT_TRUE(crostini_manager()->IsRestartPending(id1));
  EXPECT_TRUE(crostini_manager()->IsRestartPending(id2));
  EXPECT_TRUE(crostini_manager()->IsRestartPending(id3));

  crostini_manager()->RemoveCrostini(
      kVmName,
      base::BindOnce(&CrostiniManagerRestartTest::RemoveCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()));

  EXPECT_FALSE(crostini_manager()->IsRestartPending(id1));
  EXPECT_FALSE(crostini_manager()->IsRestartPending(id2));
  EXPECT_FALSE(crostini_manager()->IsRestartPending(id3));

  run_loop()->Run();

  // Aborts don't call the restart callback. If that changes, everything that
  // calls RestartCrostini will need to be checked to make sure they handle it
  // in a sensible way.
  EXPECT_EQ(0, restart_crostini_callback_count_);
  EXPECT_EQ(1, remove_crostini_callback_count_);
}

TEST_F(CrostiniManagerRestartTest, UninstallThenRestart) {
  // Install crostini first so that the uninstaller doesn't terminate before we
  // can call the installer again
  restart_id_ = crostini_manager()->RestartCrostini(
      kVmName, kContainerName,
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()));

  EXPECT_TRUE(crostini_manager()->IsRestartPending(restart_id_));

  run_loop()->Run();

  base::RunLoop run_loop2;
  crostini_manager()->RemoveCrostini(
      kVmName,
      base::BindOnce(&CrostiniManagerRestartTest::RemoveCrostiniCallback,
                     base::Unretained(this), run_loop2.QuitClosure()));

  restart_id_ = crostini_manager()->RestartCrostini(
      kVmName, kContainerName,
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), base::DoNothing::Once()));

  EXPECT_EQ(uninitialized_id_, restart_id_);

  run_loop2.Run();

  EXPECT_EQ(2, restart_crostini_callback_count_);
  EXPECT_EQ(1, remove_crostini_callback_count_);
}

TEST_F(CrostiniManagerRestartTest, VmStoppedDuringRestart) {
  fake_cicerone_client_->set_send_container_started_signal(false);
  restart_id_ = crostini_manager()->RestartCrostini(
      kVmName, kContainerName,
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  run_loop()->RunUntilIdle();
  EXPECT_TRUE(crostini_manager()->IsRestartPending(restart_id_));
  EXPECT_EQ(0, restart_crostini_callback_count_);
  vm_tools::concierge::VmStoppedSignal vm_stopped_signal;
  vm_stopped_signal.set_owner_id(CryptohomeIdForProfile(profile()));
  vm_stopped_signal.set_name(kVmName);
  crostini_manager()->OnVmStopped(vm_stopped_signal);
  EXPECT_FALSE(crostini_manager()->IsRestartPending(restart_id_));
  EXPECT_EQ(1, restart_crostini_callback_count_);
}

class CrostiniManagerEnterpriseReportingTest
    : public CrostiniManagerRestartTest {
 public:
  void SetUp() override {
    CrostiniManagerRestartTest::SetUp();

    // Ensure that Crostini restart is successful:
    disk_mount_manager_mock_ = new chromeos::disks::MockDiskMountManager;
    chromeos::disks::DiskMountManager::InitializeForTesting(
        disk_mount_manager_mock_);
    EXPECT_CALL(*disk_mount_manager_mock_, MountPath)
        .WillOnce(
            Invoke(this, &CrostiniManagerEnterpriseReportingTest::SshfsMount));

    // Enable Crostini reporting:
    profile()->GetPrefs()->SetBoolean(prefs::kReportCrostiniUsageEnabled, true);
  }

  void TearDown() override {
    chromeos::disks::DiskMountManager::Shutdown();
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

  restart_id_ = crostini_manager()->RestartCrostini(
      kCrostiniDefaultVmName, kCrostiniDefaultContainerName,
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  run_loop()->Run();

  EXPECT_TRUE(fake_concierge_client_->create_disk_image_called());
  EXPECT_TRUE(fake_concierge_client_->start_termina_vm_called());
  EXPECT_TRUE(
      fake_concierge_client_->get_vm_enterprise_reporting_info_called());
  EXPECT_EQ(1, restart_crostini_callback_count_);
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

  restart_id_ = crostini_manager()->RestartCrostini(
      kCrostiniDefaultVmName, kCrostiniDefaultContainerName,
      base::BindOnce(&CrostiniManagerRestartTest::RestartCrostiniCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  run_loop()->Run();

  EXPECT_TRUE(fake_concierge_client_->create_disk_image_called());
  EXPECT_TRUE(fake_concierge_client_->start_termina_vm_called());
  EXPECT_TRUE(
      fake_concierge_client_->get_vm_enterprise_reporting_info_called());
  EXPECT_EQ(1, restart_crostini_callback_count_);
  // In case of an error, the pref should be (re)set to the empty string:
  EXPECT_TRUE(
      profile()
          ->GetPrefs()
          ->GetString(crostini::prefs::kCrostiniLastLaunchTerminaKernelVersion)
          .empty());
}

TEST_F(CrostiniManagerTest, ExportContainerSuccess) {
  crostini_manager()->ExportLxdContainer(
      kVmName, kContainerName, base::FilePath("export_path"),
      base::BindOnce(&ExpectCrostiniExportResult, run_loop()->QuitClosure(),
                     CrostiniResult::SUCCESS, 123, 456));

  // Send signals, PACK, DOWNLOAD, DONE.
  vm_tools::cicerone::ExportLxdContainerProgressSignal signal;
  signal.set_owner_id(CryptohomeIdForProfile(profile()));
  signal.set_vm_name(kVmName);
  signal.set_container_name(kContainerName);
  signal.set_status(vm_tools::cicerone::
                        ExportLxdContainerProgressSignal_Status_EXPORTING_PACK);
  fake_cicerone_client_->NotifyExportLxdContainerProgress(signal);

  signal.set_status(
      vm_tools::cicerone::
          ExportLxdContainerProgressSignal_Status_EXPORTING_DOWNLOAD);
  fake_cicerone_client_->NotifyExportLxdContainerProgress(signal);

  signal.set_status(
      vm_tools::cicerone::ExportLxdContainerProgressSignal_Status_DONE);
  signal.set_input_bytes_streamed(123);
  signal.set_bytes_exported(456);
  fake_cicerone_client_->NotifyExportLxdContainerProgress(signal);

  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, ExportContainerFailInProgress) {
  // 1st call succeeds.
  crostini_manager()->ExportLxdContainer(
      kVmName, kContainerName, base::FilePath("export_path"),
      base::BindOnce(&ExpectCrostiniExportResult, run_loop()->QuitClosure(),
                     CrostiniResult::SUCCESS, 123, 456));

  // 2nd call fails since 1st call is in progress.
  crostini_manager()->ExportLxdContainer(
      kVmName, kContainerName, base::FilePath("export_path"),
      base::BindOnce(&ExpectCrostiniExportResult, base::DoNothing::Once(),
                     CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED, 0, 0));

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

  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, ExportContainerFailFromSignal) {
  crostini_manager()->ExportLxdContainer(
      kVmName, kContainerName, base::FilePath("export_path"),
      base::BindOnce(&ExpectCrostiniExportResult, run_loop()->QuitClosure(),
                     CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED, 123, 456));

  // Send signal with FAILED.
  vm_tools::cicerone::ExportLxdContainerProgressSignal signal;
  signal.set_owner_id(CryptohomeIdForProfile(profile()));
  signal.set_vm_name(kVmName);
  signal.set_container_name(kContainerName);
  signal.set_status(
      vm_tools::cicerone::ExportLxdContainerProgressSignal_Status_FAILED);
  signal.set_input_bytes_streamed(123);
  signal.set_bytes_exported(456);
  fake_cicerone_client_->NotifyExportLxdContainerProgress(signal);

  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, ExportContainerFailOnVmStop) {
  crostini_manager()->AddRunningVmForTesting(kVmName);
  crostini_manager()->ExportLxdContainer(
      kVmName, kContainerName, base::FilePath("export_path"),
      base::BindOnce(&ExpectCrostiniExportResult, run_loop()->QuitClosure(),
                     CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED_VM_STOPPED,
                     0, 0));
  crostini_manager()->StopVm(kVmName, base::DoNothing());
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, ImportContainerSuccess) {
  crostini_manager()->ImportLxdContainer(
      kVmName, kContainerName, base::FilePath("import_path"),
      base::BindOnce(&ExpectCrostiniResult, run_loop()->QuitClosure(),
                     CrostiniResult::SUCCESS));

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

  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, ImportContainerFailInProgress) {
  // 1st call succeeds.
  crostini_manager()->ImportLxdContainer(
      kVmName, kContainerName, base::FilePath("import_path"),
      base::BindOnce(&ExpectCrostiniResult, run_loop()->QuitClosure(),
                     CrostiniResult::SUCCESS));

  // 2nd call fails since 1st call is in progress.
  crostini_manager()->ImportLxdContainer(
      kVmName, kContainerName, base::FilePath("import_path"),
      base::BindOnce(ExpectCrostiniResult, base::DoNothing::Once(),
                     CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED));

  // Send signal to indicate 1st call is done.
  vm_tools::cicerone::ImportLxdContainerProgressSignal signal;
  signal.set_owner_id(CryptohomeIdForProfile(profile()));
  signal.set_vm_name(kVmName);
  signal.set_container_name(kContainerName);
  signal.set_status(
      vm_tools::cicerone::ImportLxdContainerProgressSignal_Status_DONE);
  fake_cicerone_client_->NotifyImportLxdContainerProgress(signal);

  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, ImportContainerFailArchitecture) {
  crostini_manager()->ImportLxdContainer(
      kVmName, kContainerName, base::FilePath("import_path"),
      base::BindOnce(
          &ExpectCrostiniResult, run_loop()->QuitClosure(),
          CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED_ARCHITECTURE));

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

  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, ImportContainerFailFromSignal) {
  crostini_manager()->ImportLxdContainer(
      kVmName, kContainerName, base::FilePath("import_path"),
      base::BindOnce(&ExpectCrostiniResult, run_loop()->QuitClosure(),
                     CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED));

  // Send signal with FAILED.
  vm_tools::cicerone::ImportLxdContainerProgressSignal signal;
  signal.set_owner_id(CryptohomeIdForProfile(profile()));
  signal.set_vm_name(kVmName);
  signal.set_container_name(kContainerName);
  signal.set_status(
      vm_tools::cicerone::ImportLxdContainerProgressSignal_Status_FAILED);
  fake_cicerone_client_->NotifyImportLxdContainerProgress(signal);

  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, ImportContainerFailOnVmStop) {
  crostini_manager()->AddRunningVmForTesting(kVmName);
  crostini_manager()->ImportLxdContainer(
      kVmName, kContainerName, base::FilePath("import_path"),
      base::BindOnce(
          &ExpectCrostiniResult, run_loop()->QuitClosure(),
          CrostiniResult::CONTAINER_EXPORT_IMPORT_FAILED_VM_STOPPED));
  crostini_manager()->StopVm(kVmName, base::DoNothing());
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, InstallLinuxPackageFromAptSignalNotConnectedError) {
  fake_cicerone_client_->set_install_linux_package_progress_signal_connected(
      false);
  crostini_manager()->InstallLinuxPackageFromApt(
      kVmName, kContainerName, kPackageID,
      base::BindOnce(&ExpectCrostiniResult, run_loop()->QuitClosure(),
                     CrostiniResult::INSTALL_LINUX_PACKAGE_FAILED));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, InstallLinuxPackageFromAptSignalSuccess) {
  vm_tools::cicerone::InstallLinuxPackageResponse response;
  response.set_status(vm_tools::cicerone::InstallLinuxPackageResponse::STARTED);
  fake_cicerone_client_->set_install_linux_package_response(response);
  crostini_manager()->InstallLinuxPackageFromApt(
      kVmName, kContainerName, kPackageID,
      base::BindOnce(&ExpectCrostiniResult, run_loop()->QuitClosure(),
                     CrostiniResult::SUCCESS));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, InstallLinuxPackageFromAptSignalFailure) {
  vm_tools::cicerone::InstallLinuxPackageResponse response;
  response.set_status(vm_tools::cicerone::InstallLinuxPackageResponse::FAILED);
  response.set_failure_reason(
      "Unit tests can't install Linux package from apt!");
  fake_cicerone_client_->set_install_linux_package_response(response);
  crostini_manager()->InstallLinuxPackageFromApt(
      kVmName, kContainerName, kPackageID,
      base::BindOnce(&ExpectCrostiniResult, run_loop()->QuitClosure(),
                     CrostiniResult::INSTALL_LINUX_PACKAGE_FAILED));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, InstallLinuxPackageFromAptSignalOperationBlocked) {
  vm_tools::cicerone::InstallLinuxPackageResponse response;
  response.set_status(
      vm_tools::cicerone::InstallLinuxPackageResponse::INSTALL_ALREADY_ACTIVE);
  fake_cicerone_client_->set_install_linux_package_response(response);
  crostini_manager()->InstallLinuxPackageFromApt(
      kVmName, kContainerName, kPackageID,
      base::BindOnce(&ExpectCrostiniResult, run_loop()->QuitClosure(),
                     CrostiniResult::BLOCKING_OPERATION_ALREADY_ACTIVE));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, InstallerStatusInitiallyFalse) {
  EXPECT_FALSE(crostini_manager()->GetInstallerViewStatus());
}

TEST_F(CrostiniManagerTest, StartContainerSuccess) {
  crostini_manager()->StartLxdContainer(
      kVmName, kContainerName,
      base::BindOnce(&ExpectCrostiniResult, run_loop()->QuitClosure(),
                     CrostiniResult::SUCCESS));

  run_loop()->Run();
}

class CrostiniManagerAnsibleInfraTest : public CrostiniManagerTest {
 public:
  void SetUp() override {
    CrostiniManagerTest::SetUp();
    ansible_management_test_helper_ =
        std::make_unique<AnsibleManagementTestHelper>(profile_.get());
    ansible_management_test_helper_->SetUpAnsibleInfra();

    SetUpViewsEnvironmentForTesting();
  }

  void TearDown() override {
    crostini::CloseCrostiniAnsibleSoftwareConfigViewForTesting();
    // Wait for view triggered to be closed.
    base::RunLoop().RunUntilIdle();

    TearDownViewsEnvironmentForTesting();

    ansible_management_test_helper_.reset();
    CrostiniManagerTest::TearDown();
  }

 protected:
  std::unique_ptr<AnsibleManagementTestHelper> ansible_management_test_helper_;
};

TEST_F(CrostiniManagerAnsibleInfraTest, StartContainerAnsibleInstallFailure) {
  ansible_management_test_helper_->SetUpAnsibleInstallation(
      vm_tools::cicerone::InstallLinuxPackageResponse::FAILED);

  crostini_manager()->StartLxdContainer(
      kCrostiniDefaultVmName, kCrostiniDefaultContainerName,
      base::BindOnce(&ExpectCrostiniResult, run_loop()->QuitClosure(),
                     CrostiniResult::UNKNOWN_ERROR));

  run_loop()->Run();
}

TEST_F(CrostiniManagerAnsibleInfraTest, StartContainerApplyFailure) {
  ansible_management_test_helper_->SetUpAnsibleInstallation(
      vm_tools::cicerone::InstallLinuxPackageResponse::STARTED);
  ansible_management_test_helper_->SetUpPlaybookApplication(
      vm_tools::cicerone::ApplyAnsiblePlaybookResponse::FAILED);

  crostini_manager()->StartLxdContainer(
      kCrostiniDefaultVmName, kCrostiniDefaultContainerName,
      base::BindOnce(&ExpectCrostiniResult, run_loop()->QuitClosure(),
                     CrostiniResult::UNKNOWN_ERROR));
  base::RunLoop().RunUntilIdle();

  ansible_management_test_helper_->SendSucceededInstallSignal();

  run_loop()->Run();
}

TEST_F(CrostiniManagerAnsibleInfraTest, StartContainerSuccess) {
  ansible_management_test_helper_->SetUpAnsibleInstallation(
      vm_tools::cicerone::InstallLinuxPackageResponse::STARTED);
  ansible_management_test_helper_->SetUpPlaybookApplication(
      vm_tools::cicerone::ApplyAnsiblePlaybookResponse::STARTED);

  crostini_manager()->StartLxdContainer(
      kCrostiniDefaultVmName, kCrostiniDefaultContainerName,
      base::BindOnce(&ExpectCrostiniResult, run_loop()->QuitClosure(),
                     CrostiniResult::SUCCESS));
  base::RunLoop().RunUntilIdle();

  ansible_management_test_helper_->SendSucceededInstallSignal();
  base::RunLoop().RunUntilIdle();

  ansible_management_test_helper_->SendSucceededApplySignal();

  run_loop()->Run();
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
    progress_run_loop_.reset();
    CrostiniManagerTest::TearDown();
  }

  void RunUntilUpgradeDone(UpgradeContainerProgressStatus final_status) {
    final_status_ = final_status;
    progress_run_loop_->Run();
  }

  void SendProgressSignal() {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &chromeos::FakeCiceroneClient::NotifyUpgradeContainerProgress,
            base::Unretained(fake_cicerone_client_), progress_signal_));
  }

 protected:
  // UpgradeContainerProgressObserver
  void OnUpgradeContainerProgress(
      const ContainerId& container_id,
      UpgradeContainerProgressStatus status,
      const std::vector<std::string>& messages) override {
    if (status == final_status_) {
      progress_run_loop_->Quit();
    }
  }

  ContainerId container_id_ = ContainerId(kVmName, kContainerName);

  UpgradeContainerProgressStatus final_status_ =
      UpgradeContainerProgressStatus::FAILED;

  vm_tools::cicerone::UpgradeContainerProgressSignal progress_signal_;
  // must be created on UI thread
  std::unique_ptr<base::RunLoop> progress_run_loop_;
};

TEST_F(CrostiniManagerUpgradeContainerTest, UpgradeContainerSuccess) {
  crostini_manager()->UpgradeContainer(
      container_id_, ContainerVersion::STRETCH, ContainerVersion::BUSTER,
      base::BindOnce(&ExpectCrostiniResult, run_loop()->QuitClosure(),
                     CrostiniResult::SUCCESS));

  run_loop()->Run();

  progress_signal_.set_status(
      vm_tools::cicerone::UpgradeContainerProgressSignal::SUCCEEDED);

  SendProgressSignal();
  RunUntilUpgradeDone(UpgradeContainerProgressStatus::SUCCEEDED);
}

TEST_F(CrostiniManagerUpgradeContainerTest, CancelUpgradeContainerSuccess) {
  crostini_manager()->UpgradeContainer(
      container_id_, ContainerVersion::STRETCH, ContainerVersion::BUSTER,
      base::BindOnce(&ExpectCrostiniResult, run_loop()->QuitClosure(),
                     CrostiniResult::SUCCESS));

  progress_signal_.set_status(
      vm_tools::cicerone::UpgradeContainerProgressSignal::IN_PROGRESS);

  SendProgressSignal();
  run_loop()->Run();

  base::RunLoop run_loop2;
  crostini_manager()->CancelUpgradeContainer(
      container_id_,
      base::BindOnce(&ExpectCrostiniResult, run_loop2.QuitClosure(),
                     CrostiniResult::SUCCESS));
  run_loop2.Run();
}

}  // namespace crostini
