// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/dice_tab_helper.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/base/signin_metrics.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "google_apis/gaia/gaia_urls.h"
#include "testing/gtest/include/gtest/gtest.h"

class DiceTabHelperTest : public ChromeRenderViewHostTestHarness {
 public:
  DiceTabHelperTest() {
    signin_url_ = GaiaUrls::GetInstance()->signin_chrome_sync_dice();
  }

  // Does a navigation to Gaia and initializes the tab helper.
  void InitializeDiceTabHelper(DiceTabHelper* helper,
                               signin_metrics::AccessPoint access_point,
                               signin_metrics::Reason reason) {
    // Load the signin page.
    std::unique_ptr<content::NavigationSimulator> simulator =
        content::NavigationSimulator::CreateRendererInitiated(signin_url_,
                                                              main_rfh());
    simulator->Start();
    helper->InitializeSigninFlow(
        signin_url_, access_point, reason,
        signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO,
        GURL::EmptyGURL());
    EXPECT_TRUE(helper->IsChromeSigninPage());
    simulator->Commit();
  }

  GURL signin_url_;
};

// Tests DiceTabHelper intialization.
TEST_F(DiceTabHelperTest, Initialization) {
  DiceTabHelper::CreateForWebContents(web_contents());
  DiceTabHelper* dice_tab_helper =
      DiceTabHelper::FromWebContents(web_contents());

  // Check default state.
  EXPECT_EQ(signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN,
            dice_tab_helper->signin_access_point());
  EXPECT_EQ(signin_metrics::Reason::REASON_UNKNOWN_REASON,
            dice_tab_helper->signin_reason());
  EXPECT_FALSE(dice_tab_helper->IsChromeSigninPage());

  // Initialize the signin flow.
  signin_metrics::AccessPoint access_point =
      signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_BUBBLE;
  signin_metrics::Reason reason =
      signin_metrics::Reason::REASON_SIGNIN_PRIMARY_ACCOUNT;
  InitializeDiceTabHelper(dice_tab_helper, access_point, reason);
  EXPECT_EQ(access_point, dice_tab_helper->signin_access_point());
  EXPECT_EQ(reason, dice_tab_helper->signin_reason());
  EXPECT_TRUE(dice_tab_helper->IsChromeSigninPage());
}

TEST_F(DiceTabHelperTest, SigninPageStatus) {
  DiceTabHelper::CreateForWebContents(web_contents());
  DiceTabHelper* dice_tab_helper =
      DiceTabHelper::FromWebContents(web_contents());
  EXPECT_FALSE(dice_tab_helper->IsChromeSigninPage());

  // Load the signin page.
  signin_metrics::AccessPoint access_point =
      signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_BUBBLE;
  signin_metrics::Reason reason =
      signin_metrics::Reason::REASON_SIGNIN_PRIMARY_ACCOUNT;
  InitializeDiceTabHelper(dice_tab_helper, access_point, reason);
  EXPECT_TRUE(dice_tab_helper->IsChromeSigninPage());

  // Reloading the signin page does not interrupt the signin flow.
  content::NavigationSimulator::Reload(web_contents());
  EXPECT_TRUE(dice_tab_helper->IsChromeSigninPage());

  // Subframe navigation are ignored.
  std::unique_ptr<content::NavigationSimulator> simulator =
      content::NavigationSimulator::CreateRendererInitiated(
          signin_url_.Resolve("#baz"), main_rfh());
  simulator->CommitSameDocument();
  EXPECT_TRUE(dice_tab_helper->IsChromeSigninPage());

  // Navigation in subframe does not interrupt the signin flow.
  content::RenderFrameHostTester* render_frame_host_tester =
      content::RenderFrameHostTester::For(main_rfh());
  content::RenderFrameHost* sub_frame =
      render_frame_host_tester->AppendChild("subframe");
  content::NavigationSimulator::NavigateAndCommitFromDocument(signin_url_,
                                                              sub_frame);
  EXPECT_TRUE(dice_tab_helper->IsChromeSigninPage());

  // Navigating to a different page resets the page status.
  simulator = content::NavigationSimulator::CreateRendererInitiated(
      signin_url_.Resolve("/foo"), main_rfh());
  simulator->Start();
  EXPECT_FALSE(dice_tab_helper->IsChromeSigninPage());
  simulator->Commit();
  EXPECT_FALSE(dice_tab_helper->IsChromeSigninPage());
}

// Tests DiceTabHelper metrics.
TEST_F(DiceTabHelperTest, Metrics) {
  base::UserActionTester ua_tester;
  base::HistogramTester h_tester;
  DiceTabHelper::CreateForWebContents(web_contents());
  DiceTabHelper* dice_tab_helper =
      DiceTabHelper::FromWebContents(web_contents());

  // No metrics are logged when the Dice tab helper is created.
  EXPECT_EQ(0, ua_tester.GetActionCount("Signin_Signin_FromStartPage"));
  EXPECT_EQ(0, ua_tester.GetActionCount("Signin_SigninPage_Loading"));
  EXPECT_EQ(0, ua_tester.GetActionCount("Signin_SigninPage_Shown"));

  // Check metrics logged when the Dice tab helper is initialized.
  std::unique_ptr<content::NavigationSimulator> simulator =
      content::NavigationSimulator::CreateRendererInitiated(signin_url_,
                                                            main_rfh());
  simulator->Start();
  dice_tab_helper->InitializeSigninFlow(
      signin_url_, signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS,
      signin_metrics::Reason::REASON_SIGNIN_PRIMARY_ACCOUNT,
      signin_metrics::PromoAction::PROMO_ACTION_NEW_ACCOUNT_NO_EXISTING_ACCOUNT,
      GURL::EmptyGURL());
  EXPECT_EQ(1, ua_tester.GetActionCount("Signin_Signin_FromSettings"));
  EXPECT_EQ(1, ua_tester.GetActionCount("Signin_SigninPage_Loading"));
  EXPECT_EQ(0, ua_tester.GetActionCount("Signin_SigninPage_Shown"));
  h_tester.ExpectUniqueSample(
      "Signin.SigninStartedAccessPoint",
      signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS, 1);
  h_tester.ExpectUniqueSample(
      "Signin.SigninStartedAccessPoint.NewAccountNoExistingAccount",
      signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS, 1);

  // First call to did finish load does logs any Signin_SigninPage_Shown user
  // action.
  simulator->Commit();
  EXPECT_EQ(1, ua_tester.GetActionCount("Signin_SigninPage_Loading"));
  EXPECT_EQ(1, ua_tester.GetActionCount("Signin_SigninPage_Shown"));

  // Second call to did finish load does not log any metrics.
  dice_tab_helper->DidFinishLoad(main_rfh(), signin_url_);
  EXPECT_EQ(1, ua_tester.GetActionCount("Signin_SigninPage_Loading"));
  EXPECT_EQ(1, ua_tester.GetActionCount("Signin_SigninPage_Shown"));

  // Check metrics are logged again when the Dice tab helper is re-initialized.
  simulator = content::NavigationSimulator::CreateRendererInitiated(signin_url_,
                                                                    main_rfh());
  simulator->Start();
  dice_tab_helper->InitializeSigninFlow(
      signin_url_, signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS,
      signin_metrics::Reason::REASON_SIGNIN_PRIMARY_ACCOUNT,
      signin_metrics::PromoAction::PROMO_ACTION_WITH_DEFAULT,
      GURL::EmptyGURL());
  EXPECT_EQ(2, ua_tester.GetActionCount("Signin_Signin_FromSettings"));
  EXPECT_EQ(2, ua_tester.GetActionCount("Signin_SigninPage_Loading"));
  h_tester.ExpectUniqueSample(
      "Signin.SigninStartedAccessPoint",
      signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS, 2);
  h_tester.ExpectUniqueSample(
      "Signin.SigninStartedAccessPoint.WithDefault",
      signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS, 1);
}
