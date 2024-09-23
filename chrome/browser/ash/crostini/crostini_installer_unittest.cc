// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_installer.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/crostini/ansible/ansible_management_test_helper.h"
#include "chrome/browser/ash/crostini/crostini_installer_ui_delegate.h"
#include "chrome/browser/ash/crostini/crostini_test_helper.h"
#include "chrome/browser/ash/crostini/crostini_types.mojom.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/test/base/browser_process_platform_part_test_api_chromeos.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/chunneld/chunneld_client.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"
#include "chromeos/ash/components/dbus/cicerone/fake_cicerone_client.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/concierge/fake_concierge_client.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"
#include "chromeos/ash/components/dbus/seneschal/seneschal_client.h"
#include "chromeos/ash/components/dbus/spaced/fake_spaced_client.h"
#include "chromeos/ash/components/dbus/vm_concierge/concierge_service.pb.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#include "chromeos/ash/components/disks/mock_disk_mount_manager.h"
#include "components/component_updater/ash/fake_component_manager_ash.h"
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

  class WaitingFakeConciergeClient : public ash::FakeConciergeClient {
   public:
    explicit WaitingFakeConciergeClient(ash::FakeCiceroneClient* client)
        : ash::FakeConciergeClient(client) {}

    void StartVm(
        const vm_tools::concierge::StartVmRequest& request,
        chromeos::DBusMethodCallback<vm_tools::concierge::StartVmResponse>
            callback) override {
      ash::FakeConciergeClient::StartVm(request, std::move(callback));
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, result_future_.GetCallback());
    }

    void WaitForStartTerminaVmCalled() {
      EXPECT_TRUE(result_future_.Wait());
      EXPECT_GE(start_vm_call_count(), 1);
    }

   private:
    base::test::TestFuture<void> result_future_;
  };

  CrostiniInstallerTest()
      : local_state_(std::make_unique<ScopedTestingLocalState>(
            TestingBrowserProcess::GetGlobal())),
        browser_part_(g_browser_process->platform_part()) {}

  CrostiniInstallerTest(const CrostiniInstallerTest&) = delete;
  CrostiniInstallerTest& operator=(const CrostiniInstallerTest&) = delete;

  void SetOSRelease() {
    vm_tools::cicerone::OsRelease os_release;
    os_release.set_id("debian");
    os_release.set_version_id("11");
    ash::FakeCiceroneClient::Get()->set_lxd_container_os_release(os_release);
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
    ash::ChunneldClient::InitializeFake();
    ash::CiceroneClient::InitializeFake();
    ash::DebugDaemonClient::InitializeFake();
    ash::FakeSpacedClient::InitializeFake();

    SetOSRelease();
    waiting_fake_concierge_client_ =
        new WaitingFakeConciergeClient(ash::FakeCiceroneClient::Get());

    ash::SeneschalClient::InitializeFake();

    disk_mount_manager_mock_ = new ash::disks::MockDiskMountManager;
    ash::disks::DiskMountManager::InitializeForTesting(
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
    TestingBrowserProcess::GetGlobal()->SetSystemNotificationHelper(
        std::make_unique<SystemNotificationHelper>());
  }

  void TearDown() override {
    g_browser_process->platform_part()->ShutdownSchedulerConfigurationManager();
    crostini_installer_->Shutdown();
    crostini_installer_.reset();
    crostini_test_helper_.reset();
    profile_.reset();

    ash::disks::MockDiskMountManager::Shutdown();
    ash::SeneschalClient::Shutdown();
    ash::DebugDaemonClient::Shutdown();
    ash::ConciergeClient::Shutdown();
    ash::CiceroneClient::Shutdown();
    ash::ChunneldClient::Shutdown();
    ash::DlcserviceClient::Shutdown();
    ash::FakeSpacedClient::Shutdown();

    browser_part_.ShutdownComponentManager();
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
  MockCallbacks mock_callbacks_;
  content::BrowserTaskEnvironment task_environment_{
      content::BrowserTaskEnvironment::REAL_IO_THREAD,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;

  // Owned by DiskMountManager
  raw_ptr<ash::disks::MockDiskMountManager, DanglingUntriaged>
      disk_mount_manager_mock_ = nullptr;

  raw_ptr<WaitingFakeConciergeClient, DanglingUntriaged>
      waiting_fake_concierge_client_ = nullptr;

  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<CrostiniTestHelper> crostini_test_helper_;
  std::unique_ptr<CrostiniInstaller> crostini_installer_;

 private:
  std::unique_ptr<ScopedTestingLocalState> local_state_;
  scoped_refptr<component_updater::FakeComponentManagerAsh> component_manager_;
  BrowserProcessPlatformPartTestApi browser_part_;
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
  // |OnProgress()| should not happens after |OnFinished()|
  EXPECT_CALL(mock_callbacks_, OnFinished(InstallerError::kNone))
      .After(expectation_set);

  Install();

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
  MockAnsibleManagementService* mock_ansible_management_service =
      AnsibleManagementTestHelper::SetUpMockAnsibleManagementService(
          profile_.get());
  AnsibleManagementTestHelper test_helper(profile_.get());
  test_helper.SetUpAnsibleInfra();

  EXPECT_CALL(*mock_ansible_management_service, ConfigureContainer).Times(1);
  ON_CALL(*mock_ansible_management_service, ConfigureContainer)
      .WillByDefault([](const guest_os::GuestId& conatiner_id,
                        base::FilePath playbook,
                        base::OnceCallback<void(bool success)> callback) {
        std::move(callback).Run(true);
      });

  double last_progress = 0.0;
  auto greater_equal_last_progress = Truly(
      [&last_progress](double progress) { return progress >= last_progress; });

  ExpectationSet expectation_set;
  expectation_set +=
      EXPECT_CALL(mock_callbacks_,
                  OnProgress(_, AllOf(greater_equal_last_progress, Le(1.0))))
          .WillRepeatedly(SaveArg<1>(&last_progress));
  // |OnProgress()| should not happens after |OnFinished()|
  EXPECT_CALL(mock_callbacks_, OnFinished(InstallerError::kNone))
      .After(expectation_set);

  Install();

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

  // Hang the installer flow waiting for Tremplin to start, so we get a chance
  // to cancel it.
  waiting_fake_concierge_client_->set_send_tremplin_started_signal_delay(
      base::Days(1));
  task_environment_.FastForwardBy(base::Seconds(1));

  check.Call("calling Cancel()");
  Cancel();
  task_environment_.FastForwardBy(base::Seconds(1));

  histogram_tester_.ExpectUniqueSample(
      "Crostini.SetupResult",
      static_cast<base::HistogramBase::Sample>(
          CrostiniInstaller::SetupResult::kUserCancelledStartTerminaVm),
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
  MockAnsibleManagementService* mock_ansible_management_service =
      AnsibleManagementTestHelper::SetUpMockAnsibleManagementService(
          profile_.get());
  AnsibleManagementTestHelper test_helper(profile_.get());
  test_helper.SetUpAnsibleInfra();

  EXPECT_CALL(*mock_ansible_management_service, ConfigureContainer).Times(1);
  ON_CALL(*mock_ansible_management_service, ConfigureContainer)
      .WillByDefault([](const guest_os::GuestId& container_id,
                        base::FilePath playbook,
                        base::OnceCallback<void(bool success)> callback) {
        std::move(callback).Run(false);
      });
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
