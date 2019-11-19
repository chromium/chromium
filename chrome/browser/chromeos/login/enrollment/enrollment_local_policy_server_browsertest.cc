// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/login_screen_test_api.h"
#include "base/bind.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/enrollment/auto_enrollment_check_screen.h"
#include "chrome/browser/chromeos/login/enrollment/enrollment_screen.h"
#include "chrome/browser/chromeos/login/enrollment/enrollment_screen_view.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/test/device_state_mixin.h"
#include "chrome/browser/chromeos/login/test/enrollment_ui_mixin.h"
#include "chrome/browser/chromeos/login/test/fake_gaia_mixin.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/local_policy_test_server_mixin.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/test/oobe_screens_utils.h"
#include "chrome/browser/chromeos/login/test/test_condition_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/server_backed_state_keys_broker.h"
#include "chrome/browser/ui/webui/chromeos/login/device_disabled_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/attestation/mock_attestation_flow.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "chromeos/dbus/cryptohome/fake_cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "chromeos/tpm/install_attributes.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/test_support/local_policy_test_server.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/test_utils.h"

namespace chromeos {

namespace {

std::string GetDmTokenFromPolicy(const std::string& blob) {
  enterprise_management::PolicyFetchResponse policy;
  CHECK(policy.ParseFromString(blob));

  enterprise_management::PolicyData policy_data;
  CHECK(policy_data.ParseFromString(policy.policy_data()));
  return policy_data.request_token();
}

}  // namespace

class EnrollmentLocalPolicyServerBase : public OobeBaseTest {
 public:
  EnrollmentLocalPolicyServerBase() = default;

  void SetUpOnMainThread() override {
    fake_gaia_.SetupFakeGaiaForLogin(FakeGaiaMixin::kFakeUserEmail,
                                     FakeGaiaMixin::kFakeUserGaiaId,
                                     FakeGaiaMixin::kFakeRefreshToken);
    policy_server_.SetUpdateDeviceAttributesPermission(false);
    OobeBaseTest::SetUpOnMainThread();
  }

 protected:
  LoginDisplayHost* host() {
    LoginDisplayHost* host = LoginDisplayHost::default_host();
    EXPECT_NE(host, nullptr);
    return host;
  }

  EnrollmentScreen* enrollment_screen() {
    EXPECT_NE(WizardController::default_controller(), nullptr);
    EnrollmentScreen* enrollment_screen = EnrollmentScreen::Get(
        WizardController::default_controller()->screen_manager());
    EXPECT_NE(enrollment_screen, nullptr);
    return enrollment_screen;
  }

  AutoEnrollmentCheckScreen* auto_enrollment_screen() {
    EXPECT_NE(WizardController::default_controller(), nullptr);
    AutoEnrollmentCheckScreen* auto_enrollment_screen =
        AutoEnrollmentCheckScreen::Get(
            WizardController::default_controller()->screen_manager());
    EXPECT_NE(auto_enrollment_screen, nullptr);
    return auto_enrollment_screen;
  }

  void TriggerEnrollmentAndSignInSuccessfully() {
    host()->StartWizard(EnrollmentScreenView::kScreenId);
    OobeScreenWaiter(EnrollmentScreenView::kScreenId).Wait();

    ASSERT_FALSE(StartupUtils::IsDeviceRegistered());
    ASSERT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
    enrollment_screen()->OnLoginDone(FakeGaiaMixin::kFakeUserEmail,
                                     FakeGaiaMixin::kFakeAuthCode);
  }

  LocalPolicyTestServerMixin policy_server_{&mixin_host_};
  test::EnrollmentUIMixin enrollment_ui_{&mixin_host_};
  FakeGaiaMixin fake_gaia_{&mixin_host_, embedded_test_server()};
  DeviceStateMixin device_state_{&mixin_host_,
                                 DeviceStateMixin::State::BEFORE_OOBE};

 private:
  DISALLOW_COPY_AND_ASSIGN(EnrollmentLocalPolicyServerBase);
};

class AutoEnrollmentLocalPolicyServer : public EnrollmentLocalPolicyServerBase {
 public:
  AutoEnrollmentLocalPolicyServer() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    EnrollmentLocalPolicyServerBase::SetUpCommandLine(command_line);

    command_line->AppendSwitchASCII(
        switches::kEnterpriseEnableForcedReEnrollment,
        AutoEnrollmentController::kForcedReEnrollmentAlways);
    command_line->AppendSwitchASCII(
        switches::kEnterpriseEnrollmentInitialModulus, "5");
    command_line->AppendSwitchASCII(switches::kEnterpriseEnrollmentModulusLimit,
                                    "5");
  }

  policy::ServerBackedStateKeysBroker* state_keys_broker() {
    return g_browser_process->platform_part()
        ->browser_policy_connector_chromeos()
        ->GetStateKeysBroker();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AutoEnrollmentLocalPolicyServer);
};

class AutoEnrollmentWithStatistics : public AutoEnrollmentLocalPolicyServer {
 public:
  AutoEnrollmentWithStatistics() : AutoEnrollmentLocalPolicyServer() {
    // AutoEnrollmentController assumes that VPD is in valid state if
    // "serial_number" or "Product_S/N" could be read from it.
    fake_statistics_provider_.SetMachineStatistic(
        system::kSerialNumberKeyForTest, test::kTestSerialNumber);
  }

  ~AutoEnrollmentWithStatistics() override = default;

 protected:
  void SetFRERequiredKey(const std::string& value) {
    fake_statistics_provider_.SetMachineStatistic(system::kCheckEnrollmentKey,
                                                  value);
  }

  void SetActivateDate(const std::string& value) {
    fake_statistics_provider_.SetMachineStatistic(system::kActivateDateKey,
                                                  value);
  }

  void SetVPDCorrupted() {
    fake_statistics_provider_.ClearMachineStatistic(
        system::kSerialNumberKeyForTest);
  }

 private:
  system::ScopedFakeStatisticsProvider fake_statistics_provider_;
  DISALLOW_COPY_AND_ASSIGN(AutoEnrollmentWithStatistics);
};

class AutoEnrollmentNoStateKeys : public AutoEnrollmentWithStatistics {
 public:
  AutoEnrollmentNoStateKeys() = default;
  ~AutoEnrollmentNoStateKeys() override = default;

  // AutoEnrollmentWithStatistics:
  void SetUpInProcessBrowserTestFixture() override {
    AutoEnrollmentWithStatistics::SetUpInProcessBrowserTestFixture();
    // Session manager client is initialized by DeviceStateMixin.
    FakeSessionManagerClient::Get()->set_force_state_keys_missing(true);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AutoEnrollmentNoStateKeys);
};

class InitialEnrollmentTest : public EnrollmentLocalPolicyServerBase {
 public:
  InitialEnrollmentTest() {
    policy_server_.ConfigureFakeStatisticsForZeroTouch(
        &fake_statistics_provider_);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    EnrollmentLocalPolicyServerBase::SetUpCommandLine(command_line);

    command_line->AppendSwitchASCII(
        switches::kEnterpriseEnableInitialEnrollment,
        AutoEnrollmentController::kInitialEnrollmentAlways);
  }

 private:
  system::ScopedFakeStatisticsProvider fake_statistics_provider_;
  DISALLOW_COPY_AND_ASSIGN(InitialEnrollmentTest);
};

// Simple manual enrollment.
IN_PROC_BROWSER_TEST_F(EnrollmentLocalPolicyServerBase, ManualEnrollment) {
  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSuccess);
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
  EXPECT_TRUE(InstallAttributes::Get()->IsCloudManaged());
}

// Simple manual enrollment with device attributes prompt.
IN_PROC_BROWSER_TEST_F(EnrollmentLocalPolicyServerBase,
                       ManualEnrollmentWithDeviceAttributes) {
  policy_server_.SetUpdateDeviceAttributesPermission(true);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepDeviceAttributes);
  enrollment_ui_.SubmitDeviceAttributes(test::values::kAssetId,
                                        test::values::kLocation);
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSuccess);
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
  EXPECT_TRUE(InstallAttributes::Get()->IsCloudManaged());
}

// Simple manual enrollment with only license type available.
// Client should automatically select the only available license type,
// so no license selection UI should be displayed.
IN_PROC_BROWSER_TEST_F(EnrollmentLocalPolicyServerBase,
                       ManualEnrollmentWithSingleLicense) {
  policy_server_.ExpectAvailableLicenseCount(5 /* perpetual */, 0 /* annual */,
                                             0 /* kiosk */);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSuccess);
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
  EXPECT_TRUE(InstallAttributes::Get()->IsCloudManaged());
}

// Simple manual enrollment with license selection.
// Enrollment selection UI should be displayed during enrollment.
IN_PROC_BROWSER_TEST_F(EnrollmentLocalPolicyServerBase,
                       ManualEnrollmentWithMultipleLicenses) {
  policy_server_.ExpectAvailableLicenseCount(5 /* perpetual */, 5 /* annual */,
                                             5 /* kiosk */);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepLicenses);
  enrollment_ui_.SelectEnrollmentLicense(test::values::kLicenseTypeAnnual);
  enrollment_ui_.UseSelectedLicense();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSuccess);
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
  EXPECT_TRUE(InstallAttributes::Get()->IsCloudManaged());
}

// Negative scenarios: see different HTTP error codes in
// device_management_service.cc

// Error during enrollment : 402 - missing licenses.
IN_PROC_BROWSER_TEST_F(EnrollmentLocalPolicyServerBase,
                       EnrollmentErrorNoLicenses) {
  policy_server_.SetExpectedDeviceEnrollmentError(402);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_ui_.ExpectErrorMessage(
      IDS_ENTERPRISE_ENROLLMENT_MISSING_LICENSES_ERROR, /* can retry */ true);
  enrollment_ui_.RetryAfterError();
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
}

// Error during enrollment : 403 - management not allowed.
IN_PROC_BROWSER_TEST_F(EnrollmentLocalPolicyServerBase,
                       EnrollmentErrorManagementNotAllowed) {
  policy_server_.SetExpectedDeviceEnrollmentError(403);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_ui_.ExpectErrorMessage(
      IDS_ENTERPRISE_ENROLLMENT_AUTH_ACCOUNT_ERROR, /* can retry */ true);
  enrollment_ui_.RetryAfterError();
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
}

// Error during enrollment : 405 - invalid device serial.
IN_PROC_BROWSER_TEST_F(EnrollmentLocalPolicyServerBase,
                       EnrollmentErrorInvalidDeviceSerial) {
  policy_server_.SetExpectedDeviceEnrollmentError(405);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  // TODO (antrim, rsorokin): find out why it makes sense to retry here?
  enrollment_ui_.ExpectErrorMessage(
      IDS_POLICY_DM_STATUS_SERVICE_INVALID_SERIAL_NUMBER,
      /* can retry */ true);
  enrollment_ui_.RetryAfterError();
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
}

// Error during enrollment : 406 - domain mismatch
IN_PROC_BROWSER_TEST_F(EnrollmentLocalPolicyServerBase,
                       EnrollmentErrorDomainMismatch) {
  policy_server_.SetExpectedDeviceEnrollmentError(406);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_ui_.ExpectErrorMessage(
      IDS_ENTERPRISE_ENROLLMENT_DOMAIN_MISMATCH_ERROR, /* can retry */ true);
  enrollment_ui_.RetryAfterError();
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
}

// Error during enrollment : 409 - Device ID is already in use
IN_PROC_BROWSER_TEST_F(EnrollmentLocalPolicyServerBase,
                       EnrollmentErrorDeviceIDConflict) {
  policy_server_.SetExpectedDeviceEnrollmentError(409);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  // TODO (antrim, rsorokin): find out why it makes sense to retry here?
  enrollment_ui_.ExpectErrorMessage(
      IDS_POLICY_DM_STATUS_SERVICE_DEVICE_ID_CONFLICT, /* can retry */ true);
  enrollment_ui_.RetryAfterError();
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
}

// Error during enrollment : 412 - Activation is pending
IN_PROC_BROWSER_TEST_F(EnrollmentLocalPolicyServerBase,
                       EnrollmentErrorActivationIsPending) {
  policy_server_.SetExpectedDeviceEnrollmentError(412);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_ui_.ExpectErrorMessage(
      IDS_POLICY_DM_STATUS_SERVICE_ACTIVATION_PENDING, /* can retry */ true);
  enrollment_ui_.RetryAfterError();
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
}

// Error during enrollment : 417 - Consumer account with packaged license.
IN_PROC_BROWSER_TEST_F(EnrollmentLocalPolicyServerBase,
                       EnrollmentErrorConsumerAccountWithPackagedLicense) {
  policy_server_.SetExpectedDeviceEnrollmentError(417);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_ui_.ExpectErrorMessage(
      IDS_ENTERPRISE_ENROLLMENT_CONSUMER_ACCOUNT_WITH_PACKAGED_LICENSE,
      /* can retry */ true);
  enrollment_ui_.RetryAfterError();
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
}

// Error during enrollment : 500 - Consumer account with packaged license.
IN_PROC_BROWSER_TEST_F(EnrollmentLocalPolicyServerBase,
                       EnrollmentErrorServerError) {
  policy_server_.SetExpectedDeviceEnrollmentError(500);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_ui_.ExpectErrorMessage(IDS_POLICY_DM_STATUS_TEMPORARY_UNAVAILABLE,
                                    /* can retry */ true);
  enrollment_ui_.RetryAfterError();
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
}

// Error during enrollment : Strange HTTP response from server.
IN_PROC_BROWSER_TEST_F(EnrollmentLocalPolicyServerBase,
                       EnrollmentErrorServerIsDrunk) {
  policy_server_.SetExpectedDeviceEnrollmentError(12345);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_ui_.ExpectErrorMessage(IDS_POLICY_DM_STATUS_HTTP_STATUS_ERROR,
                                    /* can retry */ true);
  enrollment_ui_.RetryAfterError();
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
}

// Error during enrollment : Can not update device attributes
IN_PROC_BROWSER_TEST_F(EnrollmentLocalPolicyServerBase,
                       EnrollmentErrorUploadingDeviceAttributes) {
  policy_server_.SetUpdateDeviceAttributesPermission(true);
  policy_server_.SetExpectedDeviceAttributeUpdateError(500);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepDeviceAttributes);
  enrollment_ui_.SubmitDeviceAttributes(test::values::kAssetId,
                                        test::values::kLocation);

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
  EXPECT_TRUE(InstallAttributes::Get()->IsCloudManaged());
  enrollment_ui_.LeaveDeviceAttributeErrorScreen();
  OobeScreenWaiter(GaiaView::kScreenId).Wait();
}

// Error during enrollment : Error fetching policy : 500 server error.
IN_PROC_BROWSER_TEST_F(EnrollmentLocalPolicyServerBase,
                       EnrollmentErrorFetchingPolicyTransient) {
  policy_server_.SetExpectedPolicyFetchError(500);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_ui_.ExpectErrorMessage(IDS_POLICY_DM_STATUS_TEMPORARY_UNAVAILABLE,
                                    /* can retry */ true);
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
  enrollment_ui_.RetryAfterError();
}

// Error during enrollment : Error fetching policy : 902 - policy not found.
IN_PROC_BROWSER_TEST_F(EnrollmentLocalPolicyServerBase,
                       EnrollmentErrorFetchingPolicyNotFound) {
  policy_server_.SetExpectedPolicyFetchError(902);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_ui_.ExpectErrorMessage(
      IDS_POLICY_DM_STATUS_SERVICE_POLICY_NOT_FOUND,
      /* can retry */ true);
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
  enrollment_ui_.RetryAfterError();
}

// Error during enrollment : Error fetching policy : 903 - deprovisioned.
IN_PROC_BROWSER_TEST_F(EnrollmentLocalPolicyServerBase,
                       EnrollmentErrorFetchingPolicyDeprovisioned) {
  policy_server_.SetExpectedPolicyFetchError(903);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_ui_.ExpectErrorMessage(IDS_POLICY_DM_STATUS_SERVICE_DEPROVISIONED,
                                    /* can retry */ true);
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
  enrollment_ui_.RetryAfterError();
}

// No state keys on the server. Auto enrollment check should proceed to login.
IN_PROC_BROWSER_TEST_F(AutoEnrollmentLocalPolicyServer, AutoEnrollmentCheck) {
  host()->StartWizard(AutoEnrollmentCheckScreenView::kScreenId);
  OobeScreenWaiter(GaiaView::kScreenId).Wait();
}

// State keys are present but restore mode is not requested.
IN_PROC_BROWSER_TEST_F(AutoEnrollmentLocalPolicyServer, ReenrollmentNone) {
  EXPECT_TRUE(policy_server_.SetDeviceStateRetrievalResponse(
      state_keys_broker(),
      enterprise_management::DeviceStateRetrievalResponse::RESTORE_MODE_NONE,
      test::kTestDomain));
  host()->StartWizard(AutoEnrollmentCheckScreenView::kScreenId);
  OobeScreenWaiter(GaiaView::kScreenId).Wait();
}

// Reenrollment requested. User can skip.
IN_PROC_BROWSER_TEST_F(AutoEnrollmentLocalPolicyServer, ReenrollmentRequested) {
  EXPECT_TRUE(policy_server_.SetDeviceStateRetrievalResponse(
      state_keys_broker(),
      enterprise_management::DeviceStateRetrievalResponse::
          RESTORE_MODE_REENROLLMENT_REQUESTED,
      test::kTestDomain));
  host()->StartWizard(AutoEnrollmentCheckScreenView::kScreenId);
  OobeScreenWaiter(EnrollmentScreenView::kScreenId).Wait();
  enrollment_screen()->OnCancel();
  OobeScreenWaiter(GaiaView::kScreenId).Wait();
}

// Reenrollment forced. User can not skip.
IN_PROC_BROWSER_TEST_F(AutoEnrollmentLocalPolicyServer, ReenrollmentForced) {
  EXPECT_TRUE(policy_server_.SetDeviceStateRetrievalResponse(
      state_keys_broker(),
      enterprise_management::DeviceStateRetrievalResponse::
          RESTORE_MODE_REENROLLMENT_ENFORCED,
      test::kTestDomain));
  host()->StartWizard(AutoEnrollmentCheckScreenView::kScreenId);
  OobeScreenWaiter(EnrollmentScreenView::kScreenId).Wait();
  enrollment_ui_.SetExitHandler();
  enrollment_screen()->OnCancel();
  EXPECT_EQ(EnrollmentScreen::Result::BACK, enrollment_ui_.WaitForScreenExit());
}

// Device is disabled.
IN_PROC_BROWSER_TEST_F(AutoEnrollmentLocalPolicyServer, DeviceDisabled) {
  EXPECT_TRUE(policy_server_.SetDeviceStateRetrievalResponse(
      state_keys_broker(),
      enterprise_management::DeviceStateRetrievalResponse::
          RESTORE_MODE_DISABLED,
      test::kTestDomain));
  host()->StartWizard(AutoEnrollmentCheckScreenView::kScreenId);
  OobeScreenWaiter(DeviceDisabledScreenView::kScreenId).Wait();
}

// Attestation enrollment.
IN_PROC_BROWSER_TEST_F(AutoEnrollmentLocalPolicyServer, Attestation) {
  policy_server_.SetFakeAttestationFlow();
  EXPECT_TRUE(policy_server_.SetDeviceStateRetrievalResponse(
      state_keys_broker(),
      enterprise_management::DeviceStateRetrievalResponse::
          RESTORE_MODE_REENROLLMENT_ZERO_TOUCH,
      test::kTestDomain));

  host()->StartWizard(AutoEnrollmentCheckScreenView::kScreenId);
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSuccess);
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
  EXPECT_TRUE(InstallAttributes::Get()->IsCloudManaged());
}

// FRE explicitly required in VPD, but the state keys are missing.
IN_PROC_BROWSER_TEST_F(AutoEnrollmentNoStateKeys, FREExplicitlyRequired) {
  SetFRERequiredKey("1");
  host()->StartWizard(AutoEnrollmentCheckScreenView::kScreenId);
  OobeScreenWaiter(AutoEnrollmentCheckScreenView::kScreenId).Wait();

  OobeScreenWaiter(ErrorScreenView::kScreenId).Wait();
  test::OobeJS().ExpectHasNoClass("allow-guest-signin", {"error-message"});
}

// FRE not explicitly required and the state keys are missing. Should proceed to
// normal signin.
IN_PROC_BROWSER_TEST_F(AutoEnrollmentNoStateKeys, NotRequired) {
  host()->StartWizard(AutoEnrollmentCheckScreenView::kScreenId);
  OobeScreenWaiter(GaiaView::kScreenId).Wait();
}

// FRE explicitly not required in VPD, so it should not even contact the policy
// server.
IN_PROC_BROWSER_TEST_F(AutoEnrollmentWithStatistics, ExplicitlyNotRequired) {
  SetFRERequiredKey("0");

  // Should be ignored.
  EXPECT_TRUE(policy_server_.SetDeviceStateRetrievalResponse(
      state_keys_broker(),
      enterprise_management::DeviceStateRetrievalResponse::
          RESTORE_MODE_REENROLLMENT_ENFORCED,
      test::kTestDomain));

  host()->StartWizard(AutoEnrollmentCheckScreenView::kScreenId);
  OobeScreenWaiter(GaiaView::kScreenId).Wait();
}

// FRE is not required when VPD is valid and activate date is not there.
IN_PROC_BROWSER_TEST_F(AutoEnrollmentWithStatistics, MachineNotActivated) {
  // Should be ignored.
  EXPECT_TRUE(policy_server_.SetDeviceStateRetrievalResponse(
      state_keys_broker(),
      enterprise_management::DeviceStateRetrievalResponse::
          RESTORE_MODE_REENROLLMENT_ENFORCED,
      test::kTestDomain));

  host()->StartWizard(AutoEnrollmentCheckScreenView::kScreenId);
  OobeScreenWaiter(GaiaView::kScreenId).Wait();
}

// FRE is required when VPD is valid and activate date is there.
IN_PROC_BROWSER_TEST_F(AutoEnrollmentWithStatistics, MachineActivated) {
  SetActivateDate("1970-01");

  EXPECT_TRUE(policy_server_.SetDeviceStateRetrievalResponse(
      state_keys_broker(),
      enterprise_management::DeviceStateRetrievalResponse::
          RESTORE_MODE_REENROLLMENT_ENFORCED,
      test::kTestDomain));

  host()->StartWizard(AutoEnrollmentCheckScreenView::kScreenId);
  OobeScreenWaiter(EnrollmentScreenView::kScreenId).Wait();
}

// FRE is required when VPD in invalid state.
IN_PROC_BROWSER_TEST_F(AutoEnrollmentWithStatistics, CorruptedVPD) {
  SetVPDCorrupted();

  EXPECT_TRUE(policy_server_.SetDeviceStateRetrievalResponse(
      state_keys_broker(),
      enterprise_management::DeviceStateRetrievalResponse::
          RESTORE_MODE_REENROLLMENT_ENFORCED,
      test::kTestDomain));

  host()->StartWizard(AutoEnrollmentCheckScreenView::kScreenId);
  OobeScreenWaiter(EnrollmentScreenView::kScreenId).Wait();
}

class EnrollmentRecoveryTest : public EnrollmentLocalPolicyServerBase {
 public:
  EnrollmentRecoveryTest() : EnrollmentLocalPolicyServerBase() {
    device_state_.SetState(
        DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED);
  }

  ~EnrollmentRecoveryTest() override = default;

 protected:
  void SetUpInProcessBrowserTestFixture() override {
    EnrollmentLocalPolicyServerBase::SetUpInProcessBrowserTestFixture();

    // This triggers recovery enrollment.
    device_state_.RequestDevicePolicyUpdate()->policy_data()->Clear();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(EnrollmentRecoveryTest);
};

// Consistently timing out on Linux. http://crbug.com/1025220
#if defined(OS_LINUX)
#define MAYBE_Success DISABLED_Success
#else
#define MAYBE_Success Success
#endif
IN_PROC_BROWSER_TEST_F(EnrollmentRecoveryTest, MAYBE_Success) {
  test::SkipToEnrollmentOnRecovery();

  ASSERT_TRUE(StartupUtils::IsDeviceRegistered());
  ASSERT_TRUE(InstallAttributes::Get()->IsEnterpriseManaged());
  // No DM Token
  ASSERT_TRUE(
      GetDmTokenFromPolicy(FakeSessionManagerClient::Get()->device_policy())
          .empty());

  // User can't skip.
  enrollment_ui_.SetExitHandler();
  enrollment_screen()->OnCancel();
  EXPECT_EQ(EnrollmentScreen::Result::BACK, enrollment_ui_.WaitForScreenExit());

  enrollment_screen()->OnLoginDone(FakeGaiaMixin::kEnterpriseUser1,
                                   FakeGaiaMixin::kFakeAuthCode);
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSuccess);

  // DM Token is in the device policy.
  EXPECT_FALSE(
      GetDmTokenFromPolicy(FakeSessionManagerClient::Get()->device_policy())
          .empty());
}

// TODO(1024176): Consistently timing out.
IN_PROC_BROWSER_TEST_F(EnrollmentRecoveryTest, DISABLED_DifferentDomain) {
  test::SkipToEnrollmentOnRecovery();

  ASSERT_TRUE(StartupUtils::IsDeviceRegistered());
  ASSERT_TRUE(InstallAttributes::Get()->IsEnterpriseManaged());
  enrollment_screen()->OnLoginDone(FakeGaiaMixin::kFakeUserEmail,
                                   FakeGaiaMixin::kFakeAuthCode);
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_ui_.ExpectErrorMessage(
      IDS_ENTERPRISE_ENROLLMENT_STATUS_LOCK_WRONG_USER, true);
  enrollment_ui_.RetryAfterError();
}

IN_PROC_BROWSER_TEST_F(InitialEnrollmentTest, EnrollmentForced) {
  auto initial_enrollment =
      enterprise_management::DeviceInitialEnrollmentStateResponse::
          INITIAL_ENROLLMENT_MODE_ENROLLMENT_ENFORCED;
  policy_server_.SetDeviceInitialEnrollmentResponse(
      test::kTestRlzBrandCodeKey, test::kTestSerialNumber, initial_enrollment,
      test::kTestDomain, base::nullopt /* is_license_packaged_with_device */);

  host()->StartWizard(AutoEnrollmentCheckScreenView::kScreenId);
  OobeScreenWaiter(EnrollmentScreenView::kScreenId).Wait();

  // User can't skip.
  enrollment_ui_.SetExitHandler();
  enrollment_screen()->OnCancel();
  EXPECT_EQ(EnrollmentScreen::Result::BACK, enrollment_ui_.WaitForScreenExit());

  // Domain is actually different from what the server sent down. But Chrome
  // does not enforce that domain if device is not locked.
  enrollment_screen()->OnLoginDone(FakeGaiaMixin::kEnterpriseUser1,
                                   FakeGaiaMixin::kFakeAuthCode);
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSuccess);
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
  EXPECT_TRUE(InstallAttributes::Get()->IsEnterpriseManaged());
}

// Zero touch with attestation authentication fail. Attestation fails because we
// send empty cert request. Should switch to interactive authentication.
IN_PROC_BROWSER_TEST_F(InitialEnrollmentTest, ZeroTouchForcedAttestationFail) {
  auto initial_enrollment =
      enterprise_management::DeviceInitialEnrollmentStateResponse::
          INITIAL_ENROLLMENT_MODE_ZERO_TOUCH_ENFORCED;
  policy_server_.SetDeviceInitialEnrollmentResponse(
      test::kTestRlzBrandCodeKey, test::kTestSerialNumber, initial_enrollment,
      test::kTestDomain, base::nullopt /* is_license_packaged_with_device */);

  host()->StartWizard(AutoEnrollmentCheckScreenView::kScreenId);
  OobeScreenWaiter(EnrollmentScreenView::kScreenId).Wait();

  // First it tries with attestation auth and should fail.
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_ui_.ExpectErrorMessage(
      IDS_ENTERPRISE_ENROLLMENT_STATUS_REGISTRATION_CERT_FETCH_FAILED,
      /* can retry */ true);

  // Cancel bring up Gaia sing-in page.
  enrollment_screen()->OnCancel();
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSignin);

  // User can't skip.
  enrollment_ui_.SetExitHandler();
  enrollment_screen()->OnCancel();
  EXPECT_EQ(EnrollmentScreen::Result::BACK, enrollment_ui_.WaitForScreenExit());

  // Domain is actually different from what the server sent down. But Chrome
  // does not enforce that domain if device is not locked.
  enrollment_screen()->OnLoginDone(FakeGaiaMixin::kEnterpriseUser1,
                                   FakeGaiaMixin::kFakeAuthCode);
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSuccess);
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
  EXPECT_TRUE(InstallAttributes::Get()->IsEnterpriseManaged());
}

IN_PROC_BROWSER_TEST_F(InitialEnrollmentTest,
                       ZeroTouchForcedAttestationSuccess) {
  policy_server_.SetupZeroTouchForcedEnrollment();

  host()->StartWizard(AutoEnrollmentCheckScreenView::kScreenId);
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSuccess);
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
  EXPECT_TRUE(InstallAttributes::Get()->IsCloudManaged());
}

class OobeGuestButtonPolicy : public testing::WithParamInterface<bool>,
                              public EnrollmentLocalPolicyServerBase {
 public:
  OobeGuestButtonPolicy() = default;

  void SetUpOnMainThread() override {
    enterprise_management::ChromeDeviceSettingsProto proto;
    proto.mutable_guest_mode_enabled()->set_guest_mode_enabled(GetParam());
    policy_server_.UpdateDevicePolicy(proto);
    EnrollmentLocalPolicyServerBase::SetUpOnMainThread();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(OobeGuestButtonPolicy);
};

IN_PROC_BROWSER_TEST_P(OobeGuestButtonPolicy, VisibilityAfterEnrollment) {
  TriggerEnrollmentAndSignInSuccessfully();
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSuccess);
  enrollment_screen()->OnConfirmationClosed();
  OobeScreenWaiter(GaiaView::kScreenId).Wait();

  ASSERT_EQ(GetParam(),
            user_manager::UserManager::Get()->IsGuestSessionAllowed());
  EXPECT_EQ(GetParam(), ash::LoginScreenTestApi::IsGuestButtonShown());

  test::ExecuteOobeJS("chrome.send('showGuestInOobe', [false]);");
  EXPECT_FALSE(ash::LoginScreenTestApi::IsGuestButtonShown());

  test::ExecuteOobeJS("chrome.send('showGuestInOobe', [true]);");
  EXPECT_EQ(GetParam(), ash::LoginScreenTestApi::IsGuestButtonShown());
}

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         OobeGuestButtonPolicy,
                         ::testing::Bool());

}  // namespace chromeos
