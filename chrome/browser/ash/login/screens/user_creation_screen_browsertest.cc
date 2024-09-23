// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ash/login/screens/user_creation_screen.h"

#include "ash/constants/ash_features.h"
#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/shell.h"
#include "chrome/browser/ash/login/enrollment/enrollment_screen_view.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/network_portal_detector_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/consumer_update_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_info_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/offline_login_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/user_creation_screen_handler.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "content/public/test/browser_test.h"
#include "ui/display/test/display_manager_test_api.h"

namespace ash {
namespace {

constexpr char kUserCreationId[] = "user-creation";

const test::UIPath kUserCreationDialog = {kUserCreationId,
                                          "userCreationDialog"};
const test::UIPath kUserCreationChildSetupDialog = {kUserCreationId,
                                                    "childSetupDialog"};
const test::UIPath kUserCreationEnrollTriageDialog = {kUserCreationId,
                                                      "enrollTriageDialog"};
const test::UIPath kUserCreationLearnMoreDialog = {kUserCreationId,
                                                   "learnMoreDialog"};
const test::UIPath kSelfButton = {kUserCreationId, "selfButton"};
const test::UIPath kChildButton = {kUserCreationId, "childButton"};
const test::UIPath kEnrollButton = {kUserCreationId, "enrollButton"};
const test::UIPath kBackButton = {kUserCreationId, "backButton"};
const test::UIPath kNextButton = {kUserCreationId, "nextButton"};
const test::UIPath kChildAccountButton = {kUserCreationId,
                                          "childAccountButton"};
const test::UIPath kSchoolAccountButton = {kUserCreationId,
                                           "schoolAccountButton"};
const test::UIPath kChildSetupBackButton = {kUserCreationId,
                                            "childSetupBackButton"};
const test::UIPath kChildSetupNextButton = {kUserCreationId,
                                            "childSetupNextButton"};
const test::UIPath kTriageEnrollButton = {kUserCreationId,
                                          "triageEnrollButton"};
const test::UIPath kTriageNotEnrollButton = {kUserCreationId,
                                             "triageNotEnrollButton"};
const test::UIPath kEnrollTriageNextButton = {kUserCreationId,
                                              "enrollTriageNextButton"};
const test::UIPath kEnrollTriageBackButton = {kUserCreationId,
                                              "enrollTriageBackButton"};
const test::UIPath kLearnMoreLink = {kUserCreationId, "learnMoreLink"};

class UserCreationScreenTest
    : public OobeBaseTest,
      public UserCreationScreen::UserCreationScreenExitTestDelegate {
 public:
  UserCreationScreenTest() : OobeBaseTest() {
    feature_list_.InitWithFeatures({}, {ash::features::kOobeSoftwareUpdate});
  }
  ~UserCreationScreenTest() override = default;

  void SetUpOnMainThread() override {
    UserCreationScreen::SetUserCreationScreenExitTestDelegate(this);
    OobeBaseTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    OobeBaseTest::TearDownOnMainThread();
    UserCreationScreen::SetUserCreationScreenExitTestDelegate(nullptr);
  }

  void SelectUserTypeOnUserCreationScreen(test::UIPath element_id) {
    OobeScreenWaiter(UserCreationView::kScreenId).Wait();
    ASSERT_TRUE(LoginScreenTestApi::IsEnterpriseEnrollmentButtonShown());
    test::OobeJS().ExpectVisiblePath(kUserCreationDialog);
    test::OobeJS().ExpectHasAttribute("checked", kSelfButton);
    test::OobeJS().ClickOnPath(element_id);
    test::OobeJS().TapOnPath(kNextButton);
  }

  void WaitForScreenExit() {
    if (screen_result_.has_value())
      return;
    base::RunLoop run_loop;
    screen_exit_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  std::optional<UserCreationScreen::Result> screen_result_;

 protected:
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_UNOWNED};

  NetworkPortalDetectorMixin network_portal_detector_{&mixin_host_};

 private:
  // UserCreationScreen::UserCreationScreenExitTestDelegate
  void OnUserCreationScreenExit(UserCreationScreen::Result result,
                                const UserCreationScreen::ScreenExitCallback&
                                    original_callback) override {
    ASSERT_FALSE(screen_exited_);
    screen_exited_ = true;
    screen_result_ = result;
    original_callback.Run(result);
    if (screen_exit_callback_)
      std::move(screen_exit_callback_).Run();
  }

  bool screen_exited_ = false;
  base::RepeatingClosure screen_exit_callback_;

  base::test::ScopedFeatureList feature_list_;
  FakeGaiaMixin fake_gaia_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(UserCreationScreenTest, UserCreationNoExceptions) {
  display::test::DisplayManagerTestApi display_manager(
      ash::ShellTestApi().display_manager());

  // Test minimum screen size and global ResizeObservers
  display_manager.UpdateDisplay(std::string("900x600"));
  // Test portrait transition
  display_manager.UpdateDisplay(std::string("600x900"));
  ScreenOrientationControllerTestApi(
      Shell::Get()->screen_orientation_controller())
      .UpdateNaturalOrientation();

  OobeBaseTest::CheckJsExceptionErrors(0);
}

// Verify flow for setting up the device for self.
IN_PROC_BROWSER_TEST_F(UserCreationScreenTest, SignInForSelf) {
  SelectUserTypeOnUserCreationScreen(kSelfButton);
  WaitForScreenExit();
  EXPECT_FALSE(LoginDisplayHost::default_host()
                   ->GetWizardContextForTesting()
                   ->sign_in_as_child);
  EXPECT_EQ(screen_result_.value(), UserCreationScreen::Result::SIGNIN);
  if (features::IsOobeGaiaInfoScreenEnabled()) {
    OobeScreenWaiter(GaiaInfoScreenView::kScreenId).Wait();
  } else {
    OobeScreenWaiter(GaiaView::kScreenId).Wait();
  }
}

IN_PROC_BROWSER_TEST_F(UserCreationScreenTest, SelectChild) {
  SelectUserTypeOnUserCreationScreen(kChildButton);
  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), UserCreationScreen::Result::ADD_CHILD);
  OobeScreenWaiter(AddChildScreenView::kScreenId).Wait();
}

// Verify enterprise enrollment button is available during the oobe flow (when
// no existing users).
IN_PROC_BROWSER_TEST_F(UserCreationScreenTest, EnterpriseEnroll) {
  ASSERT_TRUE(LoginScreenTestApi::ClickEnterpriseEnrollmentButton());
  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(),
            UserCreationScreen::Result::ENTERPRISE_ENROLL_SHORTCUT);
  OobeScreenWaiter(EnrollmentScreenView::kScreenId).Wait();
}

IN_PROC_BROWSER_TEST_F(UserCreationScreenTest, NetworkOffline) {
  network_portal_detector_.SimulateDefaultNetworkState(
      NetworkPortalDetectorMixin::NetworkStatus::kOffline);

  OobeScreenWaiter(ErrorScreenView::kScreenId).Wait();
  test::OobeJS().ExpectVisiblePath(
      {"error-message", "error-guest-signin-link"});

  network_portal_detector_.SimulateDefaultNetworkState(
      NetworkPortalDetectorMixin::NetworkStatus::kOnline);
  OobeScreenWaiter(UserCreationView::kScreenId).Wait();
}

class UserCreationScreenLoginTest : public UserCreationScreenTest {
 public:
  UserCreationScreenLoginTest() : UserCreationScreenTest() {
    login_manager_mixin_.AppendRegularUsers(1);
    device_state_.SetState(
        DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED);
  }

 private:
  LoginManagerMixin login_manager_mixin_{&mixin_host_};
};

// Verify back button is available during the add person flow (when there are
// existing users) and clicking it closes the oobe dialog. Enterprise
// enrollment button is hidden when there are existing users.
IN_PROC_BROWSER_TEST_F(UserCreationScreenLoginTest, Cancel) {
  EXPECT_TRUE(LoginScreenTestApi::ClickAddUserButton());
  EXPECT_TRUE(LoginScreenTestApi::IsOobeDialogVisible());
  OobeScreenWaiter(UserCreationView::kScreenId).Wait();
  ASSERT_FALSE(LoginScreenTestApi::IsEnterpriseEnrollmentButtonShown());

  test::OobeJS().ExpectVisiblePath(kUserCreationDialog);
  test::OobeJS().ExpectVisiblePath(kBackButton);
  test::OobeJS().TapOnPath(kBackButton);

  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), UserCreationScreen::Result::CANCEL);
  EXPECT_FALSE(LoginScreenTestApi::IsOobeDialogVisible());
}

class UserCreationScreenEnrolledTest : public UserCreationScreenTest {
 public:
  UserCreationScreenEnrolledTest() : UserCreationScreenTest() {
    login_manager_mixin_.AppendRegularUsers(1);
    device_state_.SetState(
        DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED);
  }

 private:
  LoginManagerMixin login_manager_mixin_{&mixin_host_};
};

// Verify user creation screen is skipped when clicking add person button
// on managed device.
IN_PROC_BROWSER_TEST_F(UserCreationScreenEnrolledTest,
                       ShouldSkipUserCreationScreen) {
  EXPECT_TRUE(LoginScreenTestApi::ClickAddUserButton());
  EXPECT_TRUE(LoginScreenTestApi::IsOobeDialogVisible());
  OobeScreenWaiter(GaiaView::kScreenId).Wait();
  test::OobeJS().ClickOnPath(
      {"gaia-signin", "signin-frame-dialog", "signin-back-button"});
  EXPECT_FALSE(LoginScreenTestApi::IsOobeDialogVisible());
}

class UserCreationScreenSoftwareUpdateTest : public UserCreationScreenTest {
 public:
  UserCreationScreenSoftwareUpdateTest() : UserCreationScreenTest() {
    feature_list_.InitWithFeatures({ash::features::kOobeSoftwareUpdate}, {});
  }

  // Redefine the SelectUserTypeOnUserCreationScreen and check no default
  // UserMethod and Next button is disabled
  void SelectUserTypeOnUserCreationScreen(test::UIPath element_id) {
    OobeScreenWaiter(UserCreationView::kScreenId).Wait();
    test::OobeJS()
        .CreateWaiter(
            test::GetAttributeExpression("isOobeLoaded_", {kUserCreationId}))
        ->Wait();
    ASSERT_TRUE(LoginScreenTestApi::IsEnterpriseEnrollmentButtonShown());
    test::OobeJS().ExpectVisiblePath(kUserCreationDialog);
    test::OobeJS().ExpectHasNoAttribute("checked", kSelfButton);
    test::OobeJS().ExpectDisabledPath(kNextButton);
    test::OobeJS().TapOnPath(element_id);
    test::OobeJS().TapOnPath(kNextButton);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(UserCreationScreenSoftwareUpdateTest, SignInForSelf) {
  SelectUserTypeOnUserCreationScreen(kSelfButton);
  WaitForScreenExit();
  EXPECT_FALSE(LoginDisplayHost::default_host()
                   ->GetWizardContextForTesting()
                   ->sign_in_as_child);
  EXPECT_EQ(screen_result_.value(), UserCreationScreen::Result::SIGNIN);
}

// Verify that google account in the child setup step in user creation
// screen display add-child screen when software update enabled.
IN_PROC_BROWSER_TEST_F(UserCreationScreenSoftwareUpdateTest,
                       SetupChildGoogleAccount) {
  SelectUserTypeOnUserCreationScreen(kChildButton);
  test::OobeJS().ExpectVisiblePath(kUserCreationChildSetupDialog);
  test::OobeJS().TapOnPath(kChildAccountButton);
  test::OobeJS().TapOnPath(kChildSetupNextButton);
  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), UserCreationScreen::Result::ADD_CHILD);
}

// Verify that school account in the child setup step in user creation
// screen display gaia signin when software update enabled.
IN_PROC_BROWSER_TEST_F(UserCreationScreenSoftwareUpdateTest,
                       SetupChildSchoolAccount) {
  SelectUserTypeOnUserCreationScreen(kChildButton);
  test::OobeJS().ExpectVisiblePath(kUserCreationChildSetupDialog);
  test::OobeJS().TapOnPath(kSchoolAccountButton);
  test::OobeJS().TapOnPath(kChildSetupNextButton);
  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), UserCreationScreen::Result::SIGNIN_SCHOOL);
}

// Verify that don't-enroll-the-device in the enorll triage step in user
// creation screen display gaia signin when software update enabled.
IN_PROC_BROWSER_TEST_F(UserCreationScreenSoftwareUpdateTest, NotEnrollDevice) {
  SelectUserTypeOnUserCreationScreen(kEnrollButton);
  test::OobeJS().ExpectVisiblePath(kUserCreationEnrollTriageDialog);
  test::OobeJS().TapOnPath(kTriageNotEnrollButton);
  test::OobeJS().TapOnPath(kEnrollTriageNextButton);
  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), UserCreationScreen::Result::SIGNIN_TRIAGE);
}

// Verify that  gaia signin back button return to enroll triage step after
// going through the enorll triage -> don't-enroll-the-device in user
// creation screen.
IN_PROC_BROWSER_TEST_F(UserCreationScreenSoftwareUpdateTest, BackFromGaia) {
  // TODO(b/325017147) Check why ConsumerUpdateScreen
  // is shown and updating in linux-chromemos
  LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build =
      false;

  SelectUserTypeOnUserCreationScreen(kEnrollButton);
  test::OobeJS().ExpectVisiblePath(kUserCreationEnrollTriageDialog);
  test::OobeJS().TapOnPath(kTriageNotEnrollButton);
  test::OobeJS().TapOnPath(kEnrollTriageNextButton);
  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), UserCreationScreen::Result::SIGNIN_TRIAGE);

  OobeScreenWaiter(GaiaView::kScreenId).Wait();
  test::OobeJS()
      .CreateVisibilityWaiter(
          true, {"gaia-signin", "signin-frame-dialog", "signin-back-button"})
      ->Wait();

  test::OobeJS().ClickOnPath(
      {"gaia-signin", "signin-frame-dialog", "signin-back-button"});

  OobeScreenWaiter(UserCreationView::kScreenId).Wait();
  test::OobeJS().ExpectVisiblePath(kUserCreationEnrollTriageDialog);
}

// Verify that enroll-device in the enorll triage step in user creation
// screen display device enrolllment when software update enabled.
IN_PROC_BROWSER_TEST_F(UserCreationScreenSoftwareUpdateTest, EnrollDevice) {
  SelectUserTypeOnUserCreationScreen(kEnrollButton);
  test::OobeJS().ExpectVisiblePath(kUserCreationEnrollTriageDialog);
  test::OobeJS().TapOnPath(kTriageEnrollButton);
  test::OobeJS().TapOnPath(kEnrollTriageNextButton);
  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(),
            UserCreationScreen::Result::ENTERPRISE_ENROLL_TRIAGE);

  OobeScreenWaiter(EnrollmentScreenView::kScreenId).Wait();

  LoginDisplayHost::default_host()->HandleAccelerator(
      LoginAcceleratorAction::kCancelScreenAction);

  OobeScreenWaiter(UserCreationView::kScreenId).Wait();
  test::OobeJS().ExpectVisiblePath(kUserCreationDialog);
}

// Verify that back button display create step in the child setup step
// in user creation screen when software update enabled.
IN_PROC_BROWSER_TEST_F(UserCreationScreenSoftwareUpdateTest,
                       BackButtonChildSetup) {
  SelectUserTypeOnUserCreationScreen(kChildButton);
  test::OobeJS().ExpectVisiblePath(kUserCreationChildSetupDialog);
  test::OobeJS().TapOnPath(kChildSetupBackButton);
  test::OobeJS().ExpectVisiblePath(kUserCreationDialog);
}

// Verify that back button display create step in the enroll triage step
// in user creation screen when software update enabled.
IN_PROC_BROWSER_TEST_F(UserCreationScreenSoftwareUpdateTest,
                       BackButtonEnrolLTriageSetup) {
  SelectUserTypeOnUserCreationScreen(kEnrollButton);
  test::OobeJS().ExpectVisiblePath(kUserCreationEnrollTriageDialog);
  test::OobeJS().TapOnPath(kEnrollTriageBackButton);
  test::OobeJS().ExpectVisiblePath(kUserCreationDialog);
}

// Verify that learn more display the modal dialogue when software update
// enabled.
IN_PROC_BROWSER_TEST_F(UserCreationScreenSoftwareUpdateTest, LearnMoreClicked) {
  SelectUserTypeOnUserCreationScreen(kEnrollButton);
  test::OobeJS().ExpectVisiblePath(kUserCreationEnrollTriageDialog);
  test::OobeJS().ExpectAttributeEQ("open", kUserCreationLearnMoreDialog, false);
  test::OobeJS().TapOnPath(kLearnMoreLink);
  test::OobeJS().ExpectAttributeEQ("open", kUserCreationLearnMoreDialog, true);
}

class UserCreationScreenEnrolledSoftwareUpdateTest
    : public UserCreationScreenSoftwareUpdateTest {
 public:
  UserCreationScreenEnrolledSoftwareUpdateTest()
      : UserCreationScreenSoftwareUpdateTest() {
    login_manager_mixin_.AppendRegularUsers(1);
    device_state_.SetState(
        DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED);
  }

 private:
  LoginManagerMixin login_manager_mixin_{&mixin_host_};
};

// Verify user creation screen is skipped during the add person flow on
// managed device with software update enabled.
IN_PROC_BROWSER_TEST_F(UserCreationScreenEnrolledSoftwareUpdateTest,
                       ShouldSkipUserCreationScreen) {
  EXPECT_TRUE(LoginScreenTestApi::ClickAddUserButton());
  EXPECT_TRUE(LoginScreenTestApi::IsOobeDialogVisible());
  OobeScreenWaiter(GaiaView::kScreenId).Wait();
  test::OobeJS().ClickOnPath(
      {"gaia-signin", "signin-frame-dialog", "signin-back-button"});
  EXPECT_FALSE(LoginScreenTestApi::IsOobeDialogVisible());
}

class UserCreationScreenLoginSoftwareUpdateTest
    : public UserCreationScreenSoftwareUpdateTest {
 public:
  UserCreationScreenLoginSoftwareUpdateTest()
      : UserCreationScreenSoftwareUpdateTest() {
    login_manager_mixin_.AppendRegularUsers(1);
    device_state_.SetState(
        DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED);
  }

 private:
  LoginManagerMixin login_manager_mixin_{&mixin_host_};
};

// Verify back button is available during the add person flow (when there are
// existing users) and clicking it closes the oobe dialog. Enterprise
// enrollment button is hidden when there are existing users with software
// update enabled.
IN_PROC_BROWSER_TEST_F(UserCreationScreenLoginSoftwareUpdateTest, Cancel) {
  EXPECT_TRUE(LoginScreenTestApi::ClickAddUserButton());
  EXPECT_TRUE(LoginScreenTestApi::IsOobeDialogVisible());
  OobeScreenWaiter(UserCreationView::kScreenId).Wait();
  ASSERT_FALSE(LoginScreenTestApi::IsEnterpriseEnrollmentButtonShown());

  test::OobeJS().ExpectVisiblePath(kUserCreationDialog);
  test::OobeJS().TapOnPath(kBackButton);

  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), UserCreationScreen::Result::CANCEL);
  EXPECT_FALSE(LoginScreenTestApi::IsOobeDialogVisible());
}

// Verify that during the add person flow when select "For work"
// skip the enroll triage step and gaia screen is shown with
// software update enabled.
IN_PROC_BROWSER_TEST_F(UserCreationScreenLoginSoftwareUpdateTest,
                       ForWorkSelected) {
  EXPECT_TRUE(LoginScreenTestApi::ClickAddUserButton());
  EXPECT_TRUE(LoginScreenTestApi::IsOobeDialogVisible());
  OobeScreenWaiter(UserCreationView::kScreenId).Wait();
  test::OobeJS().ExpectVisiblePath(kUserCreationDialog);
  test::OobeJS().TapOnPath(kEnrollButton);
  test::OobeJS().TapOnPath(kNextButton);
  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), UserCreationScreen::Result::SIGNIN);
}
}  // namespace
}  // namespace ash
