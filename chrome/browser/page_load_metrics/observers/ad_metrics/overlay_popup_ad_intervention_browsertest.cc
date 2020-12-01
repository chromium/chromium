// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/subresource_filter/chrome_subresource_filter_client.h"
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "components/subresource_filter/core/common/common_features.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace {

const char kAdsInterventionRecordedHistogram[] =
    "SubresourceFilter.PageLoad.AdsInterventionTriggered";

}  // namespace

class OverlayPopupAdViolationBrowserTest
    : public subresource_filter::SubresourceFilterBrowserTest {
 public:
  OverlayPopupAdViolationBrowserTest() = default;

  void SetUp() override {
    std::vector<base::Feature> enabled = {
        subresource_filter::kAdTagging,
        subresource_filter::kAdsInterventionsEnforced,
        features::kSitePerProcess};
    std::vector<base::Feature> disabled = {
        blink::features::kFrequencyCappingForOverlayPopupDetection};

    feature_list_.InitWithFeatures(enabled, disabled);
    subresource_filter::SubresourceFilterBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    SubresourceFilterBrowserTest::SetUpOnMainThread();
    SetRulesetWithRules(
        {subresource_filter::testing::CreateSuffixRule("ad_iframe_writer.js")});
  }

  // Navigate to |url| and wait until we see the first meaningful paint. FMP is
  // a prerequisite for starting the overlay popup ad detection.
  void NavigateAndWaitForFirstMeaningfulPaint(const GURL& url) {
    content::WebContents* web_contents =
        chrome_test_utils::GetActiveWebContents(this);
    auto waiter =
        std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
            web_contents);
    waiter->AddPageExpectation(page_load_metrics::PageLoadMetricsTestWaiter::
                                   TimingField::kFirstMeaningfulPaint);

    EXPECT_TRUE(content::NavigateToURL(web_contents, url));
    waiter->Wait();
    waiter.reset();
  }

  // Create an overlay popup ad and trigger a series of actions and layout
  // updates for the ad to be detected by the overlay popup detector.
  void TriggerAndDetectOverlayPopupAd() {
    content::WebContents* web_contents =
        chrome_test_utils::GetActiveWebContents(this);

    // Force a layout update to capture the initial state without the ad. Then
    // create the overlay-popup-ad.
    ASSERT_TRUE(EvalJsAfterLifecycleUpdate(
                    web_contents, "",
                    "createAdIframeAtRect(window.innerWidth * "
                    "0.25, window.innerHeight * 0.25, window.innerWidth * 0.5, "
                    "window.innerHeight * 0.5)",
                    content::EXECUTE_SCRIPT_NO_USER_GESTURE)
                    .error.empty());

    // Force a layout update to capture the overlay-popup-ad. Then dismiss the
    // ad.
    ASSERT_TRUE(
        EvalJsAfterLifecycleUpdate(web_contents, "",
                                   "document.getElementsByTagName('iframe')[0]."
                                   "style.display = 'none';",
                                   content::EXECUTE_SCRIPT_NO_USER_GESTURE)
            .error.empty());

    // Force a layout update to capture the state after the dismissal. At this
    // point the detector should have detected the overlay-popup-ad.
    ASSERT_TRUE(
        EvalJsAfterLifecycleUpdate(web_contents, "", "",
                                   content::EXECUTE_SCRIPT_NO_USER_GESTURE)
            .error.empty());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(OverlayPopupAdViolationBrowserTest,
                       NoOverlayPopupAd_AdInterventionNotTriggered) {
  base::HistogramTester histogram_tester;

  GURL url = embedded_test_server()->GetURL(
      "a.com", "/ads_observer/large_scrollable_page_with_adiframe_writer.html");

  NavigateAndWaitForFirstMeaningfulPaint(url);

  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);

  // Reload the page. Since we haven't seen any ad violations, expect that the
  // ad script is loaded and that the subresource filter UI doesn't show up.
  EXPECT_TRUE(content::NavigateToURL(web_contents, url));

  EXPECT_TRUE(WasParsedScriptElementLoaded(web_contents->GetMainFrame()));
  histogram_tester.ExpectBucketCount(
      "SubresourceFilter.Actions2",
      subresource_filter::SubresourceFilterAction::kUIShown, 0);
  histogram_tester.ExpectBucketCount(
      kAdsInterventionRecordedHistogram,
      subresource_filter::mojom::AdsViolation::kOverlayPopupAd, 0);
}

IN_PROC_BROWSER_TEST_F(OverlayPopupAdViolationBrowserTest,
                       OverlayPopupAd_AdInterventionTriggered) {
  base::HistogramTester histogram_tester;

  GURL url = embedded_test_server()->GetURL(
      "a.com", "/ads_observer/large_scrollable_page_with_adiframe_writer.html");

  NavigateAndWaitForFirstMeaningfulPaint(url);

  TriggerAndDetectOverlayPopupAd();

  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);

  // Reload the page. Since we are enforcing ad blocking on ads violations,
  // expect that the ad script is not loaded and that the subresource filter UI
  // shows up.
  EXPECT_TRUE(content::NavigateToURL(web_contents, url));

  EXPECT_FALSE(WasParsedScriptElementLoaded(web_contents->GetMainFrame()));
  histogram_tester.ExpectBucketCount(
      "SubresourceFilter.Actions2",
      subresource_filter::SubresourceFilterAction::kUIShown, 1);
  histogram_tester.ExpectBucketCount(
      kAdsInterventionRecordedHistogram,
      subresource_filter::mojom::AdsViolation::kOverlayPopupAd, 1);
}

class OverlayPopupAdViolationBrowserTestWithoutEnforcement
    : public OverlayPopupAdViolationBrowserTest {
 public:
  OverlayPopupAdViolationBrowserTestWithoutEnforcement() = default;

  void SetUp() override {
    std::vector<base::Feature> enabled = {subresource_filter::kAdTagging};
    std::vector<base::Feature> disabled = {
        subresource_filter::kAdsInterventionsEnforced,
        blink::features::kFrequencyCappingForOverlayPopupDetection};

    feature_list_.InitWithFeatures(enabled, disabled);
    subresource_filter::SubresourceFilterBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(OverlayPopupAdViolationBrowserTestWithoutEnforcement,
                       OverlayPopupAd_NoAdInterventionTriggered) {
  base::HistogramTester histogram_tester;

  GURL url = embedded_test_server()->GetURL(
      "a.com", "/ads_observer/large_scrollable_page_with_adiframe_writer.html");

  NavigateAndWaitForFirstMeaningfulPaint(url);

  TriggerAndDetectOverlayPopupAd();

  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);

  // Reload the page. Since we are not enforcing ad blocking on ads violations,
  // expect that the ad script is loaded and that the subresource filter UI
  // doesn't show up. Expect a histogram recording as the intervention is
  // running in dry run mode.
  EXPECT_TRUE(content::NavigateToURL(web_contents, url));

  EXPECT_TRUE(WasParsedScriptElementLoaded(web_contents->GetMainFrame()));
  histogram_tester.ExpectBucketCount(
      "SubresourceFilter.Actions2",
      subresource_filter::SubresourceFilterAction::kUIShown, 0);
  histogram_tester.ExpectBucketCount(
      kAdsInterventionRecordedHistogram,
      subresource_filter::mojom::AdsViolation::kOverlayPopupAd, 1);
}
