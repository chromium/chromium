// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_installer.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/chromeos/crostini/ansible/ansible_management_test_helper.h"
#include "chrome/browser/chromeos/crostini/crostini_installer_ui_delegate.h"
#include "chrome/browser/chromeos/crostini/crostini_test_helper.h"
#include "chrome/browser/chromeos/crostini/crostini_types.mojom.h"
#include "chrome/browser/component_updater/fake_cros_component_manager.h"
#include "chrome/test/base/browser_process_platform_part_test_api_chromeos.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/concierge/concierge_service.pb.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/dlcservice/dlcservice_client.h"
#include "chromeos/dbus/fake_concierge_client.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "chromeos/disks/mock_disk_mount_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using crostini::mojom::InstallerError;
using crostini::mojom::InstallerState;
using testing::_;
using testing::AnyNumber;
using testing::Expectation;
using testing::ExpectationSet;
using testing::Invoke;
using testing::Le;
using testing::MockFunction;
using testing::SaveArg;
using testing::Truly;

namespace crostini {

class CrostiniInstallerTest : public testing::Test {
 public:
  using ResultCallback = CrostiniInstallerUIDelegate::ResultCallback;
  using ProgressCallback = CrostiniInstallerUIDelegate::ProgressCallback;

  class MockCallbacks {
   public:
    MOCK_METHOD2(OnProgress,
                 void(InstallerState state, double progress_fraction));
    MOCK_METHOD1(OnFinished, void(InstallerError error));
    MOCK_METHOD0(OnCanceled, void());
  };

  class MountPathWaiter {
   public:
    using MountPointInfo = chromeos::disks::DiskMountManager::MountPointInfo;

    void MountPath(const std::string& source_path,
                   const std::string& source_format,
                   const std::string& mount_label,
                   const std::vector<std::string>& mount_options,
                   chromeos::MountType type,
                   chromeos::MountAccessMode access_mode) {
      mount_point_info_ = std::make_unique<MountPointInfo>(
          source_path, "/media/fuse/" + mount_label,
          chromeos::MountType::MOUNT_TYPE_NETWORK_STORAGE,
          chromeos::disks::MountCondition::MOUNT_CONDITION_NONE);
      if (quit_closure_) {
        std::move(quit_closure_).Run();
      }
    }

    MountPointInfo* get_mount_point_info() { return mount_point_info_.get(); }

    void WaitForMountPathCalled() {
      base::RunLoop loop;
      quit_closure_ = loop.QuitClosure();
      loop.Run();
    }

   private:
    base::OnceClosure quit_closure_;
    std::unique_ptr<MountPointInfo> mount_point_info_;
  };

  class WaitingFakeConciergeClient : public chromeos::FakeConciergeClient {
   public:
    void StartTerminaVm(
        const vm_tools::concierge::StartVmRequest& request,
        chromeos::DBusMethodCallback<vm_tools::concierge::StartVmResponse>
            callback) override {
      chromeos::FakeConciergeClient::StartTerminaVm(request,
                                                    std::move(callback));
      if (quit_closure_) {
        base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                      std::move(quit_closure_));
      }
    }

    void WaitForStartTerminaVmCalled() {
      base::RunLoop loop;
      quit_closure_ = loop.QuitClosure();
      loop.Run();
      EXPECT_TRUE(start_termina_vm_called());
    }

   private:
    base::OnceClosure quit_closure_;
  };

  CrostiniInstallerTest()
      : local_state_(std::make_unique<ScopedTestingLocalState>(
            TestingBrowserProcess::GetGlobal())),
        browser_part_(g_browser_process->platform_part()) {}

  void SetUp() override {
    component_manager_ =
        base::MakeRefCounted<component_updater::FakeCrOSComponentManager>();
    component_manager_->set_supported_components({"cros-termina"});
    component_manager_->ResetComponentState(
        "cros-termina",
        component_updater::FakeCrOSComponentManager::ComponentInfo(
            component_updater::CrOSComponentManager::Error::NONE,
            base::FilePath("/install/path"), base::FilePath("/mount/path")));
    browser_part_.InitializeCrosComponentManager(component_manager_);

    chromeos::DlcserviceClient::InitializeFake();
    waiting_fake_concierge_client_ = new WaitingFakeConciergeClient;
    chromeos::DBusThreadManager::GetSetterForTesting()->SetConciergeClient(
        base::WrapUnique(waiting_fake_concierge_client_));

    disk_mount_manager_mock_ = new chromeos::disks::MockDiskMountManager;
    chromeos::disks::DiskMountManager::InitializeForTesting(
        disk_mount_manager_mock_);

    profile_ = std::make_unique<TestingProfile>();
    // Needed at least for passing IsCrostiniUIAllowedForProfile() test in
    // CrostiniManager.
    crostini_test_helper_ =
        std::make_unique<CrostiniTestHelper>(profile_.get());

    crostini_installer_ = std::make_unique<CrostiniInstaller>(profile_.get());
    crostini_installer_->set_skip_launching_terminal_for_testing();

    g_browser_process->platform_part()
        ->InitializeSchedulerConfigurationManager();
  }

  void TearDown() override {
    g_browser_process->platform_part()->ShutdownSchedulerConfigurationManager();
    crostini_installer_->Shutdown();
    crostini_installer_.reset();
    crostini_test_helper_.reset();
    profile_.reset();

    chromeos::disks::MockDiskMountManager::Shutdown();
    chromeos::DBusThreadManager::Shutdown();
    chromeos::DlcserviceClient::Shutdown();

    browser_part_.ShutdownCrosComponentManager();
    component_manager_.reset();
  }

  void Install() {
    CrostiniManager::GetForProfile(profile_.get())
        ->SetCrostiniDialogStatus(DialogType::INSTALLER, true);
    crostini_installer_->Install(
        CrostiniManager::RestartOptions{},
        base::BindRepeating(&MockCallbacks::OnProgress,
                            base::Unretained(&mock_callbacks_)),
        base::BindOnce(&MockCallbacks::OnFinished,
                       base::Unretained(&mock_callbacks_)));
  }

  void Cancel() {
    crostini_installer_->Cancel(base::BindOnce(
        &MockCallbacks::OnCanceled, base::Unretained(&mock_callbacks_)));
  }

 protected:
  MountPathWaiter mount_path_waiter_;
  MockCallbacks mock_callbacks_;
  content::BrowserTaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;

  // Owned by DiskMountManager
  chromeos::disks::MockDiskMountManager* disk_mount_manager_mock_ = nullptr;
  // Owned by chromeos::DBusThreadManager
  WaitingFakeConciergeClient* waiting_fake_concierge_client_ = nullptr;

  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<CrostiniTestHelper> crostini_test_helper_;
  std::unique_ptr<CrostiniInstaller> crostini_installer_;

 private:
  std::unique_ptr<ScopedTestingLocalState> local_state_;
  scoped_refptr<component_updater::FakeCrOSComponentManager> component_manager_;
  BrowserProcessPlatformPartTestApi browser_part_;

  DISALLOW_COPY_AND_ASSIGN(CrostiniInstallerTest);
};

TEST_F(CrostiniInstallerTest, InstallFlow) {
  double last_progress = 0.0;
  auto greater_equal_last_progress = Truly(
      [&last_progress](double progress) { return progress >= last_progress; });

  ExpectationSet expectation_set;
  expectation_set +=
      EXPECT_CALL(mock_callbacks_,
                  OnProgress(_, AllOf(greater_equal_last_progress, Le(1.0))))
          .WillRepeatedly(SaveArg<1>(&last_progress));
  expectation_set +=
      EXPECT_CALL(*disk_mount_manager_mock_,
                  MountPath("sshfs://test@hostname:", _, _, _, _, _))
          .WillOnce(Invoke(&mount_path_waiter_, &MountPathWaiter::MountPath));
  // |OnProgress()| should not happens after |OnFinished()|
  EXPECT_CALL(mock_callbacks_, OnFinished(InstallerError::kNone))
      .After(expectation_set);

  Install();
  mount_path_waiter_.WaitForMountPathCalled();

  ASSERT_TRUE(mount_path_waiter_.get_mount_point_info());
  disk_mount_manager_mock_->NotifyMountEvent(
      chromeos::disks::DiskMountManager::MountEvent::MOUNTING,
      chromeos::MountError::MOUNT_ERROR_NONE,
      *mount_path_waiter_.get_mount_point_info());

  task_environment_.RunUntilIdle();
  histogram_tester_.ExpectUniqueSample(
      "Crostini.SetupResult",
      static_cast<base::HistogramBase::Sample>(
          CrostiniInstaller::SetupResult::kSuccess),
      1);
  histogram_tester_.ExpectTotalCount("Crostini.Setup.Started", 1);
  histogram_tester_.ExpectTotalCount("Crostini.Restarter.Started", 0);

  EXPECT_TRUE(crostini_installer_->CanInstall())
      << "Installer should recover to installable state";
}

TEST_F(CrostiniInstallerTest, InstallFlowWithAnsibleInfra) {
  AnsibleManagementTestHelper test_helper(profile_.get());
  test_helper.SetUpAnsibleInfra();

  double last_progress = 0.0;
  auto greater_equal_last_progress = Truly(
      [&last_progress](double progress) { return progress >= last_progress; });

  ExpectationSet expectation_set;
  expectation_set +=
      EXPECT_CALL(mock_callbacks_,
                  OnProgress(_, AllOf(greater_equal_last_progress, Le(1.0))))
          .WillRepeatedly(SaveArg<1>(&last_progress));
  expectation_set +=
      EXPECT_CALL(*disk_mount_manager_mock_,
                  MountPath("sshfs://test@hostname:", _, _, _, _, _))
          .WillOnce(Invoke(&mount_path_waiter_, &MountPathWaiter::MountPath));
  // |OnProgress()| should not happens after |OnFinished()|
  EXPECT_CALL(mock_callbacks_, OnFinished(InstallerError::kNone))
      .After(expectation_set);

  Install();

  task_environment_.RunUntilIdle();
  test_helper.SendSucceededApplySignal();

  mount_path_waiter_.WaitForMountPathCalled();

  ASSERT_TRUE(mount_path_waiter_.get_mount_point_info());
  disk_mount_manager_mock_->NotifyMountEvent(
      chromeos::disks::DiskMountManager::MountEvent::MOUNTING,
      chromeos::MountError::MOUNT_ERROR_NONE,
      *mount_path_waiter_.get_mount_point_info());

  task_environment_.RunUntilIdle();
  histogram_tester_.ExpectUniqueSample(
      "Crostini.SetupResult",
      static_cast<base::HistogramBase::Sample>(
          CrostiniInstaller::SetupResult::kSuccess),
      1);

  EXPECT_TRUE(crostini_installer_->CanInstall())
      << "Installer should recover to installable state";
}

TEST_F(CrostiniInstallerTest, CancelBeforeStart) {
  crostini_installer_->CancelBeforeStart();

  histogram_tester_.ExpectUniqueSample(
      "Crostini.SetupResult",
      static_cast<base::HistogramBase::Sample>(
          CrostiniInstaller::SetupResult::kNotStarted),
      1);
}

TEST_F(CrostiniInstallerTest, CancelAfterStart) {
  MockFunction<void(std::string check_point_name)> check;

  Expectation expect_progresses =
      EXPECT_CALL(mock_callbacks_, OnProgress(_, _)).Times(AnyNumber());
  // |OnProgress()| should not happen after |Cancel()| is called.
  Expectation expect_calling_cancel =
      EXPECT_CALL(check, Call("calling Cancel()")).After(expect_progresses);
  EXPECT_CALL(mock_callbacks_, OnCanceled()).After(expect_calling_cancel);

  Install();

  // It is too involved to also fake the cleanup process in CrostiniManager, so
  // we will just skip it.
  crostini_installer_->set_require_cleanup_for_testing(false);

  // This will stop just before mount disk finishes because we don't fake the
  // mount event.
  task_environment_.RunUntilIdle();

  check.Call("calling Cancel()");
  Cancel();
  task_environment_.RunUntilIdle();

  histogram_tester_.ExpectUniqueSample(
      "Crostini.SetupResult",
      static_cast<base::HistogramBase::Sample>(
          CrostiniInstaller::SetupResult::kUserCancelledMountContainer),
      1);
  EXPECT_TRUE(crostini_installer_->CanInstall())
      << "Installer should recover to installable state";
}

// Cancel before |OnAvailableDiskSpace()| happens is a special case, in which we
// haven't started the restarting process yet.
TEST_F(CrostiniInstallerTest, CancelAfterStartBeforeCheckDisk) {
  EXPECT_CALL(mock_callbacks_, OnCanceled());

  crostini_installer_->Install(CrostiniManager::RestartOptions{},
                               base::DoNothing(), base::DoNothing());
  Cancel();  // Cancel immediately

  task_environment_.RunUntilIdle();

  histogram_tester_.ExpectUniqueSample(
      "Crostini.SetupResult",
      static_cast<base::HistogramBase::Sample>(
          CrostiniInstaller::SetupResult::kNotStarted),
      1);
  EXPECT_TRUE(crostini_installer_->CanInstall())
      << "Installer should recover to installable state";
}

TEST_F(CrostiniInstallerTest, InstallerError) {
  Expectation expect_progresses =
      EXPECT_CALL(mock_callbacks_, OnProgress(_, _)).Times(AnyNumber());
  // |OnProgress()| should not happens after |OnFinished()|
  EXPECT_CALL(mock_callbacks_,
              OnFinished(InstallerError::kErrorStartingTermina))
      .After(expect_progresses);

  // Fake a failure for starting vm.
  vm_tools::concierge::StartVmResponse response;
  response.set_status(vm_tools::concierge::VM_STATUS_FAILURE);
  waiting_fake_concierge_client_->set_start_vm_response(std::move(response));

  Install();
  waiting_fake_concierge_client_->WaitForStartTerminaVmCalled();

  histogram_tester_.ExpectUniqueSample(
      "Crostini.SetupResult",
      static_cast<base::HistogramBase::Sample>(
          CrostiniInstaller::SetupResult::kErrorStartingTermina),
      1);
  histogram_tester_.ExpectTotalCount("Crostini.Setup.Started", 1);
  histogram_tester_.ExpectTotalCount("Crostini.Restarter.Started", 0);

  EXPECT_TRUE(crostini_installer_->CanInstall())
      << "Installer should recover to installable state";
}

TEST_F(CrostiniInstallerTest, InstallerErrorWhileConfiguring) {
  AnsibleManagementTestHelper test_helper(profile_.get());
  test_helper.SetUpAnsibleInfra();
  test_helper.SetUpAnsibleInstallation(
      vm_tools::cicerone::InstallLinuxPackageResponse::FAILED);

  Expectation expect_progresses =
      EXPECT_CALL(mock_callbacks_, OnProgress(_, _)).Times(AnyNumber());
  // |OnProgress()| should not happens after |OnFinished()|
  EXPECT_CALL(mock_callbacks_,
              OnFinished(InstallerError::kErrorConfiguringContainer))
      .After(expect_progresses);

  Install();

  task_environment_.RunUntilIdle();
  histogram_tester_.ExpectUniqueSample(
      "Crostini.SetupResult",
      static_cast<base::HistogramBase::Sample>(
          CrostiniInstaller::SetupResult::kErrorConfiguringContainer),
      1);

  EXPECT_TRUE(crostini_installer_->CanInstall())
      << "Installer should recover to installable state";
}

}  // namespace crostini
