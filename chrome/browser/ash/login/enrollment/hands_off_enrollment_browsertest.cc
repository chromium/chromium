// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/login/enrollment/enterprise_enrollment_helper_mock.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/login_wizard.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/test/enrollment_helper_mixin.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_status.h"
#include "chrome/browser/ui/webui/ash/login/network_screen_handler.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

namespace {

constexpr char kDefaultNetworkServicePath[] = "/service/eth1";

}  // namespace

// Hands-off enrollment flow test.
class HandsOffEnrollmentTest : public MixinBasedInProcessBrowserTest {
 public:
  HandsOffEnrollmentTest(const HandsOffEnrollmentTest&) = delete;
  HandsOffEnrollmentTest& operator=(const HandsOffEnrollmentTest&) = delete;

 protected:
  HandsOffEnrollmentTest() = default;
  ~HandsOffEnrollmentTest() override = default;

  // InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kEnterpriseEnableZeroTouchEnrollment, "hands-off");
    command_line->AppendSwitch(switches::kLoginManager);
    command_line->AppendSwitch(
        switches::kDisableOOBENetworkScreenSkippingForTesting);
  }

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();

    // Sets all network services into idle state to simulate disconnected state.
    NetworkStateHandler::NetworkStateList networks;
    NetworkHandler::Get()->network_state_handler()->GetNetworkListByType(
        NetworkTypePattern::Default(),
        true,   // configured_only
        false,  // visible_only,
        0,      // no limit to number of results
        &networks);
    ShillServiceClient::TestInterface* service =
        ShillServiceClient::Get()->GetTestInterface();
    for (const auto* const network : networks) {
      service->SetServiceProperty(network->path(), shill::kStateProperty,
                                  base::Value(shill::kStateIdle));
    }
    base::RunLoop().RunUntilIdle();
  }

  // Simulates device being connected to the network.
  void SimulateNetworkConnected() {
    ShillServiceClient::TestInterface* service =
        ShillServiceClient::Get()->GetTestInterface();
    service->SetServiceProperty(kDefaultNetworkServicePath,
                                shill::kStateProperty,
                                base::Value(shill::kStateOnline));
    base::RunLoop().RunUntilIdle();
  }

  // Set official build so EULA screen is not skipped by default.
  void ForceBrandedBuild() {
    LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build =
        true;
  }

 protected:
  test::EnrollmentHelperMixin enrollment_helper_{&mixin_host_};
};

// TODO(crbug.com/1303608): Test is flaky.
IN_PROC_BROWSER_TEST_F(HandsOffEnrollmentTest,
                       DISABLED_NetworkConnectionReady) {
  enrollment_helper_.ExpectEnrollmentMode(
      policy::EnrollmentConfig::MODE_ATTESTATION_LOCAL_FORCED);
  enrollment_helper_.ExpectAttestationEnrollmentSuccess();
  enrollment_helper_.DisableAttributePromptUpdate();
  enrollment_helper_.SetupClearAuth();

  SimulateNetworkConnected();

  ShowLoginWizard(OOBE_SCREEN_UNKNOWN);

  ForceBrandedBuild();

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
  ShowLoginWizard(OOBE_SCREEN_UNKNOWN);

  ForceBrandedBuild();

  OobeScreenWaiter(NetworkScreenView::kScreenId).Wait();

  SimulateNetworkConnected();

  OobeScreenWaiter(EnrollmentScreenView::kScreenId).Wait();

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(ExistingUserController::current_controller());
  EXPECT_TRUE(StartupUtils::IsOobeCompleted());
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
}

// TODO(crbug.com/1303608): Test is flaky.
IN_PROC_BROWSER_TEST_F(HandsOffEnrollmentTest, DISABLED_EnrollmentError) {
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

  ShowLoginWizard(OOBE_SCREEN_UNKNOWN);

  ForceBrandedBuild();

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

}  // namespace ash
