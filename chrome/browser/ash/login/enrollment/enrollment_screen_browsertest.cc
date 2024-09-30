// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/enrollment/enrollment_screen.h"

#include <optional>

#include "ash/constants/ash_paths.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/enrollment_helper_mixin.h"
#include "chrome/browser/ash/login/test/enrollment_ui_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_config.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_status.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/ash/login/webui_login_view.h"
#include "chrome/browser/ui/webui/ash/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/online_login_utils.h"
#include "chrome/browser/ui/webui/ash/login/tpm_error_screen_handler.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/dbus/tpm_manager/fake_tpm_manager_client.h"
#include "chromeos/dbus/tpm_manager/tpm_manager_client.h"
#include "chromeos/test/chromeos_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "google_apis/gaia/gaia_urls.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

constexpr char kEnterpriseEnrollment[] = "enterprise-enrollment";

constexpr char kTestEnrollmentToken[] = "test-enrollment-token";

const test::UIPath kEnterpriseEnrollmentDialogue = {kEnterpriseEnrollment,
                                                    "step-signin"};

constexpr char kTestUserEmail[] = "testuser@test.com";
constexpr char kTestUserGaiaId[] = "test_user_gaia_id";
constexpr char kTestUserPassword[] = "test_user_password";

const test::UIPath kEnterpriseEnrollmentSkipDialogue = {
    kEnterpriseEnrollment, "skipConfirmationDialog"};

const test::UIPath kEnterpriseEnrollmentSkipDialogueGoback = {
    kEnterpriseEnrollment, "goBackButton"};

const test::UIPath kEnterpriseEnrollmentSkipDialogueSkip = {
    kEnterpriseEnrollment, "skipButton"};

const test::UIPath kEnrollmentTPMCheckCancelButton = {
    "enterprise-enrollment", "step-tpm-checking", "cancelButton"};

}  // namespace

using ::testing::_;
using ::testing::InvokeWithoutArgs;
using ::testing::Mock;

class EnrollmentScreenTest : public OobeBaseTest {
 public:
  EnrollmentScreenTest(const EnrollmentScreenTest&) = delete;
  EnrollmentScreenTest& operator=(const EnrollmentScreenTest&) = delete;

 protected:
  EnrollmentScreenTest() = default;
  ~EnrollmentScreenTest() override = default;

  // OobeBaseTest:
  bool SetUpUserDataDirectory() override {
    if (!OobeBaseTest::SetUpUserDataDirectory())
      return false;

    // Make sure chrome paths are overridden before proceeding - this is
    // usually done in chrome main, which has not happened yet.
    base::FilePath user_data_dir;
    EXPECT_TRUE(base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir));
    RegisterStubPathOverrides(user_data_dir);

    return true;
  }

  // OobeBaseTest:
  void SetUpOnMainThread() override {
    OobeBaseTest::SetUpOnMainThread();
    LoginDisplayHost::default_host()->StartWizard(
        EnrollmentScreenView::kScreenId);
    OobeScreenWaiter(EnrollmentScreenView::kScreenId).Wait();
  }

  EnrollmentScreen* enrollment_screen() {
    EXPECT_TRUE(WizardController::default_controller());
    EnrollmentScreen* enrollment_screen = EnrollmentScreen::Get(
        WizardController::default_controller()->screen_manager());
    EXPECT_TRUE(enrollment_screen);
    return enrollment_screen;
  }

  login::OnlineSigninArtifacts CreateFakeSigninArtifacts() {
    login::OnlineSigninArtifacts signin_artifacts;
    signin_artifacts.email = kTestUserEmail;
    signin_artifacts.gaia_id = kTestUserGaiaId;
    signin_artifacts.password = kTestUserPassword;
    signin_artifacts.using_saml = false;

    return signin_artifacts;
  }

  policy::EnrollmentConfig CreateConfig(policy::EnrollmentConfig::Mode mode) {
    policy::EnrollmentConfig config;
    config.mode = mode;
    return config;
  }

  test::EnrollmentUIMixin enrollment_ui_{&mixin_host_};
  test::EnrollmentHelperMixin enrollment_helper_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(EnrollmentScreenTest, TestCancel) {
  enrollment_ui_.SetExitHandler();
  enrollment_screen()->OnCancel();
  EnrollmentScreen::Result screen_result = enrollment_ui_.WaitForScreenExit();
  EXPECT_EQ(EnrollmentScreen::Result::BACK, screen_result);

  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
}

IN_PROC_BROWSER_TEST_F(EnrollmentScreenTest, TestSuccess) {
  WizardController::SkipEnrollmentPromptsForTesting();
  enrollment_ui_.SetExitHandler();
  enrollment_screen()->OnDeviceAttributeUpdatePermission(false /* granted */);
  EnrollmentScreen::Result screen_result = enrollment_ui_.WaitForScreenExit();
  EXPECT_EQ(EnrollmentScreen::Result::COMPLETED, screen_result);

  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
}

IN_PROC_BROWSER_TEST_F(EnrollmentScreenTest, EnrollmentSpinner) {
  EnrollmentScreenView* view = enrollment_screen()->GetView();
  ASSERT_TRUE(view);

  // Run through the flow
  view->Show();
  OobeScreenWaiter(EnrollmentScreenView::kScreenId).Wait();
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSignin);

  view->ShowEnrollmentWorkingScreen();
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepWorking);

  view->ShowEnrollmentSuccessScreen();
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSuccess);
}

IN_PROC_BROWSER_TEST_F(EnrollmentScreenTest, EnrollAfterRollbackSuccess) {
  enrollment_ui_.SetExitHandler();
  policy::EnrollmentConfig enrollment_config;
  enrollment_config.mode =
      policy::EnrollmentConfig::MODE_ATTESTATION_ROLLBACK_FORCED;

  enrollment_helper_.ExpectEnrollmentMode(
      policy::EnrollmentConfig::MODE_ATTESTATION_ROLLBACK_FORCED);
  enrollment_helper_.ExpectAttestationEnrollmentSuccess();
  enrollment_helper_.DisableAttributePromptUpdate();
  enrollment_helper_.SetupClearAuth();

  enrollment_screen()->SetEnrollmentConfig(enrollment_config);

  WizardContext context;
  enrollment_screen()->Show(&context);

  enrollment_ui_.WaitForScreenExit();

  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
}

IN_PROC_BROWSER_TEST_F(EnrollmentScreenTest,
                       EnrollAfterRollbackManualFallback) {
  enrollment_ui_.SetExitHandler();
  policy::EnrollmentConfig enrollment_config;
  enrollment_config.mode =
      policy::EnrollmentConfig::MODE_ATTESTATION_ROLLBACK_FORCED;

  enrollment_helper_.ExpectEnrollmentMode(
      policy::EnrollmentConfig::MODE_ATTESTATION_ROLLBACK_FORCED);
  // Manual fallback automatically happens if the device is not found, otherwise
  // error screen is shown.
  enrollment_helper_.ExpectAttestationEnrollmentError(
      policy::EnrollmentStatus::ForRegistrationError(
          policy::DeviceManagementStatus::DM_STATUS_SERVICE_DEVICE_NOT_FOUND));
  enrollment_helper_.SetupClearAuth();

  enrollment_screen()->SetEnrollmentConfig(enrollment_config);

  WizardContext context;
  enrollment_screen()->Show(&context);

  // Expect that the screen ends up on the gaia sign-in page as a manual
  // fallback for the failed automatic enrollment.
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSignin);
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());

  // Enrollment helper mock is owned by enrollment screen and released when
  // enrollment config changes. Need to prepare a new mock to be consumed.
  enrollment_helper_.ResetMock();

  enrollment_helper_.ExpectEnrollmentMode(
      policy::EnrollmentConfig::MODE_ATTESTATION_ROLLBACK_MANUAL_FALLBACK);
  enrollment_helper_.ExpectSuccessfulOAuthEnrollment();
  enrollment_helper_.DisableAttributePromptUpdate();
  enrollment_helper_.SetupClearAuth();

  enrollment_screen()->OnLoginDone(
      CreateFakeSigninArtifacts(),
      static_cast<int>(policy::LicenseType::kEnterprise),
      test::EnrollmentHelperMixin::kTestAuthCode);

  enrollment_ui_.WaitForScreenExit();
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
}

IN_PROC_BROWSER_TEST_F(EnrollmentScreenTest,
                       ErrorInEnrollmentAfterRollbackThenRetry) {
  enrollment_ui_.SetExitHandler();
  policy::EnrollmentConfig enrollment_config;
  enrollment_config.mode =
      policy::EnrollmentConfig::MODE_ATTESTATION_ROLLBACK_FORCED;

  enrollment_helper_.ExpectEnrollmentMode(
      policy::EnrollmentConfig::MODE_ATTESTATION_ROLLBACK_FORCED);
  enrollment_helper_.ExpectAttestationEnrollmentError(
      policy::EnrollmentStatus::ForRegistrationError(
          policy::DeviceManagementStatus::DM_STATUS_HTTP_STATUS_ERROR));
  enrollment_helper_.SetupClearAuth();

  enrollment_screen()->SetEnrollmentConfig(enrollment_config);

  WizardContext context;
  enrollment_screen()->Show(&context);

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());

  // Retry enrollment and finish successfully.

  // Enrollment helper mock is owned by enrollment screen and released when
  // enrollment config changes. Need to prepare a new mock to be consumed.
  enrollment_helper_.ResetMock();

  enrollment_helper_.ExpectEnrollmentMode(
      policy::EnrollmentConfig::MODE_ATTESTATION_ROLLBACK_FORCED);
  enrollment_helper_.ExpectAttestationEnrollmentSuccess();
  enrollment_helper_.DisableAttributePromptUpdate();

  enrollment_screen()->SetEnrollmentConfig(enrollment_config);

  enrollment_screen()->OnRetry();

  enrollment_ui_.WaitForScreenExit();

  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
}

IN_PROC_BROWSER_TEST_F(EnrollmentScreenTest, AttestationEnrollmentSuccess) {
  enrollment_ui_.SetExitHandler();
  policy::EnrollmentConfig enrollment_config;
  enrollment_config.mode =
      policy::EnrollmentConfig::MODE_ATTESTATION_SERVER_FORCED;

  enrollment_helper_.ExpectEnrollmentMode(
      policy::EnrollmentConfig::MODE_ATTESTATION_SERVER_FORCED);
  enrollment_helper_.ExpectAttestationEnrollmentSuccess();
  enrollment_helper_.DisableAttributePromptUpdate();

  enrollment_screen()->SetEnrollmentConfig(enrollment_config);

  WizardContext context;
  enrollment_screen()->Show(&context);

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSuccess);

  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
}

IN_PROC_BROWSER_TEST_F(EnrollmentScreenTest,
                       AttestationEnrollmentManualFallback) {
  enrollment_ui_.SetExitHandler();
  policy::EnrollmentConfig enrollment_config;
  enrollment_config.mode =
      policy::EnrollmentConfig::MODE_ATTESTATION_SERVER_FORCED;

  enrollment_helper_.ExpectEnrollmentMode(
      policy::EnrollmentConfig::MODE_ATTESTATION_SERVER_FORCED);
  // Manual fallback automatically happens if the device is not found, otherwise
  // error screen is shown.
  enrollment_helper_.ExpectAttestationEnrollmentError(
      policy::EnrollmentStatus::ForRegistrationError(
          policy::DeviceManagementStatus::DM_STATUS_SERVICE_DEVICE_NOT_FOUND));
  enrollment_helper_.SetupClearAuth();

  enrollment_screen()->SetEnrollmentConfig(enrollment_config);

  WizardContext context;
  enrollment_screen()->Show(&context);

  // Expect that the screen ends up on the gaia sign-in page as a manual
  // fallback for the failed automatic enrollment.
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSignin);

  // Enrollment helper mock is owned by enrollment screen and released when
  // enrollment config changes. Need to prepare a new mock to be consumed.
  enrollment_helper_.ResetMock();

  enrollment_helper_.ExpectEnrollmentMode(
      policy::EnrollmentConfig::MODE_ATTESTATION_MANUAL_FALLBACK);
  enrollment_helper_.ExpectSuccessfulOAuthEnrollment();
  enrollment_helper_.DisableAttributePromptUpdate();
  enrollment_helper_.SetupClearAuth();

  enrollment_screen()->OnLoginDone(
      CreateFakeSigninArtifacts(),
      static_cast<int>(policy::LicenseType::kEnterprise),
      test::EnrollmentHelperMixin::kTestAuthCode);

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSuccess);
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
}

IN_PROC_BROWSER_TEST_F(EnrollmentScreenTest, SkipEnrollmentDialogueGoBack) {
  enrollment_ui_.SetExitHandler();
  policy::EnrollmentConfig enrollment_config;
  enrollment_config.mode = policy::EnrollmentConfig::MODE_MANUAL;

  enrollment_config.is_license_packaged_with_device = true;

  enrollment_screen()->SetEnrollmentConfig(enrollment_config);
  enrollment_helper_.ResetMock();

  WizardContext context;
  enrollment_screen()->Show(&context);

  OobeScreenWaiter(EnrollmentScreenView::kScreenId).Wait();
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSignin);

  test::OobeJS().ExpectVisiblePath(kEnterpriseEnrollmentDialogue);
  LoginDisplayHost::default_host()->HandleAccelerator(
      LoginAcceleratorAction::kCancelScreenAction);

  test::OobeJS().ExpectDialogOpen(kEnterpriseEnrollmentSkipDialogue);
  test::OobeJS().ClickOnPath(kEnterpriseEnrollmentSkipDialogueGoback);
  test::OobeJS().ExpectDialogClosed(kEnterpriseEnrollmentSkipDialogue);
  test::OobeJS().ExpectVisiblePath(kEnterpriseEnrollmentDialogue);
}

IN_PROC_BROWSER_TEST_F(EnrollmentScreenTest,
                       SkipEnrollmentDialogueGoBackWithFRE) {
  enrollment_ui_.SetExitHandler();
  policy::EnrollmentConfig enrollment_config;

  // Set enrollment config to Forced Re-enrollment.
  enrollment_config.mode = policy::EnrollmentConfig::MODE_LOCAL_FORCED;

  enrollment_config.is_license_packaged_with_device = true;

  enrollment_screen()->SetEnrollmentConfig(enrollment_config);
  enrollment_helper_.ResetMock();

  WizardContext context;
  enrollment_screen()->Show(&context);

  OobeScreenWaiter(EnrollmentScreenView::kScreenId).Wait();
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSignin);

  test::OobeJS().ExpectVisiblePath(kEnterpriseEnrollmentDialogue);
  LoginDisplayHost::default_host()->HandleAccelerator(
      LoginAcceleratorAction::kCancelScreenAction);

  // Check Dialogue is not open and Enrollment Screen is visible.
  test::OobeJS().ExpectDialogClosed(kEnterpriseEnrollmentSkipDialogue);
  test::OobeJS().ExpectVisiblePath(kEnterpriseEnrollmentDialogue);
}

IN_PROC_BROWSER_TEST_F(EnrollmentScreenTest,
                       SkipEnrollmentDialogueSkipConfirmation) {
  enrollment_ui_.SetExitHandler();
  policy::EnrollmentConfig enrollment_config;
  enrollment_config.mode = policy::EnrollmentConfig::MODE_MANUAL;

  enrollment_config.is_license_packaged_with_device = true;

  enrollment_screen()->SetEnrollmentConfig(enrollment_config);
  enrollment_helper_.ResetMock();

  WizardContext context;
  enrollment_screen()->Show(&context);

  OobeScreenWaiter(EnrollmentScreenView::kScreenId).Wait();
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSignin);

  test::OobeJS().ExpectVisiblePath(kEnterpriseEnrollmentDialogue);

  LoginDisplayHost::default_host()->HandleAccelerator(
      LoginAcceleratorAction::kCancelScreenAction);

  test::OobeJS().ExpectDialogOpen(kEnterpriseEnrollmentSkipDialogue);

  test::OobeJS().ClickOnPath(kEnterpriseEnrollmentSkipDialogueSkip);
  test::OobeJS().ExpectDialogClosed(kEnterpriseEnrollmentSkipDialogue);

  EnrollmentScreen::Result screen_result = enrollment_ui_.WaitForScreenExit();
  EXPECT_EQ(EnrollmentScreen::Result::BACK, screen_result);
}

IN_PROC_BROWSER_TEST_F(EnrollmentScreenTest, SkipEnrollmentDialogueNoLPDevice) {
  enrollment_ui_.SetExitHandler();
  policy::EnrollmentConfig enrollment_config;
  enrollment_config.mode = policy::EnrollmentConfig::MODE_MANUAL;

  enrollment_config.is_license_packaged_with_device = false;

  enrollment_screen()->SetEnrollmentConfig(enrollment_config);
  enrollment_helper_.ResetMock();

  WizardContext context;
  enrollment_screen()->Show(&context);

  OobeScreenWaiter(EnrollmentScreenView::kScreenId).Wait();
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSignin);

  test::OobeJS().ExpectVisiblePath(kEnterpriseEnrollmentDialogue);

  LoginDisplayHost::default_host()->HandleAccelerator(
      LoginAcceleratorAction::kCancelScreenAction);

  EnrollmentScreen::Result screen_result = enrollment_ui_.WaitForScreenExit();
  EXPECT_EQ(EnrollmentScreen::Result::BACK, screen_result);
}

IN_PROC_BROWSER_TEST_F(EnrollmentScreenTest, ManualEnrollmentSuccess) {
  enrollment_ui_.SetExitHandler();
  policy::EnrollmentConfig enrollment_config;
  enrollment_config.mode = policy::EnrollmentConfig::MODE_MANUAL_REENROLLMENT;

  enrollment_helper_.ExpectEnrollmentMode(
      policy::EnrollmentConfig::MODE_MANUAL_REENROLLMENT);
  enrollment_helper_.ExpectSuccessfulOAuthEnrollment();
  enrollment_helper_.DisableAttributePromptUpdate();
  enrollment_helper_.SetupClearAuth();

  enrollment_screen()->SetEnrollmentConfig(enrollment_config);

  WizardContext context;
  enrollment_screen()->Show(&context);

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSignin);

  enrollment_screen()->OnLoginDone(
      CreateFakeSigninArtifacts(),
      static_cast<int>(policy::LicenseType::kEnterprise),
      test::EnrollmentHelperMixin::kTestAuthCode);

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSuccess);
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
}

IN_PROC_BROWSER_TEST_F(EnrollmentScreenTest,
                       SuccessStepPreservedAfterNetworkErrorScreen) {
  WizardContext context;
  enrollment_helper_.ExpectAttestationEnrollmentSuccess();
  enrollment_helper_.DisableAttributePromptUpdate();
  enrollment_screen()->SetEnrollmentConfig(
      CreateConfig(policy::EnrollmentConfig::MODE_ATTESTATION_SERVER_FORCED));
  enrollment_screen()->Show(&context);
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSuccess);

  enrollment_screen()->SetNetworkStateForTesting(nullptr);
  OobeScreenWaiter(ErrorScreenView::kScreenId).Wait();
  enrollment_screen()->SetNetworkStateForTesting(
      NetworkHandler::Get()->network_state_handler()->DefaultNetwork());
  OobeScreenWaiter(EnrollmentScreenView::kScreenId).Wait();

  enrollment_ui_.ExpectStepVisibility(true, test::ui::kEnrollmentStepSuccess);
}

IN_PROC_BROWSER_TEST_F(EnrollmentScreenTest,
                       ShowsWorkingStepOnAttestationFlow) {
  WizardContext context;
  enrollment_screen()->SetEnrollmentConfig(
      CreateConfig(policy::EnrollmentConfig::MODE_ATTESTATION_SERVER_FORCED));

  enrollment_screen()->Show(&context);
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepWorking);
}

IN_PROC_BROWSER_TEST_F(EnrollmentScreenTest,
                       ShowsWorkingStepAfterAttestationRetry) {
  WizardContext context;
  enrollment_helper_.ExpectAttestationEnrollmentError(
      policy::EnrollmentStatus::ForRegistrationError(
          policy::DeviceManagementStatus::DM_STATUS_TEMPORARY_UNAVAILABLE));
  enrollment_helper_.SetupClearAuth();
  enrollment_screen()->SetEnrollmentConfig(
      CreateConfig(policy::EnrollmentConfig::MODE_ATTESTATION_SERVER_FORCED));
  enrollment_screen()->Show(&context);
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);

  enrollment_helper_.ResetMock();
  enrollment_ui_.RetryAfterError();
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepWorking);
}

IN_PROC_BROWSER_TEST_F(EnrollmentScreenTest, TokenBasedEnrollmentSuccess) {
  enrollment_ui_.SetExitHandler();
  policy::EnrollmentConfig enrollment_config;
  enrollment_config.mode =
      policy::EnrollmentConfig::MODE_ENROLLMENT_TOKEN_INITIAL_SERVER_FORCED;
  enrollment_config.enrollment_token = kTestEnrollmentToken;

  enrollment_helper_.ExpectEnrollmentTokenConfig(kTestEnrollmentToken);
  enrollment_helper_.ExpectTokenBasedEnrollmentSuccess();
  enrollment_helper_.DisableAttributePromptUpdate();

  enrollment_screen()->SetEnrollmentConfig(enrollment_config);

  WizardContext context;
  enrollment_screen()->Show(&context);

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSuccess);

  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
}

// TODO(b/320497330): Add more browser tests for token-based kiosk enrollment
// and non-fallback error handling.
IN_PROC_BROWSER_TEST_F(EnrollmentScreenTest,
                       TokenBasedEnrollmentManualFallback) {
  enrollment_ui_.SetExitHandler();
  policy::EnrollmentConfig enrollment_config;
  enrollment_config.mode =
      policy::EnrollmentConfig::MODE_ENROLLMENT_TOKEN_INITIAL_SERVER_FORCED;
  enrollment_config.enrollment_token = kTestEnrollmentToken;

  enrollment_helper_.ExpectEnrollmentTokenConfig(kTestEnrollmentToken);

  enrollment_helper_.ExpectTokenBasedEnrollmentError(
      policy::EnrollmentStatus::ForRegistrationError(
          policy::DeviceManagementStatus::DM_STATUS_TEMPORARY_UNAVAILABLE));
  enrollment_helper_.SetupClearAuth();

  enrollment_screen()->SetEnrollmentConfig(enrollment_config);

  WizardContext context;
  enrollment_screen()->Show(&context);

  // Expect the error screen, and trigger manual enrollment to go to the Gaia
  // sign-in screen.
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);
  enrollment_screen()->OnCancel();
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSignin);

  // Enrollment helper mock is owned by enrollment screen and released when
  // enrollment config changes. Need to prepare a new mock to be consumed.
  enrollment_helper_.ResetMock();

  enrollment_helper_.ExpectEnrollmentMode(
      policy::EnrollmentConfig::MODE_ENROLLMENT_TOKEN_INITIAL_MANUAL_FALLBACK);

  enrollment_helper_.ExpectSuccessfulOAuthEnrollment();
  enrollment_helper_.DisableAttributePromptUpdate();
  enrollment_helper_.SetupClearAuth();

  enrollment_screen()->OnLoginDone(
      CreateFakeSigninArtifacts(),
      static_cast<int>(policy::LicenseType::kEnterprise),
      test::EnrollmentHelperMixin::kTestAuthCode);

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSuccess);
  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
}

struct EnrollmentErrorScreenTestParams {
  policy::EnrollmentConfig::Mode enrollment_mode;
};

class EnrollmentErrorScreenTest
    : public EnrollmentScreenTest,
      public ::testing::WithParamInterface<EnrollmentErrorScreenTestParams> {
 protected:
  policy::EnrollmentConfig GetEnrollmentConfigParam() {
    policy::EnrollmentConfig config;
    config.mode = GetParam().enrollment_mode;
    return config;
  }

  // Replicates the logic of `EnrollmentModeToUIMode` categorizing enrollment
  // modes into manual and not manual.
  // TODO(b/238986105): Remove once once `EnrollmentModeToUIMode` is fixed.
  bool IsManualEnrollmentMode(policy::EnrollmentConfig::Mode mode) const {
    switch (mode) {
      case policy::EnrollmentConfig::MODE_NONE:
        NOTREACHED() << "Bad enrollment mode " << mode;
      case policy::EnrollmentConfig::MODE_MANUAL:
      case policy::EnrollmentConfig::MODE_MANUAL_REENROLLMENT:
      case policy::EnrollmentConfig::MODE_LOCAL_ADVERTISED:
      case policy::EnrollmentConfig::MODE_SERVER_ADVERTISED:
      case policy::EnrollmentConfig::MODE_ATTESTATION:
        return true;
      case policy::EnrollmentConfig::MODE_LOCAL_FORCED:
      case policy::EnrollmentConfig::MODE_SERVER_FORCED:
      case policy::EnrollmentConfig::MODE_ATTESTATION_LOCAL_FORCED:
      case policy::EnrollmentConfig::MODE_ATTESTATION_SERVER_FORCED:
      case policy::EnrollmentConfig::MODE_ATTESTATION_MANUAL_FALLBACK:
      case policy::EnrollmentConfig::MODE_INITIAL_SERVER_FORCED:
      case policy::EnrollmentConfig::MODE_ATTESTATION_INITIAL_SERVER_FORCED:
      case policy::EnrollmentConfig::MODE_ATTESTATION_INITIAL_MANUAL_FALLBACK:
      case policy::EnrollmentConfig::MODE_ATTESTATION_ROLLBACK_FORCED:
      case policy::EnrollmentConfig::MODE_ATTESTATION_ROLLBACK_MANUAL_FALLBACK:
      case policy::EnrollmentConfig::MODE_RECOVERY:
      case policy::EnrollmentConfig::
          MODE_ENROLLMENT_TOKEN_INITIAL_SERVER_FORCED:
      case policy::EnrollmentConfig::
          MODE_ENROLLMENT_TOKEN_INITIAL_MANUAL_FALLBACK:
        return false;
    }
  }
};

using ManualEnrollmentErrorScreenTest = EnrollmentErrorScreenTest;

IN_PROC_BROWSER_TEST_P(ManualEnrollmentErrorScreenTest,
                       ManualEnrollmentErrorAndScreenData) {
  enrollment_ui_.SetExitHandler();
  const policy::EnrollmentConfig enrollment_config = GetEnrollmentConfigParam();
  ASSERT_TRUE(enrollment_config.is_mode_oauth());
  ASSERT_FALSE(enrollment_config.is_mode_attestation());

  enrollment_helper_.ExpectEnrollmentMode(enrollment_config.mode);
  // The test expects the error screen to be shown. Avoid automatic fallback
  enrollment_helper_.ExpectOAuthEnrollmentError(
      policy::EnrollmentStatus::ForEnrollmentCode(
          policy::EnrollmentStatus::Code::kRegistrationFailed));
  enrollment_helper_.SetupClearAuth();

  enrollment_screen()->SetEnrollmentConfig(enrollment_config);

  WizardContext context;
  enrollment_screen()->Show(&context);

  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSignin);

  // TODO(crbug/1342179) These checks (and some additional ones that are hard to
  // do from here) should be covered by a unit test for
  // `EnrollmentScreenHandler` instead of here.
  // TODO(b/238986105): change the expectation on `isManualEnrollment_` to
  // EXPECT_TRUE once `EnrollmentModeToUIMode` is fixed.
  EXPECT_EQ(test::OobeJS().GetAttributeBool("isManualEnrollment",
                                            {"enterprise-enrollment"}),
            IsManualEnrollmentMode(enrollment_config.mode));
  EXPECT_EQ(
      test::OobeJS().GetAttributeBool("isForced", {"enterprise-enrollment"}),
      enrollment_config.is_forced());
  EXPECT_FALSE(test::OobeJS().GetAttributeBool("isAutoEnroll",
                                               {"enterprise-enrollment"}));
  EXPECT_FALSE(test::OobeJS().GetAttributeBool("hasAccountCheck",
                                               {"enterprise-enrollment"}));
  EXPECT_EQ(test::OobeJS().GetAttributeString("gaiaDialogButtonsType",
                                              {"enterprise-enrollment"}),
            "enterprise-preferred");
  EXPECT_EQ(test::OobeJS().GetAttributeString("authenticator.idpOrigin_",
                                              {"enterprise-enrollment"}),
            GaiaUrls::GetInstance()->gaia_url().spec());
  EXPECT_EQ(test::OobeJS().GetAttributeString("authenticator.clientId_",
                                              {"enterprise-enrollment"}),
            GaiaUrls::GetInstance()->oauth2_chrome_client_id());
  EXPECT_FALSE(test::OobeJS().GetAttributeBool("authenticator.needPassword",
                                               {"enterprise-enrollment"}));
  EXPECT_TRUE(test::OobeJS().GetAttributeBool(
      "authenticator.enableGaiaActionButtons_", {"enterprise-enrollment"}));

  test::OobeJS().ExpectHasNoAttribute("licenseType", {"enterprise-enrollment"});

  enrollment_screen()->OnLoginDone(
      CreateFakeSigninArtifacts(),
      static_cast<int>(policy::LicenseType::kEnterprise),
      test::EnrollmentHelperMixin::kTestAuthCode);

  // Expect that the screen ends up on error screen.
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);

  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  if (enrollment_config.is_forced()) {
    EXPECT_TRUE(
        test::OobeJS().GetAttributeBool("isForced", {"enterprise-enrollment"}));
    // TODO(b/238175743) isCancelDisabled also blocks manual fallback. Figure
    // out what we want here and fix naming.
    // EXPECT_TRUE(test::OobeJS().GetAttributeBool("isCancelDisabled",
    //                                             {"enterprise-enrollment"}));
  } else {
    EXPECT_FALSE(
        test::OobeJS().GetAttributeBool("isForced", {"enterprise-enrollment"}));
    // TODO(b/238175743) isCancelDisabled also blocks manual fallback. Figure
    // out what we want here and fix naming.
    // EXPECT_FALSE(test::OobeJS().GetAttributeBool("isCancelDisabled",
    //                                              {"enterprise-enrollment"}));
  }
}

INSTANTIATE_TEST_SUITE_P(
    ForcedEnrollment,
    ManualEnrollmentErrorScreenTest,
    testing::ValuesIn(std::vector<EnrollmentErrorScreenTestParams>{
        {policy::EnrollmentConfig::MODE_LOCAL_FORCED},
        {policy::EnrollmentConfig::MODE_SERVER_FORCED},
        {policy::EnrollmentConfig::MODE_RECOVERY},
        {policy::EnrollmentConfig::MODE_ATTESTATION_MANUAL_FALLBACK},
        {policy::EnrollmentConfig::MODE_ATTESTATION_INITIAL_MANUAL_FALLBACK},
        {policy::EnrollmentConfig::
             MODE_ATTESTATION_ROLLBACK_MANUAL_FALLBACK}}));

INSTANTIATE_TEST_SUITE_P(
    NotForcedEnrollment,
    ManualEnrollmentErrorScreenTest,
    testing::ValuesIn(std::vector<EnrollmentErrorScreenTestParams>{
        {policy::EnrollmentConfig::MODE_MANUAL},
        {policy::EnrollmentConfig::MODE_MANUAL_REENROLLMENT},
        {policy::EnrollmentConfig::MODE_LOCAL_ADVERTISED},
        {policy::EnrollmentConfig::MODE_SERVER_ADVERTISED}}));

using AttestationEnrollmentErrorScreenTest = EnrollmentErrorScreenTest;

IN_PROC_BROWSER_TEST_P(AttestationEnrollmentErrorScreenTest,
                       AttestationEnrollmentErrorAndScreenData) {
  enrollment_ui_.SetExitHandler();
  const policy::EnrollmentConfig enrollment_config = GetEnrollmentConfigParam();
  ASSERT_TRUE(enrollment_config.is_automatic_enrollment());
  ASSERT_TRUE(enrollment_config.is_mode_attestation());

  // The test expects the error screen to be shown. Avoid automatic fallback
  // to manual enrollment.
  enrollment_helper_.ExpectAttestationEnrollmentError(
      policy::EnrollmentStatus::ForRegistrationError(
          policy::DeviceManagementStatus::DM_STATUS_REQUEST_INVALID));
  enrollment_helper_.SetupClearAuth();
  enrollment_helper_.ExpectEnrollmentMode(enrollment_config.mode);

  enrollment_screen()->SetEnrollmentConfig(enrollment_config);

  WizardContext context;
  enrollment_screen()->Show(&context);

  // TODO(b/238986105): change the expectation on `isManualEnrollment_` to
  // EXPECT_TRUE once `EnrollmentModeToUIMode` is fixed.
  EXPECT_EQ(test::OobeJS().GetAttributeBool("isManualEnrollment",
                                            {"enterprise-enrollment"}),
            IsManualEnrollmentMode(enrollment_config.mode));
  EXPECT_EQ(
      test::OobeJS().GetAttributeBool("isForced", {"enterprise-enrollment"}),
      enrollment_config.is_forced());
  EXPECT_TRUE(test::OobeJS().GetAttributeBool("isAutoEnroll",
                                              {"enterprise-enrollment"}));
  EXPECT_FALSE(test::OobeJS().GetAttributeBool("hasAccountCheck_",
                                               {"enterprise-enrollment"}));

  test::OobeJS().ExpectHasNoAttribute("authenticator.idpOrigin_",
                                      {"enterprise-enrollment"});
  test::OobeJS().ExpectHasNoAttribute("authenticator.clientId_",
                                      {"enterprise-enrollment"});
  test::OobeJS().ExpectHasNoAttribute("authenticator.needPassword",
                                      {"enterprise-enrollment"});
  test::OobeJS().ExpectHasNoAttribute("authenticator.enableGaiaActionButtons_",
                                      {"enterprise-enrollment"});
  test::OobeJS().ExpectHasNoAttribute("gaiaDialogButtonsType",
                                      {"enterprise-enrollment"});
  test::OobeJS().ExpectHasNoAttribute("licenseType", {"enterprise-enrollment"});

  // Expect that the screen ends up on error screen.
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepError);

  EXPECT_FALSE(StartupUtils::IsDeviceRegistered());
  if (enrollment_config.is_forced()) {
    EXPECT_TRUE(
        test::OobeJS().GetAttributeBool("isForced", {"enterprise-enrollment"}));
    // TODO(b/238175743) isCancelDisabled also blocks manual fallback. Figure
    // out what we want here and fix naming.
    // EXPECT_TRUE(test::OobeJS().GetAttributeBool("isCancelDisabled",
    //                                             {"enterprise-enrollment"}));
  } else {
    EXPECT_FALSE(
        test::OobeJS().GetAttributeBool("isForced", {"enterprise-enrollment"}));
    // TODO(b/238175743) isCancelDisabled also blocks manual fallback. Figure
    // out what we want here and fix naming.
    // EXPECT_FALSE(test::OobeJS().GetAttributeBool("isCancelDisabled",
    //                                              {"enterprise-enrollment"}));
  }
}

INSTANTIATE_TEST_SUITE_P(
    ForcedEnrollment,
    AttestationEnrollmentErrorScreenTest,
    testing::ValuesIn(std::vector<EnrollmentErrorScreenTestParams>{
        {policy::EnrollmentConfig::MODE_ATTESTATION_LOCAL_FORCED},
        {policy::EnrollmentConfig::MODE_ATTESTATION_SERVER_FORCED},
        {policy::EnrollmentConfig::MODE_ATTESTATION_ROLLBACK_FORCED},
        {policy::EnrollmentConfig::MODE_ATTESTATION_INITIAL_SERVER_FORCED}}));

INSTANTIATE_TEST_SUITE_P(
    NotForcedEnrollment,
    AttestationEnrollmentErrorScreenTest,
    testing::ValuesIn(std::vector<EnrollmentErrorScreenTestParams>{
        {policy::EnrollmentConfig::MODE_ATTESTATION}}));

// Class to test TPM pre-enrollment check that happens only with
// --tpm-is-dynamic switch enabled. Test parameter represents take TPM
// ownership reply possible statuses.
class EnrollmentScreenDynamicTPMTest
    : public EnrollmentScreenTest,
      public ::testing::WithParamInterface<::tpm_manager::TpmManagerStatus> {
 public:
  EnrollmentScreenDynamicTPMTest() = default;
  EnrollmentScreenDynamicTPMTest(const EnrollmentScreenDynamicTPMTest&) =
      delete;
  EnrollmentScreenDynamicTPMTest& operator=(
      const EnrollmentScreenDynamicTPMTest&) = delete;

  ~EnrollmentScreenDynamicTPMTest() override = default;

  // EnrollmentScreenTest:
  void SetUpOnMainThread() override {
    original_tpm_check_callback_ =
        enrollment_screen()->get_tpm_ownership_callback_for_testing();
    enrollment_screen()->set_tpm_ownership_callback_for_testing(base::BindOnce(
        &EnrollmentScreenDynamicTPMTest::HandleTakeTPMOwnershipResponse,
        base::Unretained(this)));

    enrollment_ui_.SetExitHandler();
    EnrollmentScreenTest::SetUpOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    EnrollmentScreenTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kTpmIsDynamic);
  }

  void WaitForTPMCheckReply() {
    if (tpm_reply_.has_value()) {
      std::move(original_tpm_check_callback_).Run(tpm_reply_.value());
      return;
    }

    base::RunLoop run_loop;
    tpm_check_callback_ = run_loop.QuitClosure();
    run_loop.Run();
    std::move(original_tpm_check_callback_).Run(tpm_reply_.value());
  }

  bool tpm_is_owned() { return tpm_is_owned_; }
  EnrollmentScreen::TpmStatusCallback original_tpm_check_callback_;
  std::optional<::tpm_manager::TakeOwnershipReply> tpm_reply_;

 private:
  void HandleTakeTPMOwnershipResponse(
      const ::tpm_manager::TakeOwnershipReply& reply) {
    EXPECT_FALSE(tpm_reply_.has_value());
    tpm_reply_ = reply;
    // Here we substitute fake reply with status that we want to test.
    tpm_reply_.value().set_status(GetParam());

    if (tpm_check_callback_)
      std::move(tpm_check_callback_).Run();
  }

  base::OnceClosure tpm_check_callback_;
  bool tpm_is_owned_ = false;
};

IN_PROC_BROWSER_TEST_P(EnrollmentScreenDynamicTPMTest, TPMCheckCompleted) {
  switch (GetParam()) {
    case ::tpm_manager::STATUS_DEVICE_ERROR: {
      enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepTPMChecking);
      WaitForTPMCheckReply();
      EnrollmentScreen::Result screen_result =
          enrollment_ui_.WaitForScreenExit();
      EXPECT_EQ(screen_result, EnrollmentScreen::Result::TPM_ERROR);
      return;
    }
    case ::tpm_manager::STATUS_SUCCESS:
    case ::tpm_manager::STATUS_NOT_AVAILABLE:
      enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepTPMChecking);
      WaitForTPMCheckReply();
      enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSignin);
      return;
    case ::tpm_manager::STATUS_DBUS_ERROR: {
      enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepTPMChecking);
      WaitForTPMCheckReply();
      EnrollmentScreen::Result screen_result =
          enrollment_ui_.WaitForScreenExit();
      EXPECT_EQ(screen_result, EnrollmentScreen::Result::TPM_DBUS_ERROR);
      return;
    }
  }
}

IN_PROC_BROWSER_TEST_P(EnrollmentScreenDynamicTPMTest, TPMCheckCanceled) {
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepTPMChecking);
  test::OobeJS().TapOnPath(kEnrollmentTPMCheckCancelButton);
  EnrollmentScreen::Result screen_result = enrollment_ui_.WaitForScreenExit();
  EXPECT_EQ(screen_result, EnrollmentScreen::Result::BACK);
}

INSTANTIATE_TEST_SUITE_P(All,
                         EnrollmentScreenDynamicTPMTest,
                         ::testing::Values(::tpm_manager::STATUS_SUCCESS,
                                           ::tpm_manager::STATUS_DEVICE_ERROR,
                                           ::tpm_manager::STATUS_NOT_AVAILABLE,
                                           ::tpm_manager::STATUS_DBUS_ERROR));

class ProvisionedEnrollmentScreenTest : public EnrollmentScreenTest {
 public:
  ProvisionedEnrollmentScreenTest() = default;

  ProvisionedEnrollmentScreenTest(const ProvisionedEnrollmentScreenTest&) =
      delete;
  ProvisionedEnrollmentScreenTest& operator=(
      const ProvisionedEnrollmentScreenTest&) = delete;

  ~ProvisionedEnrollmentScreenTest() override = default;

 private:
  // EnrollmentScreenTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EnrollmentScreenTest::SetUpCommandLine(command_line);
    base::FilePath test_data_dir;
    ASSERT_TRUE(chromeos::test_utils::GetTestDataPath(
        "app_mode", "kiosk_manifest", &test_data_dir));
    command_line->AppendSwitchPath(
        switches::kAppOemManifestFile,
        test_data_dir.AppendASCII("kiosk_manifest.json"));
  }
};

IN_PROC_BROWSER_TEST_F(ProvisionedEnrollmentScreenTest, TestBackButton) {
  enrollment_ui_.SetExitHandler();
  enrollment_screen()->OnCancel();
  EnrollmentScreen::Result screen_result = enrollment_ui_.WaitForScreenExit();
  EXPECT_EQ(EnrollmentScreen::Result::BACK_TO_AUTO_ENROLLMENT_CHECK,
            screen_result);
}

class OobeCompletedUnownedTest : public OobeBaseTest {
  DeviceStateMixin device_state{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_UNOWNED};
};

// Tests that enrollment screen could be triggered after OOBE completed and
// Chrome restarted (or device rebooted).
IN_PROC_BROWSER_TEST_F(OobeCompletedUnownedTest, TriggerEnrollment) {
  OobeScreenWaiter(GetFirstSigninScreen()).Wait();
  LoginDisplayHost::default_host()->StartWizard(
      EnrollmentScreenView::kScreenId);
  OobeScreenWaiter(EnrollmentScreenView::kScreenId).Wait();
}

}  // namespace ash
