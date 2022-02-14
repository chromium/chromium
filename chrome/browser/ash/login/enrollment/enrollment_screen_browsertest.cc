// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_paths.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/login/enrollment/enrollment_screen.h"
#include "chrome/browser/ash/login/enrollment/mock_enrollment_screen.h"
#include "chrome/browser/ash/login/login_wizard.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/enrollment_ui_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/ui/webui_login_view.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/chromeos/login/tpm_error_screen_handler.h"
#include "chromeos/dbus/tpm_manager/fake_tpm_manager_client.h"
#include "chromeos/dbus/tpm_manager/tpm_manager_client.h"
#include "chromeos/test/chromeos_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace {
const test::UIPath kEnrollmentTPMCheckCancelButton = {
    "enterprise-enrollment", "step-tpm-checking", "cancelButton"};
}  // namespace

using ::testing::_;
using ::testing::InvokeWithoutArgs;
using ::testing::Mock;

class EnrollmentScreenTest : public OobeBaseTest {
 public:
  EnrollmentScreenTest() = default;

  EnrollmentScreenTest(const EnrollmentScreenTest&) = delete;
  EnrollmentScreenTest& operator=(const EnrollmentScreenTest&) = delete;

  ~EnrollmentScreenTest() override = default;

  // OobeBaseTest:
  bool SetUpUserDataDirectory() override {
    if (!OobeBaseTest::SetUpUserDataDirectory())
      return false;

    // Make sure chrome paths are overridden before proceeding - this is usually
    // done in chrome main, which has not happened yet.
    base::FilePath user_data_dir;
    EXPECT_TRUE(base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir));
    ash::RegisterStubPathOverrides(user_data_dir);

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

  test::EnrollmentUIMixin enrollment_ui_{&mixin_host_};
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

class EnrollmentScreenHandsOffTest : public EnrollmentScreenTest {
 public:
  EnrollmentScreenHandsOffTest() = default;
  ~EnrollmentScreenHandsOffTest() override = default;

  EnrollmentScreenHandsOffTest(const EnrollmentScreenHandsOffTest&) = delete;
  EnrollmentScreenHandsOffTest& operator=(const EnrollmentScreenHandsOffTest&) =
      delete;

  // EnrollmentScreenTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EnrollmentScreenTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kEnterpriseEnableZeroTouchEnrollment, "hands-off");
  }
};

IN_PROC_BROWSER_TEST_F(EnrollmentScreenHandsOffTest,
                       SkipEnrollmentCompleteScreen) {
  enrollment_ui_.SetExitHandler();
  enrollment_screen()->OnDeviceAttributeUpdatePermission(false /* granted */);
  EnrollmentScreen::Result screen_result = enrollment_ui_.WaitForScreenExit();
  EXPECT_EQ(EnrollmentScreen::Result::COMPLETED, screen_result);

  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
}

class EnrollmentScreenChromadMigrationTest : public EnrollmentScreenTest {
 public:
  EnrollmentScreenChromadMigrationTest() = default;
  ~EnrollmentScreenChromadMigrationTest() override = default;

  EnrollmentScreenChromadMigrationTest(
      const EnrollmentScreenChromadMigrationTest&) = delete;
  EnrollmentScreenChromadMigrationTest& operator=(
      const EnrollmentScreenChromadMigrationTest&) = delete;

  // EnrollmentScreenTest:
  bool SetUpUserDataDirectory() override {
    if (!EnrollmentScreenTest::SetUpUserDataDirectory())
      return false;

    base::FilePath preinstalled_components_dir;
    EXPECT_TRUE(base::PathService::Get(ash::DIR_PREINSTALLED_COMPONENTS,
                                       &preinstalled_components_dir));

    base::FilePath preserve_dir =
        preinstalled_components_dir.AppendASCII("preserve/");
    EXPECT_TRUE(base::CreateDirectory(preserve_dir));
    EXPECT_TRUE(base::WriteFile(
        preserve_dir.AppendASCII("chromad_migration_skip_oobe"), "1"));

    return true;
  }
};

IN_PROC_BROWSER_TEST_F(EnrollmentScreenChromadMigrationTest,
                       SkipEnrollmentCompleteScreen) {
  enrollment_ui_.SetExitHandler();
  enrollment_screen()->OnDeviceAttributeUpdatePermission(false /* granted */);
  EnrollmentScreen::Result screen_result = enrollment_ui_.WaitForScreenExit();
  EXPECT_EQ(EnrollmentScreen::Result::COMPLETED, screen_result);

  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
}

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
  absl::optional<::tpm_manager::TakeOwnershipReply> tpm_reply_;

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

class AttestationAuthEnrollmentScreenTest : public EnrollmentScreenTest {
 public:
  AttestationAuthEnrollmentScreenTest() = default;

  AttestationAuthEnrollmentScreenTest(
      const AttestationAuthEnrollmentScreenTest&) = delete;
  AttestationAuthEnrollmentScreenTest& operator=(
      const AttestationAuthEnrollmentScreenTest&) = delete;

  ~AttestationAuthEnrollmentScreenTest() override = default;

 private:
  // EnrollmentScreenTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EnrollmentScreenTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnterpriseEnableZeroTouchEnrollment);
  }
};

IN_PROC_BROWSER_TEST_F(AttestationAuthEnrollmentScreenTest, TestCancel) {
  ASSERT_FALSE(enrollment_screen()->AdvanceToNextAuth());
  enrollment_ui_.SetExitHandler();
  enrollment_screen()->OnCancel();
  EnrollmentScreen::Result screen_result = enrollment_ui_.WaitForScreenExit();
  EXPECT_EQ(EnrollmentScreen::Result::BACK, screen_result);
}

class ForcedAttestationAuthEnrollmentScreenTest : public EnrollmentScreenTest {
 public:
  ForcedAttestationAuthEnrollmentScreenTest() = default;

  ForcedAttestationAuthEnrollmentScreenTest(
      const ForcedAttestationAuthEnrollmentScreenTest&) = delete;
  ForcedAttestationAuthEnrollmentScreenTest& operator=(
      const ForcedAttestationAuthEnrollmentScreenTest&) = delete;

  ~ForcedAttestationAuthEnrollmentScreenTest() override = default;

 private:
  // EnrollmentScreenTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EnrollmentScreenTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kEnterpriseEnableZeroTouchEnrollment, "forced");
  }
};

IN_PROC_BROWSER_TEST_F(ForcedAttestationAuthEnrollmentScreenTest, TestCancel) {
  ASSERT_FALSE(enrollment_screen()->AdvanceToNextAuth());
  enrollment_ui_.SetExitHandler();
  enrollment_screen()->OnCancel();
  EnrollmentScreen::Result screen_result = enrollment_ui_.WaitForScreenExit();
  EXPECT_EQ(EnrollmentScreen::Result::BACK_TO_AUTO_ENROLLMENT_CHECK,
            screen_result);
}

class MultiAuthEnrollmentScreenTest : public EnrollmentScreenTest {
 public:
  MultiAuthEnrollmentScreenTest() = default;

  MultiAuthEnrollmentScreenTest(const MultiAuthEnrollmentScreenTest&) = delete;
  MultiAuthEnrollmentScreenTest& operator=(
      const MultiAuthEnrollmentScreenTest&) = delete;

  ~MultiAuthEnrollmentScreenTest() override = default;

 private:
  // EnrollmentScreenTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EnrollmentScreenTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnterpriseEnableZeroTouchEnrollment);
    // Kiosk mode will force OAuth enrollment.
    base::FilePath test_data_dir;
    ASSERT_TRUE(chromeos::test_utils::GetTestDataPath(
        "app_mode", "kiosk_manifest", &test_data_dir));
    command_line->AppendSwitchPath(
        switches::kAppOemManifestFile,
        test_data_dir.AppendASCII("kiosk_manifest.json"));
  }
};

IN_PROC_BROWSER_TEST_F(MultiAuthEnrollmentScreenTest, TestCancel) {
  ASSERT_TRUE(enrollment_screen()->AdvanceToNextAuth());
  enrollment_ui_.SetExitHandler();
  enrollment_screen()->OnCancel();
  EnrollmentScreen::Result screen_result = enrollment_ui_.WaitForScreenExit();
  EXPECT_EQ(EnrollmentScreen::Result::BACK_TO_AUTO_ENROLLMENT_CHECK,
            screen_result);
}

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
