// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/page_load_metrics/browser/observers/ad_metrics/ad_intervention_browser_test_utils.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "components/subresource_filter/core/common/common_features.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace {

const char kAdsInterventionRecordedHistogram[] =
    "SubresourceFilter.PageLoad.AdsInterventionTriggered";

}  // namespace

class LargeStickyAdViolationBrowserTest
    : public subresource_filter::SubresourceFilterBrowserTest {
 public:
  LargeStickyAdViolationBrowserTest() = default;

  void SetUp() override {
    std::vector<base::test::FeatureRef> enabled = {
        subresource_filter::kAdTagging,
        subresource_filter::kAdsInterventionsEnforced};
    std::vector<base::test::FeatureRef> disabled = {
        blink::features::kFrequencyCappingForLargeStickyAdDetection};

    feature_list_.InitWithFeatures(enabled, disabled);
    subresource_filter::SubresourceFilterBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    SubresourceFilterBrowserTest::SetUpOnMainThread();
    SetRulesetWithRules(
        {subresource_filter::testing::CreateSuffixRule("ad_iframe_writer.js")});
  }

 protected:
  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(LargeStickyAdViolationBrowserTest,
                       NoLargeStickyAd_AdInterventionNotTriggered) {
  base::HistogramTester histogram_tester;

  GURL url = embedded_test_server()->GetURL(
      "a.com", "/ads_observer/large_scrollable_page_with_adiframe_writer.html");

  page_load_metrics::NavigateAndWaitForFirstContentfulPaint(web_contents(),
                                                            url);

  // Reload the page. Since we haven't seen any ad violations, expect that the
  // ad script is loaded and that the subresource filter UI doesn't show up.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));

  EXPECT_TRUE(
      WasParsedScriptElementLoaded(web_contents()->GetPrimaryMainFrame()));
  histogram_tester.ExpectBucketCount(
      "SubresourceFilter.Actions2",
      subresource_filter::SubresourceFilterAction::kUIShown, 0);
  histogram_tester.ExpectBucketCount(
      kAdsInterventionRecordedHistogram,
      subresource_filter::mojom::AdsViolation::kLargeStickyAd, 0);
}

IN_PROC_BROWSER_TEST_F(LargeStickyAdViolationBrowserTest,
                       LargeStickyAd_AdInterventionTriggered) {
  base::HistogramTester histogram_tester;

  GURL url = embedded_test_server()->GetURL(
      "a.com", "/ads_observer/large_scrollable_page_with_adiframe_writer.html");

  page_load_metrics::NavigateAndWaitForFirstContentfulPaint(web_contents(),
                                                            url);

  page_load_metrics::TriggerAndDetectLargeStickyAd(web_contents());

  // Reload the page. Since we are enforcing ad blocking on ads violations,
  // expect that the ad script is not loaded and that the subresource filter UI
  // shows up.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));

  EXPECT_FALSE(
      WasParsedScriptElementLoaded(web_contents()->GetPrimaryMainFrame()));
  histogram_tester.ExpectBucketCount(
      "SubresourceFilter.Actions2",
      subresource_filter::SubresourceFilterAction::kUIShown, 1);
  histogram_tester.ExpectBucketCount(
      kAdsInterventionRecordedHistogram,
      subresource_filter::mojom::AdsViolation::kLargeStickyAd, 1);
}

class LargeStickyAdViolationBrowserTestWithoutEnforcement
    : public LargeStickyAdViolationBrowserTest {
 public:
  LargeStickyAdViolationBrowserTestWithoutEnforcement() = default;

  void SetUp() override {
    std::vector<base::test::FeatureRef> enabled = {
        subresource_filter::kAdTagging};
    std::vector<base::test::FeatureRef> disabled = {
        subresource_filter::kAdsInterventionsEnforced,
        blink::features::kFrequencyCappingForLargeStickyAdDetection};

    feature_list_.InitWithFeatures(enabled, disabled);
    subresource_filter::SubresourceFilterBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(LargeStickyAdViolationBrowserTestWithoutEnforcement,
                       LargeStickyAd_NoAdInterventionTriggered) {
  base::HistogramTester histogram_tester;

  GURL url = embedded_test_server()->GetURL(
      "a.com", "/ads_observer/large_scrollable_page_with_adiframe_writer.html");

  page_load_metrics::NavigateAndWaitForFirstContentfulPaint(web_contents(),
                                                            url);

  page_load_metrics::TriggerAndDetectLargeStickyAd(web_contents());

  // Reload the page. Since we are not enforcing ad blocking on ads violations,
  // expect that the ad script is loaded and that the subresource filter UI
  // doesn't show up. Expect a histogram recording as the intervention is
  // running in dry run mode.
  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));

  EXPECT_TRUE(
      WasParsedScriptElementLoaded(web_contents()->GetPrimaryMainFrame()));
  histogram_tester.ExpectBucketCount(
      "SubresourceFilter.Actions2",
      subresource_filter::SubresourceFilterAction::kUIShown, 0);
  histogram_tester.ExpectBucketCount(
      kAdsInterventionRecordedHistogram,
      subresource_filter::mojom::AdsViolation::kLargeStickyAd, 1);
}
