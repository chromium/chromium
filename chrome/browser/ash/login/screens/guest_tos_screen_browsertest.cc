// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/guest_tos_screen.h"

#include "ash/constants/ash_switches.h"
#include "base/containers/contains.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/test/fake_eula_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/webview_content_extractor.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/ash/login/webui_login_view.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/ash/login/guest_tos_screen_handler.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {
namespace {

constexpr char kGuestTostId[] = "guest-tos";

// Overview Dialog
const test::UIPath kOverviewDialog = {kGuestTostId, "overview"};
const test::UIPath kGoogleEulaLink = {kGuestTostId, "googleEulaLink"};
const test::UIPath kCrosEulaLink = {kGuestTostId, "crosEulaLink"};

const test::UIPath kUsageStats = {kGuestTostId, "usageStats"};
const test::UIPath kUsageLearnMoreLink = {kGuestTostId, "usageLearnMore"};
const test::UIPath kUsageLearnMorePopUp = {kGuestTostId, "usageLearnMorePopUp"};
const test::UIPath kUsageLearnMorePopUpClose = {
    kGuestTostId, "usageLearnMorePopUp", "closeButton"};

// Google EUlA Dialog
const test::UIPath kGoogleEulaDialog = {kGuestTostId, "googleEulaDialog"};
const test::UIPath kGoogleEulaWebview = {kGuestTostId,
                                         "guestTosGoogleEulaWebview"};
const test::UIPath kGoogleEulaOkButton = {kGuestTostId, "googleEulaOkButton"};

// CROS EULA Dialog
const test::UIPath kCrosEulaDialog = {kGuestTostId, "crosEulaDialog"};
const test::UIPath kCrosEulaWebview = {kGuestTostId, "guestTosCrosEulaWebview"};
const test::UIPath kCrosEulaOkButton = {kGuestTostId, "crosEulaOkButton"};

}  // namespace

class GuestTosScreenTest : public OobeBaseTest {
 public:
  void SetUpOnMainThread() override {
    LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build =
        true;
    OobeBaseTest::SetUpOnMainThread();
  }

  void ShowGuestTosScreen() {
    WizardController::default_controller()->AdvanceToScreen(
        GuestTosScreenView::kScreenId);
    OobeScreenWaiter(GuestTosScreenView::kScreenId).Wait();
    test::OobeJS().CreateVisibilityWaiter(true, kOverviewDialog)->Wait();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;

  FakeEulaMixin fake_eula_{&mixin_host_, embedded_test_server()};
};

IN_PROC_BROWSER_TEST_F(GuestTosScreenTest, GoogleEula) {
  ShowGuestTosScreen();
  test::OobeJS().CreateVisibilityWaiter(true, kOverviewDialog)->Wait();
  test::OobeJS().ClickOnPath(kGoogleEulaLink);
  test::OobeJS().CreateVisibilityWaiter(true, kGoogleEulaDialog)->Wait();
  const std::string webview_contents =
      test::GetWebViewContents(kGoogleEulaWebview);
  EXPECT_TRUE(base::Contains(webview_contents, FakeEulaMixin::kFakeOnlineEula));
  test::OobeJS().ClickOnPath(kGoogleEulaOkButton);
  test::OobeJS().CreateVisibilityWaiter(true, kOverviewDialog)->Wait();
}

IN_PROC_BROWSER_TEST_F(GuestTosScreenTest, CrosEula) {
  ShowGuestTosScreen();
  test::OobeJS().CreateVisibilityWaiter(true, kOverviewDialog)->Wait();
  test::OobeJS().ClickOnPath(kCrosEulaLink);
  test::OobeJS().CreateVisibilityWaiter(true, kCrosEulaDialog)->Wait();
  const std::string webview_contents =
      test::GetWebViewContents(kCrosEulaWebview);
  EXPECT_TRUE(base::Contains(webview_contents, FakeEulaMixin::kFakeOnlineEula));
  test::OobeJS().ClickOnPath(kCrosEulaOkButton);
  test::OobeJS().CreateVisibilityWaiter(true, kOverviewDialog)->Wait();
}

IN_PROC_BROWSER_TEST_F(GuestTosScreenTest, UsageStatsOptin) {
  ShowGuestTosScreen();
  test::OobeJS().ExpectVisiblePath(kUsageStats);
  test::OobeJS().ClickOnPath(kUsageLearnMoreLink);
  test::OobeJS().ExpectAttributeEQ("open", kUsageLearnMorePopUp, true);
  test::OobeJS().ClickOnPath(kUsageLearnMorePopUpClose);
  test::OobeJS().ExpectAttributeEQ("open", kUsageLearnMorePopUp, false);
}

}  // namespace ash
