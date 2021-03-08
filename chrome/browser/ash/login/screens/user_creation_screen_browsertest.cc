// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ash/login/screens/user_creation_screen.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "chrome/browser/ash/login/enrollment/enrollment_screen_view.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/fake_gaia_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/network_portal_detector_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/chromeos/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/offline_login_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/user_creation_screen_handler.h"
#include "content/public/test/browser_test.h"

namespace chromeos {

namespace {

constexpr char kUserCreationId[] = "user-creation";

const test::UIPath kUserCreationDialog = {kUserCreationId,
                                          "userCreationDialog"};
const test::UIPath kChildSignInDialog = {kUserCreationId, "childSignInDialog"};
const test::UIPath kSelfButton = {kUserCreationId, "selfButton"};
const test::UIPath kChildButton = {kUserCreationId, "childButton"};
const test::UIPath kBackButton = {kUserCreationId, "backButton"};
const test::UIPath kNextButton = {kUserCreationId, "nextButton"};
const test::UIPath kChildCreateButton = {kUserCreationId, "childCreateButton"};
const test::UIPath kChildSignInButton = {kUserCreationId, "childSignInButton"};
const test::UIPath kChildBackButton = {kUserCreationId, "childBackButton"};
const test::UIPath kChildNextButton = {kUserCreationId, "childNextButton"};

}  // namespace

class UserCreationScreenTest : public OobeBaseTest {
 public:
  UserCreationScreenTest() {
    feature_list_.InitAndEnableFeature(
        chromeos::features::kChildSpecificSignin);
  }
  ~UserCreationScreenTest() override = default;

  void SetUpOnMainThread() override {
    UserCreationScreen* screen = static_cast<UserCreationScreen*>(
        WizardController::default_controller()->screen_manager()->GetScreen(
            UserCreationView::kScreenId));
    original_callback_ = screen->get_exit_callback_for_testing();
    screen->set_exit_callback_for_testing(base::BindRepeating(
        &UserCreationScreenTest::HandleScreenExit, base::Unretained(this)));
    OobeBaseTest::SetUpOnMainThread();
  }

  void SelectUserTypeOnUserCreationScreen(test::UIPath element_id) {
    ASSERT_TRUE(ash::LoginScreenTestApi::IsEnterpriseEnrollmentButtonShown());
    test::OobeJS().ExpectVisiblePath(kUserCreationDialog);
    test::OobeJS().ExpectHasAttribute("checked", kSelfButton);
    test::OobeJS().ClickOnPath(element_id);
    test::OobeJS().TapOnPath(kNextButton);
  }

  void SelectSetUpMethodOnChildScreen(test::UIPath element_id) {
    ASSERT_FALSE(ash::LoginScreenTestApi::IsEnterpriseEnrollmentButtonShown());
    test::OobeJS().ExpectHiddenPath(kUserCreationDialog);
    test::OobeJS().ExpectVisiblePath(kChildSignInDialog);
    test::OobeJS().ClickOnPath(element_id);
    test::OobeJS().TapOnPath(kChildNextButton);
  }

  void WaitForScreenExit() {
    if (screen_result_.has_value())
      return;
    base::RunLoop run_loop;
    screen_exit_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  base::Optional<UserCreationScreen::Result> screen_result_;

 protected:
  chromeos::DeviceStateMixin device_state_{
      &mixin_host_, chromeos::DeviceStateMixin::State::OOBE_COMPLETED_UNOWNED};

  NetworkPortalDetectorMixin network_portal_detector_{&mixin_host_};

 private:
  void HandleScreenExit(UserCreationScreen::Result result) {
    ASSERT_FALSE(screen_exited_);
    screen_exited_ = true;
    screen_result_ = result;
    original_callback_.Run(result);
    if (screen_exit_callback_)
      std::move(screen_exit_callback_).Run();
  }

  bool screen_exited_ = false;
  base::RepeatingClosure screen_exit_callback_;
  UserCreationScreen::ScreenExitCallback original_callback_;

  base::test::ScopedFeatureList feature_list_;
  FakeGaiaMixin fake_gaia_{&mixin_host_, embedded_test_server()};
};

// Verify flow for setting up the device for self.
IN_PROC_BROWSER_TEST_F(UserCreationScreenTest, SignInForSelf) {
  SelectUserTypeOnUserCreationScreen(kSelfButton);
  WaitForScreenExit();
  EXPECT_FALSE(WizardController::default_controller()
                   ->get_wizard_context_for_testing()
                   ->sign_in_as_child);
  EXPECT_EQ(screen_result_.value(), UserCreationScreen::Result::SIGNIN);
  OobeScreenWaiter(GaiaView::kScreenId).Wait();
}

// Verify flow for setting up the device for a child with a newly created gaia
// account.
IN_PROC_BROWSER_TEST_F(UserCreationScreenTest, CreateAccountForChild) {
  SelectUserTypeOnUserCreationScreen(kChildButton);
  SelectSetUpMethodOnChildScreen(kChildCreateButton);
  WaitForScreenExit();
  EXPECT_TRUE(WizardController::default_controller()
                  ->get_wizard_context_for_testing()
                  ->sign_in_as_child);
  EXPECT_TRUE(WizardController::default_controller()
                  ->get_wizard_context_for_testing()
                  ->is_child_gaia_account_new);
  EXPECT_EQ(screen_result_.value(),
            UserCreationScreen::Result::CHILD_ACCOUNT_CREATE);
  OobeScreenWaiter(GaiaView::kScreenId).Wait();
}

// Verify flow for setting up the device for a child with an existing gaia
// account.
IN_PROC_BROWSER_TEST_F(UserCreationScreenTest, SignInForChild) {
  SelectUserTypeOnUserCreationScreen(kChildButton);
  SelectSetUpMethodOnChildScreen(kChildSignInButton);
  WaitForScreenExit();
  EXPECT_TRUE(WizardController::default_controller()
                  ->get_wizard_context_for_testing()
                  ->sign_in_as_child);
  EXPECT_FALSE(WizardController::default_controller()
                   ->get_wizard_context_for_testing()
                   ->is_child_gaia_account_new);
  EXPECT_EQ(screen_result_.value(), UserCreationScreen::Result::CHILD_SIGNIN);
  OobeScreenWaiter(GaiaView::kScreenId).Wait();
}

// Verify back button is hidden during the oobe flow (when no existing users).
IN_PROC_BROWSER_TEST_F(UserCreationScreenTest, Cancel) {
  SelectUserTypeOnUserCreationScreen(kChildButton);

  test::OobeJS().ExpectHiddenPath(kUserCreationDialog);
  test::OobeJS().ExpectVisiblePath(kChildSignInDialog);
  test::OobeJS().TapOnPath(kChildBackButton);

  test::OobeJS().ExpectVisiblePath(kUserCreationDialog);
  test::OobeJS().ExpectHiddenPath(kChildSignInDialog);
  test::OobeJS().ExpectHiddenPath(kBackButton);
}

// Verify enterprise enrollment button is available during the oobe flow (when
// no existing users).
IN_PROC_BROWSER_TEST_F(UserCreationScreenTest, EnterpriseEnroll) {
  ASSERT_TRUE(ash::LoginScreenTestApi::ClickEnterpriseEnrollmentButton());
  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(),
            UserCreationScreen::Result::ENTERPRISE_ENROLL);
  OobeScreenWaiter(EnrollmentScreenView::kScreenId).Wait();
}

IN_PROC_BROWSER_TEST_F(UserCreationScreenTest, NetworkOffline) {
  network_portal_detector_.SimulateDefaultNetworkState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_OFFLINE);

  OobeScreenWaiter(ErrorScreenView::kScreenId).Wait();
  test::OobeJS().ExpectVisiblePath(
      {"error-message", "error-guest-signin-link"});

  network_portal_detector_.SimulateDefaultNetworkState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE);
  OobeScreenWaiter(UserCreationView::kScreenId).Wait();
}

class UserCreationScreenLoginTest : public UserCreationScreenTest {
 public:
  UserCreationScreenLoginTest() : UserCreationScreenTest() {
    login_manager_mixin_.AppendRegularUsers(1);
    device_state_.SetState(
        chromeos::DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED);
  }

 private:
  LoginManagerMixin login_manager_mixin_{&mixin_host_};
};

// Verify back button is available during the add user flow (when there are
// existing users) and clicking it closes the oobe dialog. Enterprise
// enrollment button is hidden when there are existing users.
IN_PROC_BROWSER_TEST_F(UserCreationScreenLoginTest, Cancel) {
  EXPECT_TRUE(ash::LoginScreenTestApi::ClickAddUserButton());
  EXPECT_TRUE(ash::LoginScreenTestApi::IsOobeDialogVisible());
  OobeScreenWaiter(UserCreationView::kScreenId).Wait();
  ASSERT_FALSE(ash::LoginScreenTestApi::IsEnterpriseEnrollmentButtonShown());

  test::OobeJS().ExpectVisiblePath(kUserCreationDialog);
  test::OobeJS().ClickOnPath(kChildButton);
  test::OobeJS().TapOnPath(kNextButton);

  test::OobeJS().ExpectHiddenPath(kUserCreationDialog);
  test::OobeJS().ExpectVisiblePath(kChildSignInDialog);
  test::OobeJS().TapOnPath(kChildBackButton);

  test::OobeJS().ExpectVisiblePath(kUserCreationDialog);
  test::OobeJS().ExpectHiddenPath(kChildSignInDialog);
  test::OobeJS().ExpectVisiblePath(kBackButton);
  test::OobeJS().TapOnPath(kBackButton);

  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), UserCreationScreen::Result::CANCEL);
  EXPECT_FALSE(ash::LoginScreenTestApi::IsOobeDialogVisible());
}

class UserCreationScreenEnrolledTest : public UserCreationScreenTest {
 public:
  UserCreationScreenEnrolledTest() : UserCreationScreenTest() {
    login_manager_mixin_.AppendRegularUsers(1);
    device_state_.SetState(
        chromeos::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED);
  }

 private:
  LoginManagerMixin login_manager_mixin_{&mixin_host_};
};

// Verify user creation screen is skipped when clicking add user button on
// managed device.
IN_PROC_BROWSER_TEST_F(UserCreationScreenEnrolledTest,
                       ShouldSkipUserCreationScreen) {
  EXPECT_TRUE(ash::LoginScreenTestApi::ClickAddUserButton());
  EXPECT_TRUE(ash::LoginScreenTestApi::IsOobeDialogVisible());
  OobeScreenWaiter(GaiaView::kScreenId).Wait();
  test::OobeJS().ClickOnPath(
      {"gaia-signin", "signin-frame-dialog", "signin-back-button"});
  EXPECT_FALSE(ash::LoginScreenTestApi::IsOobeDialogVisible());
}

}  // namespace chromeos
