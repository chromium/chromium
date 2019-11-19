// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/plugin_vm/plugin_vm_manager.h"

#include "ash/public/cpp/shelf_model.h"
#include "base/files/file_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_metrics_util.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_test_helper.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/shelf_spinner_controller.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_concierge_client.h"
#include "chromeos/dbus/fake_seneschal_client.h"
#include "chromeos/dbus/fake_vm_plugin_dispatcher_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plugin_vm {

constexpr char kInvalidLicenseNotificationId[] = "plugin-vm-invalid-license";

class PluginVmManagerTest : public testing::Test {
 public:
  PluginVmManagerTest() {
    chromeos::DBusThreadManager::Initialize();
    testing_profile_ = std::make_unique<TestingProfile>();
    test_helper_ = std::make_unique<PluginVmTestHelper>(testing_profile_.get());
    plugin_vm_manager_ = PluginVmManager::GetForProfile(testing_profile_.get());
    display_service_ = std::make_unique<NotificationDisplayServiceTester>(
        testing_profile_.get());
    shelf_model_ = std::make_unique<ash::ShelfModel>();
    chrome_launcher_controller_ = std::make_unique<ChromeLauncherController>(
        testing_profile_.get(), shelf_model_.get());
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  ~PluginVmManagerTest() override {
    histogram_tester_.reset();
    chrome_launcher_controller_.reset();
    shelf_model_.reset();
    display_service_.reset();
    test_helper_.reset();
    testing_profile_.reset();
    chromeos::DBusThreadManager::Shutdown();
  }

 protected:
  chromeos::FakeVmPluginDispatcherClient& VmPluginDispatcherClient() {
    return *static_cast<chromeos::FakeVmPluginDispatcherClient*>(
        chromeos::DBusThreadManager::Get()->GetVmPluginDispatcherClient());
  }
  chromeos::FakeConciergeClient& ConciergeClient() {
    return *static_cast<chromeos::FakeConciergeClient*>(
        chromeos::DBusThreadManager::Get()->GetConciergeClient());
  }
  chromeos::FakeSeneschalClient& SeneschalClient() {
    return *static_cast<chromeos::FakeSeneschalClient*>(
        chromeos::DBusThreadManager::Get()->GetSeneschalClient());
  }

  ShelfSpinnerController* SpinnerController() {
    return chrome_launcher_controller_->GetShelfSpinnerController();
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> testing_profile_;
  std::unique_ptr<PluginVmTestHelper> test_helper_;
  std::unique_ptr<NotificationDisplayServiceTester> display_service_;
  PluginVmManager* plugin_vm_manager_;
  std::unique_ptr<ash::ShelfModel> shelf_model_;
  std::unique_ptr<ChromeLauncherController> chrome_launcher_controller_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PluginVmManagerTest);
};

TEST_F(PluginVmManagerTest, LaunchPluginVmRequiresPluginVmAllowed) {
  EXPECT_FALSE(IsPluginVmAllowedForProfile(testing_profile_.get()));
  plugin_vm_manager_->LaunchPluginVm();
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(VmPluginDispatcherClient().list_vms_called());
  EXPECT_FALSE(VmPluginDispatcherClient().start_vm_called());
  EXPECT_FALSE(VmPluginDispatcherClient().show_vm_called());
  EXPECT_FALSE(ConciergeClient().get_vm_info_called());
  EXPECT_FALSE(SeneschalClient().share_path_called());
  EXPECT_EQ(plugin_vm_manager_->seneschal_server_handle(), 0ul);

  histogram_tester_->ExpectUniqueSample(kPluginVmLaunchResultHistogram,
                                        PluginVmLaunchResult::kError, 1);
}

TEST_F(PluginVmManagerTest, LaunchPluginVmStartAndShow) {
  test_helper_->AllowPluginVm();
  EXPECT_TRUE(IsPluginVmAllowedForProfile(testing_profile_.get()));

  // The PluginVmManager calls StartVm when the VM is not yet running.
  vm_tools::plugin_dispatcher::ListVmResponse list_vms_response;
  list_vms_response.add_vm_info()->set_state(
      vm_tools::plugin_dispatcher::VmState::VM_STATE_STOPPED);
  VmPluginDispatcherClient().set_list_vms_response(list_vms_response);

  plugin_vm_manager_->LaunchPluginVm();
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(VmPluginDispatcherClient().list_vms_called());
  EXPECT_TRUE(VmPluginDispatcherClient().start_vm_called());
  EXPECT_TRUE(VmPluginDispatcherClient().show_vm_called());
  EXPECT_FALSE(ConciergeClient().get_vm_info_called());
  EXPECT_FALSE(SeneschalClient().share_path_called());
  EXPECT_EQ(plugin_vm_manager_->seneschal_server_handle(), 0ul);

  histogram_tester_->ExpectUniqueSample(kPluginVmLaunchResultHistogram,
                                        PluginVmLaunchResult::kSuccess, 1);
}

TEST_F(PluginVmManagerTest, LaunchPluginVmShowAndStop) {
  test_helper_->AllowPluginVm();
  EXPECT_TRUE(IsPluginVmAllowedForProfile(testing_profile_.get()));

  // The PluginVmManager skips calling StartVm when the VM is already running.
  vm_tools::plugin_dispatcher::ListVmResponse list_vms_response;
  list_vms_response.add_vm_info()->set_state(
      vm_tools::plugin_dispatcher::VmState::VM_STATE_RUNNING);
  VmPluginDispatcherClient().set_list_vms_response(list_vms_response);

  plugin_vm_manager_->LaunchPluginVm();
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(VmPluginDispatcherClient().list_vms_called());
  EXPECT_FALSE(VmPluginDispatcherClient().start_vm_called());
  EXPECT_TRUE(VmPluginDispatcherClient().show_vm_called());
  EXPECT_FALSE(VmPluginDispatcherClient().stop_vm_called());
  EXPECT_FALSE(ConciergeClient().get_vm_info_called());
  EXPECT_FALSE(SeneschalClient().share_path_called());
  EXPECT_EQ(plugin_vm_manager_->seneschal_server_handle(), 0ul);

  plugin_vm_manager_->StopPluginVm(kPluginVmName);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(VmPluginDispatcherClient().stop_vm_called());

  histogram_tester_->ExpectUniqueSample(kPluginVmLaunchResultHistogram,
                                        PluginVmLaunchResult::kSuccess, 1);
}

TEST_F(PluginVmManagerTest, OnStateChangedRunningStoppedSuspended) {
  test_helper_->AllowPluginVm();
  EXPECT_TRUE(IsPluginVmAllowedForProfile(testing_profile_.get()));

  // Signals for RUNNING, then STOPPED.
  test_helper_->OpenShelfItem();
  EXPECT_TRUE(
      chrome_launcher_controller_->IsOpen(ash::ShelfID(kPluginVmAppId)));

  vm_tools::plugin_dispatcher::VmStateChangedSignal state_changed_signal;
  state_changed_signal.set_owner_id(
      chromeos::ProfileHelper::GetUserIdHashFromProfile(
          testing_profile_.get()));
  state_changed_signal.set_vm_name(kPluginVmName);
  state_changed_signal.set_vm_state(
      vm_tools::plugin_dispatcher::VmState::VM_STATE_RUNNING);
  VmPluginDispatcherClient().NotifyVmStateChanged(state_changed_signal);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(ConciergeClient().get_vm_info_called());
  EXPECT_TRUE(base::DirectoryExists(
      file_manager::util::GetMyFilesFolderForProfile(testing_profile_.get())));
  EXPECT_TRUE(SeneschalClient().share_path_called());
  EXPECT_EQ(plugin_vm_manager_->seneschal_server_handle(), 1ul);

  state_changed_signal.set_vm_state(
      vm_tools::plugin_dispatcher::VmState::VM_STATE_STOPPED);
  VmPluginDispatcherClient().NotifyVmStateChanged(state_changed_signal);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(plugin_vm_manager_->seneschal_server_handle(), 0ul);
  EXPECT_FALSE(
      chrome_launcher_controller_->IsOpen(ash::ShelfID(kPluginVmAppId)));

  // Signals for RUNNING, then SUSPENDED.
  state_changed_signal.set_vm_state(
      vm_tools::plugin_dispatcher::VmState::VM_STATE_RUNNING);
  VmPluginDispatcherClient().NotifyVmStateChanged(state_changed_signal);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(plugin_vm_manager_->seneschal_server_handle(), 1ul);

  state_changed_signal.set_vm_state(
      vm_tools::plugin_dispatcher::VmState::VM_STATE_SUSPENDED);
  VmPluginDispatcherClient().NotifyVmStateChanged(state_changed_signal);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(plugin_vm_manager_->seneschal_server_handle(), 0ul);
}

TEST_F(PluginVmManagerTest, LaunchPluginVmSpinner) {
  test_helper_->AllowPluginVm();
  EXPECT_TRUE(IsPluginVmAllowedForProfile(testing_profile_.get()));

  // No spinner before doing anything
  EXPECT_FALSE(SpinnerController()->HasApp(kPluginVmAppId));

  vm_tools::plugin_dispatcher::ListVmResponse list_vms_response;
  list_vms_response.add_vm_info()->set_state(
      vm_tools::plugin_dispatcher::VmState::VM_STATE_STOPPED);
  VmPluginDispatcherClient().set_list_vms_response(list_vms_response);

  plugin_vm_manager_->LaunchPluginVm();
  task_environment_.RunUntilIdle();

  // Spinner exists for first launch.
  EXPECT_TRUE(SpinnerController()->HasApp(kPluginVmAppId));
  // The actual flow would've launched a real window.
  test_helper_->OpenShelfItem();
  EXPECT_FALSE(SpinnerController()->HasApp(kPluginVmAppId));
  test_helper_->CloseShelfItem();

  plugin_vm_manager_->LaunchPluginVm();
  task_environment_.RunUntilIdle();
  // A second launch shouldn't show a spinner.
  EXPECT_FALSE(SpinnerController()->HasApp(kPluginVmAppId));
}

TEST_F(PluginVmManagerTest, LaunchPluginVmFromSuspending) {
  // We cannot start a vm in states like SUSPENDING, so the StartVm call is
  // delayed until an appropriate state change signal is received.
  test_helper_->AllowPluginVm();

  vm_tools::plugin_dispatcher::VmStateChangedSignal state_changed_signal;
  state_changed_signal.set_owner_id(
      chromeos::ProfileHelper::GetUserIdHashFromProfile(
          testing_profile_.get()));
  state_changed_signal.set_vm_name(kPluginVmName);
  state_changed_signal.set_vm_state(
      vm_tools::plugin_dispatcher::VmState::VM_STATE_SUSPENDING);
  VmPluginDispatcherClient().NotifyVmStateChanged(state_changed_signal);

  vm_tools::plugin_dispatcher::ListVmResponse list_vms_response;
  list_vms_response.add_vm_info()->set_state(
      vm_tools::plugin_dispatcher::VmState::VM_STATE_SUSPENDING);
  VmPluginDispatcherClient().set_list_vms_response(list_vms_response);
  plugin_vm_manager_->LaunchPluginVm();
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(VmPluginDispatcherClient().list_vms_called());
  EXPECT_FALSE(VmPluginDispatcherClient().start_vm_called());
  EXPECT_FALSE(VmPluginDispatcherClient().show_vm_called());
  EXPECT_TRUE(SpinnerController()->HasApp(kPluginVmAppId));

  // The launch process continues once the operation completes.
  state_changed_signal.set_vm_state(
      vm_tools::plugin_dispatcher::VmState::VM_STATE_SUSPENDED);
  VmPluginDispatcherClient().NotifyVmStateChanged(state_changed_signal);
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(VmPluginDispatcherClient().list_vms_called());
  EXPECT_TRUE(VmPluginDispatcherClient().start_vm_called());
  EXPECT_TRUE(VmPluginDispatcherClient().show_vm_called());
}

TEST_F(PluginVmManagerTest, LaunchPluginVmInvalidLicense) {
  test_helper_->AllowPluginVm();
  EXPECT_TRUE(IsPluginVmAllowedForProfile(testing_profile_.get()));

  // The PluginVmManager calls StartVm when the VM is not yet running.
  vm_tools::plugin_dispatcher::ListVmResponse list_vms_response;
  list_vms_response.add_vm_info()->set_state(
      vm_tools::plugin_dispatcher::VmState::VM_STATE_STOPPED);
  VmPluginDispatcherClient().set_list_vms_response(list_vms_response);

  vm_tools::plugin_dispatcher::StartVmResponse start_vm_response;
  start_vm_response.set_error(
      vm_tools::plugin_dispatcher::VmErrorCode::VM_ERR_LIC_NOT_VALID);
  VmPluginDispatcherClient().set_start_vm_response(start_vm_response);

  plugin_vm_manager_->LaunchPluginVm();
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(VmPluginDispatcherClient().show_vm_called());

  EXPECT_TRUE(display_service_->GetNotification(kInvalidLicenseNotificationId));

  histogram_tester_->ExpectUniqueSample(
      kPluginVmLaunchResultHistogram, PluginVmLaunchResult::kInvalidLicense, 1);
}

}  // namespace plugin_vm
