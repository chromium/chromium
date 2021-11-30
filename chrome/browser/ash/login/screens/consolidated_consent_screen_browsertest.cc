// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/consolidated_consent_screen.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/test/fake_eula_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/webview_content_extractor.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/ui/webui_login_view.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {
namespace {

using ::testing::_;

constexpr char kConsolidatedConsentId[] = "consolidated-consent";

// Loaded Dialog
const test::UIPath kLoadedDialog = {kConsolidatedConsentId, "loadedDialog"};
const test::UIPath kGoogleEulaLinkArcDisabled = {kConsolidatedConsentId,
                                                 "googleEulaLinkArcDisabled"};
const test::UIPath kCrosEulaLinkArcDisabled = {kConsolidatedConsentId,
                                               "crosEulaLinkArcDisabled"};
const test::UIPath kUsageStats = {kConsolidatedConsentId, "usageStats"};
const test::UIPath kUsageStatsToggle = {kConsolidatedConsentId, "usageOptin"};
const test::UIPath kUsageLearnMoreLink = {kConsolidatedConsentId,
                                          "usageLearnMore"};
const test::UIPath kUsageLearnMorePopUp = {kConsolidatedConsentId,
                                           "usageLearnMorePopUp"};
const test::UIPath kUsageLearnMorePopUpClose = {
    kConsolidatedConsentId, "usageLearnMorePopUp", "closeButton"};
const test::UIPath kBackup = {kConsolidatedConsentId, "backup"};
const test::UIPath kLocation = {kConsolidatedConsentId, "location"};
const test::UIPath kFooter = {kConsolidatedConsentId, "footer"};
const test::UIPath kAcceptButton = {kConsolidatedConsentId, "acceptButton"};

// Google EUlA Dialog
const test::UIPath kGoogleEulaDialog = {kConsolidatedConsentId,
                                        "googleEulaDialog"};
const test::UIPath kGoogleEulaWebview = {kConsolidatedConsentId,
                                         "googleEulaWebview"};
const test::UIPath kGoogleEulaOkButton = {kConsolidatedConsentId,
                                          "googleEulaOkButton"};

// CROS EULA Dialog
const test::UIPath kCrosEulaDialog = {kConsolidatedConsentId, "crosEulaDialog"};
const test::UIPath kCrosEulaWebview = {kConsolidatedConsentId,
                                       "crosEulaWebview"};
const test::UIPath kCrosEulaOkButton = {kConsolidatedConsentId,
                                        "crosEulaOkButton"};

// Regular user flow with ARC not enabled
class ConsolidatedConsentScreenTest : public OobeBaseTest {
 public:
  ConsolidatedConsentScreenTest() {
    feature_list_.InitAndEnableFeature(features::kOobeConsolidatedConsent);
  }

  void SetUpOnMainThread() override {
    LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build =
        true;

    // Override the screen exit callback with our own method.
    ConsolidatedConsentScreen* screen =
        WizardController::default_controller()
            ->GetScreen<ConsolidatedConsentScreen>();

    original_callback_ = screen->get_exit_callback_for_testing();
    screen->set_exit_callback_for_testing(
        base::BindRepeating(&ConsolidatedConsentScreenTest::HandleScreenExit,
                            base::Unretained(this)));

    OobeBaseTest::SetUpOnMainThread();
  }

  void LoginAsRegularUser() { login_manager_mixin_.LoginAsNewRegularUser(); }

  void WaitForScreenExit() {
    if (screen_exited_)
      return;
    base::RunLoop run_loop;
    screen_exit_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  absl::optional<ConsolidatedConsentScreen::Result> screen_result_;

 protected:
  void HandleScreenExit(ConsolidatedConsentScreen::Result result) {
    ASSERT_FALSE(screen_exited_);
    screen_exited_ = true;
    screen_result_ = result;
    original_callback_.Run(result);
    if (screen_exit_callback_)
      std::move(screen_exit_callback_).Run();
  }

  bool screen_exited_ = false;
  base::RepeatingClosure screen_exit_callback_;
  ConsolidatedConsentScreen::ScreenExitCallback original_callback_;

  base::test::ScopedFeatureList feature_list_;
  LoginManagerMixin login_manager_mixin_{&mixin_host_};
  FakeEulaMixin fake_eula_{&mixin_host_, embedded_test_server()};
};

// For regular users with ARC disabled, only usage stats opt-in is visible
// and the toggle is enabled.
IN_PROC_BROWSER_TEST_F(ConsolidatedConsentScreenTest, OptinsVisiblity) {
  LoginAsRegularUser();
  OobeScreenWaiter(ConsolidatedConsentScreenView::kScreenId).Wait();
  test::OobeJS().CreateVisibilityWaiter(true, kLoadedDialog)->Wait();

  test::OobeJS().ExpectVisiblePath(kUsageStats);
  test::OobeJS().ExpectEnabledPath(kUsageStatsToggle);
  test::OobeJS().ExpectHiddenPath(kBackup);
  test::OobeJS().ExpectHiddenPath(kLocation);
  test::OobeJS().ExpectHiddenPath(kFooter);
}

IN_PROC_BROWSER_TEST_F(ConsolidatedConsentScreenTest, GoogleEula) {
  LoginAsRegularUser();
  OobeScreenWaiter(ConsolidatedConsentScreenView::kScreenId).Wait();
  test::OobeJS().CreateVisibilityWaiter(true, kLoadedDialog)->Wait();
  test::OobeJS().ClickOnPath(kGoogleEulaLinkArcDisabled);
  test::OobeJS().CreateVisibilityWaiter(true, kGoogleEulaDialog)->Wait();
  const std::string webview_contents =
      test::GetWebViewContents(kGoogleEulaWebview);
  EXPECT_TRUE(webview_contents.find(FakeEulaMixin::kFakeOnlineEula) !=
              std::string::npos);
  test::OobeJS().ClickOnPath(kGoogleEulaOkButton);
  test::OobeJS().CreateVisibilityWaiter(true, kLoadedDialog)->Wait();
}

IN_PROC_BROWSER_TEST_F(ConsolidatedConsentScreenTest, CrosEula) {
  LoginAsRegularUser();
  OobeScreenWaiter(ConsolidatedConsentScreenView::kScreenId).Wait();
  test::OobeJS().CreateVisibilityWaiter(true, kLoadedDialog)->Wait();

  test::OobeJS().ClickOnPath(kCrosEulaLinkArcDisabled);
  test::OobeJS().CreateVisibilityWaiter(true, kCrosEulaDialog)->Wait();

  const std::string webview_contents =
      test::GetWebViewContents(kCrosEulaWebview);
  EXPECT_TRUE(webview_contents.find(FakeEulaMixin::kFakeOnlineEula) !=
              std::string::npos);

  test::OobeJS().ClickOnPath(kCrosEulaOkButton);
  test::OobeJS().CreateVisibilityWaiter(true, kLoadedDialog)->Wait();
}

IN_PROC_BROWSER_TEST_F(ConsolidatedConsentScreenTest, Accept) {
  LoginAsRegularUser();
  OobeScreenWaiter(ConsolidatedConsentScreenView::kScreenId).Wait();
  test::OobeJS().CreateVisibilityWaiter(true, kLoadedDialog)->Wait();

  test::OobeJS().ClickOnPath(kCrosEulaOkButton);
  test::OobeJS().CreateVisibilityWaiter(true, kAcceptButton)->Wait();

  test::OobeJS().ClickOnPath(kAcceptButton);
  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(),
            ConsolidatedConsentScreen::Result::ACCEPTED);
}

IN_PROC_BROWSER_TEST_F(ConsolidatedConsentScreenTest, LearnMore) {
  LoginAsRegularUser();
  OobeScreenWaiter(ConsolidatedConsentScreenView::kScreenId).Wait();
  test::OobeJS().CreateVisibilityWaiter(true, kLoadedDialog)->Wait();

  test::OobeJS().ClickOnPath(kUsageLearnMoreLink);
  test::OobeJS().ExpectAttributeEQ("open", kUsageLearnMorePopUp, true);
  test::OobeJS().ClickOnPath(kUsageLearnMorePopUpClose);
  test::OobeJS().ExpectAttributeEQ("open", kUsageLearnMorePopUp, false);
}

}  // namespace
}  // namespace ash
