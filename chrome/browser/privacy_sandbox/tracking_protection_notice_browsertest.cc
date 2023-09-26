// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/tracking_protection_notice_factory.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_onboarding_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/privacy_sandbox/tracking_protection_onboarding.h"
#include "components/user_education/test/feature_promo_test_util.h"
#include "components/user_education/views/help_bubble_factory_views.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/views/interaction/interaction_test_util_views.h"

namespace {

void WaitForFeatureEngagement(Browser* browser) {
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser);
  ASSERT_TRUE(user_education::test::WaitForFeatureEngagementReady(
      browser_view->GetFeaturePromoController()));
}

BrowserFeaturePromoController* GetFeaturePromoController(Browser* browser) {
  auto* promo_controller = static_cast<BrowserFeaturePromoController*>(
      browser->window()->GetFeaturePromoController());
  return promo_controller;
}

enum class PromoButton { kDefault, kNonDefault };

void PressPromoButton(Browser* browser,
                      PromoButton button = PromoButton::kDefault) {
  auto* const promo_controller = GetFeaturePromoController(browser);
  auto* promo_bubble = promo_controller->promo_bubble_for_testing()
                           ->AsA<user_education::HelpBubbleViews>()
                           ->bubble_view();
  switch (button) {
    case PromoButton::kDefault:
      views::test::InteractionTestUtilSimulatorViews::PressButton(
          promo_bubble->GetDefaultButtonForTesting(),
          ui::test::InteractionTestUtil::InputType::kMouse);
      return;
    case PromoButton::kNonDefault:
      views::test::InteractionTestUtilSimulatorViews::PressButton(
          promo_bubble->GetNonDefaultButtonForTesting(0),
          ui::test::InteractionTestUtil::InputType::kMouse);
      return;
  }
}

class TrackingProtectionNoticeBrowserTest : public InProcessBrowserTest {
 protected:
  TrackingProtectionNoticeBrowserTest() {
    std::vector<base::test::FeatureRef> enabled_features = {
        feature_engagement::kIPHTrackingProtectionOnboardingFeature};
    feature_list_.InitAndEnableFeatures(enabled_features);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());

    content::SetupCrossSiteRedirector(&https_server_);
    ASSERT_TRUE(https_server_.Start());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  privacy_sandbox::TrackingProtectionOnboarding* onboarding_service() {
    return TrackingProtectionOnboardingFactory::GetForProfile(
        browser()->profile());
  }

  privacy_sandbox::TrackingProtectionNoticeService* notice_service() {
    return TrackingProtectionNoticeFactory::GetForProfile(browser()->profile());
  }

  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};

 private:
  feature_engagement::test::ScopedIphFeatureList feature_list_;
};

// Navigation

// Profile marked eligible, then the user navigates to a new Secure HTTPS tab
// with the lock button.
// Should be shown the notice
IN_PROC_BROWSER_TEST_F(TrackingProtectionNoticeBrowserTest,
                       NewTabEligiblePage) {
  // Setup
  auto lock = BrowserFeaturePromoController::BlockActiveWindowCheckForTesting();
  WaitForFeatureEngagement(browser());
  onboarding_service()->MaybeMarkEligible();

  browser()->window()->Activate();
  // Action: Navigate to an HTTPS eligible page in current tab.
  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(), https_server_.GetURL("a.test", "/empty.html"), 1,
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Verification
  // Profile is onboarded
  EXPECT_EQ(onboarding_service()->GetOnboardingStatus(),
            privacy_sandbox::TrackingProtectionOnboarding::OnboardingStatus::
                kOnboarded);
  // Notice is showing.
  EXPECT_TRUE(GetFeaturePromoController(browser())->IsPromoActive(
      feature_engagement::kIPHTrackingProtectionOnboardingFeature));
}

// Profile marked eligible, the user navigates to a new Secure HTTPS tab
// with the lock button. Is shown the notice, navigates to another eligible
// page.
// Notice should remain on the page.
IN_PROC_BROWSER_TEST_F(TrackingProtectionNoticeBrowserTest,
                       SecondEligibleNavigation) {
  // Setup
  auto lock = BrowserFeaturePromoController::BlockActiveWindowCheckForTesting();
  WaitForFeatureEngagement(browser());
  onboarding_service()->MaybeMarkEligible();

  browser()->window()->Activate();
  // Action: Navigate to an HTTPS eligible page in current tab.
  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(), https_server_.GetURL("a.test", "/empty.html"), 1,
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  // Then navigate to another eligible page.
  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(), https_server_.GetURL("b.test", "/empty.html"), 1,
      WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Verification
  // Notice is showing.
  EXPECT_TRUE(GetFeaturePromoController(browser())->IsPromoActive(
      feature_engagement::kIPHTrackingProtectionOnboardingFeature));
}

// User is shown the notice, but was marked as Acked somehow.
// Hide the notice
IN_PROC_BROWSER_TEST_F(TrackingProtectionNoticeBrowserTest,
                       NoticeWasShowingWhenAckPrefUpdated) {
  // Setup
  auto lock = BrowserFeaturePromoController::BlockActiveWindowCheckForTesting();
  WaitForFeatureEngagement(browser());
  onboarding_service()->MaybeMarkEligible();

  browser()->window()->Activate();
  // Action: Navigate to an HTTPS eligible page in current tab.
  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(), https_server_.GetURL("a.test", "/empty.html"), 1,
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  // Simulate backend ack
  onboarding_service()->NoticeActionTaken(
      privacy_sandbox::TrackingProtectionOnboarding::NoticeAction::kGotIt);
  // Then navigate to another eligible page.
  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(), https_server_.GetURL("b.test", "/empty.html"), 1,
      WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Verification
  // Notice is no longer showing.
  EXPECT_FALSE(GetFeaturePromoController(browser())->IsPromoActive(
      feature_engagement::kIPHTrackingProtectionOnboardingFeature));
}

// Profile Marked eligible, added navigation to a new eligible background tab
// Current tab is eligible.
// Does not show the notice as the current tab was created before eligibility,
// therefore not tracked, and the new navigation happened in an inactive tab.
IN_PROC_BROWSER_TEST_F(TrackingProtectionNoticeBrowserTest,
                       NewBackgroundTabEligiblePage) {
  // Setup
  auto lock = BrowserFeaturePromoController::BlockActiveWindowCheckForTesting();
  WaitForFeatureEngagement(browser());
  onboarding_service()->MaybeMarkEligible();

  browser()->window()->Activate();
  // Action: Navigate to an HTTPS eligible page in current tab and New
  // background tab.
  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(), https_server_.GetURL("a.test", "/empty.html"), 1,
      WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(), https_server_.GetURL("a.test", "/empty.html"), 1,
      WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Verification
  // Profile is onboarded
  EXPECT_EQ(onboarding_service()->GetOnboardingStatus(),
            privacy_sandbox::TrackingProtectionOnboarding::OnboardingStatus::
                kEligible);
  // Notice is showing.
  EXPECT_FALSE(GetFeaturePromoController(browser())->IsPromoActive(
      feature_engagement::kIPHTrackingProtectionOnboardingFeature));
}

// Profile Marked eligible, added navigation to a new Ineligible Foreground tab
// Does not show the notice as the page isn't eligible.
IN_PROC_BROWSER_TEST_F(TrackingProtectionNoticeBrowserTest,
                       NewTabIneligiblePage) {
  // Setup
  auto lock = BrowserFeaturePromoController::BlockActiveWindowCheckForTesting();
  WaitForFeatureEngagement(browser());
  onboarding_service()->MaybeMarkEligible();

  browser()->window()->Activate();
  // Action: Navigate to an HTTP ineligible page in current tab. ( No lock icon)
  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(), embedded_test_server()->GetURL("a.test", "/empty.html"), 1,
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Verification
  // Profile stays Eligible
  EXPECT_EQ(onboarding_service()->GetOnboardingStatus(),
            privacy_sandbox::TrackingProtectionOnboarding::OnboardingStatus::
                kEligible);
  // Notice is not showing.
  EXPECT_FALSE(GetFeaturePromoController(browser())->IsPromoActive(
      feature_engagement::kIPHTrackingProtectionOnboardingFeature));
}

// Switching between eligible/ineligible tabs shows/hides the notice
// accordingly.
IN_PROC_BROWSER_TEST_F(TrackingProtectionNoticeBrowserTest, SwitchesTabs) {
  // Setup
  auto lock = BrowserFeaturePromoController::BlockActiveWindowCheckForTesting();
  WaitForFeatureEngagement(browser());

  browser()->window()->Activate();
  //  Navigate to an HTTPS eligible page in current tab
  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(), https_server_.GetURL("a.test", "/empty.html"), 1,
      WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  // Creates new background tab and navigates to Ineligible page.
  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(), embedded_test_server()->GetURL("b.test", "/empty.html"), 1,
      WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Action: Profile becomes eligible.
  onboarding_service()->MaybeMarkEligible();

  // Verification
  // Notice is not yet showing.
  EXPECT_FALSE(GetFeaturePromoController(browser())->IsPromoActive(
      feature_engagement::kIPHTrackingProtectionOnboardingFeature));

  // This selects the second tab (ineligible). Promo shouldn't show, and profile
  // not yet onboarded.
  browser()->tab_strip_model()->SelectNextTab();
  EXPECT_FALSE(GetFeaturePromoController(browser())->IsPromoActive(
      feature_engagement::kIPHTrackingProtectionOnboardingFeature));
  EXPECT_EQ(onboarding_service()->GetOnboardingStatus(),
            privacy_sandbox::TrackingProtectionOnboarding::OnboardingStatus::
                kEligible);

  // Goes back to eligible tab. Promo will show, and profile is onboarded.
  browser()->tab_strip_model()->SelectPreviousTab();
  EXPECT_TRUE(GetFeaturePromoController(browser())->IsPromoActive(
      feature_engagement::kIPHTrackingProtectionOnboardingFeature));
  EXPECT_EQ(onboarding_service()->GetOnboardingStatus(),
            privacy_sandbox::TrackingProtectionOnboarding::OnboardingStatus::
                kOnboarded);

  // Goes to the ineligible tab again. Notice should hide, and profile remain
  // onboarded.
  browser()->tab_strip_model()->SelectNextTab();
  EXPECT_FALSE(GetFeaturePromoController(browser())->IsPromoActive(
      feature_engagement::kIPHTrackingProtectionOnboardingFeature));
  EXPECT_EQ(onboarding_service()->GetOnboardingStatus(),
            privacy_sandbox::TrackingProtectionOnboarding::OnboardingStatus::
                kOnboarded);
}

// Popup to eligible page does not show the notice.
IN_PROC_BROWSER_TEST_F(TrackingProtectionNoticeBrowserTest,
                       NewPopupEligiblePage) {
  // Setup
  auto lock = BrowserFeaturePromoController::BlockActiveWindowCheckForTesting();
  WaitForFeatureEngagement(browser());
  onboarding_service()->MaybeMarkEligible();

  browser()->window()->Activate();
  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(), https_server_.GetURL("a.test", "/empty.html"), 1,
      WindowOpenDisposition::NEW_POPUP,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Verification
  // Profile is not onboarded - remains eligible.
  EXPECT_EQ(onboarding_service()->GetOnboardingStatus(),
            privacy_sandbox::TrackingProtectionOnboarding::OnboardingStatus::
                kEligible);
  // Notice is Not showing.
  EXPECT_FALSE(GetFeaturePromoController(browser())->IsPromoActive(
      feature_engagement::kIPHTrackingProtectionOnboardingFeature));
}

// New Browser Window picks up the promo if it navigates to an eligible page.
IN_PROC_BROWSER_TEST_F(TrackingProtectionNoticeBrowserTest,
                       NewWindowEligiblePage) {
  // Setup
  auto lock = BrowserFeaturePromoController::BlockActiveWindowCheckForTesting();
  WaitForFeatureEngagement(browser());
  onboarding_service()->MaybeMarkEligible();

  browser()->window()->Activate();
  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(), https_server_.GetURL("a.test", "/empty.html"), 1,
      WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Notice is showing on the new active window.
  EXPECT_TRUE(
      GetFeaturePromoController(BrowserList::GetInstance()->get(1))
          ->IsPromoActive(
              feature_engagement::kIPHTrackingProtectionOnboardingFeature));
}

// The promo will only show on a single window.
IN_PROC_BROWSER_TEST_F(TrackingProtectionNoticeBrowserTest,
                       FirstWindowEligibleSecondWindowEligible) {
  // Setup
  auto lock = BrowserFeaturePromoController::BlockActiveWindowCheckForTesting();
  WaitForFeatureEngagement(browser());
  onboarding_service()->MaybeMarkEligible();

  browser()->window()->Activate();
  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(), https_server_.GetURL("a.test", "/empty.html"), 1,
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Promo shown on first window as expected.
  EXPECT_TRUE(GetFeaturePromoController(browser())->IsPromoActive(
      feature_engagement::kIPHTrackingProtectionOnboardingFeature));

  // Open new eligible window.
  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(), https_server_.GetURL("b.test", "/empty.html"), 1,
      WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Verification
  EXPECT_TRUE(GetFeaturePromoController(browser())->IsPromoActive(
      feature_engagement::kIPHTrackingProtectionOnboardingFeature));

  // Doesn't create a second notice on the second window.
  EXPECT_FALSE(
      GetFeaturePromoController(BrowserList::GetInstance()->get(1))
          ->IsPromoActive(
              feature_engagement::kIPHTrackingProtectionOnboardingFeature));
}

// Notice Acknowledgement

// Profile marked Onboarded, but not yet acknowledged still shows the notice.
IN_PROC_BROWSER_TEST_F(TrackingProtectionNoticeBrowserTest, OnboardedNotAck) {
  // Setup
  auto lock = BrowserFeaturePromoController::BlockActiveWindowCheckForTesting();
  WaitForFeatureEngagement(browser());

  onboarding_service()->MaybeMarkEligible();
  // Telling the OnboardingService that the notice has been shown so it marks
  // the profile as Onboarded.
  onboarding_service()->NoticeShown();

  // Action: Navigate to an HTTPS eligible page in current tab.
  browser()->window()->Activate();
  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(), https_server_.GetURL("a.test", "/empty.html"), 1,
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Verification
  // Notice is showing.
  EXPECT_TRUE(GetFeaturePromoController(browser())->IsPromoActive(
      feature_engagement::kIPHTrackingProtectionOnboardingFeature));
}

// Profile marked Onboarded and Ack no longer shows the notice.
IN_PROC_BROWSER_TEST_F(TrackingProtectionNoticeBrowserTest,
                       AcknowledgesTheNotice) {
  // Setup
  auto lock = BrowserFeaturePromoController::BlockActiveWindowCheckForTesting();
  WaitForFeatureEngagement(browser());
  // Action
  onboarding_service()->MaybeMarkEligible();
  // Navigates to eligible page.
  browser()->window()->Activate();
  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(), https_server_.GetURL("a.test", "/empty.html"), 1,
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  PressPromoButton(browser(), PromoButton::kNonDefault);

  // Verification - Notice acknowledged.
  EXPECT_FALSE(GetFeaturePromoController(browser())->IsPromoActive(
      feature_engagement::kIPHTrackingProtectionOnboardingFeature));
  EXPECT_FALSE(onboarding_service()->ShouldShowOnboardingNotice());
}

// Observation

// Profile is ineligible. Notice Service is not observing tab changes.
IN_PROC_BROWSER_TEST_F(TrackingProtectionNoticeBrowserTest,
                       DoesntStartObserving) {
  EXPECT_FALSE(TabStripModelObserver::IsObservingAny(notice_service()));
  EXPECT_FALSE(privacy_sandbox::TrackingProtectionNoticeService::TabHelper::
                   IsHelperNeeded(browser()->profile()));
}

// Profile is eligible. Notice service is observing tab changes.
IN_PROC_BROWSER_TEST_F(TrackingProtectionNoticeBrowserTest, StartsObserving) {
  // Action
  onboarding_service()->MaybeMarkEligible();
  browser()->window()->Activate();
  // Verification
  EXPECT_TRUE(TabStripModelObserver::IsObservingAny(notice_service()));
  EXPECT_TRUE(privacy_sandbox::TrackingProtectionNoticeService::TabHelper::
                  IsHelperNeeded(browser()->profile()));
}

// Notice is acknowledged. Notice Service stops observing tab changes.
IN_PROC_BROWSER_TEST_F(TrackingProtectionNoticeBrowserTest, StopsObserving) {
  // Setup
  auto lock = BrowserFeaturePromoController::BlockActiveWindowCheckForTesting();
  WaitForFeatureEngagement(browser());
  // Action
  onboarding_service()->MaybeMarkEligible();
  // Navigates to eligible page.
  browser()->window()->Activate();
  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(), https_server_.GetURL("a.test", "/empty.html"), 1,
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  PressPromoButton(browser());

  // Verification - Observation stops
  EXPECT_FALSE(TabStripModelObserver::IsObservingAny(notice_service()));
  EXPECT_FALSE(privacy_sandbox::TrackingProtectionNoticeService::TabHelper::
                   IsHelperNeeded(browser()->profile()));
}

}  // namespace
