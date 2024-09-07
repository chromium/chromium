// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/enrollment/enrollment_screen_view.h"
#include "chrome/browser/ash/login/test/enrollment_ui_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/test_condition_waiter.h"
#include "chrome/browser/ash/policy/test_support/embedded_policy_test_server_mixin.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "components/policy/test_support/embedded_policy_test_server.h"
#include "components/policy/test_support/policy_storage.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

constexpr test::UIPath kEnrollmentNudgeDialog = {"gaia-signin",
                                                 "enrollmentNudge"};
constexpr test::UIPath kUseAnotherAccountButton = {"gaia-signin",
                                                   "useAnotherAccount"};
constexpr test::UIPath kEnterpriseEnrollmentButton = {"gaia-signin",
                                                      "enterpriseEnrollment"};

constexpr char kSigninWebview[] = "$('gaia-signin').getSigninFrame()";

constexpr test::UIPath kSigninBackButton = {
    "gaia-signin", "signin-frame-dialog", "signin-back-button"};

constexpr test::UIPath kSigninNextButton = {
    "gaia-signin", "signin-frame-dialog", "primary-action-button"};

}  // namespace

class EnrollmentNudgeTest : public OobeBaseTest {
 public:
  // This enum is tied directly to the `EnrollmentNudgeUserAction` UMA enum
  // defined in //tools/metrics/histograms/enums.xml and to the
  // `EnrollmentNudgeUserAction` enum defined in
  // // chrome/browser/resources/chromeos/login/screens/common/gaia_signin.js.
  // Do not change one without changing the others.
  enum class UserAction {
    kEnterpriseEnrollmentButtonClicked = 0,
    kUseAnotherAccountButtonClicked = 1,
    kMaxValue = kUseAnotherAccountButtonClicked,
  };

  EnrollmentNudgeTest() = default;
  ~EnrollmentNudgeTest() override = default;

  void SetUpOnMainThread() override {
    policy_server_.server()->policy_storage()->add_managed_user(
        FakeGaiaMixin::kEnterpriseUser1);
    policy_server_.SetEnrollmentNudgePolicy(true);
    OobeBaseTest::SetUpOnMainThread();
  }

  void WaitForEnrollmentNudgeDialogToOpen() {
    test::OobeJS()
        .CreateWaiter(test::GetOobeElementPath(kEnrollmentNudgeDialog) +
                      ".open")
        ->Wait();
  }

  void ExpectGaiaIdentifierPage() {
    test::OobeJS().ExpectTrue(
        base::StrCat({kSigninWebview, ".src.indexOf('#identifier') != -1"}));
  }

  void ExpectGaiaPasswordPage() {
    test::OobeJS().ExpectTrue(base::StrCat(
        {kSigninWebview, ".src.indexOf('#challengepassword') != -1"}));
  }

  void ExpectNavigationButtonsHidden() {
    test::OobeJS().ExpectHiddenPath(kSigninBackButton);
    // TODO(b/302676357): we check if the button is disabled because the
    // "hidden" attribute for this button is not always set as expected
    test::OobeJS().ExpectDisabledPath(kSigninNextButton);
  }

  void ExpectNavigationButtonsVisible() {
    test::OobeJS().ExpectVisiblePath(kSigninBackButton);
    test::OobeJS().ExpectVisiblePath(kSigninNextButton);
  }

  void CheckUserActionHistogram(const UserAction& expected_user_action) {
    EXPECT_THAT(
        histogram_tester.GetAllSamples("Enterprise.EnrollmentNudge.UserAction"),
        testing::ElementsAre(
            base::Bucket(static_cast<int>(expected_user_action), 1)));
  }

  FakeGaiaMixin fake_gaia{&mixin_host_};
  test::EnrollmentUIMixin enrollment_ui{&mixin_host_};
  base::HistogramTester histogram_tester;

 private:
  base::test::ScopedFeatureList feature_list_;
  EmbeddedPolicyTestServerMixin policy_server_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(EnrollmentNudgeTest, SwitchToEnrollment) {
  // Proceed to Gaia sign in page during OOBE.
  WaitForGaiaPageLoadAndPropertyUpdate();
  ExpectGaiaIdentifierPage();
  ExpectNavigationButtonsVisible();

  // Enter the `FakeGaiaMixin::kEnterpriseUser1` email. It should trigger
  // enrollment nudge pop-up.
  SigninFrameJS().TypeIntoPath(FakeGaiaMixin::kEnterpriseUser1,
                               FakeGaiaMixin::kEmailPath);
  test::OobeJS().ClickOnPath(kSigninNextButton);
  WaitForEnrollmentNudgeDialogToOpen();
  test::OobeJS().CreateVisibilityWaiter(false, kSigninBackButton)->Wait();
  ExpectNavigationButtonsHidden();

  // Clicking on `kEnterpriseEnrollmentButton` should lead to enrollment screen.
  test::OobeJS().ClickOnPath(kEnterpriseEnrollmentButton);
  OobeScreenWaiter(EnrollmentScreenView::kScreenId).Wait();
  enrollment_ui.WaitForStep(test::ui::kEnrollmentStepSignin);

  // Check that email field on the enrollment screen is prefilled.
  EXPECT_EQ(fake_gaia.fake_gaia()->prefilled_email(),
            FakeGaiaMixin::kEnterpriseUser1);
  CheckUserActionHistogram(UserAction::kEnterpriseEnrollmentButtonClicked);
}

IN_PROC_BROWSER_TEST_F(EnrollmentNudgeTest, UseAnotherAccountButton) {
  // Proceed to Gaia sign in page during OOBE.
  WaitForGaiaPageLoadAndPropertyUpdate();
  ExpectGaiaIdentifierPage();
  ExpectNavigationButtonsVisible();

  // Enter the `FakeGaiaMixin::kEnterpriseUser1` email. It should trigger
  // enrollment nudge pop-up.
  SigninFrameJS().TypeIntoPath(FakeGaiaMixin::kEnterpriseUser1,
                               FakeGaiaMixin::kEmailPath);
  test::OobeJS().ClickOnPath(kSigninNextButton);
  WaitForEnrollmentNudgeDialogToOpen();
  test::OobeJS().CreateVisibilityWaiter(false, kSigninBackButton)->Wait();
  ExpectNavigationButtonsHidden();

  // Clicking on `kUseAnotherAccountButton` should result in Gaia page being
  // reload.
  test::OobeJS().ClickOnPath(kUseAnotherAccountButton);
  WaitForGaiaPageReload();
  ExpectNavigationButtonsVisible();
  CheckUserActionHistogram(UserAction::kUseAnotherAccountButtonClicked);
}

IN_PROC_BROWSER_TEST_F(EnrollmentNudgeTest, NoNudgeForKnownConsumerDomain) {
  // Proceed to Gaia sign in page during OOBE.
  WaitForGaiaPageLoadAndPropertyUpdate();
  ExpectGaiaIdentifierPage();
  ExpectNavigationButtonsVisible();

  // Make sure that `FakeGaiaMixin::kFakeUserEmail` belongs to a known consumer
  // domain.
  const std::string email_domain =
      enterprise_util::GetDomainFromEmail(FakeGaiaMixin::kFakeUserEmail);
  EXPECT_TRUE(enterprise_util::IsKnownConsumerDomain(email_domain));

  // Using an email belonging to a known consumer domain should lead to the Gaia
  // password page without enrollment nudging.
  SigninFrameJS().TypeIntoPath(FakeGaiaMixin::kFakeUserEmail,
                               FakeGaiaMixin::kEmailPath);
  test::OobeJS().ClickOnPath(kSigninNextButton);
  WaitForGaiaPageBackButtonUpdate();
  ExpectGaiaPasswordPage();
  ExpectNavigationButtonsVisible();
  EXPECT_TRUE(
      histogram_tester.GetAllSamples("Enterprise.EnrollmentNudge.UserAction")
          .empty());
}

}  // namespace ash
