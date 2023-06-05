// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/test/gtest_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ash/login/app_mode/kiosk_launch_controller.h"
#include "chrome/browser/ash/login/app_mode/test/kiosk_apps_mixin.h"
#include "chrome/browser/ash/login/app_mode/test/kiosk_test_helpers.h"
#include "chrome/browser/ash/login/enrollment/auto_enrollment_check_screen.h"
#include "chrome/browser/ash/login/enrollment/enrollment_screen.h"
#include "chrome/browser/ash/login/enrollment/enrollment_screen_view.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/enrollment_ui_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/login_or_lock_screen_visible_waiter.h"
#include "chrome/browser/ash/login/test/network_portal_detector_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screens_utils.h"
#include "chrome/browser/ash/login/test/test_condition_waiter.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/ownership/fake_owner_settings_service.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_type_checker.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_requisition_manager.h"
#include "chrome/browser/ash/policy/enrollment/psm/rlwe_test_support.h"
#include "chrome/browser/ash/policy/server_backed_state/server_backed_state_keys_broker.h"
#include "chrome/browser/ash/policy/test_support/embedded_policy_test_server_mixin.h"
#include "chrome/browser/ash/policy/test_support/policy_test_server_constants.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/ui/webui/ash/login/device_disabled_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chromeos/ash/components/attestation/mock_attestation_flow.h"
#include "chromeos/ash/components/attestation/stub_attestation_features.h"
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "net/http/http_status_code.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace {

namespace em = enterprise_management;

constexpr test::UIPath kEnterprisePrimaryButton = {
    "enterprise-enrollment", "step-signin", "primary-action-button"};
constexpr test::UIPath kEnterpriseEnrollmentButton = {
    "enterprise-enrollment", "step-signin", "enterprise-navigation-enterprise"};
constexpr test::UIPath kKioskEnrollmentButton = {
    "enterprise-enrollment", "step-signin", "enterprise-navigation-kiosk"};
constexpr test::UIPath kKioskModeEnterpriseEnrollmentButton = {
    "enterprise-enrollment", "step-signin", "kiosk-navigation-enterprise"};
constexpr test::UIPath kKioskModeKioskEnrollmentButton = {
    "enterprise-enrollment", "step-signin", "kiosk-navigation-kiosk"};

const char kRemoraRequisition[] = "remora";

std::string GetDmTokenFromPolicy(const std::string& blob) {
  enterprise_management::PolicyFetchResponse policy;
  CHECK(policy.ParseFromString(blob));

  enterprise_management::PolicyData policy_data;
  CHECK(policy_data.ParseFromString(policy.policy_data()));
  return policy_data.request_token();
}

void AllowlistSimpleChallengeSigningKey() {
  AttestationClient::Get()->GetTestInterface()->AllowlistSignSimpleChallengeKey(
      /*username=*/"", attestation::kEnterpriseEnrollmentKey);
}

class EnrollmentEmbeddedPolicyServerBase : public OobeBaseTest {
 public:
  EnrollmentEmbeddedPolicyServerBase() {
    gaia_frame_parent_ = "authView";
    authenticator_id_ = "$('enterprise-enrollment').authenticator_";
  }

  EnrollmentEmbeddedPolicyServerBase(
      const EnrollmentEmbeddedPolicyServerBase&) = delete;
  EnrollmentEmbeddedPolicyServerBase& operator=(
      const EnrollmentEmbeddedPolicyServerBase&) = delete;

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
        WizardController::default_controller()
            ->GetScreen<AutoEnrollmentCheckScreen>();
    EXPECT_NE(auto_enrollment_screen, nullptr);
    return auto_enrollment_screen;
  }

  void TriggerEnrollmentAndSignInSuccessfully(bool enroll_kiosk = false) {
    host()->HandleAccelerator(LoginAcceleratorAction::kStartEnrollment);
    OobeScreenWaiter(EnrollmentScreenView::kScreenId).Wait();

    ASSERT_FALSE(StartupUtils::IsDeviceRegistered());
    ASSERT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
    WaitForGaiaPageBackButtonUpdate();

    SigninFrameJS().TypeIntoPath(FakeGaiaMixin::kFakeUserEmail,
                                 FakeGaiaMixin::kEmailPath);
    test::OobeJS().ClickOnPath(kEnterprisePrimaryButton);
    SigninFrameJS().TypeIntoPath(FakeGaiaMixin::kFakeUserPassword,
                                 FakeGaiaMixin::kPasswordPath);
    if (features::IsKioskEnrollmentInOobeEnabled()) {
      if (enroll_kiosk) {
        test::OobeJS().ClickOnPath(kKioskEnrollmentButton);
      } else {
        test::OobeJS().ClickOnPath(kEnterpriseEnrollmentButton);
      }
    } else {
      test::OobeJS().ClickOnPath(kEnterprisePrimaryButton);
    }
  }

  std::unique_ptr<LoginOrLockScreenVisibleWaiter> CreateLoginVisibleWaiter() {
    return std::make_unique<LoginOrLockScreenVisibleWaiter>();
  }

  void ConfirmAndWaitLoginScreen() {
    auto login_screen_waiter = CreateLoginVisibleWaiter();
    enrollment_screen()->OnConfirmationClosed();
    login_screen_waiter->WaitEvenIfShown();
  }

  void AddPublicUser(const std::string& account_id) {
    enterprise_management::ChromeDeviceSettingsProto proto;
    enterprise_management::DeviceLocalAccountInfoProto* account =
        proto.mutable_device_local_accounts()->add_account();
    account->set_account_id(account_id);
    account->set_type(enterprise_management::DeviceLocalAccountInfoProto::
                          ACCOUNT_TYPE_PUBLIC_SESSION);
    policy_server_.UpdateDevicePolicy(proto);
  }

  void SetLoginScreenLocale(const std::string& locale) {
    enterprise_management::ChromeDeviceSettingsProto proto;
    proto.mutable_login_screen_locales()->add_login_screen_locales(locale);
    policy_server_.UpdateDevicePolicy(proto);
  }

  EmbeddedPolicyTestServerMixin policy_server_{&mixin_host_};
  test::EnrollmentUIMixin enrollment_ui_{&mixin_host_};
  FakeGaiaMixin fake_gaia_{&mixin_host_};
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_UNOWNED};

 private:
  attestation::ScopedStubAttestationFeatures attestation_features_;
};

class AutoEnrollmentEmbeddedPolicyServer
    : public EnrollmentEmbeddedPolicyServerBase {
 public:
  AutoEnrollmentEmbeddedPolicyServer() {
    device_state_.SetState(DeviceStateMixin::State::BEFORE_OOBE);
  }

  AutoEnrollmentEmbeddedPolicyServer(
      const AutoEnrollmentEmbeddedPolicyServer&) = delete;
  AutoEnrollmentEmbeddedPolicyServer& operator=(
      const AutoEnrollmentEmbeddedPolicyServer&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    EnrollmentEmbeddedPolicyServerBase::SetUpCommandLine(command_line);

    command_line->AppendSwitchASCII(
        switches::kEnterpriseEnableForcedReEnrollment,
        policy::AutoEnrollmentTypeChecker::kForcedReEnrollmentAlways);
    command_line->AppendSwitchASCII(
        switches::kEnterpriseEnrollmentInitialModulus, "5");
    command_line->AppendSwitchASCII(switches::kEnterpriseEnrollmentModulusLimit,
                                    "5");
  }

  policy::ServerBackedStateKeysBroker* state_keys_broker() {
    return g_browser_process->platform_part()
        ->browser_policy_connector_ash()
        ->GetStateKeysBroker();
  }

 protected:
  NetworkPortalDetectorMixin network_portal_detector_{&mixin_host_};
};

class AutoEnrollmentWithStatistics : public AutoEnrollmentEmbeddedPolicyServer {
 public:
  AutoEnrollmentWithStatistics() : AutoEnrollmentEmbeddedPolicyServer() {
    // `AutoEnrollmentTypeChecker` assumes that VPD is in valid state if
    // "serial_number" or "Product_S/N" could be read from it.
    fake_statistics_provider_.SetMachineStatistic(
        system::kSerialNumberKeyForTest, test::kTestSerialNumber);
    fake_statistics_provider_.SetVpdStatus(
        system::StatisticsProvider::VpdStatus::kValid);
  }

  AutoEnrollmentWithStatistics(const AutoEnrollmentWithStatistics&) = delete;
  AutoEnrollmentWithStatistics& operator=(const AutoEnrollmentWithStatistics&) =
      delete;

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
    fake_statistics_provider_.SetVpdStatus(
        system::StatisticsProvider::VpdStatus::kRwInvalid);
  }

 private:
  system::ScopedFakeStatisticsProvider fake_statistics_provider_;
};

class AutoEnrollmentNoStateKeys : public AutoEnrollmentWithStatistics {
 public:
  AutoEnrollmentNoStateKeys() = default;

  AutoEnrollmentNoStateKeys(const AutoEnrollmentNoStateKeys&) = delete;
  AutoEnrollmentNoStateKeys& operator=(const AutoEnrollmentNoStateKeys&) =
      delete;

  ~AutoEnrollmentNoStateKeys() override = default;

  // AutoEnrollmentWithStatistics:
  void SetUpInProcessBrowserTestFixture() override {
    AutoEnrollmentWithStatistics::SetUpInProcessBrowserTestFixture();
    // Session manager client is initialized by DeviceStateMixin.
    FakeSessionManagerClient::Get()->set_state_keys_handling(
        FakeSessionManagerClient::ServerBackedStateKeysHandling::
            kForceNotAvailable);
  }
};

class InitialEnrollmentTest : public EnrollmentEmbeddedPolicyServerBase {
 public:
  InitialEnrollmentTest() {
    policy_server_.ConfigureFakeStatisticsForZeroTouch(
        &fake_statistics_provider_);
  }

  InitialEnrollmentTest(const InitialEnrollmentTest&) = delete;
  InitialEnrollmentTest& operator=(const InitialEnrollmentTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    EnrollmentEmbeddedPolicyServerBase::SetUpCommandLine(command_line);

    command_line->AppendSwitchASCII(
        switches::kEnterpriseEnableInitialEnrollment,
        policy::AutoEnrollmentTypeChecker::kInitialEnrollmentAlways);
  }

  int GetPsmExecutionResultPref() const {
    const PrefService& local_state = *g_browser_process->local_state();
    const PrefService::Preference* has_psm_execution_result_pref =
        local_state.FindPreference(prefs::kEnrollmentPsmResult);

    // Verify the existence of an integer pref value
    // `prefs::kEnrollmentPsmResult`.
    if (!has_psm_execution_result_pref) {
      ADD_FAILURE() << "kEnrollmentPsmResult pref not found";
      return -1;
    }
    if (!has_psm_execution_result_pref->GetValue()->is_int()) {
      ADD_FAILURE()
          << "kEnrollmentPsmResult pref does not have an integer value";
      return -1;
    }
    EXPECT_FALSE(has_psm_execution_result_pref->IsDefaultValue());

    int psm_execution_result =
        has_psm_execution_result_pref->GetValue()->GetInt();

    // Verify that `psm_execution_result` has a valid value of
    // em::DeviceRegisterRequest::PsmExecutionResult enum.
    EXPECT_TRUE(em::DeviceRegisterRequest::PsmExecutionResult_IsValid(
        psm_execution_result));

    return psm_execution_result;
  }

  int64_t GetPsmDeterminationTimestampPref() const {
    const PrefService& local_state = *g_browser_process->local_state();
    const PrefService::Preference* has_psm_determination_timestamp_pref =
        local_state.FindPreference(prefs::kEnrollmentPsmDeterminationTime);

    // Verify the existence of non-default value pref
    // `prefs::kEnrollmentPsmDeterminationTime`.
    if (!has_psm_determination_timestamp_pref) {
      ADD_FAILURE() << "kEnrollmentPsmDeterminationTime pref not found";
      return -1;
    }
    EXPECT_FALSE(has_psm_determination_timestamp_pref->IsDefaultValue());

    const base::Time psm_determination_timestamp =
        local_state.GetTime(prefs::kEnrollmentPsmDeterminationTime);

    // The PSM determination timestamp should exist at this stage. Because
    // we already checked the existence of the pref with non-default value.
    EXPECT_FALSE(psm_determination_timestamp.is_null());

    return psm_determination_timestamp.ToJavaTime();
  }

 private:
  system::ScopedFakeStatisticsProvider fake_statistics_provider_;
};

// Requesting state keys hangs forever, but that should not matter because we're
// running on reven.
class EnrollmentOnRevenWithNoStateKeysResponse
    : public EnrollmentEmbeddedPolicyServerBase {
 public:
  EnrollmentOnRevenWithNoStateKeysResponse() = default;

  EnrollmentOnRevenWithNoStateKeysResponse(
      const EnrollmentOnRevenWithNoStateKeysResponse&) = delete;
  EnrollmentOnRevenWithNoStateKeysResponse& operator=(
      const EnrollmentOnRevenWithNoStateKeysResponse&) = delete;

  ~EnrollmentOnRevenWithNoStateKeysResponse() override = default;

  // EnrollmentEmbeddedPolicyServerBase:
  void SetUpInProcessBrowserTestFixture() override {
    EnrollmentEmbeddedPolicyServerBase::SetUpInProcessBrowserTestFixture();
    // Session manager client is initialized by DeviceStateMixin.
    FakeSessionManagerClient::Get()->set_state_keys_handling(
        FakeSessionManagerClient::ServerBackedStateKeysHandling::kNoResponse);
    // reven devices are also marked 'nochrome' which is important because it
    // disables Forced Re-Enrollment (FRE). If FRE is enabled, state keys are
    // required.
    fake_statistics_provider_.SetMachineStatistic(
        system::kFirmwareTypeKey, system::kFirmwareTypeValueNonchrome);
    // When using a fresh ScopedFakeStatisticsProvider we also need to configure
    // a few entries (serial number, machine model).
    // ConfigureFakeStatisticsForZeroTouch does that for us.
    policy_server_.ConfigureFakeStatisticsForZeroTouch(
        &fake_statistics_provider_);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    EnrollmentEmbeddedPolicyServerBase::SetUpCommandLine(command_line);

    command_line->AppendSwitch(switches::kRevenBranding);
  }

 private:
  system::ScopedFakeStatisticsProvider fake_statistics_provider_;
};

// Simple manual enrollment.
IN_PROC_BROWSER_TEST_F(EnrollmentEmbeddedPolicyServerBase, ManualEnrollment) {
  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSuccess);
  test::OobeJS().ExpectTrue("Oobe.isEnrollmentSuccessfulForTest()");
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
  EXPECT_TRUE(InstallAttributes::Get()->IsCloudManaged());
}

// The test case is the same as
// EnrollmentEmbeddedPolicyServerBase.ManualEnrollment but the environment is
// different (simulate reven board, simulate state keys not being available).
IN_PROC_BROWSER_TEST_F(EnrollmentOnRevenWithNoStateKeysResponse,
                       ManualEnrollment) {
  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSuccess);
  test::OobeJS().ExpectTrue("Oobe.isEnrollmentSuccessfulForTest()");
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
  EXPECT_TRUE(InstallAttributes::Get()->IsCloudManaged());
}

// Device policy blocks dev mode and this is not prohibited by a command-line
// flag.
IN_PROC_BROWSER_TEST_F(EnrollmentEmbeddedPolicyServerBase,
                       DeviceBlockDevmodeAllowed) {
  enterprise_management::ChromeDeviceSettingsProto proto;
  proto.mutable_system_settings()->set_block_devmode(true);
  policy_server_.UpdateDevicePolicy(proto);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSuccess);
  test::OobeJS().ExpectTrue("Oobe.isEnrollmentSuccessfulForTest()");
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
  EXPECT_TRUE(InstallAttributes::Get()->IsCloudManaged());
}

// Device policy blocks dev mode and a command-line flag prevents this from
// applying.
IN_PROC_BROWSER_TEST_F(EnrollmentEmbeddedPolicyServerBase,
                       DeviceBlockDevmodeDisallowed) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisallowPolicyBlockDevMode);
  enterprise_management::ChromeDeviceSettingsProto proto;
  proto.mutable_system_settings()->set_block_devmode(true);
  policy_server_.UpdateDevicePolicy(proto);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_ui_.ExpectErrorMessage(
      IDS_ENTERPRISE_ENROLLMENT_ERROR_MAY_NOT_BLOCK_DEV_MODE,
      /*can_retry=*/false);
  enrollment_ui_.CancelAfterError();
}

// Simple manual enrollment with device attributes prompt.
IN_PROC_BROWSER_TEST_F(EnrollmentEmbeddedPolicyServerBase,
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

// Negative scenarios: see different HTTP error codes in
// device_management_service.cc

// Error during enrollment : 402 - missing licenses.
IN_PROC_BROWSER_TEST_F(EnrollmentEmbeddedPolicyServerBase,
                       EnrollmentErrorNoLicenses) {
  policy_server_.SetDeviceEnrollmentError(
      policy::DeviceManagementService::kMissingLicenses);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_ui_.ExpectErrorMessage(
      IDS_ENTERPRISE_ENROLLMENT_MISSING_LICENSES_ERROR, /*can_retry=*/true);
  enrollment_ui_.RetryAndWaitForSigninStep();
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
}

IN_PROC_BROWSER_TEST_F(EnrollmentEmbeddedPolicyServerBase,
                       EnrollmentErrorNoLicensesMeets) {
  policy::EnrollmentRequisitionManager::SetDeviceRequisition(
      kRemoraRequisition);
  policy_server_.SetDeviceEnrollmentError(
      policy::DeviceManagementService::kMissingLicenses);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_ui_.ExpectErrorMessage(
      IDS_ENTERPRISE_ENROLLMENT_MISSING_LICENSES_ERROR_MEETS,
      /*can_retry=*/true);
  enrollment_ui_.RetryAndWaitForSigninStep();
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
}

// Error during enrollment : 403 - management not allowed.
IN_PROC_BROWSER_TEST_F(EnrollmentEmbeddedPolicyServerBase,
                       EnrollmentErrorManagementNotAllowed) {
  policy_server_.SetDeviceEnrollmentError(
      policy::DeviceManagementService::kDeviceManagementNotAllowed);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_ui_.ExpectErrorMessage(
      IDS_ENTERPRISE_ENROLLMENT_AUTH_ACCOUNT_ERROR, /*can_retry=*/true);
  enrollment_ui_.RetryAndWaitForSigninStep();
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
}

IN_PROC_BROWSER_TEST_F(EnrollmentEmbeddedPolicyServerBase,
                       EnrollmentErrorManagementNotAllowedMeets) {
  policy::EnrollmentRequisitionManager::SetDeviceRequisition(
      kRemoraRequisition);
  policy_server_.SetDeviceEnrollmentError(
      policy::DeviceManagementService::kDeviceManagementNotAllowed);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_ui_.ExpectErrorMessage(
      IDS_ENTERPRISE_ENROLLMENT_ACCOUNT_ERROR_MEETS, /*can_retry=*/true);
  enrollment_ui_.RetryAndWaitForSigninStep();
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
}

// Error during enrollment : 405 - invalid device serial.
IN_PROC_BROWSER_TEST_F(EnrollmentEmbeddedPolicyServerBase,
                       EnrollmentErrorInvalidDeviceSerial) {
  policy_server_.SetDeviceEnrollmentError(
      policy::DeviceManagementService::kInvalidSerialNumber);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  // TODO (antrim, rsorokin): find out why it makes sense to retry here?
  enrollment_ui_.ExpectErrorMessage(
      IDS_POLICY_DM_STATUS_SERVICE_INVALID_SERIAL_NUMBER,
      /*can_retry=*/true);
  enrollment_ui_.RetryAndWaitForSigninStep();
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
}

// Error during enrollment : 406 - domain mismatch
IN_PROC_BROWSER_TEST_F(EnrollmentEmbeddedPolicyServerBase,
                       EnrollmentErrorDomainMismatch) {
  policy_server_.SetDeviceEnrollmentError(
      policy::DeviceManagementService::kDomainMismatch);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_ui_.ExpectErrorMessage(
      IDS_ENTERPRISE_ENROLLMENT_DOMAIN_MISMATCH_ERROR, /*can_retry=*/true);
  enrollment_ui_.RetryAndWaitForSigninStep();
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
}

// Error during enrollment : 409 - Device ID is already in use
IN_PROC_BROWSER_TEST_F(EnrollmentEmbeddedPolicyServerBase,
                       EnrollmentErrorDeviceIDConflict) {
  policy_server_.SetDeviceEnrollmentError(
      policy::DeviceManagementService::kDeviceIdConflict);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  // TODO (antrim, rsorokin): find out why it makes sense to retry here?
  enrollment_ui_.ExpectErrorMessage(
      IDS_POLICY_DM_STATUS_SERVICE_DEVICE_ID_CONFLICT, /*can_retry=*/true);
  enrollment_ui_.RetryAndWaitForSigninStep();
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
}

// Error during enrollment : 412 - Activation is pending
IN_PROC_BROWSER_TEST_F(EnrollmentEmbeddedPolicyServerBase,
                       EnrollmentErrorActivationIsPending) {
  policy_server_.SetDeviceEnrollmentError(
      policy::DeviceManagementService::kPendingApproval);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_ui_.ExpectErrorMessage(
      IDS_POLICY_DM_STATUS_SERVICE_ACTIVATION_PENDING, /*can_retry=*/true);
  enrollment_ui_.RetryAndWaitForSigninStep();
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
}

// Error during enrollment : 417 - Consumer account with packaged license.
IN_PROC_BROWSER_TEST_F(EnrollmentEmbeddedPolicyServerBase,
                       EnrollmentErrorConsumerAccountWithPackagedLicense) {
  policy_server_.SetDeviceEnrollmentError(
      policy::DeviceManagementService::kConsumerAccountWithPackagedLicense);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_ui_.ExpectErrorMessage(
      IDS_ENTERPRISE_ENROLLMENT_CONSUMER_ACCOUNT_WITH_PACKAGED_LICENSE,
      /*can_retry=*/true);
  enrollment_ui_.RetryAndWaitForSigninStep();
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
}

// Error during enrollment : 500 - Consumer account with packaged license.
IN_PROC_BROWSER_TEST_F(EnrollmentEmbeddedPolicyServerBase,
                       EnrollmentErrorServerError) {
  policy_server_.SetDeviceEnrollmentError(
      policy::DeviceManagementService::kInternalServerError);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_ui_.ExpectErrorMessage(IDS_POLICY_DM_STATUS_TEMPORARY_UNAVAILABLE,
                                    /*can_retry=*/true);
  enrollment_ui_.RetryAndWaitForSigninStep();
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
}

// Error during enrollment : 905 - Ineligible enterprise account.
IN_PROC_BROWSER_TEST_F(EnrollmentEmbeddedPolicyServerBase,
                       EnrollmentErrorEnterpriseAccountIsNotEligibleToEnroll) {
  policy_server_.SetDeviceEnrollmentError(
      policy::DeviceManagementService::kInvalidDomainlessCustomer);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_ui_.ExpectErrorMessage(
      IDS_ENTERPRISE_ENROLLMENT_ENTERPRISE_ACCOUNT_IS_NOT_ELIGIBLE_TO_ENROLL,
      /*can_retry=*/true);
  enrollment_ui_.RetryAndWaitForSigninStep();
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
}

IN_PROC_BROWSER_TEST_F(EnrollmentEmbeddedPolicyServerBase,
                       EnrollmentErrorEnterpriseTosHasNotBeenAccepeted) {
  policy_server_.SetDeviceEnrollmentError(
      policy::DeviceManagementService::kTosHasNotBeenAccepted);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_ui_.ExpectErrorMessage(
      IDS_ENTERPRISE_ENROLLMENT_ENTERPRISE_TOS_HAS_NOT_BEEN_ACCEPTED,
      /*can_retry=*/true);
  enrollment_ui_.RetryAndWaitForSigninStep();
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
}

IN_PROC_BROWSER_TEST_F(EnrollmentEmbeddedPolicyServerBase,
                       EnrollmentErrorEnterpriseTosHasNotBeenAcceptedMeets) {
  policy::EnrollmentRequisitionManager::SetDeviceRequisition(
      kRemoraRequisition);
  policy_server_.SetDeviceEnrollmentError(
      policy::DeviceManagementService::kTosHasNotBeenAccepted);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_ui_.ExpectErrorMessage(
      IDS_ENTERPRISE_ENROLLMENT_ENTERPRISE_TOS_HAS_NOT_BEEN_ACCEPTED_MEETS,
      /*can_retry=*/true);
  enrollment_ui_.RetryAndWaitForSigninStep();
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
}

IN_PROC_BROWSER_TEST_F(EnrollmentEmbeddedPolicyServerBase,
                       EnrollmentErrorIllegalAccountForPackagedEDULicense) {
  policy_server_.SetDeviceEnrollmentError(
      policy::DeviceManagementService::kIllegalAccountForPackagedEDULicense);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_ui_.ExpectErrorMessage(
      IDS_ENTERPRISE_ENROLLMENT_ILLEGAL_ACCOUNT_FOR_PACKAGED_EDU_LICENSE,
      /*can_retry=*/true);
  enrollment_ui_.RetryAndWaitForSigninStep();
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
}

// Error during enrollment : Strange HTTP response from server.
IN_PROC_BROWSER_TEST_F(EnrollmentEmbeddedPolicyServerBase,
                       EnrollmentErrorServerIsDrunk) {
  policy_server_.SetDeviceEnrollmentError(12345);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_ui_.ExpectErrorMessage(IDS_POLICY_DM_STATUS_HTTP_STATUS_ERROR,
                                    /*can_retry=*/true);
  enrollment_ui_.RetryAndWaitForSigninStep();
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
}

// Error during enrollment : Can not update device attributes
IN_PROC_BROWSER_TEST_F(EnrollmentEmbeddedPolicyServerBase,
                       EnrollmentErrorUploadingDeviceAttributes) {
  policy_server_.SetUpdateDeviceAttributesPermission(true);
  policy_server_.SetDeviceAttributeUpdateError(
      policy::DeviceManagementService::kInternalServerError);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepDeviceAttributes);
  enrollment_ui_.SubmitDeviceAttributes(test::values::kAssetId,
                                        test::values::kLocation);

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
  EXPECT_TRUE(InstallAttributes::Get()->IsCloudManaged());
  auto login_waiter = CreateLoginVisibleWaiter();
  enrollment_ui_.LeaveDeviceAttributeErrorScreen();
  login_waiter->WaitEvenIfShown();
  // TODO(crbug/1295825): Wait for OOBE to be reloaded on the first screen once
  // loading is faster and does not cause the test to time out.
}

// Error during enrollment : Error fetching policy : 500 server error.
IN_PROC_BROWSER_TEST_F(EnrollmentEmbeddedPolicyServerBase,
                       EnrollmentErrorFetchingPolicyTransient) {
  policy_server_.SetPolicyFetchError(
      policy::DeviceManagementService::kInternalServerError);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_ui_.ExpectErrorMessage(IDS_POLICY_DM_STATUS_TEMPORARY_UNAVAILABLE,
                                    /*can_retry=*/true);
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
  enrollment_ui_.RetryAndWaitForSigninStep();
}

// Error during enrollment : Error 418: PACKAGED_DEVICE_KIOSK_DISALLOWED.
IN_PROC_BROWSER_TEST_F(EnrollmentEmbeddedPolicyServerBase,
                       EnrollmentErrorPackagedDeviceInvalidForKiosk) {
  policy_server_.SetDeviceEnrollmentError(
      policy::DeviceManagementService::kInvalidPackagedDeviceForKiosk);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_ui_.ExpectErrorMessage(
      IDS_ENTERPRISE_ENROLLMENT_INVALID_PACKAGED_DEVICE_FOR_KIOSK,
      /*can_retry=*/true);
  enrollment_ui_.RetryAndWaitForSigninStep();
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
}

// Error during enrollment : Error fetching policy : 902 - policy not found.
IN_PROC_BROWSER_TEST_F(EnrollmentEmbeddedPolicyServerBase,
                       EnrollmentErrorFetchingPolicyNotFound) {
  policy_server_.SetPolicyFetchError(
      policy::DeviceManagementService::kPolicyNotFound);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_ui_.ExpectErrorMessage(
      IDS_POLICY_DM_STATUS_SERVICE_POLICY_NOT_FOUND,
      /*can_retry=*/true);
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
  enrollment_ui_.RetryAndWaitForSigninStep();
}

// Error during enrollment : Error fetching policy : 903 - deprovisioned.
IN_PROC_BROWSER_TEST_F(EnrollmentEmbeddedPolicyServerBase,
                       EnrollmentErrorFetchingPolicyDeprovisioned) {
  policy_server_.SetPolicyFetchError(
      policy::DeviceManagementService::kDeprovisioned);

  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_ui_.ExpectErrorMessage(IDS_POLICY_DM_STATUS_SERVICE_DEPROVISIONED,
                                    /*can_retry=*/true);
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
  enrollment_ui_.RetryAndWaitForSigninStep();
}

// No state keys on the server. Auto enrollment check should proceed to login.
IN_PROC_BROWSER_TEST_F(AutoEnrollmentEmbeddedPolicyServer,
                       AutoEnrollmentCheck) {
  host()->StartWizard(AutoEnrollmentCheckScreenView::kScreenId);
  OobeScreenWaiter(GetFirstSigninScreen()).Wait();
}

// State keys are present but restore mode is not requested.
IN_PROC_BROWSER_TEST_F(AutoEnrollmentEmbeddedPolicyServer, ReenrollmentNone) {
  EXPECT_TRUE(policy_server_.SetDeviceStateRetrievalResponse(
      state_keys_broker(),
      enterprise_management::DeviceStateRetrievalResponse::RESTORE_MODE_NONE,
      test::kTestDomain));
  host()->StartWizard(AutoEnrollmentCheckScreenView::kScreenId);
  OobeScreenWaiter(GetFirstSigninScreen()).Wait();
}

// Reenrollment requested. User can skip.
IN_PROC_BROWSER_TEST_F(AutoEnrollmentEmbeddedPolicyServer,
                       ReenrollmentRequested) {
  EXPECT_TRUE(policy_server_.SetDeviceStateRetrievalResponse(
      state_keys_broker(),
      enterprise_management::DeviceStateRetrievalResponse::
          RESTORE_MODE_REENROLLMENT_REQUESTED,
      test::kTestDomain));
  host()->StartWizard(AutoEnrollmentCheckScreenView::kScreenId);
  OobeScreenWaiter(EnrollmentScreenView::kScreenId).Wait();
  enrollment_screen()->OnCancel();
  OobeScreenWaiter(GetFirstSigninScreen()).Wait();
}

// Reenrollment forced. User can not skip.
IN_PROC_BROWSER_TEST_F(AutoEnrollmentEmbeddedPolicyServer, ReenrollmentForced) {
  EXPECT_TRUE(policy_server_.SetDeviceStateRetrievalResponse(
      state_keys_broker(),
      enterprise_management::DeviceStateRetrievalResponse::
          RESTORE_MODE_REENROLLMENT_ENFORCED,
      test::kTestDomain));
  host()->StartWizard(AutoEnrollmentCheckScreenView::kScreenId);
  OobeScreenWaiter(EnrollmentScreenView::kScreenId).Wait();
  enrollment_ui_.SetExitHandler();
  enrollment_screen()->OnCancel();
  EXPECT_EQ(EnrollmentScreen::Result::BACK_TO_AUTO_ENROLLMENT_CHECK,
            enrollment_ui_.WaitForScreenExit());
}

// Device is disabled.
IN_PROC_BROWSER_TEST_F(AutoEnrollmentEmbeddedPolicyServer, DeviceDisabled) {
  EXPECT_TRUE(policy_server_.SetDeviceStateRetrievalResponse(
      state_keys_broker(),
      enterprise_management::DeviceStateRetrievalResponse::
          RESTORE_MODE_DISABLED,
      test::kTestDomain));
  host()->StartWizard(AutoEnrollmentCheckScreenView::kScreenId);
  OobeScreenWaiter(DeviceDisabledScreenView::kScreenId).Wait();
}

// Attestation enrollment.
IN_PROC_BROWSER_TEST_F(AutoEnrollmentEmbeddedPolicyServer, Attestation) {
  WaitForOobeUI();
  policy_server_.SetUpdateDeviceAttributesPermission(true);

  AllowlistSimpleChallengeSigningKey();
  policy_server_.SetFakeAttestationFlow();
  EXPECT_TRUE(policy_server_.SetDeviceStateRetrievalResponse(
      state_keys_broker(),
      enterprise_management::DeviceStateRetrievalResponse::
          RESTORE_MODE_REENROLLMENT_ZERO_TOUCH,
      test::kTestDomain));

  WizardController::default_controller()->AdvanceToScreen(
      AutoEnrollmentCheckScreenView::kScreenId);

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSuccess);
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
  EXPECT_TRUE(InstallAttributes::Get()->IsCloudManaged());
}

// Verify able to advance to login screen when error screen is shown.
IN_PROC_BROWSER_TEST_F(AutoEnrollmentEmbeddedPolicyServer, TestCaptivePortal) {
  network_portal_detector_.SimulateDefaultNetworkState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL);
  host()->StartWizard(AutoEnrollmentCheckScreenView::kScreenId);
  OobeScreenWaiter(ErrorScreenView::kScreenId).Wait();

  network_portal_detector_.SimulateDefaultNetworkState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE);
  OobeScreenWaiter(GetFirstSigninScreen()).Wait();
}

// FRE explicitly required in VPD, but the state keys are missing.
IN_PROC_BROWSER_TEST_F(AutoEnrollmentNoStateKeys, FREExplicitlyRequired) {
  SetFRERequiredKey("1");
  host()->StartWizard(AutoEnrollmentCheckScreenView::kScreenId);
  WaitForOobeUI();

  OobeScreenWaiter(ErrorScreenView::kScreenId).Wait();
  test::OobeJS().ExpectHiddenPath({"error-message", "error-guest-signin"});
  test::OobeJS().ExpectHiddenPath(
      {"error-message", "error-guest-signin-fix-network"});
}

// FRE explicitly required when kCheckEnrollmentKey is set to an invalid value.
IN_PROC_BROWSER_TEST_F(AutoEnrollmentNoStateKeys,
                       FREExplicitlyRequiredInvalid) {
  SetFRERequiredKey("anything");
  host()->StartWizard(AutoEnrollmentCheckScreenView::kScreenId);
  WaitForOobeUI();

  OobeScreenWaiter(ErrorScreenView::kScreenId).Wait();
  test::OobeJS().ExpectHiddenPath({"error-message", "error-guest-signin"});
  test::OobeJS().ExpectHiddenPath(
      {"error-message", "error-guest-signin-fix-network"});
}

// FRE not explicitly required and the state keys are missing. Should proceed to
// normal signin.
IN_PROC_BROWSER_TEST_F(AutoEnrollmentNoStateKeys, NotRequired) {
  host()->StartWizard(AutoEnrollmentCheckScreenView::kScreenId);
  OobeScreenWaiter(GetFirstSigninScreen()).Wait();
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
  OobeScreenWaiter(GetFirstSigninScreen()).Wait();
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
  OobeScreenWaiter(GetFirstSigninScreen()).Wait();
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

class EnrollmentRecoveryTest : public EnrollmentEmbeddedPolicyServerBase {
 public:
  EnrollmentRecoveryTest() {
    device_state_.SetState(
        DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED);
  }

  EnrollmentRecoveryTest(const EnrollmentRecoveryTest&) = delete;
  EnrollmentRecoveryTest& operator=(const EnrollmentRecoveryTest&) = delete;

  ~EnrollmentRecoveryTest() override = default;

 protected:
  // EnrollmentEmbeddedPolicyServerBase:
  void SetUpInProcessBrowserTestFixture() override {
    EnrollmentEmbeddedPolicyServerBase::SetUpInProcessBrowserTestFixture();

    // This triggers recovery enrollment.
    device_state_.RequestDevicePolicyUpdate()->policy_data()->Clear();
  }
  void SetUpOnMainThread() override {
    EnrollmentEmbeddedPolicyServerBase::SetUpOnMainThread();
    LoginDisplayHost::default_host()
        ->GetWizardContextForTesting()
        ->is_branded_build = true;
  }
};

IN_PROC_BROWSER_TEST_F(EnrollmentRecoveryTest, Success) {
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
  EXPECT_EQ(EnrollmentScreen::Result::BACK_TO_AUTO_ENROLLMENT_CHECK,
            enrollment_ui_.WaitForScreenExit());

  enrollment_screen()->OnLoginDone(
      FakeGaiaMixin::kEnterpriseUser1,
      static_cast<int>(policy::LicenseType::kEnterprise),
      FakeGaiaMixin::kFakeAuthCode);
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSuccess);

  // DM Token is in the device policy.
  EXPECT_FALSE(
      GetDmTokenFromPolicy(FakeSessionManagerClient::Get()->device_policy())
          .empty());
}

IN_PROC_BROWSER_TEST_F(EnrollmentRecoveryTest, DifferentDomain) {
  test::SkipToEnrollmentOnRecovery();

  ASSERT_TRUE(StartupUtils::IsDeviceRegistered());
  ASSERT_TRUE(InstallAttributes::Get()->IsEnterpriseManaged());
  enrollment_screen()->OnLoginDone(
      FakeGaiaMixin::kFakeUserEmail,
      static_cast<int>(policy::LicenseType::kEnterprise),
      FakeGaiaMixin::kFakeAuthCode);
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_ui_.ExpectErrorMessage(
      IDS_ENTERPRISE_ENROLLMENT_STATUS_LOCK_WRONG_USER, true);
  enrollment_ui_.RetryAndWaitForSigninStep();
}

IN_PROC_BROWSER_TEST_F(InitialEnrollmentTest, EnrollmentForced) {
  auto initial_enrollment =
      enterprise_management::DeviceInitialEnrollmentStateResponse::
          INITIAL_ENROLLMENT_MODE_ENROLLMENT_ENFORCED;
  policy_server_.SetDeviceInitialEnrollmentResponse(
      test::kTestRlzBrandCodeKey, test::kTestSerialNumber, initial_enrollment,
      test::kTestDomain);

  host()->StartWizard(AutoEnrollmentCheckScreenView::kScreenId);
  WizardController::default_controller()
      ->GetAutoEnrollmentControllerForTesting()
      ->SetRlweClientFactoryForTesting(
          policy::psm::testing::CreateClientFactory(/*is_member=*/true));

  OobeScreenWaiter(EnrollmentScreenView::kScreenId).Wait();

  // Expect PSM fields in DeviceRegisterRequest.
  policy_server_.SetExpectedPsmParamsInDeviceRegisterRequest(
      test::kTestRlzBrandCodeKey, test::kTestSerialNumber,
      GetPsmExecutionResultPref(), GetPsmDeterminationTimestampPref());

  // User can't skip.
  enrollment_ui_.SetExitHandler();
  enrollment_screen()->OnCancel();
  EXPECT_EQ(EnrollmentScreen::Result::BACK_TO_AUTO_ENROLLMENT_CHECK,
            enrollment_ui_.WaitForScreenExit());

  // Domain is actually different from what the server sent down. But Chrome
  // does not enforce that domain if device is not locked.
  enrollment_screen()->OnLoginDone(
      FakeGaiaMixin::kEnterpriseUser1,
      static_cast<int>(policy::LicenseType::kEnterprise),
      FakeGaiaMixin::kFakeAuthCode);
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSuccess);
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
  EXPECT_TRUE(InstallAttributes::Get()->IsEnterpriseManaged());
}

// Zero touch with attestation authentication fail. Attestation fails because we
// send empty cert request. Should switch to interactive authentication.
// This test is flaky on ChromeOS. https://crbug.com/1231472
IN_PROC_BROWSER_TEST_F(InitialEnrollmentTest,
                       DISABLED_ZeroTouchForcedAttestationFail) {
  auto initial_enrollment =
      enterprise_management::DeviceInitialEnrollmentStateResponse::
          INITIAL_ENROLLMENT_MODE_ZERO_TOUCH_ENFORCED;
  policy_server_.SetDeviceInitialEnrollmentResponse(
      test::kTestRlzBrandCodeKey, test::kTestSerialNumber, initial_enrollment,
      test::kTestDomain);

  host()->StartWizard(AutoEnrollmentCheckScreenView::kScreenId);
  WizardController::default_controller()
      ->GetAutoEnrollmentControllerForTesting()
      ->SetRlweClientFactoryForTesting(
          policy::psm::testing::CreateClientFactory(/*is_member=*/true));

  OobeScreenWaiter(EnrollmentScreenView::kScreenId).Wait();

  // First it tries with attestation auth and should fail.
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_ui_.ExpectErrorMessage(
      IDS_ENTERPRISE_ENROLLMENT_STATUS_REGISTRATION_CERT_FETCH_FAILED,
      /*can_retry=*/true);

  // Cancel bring up Gaia sing-in page.
  enrollment_screen()->OnCancel();
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSignin);

  // User can't skip.
  enrollment_ui_.SetExitHandler();
  enrollment_screen()->OnCancel();
  EXPECT_EQ(EnrollmentScreen::Result::BACK, enrollment_ui_.WaitForScreenExit());

  // Domain is actually different from what the server sent down. But Chrome
  // does not enforce that domain if device is not locked.
  enrollment_screen()->OnLoginDone(
      FakeGaiaMixin::kEnterpriseUser1,
      static_cast<int>(policy::LicenseType::kEnterprise),
      FakeGaiaMixin::kFakeAuthCode);
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSuccess);
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
  EXPECT_TRUE(InstallAttributes::Get()->IsEnterpriseManaged());
}

IN_PROC_BROWSER_TEST_F(InitialEnrollmentTest,
                       ZeroTouchForcedAttestationSuccess) {
  AllowlistSimpleChallengeSigningKey();
  policy_server_.SetupZeroTouchForcedEnrollment();
  policy_server_.SetUpdateDeviceAttributesPermission(true);

  host()->StartWizard(AutoEnrollmentCheckScreenView::kScreenId);
  WizardController::default_controller()
      ->GetAutoEnrollmentControllerForTesting()
      ->SetRlweClientFactoryForTesting(
          policy::psm::testing::CreateClientFactory(/*is_member=*/true));

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepDeviceAttributes);
  enrollment_ui_.SubmitDeviceAttributes(test::values::kAssetId,
                                        test::values::kLocation);
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSuccess);
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
  EXPECT_TRUE(InstallAttributes::Get()->IsCloudManaged());
}

class OobeGuestButtonPolicy : public testing::WithParamInterface<bool>,
                              public EnrollmentEmbeddedPolicyServerBase {
 public:
  OobeGuestButtonPolicy() = default;

  OobeGuestButtonPolicy(const OobeGuestButtonPolicy&) = delete;
  OobeGuestButtonPolicy& operator=(const OobeGuestButtonPolicy&) = delete;

  void SetUpOnMainThread() override {
    enterprise_management::ChromeDeviceSettingsProto proto;
    proto.mutable_guest_mode_enabled()->set_guest_mode_enabled(GetParam());
    policy_server_.UpdateDevicePolicy(proto);
    EnrollmentEmbeddedPolicyServerBase::SetUpOnMainThread();
  }
};

IN_PROC_BROWSER_TEST_P(OobeGuestButtonPolicy, VisibilityAfterEnrollment) {
  TriggerEnrollmentAndSignInSuccessfully();
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSuccess);
  ConfirmAndWaitLoginScreen();
  OobeScreenWaiter(GetFirstSigninScreen()).Wait();

  ASSERT_EQ(GetParam(),
            user_manager::UserManager::Get()->IsGuestSessionAllowed());
  EXPECT_EQ(GetParam(), LoginScreenTestApi::IsGuestButtonShown());

  test::ExecuteOobeJS("chrome.send('setIsFirstSigninStep', [false]);");
  EXPECT_FALSE(LoginScreenTestApi::IsGuestButtonShown());

  test::ExecuteOobeJS("chrome.send('setIsFirstSigninStep', [true]);");
  EXPECT_EQ(GetParam(), LoginScreenTestApi::IsGuestButtonShown());
}

INSTANTIATE_TEST_SUITE_P(All, OobeGuestButtonPolicy, ::testing::Bool());

IN_PROC_BROWSER_TEST_F(EnrollmentEmbeddedPolicyServerBase, SwitchToViews) {
  TriggerEnrollmentAndSignInSuccessfully();
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSuccess);
  ConfirmAndWaitLoginScreen();
  EXPECT_TRUE(LoginScreenTestApi::IsOobeDialogVisible());
}

IN_PROC_BROWSER_TEST_F(EnrollmentEmbeddedPolicyServerBase,
                       SwitchToViewsLocalUsers) {
  AddPublicUser("test_user");
  TriggerEnrollmentAndSignInSuccessfully();
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSuccess);
  ConfirmAndWaitLoginScreen();
  EXPECT_FALSE(LoginScreenTestApi::IsOobeDialogVisible());
  EXPECT_EQ(LoginScreenTestApi::GetUsersCount(), 1);
}

IN_PROC_BROWSER_TEST_F(EnrollmentEmbeddedPolicyServerBase,
                       SwitchToViewsLocales) {
  auto initial_label = LoginScreenTestApi::GetShutDownButtonLabel();

  SetLoginScreenLocale("ru-RU");
  TriggerEnrollmentAndSignInSuccessfully();
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSuccess);
  ConfirmAndWaitLoginScreen();
  EXPECT_TRUE(LoginScreenTestApi::IsOobeDialogVisible());
  EXPECT_NE(LoginScreenTestApi::GetShutDownButtonLabel(), initial_label);
}

class KioskEnrollmentPolicyServerTest
    : public EnrollmentEmbeddedPolicyServerBase {
 public:
  KioskEnrollmentPolicyServerTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kEnableKioskEnrollmentInOobe);
  }

  void TriggerKioskEnrollmentAndSignInSuccessfully(bool enroll_kiosk = false) {
    host()->HandleAccelerator(LoginAcceleratorAction::kStartKioskEnrollment);
    OobeScreenWaiter(EnrollmentScreenView::kScreenId).Wait();

    ASSERT_FALSE(StartupUtils::IsDeviceRegistered());
    ASSERT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
    WaitForGaiaPageBackButtonUpdate();

    SigninFrameJS().TypeIntoPath(FakeGaiaMixin::kFakeUserEmail,
                                 FakeGaiaMixin::kEmailPath);
    test::OobeJS().ClickOnPath(kEnterprisePrimaryButton);
    SigninFrameJS().TypeIntoPath(FakeGaiaMixin::kFakeUserPassword,
                                 FakeGaiaMixin::kPasswordPath);
    if (enroll_kiosk) {
      test::OobeJS().ClickOnPath(kKioskModeKioskEnrollmentButton);
    } else {
      test::OobeJS().ClickOnPath(kKioskModeEnterpriseEnrollmentButton);
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(KioskEnrollmentPolicyServerTest, KioskEnrollment) {
  policy_server_.SetAvailableLicenses(/*has_enterpise_license=*/false,
                                      /*has_kiosk_license=*/true);
  policy_server_.SetUpdateDeviceAttributesPermission(true);

  TriggerKioskEnrollmentAndSignInSuccessfully(/*enroll_kiosk=*/true);

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepKioskEnrollment);
  enrollment_ui_.ConfirmKioskEnrollment();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepDeviceAttributes);
  enrollment_ui_.SubmitDeviceAttributes(test::values::kAssetId,
                                        test::values::kLocation);
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSuccess);
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
  EXPECT_TRUE(InstallAttributes::Get()->IsCloudManaged());
}

IN_PROC_BROWSER_TEST_F(KioskEnrollmentPolicyServerTest,
                       KioskEnrollmentNoLicenses) {
  policy_server_.SetAvailableLicenses(/*has_enterpise_license=*/false,
                                      /*has_kiosk_license=*/false);
  policy_server_.SetUpdateDeviceAttributesPermission(true);

  TriggerKioskEnrollmentAndSignInSuccessfully(/*enroll_kiosk=*/true);

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepKioskEnrollment);
  enrollment_ui_.ConfirmKioskEnrollment();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_ui_.ExpectErrorMessage(
      IDS_ENTERPRISE_ENROLLMENT_MISSING_LICENSES_ERROR, /*can_retry=*/true);
  enrollment_ui_.RetryAndWaitForSigninStep();
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
}

IN_PROC_BROWSER_TEST_F(KioskEnrollmentPolicyServerTest, EnterpriseEnrollment) {
  policy_server_.SetAvailableLicenses(/*has_enterpise_license=*/true,
                                      /*has_kiosk_license=*/false);
  policy_server_.SetUpdateDeviceAttributesPermission(true);

  TriggerKioskEnrollmentAndSignInSuccessfully(/*enroll_kiosk=*/false);

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepDeviceAttributes);
  enrollment_ui_.SubmitDeviceAttributes(test::values::kAssetId,
                                        test::values::kLocation);
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSuccess);
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
  EXPECT_TRUE(InstallAttributes::Get()->IsCloudManaged());
}

IN_PROC_BROWSER_TEST_F(KioskEnrollmentPolicyServerTest,
                       EnterpriseEnrollmentNoLicenses) {
  policy_server_.SetAvailableLicenses(/*has_enterpise_license=*/false,
                                      /*has_kiosk_license=*/false);
  policy_server_.SetUpdateDeviceAttributesPermission(true);

  TriggerKioskEnrollmentAndSignInSuccessfully(/*enroll_kiosk=*/false);

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_ui_.ExpectErrorMessage(
      IDS_ENTERPRISE_ENROLLMENT_MISSING_LICENSES_ERROR, /*can_retry=*/true);
  enrollment_ui_.RetryAndWaitForSigninStep();
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  EXPECT_FALSE(InstallAttributes::Get()->IsEnterpriseManaged());
}

class KioskEnrollmentTest : public EnrollmentEmbeddedPolicyServerBase {
 public:
  KioskEnrollmentTest() = default;

  // EnrollmentEmbeddedPolicyServerBase:
  void SetUp() override {
    needs_background_networking_ = true;
    skip_splash_wait_override_ =
        KioskLaunchController::SkipSplashScreenWaitForTesting();
    EnrollmentEmbeddedPolicyServerBase::SetUp();
  }

  void SetupAutoLaunchApp(FakeOwnerSettingsService* service) {
    KioskAppManager::Get()->AddApp(KioskAppsMixin::kKioskAppId, service);
    KioskAppManager::Get()->SetAutoLaunchApp(KioskAppsMixin::kKioskAppId,
                                             service);
  }

 private:
  KioskAppsMixin kiosk_apps_{&mixin_host_, embedded_test_server()};
  std::unique_ptr<base::AutoReset<bool>> skip_splash_wait_override_;
};

IN_PROC_BROWSER_TEST_F(KioskEnrollmentTest,
                       ManualEnrollmentAutolaunchKioskApp) {
  TriggerEnrollmentAndSignInSuccessfully();

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSuccess);
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
  EXPECT_TRUE(InstallAttributes::Get()->IsCloudManaged());

  ScopedDeviceSettings settings;

  SetupAutoLaunchApp(settings.owner_settings_service());
  enrollment_screen()->OnConfirmationClosed();

  // Wait for app to be launched.
  KioskSessionInitializedWaiter().Wait();
}

}  // namespace
}  // namespace ash
