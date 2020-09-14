// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "chrome/browser/subresource_filter/ads_intervention_manager.h"
#include "chrome/browser/subresource_filter/chrome_subresource_filter_client.h"
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace subresource_filter {

namespace {

const char kSubresourceFilterActionsHistogram[] = "SubresourceFilter.Actions2";

}  // namespace

class AdsInterventionManagerTestWithEnforcement
    : public SubresourceFilterBrowserTest {
 public:
  AdsInterventionManagerTestWithEnforcement() {
    feature_list_.InitAndEnableFeature(
        subresource_filter::kAdsInterventionsEnforced);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(AdsInterventionManagerTestWithEnforcement,
                       AdsInterventionEnforced_PageActivated) {
  base::HistogramTester histogram_tester;
  ChromeSubresourceFilterClient* client =
      ChromeSubresourceFilterClient::FromWebContents(web_contents());
  auto test_clock = std::make_unique<base::SimpleTestClock>();
  ads_intervention_manager()->set_clock_for_testing(test_clock.get());

  const GURL url(
      GetTestUrl("subresource_filter/frame_with_included_script.html"));
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));

  // Should not trigger activation as the URL is not on the blocklist and
  // has no active ads interventions.
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_TRUE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));
  histogram_tester.ExpectBucketCount(kSubresourceFilterActionsHistogram,
                                     SubresourceFilterAction::kUIShown, 0);

  // Trigger an ads violation and renavigate the page. Should trigger
  // subresource filter activation.
  client->OnAdsViolationTriggered(
      web_contents()->GetMainFrame(),
      mojom::AdsViolation::kMobileAdDensityByHeightAbove30);
  ui_test_utils::NavigateToURL(browser(), url);

  EXPECT_FALSE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));
  histogram_tester.ExpectBucketCount(kSubresourceFilterActionsHistogram,
                                     SubresourceFilterAction::kUIShown, 1);

  // Advance the clock to clear the intervention.
  test_clock->Advance(subresource_filter::kAdsInterventionDuration.Get());
  ui_test_utils::NavigateToURL(browser(), url);

  EXPECT_TRUE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));
}

IN_PROC_BROWSER_TEST_F(
    AdsInterventionManagerTestWithEnforcement,
    MultipleAdsInterventions_PageActivationClearedAfterFirst) {
  base::HistogramTester histogram_tester;
  ChromeSubresourceFilterClient* client =
      ChromeSubresourceFilterClient::FromWebContents(web_contents());
  auto test_clock = std::make_unique<base::SimpleTestClock>();
  ads_intervention_manager()->set_clock_for_testing(test_clock.get());

  const GURL url(
      GetTestUrl("subresource_filter/frame_with_included_script.html"));
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));

  // Should not trigger activation as the URL is not on the blocklist and
  // has no active ads interventions.
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_TRUE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));
  histogram_tester.ExpectBucketCount(kSubresourceFilterActionsHistogram,
                                     SubresourceFilterAction::kUIShown, 0);

  // Trigger an ads violation and renavigate the page. Should trigger
  // subresource filter activation.
  client->OnAdsViolationTriggered(
      web_contents()->GetMainFrame(),
      mojom::AdsViolation::kMobileAdDensityByHeightAbove30);
  ui_test_utils::NavigateToURL(browser(), url);

  EXPECT_FALSE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));
  histogram_tester.ExpectBucketCount(kSubresourceFilterActionsHistogram,
                                     SubresourceFilterAction::kUIShown, 1);

  // Advance the clock by less than kAdsInterventionDuration and trigger another
  // intervention. This intervention is a no-op.
  test_clock->Advance(subresource_filter::kAdsInterventionDuration.Get() -
                      base::TimeDelta::FromMinutes(30));
  client->OnAdsViolationTriggered(
      web_contents()->GetMainFrame(),
      mojom::AdsViolation::kMobileAdDensityByHeightAbove30);

  // Advance the clock to to kAdsInterventionDuration from the first
  // intervention, this clear the intervention.
  test_clock->Advance(base::TimeDelta::FromMinutes(30));
  ui_test_utils::NavigateToURL(browser(), url);

  EXPECT_TRUE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));
}

class AdsInterventionManagerTestWithoutEnforcement
    : public SubresourceFilterBrowserTest {
 public:
  AdsInterventionManagerTestWithoutEnforcement() {
    feature_list_.InitAndDisableFeature(
        subresource_filter::kAdsInterventionsEnforced);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(AdsInterventionManagerTestWithoutEnforcement,
                       AdsInterventionNotEnforced_NoPageActivation) {
  base::HistogramTester histogram_tester;
  ChromeSubresourceFilterClient* client =
      ChromeSubresourceFilterClient::FromWebContents(web_contents());

  const GURL url(
      GetTestUrl("subresource_filter/frame_with_included_script.html"));
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));

  // Should not trigger activation as the URL is not on the blocklist and
  // has no active ads interventions.
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_TRUE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));
  histogram_tester.ExpectBucketCount(kSubresourceFilterActionsHistogram,
                                     SubresourceFilterAction::kUIShown, 0);

  // Trigger an ads violation and renavigate to the page. Interventions are not
  // enforced so no activation should occur.
  client->OnAdsViolationTriggered(
      web_contents()->GetMainFrame(),
      mojom::AdsViolation::kMobileAdDensityByHeightAbove30);
  ui_test_utils::NavigateToURL(browser(), url);

  EXPECT_TRUE(WasParsedScriptElementLoaded(web_contents()->GetMainFrame()));
  histogram_tester.ExpectBucketCount(kSubresourceFilterActionsHistogram,
                                     SubresourceFilterAction::kUIShown, 0);
}

}  // namespace subresource_filter
