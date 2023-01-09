// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/display/display_rotation_default_handler.h"

#include <memory>

#include "ash/constants/ash_switches.h"
#include "ash/display/display_configuration_controller.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/policy/core/device_policy_builder.h"
#include "chrome/browser/ash/policy/display/device_display_cros_browser_test.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display_layout.h"
#include "ui/display/manager/display_manager.h"

namespace policy {

namespace em = ::enterprise_management;

class DisplayRotationDefaultTest
    : public DeviceDisplayPolicyCrosBrowserTest,
      public testing::WithParamInterface<display::Display::Rotation> {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(ash::switches::kLoginManager);
    command_line->AppendSwitch(ash::switches::kForceLoginManagerInTests);
  }

  void SetRotationPolicy(int rotation) {
    em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
    proto.mutable_display_rotation_default()->set_display_rotation_default(
        static_cast<em::DisplayRotationDefaultProto::Rotation>(rotation));
    policy_helper()->RefreshPolicyAndWaitUntilDeviceSettingsUpdated(
        {ash::kDisplayRotationDefault, ash::kSystemUse24HourClock});
  }

  void RetriggerRotationPolicy() {
    em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
    const bool clock24 = proto.use_24hour_clock().use_24hour_clock();
    proto.mutable_use_24hour_clock()->set_use_24hour_clock(!clock24);
    policy_helper()->RefreshPolicyAndWaitUntilDeviceSettingsUpdated(
        {ash::kDisplayRotationDefault, ash::kSystemUse24HourClock});
  }
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
  EXPECT_EQ(display::Display::ROTATE_0,
            display_helper()->GetRotationOfFirstDisplay())
      << "Initial primary rotation before policy";

  SetRotationPolicy(policy_rotation);
  int settings_rotation;
  EXPECT_TRUE(ash::CrosSettings::Get()->GetInteger(ash::kDisplayRotationDefault,
                                                   &settings_rotation));
  EXPECT_EQ(policy_rotation, settings_rotation)
      << "Value of CrosSettings after policy value changed";
  EXPECT_EQ(policy_rotation, display_helper()->GetRotationOfFirstDisplay())
      << "Rotation of primary display after policy";
}

IN_PROC_BROWSER_TEST_P(DisplayRotationDefaultTest, RefreshSecondDisplay) {
  const display::Display::Rotation policy_rotation = GetParam();
  display_helper()->ToggleSecondDisplay();
  EXPECT_EQ(display::Display::ROTATE_0,
            display_helper()->GetRotationOfSecondDisplay())
      << "Rotation of secondary display before policy";
  SetRotationPolicy(policy_rotation);
  EXPECT_EQ(policy_rotation, display_helper()->GetRotationOfSecondDisplay())
      << "Rotation of already connected secondary display after policy";
}

IN_PROC_BROWSER_TEST_P(DisplayRotationDefaultTest, ConnectSecondDisplay) {
  const display::Display::Rotation policy_rotation = GetParam();
  SetRotationPolicy(policy_rotation);
  display_helper()->ToggleSecondDisplay();
  EXPECT_EQ(policy_rotation, display_helper()->GetRotationOfFirstDisplay())
      << "Rotation of primary display after policy";
  EXPECT_EQ(policy_rotation, display_helper()->GetRotationOfSecondDisplay())
      << "Rotation of newly connected secondary display after policy";
}

// This test is needed to test that refreshing the settings without change to
// the DisplayRotationDefault policy will not rotate the display again because
// it was changed by the user.
IN_PROC_BROWSER_TEST_P(DisplayRotationDefaultTest, UserInteraction) {
  const display::Display::Rotation policy_rotation = GetParam();
  const display::Display::Rotation user_rotation = display::Display::ROTATE_90;
  display_helper()->GetDisplayManager()->SetDisplayRotation(
      display_helper()->GetDisplayManager()->first_display_id(), user_rotation,
      display::Display::RotationSource::USER);
  EXPECT_EQ(user_rotation, display_helper()->GetRotationOfFirstDisplay())
      << "Rotation of primary display after user change";
  SetRotationPolicy(policy_rotation);
  EXPECT_EQ(policy_rotation, display_helper()->GetRotationOfFirstDisplay())
      << "Rotation of primary display after policy overrode user change";
  display_helper()->GetDisplayManager()->SetDisplayRotation(
      display_helper()->GetDisplayManager()->first_display_id(), user_rotation,
      display::Display::RotationSource::USER);
  EXPECT_EQ(user_rotation, display_helper()->GetRotationOfFirstDisplay())
      << "Rotation of primary display after user overrode policy change";
  RetriggerRotationPolicy();
  EXPECT_EQ(user_rotation, display_helper()->GetRotationOfFirstDisplay())
      << "Rotation of primary display after policy reloaded without change";
}

IN_PROC_BROWSER_TEST_P(DisplayRotationDefaultTest, SetAndUnsetPolicy) {
  const display::Display::Rotation policy_rotation = GetParam();
  SetRotationPolicy(policy_rotation);
  policy_helper()->UnsetPolicy(
      {ash::kDisplayRotationDefault, ash::kSystemUse24HourClock});
  EXPECT_EQ(policy_rotation, display_helper()->GetRotationOfFirstDisplay())
      << "Rotation of primary display after policy was set and removed.";
}

IN_PROC_BROWSER_TEST_P(DisplayRotationDefaultTest,
                       SetAndUnsetPolicyWithUserInteraction) {
  const display::Display::Rotation policy_rotation = GetParam();
  const display::Display::Rotation user_rotation = display::Display::ROTATE_90;
  SetRotationPolicy(policy_rotation);
  display_helper()->GetDisplayManager()->SetDisplayRotation(
      display_helper()->GetDisplayManager()->first_display_id(), user_rotation,
      display::Display::RotationSource::USER);
  policy_helper()->UnsetPolicy(
      {ash::kDisplayRotationDefault, ash::kSystemUse24HourClock});
  EXPECT_EQ(user_rotation, display_helper()->GetRotationOfFirstDisplay())
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
 public:
  DisplayRotationBootTest(const DisplayRotationBootTest&) = delete;
  DisplayRotationBootTest& operator=(const DisplayRotationBootTest&) = delete;

 protected:
  DisplayRotationBootTest() {
    device_state_.set_skip_initial_policy_setup(true);
  }
  ~DisplayRotationBootTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    ash::SessionManagerClient::InitializeFakeInMemory();
    ash::DisplayConfigurationController::DisableAnimatorForTest();
    MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  ash::DeviceStateMixin device_state_{
      &mixin_host_,
      ash::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};

  DevicePolicyCrosTestHelper* policy_helper() { return &policy_helper_; }
  DeviceDisplayCrosTestHelper* display_helper() { return &display_helper_; }

 private:
  DevicePolicyCrosTestHelper policy_helper_;
  DeviceDisplayCrosTestHelper display_helper_;
};

IN_PROC_BROWSER_TEST_P(DisplayRotationBootTest, PRE_Reboot) {
  const display::Display::Rotation policy_rotation = GetParam();
  const display::Display::Rotation user_rotation = display::Display::ROTATE_180;

  // Set policy.
  DevicePolicyBuilder* const device_policy(policy_helper()->device_policy());
  em::ChromeDeviceSettingsProto& proto(device_policy->payload());
  proto.mutable_display_rotation_default()->set_display_rotation_default(
      static_cast<em::DisplayRotationDefaultProto::Rotation>(policy_rotation));
  base::RunLoop run_loop;
  base::CallbackListSubscription subscription =
      ash::CrosSettings::Get()->AddSettingsObserver(
          ash::kDisplayRotationDefault, run_loop.QuitClosure());
  device_policy->SetDefaultSigningKey();
  device_policy->Build();
  ash::FakeSessionManagerClient::Get()->set_device_policy(
      device_policy->GetBlob());
  ash::FakeSessionManagerClient::Get()->OnPropertyChangeComplete(true);
  run_loop.Run();
  // Allow tasks posted by CrosSettings observers to complete:
  base::RunLoop().RunUntilIdle();

  // Check the display's rotation.
  EXPECT_EQ(policy_rotation, display_helper()->GetRotationOfFirstDisplay());

  // Let the user rotate the display to a different orientation, to check that
  // the policy value is restored after reboot.
  display_helper()->GetDisplayManager()->SetDisplayRotation(
      display_helper()->GetFirstDisplayId(), user_rotation,
      display::Display::RotationSource::USER);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(user_rotation, display_helper()->GetRotationOfFirstDisplay());
}

IN_PROC_BROWSER_TEST_P(DisplayRotationBootTest, Reboot) {
  const display::Display::Rotation policy_rotation = GetParam();

  // Check that the policy rotation is restored.
  EXPECT_EQ(policy_rotation, display_helper()->GetRotationOfFirstDisplay());
}

INSTANTIATE_TEST_SUITE_P(PolicyDisplayRotationDefault,
                         DisplayRotationBootTest,
                         testing::Values(display::Display::ROTATE_0,
                                         display::Display::ROTATE_90,
                                         display::Display::ROTATE_180,
                                         display::Display::ROTATE_270));

}  // namespace policy
