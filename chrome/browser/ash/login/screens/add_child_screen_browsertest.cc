// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ash/login/screens/add_child_screen.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/enrollment/enrollment_screen_view.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/network_portal_detector_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/ash/login/add_child_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_info_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/offline_login_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/user_creation_screen_handler.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "content/public/test/browser_test.h"

namespace ash {
namespace {

constexpr char kAddChildId[] = "add-child";

const test::UIPath kAddChildDialog = {kAddChildId, "childSignInDialog"};
const test::UIPath kChildCreateButton = {kAddChildId, "childCreateButton"};
const test::UIPath kChildSignInButton = {kAddChildId, "childSignInButton"};
const test::UIPath kBackButton = {kAddChildId, "childBackButton"};
const test::UIPath kNextButton = {kAddChildId, "childNextButton"};

class AddChildScreenTest : public OobeBaseTest {
 public:
  AddChildScreenTest() = default;
  ~AddChildScreenTest() override = default;

  void SetUpOnMainThread() override {
    AddChildScreen* add_child_screen =
        WizardController::default_controller()->GetScreen<AddChildScreen>();

    original_callback_ = add_child_screen->get_exit_callback_for_testing();
    add_child_screen->set_exit_callback_for_testing(base::BindRepeating(
        &AddChildScreenTest::HandleScreenExit, base::Unretained(this)));

    OobeBaseTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override { OobeBaseTest::TearDownOnMainThread(); }

  void ShowAddChildScreen() {
    OobeScreenExitWaiter(GetFirstSigninScreen()).Wait();
    WizardController::default_controller()->AdvanceToScreen(
        AddChildScreenView::kScreenId);
  }

  void SelectSetUpMethodOnChildScreen(test::UIPath element_id) {
    ShowAddChildScreen();
    ASSERT_FALSE(LoginScreenTestApi::IsEnterpriseEnrollmentButtonShown());
    test::OobeJS().ExpectVisiblePath(kAddChildDialog);
    test::OobeJS().ClickOnPath(element_id);
    test::OobeJS().TapOnPath(kNextButton);
  }

  void WaitForScreenExit() {
    if (result_.has_value()) {
      return;
    }
    base::test::TestFuture<void> waiter;
    quit_closure_ = waiter.GetCallback();
    EXPECT_TRUE(waiter.Wait());
  }

  AddChildScreen::ScreenExitCallback original_callback_;
  absl::optional<AddChildScreen::Result> result_;

 protected:
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_UNOWNED};

  NetworkPortalDetectorMixin network_portal_detector_{&mixin_host_};

 private:
  void HandleScreenExit(AddChildScreen::Result result) {
    result_ = result;
    original_callback_.Run(result);
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

  base::OnceClosure quit_closure_;

  base::test::ScopedFeatureList feature_list_;
  FakeGaiaMixin fake_gaia_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(AddChildScreenTest, CreateChild) {
  SelectSetUpMethodOnChildScreen(kChildCreateButton);
  WaitForScreenExit();
  EXPECT_TRUE(LoginDisplayHost::default_host()
                  ->GetWizardContextForTesting()
                  ->sign_in_as_child);
  EXPECT_TRUE(LoginDisplayHost::default_host()
                  ->GetWizardContextForTesting()
                  ->is_child_gaia_account_new);
  EXPECT_EQ(result_.value(), AddChildScreen::Result::CHILD_ACCOUNT_CREATE);
  OobeScreenWaiter(GaiaView::kScreenId).Wait();
}

IN_PROC_BROWSER_TEST_F(AddChildScreenTest, SignInForChild) {
  SelectSetUpMethodOnChildScreen(kChildSignInButton);
  WaitForScreenExit();
  EXPECT_TRUE(LoginDisplayHost::default_host()
                  ->GetWizardContextForTesting()
                  ->sign_in_as_child);
  EXPECT_FALSE(LoginDisplayHost::default_host()
                   ->GetWizardContextForTesting()
                   ->is_child_gaia_account_new);
  EXPECT_EQ(result_.value(), AddChildScreen::Result::CHILD_SIGNIN);
  OobeScreenWaiter(GaiaView::kScreenId).Wait();
}

IN_PROC_BROWSER_TEST_F(AddChildScreenTest, BackButton) {
  ShowAddChildScreen();
  test::OobeJS().ExpectVisiblePath(kAddChildDialog);
  test::OobeJS().ClickOnPath(kBackButton);
  WaitForScreenExit();

  EXPECT_EQ(result_.value(), AddChildScreen::Result::BACK);
  OobeScreenWaiter(UserCreationView::kScreenId).Wait();
}

IN_PROC_BROWSER_TEST_F(AddChildScreenTest, NetworkOffline) {
  ShowAddChildScreen();
  OobeScreenWaiter(AddChildScreenView::kScreenId).Wait();
  network_portal_detector_.SimulateDefaultNetworkState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_OFFLINE);

  OobeScreenWaiter(ErrorScreenView::kScreenId).Wait();
  test::OobeJS().ExpectVisiblePath(
      {"error-message", "error-guest-signin-link"});

  network_portal_detector_.SimulateDefaultNetworkState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE);
  OobeScreenWaiter(AddChildScreenView::kScreenId).Wait();
}

}  // namespace
}  // namespace ash
