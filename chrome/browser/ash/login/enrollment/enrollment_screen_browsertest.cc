// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/optional.h"
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
#include "chromeos/test/chromeos_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::InvokeWithoutArgs;
using testing::Mock;

namespace chromeos {

class EnrollmentScreenTest : public OobeBaseTest {
 public:
  EnrollmentScreenTest() = default;
  ~EnrollmentScreenTest() override = default;

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

 private:
  DISALLOW_COPY_AND_ASSIGN(EnrollmentScreenTest);
};

IN_PROC_BROWSER_TEST_F(EnrollmentScreenTest, TestCancel) {
  enrollment_ui_.SetExitHandler();
  enrollment_screen()->OnCancel();
  EnrollmentScreen::Result screen_result = enrollment_ui_.WaitForScreenExit();
  EXPECT_EQ(EnrollmentScreen::Result::COMPLETED, screen_result);
}

IN_PROC_BROWSER_TEST_F(EnrollmentScreenTest, TestSuccess) {
  WizardController::SkipEnrollmentPromptsForTesting();
  enrollment_ui_.SetExitHandler();
  enrollment_screen()->OnDeviceAttributeUpdatePermission(false /* granted */);
  EnrollmentScreen::Result screen_result = enrollment_ui_.WaitForScreenExit();
  EXPECT_EQ(EnrollmentScreen::Result::COMPLETED, screen_result);

  EXPECT_TRUE(StartupUtils::IsDeviceRegistered());
}

class AttestationAuthEnrollmentScreenTest : public EnrollmentScreenTest {
 public:
  AttestationAuthEnrollmentScreenTest() = default;
  ~AttestationAuthEnrollmentScreenTest() override = default;

 private:
  // EnrollmentScreenTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EnrollmentScreenTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnterpriseEnableZeroTouchEnrollment);
  }

  DISALLOW_COPY_AND_ASSIGN(AttestationAuthEnrollmentScreenTest);
};

IN_PROC_BROWSER_TEST_F(AttestationAuthEnrollmentScreenTest, TestCancel) {
  ASSERT_FALSE(enrollment_screen()->AdvanceToNextAuth());
  enrollment_ui_.SetExitHandler();
  enrollment_screen()->OnCancel();
  EnrollmentScreen::Result screen_result = enrollment_ui_.WaitForScreenExit();
  EXPECT_EQ(EnrollmentScreen::Result::COMPLETED, screen_result);
}

IN_PROC_BROWSER_TEST_F(EnrollmentScreenTest, EnrollmentSpinner) {
  EnrollmentScreenView* view = enrollment_screen()->GetView();
  ASSERT_TRUE(view);

  // Run through the flow
  view->Show();
  OobeScreenWaiter(EnrollmentScreenView::kScreenId).Wait();
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSignin);

  view->ShowEnrollmentSpinnerScreen();
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepWorking);

  view->ShowEnrollmentSuccessScreen();
  enrollment_ui_.WaitForStep(test::ui::kEnrollmentStepSuccess);
}

class ForcedAttestationAuthEnrollmentScreenTest : public EnrollmentScreenTest {
 public:
  ForcedAttestationAuthEnrollmentScreenTest() = default;
  ~ForcedAttestationAuthEnrollmentScreenTest() override = default;

 private:
  // EnrollmentScreenTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EnrollmentScreenTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kEnterpriseEnableZeroTouchEnrollment, "forced");
  }

  DISALLOW_COPY_AND_ASSIGN(ForcedAttestationAuthEnrollmentScreenTest);
};

IN_PROC_BROWSER_TEST_F(ForcedAttestationAuthEnrollmentScreenTest, TestCancel) {
  ASSERT_FALSE(enrollment_screen()->AdvanceToNextAuth());
  enrollment_ui_.SetExitHandler();
  enrollment_screen()->OnCancel();
  EnrollmentScreen::Result screen_result = enrollment_ui_.WaitForScreenExit();
  EXPECT_EQ(EnrollmentScreen::Result::BACK, screen_result);
}

class MultiAuthEnrollmentScreenTest : public EnrollmentScreenTest {
 public:
  MultiAuthEnrollmentScreenTest() = default;
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

  DISALLOW_COPY_AND_ASSIGN(MultiAuthEnrollmentScreenTest);
};

IN_PROC_BROWSER_TEST_F(MultiAuthEnrollmentScreenTest, TestCancel) {
  ASSERT_TRUE(enrollment_screen()->AdvanceToNextAuth());
  enrollment_ui_.SetExitHandler();
  enrollment_screen()->OnCancel();
  EnrollmentScreen::Result screen_result = enrollment_ui_.WaitForScreenExit();
  EXPECT_EQ(EnrollmentScreen::Result::BACK, screen_result);
}

class ProvisionedEnrollmentScreenTest : public EnrollmentScreenTest {
 public:
  ProvisionedEnrollmentScreenTest() = default;
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

  DISALLOW_COPY_AND_ASSIGN(ProvisionedEnrollmentScreenTest);
};

IN_PROC_BROWSER_TEST_F(ProvisionedEnrollmentScreenTest, TestBackButton) {
  enrollment_ui_.SetExitHandler();
  enrollment_screen()->OnCancel();
  EnrollmentScreen::Result screen_result = enrollment_ui_.WaitForScreenExit();
  EXPECT_EQ(EnrollmentScreen::Result::BACK, screen_result);
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

}  // namespace chromeos
