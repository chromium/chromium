// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_helper_mock.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/test/wizard_in_process_browser_test.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/enrollment_status_chromeos.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_service_client.h"
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
class HandsOffEnrollmentTest : public WizardInProcessBrowserTest {
 protected:
  HandsOffEnrollmentTest()
      : WizardInProcessBrowserTest(OobeScreen::SCREEN_TEST_NO_WINDOW) {}
  ~HandsOffEnrollmentTest() override = default;

  // WizardInProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    WizardInProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kEnterpriseEnableZeroTouchEnrollment, "hands-off");
  }

  void SetUpOnMainThread() override {
    WizardInProcessBrowserTest::SetUpOnMainThread();
    // Set official build so EULA screen is not skipped by default.
    WizardController::default_controller()->is_official_build_ = true;

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

  // Result of attestation based enrollment used by
  // EnterpriseEnrollmentHelperMock.
  enum class AttestationEnrollmentResult { SUCCESS, ERROR };

  // Helper method that mocks EnterpriseEnrollmentHelper for hands-off
  // enrollment. It simulates specified attestation based enrollment |result|.
  template <AttestationEnrollmentResult result>
  static EnterpriseEnrollmentHelper* MockEnrollmentHelperCreator(
      EnterpriseEnrollmentHelper::EnrollmentStatusConsumer* status_consumer,
      const policy::EnrollmentConfig& enrollment_config,
      const std::string& enrolling_user_domain) {
    EnterpriseEnrollmentHelperMock* mock =
        new EnterpriseEnrollmentHelperMock(status_consumer);
    if (result == AttestationEnrollmentResult::SUCCESS) {
      // Simulate successful attestation based enrollment.
      EXPECT_CALL(*mock, EnrollUsingAttestation())
          .Times(testing::AnyNumber())
          .WillRepeatedly(testing::Invoke(
              [mock]() { mock->status_consumer()->OnDeviceEnrolled(""); }));
      EXPECT_CALL(*mock, GetDeviceAttributeUpdatePermission())
          .Times(testing::AnyNumber())
          .WillRepeatedly(testing::Invoke([mock]() {
            mock->status_consumer()->OnDeviceAttributeUpdatePermission(false);
          }));
    } else {
      // Simulate error during attestation based enrollment.
      const policy::EnrollmentStatus enrollment_status =
          policy::EnrollmentStatus::ForRegistrationError(
              policy::DeviceManagementStatus::DM_STATUS_TEMPORARY_UNAVAILABLE);
      EXPECT_CALL(*mock, GetDeviceAttributeUpdatePermission())
          .Times(testing::AnyNumber())
          .WillRepeatedly(testing::Invoke([mock, enrollment_status]() {
            mock->status_consumer()->OnEnrollmentError(enrollment_status);
          }));
    }
    // Define behavior of ClearAuth to only run the callback it is given.
    EXPECT_CALL(*mock, ClearAuth(testing::_))
        .Times(testing::AnyNumber())
        .WillRepeatedly(testing::Invoke(
            [](const base::RepeatingClosure& callback) { callback.Run(); }));
    return mock;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(HandsOffEnrollmentTest);
};

IN_PROC_BROWSER_TEST_F(HandsOffEnrollmentTest, NetworkConnectionReady) {
  EnterpriseEnrollmentHelper::SetupEnrollmentHelperMock(
      &MockEnrollmentHelperCreator<AttestationEnrollmentResult::SUCCESS>);
  SimulateNetworkConnected();

  WizardController::default_controller()->AdvanceToScreen(
      OobeScreen::SCREEN_OOBE_WELCOME);

  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_NETWORK).Wait();

  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_ENROLLMENT).Wait();

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(StartupUtils::IsOobeCompleted());
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
  EXPECT_TRUE(ExistingUserController::current_controller());
}

IN_PROC_BROWSER_TEST_F(HandsOffEnrollmentTest, WaitForNetworkConnection) {
  EnterpriseEnrollmentHelper::SetupEnrollmentHelperMock(
      &MockEnrollmentHelperCreator<AttestationEnrollmentResult::SUCCESS>);

  WizardController::default_controller()->AdvanceToScreen(
      OobeScreen::SCREEN_OOBE_WELCOME);

  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_NETWORK).Wait();

  SimulateNetworkConnected();

  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_ENROLLMENT).Wait();

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(ExistingUserController::current_controller());
  EXPECT_TRUE(StartupUtils::IsOobeCompleted());
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
}

IN_PROC_BROWSER_TEST_F(HandsOffEnrollmentTest, EnrollmentError) {
  EnterpriseEnrollmentHelper::SetupEnrollmentHelperMock(
      &MockEnrollmentHelperCreator<AttestationEnrollmentResult::ERROR>);
  SimulateNetworkConnected();

  WizardController::default_controller()->AdvanceToScreen(
      OobeScreen::SCREEN_OOBE_WELCOME);

  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_NETWORK).Wait();

  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_ENROLLMENT).Wait();

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(
      OobeScreen::SCREEN_OOBE_ENROLLMENT,
      WizardController::default_controller()->current_screen()->screen_id());
  EXPECT_FALSE(ExistingUserController::current_controller());
  EXPECT_FALSE(StartupUtils::IsOobeCompleted());
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
}

}  // namespace chromeos
