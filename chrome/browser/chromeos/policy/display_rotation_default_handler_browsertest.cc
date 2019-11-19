// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/display_rotation_default_handler.h"

#include <memory>

#include "ash/display/display_configuration_controller.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/login/test/device_state_mixin.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"
#include "ui/display/display_layout.h"
#include "ui/display/manager/display_manager.h"

namespace em = enterprise_management;

namespace {

display::DisplayManager* GetDisplayManager() {
  return ash::Shell::Get()->display_manager();
}

display::Display::Rotation GetRotationOfFirstDisplay() {
  const display::DisplayManager* const display_manager = GetDisplayManager();
  const int64_t first_display_id = display_manager->first_display_id();
  const display::Display& first_display =
      display_manager->GetDisplayForId(first_display_id);
  return first_display.rotation();
}

// Fails the test and returns ROTATE_0 if there is no second display.
display::Display::Rotation GetRotationOfSecondDisplay() {
  const display::DisplayManager* const display_manager = GetDisplayManager();
  if (display_manager->GetNumDisplays() < 2) {
    ADD_FAILURE()
        << "Requested rotation of second display while there was only one.";
    return display::Display::ROTATE_0;
  }
  const display::DisplayIdList display_id_pair =
      display_manager->GetCurrentDisplayIdList();
  const display::Display& second_display =
      display_manager->GetDisplayForId(display_id_pair[1]);
  return second_display.rotation();
}

} // anonymous namespace

namespace policy {

class DisplayRotationDefaultTest
    : public policy::DevicePolicyCrosBrowserTest,
      public testing::WithParamInterface<display::Display::Rotation> {
 public:
  DisplayRotationDefaultTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(chromeos::switches::kLoginManager);
    command_line->AppendSwitch(chromeos::switches::kForceLoginManagerInTests);
  }

  void SetUpInProcessBrowserTestFixture() override {
    ash::DisplayConfigurationController::DisableAnimatorForTest();
    DevicePolicyCrosBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  void TearDownOnMainThread() override {
    // If the login display is still showing, exit gracefully.
    if (chromeos::LoginDisplayHost::default_host()) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(&chrome::AttemptExit));
      RunUntilBrowserProcessQuits();
    }
  }

 protected:
  void SetPolicy(int rotation) {
    em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
    proto.mutable_display_rotation_default()->set_display_rotation_default(
        static_cast<em::DisplayRotationDefaultProto::Rotation>(rotation));
    RefreshPolicyAndWaitUntilDeviceSettingsUpdated();
  }

  // This is needed to test that refreshing the settings without change to the
  // DisplayRotationDefault policy will not rotate the display again.
  void SetADifferentPolicy() {
    em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
    const bool clock24 = proto.use_24hour_clock().use_24hour_clock();
    proto.mutable_use_24hour_clock()->set_use_24hour_clock(!clock24);
    RefreshPolicyAndWaitUntilDeviceSettingsUpdated();
  }

  void UnsetPolicy() {
    em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
    proto.clear_display_rotation_default();
    RefreshPolicyAndWaitUntilDeviceSettingsUpdated();
  }

  // Creates second display if there is none yet, or removes it if there is one.
  void ToggleSecondDisplay() {
    GetDisplayManager()->AddRemoveDisplay();
    base::RunLoop().RunUntilIdle();
  }

  void RefreshPolicyAndWaitUntilDeviceSettingsUpdated() {
    base::RunLoop run_loop;
    // For calls from SetPolicy().
    std::unique_ptr<chromeos::CrosSettings::ObserverSubscription> observer =
        chromeos::CrosSettings::Get()->AddSettingsObserver(
            chromeos::kDisplayRotationDefault, run_loop.QuitClosure());
    // For calls from SetADifferentPolicy().
    std::unique_ptr<chromeos::CrosSettings::ObserverSubscription> observer2 =
        chromeos::CrosSettings::Get()->AddSettingsObserver(
            chromeos::kSystemUse24HourClock, run_loop.QuitClosure());
    RefreshDevicePolicy();
    run_loop.Run();
    // Allow tasks posted by CrosSettings observers to complete:
    base::RunLoop().RunUntilIdle();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DisplayRotationDefaultTest);
};

// If display::Display::Rotation is ever modified and this test fails, there are
// hardcoded enum-values in the following files that might need adjustment:
// * this file: range check in this function, initializations, expected values,
//              the list of parameters at the bottom of the file
// * display_rotation_default_handler.cc: Range check in UpdateFromCrosSettings,
//                                        initialization to ROTATE_0
// * display_rotation_default_handler.h: initialization to ROTATE_0
// * components/policy/proto/chrome_device_policy.proto:
//       DisplayRotationDefaultProto::Rotation should match one to one
IN_PROC_BROWSER_TEST_P(DisplayRotationDefaultTest, EnumsInSync) {
  const display::Display::Rotation rotation = GetParam();
  EXPECT_EQ(em::DisplayRotationDefaultProto::Rotation_ARRAYSIZE,
            display::Display::ROTATE_270 + 1)
      << "Enums display::Display::Rotation and "
      << "em::DisplayRotationDefaultProto::Rotation are not in sync.";
  EXPECT_TRUE(em::DisplayRotationDefaultProto::Rotation_IsValid(rotation))
      << rotation << " is invalid as rotation. All valid values lie in "
      << "the range from " << em::DisplayRotationDefaultProto::Rotation_MIN
      << " to " << em::DisplayRotationDefaultProto::Rotation_MAX << ".";
}

IN_PROC_BROWSER_TEST_P(DisplayRotationDefaultTest, FirstDisplay) {
  const display::Display::Rotation policy_rotation = GetParam();
  EXPECT_EQ(display::Display::ROTATE_0, GetRotationOfFirstDisplay())
      << "Initial primary rotation before policy";

  SetPolicy(policy_rotation);
  int settings_rotation;
  EXPECT_TRUE(chromeos::CrosSettings::Get()->GetInteger(
      chromeos::kDisplayRotationDefault, &settings_rotation));
  EXPECT_EQ(policy_rotation, settings_rotation)
      << "Value of CrosSettings after policy value changed";
  EXPECT_EQ(policy_rotation, GetRotationOfFirstDisplay())
      << "Rotation of primary display after policy";
}

IN_PROC_BROWSER_TEST_P(DisplayRotationDefaultTest, RefreshSecondDisplay) {
  const display::Display::Rotation policy_rotation = GetParam();
  ToggleSecondDisplay();
  EXPECT_EQ(display::Display::ROTATE_0, GetRotationOfSecondDisplay())
      << "Rotation of secondary display before policy";
  SetPolicy(policy_rotation);
  EXPECT_EQ(policy_rotation, GetRotationOfSecondDisplay())
      << "Rotation of already connected secondary display after policy";
}

IN_PROC_BROWSER_TEST_P(DisplayRotationDefaultTest, ConnectSecondDisplay) {
  const display::Display::Rotation policy_rotation = GetParam();
  SetPolicy(policy_rotation);
  ToggleSecondDisplay();
  EXPECT_EQ(policy_rotation, GetRotationOfFirstDisplay())
      << "Rotation of primary display after policy";
  EXPECT_EQ(policy_rotation, GetRotationOfSecondDisplay())
      << "Rotation of newly connected secondary display after policy";
}

IN_PROC_BROWSER_TEST_P(DisplayRotationDefaultTest, UserInteraction) {
  const display::Display::Rotation policy_rotation = GetParam();
  const display::Display::Rotation user_rotation = display::Display::ROTATE_90;
  GetDisplayManager()->SetDisplayRotation(
      GetDisplayManager()->first_display_id(), user_rotation,
      display::Display::RotationSource::USER);
  EXPECT_EQ(user_rotation, GetRotationOfFirstDisplay())
      << "Rotation of primary display after user change";
  SetPolicy(policy_rotation);
  EXPECT_EQ(policy_rotation, GetRotationOfFirstDisplay())
      << "Rotation of primary display after policy overrode user change";
  GetDisplayManager()->SetDisplayRotation(
      GetDisplayManager()->first_display_id(), user_rotation,
      display::Display::RotationSource::USER);
  EXPECT_EQ(user_rotation, GetRotationOfFirstDisplay())
      << "Rotation of primary display after user overrode policy change";
  SetADifferentPolicy();
  EXPECT_EQ(user_rotation, GetRotationOfFirstDisplay())
      << "Rotation of primary display after policy reloaded without change";
}

IN_PROC_BROWSER_TEST_P(DisplayRotationDefaultTest, SetAndUnsetPolicy) {
  const display::Display::Rotation policy_rotation = GetParam();
  SetPolicy(policy_rotation);
  UnsetPolicy();
  EXPECT_EQ(policy_rotation, GetRotationOfFirstDisplay())
      << "Rotation of primary display after policy was set and removed.";
}

IN_PROC_BROWSER_TEST_P(DisplayRotationDefaultTest,
                       SetAndUnsetPolicyWithUserInteraction) {
  const display::Display::Rotation policy_rotation = GetParam();
  const display::Display::Rotation user_rotation = display::Display::ROTATE_90;
  SetPolicy(policy_rotation);
  GetDisplayManager()->SetDisplayRotation(
      GetDisplayManager()->first_display_id(), user_rotation,
      display::Display::RotationSource::USER);
  UnsetPolicy();
  EXPECT_EQ(user_rotation, GetRotationOfFirstDisplay())
      << "Rotation of primary display after policy was set to "
      << policy_rotation << ", user changed the rotation to " << user_rotation
      << ", and policy was removed.";
}

INSTANTIATE_TEST_SUITE_P(PolicyDisplayRotationDefault,
                         DisplayRotationDefaultTest,
                         testing::Values(display::Display::ROTATE_0,
                                         display::Display::ROTATE_90,
                                         display::Display::ROTATE_180,
                                         display::Display::ROTATE_270));

// This class tests that the policy is reapplied after a reboot. To persist from
// PRE_Reboot to Reboot, the policy is inserted into a FakeSessionManagerClient.
// From there, it travels to DeviceSettingsProvider, whose UpdateFromService()
// method caches the policy (using device_settings_cache::Store()).
// In the main test, the FakeSessionManagerClient is not fully initialized.
// Thus, DeviceSettingsProvider falls back on the cached values (using
// device_settings_cache::Retrieve()).
class DisplayRotationBootTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<display::Display::Rotation> {
 protected:
  DisplayRotationBootTest() {
    device_state_.set_skip_initial_policy_setup(true);
  }
  ~DisplayRotationBootTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    // Override FakeSessionManagerClient. This will be shut down by the browser.
    chromeos::SessionManagerClient::InitializeFakeInMemory();
    ash::DisplayConfigurationController::DisableAnimatorForTest();
    MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  policy::DevicePolicyCrosTestHelper test_helper_;

  chromeos::DeviceStateMixin device_state_{
      &mixin_host_,
      chromeos::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};

 private:
  DISALLOW_COPY_AND_ASSIGN(DisplayRotationBootTest);
};

IN_PROC_BROWSER_TEST_P(DisplayRotationBootTest, PRE_Reboot) {
  const display::Display::Rotation policy_rotation = GetParam();
  const display::Display::Rotation user_rotation = display::Display::ROTATE_180;

  // Set policy.
  policy::DevicePolicyBuilder* const device_policy(
      test_helper_.device_policy());
  em::ChromeDeviceSettingsProto& proto(device_policy->payload());
  proto.mutable_display_rotation_default()->set_display_rotation_default(
      static_cast<em::DisplayRotationDefaultProto::Rotation>(policy_rotation));
  base::RunLoop run_loop;
  std::unique_ptr<chromeos::CrosSettings::ObserverSubscription> observer =
      chromeos::CrosSettings::Get()->AddSettingsObserver(
          chromeos::kDisplayRotationDefault, run_loop.QuitClosure());
  device_policy->SetDefaultSigningKey();
  device_policy->Build();
  chromeos::FakeSessionManagerClient::Get()->set_device_policy(
      device_policy->GetBlob());
  chromeos::FakeSessionManagerClient::Get()->OnPropertyChangeComplete(true);
  run_loop.Run();
  // Allow tasks posted by CrosSettings observers to complete:
  base::RunLoop().RunUntilIdle();

  // Check the display's rotation.
  display::DisplayManager* const display_manager = GetDisplayManager();
  const int64_t first_display_id = display_manager->first_display_id();
  const display::Display& first_display =
      display_manager->GetDisplayForId(first_display_id);
  EXPECT_EQ(policy_rotation, first_display.rotation());

  // Let the user rotate the display to a different orientation, to check that
  // the policy value is restored after reboot.
  display_manager->SetDisplayRotation(first_display_id, user_rotation,
                                      display::Display::RotationSource::USER);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(user_rotation, first_display.rotation());
}

IN_PROC_BROWSER_TEST_P(DisplayRotationBootTest, Reboot) {
  const display::Display::Rotation policy_rotation = GetParam();

  // Check that the policy rotation is restored.
  EXPECT_EQ(policy_rotation, GetRotationOfFirstDisplay());
}

INSTANTIATE_TEST_SUITE_P(PolicyDisplayRotationDefault,
                         DisplayRotationBootTest,
                         testing::Values(display::Display::ROTATE_0,
                                         display::Display::ROTATE_90,
                                         display::Display::ROTATE_180,
                                         display::Display::ROTATE_270));

}  // namespace policy
