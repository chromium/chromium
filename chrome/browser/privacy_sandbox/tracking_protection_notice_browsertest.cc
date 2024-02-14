// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_notice_factory.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_onboarding_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tpcd/experiment/tpcd_experiment_features.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/privacy_sandbox/tracking_protection_onboarding.h"
#include "components/user_education/test/feature_promo_test_util.h"
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
using SentimentSurveyGroup =
    privacy_sandbox::TrackingProtectionOnboarding::SentimentSurveyGroup;

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

base::test::FeatureRefAndParams ModeBFeatures() {
  return {features::kCookieDeprecationFacilitatedTesting,
          {{tpcd::experiment::kDisable3PCookiesName, "true"},
           {features::kCookieDeprecationTestingDisableAdsAPIsName, "false"},
           {tpcd::experiment::kForceEligibleForTestingName, "true"}}};
}

base::test::FeatureRefAndParams ModeBPrimeFeatures() {
  return {features::kCookieDeprecationFacilitatedTesting,
          {{tpcd::experiment::kDisable3PCookiesName, "true"},
           {features::kCookieDeprecationTestingDisableAdsAPIsName, "true"},
           {tpcd::experiment::kForceEligibleForTestingName, "true"}}};
}

base::test::FeatureRefAndParams ControlFeatures() {
  return {features::kCookieDeprecationFacilitatedTesting,
          {{tpcd::experiment::kDisable3PCookiesName, "false"},
           {tpcd::experiment::kForceEligibleForTestingName, "true"}}};
}

base::test::FeatureRefAndParams ControlFeaturesWithSilentOnboarding() {
  return {features::kCookieDeprecationFacilitatedTesting,
          {
              {tpcd::experiment::kDisable3PCookiesName, "false"},
              {tpcd::experiment::kForceEligibleForTestingName, "true"},
              {tpcd::experiment::kEnableSilentOnboardingName, "true"},
          }};
}

std::vector<base::test::FeatureRefAndParams> HatsImmediateControlFeatures() {
  return {
      ControlFeatures(),
      {features::kTrackingProtectionSentimentSurvey,
       {{"tracking-protection-immediate-over-delayed-probability", "1"},
        {"tracking-protection-control-immediate-probability", "1.0"},
        {"tracking-protection-control-immediate-trigger-id", "trigger-1"}}}};
}

std::vector<base::test::FeatureRefAndParams>
HatsImmediateControlFeaturesWithSilentOnboarding() {
  return {
      ControlFeaturesWithSilentOnboarding(),
      {features::kTrackingProtectionSentimentSurvey,
       {{"tracking-protection-immediate-over-delayed-probability", "1"},
        {"tracking-protection-control-immediate-probability", "1.0"},
        {"tracking-protection-control-immediate-trigger-id", "trigger-1"}}}};
}

std::vector<base::test::FeatureRefAndParams> HatsDelayedControlFeatures() {
  return {ControlFeatures(),
          {features::kTrackingProtectionSentimentSurvey,
           {{"tracking-protection-immediate-over-delayed-probability", "0"},
            {"tracking-protection-control-delayed-probability", "1.0"},
            {"tracking-protection-control-delayed-trigger-id", "trigger-1"}}}};
}

std::vector<base::test::FeatureRefAndParams> HatsImmediateModeBFeatures() {
  return {
      ModeBFeatures(),
      {features::kTrackingProtectionSentimentSurvey,
       {{"tracking-protection-immediate-over-delayed-probability", "1"},
        {"tracking-protection-treatment-immediate-probability", "1.0"},
        {"tracking-protection-treatment-immediate-trigger-id", "trigger-1"}}}};
}

std::vector<base::test::FeatureRefAndParams> HatsDelayedModeBFeatures() {
  return {
      ModeBFeatures(),
      {features::kTrackingProtectionSentimentSurvey,
       {{"tracking-protection-immediate-over-delayed-probability", "0"},
        {"tracking-protection-treatment-delayed-probability", "1.0"},
        {"tracking-protection-treatment-delayed-trigger-id", "trigger-1"}}}};
}

std::vector<base::test::FeatureRefAndParams> HatsImmediateModeBPrimeFeatures() {
  return {
      ModeBPrimeFeatures(),
      {features::kTrackingProtectionSentimentSurvey,
       {{"tracking-protection-immediate-over-delayed-probability", "1"},
        {"tracking-protection-treatment-immediate-probability", "1.0"},
        {"tracking-protection-treatment-immediate-trigger-id", "trigger-1"}}}};
}

std::vector<base::test::FeatureRefAndParams> HatsDelayedModeBPrimeFeatures() {
  return {
      ModeBPrimeFeatures(),
      {features::kTrackingProtectionSentimentSurvey,
       {{"tracking-protection-immediate-over-delayed-probability", "0"},
        {"tracking-protection-treatment-delayed-probability", "1.0"},
        {"tracking-protection-treatment-delayed-trigger-id", "trigger-1"}}}};
}

void ExpectSurveyGroupHistogramEmitted(
    TrackingProtectionOnboarding::SentimentSurveyGroup group,
    base::HistogramTester* histogram_tester) {
  TrackingProtectionOnboarding::SentimentSurveyGroupMetrics metric_group;
  switch (group) {
    case TrackingProtectionOnboarding::SentimentSurveyGroup::kNotSet:
      return;
    case TrackingProtectionOnboarding::SentimentSurveyGroup::kControlImmediate:
      metric_group = TrackingProtectionOnboarding::SentimentSurveyGroupMetrics::
          kControlImmediate;
      break;
    case TrackingProtectionOnboarding::SentimentSurveyGroup::kControlDelayed:
      metric_group = TrackingProtectionOnboarding::SentimentSurveyGroupMetrics::
          kControlDelayed;
      break;
    case TrackingProtectionOnboarding::SentimentSurveyGroup::
        kTreatmentImmediate:
      metric_group = TrackingProtectionOnboarding::SentimentSurveyGroupMetrics::
          kTreatmentImmediate;
      break;
    case TrackingProtectionOnboarding::SentimentSurveyGroup::kTreatmentDelayed:
      metric_group = TrackingProtectionOnboarding::SentimentSurveyGroupMetrics::
          kTreatmentDelayed;
  }

  histogram_tester->ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.SentimentSurvey."
      "HatsGroupRegisteredAndEligible",
      metric_group, 1);
}

}  // namespace

class TrackingProtectionBaseNoticeBrowserTest : public InProcessBrowserTest {
 protected:
  explicit TrackingProtectionBaseNoticeBrowserTest(
      const std::vector<base::test::FeatureRef>& enabled_features,
      const std::vector<base::test::FeatureRef>& disabled_features = {}) {
    feature_list_.InitAndEnableFeatures(enabled_features, disabled_features);
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

  virtual std::vector<base::test::FeatureRef> EnabledFeatures() = 0;
  virtual std::vector<base::test::FeatureRef> DisabledFeatures() = 0;

  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  base::HistogramTester histogram_tester_;

 private:
  feature_engagement::test::ScopedIphFeatureList feature_list_;
};

class TrackingProtectionOnboardingNoticeBrowserTest
    : public TrackingProtectionBaseNoticeBrowserTest {
 protected:
  TrackingProtectionOnboardingNoticeBrowserTest()
      : TrackingProtectionBaseNoticeBrowserTest(EnabledFeatures(),
                                                DisabledFeatures()) {}

  std::vector<base::test::FeatureRef> EnabledFeatures() override {
    return {feature_engagement::kIPHTrackingProtectionOnboardingFeature,
            feature_engagement::kIPHTrackingProtectionOffboardingFeature};
  }

  std::vector<base::test::FeatureRef> DisabledFeatures() override {
    return {privacy_sandbox::kTrackingProtectionOnboardingRollback,
            features::kCookieDeprecationFacilitatedTesting};
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
  EXPECT_TRUE(IsOnboardingPromoActive(browser()));
}

// Profile marked eligible, the user navigates to a new Secure HTTPS tab
// with the lock button. Is shown the notice, navigates to another eligible
// page.
// Notice should remain on the page.
IN_PROC_BROWSER_TEST_F(TrackingProtectionOnboardingNoticeBrowserTest,
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
  EXPECT_TRUE(IsOnboardingPromoActive(browser()));
}

// User is shown the notice, but was marked as Acked somehow.
// Hide the notice
IN_PROC_BROWSER_TEST_F(TrackingProtectionOnboardingNoticeBrowserTest,
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
  EXPECT_FALSE(IsOnboardingPromoActive(browser()));
}

// Switching between eligible/ineligible tabs shows/hides the notice
// accordingly.
IN_PROC_BROWSER_TEST_F(TrackingProtectionOnboardingNoticeBrowserTest,
                       SwitchesTabs) {
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
  auto lock = BrowserFeaturePromoController::BlockActiveWindowCheckForTesting();
  WaitForFeatureEngagement(browser());
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
  auto lock = BrowserFeaturePromoController::BlockActiveWindowCheckForTesting();
  WaitForFeatureEngagement(browser());
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
  auto lock = BrowserFeaturePromoController::BlockActiveWindowCheckForTesting();
  WaitForFeatureEngagement(browser());

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
  auto lock = BrowserFeaturePromoController::BlockActiveWindowCheckForTesting();
  WaitForFeatureEngagement(browser());

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

class TrackingProtectionOffboardingNoticeBrowserTest
    : public TrackingProtectionBaseNoticeBrowserTest {
 protected:
  TrackingProtectionOffboardingNoticeBrowserTest()
      : TrackingProtectionBaseNoticeBrowserTest(EnabledFeatures(),
                                                DisabledFeatures()) {}

  std::vector<base::test::FeatureRef> EnabledFeatures() override {
    return {privacy_sandbox::kTrackingProtectionOnboardingRollback,
            feature_engagement::kIPHTrackingProtectionOnboardingFeature,
            feature_engagement::kIPHTrackingProtectionOffboardingFeature};
  }

  std::vector<base::test::FeatureRef> DisabledFeatures() override {
    // This feature is irrelevant for these tests, disabling to avoid
    // interplaying with the tests.
    return {features::kCookieDeprecationFacilitatedTesting};
  }

  bool IsOnboardingPromoActive(Browser* browser) {
    return GetFeaturePromoController(browser)->IsPromoActive(
        feature_engagement::kIPHTrackingProtectionOnboardingFeature);
  }

  bool IsOffboardingPromoActive(Browser* browser) {
    return GetFeaturePromoController(browser)->IsPromoActive(
        feature_engagement::kIPHTrackingProtectionOffboardingFeature);
  }
};

// Profile marked Ineligible.
// The user navigates to a new Secure HTTPS tab with the lock button.
// Offboarding notice should not be show.
IN_PROC_BROWSER_TEST_F(TrackingProtectionOffboardingNoticeBrowserTest,
                       IneligibleProfile) {
  // Setup
  auto lock = BrowserFeaturePromoController::BlockActiveWindowCheckForTesting();
  WaitForFeatureEngagement(browser());

  browser()->window()->Activate();
  // Action: Navigate to an HTTPS eligible page in current tab.
  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(), https_server_.GetURL("a.test", "/empty.html"), 1,
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Verification
  // Profile stays Ineligible
  EXPECT_EQ(onboarding_service()->GetOnboardingStatus(),
            privacy_sandbox::TrackingProtectionOnboarding::OnboardingStatus::
                kIneligible);
  // Notice is not showing.
  EXPECT_FALSE(IsOffboardingPromoActive(browser()));
}

// Profile marked Eligible, then browser restarted.
// The user navigates to a new Secure HTTPS tab with the lock button.
// Offboarding notice should not be show.
IN_PROC_BROWSER_TEST_F(TrackingProtectionOffboardingNoticeBrowserTest,
                       PRE_EligibleProfile) {
  onboarding_service()->MaybeMarkEligible();
}

IN_PROC_BROWSER_TEST_F(TrackingProtectionOffboardingNoticeBrowserTest,
                       EligibleProfile) {
  // Setup
  auto lock = BrowserFeaturePromoController::BlockActiveWindowCheckForTesting();
  WaitForFeatureEngagement(browser());

  browser()->window()->Activate();
  // Action: Navigate to an HTTPS eligible page in current tab.
  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(), https_server_.GetURL("a.test", "/empty.html"), 1,
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Verification
  // Profile stays eligible
  EXPECT_EQ(onboarding_service()->GetOnboardingStatus(),
            privacy_sandbox::TrackingProtectionOnboarding::OnboardingStatus::
                kEligible);
  // Notice is not showing.
  EXPECT_FALSE(IsOffboardingPromoActive(browser()));
}

// Profile marked Onboarded, then browser restarted.
// The user navigates to a new Secure HTTPS tab with the lock button.
// Offboarding notice should show.
IN_PROC_BROWSER_TEST_F(TrackingProtectionOffboardingNoticeBrowserTest,
                       PRE_OnboardedProfile) {
  onboarding_service()->MaybeMarkEligible();
  onboarding_service()->NoticeShown(NoticeType::kOnboarding);
}

IN_PROC_BROWSER_TEST_F(TrackingProtectionOffboardingNoticeBrowserTest,
                       OnboardedProfile) {
  // Setup
  auto lock = BrowserFeaturePromoController::BlockActiveWindowCheckForTesting();
  WaitForFeatureEngagement(browser());

  browser()->window()->Activate();
  // Action: Navigate to an HTTPS eligible page in current tab.
  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(), https_server_.GetURL("a.test", "/empty.html"), 1,
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Verification
  // Profile is offboarded
  EXPECT_EQ(onboarding_service()->GetOnboardingStatus(),
            privacy_sandbox::TrackingProtectionOnboarding::OnboardingStatus::
                kOffboarded);
  // Notice is showing.
  EXPECT_TRUE(IsOffboardingPromoActive(browser()));
}

// Profile marked Offboarded, then browser restarted.
// The user navigates to a new Secure HTTPS tab with the lock button.
// Offboarding notice should not show.
IN_PROC_BROWSER_TEST_F(TrackingProtectionOffboardingNoticeBrowserTest,
                       PRE_OffboardedProfile) {
  onboarding_service()->NoticeShown(NoticeType::kOffboarding);
}

IN_PROC_BROWSER_TEST_F(TrackingProtectionOffboardingNoticeBrowserTest,
                       OffboardedProfile) {
  // Setup
  auto lock = BrowserFeaturePromoController::BlockActiveWindowCheckForTesting();
  WaitForFeatureEngagement(browser());

  browser()->window()->Activate();
  // Action: Navigate to an HTTPS eligible page in current tab.
  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(), https_server_.GetURL("a.test", "/empty.html"), 1,
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Verification
  // Profile is offboarded
  EXPECT_EQ(onboarding_service()->GetOnboardingStatus(),
            privacy_sandbox::TrackingProtectionOnboarding::OnboardingStatus::
                kOffboarded);
  // Notice is not showing.
  EXPECT_FALSE(IsOffboardingPromoActive(browser()));
}

// Once hidden, the offboarding notice doesn't reappear.
IN_PROC_BROWSER_TEST_F(TrackingProtectionOffboardingNoticeBrowserTest,
                       PRE_NoticeDoesntReshow) {
  onboarding_service()->MaybeMarkEligible();
  onboarding_service()->NoticeShown(NoticeType::kOnboarding);
}

IN_PROC_BROWSER_TEST_F(TrackingProtectionOffboardingNoticeBrowserTest,
                       NoticeDoesntReshow) {
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

  // Verification
  // Profile is offboarded
  EXPECT_EQ(onboarding_service()->GetOnboardingStatus(),
            privacy_sandbox::TrackingProtectionOnboarding::OnboardingStatus::
                kOffboarded);

  // Notice is showing.
  EXPECT_TRUE(IsOffboardingPromoActive(browser()));

  // This selects the second tab (ineligible). Promo shouldn't show.
  browser()->tab_strip_model()->SelectNextTab();
  EXPECT_FALSE(IsOffboardingPromoActive(browser()));
  EXPECT_EQ(onboarding_service()->GetOnboardingStatus(),
            privacy_sandbox::TrackingProtectionOnboarding::OnboardingStatus::
                kOffboarded);

  // Goes back to eligible tab. Promo will not show.
  browser()->tab_strip_model()->SelectPreviousTab();
  EXPECT_FALSE(IsOffboardingPromoActive(browser()));
}

IN_PROC_BROWSER_TEST_F(TrackingProtectionOnboardingNoticeBrowserTest,
                       NoticeServiceEventHistogramCheck) {
  // Setup
  auto lock = BrowserFeaturePromoController::BlockActiveWindowCheckForTesting();
  WaitForFeatureEngagement(browser());

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
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
IN_PROC_BROWSER_TEST_F(TrackingProtectionOffboardingNoticeBrowserTest,
                       PRE_NoticeServiceEventHistogramCheck) {
  onboarding_service()->MaybeMarkEligible();
  onboarding_service()->NoticeShown(NoticeType::kOnboarding);
}

IN_PROC_BROWSER_TEST_F(TrackingProtectionOffboardingNoticeBrowserTest,
                       NoticeServiceEventHistogramCheck) {
  // Setup
  auto lock = BrowserFeaturePromoController::BlockActiveWindowCheckForTesting();

  WaitForFeatureEngagement(browser());
  browser()->window()->Activate();
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.Offboarding.NoticeServiceEvent",
      privacy_sandbox::TrackingProtectionNoticeService::
          TrackingProtectionMetricsNoticeEvent::kNoticeObjectCreated,
      1);

  // On the first load the OnTabStripModelChanged is invoked causing the
  // MaybeUpdateNoticeVisibility function to be called. Then once the navigation
  // goes through which also calls the MaybeUpdateNoticeVisibility which is why
  // there are two histograms emitted for kUpdateNoticeVisibility.
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.Offboarding.NoticeServiceEvent",
      privacy_sandbox::TrackingProtectionNoticeService::
          TrackingProtectionMetricsNoticeEvent::kUpdateNoticeVisibility,
      2);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.Offboarding.NoticeServiceEvent",
      privacy_sandbox::TrackingProtectionNoticeService::
          TrackingProtectionMetricsNoticeEvent::kActiveTabChanged,
      1);

  // Action: Navigate to an HTTPS eligible page in current tab.
  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(), https_server_.GetURL("a.test", "/empty.html"), 1,
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Verification
  // Profile is offboarded
  EXPECT_EQ(onboarding_service()->GetOnboardingStatus(),
            privacy_sandbox::TrackingProtectionOnboarding::OnboardingStatus::
                kOffboarded);
  // Notice is showing.
  EXPECT_TRUE(IsOffboardingPromoActive(browser()));

  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.Offboarding.NoticeServiceEvent",
      privacy_sandbox::TrackingProtectionNoticeService::
          TrackingProtectionMetricsNoticeEvent::kNoticeRequestedAndShown,
      1);

  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(), https_server_.GetURL("b.test", "/empty.html"), 1,
      WindowOpenDisposition::NEW_POPUP,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // This selects the second tab (ineligible). Promo shouldn't show.
  browser()->tab_strip_model()->SelectNextTab();

  EXPECT_FALSE(IsOffboardingPromoActive(browser()));
  EXPECT_EQ(onboarding_service()->GetOnboardingStatus(),
            privacy_sandbox::TrackingProtectionOnboarding::OnboardingStatus::
                kOffboarded);

  // Goes back to eligible tab. Promo will not show.
  browser()->tab_strip_model()->SelectPreviousTab();

  EXPECT_FALSE(IsOffboardingPromoActive(browser()));

  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.Offboarding.NoticeServiceEvent",
      privacy_sandbox::TrackingProtectionNoticeService::
          TrackingProtectionMetricsNoticeEvent::kNavigationFinished,
      2);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.Offboarding.NoticeServiceEvent",
      privacy_sandbox::TrackingProtectionNoticeService::
          TrackingProtectionMetricsNoticeEvent::kNoticeShowingButShouldnt,
      1);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

IN_PROC_BROWSER_TEST_F(TrackingProtectionOffboardingNoticeBrowserTest,
                       PRE_NoticeServiceEventHistogramCheckNonNormal) {
  onboarding_service()->MaybeMarkEligible();
  onboarding_service()->NoticeShown(NoticeType::kOnboarding);
}

IN_PROC_BROWSER_TEST_F(TrackingProtectionOffboardingNoticeBrowserTest,
                       NoticeServiceEventHistogramCheckNonNormal) {
  auto lock = BrowserFeaturePromoController::BlockActiveWindowCheckForTesting();
  WaitForFeatureEngagement(browser());

  browser()->window()->Activate();
  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(), https_server_.GetURL("a.test", "/empty.html"), 1,
      WindowOpenDisposition::NEW_POPUP,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.Offboarding.NoticeServiceEvent",
      privacy_sandbox::TrackingProtectionNoticeService::
          TrackingProtectionMetricsNoticeEvent::kBrowserTypeNonNormal,
      1);
}

IN_PROC_BROWSER_TEST_F(TrackingProtectionOffboardingNoticeBrowserTest,
                       PRE_IsObserving) {
  onboarding_service()->MaybeMarkEligible();
  onboarding_service()->NoticeShown(NoticeType::kOnboarding);
}

IN_PROC_BROWSER_TEST_F(TrackingProtectionOffboardingNoticeBrowserTest,
                       IsObserving) {
  auto lock = BrowserFeaturePromoController::BlockActiveWindowCheckForTesting();
  WaitForFeatureEngagement(browser());

  browser()->window()->Activate();

  // Once the notice object is created, the tab strip tracker is initialized
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.NoticeService."
      "IsObservingTabStripModel",
      true, 1);
}

IN_PROC_BROWSER_TEST_F(TrackingProtectionOffboardingNoticeBrowserTest,
                       PRE_StopsObserving) {
  onboarding_service()->MaybeMarkEligible();
  onboarding_service()->NoticeShown(NoticeType::kOnboarding);
}

IN_PROC_BROWSER_TEST_F(TrackingProtectionOffboardingNoticeBrowserTest,
                       StopsObserving) {
  // Setup
  auto lock = BrowserFeaturePromoController::BlockActiveWindowCheckForTesting();
  WaitForFeatureEngagement(browser());

  // Navigates to eligible page.
  browser()->window()->Activate();
  ui_test_utils::NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser(), https_server_.GetURL("a.test", "/empty.html"), 1,
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  EXPECT_TRUE(TabStripModelObserver::IsObservingAny(notice_service()));
  PressPromoButton(browser());
  // Verification - Observation stops
  EXPECT_FALSE(TabStripModelObserver::IsObservingAny(notice_service()));
  EXPECT_FALSE(privacy_sandbox::TrackingProtectionNoticeService::TabHelper::
                   IsHelperNeeded(browser()->profile()));
}

struct TrackingProtectionSurveyTestData {
  // Inputs
  std::vector<base::test::FeatureRefAndParams> features;
  bool has_cookie_controls_3pc_blocked = false;
  bool has_tracking_protection_3pc_blocked = false;
  bool has_topics_enabled = false;
  bool has_fledge_enabled = false;
  bool has_measurement_enabled = false;
  std::optional<bool> should_silently_onboard = std::nullopt;
  std::optional<NoticeAction> ack_action = std::nullopt;
  base::TimeDelta to_start_survey;
  base::TimeDelta after_end_of_survey;
  // Expectations
  std::string trigger_id;
  SentimentSurveyGroup group;
  bool is_b_prime = false;
};

class TrackingProtectionHatsBaseTest : public InProcessBrowserTest {
 protected:
  explicit TrackingProtectionHatsBaseTest(
      const std::vector<base::test::FeatureRefAndParams>&
          allow_and_enable_features) {
    feature_list_.InitWithFeaturesAndParameters(
        allow_and_enable_features,
        {content_settings::features::kTrackingProtection3pcd});
  }

  void SetUpOnMainThread() override {
    mock_hats_service_ = static_cast<MockHatsService*>(
        HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            browser()->profile(), base::BindRepeating(&BuildMockHatsService)));
    EXPECT_CALL(*mock_hats_service_, CanShowAnySurvey(testing::_))
        .WillRepeatedly(testing::Return(true));

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

  virtual std::vector<base::test::FeatureRefAndParams>
  EnabledFeaturesWithParams() = 0;

  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  raw_ptr<MockHatsService, DanglingUntriaged> mock_hats_service_;
  base::test::ScopedFeatureList feature_list_;
};

class TrackingProtectionHatsBrowserTest
    : public TrackingProtectionHatsBaseTest,
      public testing::WithParamInterface<TrackingProtectionSurveyTestData> {
 protected:
  TrackingProtectionHatsBrowserTest()
      : TrackingProtectionHatsBaseTest(EnabledFeaturesWithParams()) {}
  base::HistogramTester histogram_tester_;

  std::vector<base::test::FeatureRefAndParams> EnabledFeaturesWithParams()
      override {
    return GetParam().features;
  }
};

base::Time Now() {
  static base::Time now = base::subtle::TimeNowIgnoringOverride();
  return now;
}

IN_PROC_BROWSER_TEST_P(TrackingProtectionHatsBrowserTest,
                       CallHatsServiceWithProductData) {
  TrackingProtectionSurveyTestData params = GetParam();
  // Setup
  if (params.has_cookie_controls_3pc_blocked) {
    browser()->profile()->GetPrefs()->SetInteger(
        prefs::kCookieControlsMode,
        static_cast<int>(
            content_settings::CookieControlsMode::kBlockThirdParty));
  }
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kBlockAll3pcToggleEnabled,
      params.has_tracking_protection_3pc_blocked);
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kPrivacySandboxM1TopicsEnabled, params.has_topics_enabled);
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kPrivacySandboxM1FledgeEnabled, params.has_fledge_enabled);
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kPrivacySandboxM1AdMeasurementEnabled,
      params.has_measurement_enabled);

  if (params.should_silently_onboard) {
    {
      base::subtle::ScopedTimeClockOverrides override([]() { return Now(); },
                                                      nullptr, nullptr);
      onboarding_service()->MaybeMarkSilentEligible();
      onboarding_service()->SilentOnboardingNoticeShown();
    }
  }
  // Ack if necessary.
  if (params.ack_action.has_value()) {
    // Onboarding first
    {
      base::subtle::ScopedTimeClockOverrides override([]() { return Now(); },
                                                      nullptr, nullptr);
      onboarding_service()->OnboardingNoticeShown();
    }
    // Ack after 65 seconds
    {
      base::subtle::ScopedTimeClockOverrides override(
          []() { return Now() + base::Seconds(65); }, nullptr, nullptr);
      onboarding_service()->OnboardingNoticeActionTaken(
          params.ack_action.value());
    }
  }

  // Navigation to first NTP, triggering group registration.
  browser()->window()->Activate();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUINewTabURL)));

  // Verification
  // After the start delay, a survey should be required.
  {
    base::subtle::ScopedTimeClockOverrides override(
        []() { return Now() + GetParam().to_start_survey; }, nullptr, nullptr);
    EXPECT_EQ(onboarding_service()->GetEligibleSurveyGroup(), params.group);

    SurveyBitsData product_bits{
        {"3P cookies blocked", params.has_cookie_controls_3pc_blocked ||
                                   params.has_tracking_protection_3pc_blocked},
        {"Fledge enabled", params.has_fledge_enabled},
        {"Is Mode B'", params.is_b_prime},
        {"Measurement enabled", params.has_measurement_enabled},
        {"Onboarding Settings Clicked",
         params.ack_action == NoticeAction::kSettings},
        {"Topics enabled", params.has_topics_enabled}};
    SurveyStringData product_strings{
        {"Seconds to acknowledge", params.ack_action.has_value()
                                       ? "65"  // 65 seconds to ack above.
                                       : "-1"}};

    EXPECT_CALL(
        *mock_hats_service_,
        LaunchSurvey(params.trigger_id, _, _, product_bits, product_strings));

    // Navigation actually triggering the survey;
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                             GURL(chrome::kChromeUINewTabURL)));

    ExpectSurveyGroupHistogramEmitted(params.group, &histogram_tester_);

    testing::Mock::VerifyAndClearExpectations(mock_hats_service_);
  }

  // After the end date, the survey should no longer trigger.
  {
    base::subtle::ScopedTimeClockOverrides override(
        []() { return Now() + GetParam().after_end_of_survey; }, nullptr,
        nullptr);
    EXPECT_EQ(onboarding_service()->GetEligibleSurveyGroup(),
              SentimentSurveyGroup::kNotSet);

    EXPECT_CALL(*mock_hats_service_, LaunchSurvey).Times(0);

    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                             GURL(chrome::kChromeUINewTabURL)));

    testing::Mock::VerifyAndClearExpectations(mock_hats_service_);
  }
}

INSTANTIATE_TEST_SUITE_P(
    ,
    TrackingProtectionHatsBrowserTest,
    testing::Values(
        // Immediate Control only measuerement enabled.
        TrackingProtectionSurveyTestData{
            .features = HatsImmediateControlFeatures(),
            .has_cookie_controls_3pc_blocked = false,
            .has_topics_enabled = false,
            .has_fledge_enabled = false,
            .has_measurement_enabled = true,
            .to_start_survey = base::Minutes(5),
            .after_end_of_survey = base::Minutes(65),
            .trigger_id = kHatsSurveyTriggerTrackingProtectionControlImmediate,
            .group = SentimentSurveyGroup::kControlImmediate,
        },
        // Immediate Control Only fledge enabled
        TrackingProtectionSurveyTestData{
            .features = HatsImmediateControlFeatures(),
            .has_cookie_controls_3pc_blocked = false,
            .has_topics_enabled = false,
            .has_fledge_enabled = true,
            .has_measurement_enabled = false,
            .to_start_survey = base::Minutes(5),
            .after_end_of_survey = base::Minutes(65),
            .trigger_id = kHatsSurveyTriggerTrackingProtectionControlImmediate,
            .group = SentimentSurveyGroup::kControlImmediate,
        },
        // Immediate Control Only topics Enabled
        TrackingProtectionSurveyTestData{
            .features = HatsImmediateControlFeatures(),
            .has_cookie_controls_3pc_blocked = false,
            .has_topics_enabled = true,
            .has_fledge_enabled = false,
            .has_measurement_enabled = false,
            .to_start_survey = base::Minutes(5),
            .after_end_of_survey = base::Minutes(65),
            .trigger_id = kHatsSurveyTriggerTrackingProtectionControlImmediate,
            .group = SentimentSurveyGroup::kControlImmediate,
        },
        // Immediate Control No Ads API Enabled
        TrackingProtectionSurveyTestData{
            .features = HatsImmediateControlFeatures(),
            .has_cookie_controls_3pc_blocked = true,
            .has_topics_enabled = false,
            .has_fledge_enabled = false,
            .has_measurement_enabled = false,
            .to_start_survey = base::Minutes(5),
            .after_end_of_survey = base::Minutes(65),
            .trigger_id = kHatsSurveyTriggerTrackingProtectionControlImmediate,
            .group = SentimentSurveyGroup::kControlImmediate,
        },
        // Immediate Control with silent onbaording. No Ads API Enabled
        TrackingProtectionSurveyTestData{
            .features = HatsImmediateControlFeaturesWithSilentOnboarding(),
            .has_cookie_controls_3pc_blocked = true,
            .has_topics_enabled = false,
            .has_fledge_enabled = false,
            .has_measurement_enabled = false,
            .should_silently_onboard = true,
            .to_start_survey = base::Minutes(5),
            .after_end_of_survey = base::Minutes(65),
            .trigger_id = kHatsSurveyTriggerTrackingProtectionControlImmediate,
            .group = SentimentSurveyGroup::kControlImmediate,
        },
        // Delayed Control No Ads API Enabled
        TrackingProtectionSurveyTestData{
            .features = HatsDelayedControlFeatures(),
            .has_cookie_controls_3pc_blocked = true,
            .has_topics_enabled = false,
            .has_fledge_enabled = false,
            .has_measurement_enabled = false,
            .to_start_survey = base::Days(14) + base::Minutes(5),
            .after_end_of_survey = base::Days(16),
            .trigger_id = kHatsSurveyTriggerTrackingProtectionControlDelayed,
            .group = SentimentSurveyGroup::kControlDelayed,
        },
        // Immediate Mode B acked with "Settings" button
        TrackingProtectionSurveyTestData{
            .features = HatsImmediateModeBFeatures(),
            .has_tracking_protection_3pc_blocked = true,
            .has_topics_enabled = false,
            .has_fledge_enabled = false,
            .has_measurement_enabled = false,
            .ack_action = NoticeAction::kSettings,
            .to_start_survey = base::Minutes(5),
            .after_end_of_survey = base::Minutes(65),
            .trigger_id =
                kHatsSurveyTriggerTrackingProtectionTreatmentImmediate,
            .group = SentimentSurveyGroup::kTreatmentImmediate,
        },
        // Immediate Mode B Acked with "Got It" button
        TrackingProtectionSurveyTestData{
            .features = HatsImmediateModeBFeatures(),
            .has_tracking_protection_3pc_blocked = true,
            .has_topics_enabled = false,
            .has_fledge_enabled = false,
            .has_measurement_enabled = false,
            .ack_action = NoticeAction::kGotIt,
            .to_start_survey = base::Minutes(5),
            .after_end_of_survey = base::Minutes(65),
            .trigger_id =
                kHatsSurveyTriggerTrackingProtectionTreatmentImmediate,
            .group = SentimentSurveyGroup::kTreatmentImmediate,
        },
        // Delayed Mode B Acked with "Got It" button
        TrackingProtectionSurveyTestData{
            .features = HatsDelayedModeBFeatures(),
            .has_tracking_protection_3pc_blocked = true,
            .has_topics_enabled = false,
            .has_fledge_enabled = false,
            .has_measurement_enabled = false,
            .ack_action = NoticeAction::kGotIt,
            .to_start_survey = base::Days(14) + base::Minutes(5),
            .after_end_of_survey = base::Days(16),
            .trigger_id = kHatsSurveyTriggerTrackingProtectionTreatmentDelayed,
            .group = SentimentSurveyGroup::kTreatmentDelayed,
        },
        // Immediate Mode B Prime
        TrackingProtectionSurveyTestData{
            .features = HatsImmediateModeBPrimeFeatures(),
            .has_tracking_protection_3pc_blocked = true,
            .has_topics_enabled = false,
            .has_fledge_enabled = false,
            .has_measurement_enabled = false,
            .ack_action = NoticeAction::kGotIt,
            .to_start_survey = base::Minutes(5),
            .after_end_of_survey = base::Minutes(65),
            .trigger_id =
                kHatsSurveyTriggerTrackingProtectionTreatmentImmediate,
            .group = SentimentSurveyGroup::kTreatmentImmediate,
            .is_b_prime = true,
        },
        // Delayed Mode B Prime
        TrackingProtectionSurveyTestData{
            .features = HatsDelayedModeBPrimeFeatures(),
            .has_tracking_protection_3pc_blocked = true,
            .has_topics_enabled = false,
            .has_fledge_enabled = false,
            .has_measurement_enabled = false,
            .ack_action = NoticeAction::kGotIt,
            .to_start_survey = base::Days(14) + base::Minutes(5),
            .after_end_of_survey = base::Days(16),
            .trigger_id = kHatsSurveyTriggerTrackingProtectionTreatmentDelayed,
            .group = SentimentSurveyGroup::kTreatmentDelayed,
            .is_b_prime = true,
        }));

class TrackingProtectionHatsIneligibleClientBrowserTest
    : public TrackingProtectionHatsBaseTest {
 protected:
  TrackingProtectionHatsIneligibleClientBrowserTest()
      : TrackingProtectionHatsBaseTest(EnabledFeaturesWithParams()) {}

  std::vector<base::test::FeatureRefAndParams> EnabledFeaturesWithParams()
      override {
    return {
        {features::kCookieDeprecationFacilitatedTesting,
         {{tpcd::experiment::kDisable3PCookiesName, "false"},
          {tpcd::experiment::kForceEligibleForTestingName, "false"}}},
        {features::kTrackingProtectionSentimentSurvey,
         {{"tracking-protection-immediate-over-delayed-probability", "1"},
          {"tracking-protection-control-immediate-probability", "1.0"},
          {"tracking-protection-control-immediate-trigger-id", "trigger-1"}}}};
  }
};

IN_PROC_BROWSER_TEST_F(TrackingProtectionHatsIneligibleClientBrowserTest,
                       IneligibleClientDoesntSurvey) {
  // Navigation to first NTP, triggering group registration.
  browser()->window()->Activate();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUINewTabURL)));

  // Verification
  // After the start delay, a survey should not be required.
  {
    base::subtle::ScopedTimeClockOverrides override(
        []() { return Now() + base::Minutes(5); }, nullptr, nullptr);
    EXPECT_EQ(onboarding_service()->GetEligibleSurveyGroup(),
              SentimentSurveyGroup::kNotSet);

    // Navigation will not trigger the survey.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                             GURL(chrome::kChromeUINewTabURL)));

    EXPECT_CALL(*mock_hats_service_, LaunchSurvey).Times(0);
    testing::Mock::VerifyAndClearExpectations(mock_hats_service_);
  }
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
  auto lock = BrowserFeaturePromoController::BlockActiveWindowCheckForTesting();
  WaitForFeatureEngagement(browser());
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
  auto lock = BrowserFeaturePromoController::BlockActiveWindowCheckForTesting();
  WaitForFeatureEngagement(browser());
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
  auto lock = BrowserFeaturePromoController::BlockActiveWindowCheckForTesting();
  WaitForFeatureEngagement(browser());
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
  auto lock = BrowserFeaturePromoController::BlockActiveWindowCheckForTesting();
  WaitForFeatureEngagement(browser());
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
  // Setup
  auto lock = BrowserFeaturePromoController::BlockActiveWindowCheckForTesting();
  WaitForFeatureEngagement(browser());
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
