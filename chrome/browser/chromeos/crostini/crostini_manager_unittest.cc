// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include <memory>
#include "base/base64.h"
#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_cicerone_client.h"
#include "chromeos/dbus/fake_concierge_client.h"
#include "chromeos/disks/mock_disk_mount_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "storage/browser/fileapi/external_mount_points.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crostini {

namespace {
const char kVmName[] = "vm_name";
const char kContainerName[] = "container_name";
}  // namespace

class CrostiniManagerTest : public testing::Test {
 public:
  void CreateDiskImageClientErrorCallback(base::OnceClosure closure,
                                          CrostiniResult result,
                                          const base::FilePath& file_path) {
    EXPECT_FALSE(fake_concierge_client_->create_disk_image_called());
    EXPECT_EQ(result, CrostiniResult::CLIENT_ERROR);
    std::move(closure).Run();
  }

  void DestroyDiskImageClientErrorCallback(base::OnceClosure closure,
                                           CrostiniResult result) {
    EXPECT_FALSE(fake_concierge_client_->destroy_disk_image_called());
    EXPECT_EQ(result, CrostiniResult::CLIENT_ERROR);
    std::move(closure).Run();
  }

  void ListVmDisksClientErrorCallback(base::OnceClosure closure,
                                      CrostiniResult result,
                                      int64_t total_size) {
    EXPECT_FALSE(fake_concierge_client_->list_vm_disks_called());
    EXPECT_EQ(result, CrostiniResult::CLIENT_ERROR);
    std::move(closure).Run();
  }

  void StartTerminaVmClientErrorCallback(base::OnceClosure closure,
                                         CrostiniResult result) {
    EXPECT_FALSE(fake_concierge_client_->start_termina_vm_called());
    EXPECT_EQ(result, CrostiniResult::CLIENT_ERROR);
    std::move(closure).Run();
  }

  void StopVmClientErrorCallback(base::OnceClosure closure,
                                 CrostiniResult result) {
    EXPECT_FALSE(fake_concierge_client_->stop_vm_called());
    EXPECT_EQ(result, CrostiniResult::CLIENT_ERROR);
    std::move(closure).Run();
  }

  void CreateDiskImageSuccessCallback(base::OnceClosure closure,
                                      CrostiniResult result,
                                      const base::FilePath& file_path) {
    EXPECT_TRUE(fake_concierge_client_->create_disk_image_called());
    std::move(closure).Run();
  }

  void DestroyDiskImageSuccessCallback(base::OnceClosure closure,
                                       CrostiniResult result) {
    EXPECT_TRUE(fake_concierge_client_->destroy_disk_image_called());
    std::move(closure).Run();
  }

  void ListVmDisksSuccessCallback(base::OnceClosure closure,
                                  CrostiniResult result,
                                  int64_t total_size) {
    EXPECT_TRUE(fake_concierge_client_->list_vm_disks_called());
    std::move(closure).Run();
  }

  void StartTerminaVmSuccessCallback(base::OnceClosure closure,
                                     CrostiniResult result) {
    EXPECT_TRUE(fake_concierge_client_->start_termina_vm_called());
    std::move(closure).Run();
  }

  void OnStartTremplinRecordsRunningVmCallback(base::OnceClosure closure,
                                               CrostiniResult result) {
    // Check that running_vms_ contains the running vm.
    EXPECT_TRUE(crostini_manager()->IsVmRunning(kVmName));
    std::move(closure).Run();
  }

  void StopVmSuccessCallback(base::OnceClosure closure, CrostiniResult result) {
    EXPECT_TRUE(fake_concierge_client_->stop_vm_called());
    std::move(closure).Run();
  }

  void CreateContainerFailsCallback(base::OnceClosure closure,
                                    CrostiniResult result) {
    create_container_fails_callback_called_ = true;
    EXPECT_EQ(result, CrostiniResult::UNKNOWN_ERROR);
    std::move(closure).Run();
  }

  void InstallLinuxPackageCallback(base::OnceClosure closure,
                                   CrostiniResult expected_result,
                                   const std::string& expected_failure_reason,
                                   CrostiniResult result,
                                   const std::string& failure_reason) {
    EXPECT_EQ(expected_result, result);
    EXPECT_EQ(expected_failure_reason, failure_reason);
    std::move(closure).Run();
  }

  CrostiniManagerTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI),
        test_browser_thread_bundle_(
            content::TestBrowserThreadBundle::REAL_IO_THREAD) {
    chromeos::DBusThreadManager::Initialize();
    fake_cicerone_client_ = static_cast<chromeos::FakeCiceroneClient*>(
        chromeos::DBusThreadManager::Get()->GetCiceroneClient());
    fake_concierge_client_ = static_cast<chromeos::FakeConciergeClient*>(
        chromeos::DBusThreadManager::Get()->GetConciergeClient());
  }

  ~CrostiniManagerTest() override { chromeos::DBusThreadManager::Shutdown(); }

  void SetUp() override {
    run_loop_ = std::make_unique<base::RunLoop>();
    profile_ = std::make_unique<TestingProfile>();
    crostini_manager_ = std::make_unique<CrostiniManager>(profile_.get());
  }

  void TearDown() override {
    crostini_manager_.reset();
    profile_.reset();
    run_loop_.reset();
  }

 protected:
  base::RunLoop* run_loop() { return run_loop_.get(); }
  Profile* profile() { return profile_.get(); }
  CrostiniManager* crostini_manager() { return crostini_manager_.get(); }

  // Owned by chromeos::DBusThreadManager
  chromeos::FakeCiceroneClient* fake_cicerone_client_;
  chromeos::FakeConciergeClient* fake_concierge_client_;

  std::unique_ptr<base::RunLoop>
      run_loop_;  // run_loop_ must be created on the UI thread.
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<CrostiniManager> crostini_manager_;
  bool create_container_fails_callback_called_ = false;

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  content::TestBrowserThreadBundle test_browser_thread_bundle_;
  DISALLOW_COPY_AND_ASSIGN(CrostiniManagerTest);
};

TEST_F(CrostiniManagerTest, CreateDiskImageNameError) {
  const base::FilePath& disk_path = base::FilePath("");

  crostini_manager()->CreateDiskImage(
      disk_path, vm_tools::concierge::STORAGE_CRYPTOHOME_ROOT,
      base::BindOnce(&CrostiniManagerTest::CreateDiskImageClientErrorCallback,
                     base::Unretained(this), run_loop()->QuitClosure()));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, CreateDiskImageStorageLocationError) {
  const base::FilePath& disk_path = base::FilePath(kVmName);

  crostini_manager()->CreateDiskImage(
      disk_path,
      vm_tools::concierge::StorageLocation_INT_MIN_SENTINEL_DO_NOT_USE_,
      base::BindOnce(&CrostiniManagerTest::CreateDiskImageClientErrorCallback,
                     base::Unretained(this), run_loop()->QuitClosure()));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, CreateDiskImageSuccess) {
  const base::FilePath& disk_path = base::FilePath(kVmName);

  crostini_manager()->CreateDiskImage(
      disk_path, vm_tools::concierge::STORAGE_CRYPTOHOME_DOWNLOADS,
      base::BindOnce(&CrostiniManagerTest::CreateDiskImageSuccessCallback,
                     base::Unretained(this), run_loop()->QuitClosure()));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, DestroyDiskImageNameError) {
  const base::FilePath& disk_path = base::FilePath("");

  crostini_manager()->DestroyDiskImage(
      disk_path, vm_tools::concierge::STORAGE_CRYPTOHOME_ROOT,
      base::BindOnce(&CrostiniManagerTest::DestroyDiskImageClientErrorCallback,
                     base::Unretained(this), run_loop()->QuitClosure()));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, DestroyDiskImageStorageLocationError) {
  const base::FilePath& disk_path = base::FilePath(kVmName);

  crostini_manager()->DestroyDiskImage(
      disk_path,
      vm_tools::concierge::StorageLocation_INT_MIN_SENTINEL_DO_NOT_USE_,
      base::BindOnce(&CrostiniManagerTest::DestroyDiskImageClientErrorCallback,
                     base::Unretained(this), run_loop()->QuitClosure()));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, DestroyDiskImageSuccess) {
  const base::FilePath& disk_path = base::FilePath(kVmName);

  crostini_manager()->DestroyDiskImage(
      disk_path, vm_tools::concierge::STORAGE_CRYPTOHOME_DOWNLOADS,
      base::BindOnce(&CrostiniManagerTest::DestroyDiskImageSuccessCallback,
                     base::Unretained(this), run_loop()->QuitClosure()));
  run_loop()->Run();
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
      "", disk_path,
      base::BindOnce(&CrostiniManagerTest::StartTerminaVmClientErrorCallback,
                     base::Unretained(this), run_loop()->QuitClosure()));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, StartTerminaVmDiskPathError) {
  const base::FilePath& disk_path = base::FilePath();

  crostini_manager()->StartTerminaVm(
      kVmName, disk_path,
      base::BindOnce(&CrostiniManagerTest::StartTerminaVmClientErrorCallback,
                     base::Unretained(this), run_loop()->QuitClosure()));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, StartTerminaVmSuccess) {
  const base::FilePath& disk_path = base::FilePath(kVmName);

  crostini_manager()->StartTerminaVm(
      kVmName, disk_path,
      base::BindOnce(&CrostiniManagerTest::StartTerminaVmSuccessCallback,
                     base::Unretained(this), run_loop()->QuitClosure()));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, OnStartTremplinRecordsRunningVm) {
  const base::FilePath& disk_path = base::FilePath(kVmName);
  const std::string owner_id = CryptohomeIdForProfile(profile());

  // Start the Vm.
  crostini_manager()->StartTerminaVm(
      kVmName, disk_path,
      base::BindOnce(
          &CrostiniManagerTest::OnStartTremplinRecordsRunningVmCallback,
          base::Unretained(this), run_loop()->QuitClosure()));

  // Check that the Vm start is not recorded (without tremplin start).
  EXPECT_FALSE(crostini_manager()->IsVmRunning(kVmName));

  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, StopVmNameError) {
  crostini_manager()->StopVm(
      "", base::BindOnce(&CrostiniManagerTest::StopVmClientErrorCallback,
                         base::Unretained(this), run_loop()->QuitClosure()));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, StopVmSuccess) {
  crostini_manager()->StopVm(
      kVmName,
      base::BindOnce(&CrostiniManagerTest::StopVmSuccessCallback,
                     base::Unretained(this), run_loop()->QuitClosure()));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, InstallLinuxPackageSignalNotConnectedError) {
  fake_cicerone_client_->set_install_linux_package_progress_signal_connected(
      false);
  crostini_manager()->InstallLinuxPackage(
      kVmName, kContainerName, "/tmp/package.deb",
      base::BindOnce(&CrostiniManagerTest::InstallLinuxPackageCallback,
                     base::Unretained(this), run_loop()->QuitClosure(),
                     CrostiniResult::INSTALL_LINUX_PACKAGE_FAILED,
                     std::string()));
  run_loop()->Run();
}

TEST_F(CrostiniManagerTest, InstallLinuxPackageSignalSuccess) {
  vm_tools::cicerone::InstallLinuxPackageResponse response;
  response.set_status(vm_tools::cicerone::InstallLinuxPackageResponse::STARTED);
  fake_cicerone_client_->set_install_linux_package_response(response);
  crostini_manager()->InstallLinuxPackage(
      kVmName, kContainerName, "/tmp/package.deb",
      base::BindOnce(&CrostiniManagerTest::InstallLinuxPackageCallback,
                     base::Unretained(this), run_loop()->QuitClosure(),
                     CrostiniResult::SUCCESS, std::string()));
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
      base::BindOnce(&CrostiniManagerTest::InstallLinuxPackageCallback,
                     base::Unretained(this), run_loop()->QuitClosure(),
                     CrostiniResult::INSTALL_LINUX_PACKAGE_FAILED,
                     failure_reason));
  run_loop()->Run();
}

class CrostiniManagerRestartTest : public CrostiniManagerTest,
                                   public CrostiniManager::RestartObserver {
 public:
  void SetUp() override { CrostiniManagerTest::SetUp(); }

  void RestartCrostiniCallback(base::OnceClosure closure,
                               CrostiniResult result) {
    restart_crostini_callback_count_++;
    std::move(closure).Run();
  }

  // CrostiniManager::RestartObserver
  void OnComponentLoaded(CrostiniResult result) override {
    if (abort_on_component_loaded_) {
      Abort();
    }
  }

  void OnConciergeStarted(CrostiniResult result) override {
    if (abort_on_concierge_started_) {
      Abort();
    }
  }

  void OnDiskImageCreated(CrostiniResult result) override {
    if (abort_on_disk_image_created_) {
      Abort();
    }
  }

  void OnVmStarted(CrostiniResult result) override {
    if (abort_on_vm_started_) {
      Abort();
    }
  }

  void OnContainerDownloading(int32_t download_percent) override {}

  void OnContainerCreated(CrostiniResult result) override {
    if (abort_on_container_created_) {
      Abort();
    }
  }

  void OnContainerStarted(CrostiniResult result) override {
    if (abort_on_container_started_) {
      Abort();
    }
  }

  void OnContainerSetup(CrostiniResult result) override {
    if (abort_on_container_setup_) {
      Abort();
    }
  }

  void OnSshKeysFetched(CrostiniResult result) override {
    if (abort_on_ssh_keys_fetched_) {
      Abort();
    }
  }

 protected:
  void Abort() {
    crostini_manager()->AbortRestartCrostini(restart_id_);
    run_loop()->Quit();
  }

  void SshfsMount(const std::string& source_path,
                  const std::string& source_format,
                  const std::string& mount_label,
                  const std::vector<std::string>& mount_options,
                  chromeos::MountType type,
                  chromeos::MountAccessMode access_mode) {
    disk_mount_manager_mock_->NotifyMountEvent(
        chromeos::disks::DiskMountManager::MountEvent::MOUNTING,
        chromeos::MountError::MOUNT_ERROR_NONE,
        chromeos::disks::DiskMountManager::MountPointInfo(
            source_path, "/media/fuse/" + mount_label,
            chromeos::MountType::MOUNT_TYPE_NETWORK_STORAGE,
            chromeos::disks::MountCondition::MOUNT_CONDITION_NONE));
  }

  CrostiniManager::RestartId restart_id_ =
      CrostiniManager::kUninitializedRestartId;
  bool abort_on_component_loaded_ = false;
  bool abort_on_concierge_started_ = false;
  bool abort_on_disk_image_created_ = false;
  bool abort_on_vm_started_ = false;
  bool abort_on_container_created_ = false;
  bool abort_on_container_started_ = false;
  bool abort_on_container_setup_ = false;
  bool abort_on_ssh_keys_fetched_ = false;
  int restart_crostini_callback_count_ = 0;
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
}

TEST_F(CrostiniManagerRestartTest, AbortOnComponentLoaded) {
  abort_on_component_loaded_ = true;
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
      base::BindOnce(&CrostiniManagerTest::CreateContainerFailsCallback,
                     base::Unretained(this), run_loop()->QuitClosure()),
      this);
  run_loop()->Run();

  EXPECT_TRUE(create_container_fails_callback_called_);
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
  EXPECT_TRUE(crostini_manager()->IsContainerRunning(kVmName, kContainerName));

  // Now call StartTerminaVm again. The default response state is "STARTING",
  // so no container should be considered running.
  const base::FilePath& disk_path = base::FilePath(kVmName);

  base::RunLoop run_loop2;
  crostini_manager()->StartTerminaVm(
      kVmName, disk_path,
      base::BindOnce(&CrostiniManagerTest::StartTerminaVmSuccessCallback,
                     base::Unretained(this), run_loop2.QuitClosure()));
  run_loop2.Run();
  EXPECT_TRUE(crostini_manager()->IsVmRunning(kVmName));
  EXPECT_FALSE(crostini_manager()->IsContainerRunning(kVmName, kContainerName));
}

}  // namespace crostini
