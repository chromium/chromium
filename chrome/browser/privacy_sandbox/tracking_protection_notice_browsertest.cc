// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_notice_factory.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_onboarding_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tpcd/experiment/tpcd_experiment_features.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/privacy_sandbox/tracking_protection_onboarding.h"
#include "components/user_education/views/help_bubble_factory_views.h"
#include "components/user_education/views/help_bubble_view.h"
#include "components/version_info/channel.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/views/interaction/interaction_test_util_views.h"

namespace privacy_sandbox {

namespace {

using ::testing::_;

using NoticeAction =
    privacy_sandbox::TrackingProtectionOnboarding::NoticeAction;
using NoticeType = privacy_sandbox::TrackingProtectionOnboarding::NoticeType;

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
}  // namespace

class TrackingProtectionBaseNoticeBrowserTest
    : public InteractiveFeaturePromoTest {
 protected:
  explicit TrackingProtectionBaseNoticeBrowserTest(
      const std::vector<base::test::FeatureRef>& enabled_features,
      const std::vector<base::test::FeatureRef>& disabled_features = {})
      : InteractiveFeaturePromoTest(
            UseDefaultTrackerAllowingPromos(enabled_features)) {
    disabled_features_.InitWithFeatures({}, disabled_features);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());

    content::SetupCrossSiteRedirector(&https_server_);
    ASSERT_TRUE(https_server_.Start());
    ASSERT_TRUE(embedded_test_server()->Start());
    InteractiveFeaturePromoTest::SetUpOnMainThread();
  }

  privacy_sandbox::TrackingProtectionOnboarding* onboarding_service() {
    return TrackingProtectionOnboardingFactory::GetForProfile(
        browser()->profile());
  }

  privacy_sandbox::TrackingProtectionNoticeService* notice_service() {
    return TrackingProtectionNoticeFactory::GetForProfile(browser()->profile());
  }

  virtual std::vector<base::test::FeatureRef> EnabledFeatures() = 0;
  virtual std::vector<base::test::FeatureRef> DisabledFeatures() = 0;

  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  base::HistogramTester histogram_tester_;

 private:
  base::test::ScopedFeatureList disabled_features_;
};

class TrackingProtectionOnboardingNoticeBrowserTest
    : public TrackingProtectionBaseNoticeBrowserTest {
 protected:
  TrackingProtectionOnboardingNoticeBrowserTest()
      : TrackingProtectionBaseNoticeBrowserTest(EnabledFeatures(),
                                                DisabledFeatures()) {}

  std::vector<base::test::FeatureRef> EnabledFeatures() override {
    return {feature_engagement::kIPHTrackingProtectionOnboardingFeature};
  }

  std::vector<base::test::FeatureRef> DisabledFeatures() override {
    return {features::kCookieDeprecationFacilitatedTesting};
  }

  bool IsOnboardingPromoActive(Browser* browser) {
    return GetFeaturePromoController(browser)->IsPromoActive(
        feature_engagement::kIPHTrackingProtectionOnboardingFeature);
  }
};

// Navigation

// Profile marked eligible, then the user navigates to a new Secure HTTPS tab
// with the lock button.
// Should be shown the notice
IN_PROC_BROWSER_TEST_F(TrackingProtectionOnboardingNoticeBrowserTest,
                       NewTabEligiblePage) {
  // Setup
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
  EXPECT_TRUE(IsOnboardingPromoActive(browser()));
}

// Profile marked eligible, the user navigates to a new Secure HTTPS tab
// with the lock button. Is shown the notice, navigates to another eligible
// page.
// Notice should remain on the page.
IN_PROC_BROWSER_TEST_F(TrackingProtectionOnboardingNoticeBrowserTest,
                       SecondEligibleNavigation) {
  // Setup
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
  EXPECT_TRUE(IsOnboardingPromoActive(browser()));
}

// User is shown the notice, but was marked as Acked somehow.
// Hide the notice
IN_PROC_BROWSER_TEST_F(TrackingProtectionOnboardingNoticeBrowserTest,
                       NoticeWasShowingWhenAckPrefUpdated) {
  // Setup
  onboarding_service()->MaybeMarkEligible();

  browser()->window()->Activate();
  // Action: Navigate to an HTTPS eligible page in current tab.
  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(), https_server_.GetURL("a.test", "/empty.html"), 1,
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  // Simulate backend ack
  onboarding_service()->OnboardingNoticeActionTaken(
      privacy_sandbox::TrackingProtectionOnboarding::NoticeAction::kGotIt);
  // Then navigate to another eligible page.
  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(), https_server_.GetURL("b.test", "/empty.html"), 1,
      WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Verification
  // Notice is no longer showing.
  EXPECT_FALSE(IsOnboardingPromoActive(browser()));
}

// Profile Marked eligible, added navigation to a new eligible background tab
// Current tab is eligible.
// Does not show the notice as the current tab was created before eligibility,
// therefore not tracked, and the new navigation happened in an inactive tab.
IN_PROC_BROWSER_TEST_F(TrackingProtectionOnboardingNoticeBrowserTest,
                       NewBackgroundTabEligiblePage) {
  // Setup
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
  EXPECT_FALSE(IsOnboardingPromoActive(browser()));
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.Onboarding.NoticeServiceEvent",
      privacy_sandbox::TrackingProtectionNoticeService::
          TrackingProtectionMetricsNoticeEvent::kInactiveWebcontentUpdated,
      1);
}

// Profile Marked eligible, added navigation to a new Ineligible Foreground tab
// Does not show the notice as the page isn't eligible.
IN_PROC_BROWSER_TEST_F(TrackingProtectionOnboardingNoticeBrowserTest,
                       NewTabIneligiblePage) {
  // Setup
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
  EXPECT_FALSE(IsOnboardingPromoActive(browser()));
}

// Switching between eligible/ineligible tabs shows/hides the notice
// accordingly.
IN_PROC_BROWSER_TEST_F(TrackingProtectionOnboardingNoticeBrowserTest,
                       SwitchesTabs) {
  // Setup
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
  EXPECT_FALSE(IsOnboardingPromoActive(browser()));

  // This selects the second tab (ineligible). Promo shouldn't show, and profile
  // not yet onboarded.
  browser()->tab_strip_model()->SelectNextTab();
  EXPECT_FALSE(IsOnboardingPromoActive(browser()));
  EXPECT_EQ(onboarding_service()->GetOnboardingStatus(),
            privacy_sandbox::TrackingProtectionOnboarding::OnboardingStatus::
                kEligible);

  // Goes back to eligible tab. Promo will show, and profile is onboarded.
  browser()->tab_strip_model()->SelectPreviousTab();
  EXPECT_TRUE(IsOnboardingPromoActive(browser()));
  EXPECT_EQ(onboarding_service()->GetOnboardingStatus(),
            privacy_sandbox::TrackingProtectionOnboarding::OnboardingStatus::
                kOnboarded);

  // Goes to the ineligible tab again. Notice should hide, and profile remain
  // onboarded.
  browser()->tab_strip_model()->SelectNextTab();
  EXPECT_FALSE(IsOnboardingPromoActive(browser()));
  EXPECT_EQ(onboarding_service()->GetOnboardingStatus(),
            privacy_sandbox::TrackingProtectionOnboarding::OnboardingStatus::
                kOnboarded);
}

// Popup to eligible page does not show the notice.
IN_PROC_BROWSER_TEST_F(TrackingProtectionOnboardingNoticeBrowserTest,
                       NewPopupEligiblePage) {
  // Setup
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
  EXPECT_FALSE(IsOnboardingPromoActive(browser()));
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.Onboarding.NoticeServiceEvent",
      privacy_sandbox::TrackingProtectionNoticeService::
          TrackingProtectionMetricsNoticeEvent::kBrowserTypeNonNormal,
      1);
}

// New Browser Window picks up the promo if it navigates to an eligible page.
IN_PROC_BROWSER_TEST_F(TrackingProtectionOnboardingNoticeBrowserTest,
                       NewWindowEligiblePage) {
  // Setup
  onboarding_service()->MaybeMarkEligible();

  browser()->window()->Activate();

  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(), https_server_.GetURL("a.test", "/empty.html"), 1,
      WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Notice is showing on the new active window.
  EXPECT_TRUE(IsOnboardingPromoActive(BrowserList::GetInstance()->get(1)));

  // These histograms are emitted due to the location icon is not secure/visible
  // due to the first page load being a non secure page.
  // Once the navigation to empty.html goes through, the promo is then active
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.Onboarding.NoticeServiceEvent",
      privacy_sandbox::TrackingProtectionNoticeService::
          TrackingProtectionMetricsNoticeEvent::kLocationIconNonVisible,
      1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.Onboarding.NoticeServiceEvent",
      privacy_sandbox::TrackingProtectionNoticeService::
          TrackingProtectionMetricsNoticeEvent::kLocationIconNonSecure,
      1);
}

// The promo will only show on a single window.
IN_PROC_BROWSER_TEST_F(TrackingProtectionOnboardingNoticeBrowserTest,
                       FirstWindowEligibleSecondWindowEligible) {
  // Setup
  onboarding_service()->MaybeMarkEligible();

  browser()->window()->Activate();
  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(), https_server_.GetURL("a.test", "/empty.html"), 1,
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Promo shown on first window as expected.
  EXPECT_TRUE(IsOnboardingPromoActive(browser()));

  // Open new eligible window.
  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(), https_server_.GetURL("b.test", "/empty.html"), 1,
      WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Verification
  EXPECT_TRUE(IsOnboardingPromoActive(browser()));

  // Doesn't create a second notice on the second window.
  EXPECT_FALSE(IsOnboardingPromoActive(BrowserList::GetInstance()->get(1)));
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.Onboarding.NoticeServiceEvent",
      privacy_sandbox::TrackingProtectionNoticeService::
          TrackingProtectionMetricsNoticeEvent::kNoticeRequestedButNotShown,
      1);
}

// Notice Acknowledgement

// Profile marked Onboarded, but not yet acknowledged still shows the notice.
IN_PROC_BROWSER_TEST_F(TrackingProtectionOnboardingNoticeBrowserTest,
                       OnboardedNotAck) {
  // Setup
  onboarding_service()->MaybeMarkEligible();
  // Telling the OnboardingService that the notice has been shown so it marks
  // the profile as Onboarded.
  onboarding_service()->OnboardingNoticeShown();

  // Action: Navigate to an HTTPS eligible page in current tab.
  browser()->window()->Activate();
  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(), https_server_.GetURL("a.test", "/empty.html"), 1,
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Verification
  // Notice is showing.
  EXPECT_TRUE(IsOnboardingPromoActive(browser()));
}

// Profile marked Onboarded and Ack no longer shows the notice.
IN_PROC_BROWSER_TEST_F(TrackingProtectionOnboardingNoticeBrowserTest,
                       AcknowledgesTheNotice) {
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
  EXPECT_FALSE(IsOnboardingPromoActive(browser()));
  EXPECT_FALSE(onboarding_service()->ShouldShowOnboardingNotice());
}

// Profile marked Onboarded and Ack, then onboarding prefs reset, then profile
// marked eligible again. Shouldn't show the notice again, but will be
// considered onboarded.
IN_PROC_BROWSER_TEST_F(TrackingProtectionOnboardingNoticeBrowserTest,
                       TreatsAsShownIfPreviouslyDismissed) {
  // Setup
  onboarding_service()->channel_ = version_info::Channel::CANARY;

  // Action Onboarding and ack the user
  onboarding_service()->MaybeMarkEligible();
  browser()->window()->Activate();
  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(), https_server_.GetURL("a.test", "/empty.html"), 1,
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  // Notice should be showing at this point.
  EXPECT_TRUE(IsOnboardingPromoActive(browser()));
  // Ack the notice.
  PressPromoButton(browser(), PromoButton::kNonDefault);

  // Then reset the user prefs.
  onboarding_service()->MaybeResetOnboardingPrefs();
  // Then mark as eligible again
  onboarding_service()->MaybeMarkEligible();

  EXPECT_EQ(onboarding_service()->GetOnboardingStatus(),
            privacy_sandbox::TrackingProtectionOnboarding::OnboardingStatus::
                kEligible);

  // Navigates to any page.
  browser()->window()->Activate();
  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(), https_server_.GetURL("a.test", "/empty.html"), 1,
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Verification - Notice not shown again, but the profile is onboarded.
  EXPECT_FALSE(IsOnboardingPromoActive(browser()));
  EXPECT_FALSE(onboarding_service()->ShouldShowOnboardingNotice());
  EXPECT_EQ(onboarding_service()->GetOnboardingStatus(),
            privacy_sandbox::TrackingProtectionOnboarding::OnboardingStatus::
                kOnboarded);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.Onboarding.NoticeServiceEvent",
      privacy_sandbox::TrackingProtectionNoticeService::
          TrackingProtectionMetricsNoticeEvent::kPromoPreviouslyDismissed,
      1);
}

// Observation

// Profile is ineligible. Notice Service is not observing tab changes.
IN_PROC_BROWSER_TEST_F(TrackingProtectionOnboardingNoticeBrowserTest,
                       DoesntStartObserving) {
  EXPECT_FALSE(TabStripModelObserver::IsObservingAny(notice_service()));
  EXPECT_FALSE(privacy_sandbox::TrackingProtectionNoticeService::TabHelper::
                   IsHelperNeeded(browser()->profile()));
}

// Profile is eligible. Notice service is observing tab changes.
IN_PROC_BROWSER_TEST_F(TrackingProtectionOnboardingNoticeBrowserTest,
                       StartsObserving) {
  // Action
  onboarding_service()->MaybeMarkEligible();
  browser()->window()->Activate();
  // Verification
  EXPECT_TRUE(TabStripModelObserver::IsObservingAny(notice_service()));
  EXPECT_TRUE(privacy_sandbox::TrackingProtectionNoticeService::TabHelper::
                  IsHelperNeeded(browser()->profile()));
}

// Notice is acknowledged. Notice Service stops observing tab changes.
IN_PROC_BROWSER_TEST_F(TrackingProtectionOnboardingNoticeBrowserTest,
                       StopsObserving) {
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

  // Once the notice object is created, the tab strip tracker is initialized but
  // in this test we press the promo button, which also causes the
  // tracker to be reset again.
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.NoticeService."
      "IsObservingTabStripModel",
      true, 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.NoticeService."
      "IsObservingTabStripModel",
      false, 1);
}

IN_PROC_BROWSER_TEST_F(TrackingProtectionOnboardingNoticeBrowserTest,
                       NoticeServiceEventHistogramCheck) {
  // Setup
  onboarding_service()->MaybeMarkEligible();

  browser()->window()->Activate();

  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.Onboarding.NoticeServiceEvent",
      privacy_sandbox::TrackingProtectionNoticeService::
          TrackingProtectionMetricsNoticeEvent::kNoticeObjectCreated,
      1);

  // Action: Navigate to an HTTPS eligible page in current tab.
  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(), https_server_.GetURL("a.test", "/empty.html"), 1,
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // On the first load the OnTabStripModelChanged is invoked causing the
  // MaybeUpdateNoticeVisibility function to be called. Then once the navigation
  // goes through which also calls the MaybeUpdateNoticeVisibility which is why
  // there are two histograms emitted for kUpdateNoticeVisibility.
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.Onboarding.NoticeServiceEvent",
      privacy_sandbox::TrackingProtectionNoticeService::
          TrackingProtectionMetricsNoticeEvent::kUpdateNoticeVisibility,
      2);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.Onboarding.NoticeServiceEvent",
      privacy_sandbox::TrackingProtectionNoticeService::
          TrackingProtectionMetricsNoticeEvent::kActiveTabChanged,
      1);

  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.Onboarding.NoticeServiceEvent",
      privacy_sandbox::TrackingProtectionNoticeService::
          TrackingProtectionMetricsNoticeEvent::kNoticeRequestedAndShown,
      1);

  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.Onboarding.NoticeServiceEvent",
      privacy_sandbox::TrackingProtectionNoticeService::
          TrackingProtectionMetricsNoticeEvent::kNavigationFinished,
      1);
  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(), https_server_.GetURL("b.test", "/empty.html"), 1,
      WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.Onboarding.NoticeServiceEvent",
      privacy_sandbox::TrackingProtectionNoticeService::
          TrackingProtectionMetricsNoticeEvent::kNoticeAlreadyShowing,
      1);

  // Acknowledging the notice with the "Got It" button. Then navigating to a
  // different page with the same tab to see that the promo is still showing due
  // to the status of the notice not being updated yet.
  onboarding_service()->OnboardingNoticeActionTaken(
      privacy_sandbox::TrackingProtectionOnboarding::NoticeAction::kGotIt);
  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(), https_server_.GetURL("c.test", "/empty.html"), 1,
      WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.Onboarding.NoticeServiceEvent",
      privacy_sandbox::TrackingProtectionNoticeService::
          TrackingProtectionMetricsNoticeEvent::kNoticeShowingButShouldnt,
      1);
}

class TrackingProtectionSilentOnboardingNoticeBrowserTest
    : public TrackingProtectionOnboardingNoticeBrowserTest {};

// Navigation

// Profile marked eligible, then the user navigates to a new Secure HTTPS tab
// with the lock button.
// Should be shown the notice
IN_PROC_BROWSER_TEST_F(TrackingProtectionSilentOnboardingNoticeBrowserTest,
                       NewTabEligiblePage) {
  // Setup
  onboarding_service()->MaybeMarkSilentEligible();

  browser()->window()->Activate();
  // Action: Navigate to an HTTPS eligible page in current tab.
  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(), https_server_.GetURL("a.test", "/empty.html"), 1,
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Verification
  // Profile is Onboarded
  EXPECT_EQ(onboarding_service()->GetSilentOnboardingStatus(),
            privacy_sandbox::TrackingProtectionOnboarding::
                SilentOnboardingStatus::kOnboarded);
}

// Profile Marked eligible, added navigation to a new eligible background tab
// Current tab is eligible.
// Does not show the notice as the current tab was created before eligibility,
// therefore not tracked, and the new navigation happened in an inactive tab.
IN_PROC_BROWSER_TEST_F(TrackingProtectionSilentOnboardingNoticeBrowserTest,
                       NewBackgroundTabEligiblePage) {
  // Setup
  onboarding_service()->MaybeMarkSilentEligible();

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
  // Profile stays Eligible
  EXPECT_EQ(onboarding_service()->GetSilentOnboardingStatus(),
            privacy_sandbox::TrackingProtectionOnboarding::
                SilentOnboardingStatus::kEligible);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.SilentOnboarding.NoticeServiceEvent",
      privacy_sandbox::TrackingProtectionNoticeService::
          TrackingProtectionMetricsNoticeEvent::kInactiveWebcontentUpdated,
      1);
}

// Profile Marked eligible, added navigation to a new Ineligible Foreground tab
// Does not silently onboard the profile as the page isn't eligible.
IN_PROC_BROWSER_TEST_F(TrackingProtectionSilentOnboardingNoticeBrowserTest,
                       NewTabIneligiblePage) {
  // Setup
  onboarding_service()->MaybeMarkSilentEligible();

  browser()->window()->Activate();
  // Action: Navigate to an HTTP ineligible page in current tab. ( No lock icon)
  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(), embedded_test_server()->GetURL("a.test", "/empty.html"), 1,
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Verification
  // Profile stays Eligible
  EXPECT_EQ(onboarding_service()->GetSilentOnboardingStatus(),
            privacy_sandbox::TrackingProtectionOnboarding::
                SilentOnboardingStatus::kEligible);
}

// Switching between eligible/ineligible tabs silently onboards the profile
// accordingly.
IN_PROC_BROWSER_TEST_F(TrackingProtectionSilentOnboardingNoticeBrowserTest,
                       SwitchesTabs) {
  // Setup
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
  onboarding_service()->MaybeMarkSilentEligible();

  // This selects the second tab (ineligible). Promo shouldn't show, and profile
  // not yet onboarded.
  browser()->tab_strip_model()->SelectNextTab();
  EXPECT_EQ(onboarding_service()->GetSilentOnboardingStatus(),
            privacy_sandbox::TrackingProtectionOnboarding::
                SilentOnboardingStatus::kEligible);

  // Goes back to eligible tab. Promo will show, and profile is onboarded.
  browser()->tab_strip_model()->SelectPreviousTab();
  EXPECT_EQ(onboarding_service()->GetSilentOnboardingStatus(),
            privacy_sandbox::TrackingProtectionOnboarding::
                SilentOnboardingStatus::kOnboarded);

  // Goes to the ineligible tab again. Profile remains onboarded.
  browser()->tab_strip_model()->SelectNextTab();
  EXPECT_EQ(onboarding_service()->GetSilentOnboardingStatus(),
            privacy_sandbox::TrackingProtectionOnboarding::
                SilentOnboardingStatus::kOnboarded);
}

// Popup to eligible page does not show the notice.
IN_PROC_BROWSER_TEST_F(TrackingProtectionSilentOnboardingNoticeBrowserTest,
                       NewPopupEligiblePage) {
  // Setup
  onboarding_service()->MaybeMarkSilentEligible();

  browser()->window()->Activate();
  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(), https_server_.GetURL("a.test", "/empty.html"), 1,
      WindowOpenDisposition::NEW_POPUP,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Verification
  // Profile is not onboarded - remains eligible.
  EXPECT_EQ(onboarding_service()->GetSilentOnboardingStatus(),
            privacy_sandbox::TrackingProtectionOnboarding::
                SilentOnboardingStatus::kEligible);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.SilentOnboarding.NoticeServiceEvent",
      privacy_sandbox::TrackingProtectionNoticeService::
          TrackingProtectionMetricsNoticeEvent::kBrowserTypeNonNormal,
      1);
}

// Observation

// Profile is ineligible. Notice Service is not observing tab changes.
IN_PROC_BROWSER_TEST_F(TrackingProtectionSilentOnboardingNoticeBrowserTest,
                       DoesntStartObserving) {
  EXPECT_FALSE(TabStripModelObserver::IsObservingAny(notice_service()));
  EXPECT_FALSE(privacy_sandbox::TrackingProtectionNoticeService::TabHelper::
                   IsHelperNeeded(browser()->profile()));
}

// Profile is eligible. Notice service is observing tab changes.
IN_PROC_BROWSER_TEST_F(TrackingProtectionSilentOnboardingNoticeBrowserTest,
                       StartsObserving) {
  // Action
  onboarding_service()->MaybeMarkSilentEligible();
  browser()->window()->Activate();
  // Verification
  EXPECT_TRUE(TabStripModelObserver::IsObservingAny(notice_service()));
  EXPECT_TRUE(privacy_sandbox::TrackingProtectionNoticeService::TabHelper::
                  IsHelperNeeded(browser()->profile()));
}

// Notice is shown. Notice Service stops observing tab changes.
IN_PROC_BROWSER_TEST_F(TrackingProtectionSilentOnboardingNoticeBrowserTest,
                       StopsObserving) {
  // Action
  onboarding_service()->MaybeMarkSilentEligible();
  // Navigates to eligible page.
  browser()->window()->Activate();
  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(), https_server_.GetURL("a.test", "/empty.html"), 1,
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Verification - Observation stops
  EXPECT_FALSE(TabStripModelObserver::IsObservingAny(notice_service()));
  EXPECT_FALSE(privacy_sandbox::TrackingProtectionNoticeService::TabHelper::
                   IsHelperNeeded(browser()->profile()));

  // Once the notice object is created, the tab strip tracker is initialized but
  // in this test we press the promo button, which also causes the
  // tracker to be reset again.
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.NoticeService."
      "IsObservingTabStripModel",
      true, 1);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.NoticeService."
      "IsObservingTabStripModel",
      false, 1);
}

// Profile is onboarded. Notice Service is not observing tab changes.
IN_PROC_BROWSER_TEST_F(TrackingProtectionSilentOnboardingNoticeBrowserTest,
                       OnboardedProfileDoesntStartObserving) {
  onboarding_service()->MaybeMarkSilentEligible();
  // Telling the OnboardingService that the notice has been shown so it marks
  // the profile as Onboarded.
  onboarding_service()->SilentOnboardingNoticeShown();

  EXPECT_FALSE(TabStripModelObserver::IsObservingAny(notice_service()));
  EXPECT_FALSE(privacy_sandbox::TrackingProtectionNoticeService::TabHelper::
                   IsHelperNeeded(browser()->profile()));
}

}  // namespace privacy_sandbox
