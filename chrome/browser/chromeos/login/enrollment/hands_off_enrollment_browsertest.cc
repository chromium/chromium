// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_helper_mock.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/login_wizard.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/test/enrollment_helper_mixin.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/enrollment_status_chromeos.h"
#include "chrome/browser/ui/webui/chromeos/login/network_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/welcome_screen_handler.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace chromeos {

namespace {

constexpr char kDefaultNetworkServicePath[] = "/service/eth1";

}  // namespace

// Hands-off enrollment flow test.
class HandsOffEnrollmentTest : public MixinBasedInProcessBrowserTest {
 protected:
  HandsOffEnrollmentTest() {}
  ~HandsOffEnrollmentTest() override = default;

  // InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendArg(switches::kLoginManager);
    command_line->AppendSwitchASCII(
        switches::kEnterpriseEnableZeroTouchEnrollment, "hands-off");
  }

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    ShowLoginWizard(OobeScreen::SCREEN_TEST_NO_WINDOW);

    // Set official build so EULA screen is not skipped by default.
    branded_build_override_ = WizardController::ForceBrandedBuildForTesting();

    // Sets all network services into idle state to simulate disconnected state.
    NetworkStateHandler::NetworkStateList networks;
    NetworkHandler::Get()->network_state_handler()->GetNetworkListByType(
        NetworkTypePattern::Default(),
        true,   // configured_only
        false,  // visible_only,
        0,      // no limit to number of results
        &networks);
    ShillServiceClient::TestInterface* service =
        DBusThreadManager::Get()->GetShillServiceClient()->GetTestInterface();
    for (const auto* const network : networks) {
      service->SetServiceProperty(network->path(), shill::kStateProperty,
                                  base::Value(shill::kStateIdle));
    }
    base::RunLoop().RunUntilIdle();
  }

  // Simulates device being connected to the network.
  void SimulateNetworkConnected() {
    ShillServiceClient::TestInterface* service =
        DBusThreadManager::Get()->GetShillServiceClient()->GetTestInterface();
    service->SetServiceProperty(kDefaultNetworkServicePath,
                                shill::kStateProperty,
                                base::Value(shill::kStateOnline));
    base::RunLoop().RunUntilIdle();
  }

 protected:
  test::EnrollmentHelperMixin enrollment_helper_{&mixin_host_};
  std::unique_ptr<base::AutoReset<bool>> branded_build_override_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HandsOffEnrollmentTest);
};

IN_PROC_BROWSER_TEST_F(HandsOffEnrollmentTest, NetworkConnectionReady) {
  enrollment_helper_.ExpectEnrollmentMode(
      policy::EnrollmentConfig::MODE_ATTESTATION_LOCAL_FORCED);
  enrollment_helper_.ExpectAttestationEnrollmentSuccess();
  enrollment_helper_.DisableAttributePromptUpdate();
  enrollment_helper_.SetupClearAuth();

  SimulateNetworkConnected();

  WizardController::default_controller()->AdvanceToScreen(
      WelcomeView::kScreenId);

  OobeScreenWaiter(NetworkScreenView::kScreenId).Wait();

  OobeScreenWaiter(EnrollmentScreenView::kScreenId).Wait();

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(StartupUtils::IsOobeCompleted());
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
  EXPECT_TRUE(ExistingUserController::current_controller());
}

IN_PROC_BROWSER_TEST_F(HandsOffEnrollmentTest, WaitForNetworkConnection) {
  enrollment_helper_.ExpectEnrollmentMode(
      policy::EnrollmentConfig::MODE_ATTESTATION_LOCAL_FORCED);
  enrollment_helper_.ExpectAttestationEnrollmentSuccess();
  enrollment_helper_.DisableAttributePromptUpdate();
  enrollment_helper_.SetupClearAuth();
  WizardController::default_controller()->AdvanceToScreen(
      WelcomeView::kScreenId);

  OobeScreenWaiter(NetworkScreenView::kScreenId).Wait();

  SimulateNetworkConnected();

  OobeScreenWaiter(EnrollmentScreenView::kScreenId).Wait();

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(ExistingUserController::current_controller());
  EXPECT_TRUE(StartupUtils::IsOobeCompleted());
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
}

IN_PROC_BROWSER_TEST_F(HandsOffEnrollmentTest, EnrollmentError) {
  enrollment_helper_.SetupClearAuth();
  const policy::EnrollmentStatus enrollment_status =
      policy::EnrollmentStatus::ForRegistrationError(
          policy::DeviceManagementStatus::DM_STATUS_TEMPORARY_UNAVAILABLE);
  // With hands-off we expect that there will be a retry attempt, so
  // set up repeated expectations.
  enrollment_helper_.ExpectEnrollmentModeRepeated(
      policy::EnrollmentConfig::MODE_ATTESTATION_LOCAL_FORCED);
  enrollment_helper_.ExpectAttestationEnrollmentErrorRepeated(
      enrollment_status);

  SimulateNetworkConnected();

  WizardController::default_controller()->AdvanceToScreen(
      WelcomeView::kScreenId);

  OobeScreenWaiter screen_waiter(NetworkScreenView::kScreenId);
  // WebUI window is not visible until the screen animation finishes.
  screen_waiter.set_no_check_native_window_visible();
  screen_waiter.Wait();

  OobeScreenWaiter(EnrollmentScreenView::kScreenId).Wait();

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(
      EnrollmentScreenView::kScreenId.AsId(),
      WizardController::default_controller()->current_screen()->screen_id());
  EXPECT_FALSE(ExistingUserController::current_controller());
  EXPECT_FALSE(StartupUtils::IsOobeCompleted());
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
}

}  // namespace chromeos
