// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/plugin_vm/plugin_vm_manager_impl.h"

#include "ash/public/cpp/shelf_model.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_features.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_manager_factory.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_metrics_util.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_test_helper.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/shelf_controller_helper.h"
#include "chrome/browser/ui/ash/shelf/shelf_spinner_controller.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/chunneld/chunneld_client.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/concierge/fake_concierge_client.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/ash/components/dbus/dlcservice/fake_dlcservice_client.h"
#include "chromeos/ash/components/dbus/seneschal/fake_seneschal_client.h"
#include "chromeos/ash/components/dbus/seneschal/seneschal_client.h"
#include "chromeos/ash/components/dbus/vm_plugin_dispatcher/fake_vm_plugin_dispatcher_client.h"
#include "chromeos/ash/components/dbus/vm_plugin_dispatcher/vm_plugin_dispatcher_client.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plugin_vm {

namespace {

using MockLaunchPluginVmCallback =
    testing::StrictMock<base::MockCallback<base::OnceCallback<void(bool)>>>;

}  // namespace

class PluginVmManagerImplTest : public testing::Test {
 public:
  PluginVmManagerImplTest() {
    ash::ChunneldClient::InitializeFake();
    ash::CiceroneClient::InitializeFake();
    ash::ConciergeClient::InitializeFake();
    ash::DebugDaemonClient::InitializeFake();
    ash::SeneschalClient::InitializeFake();
    ash::VmPluginDispatcherClient::InitializeFake();
    testing_profile_ = std::make_unique<TestingProfile>();
    test_helper_ = std::make_unique<PluginVmTestHelper>(testing_profile_.get());
    plugin_vm_manager_ = static_cast<PluginVmManagerImpl*>(
        PluginVmManagerFactory::GetForProfile(testing_profile_.get()));
    display_service_ = std::make_unique<NotificationDisplayServiceTester>(
        testing_profile_.get());
    shelf_model_ = std::make_unique<ash::ShelfModel>();
    chrome_shelf_controller_ = std::make_unique<ChromeShelfController>(
        testing_profile_.get(), shelf_model_.get());
    chrome_shelf_controller_->SetProfileForTest(testing_profile_.get());
    chrome_shelf_controller_->SetShelfControllerHelperForTest(
        std::make_unique<ShelfControllerHelper>(testing_profile_.get()));
    chrome_shelf_controller_->Init();
    histogram_tester_ = std::make_unique<base::HistogramTester>();
    ash::DlcserviceClient::InitializeFake();

    // Make StartVm succeed by default, tests can override as needed.
    VmPluginDispatcherClient().set_start_vm_response(
        vm_tools::plugin_dispatcher::StartVmResponse());

    // Borealis makes a call, unrelated to this test so just reset it.
    DCHECK_EQ(ConciergeClient().get_vm_info_call_count(), 1);
    ConciergeClient().reset_get_vm_info_call_count();
  }

  PluginVmManagerImplTest(const PluginVmManagerImplTest&) = delete;
  PluginVmManagerImplTest& operator=(const PluginVmManagerImplTest&) = delete;

  ~PluginVmManagerImplTest() override {
    ash::DlcserviceClient::Shutdown();
    histogram_tester_.reset();
    chrome_shelf_controller_.reset();
    shelf_model_.reset();
    display_service_.reset();
    test_helper_.reset();
    testing_profile_.reset();
    ash::VmPluginDispatcherClient::Shutdown();
    ash::SeneschalClient::Shutdown();
    ash::DebugDaemonClient::Shutdown();
    ash::ConciergeClient::Shutdown();
    ash::CiceroneClient::Shutdown();
    ash::ChunneldClient::Shutdown();
  }

 protected:
  ash::FakeVmPluginDispatcherClient& VmPluginDispatcherClient() {
    return *static_cast<ash::FakeVmPluginDispatcherClient*>(
        ash::VmPluginDispatcherClient::Get());
  }
  ash::FakeConciergeClient& ConciergeClient() {
    return *ash::FakeConciergeClient::Get();
  }
  ash::FakeSeneschalClient& SeneschalClient() {
    return *ash::FakeSeneschalClient::Get();
  }

  ShelfSpinnerController* SpinnerController() {
    return chrome_shelf_controller_->GetShelfSpinnerController();
  }

  void SetListVmsResponse(vm_tools::plugin_dispatcher::VmState state) {
    vm_tools::plugin_dispatcher::ListVmResponse list_vms_response;
    list_vms_response.add_vm_info()->set_state(state);
    VmPluginDispatcherClient().set_list_vms_response(list_vms_response);
  }

  void NotifyVmToolsStateChanged(
      vm_tools::plugin_dispatcher::VmToolsState state) {
    vm_tools::plugin_dispatcher::VmToolsStateChangedSignal state_changed_signal;
    state_changed_signal.set_owner_id(
        ash::ProfileHelper::GetUserIdHashFromProfile(testing_profile_.get()));
    state_changed_signal.set_vm_name(kPluginVmName);
    state_changed_signal.set_vm_tools_state(state);
    VmPluginDispatcherClient().NotifyVmToolsStateChanged(state_changed_signal);
  }

  void NotifyVmStateChanged(vm_tools::plugin_dispatcher::VmState state) {
    vm_tools::plugin_dispatcher::VmStateChangedSignal state_changed_signal;
    state_changed_signal.set_owner_id(
        ash::ProfileHelper::GetUserIdHashFromProfile(testing_profile_.get()));
    state_changed_signal.set_vm_name(kPluginVmName);
    state_changed_signal.set_vm_state(state);
    VmPluginDispatcherClient().NotifyVmStateChanged(state_changed_signal);
  }

  void NotifyVmStarted() {
    vm_tools::concierge::VmStartedSignal signal;
    signal.set_name(kPluginVmName);
    signal.set_owner_id(
        ash::ProfileHelper::GetUserIdHashFromProfile(testing_profile_.get()));
    ConciergeClient().NotifyVmStarted(signal);
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> testing_profile_;
  std::unique_ptr<PluginVmTestHelper> test_helper_;
  std::unique_ptr<NotificationDisplayServiceTester> display_service_;
  raw_ptr<PluginVmManagerImpl, DanglingUntriaged> plugin_vm_manager_;
  std::unique_ptr<ash::ShelfModel> shelf_model_;
  std::unique_ptr<ChromeShelfController> chrome_shelf_controller_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

TEST_F(PluginVmManagerImplTest, LaunchPluginVmRequiresPluginVmAllowed) {
  EXPECT_FALSE(PluginVmFeatures::Get()->IsAllowed(testing_profile_.get()));
  MockLaunchPluginVmCallback callback;
  EXPECT_CALL(callback, Run(false));
  plugin_vm_manager_->LaunchPluginVm(callback.Get());
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(VmPluginDispatcherClient().list_vms_called());
  EXPECT_FALSE(VmPluginDispatcherClient().start_vm_called());
  EXPECT_FALSE(VmPluginDispatcherClient().show_vm_called());
  EXPECT_EQ(ConciergeClient().get_vm_info_call_count(), 0);
  EXPECT_FALSE(SeneschalClient().share_path_called());
  EXPECT_EQ(plugin_vm_manager_->seneschal_server_handle(), 0ul);

  histogram_tester_->ExpectUniqueSample(kPluginVmLaunchResultHistogram,
                                        PluginVmLaunchResult::kError, 1);
}

TEST_F(PluginVmManagerImplTest, LaunchPluginVmStartAndShow) {
  test_helper_->AllowPluginVm();
  EXPECT_TRUE(PluginVmFeatures::Get()->IsAllowed(testing_profile_.get()));

  // The PluginVmManagerImpl calls StartVm when the VM is not yet running.
  SetListVmsResponse(vm_tools::plugin_dispatcher::VmState::VM_STATE_STOPPED);

  MockLaunchPluginVmCallback callback;
  plugin_vm_manager_->LaunchPluginVm(callback.Get());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(VmPluginDispatcherClient().list_vms_called());
  EXPECT_TRUE(VmPluginDispatcherClient().start_vm_called());
  EXPECT_TRUE(VmPluginDispatcherClient().show_vm_called());
  EXPECT_EQ(ConciergeClient().get_vm_info_call_count(), 0);
  EXPECT_FALSE(SeneschalClient().share_path_called());
  EXPECT_EQ(plugin_vm_manager_->seneschal_server_handle(), 0ul);

  histogram_tester_->ExpectUniqueSample(kPluginVmLaunchResultHistogram,
                                        PluginVmLaunchResult::kSuccess, 1);

  EXPECT_CALL(callback, Run(true));
  NotifyVmToolsStateChanged(
      vm_tools::plugin_dispatcher::VmToolsState::VM_TOOLS_STATE_INSTALLED);
}

TEST_F(PluginVmManagerImplTest, LaunchesOnceFromMultipleRequests) {
  test_helper_->AllowPluginVm();
  EXPECT_TRUE(PluginVmFeatures::Get()->IsAllowed(testing_profile_.get()));

  // The PluginVmManagerImpl calls StartVm when the VM is not yet running.
  SetListVmsResponse(vm_tools::plugin_dispatcher::VmState::VM_STATE_STOPPED);

  MockLaunchPluginVmCallback callback1, callback2, callback3;
  plugin_vm_manager_->LaunchPluginVm(callback1.Get());
  plugin_vm_manager_->LaunchPluginVm(callback2.Get());
  plugin_vm_manager_->LaunchPluginVm(callback3.Get());
  task_environment_.RunUntilIdle();

  histogram_tester_->ExpectUniqueSample(kPluginVmLaunchResultHistogram,
                                        PluginVmLaunchResult::kSuccess, 1);

  EXPECT_CALL(callback1, Run(true));
  EXPECT_CALL(callback2, Run(true));
  EXPECT_CALL(callback3, Run(true));
  NotifyVmToolsStateChanged(
      vm_tools::plugin_dispatcher::VmToolsState::VM_TOOLS_STATE_INSTALLED);
}

TEST_F(PluginVmManagerImplTest, LaunchPluginVmShowAndStop) {
  test_helper_->AllowPluginVm();
  EXPECT_TRUE(PluginVmFeatures::Get()->IsAllowed(testing_profile_.get()));

  // The PluginVmManagerImpl skips calling StartVm when the VM is already
  // running.
  SetListVmsResponse(vm_tools::plugin_dispatcher::VmState::VM_STATE_RUNNING);
  // The PluginVmManagerImpl doesn't wait for VmToolsState if it is already
  // known.
  NotifyVmToolsStateChanged(
      vm_tools::plugin_dispatcher::VmToolsState::VM_TOOLS_STATE_INSTALLED);

  MockLaunchPluginVmCallback callback;
  plugin_vm_manager_->LaunchPluginVm(callback.Get());
  EXPECT_CALL(callback, Run(true));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(VmPluginDispatcherClient().list_vms_called());
  EXPECT_FALSE(VmPluginDispatcherClient().start_vm_called());
  EXPECT_TRUE(VmPluginDispatcherClient().show_vm_called());
  EXPECT_FALSE(VmPluginDispatcherClient().stop_vm_called());
  EXPECT_EQ(ConciergeClient().get_vm_info_call_count(), 0);
  EXPECT_FALSE(SeneschalClient().share_path_called());
  EXPECT_EQ(plugin_vm_manager_->seneschal_server_handle(), 0ul);

  histogram_tester_->ExpectUniqueSample(kPluginVmLaunchResultHistogram,
                                        PluginVmLaunchResult::kSuccess, 1);

  plugin_vm_manager_->StopPluginVm(kPluginVmName, /*force=*/true);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(VmPluginDispatcherClient().stop_vm_called());
}

TEST_F(PluginVmManagerImplTest, OnStateChangedRunningStoppedSuspended) {
  test_helper_->AllowPluginVm();
  EXPECT_TRUE(PluginVmFeatures::Get()->IsAllowed(testing_profile_.get()));

  // Signals for RUNNING, then STOPPED.
  test_helper_->OpenShelfItem();
  EXPECT_TRUE(
      chrome_shelf_controller_->IsOpen(ash::ShelfID(kPluginVmShelfAppId)));

  NotifyVmStarted();
  NotifyVmStateChanged(vm_tools::plugin_dispatcher::VmState::VM_STATE_RUNNING);
  task_environment_.RunUntilIdle();
  EXPECT_GE(ConciergeClient().get_vm_info_call_count(), 1);
  EXPECT_TRUE(base::DirectoryExists(
      file_manager::util::GetMyFilesFolderForProfile(testing_profile_.get())));
  EXPECT_TRUE(SeneschalClient().share_path_called());
  EXPECT_EQ(plugin_vm_manager_->seneschal_server_handle(), 1ul);

  NotifyVmStateChanged(vm_tools::plugin_dispatcher::VmState::VM_STATE_STOPPED);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(plugin_vm_manager_->seneschal_server_handle(), 0ul);
  EXPECT_FALSE(
      chrome_shelf_controller_->IsOpen(ash::ShelfID(kPluginVmShelfAppId)));

  // Signals for RUNNING, then SUSPENDED.
  NotifyVmStateChanged(vm_tools::plugin_dispatcher::VmState::VM_STATE_RUNNING);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(plugin_vm_manager_->seneschal_server_handle(), 1ul);

  NotifyVmStateChanged(
      vm_tools::plugin_dispatcher::VmState::VM_STATE_SUSPENDED);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(plugin_vm_manager_->seneschal_server_handle(), 0ul);
}

TEST_F(PluginVmManagerImplTest, LaunchPluginVmSpinner) {
  test_helper_->AllowPluginVm();
  EXPECT_TRUE(PluginVmFeatures::Get()->IsAllowed(testing_profile_.get()));

  // No spinner before doing anything
  EXPECT_FALSE(SpinnerController()->HasApp(kPluginVmShelfAppId));

  SetListVmsResponse(vm_tools::plugin_dispatcher::VmState::VM_STATE_STOPPED);

  plugin_vm_manager_->LaunchPluginVm(base::DoNothing());
  task_environment_.RunUntilIdle();

  // Spinner exists for first launch.
  EXPECT_TRUE(SpinnerController()->HasApp(kPluginVmShelfAppId));

  // The actual flow would've launched a real window.
  test_helper_->OpenShelfItem();
  EXPECT_FALSE(SpinnerController()->HasApp(kPluginVmShelfAppId));
  test_helper_->CloseShelfItem();

  plugin_vm_manager_->LaunchPluginVm(base::DoNothing());
  task_environment_.RunUntilIdle();
  // A second launch shouldn't show a spinner.
  EXPECT_FALSE(SpinnerController()->HasApp(kPluginVmShelfAppId));
}

TEST_F(PluginVmManagerImplTest, LaunchPluginVmFromSuspending) {
  // We cannot start a vm in states like SUSPENDING, so the StartVm call is
  // delayed until an appropriate state change signal is received.
  test_helper_->AllowPluginVm();

  NotifyVmStateChanged(
      vm_tools::plugin_dispatcher::VmState::VM_STATE_SUSPENDING);

  SetListVmsResponse(vm_tools::plugin_dispatcher::VmState::VM_STATE_SUSPENDING);
  MockLaunchPluginVmCallback callback;
  plugin_vm_manager_->LaunchPluginVm(callback.Get());
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(VmPluginDispatcherClient().list_vms_called());
  EXPECT_FALSE(VmPluginDispatcherClient().start_vm_called());
  EXPECT_FALSE(VmPluginDispatcherClient().show_vm_called());
  EXPECT_TRUE(SpinnerController()->HasApp(kPluginVmShelfAppId));

  // The launch process continues once the operation completes.
  NotifyVmStateChanged(
      vm_tools::plugin_dispatcher::VmState::VM_STATE_SUSPENDED);
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(VmPluginDispatcherClient().list_vms_called());
  EXPECT_TRUE(VmPluginDispatcherClient().start_vm_called());
  EXPECT_TRUE(VmPluginDispatcherClient().show_vm_called());

  EXPECT_CALL(callback, Run(true));
  NotifyVmToolsStateChanged(
      vm_tools::plugin_dispatcher::VmToolsState::VM_TOOLS_STATE_INSTALLED);
}

TEST_F(PluginVmManagerImplTest, LaunchPluginVmInterrupted) {
  test_helper_->AllowPluginVm();

  // Skip StartVm call.
  SetListVmsResponse(vm_tools::plugin_dispatcher::VmState::VM_STATE_RUNNING);
  NotifyVmToolsStateChanged(
      vm_tools::plugin_dispatcher::VmToolsState::VM_TOOLS_STATE_OUTDATED);
  MockLaunchPluginVmCallback callback1;
  plugin_vm_manager_->LaunchPluginVm(callback1.Get());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(VmPluginDispatcherClient().show_vm_called());

  // VM is shown, but we are waiting for tools to be installed. If the VM is
  // suspended now, we consider the launch to have failed.
  EXPECT_CALL(callback1, Run(false));
  NotifyVmStateChanged(
      vm_tools::plugin_dispatcher::VmState::VM_STATE_SUSPENDED);
  SetListVmsResponse(vm_tools::plugin_dispatcher::VmState::VM_STATE_SUSPENDED);
  task_environment_.RunUntilIdle();

  MockLaunchPluginVmCallback callback2;
  EXPECT_CALL(callback2, Run(true));
  plugin_vm_manager_->LaunchPluginVm(callback2.Get());
  NotifyVmToolsStateChanged(
      vm_tools::plugin_dispatcher::VmToolsState::VM_TOOLS_STATE_INSTALLED);
  task_environment_.RunUntilIdle();
}

TEST_F(PluginVmManagerImplTest, LaunchPluginVmInvalidLicense) {
  test_helper_->AllowPluginVm();
  EXPECT_TRUE(PluginVmFeatures::Get()->IsAllowed(testing_profile_.get()));

  // The PluginVmManagerImpl calls StartVm when the VM is not yet running.
  SetListVmsResponse(vm_tools::plugin_dispatcher::VmState::VM_STATE_STOPPED);

  vm_tools::plugin_dispatcher::StartVmResponse start_vm_response;
  start_vm_response.set_error(
      vm_tools::plugin_dispatcher::VmErrorCode::VM_ERR_NATIVE_RESULT_CODE);
  start_vm_response.set_result_code(PRL_ERR_LICENSE_NOT_VALID);
  VmPluginDispatcherClient().set_start_vm_response(start_vm_response);

  MockLaunchPluginVmCallback callback;
  plugin_vm_manager_->LaunchPluginVm(callback.Get());
  EXPECT_CALL(callback, Run(false));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(VmPluginDispatcherClient().show_vm_called());

  histogram_tester_->ExpectUniqueSample(
      kPluginVmLaunchResultHistogram, PluginVmLaunchResult::kInvalidLicense, 1);
}

TEST_F(PluginVmManagerImplTest, RelaunchPluginVm) {
  test_helper_->AllowPluginVm();
  EXPECT_TRUE(PluginVmFeatures::Get()->IsAllowed(testing_profile_.get()));

  // The PluginVmManagerImpl calls StartVm when the VM is not yet running.
  SetListVmsResponse(vm_tools::plugin_dispatcher::VmState::VM_STATE_STOPPED);

  plugin_vm_manager_->RelaunchPluginVm();
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(VmPluginDispatcherClient().list_vms_called());
  EXPECT_TRUE(VmPluginDispatcherClient().start_vm_called());
  EXPECT_TRUE(VmPluginDispatcherClient().show_vm_called());
  EXPECT_EQ(ConciergeClient().get_vm_info_call_count(), 0);
  EXPECT_FALSE(SeneschalClient().share_path_called());
  EXPECT_EQ(plugin_vm_manager_->seneschal_server_handle(), 0ul);

  histogram_tester_->ExpectUniqueSample(kPluginVmLaunchResultHistogram,
                                        PluginVmLaunchResult::kSuccess, 1);

  NotifyVmToolsStateChanged(
      vm_tools::plugin_dispatcher::VmToolsState::VM_TOOLS_STATE_INSTALLED);
}

TEST_F(PluginVmManagerImplTest, UninstallRunningPluginVm) {
  test_helper_->AllowPluginVm();
  EXPECT_TRUE(PluginVmFeatures::Get()->IsAllowed(testing_profile_.get()));

  SetListVmsResponse(vm_tools::plugin_dispatcher::VmState::VM_STATE_RUNNING);
  testing_profile_->GetPrefs()->SetBoolean(
      plugin_vm::prefs::kPluginVmImageExists, true);

  EXPECT_EQ(plugin_vm_manager_->uninstaller_notification_for_testing(),
            nullptr);
  plugin_vm_manager_->UninstallPluginVm();
  EXPECT_NE(plugin_vm_manager_->uninstaller_notification_for_testing(),
            nullptr);
  EXPECT_TRUE(testing_profile_->GetPrefs()->GetBoolean(
      plugin_vm::prefs::kPluginVmImageExists));

  task_environment_.RunUntilIdle();
  EXPECT_TRUE(VmPluginDispatcherClient().list_vms_called());
  EXPECT_TRUE(VmPluginDispatcherClient().stop_vm_called());
  EXPECT_GE(ConciergeClient().destroy_disk_image_call_count(), 1);
  EXPECT_EQ(plugin_vm_manager_->uninstaller_notification_for_testing(),
            nullptr);
  EXPECT_FALSE(testing_profile_->GetPrefs()->GetBoolean(
      plugin_vm::prefs::kPluginVmImageExists));
}

TEST_F(PluginVmManagerImplTest, UninstallStoppedPluginVm) {
  test_helper_->AllowPluginVm();
  EXPECT_TRUE(PluginVmFeatures::Get()->IsAllowed(testing_profile_.get()));

  SetListVmsResponse(vm_tools::plugin_dispatcher::VmState::VM_STATE_STOPPED);
  testing_profile_->GetPrefs()->SetBoolean(
      plugin_vm::prefs::kPluginVmImageExists, true);

  EXPECT_EQ(plugin_vm_manager_->uninstaller_notification_for_testing(),
            nullptr);
  plugin_vm_manager_->UninstallPluginVm();
  EXPECT_NE(plugin_vm_manager_->uninstaller_notification_for_testing(),
            nullptr);
  EXPECT_TRUE(testing_profile_->GetPrefs()->GetBoolean(
      plugin_vm::prefs::kPluginVmImageExists));

  task_environment_.RunUntilIdle();
  EXPECT_TRUE(VmPluginDispatcherClient().list_vms_called());
  EXPECT_FALSE(VmPluginDispatcherClient().stop_vm_called());
  EXPECT_GE(ConciergeClient().destroy_disk_image_call_count(), 1);
  EXPECT_EQ(plugin_vm_manager_->uninstaller_notification_for_testing(),
            nullptr);
  EXPECT_FALSE(testing_profile_->GetPrefs()->GetBoolean(
      plugin_vm::prefs::kPluginVmImageExists));
}

TEST_F(PluginVmManagerImplTest, UninstallSuspendingPluginVm) {
  test_helper_->AllowPluginVm();
  EXPECT_TRUE(PluginVmFeatures::Get()->IsAllowed(testing_profile_.get()));

  SetListVmsResponse(vm_tools::plugin_dispatcher::VmState::VM_STATE_SUSPENDING);
  testing_profile_->GetPrefs()->SetBoolean(
      plugin_vm::prefs::kPluginVmImageExists, true);

  EXPECT_EQ(plugin_vm_manager_->uninstaller_notification_for_testing(),
            nullptr);
  plugin_vm_manager_->UninstallPluginVm();
  EXPECT_NE(plugin_vm_manager_->uninstaller_notification_for_testing(),
            nullptr);
  EXPECT_TRUE(testing_profile_->GetPrefs()->GetBoolean(
      plugin_vm::prefs::kPluginVmImageExists));

  task_environment_.RunUntilIdle();
  EXPECT_TRUE(VmPluginDispatcherClient().list_vms_called());
  EXPECT_FALSE(VmPluginDispatcherClient().stop_vm_called());
  EXPECT_EQ(ConciergeClient().destroy_disk_image_call_count(), 0);
  EXPECT_NE(plugin_vm_manager_->uninstaller_notification_for_testing(),
            nullptr);
  EXPECT_TRUE(testing_profile_->GetPrefs()->GetBoolean(
      plugin_vm::prefs::kPluginVmImageExists));

  NotifyVmStateChanged(
      vm_tools::plugin_dispatcher::VmState::VM_STATE_SUSPENDED);
  EXPECT_FALSE(VmPluginDispatcherClient().stop_vm_called());
  EXPECT_GE(ConciergeClient().destroy_disk_image_call_count(), 1);
  EXPECT_NE(plugin_vm_manager_->uninstaller_notification_for_testing(),
            nullptr);
  EXPECT_TRUE(testing_profile_->GetPrefs()->GetBoolean(
      plugin_vm::prefs::kPluginVmImageExists));

  task_environment_.RunUntilIdle();
  EXPECT_EQ(plugin_vm_manager_->uninstaller_notification_for_testing(),
            nullptr);
  EXPECT_FALSE(testing_profile_->GetPrefs()->GetBoolean(
      plugin_vm::prefs::kPluginVmImageExists));
}

TEST_F(PluginVmManagerImplTest, UninstallMissingPluginVm) {
  test_helper_->AllowPluginVm();
  EXPECT_TRUE(PluginVmFeatures::Get()->IsAllowed(testing_profile_.get()));

  VmPluginDispatcherClient().set_list_vms_response(
      vm_tools::plugin_dispatcher::ListVmResponse());
  testing_profile_->GetPrefs()->SetBoolean(
      plugin_vm::prefs::kPluginVmImageExists, true);

  EXPECT_EQ(plugin_vm_manager_->uninstaller_notification_for_testing(),
            nullptr);
  plugin_vm_manager_->UninstallPluginVm();
  EXPECT_NE(plugin_vm_manager_->uninstaller_notification_for_testing(),
            nullptr);
  EXPECT_TRUE(testing_profile_->GetPrefs()->GetBoolean(
      plugin_vm::prefs::kPluginVmImageExists));

  task_environment_.RunUntilIdle();
  EXPECT_TRUE(VmPluginDispatcherClient().list_vms_called());
  EXPECT_FALSE(VmPluginDispatcherClient().stop_vm_called());
  EXPECT_EQ(ConciergeClient().destroy_disk_image_call_count(), 0);
  EXPECT_EQ(plugin_vm_manager_->uninstaller_notification_for_testing(),
            nullptr);
  EXPECT_FALSE(testing_profile_->GetPrefs()->GetBoolean(
      plugin_vm::prefs::kPluginVmImageExists));
}

}  // namespace plugin_vm
