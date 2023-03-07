// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/enrollment/enrollment_screen_view.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/test_condition_waiter.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

constexpr test::UIPath kEnrollmentNudgeDialog = {"gaia-signin",
                                                 "enrollmentNudge"};
constexpr test::UIPath kUseAnotherAccountButton = {"gaia-signin",
                                                   "useAnotherAccount"};
constexpr test::UIPath kEnterpriseEnrollmentButton = {"gaia-signin",
                                                      "enterpriseEnrollment"};

constexpr char kSigninWebview[] = "$('gaia-signin').getSigninFrame_()";
constexpr test::UIPath kSigninNextButton = {
    "gaia-signin", "signin-frame-dialog", "primary-action-button"};

}  // namespace

class EnrollmentNudgeTest : public OobeBaseTest {
 public:
  EnrollmentNudgeTest() {
    // TODO(b/271104781): replace with policy-based setup after we land DM
    // server API changes. For now `kEnrollmentNudgingForTesting` feature is
    // used to enable enrollment nudging for all managed users.
    feature_list_.InitAndEnableFeature(
        ash::features::kEnrollmentNudgingForTesting);
  }
  ~EnrollmentNudgeTest() override = default;

  void WaitForEnrollmentNudgeDialogToOpen() {
    test::OobeJS()
        .CreateWaiter(test::GetOobeElementPath({kEnrollmentNudgeDialog}) +
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

 private:
  base::test::ScopedFeatureList feature_list_;
  FakeGaiaMixin fake_gaia_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(EnrollmentNudgeTest, SwitchToEnrollment) {
  // Proceed to Gaia sign in page during OOBE.
  WaitForGaiaPageLoadAndPropertyUpdate();
  ExpectGaiaIdentifierPage();

  // Enter the `FakeGaiaMixin::kEnterpriseUser1` email. It should trigger
  // enrollment nudge pop-up.
  SigninFrameJS().TypeIntoPath(FakeGaiaMixin::kEnterpriseUser1,
                               FakeGaiaMixin::kEmailPath);
  test::OobeJS().ClickOnPath(kSigninNextButton);
  WaitForEnrollmentNudgeDialogToOpen();

  // Clicking on `kEnterpriseEnrollmentButton` should lead to enrollment screen.
  test::OobeJS().ClickOnPath(kEnterpriseEnrollmentButton);
  OobeScreenWaiter(EnrollmentScreenView::kScreenId).Wait();
}

IN_PROC_BROWSER_TEST_F(EnrollmentNudgeTest, UseAnotherAccountButton) {
  // Proceed to Gaia sign in page during OOBE.
  WaitForGaiaPageLoadAndPropertyUpdate();
  ExpectGaiaIdentifierPage();

  // Enter the `FakeGaiaMixin::kEnterpriseUser1` email. It should trigger
  // enrollment nudge pop-up.
  SigninFrameJS().TypeIntoPath(FakeGaiaMixin::kEnterpriseUser1,
                               FakeGaiaMixin::kEmailPath);
  test::OobeJS().ClickOnPath(kSigninNextButton);
  WaitForEnrollmentNudgeDialogToOpen();

  // Clicking on `kUseAnotherAccountButton` should result in Gaia page being
  // reload.
  test::OobeJS().ClickOnPath(kUseAnotherAccountButton);
  WaitForGaiaPageReload();
}

IN_PROC_BROWSER_TEST_F(EnrollmentNudgeTest, NoNudgeForKnownConsumerDomain) {
  // Proceed to Gaia sign in page during OOBE.
  WaitForGaiaPageLoadAndPropertyUpdate();
  ExpectGaiaIdentifierPage();

  // Make sure that `FakeGaiaMixin::kFakeUserEmail` belongs to a known consumer
  // domain.
  const std::string email_domain = chrome::enterprise_util::GetDomainFromEmail(
      FakeGaiaMixin::kFakeUserEmail);
  EXPECT_TRUE(chrome::enterprise_util::IsKnownConsumerDomain(email_domain));

  // Using an email belonging to a known consumer domain should lead to the Gaia
  // password page without enrollment nudging.
  SigninFrameJS().TypeIntoPath(FakeGaiaMixin::kFakeUserEmail,
                               FakeGaiaMixin::kEmailPath);
  test::OobeJS().ClickOnPath(kSigninNextButton);
  WaitForGaiaPageBackButtonUpdate();
  ExpectGaiaPasswordPage();
}

}  // namespace ash
