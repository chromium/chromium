// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/subresource_filter/content/browser/ads_intervention_manager.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace subresource_filter {

namespace {

const char kAdsInterventionRecordedHistogram[] =
    "SubresourceFilter.PageLoad.AdsInterventionTriggered";

}  // namespace

class AdsInterventionManagerTestWithEnforcement
    : public SubresourceFilterBrowserTest {
 public:
  AdsInterventionManagerTestWithEnforcement() {
    feature_list_.InitAndEnableFeature(
        subresource_filter::kAdsInterventionsEnforced);
  }

  subresource_filter::ContentSubresourceFilterThrottleManager*
  current_throttle_manager() {
    return subresource_filter::ContentSubresourceFilterThrottleManager::
        FromPage(web_contents()->GetPrimaryPage());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(AdsInterventionManagerTestWithEnforcement,
                       AdsInterventionEnforced_PageActivated) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  auto test_clock = std::make_unique<base::SimpleTestClock>();
  ads_intervention_manager()->set_clock_for_testing(test_clock.get());

  const GURL url(
      GetTestUrl("subresource_filter/frame_with_included_script.html"));
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));

  // Should not trigger activation as the URL is not on the blocklist and
  // has no active ads interventions.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_TRUE(
      WasParsedScriptElementLoaded(web_contents()->GetPrimaryMainFrame()));
  histogram_tester.ExpectBucketCount(
      kSubresourceFilterActionsHistogram,
      subresource_filter::SubresourceFilterAction::kUIShown, 0);
  histogram_tester.ExpectTotalCount(kAdsInterventionRecordedHistogram, 0);
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::AdsIntervention_LastIntervention::kEntryName);
  EXPECT_EQ(0u, entries.size());

  // Trigger an ads violation and renavigate the page. Should trigger
  // subresource filter activation.
  current_throttle_manager()->OnAdsViolationTriggered(
      web_contents()->GetPrimaryMainFrame(),
      mojom::AdsViolation::kMobileAdDensityByHeightAbove30);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  EXPECT_FALSE(
      WasParsedScriptElementLoaded(web_contents()->GetPrimaryMainFrame()));
  histogram_tester.ExpectBucketCount(
      kSubresourceFilterActionsHistogram,
      subresource_filter::SubresourceFilterAction::kUIShown, 1);
  histogram_tester.ExpectBucketCount(
      kAdsInterventionRecordedHistogram,
      static_cast<int>(mojom::AdsViolation::kMobileAdDensityByHeightAbove30),
      1);
  entries = ukm_recorder.GetEntriesByName(
      ukm::builders::AdsIntervention_LastIntervention::kEntryName);
  EXPECT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntryMetric(
      entries.front(),
      ukm::builders::AdsIntervention_LastIntervention::kInterventionTypeName,
      static_cast<int>(mojom::AdsViolation::kMobileAdDensityByHeightAbove30));
  ukm_recorder.ExpectEntryMetric(
      entries.front(),
      ukm::builders::AdsIntervention_LastIntervention::kInterventionStatusName,
      static_cast<int>(AdsInterventionStatus::kBlocking));

  // Advance the clock to clear the intervention.
  test_clock->Advance(subresource_filter::kAdsInterventionDuration.Get());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  EXPECT_TRUE(
      WasParsedScriptElementLoaded(web_contents()->GetPrimaryMainFrame()));
  histogram_tester.ExpectBucketCount(
      kAdsInterventionRecordedHistogram,
      static_cast<int>(mojom::AdsViolation::kMobileAdDensityByHeightAbove30),
      1);
  entries = ukm_recorder.GetEntriesByName(
      ukm::builders::AdsIntervention_LastIntervention::kEntryName);
  EXPECT_EQ(2u, entries.size());

  // One of the entries is kBlocking, verify that the other is kExpired after
  // the intervention is cleared.
  EXPECT_TRUE(
      (*ukm_recorder.GetEntryMetric(
           entries.front(), ukm::builders::AdsIntervention_LastIntervention::
                                kInterventionStatusName) ==
       static_cast<int>(AdsInterventionStatus::kExpired)) ||
      (*ukm_recorder.GetEntryMetric(
           entries.back(), ukm::builders::AdsIntervention_LastIntervention::
                               kInterventionStatusName) ==
       static_cast<int>(AdsInterventionStatus::kExpired)));
}

IN_PROC_BROWSER_TEST_F(
    AdsInterventionManagerTestWithEnforcement,
    MultipleAdsInterventions_PageActivationClearedAfterFirst) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  auto test_clock = std::make_unique<base::SimpleTestClock>();
  ads_intervention_manager()->set_clock_for_testing(test_clock.get());

  const GURL url(
      GetTestUrl("subresource_filter/frame_with_included_script.html"));
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));

  // Should not trigger activation as the URL is not on the blocklist and
  // has no active ads interventions.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_TRUE(
      WasParsedScriptElementLoaded(web_contents()->GetPrimaryMainFrame()));
  histogram_tester.ExpectBucketCount(
      kSubresourceFilterActionsHistogram,
      subresource_filter::SubresourceFilterAction::kUIShown, 0);
  histogram_tester.ExpectTotalCount(kAdsInterventionRecordedHistogram, 0);
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::AdsIntervention_LastIntervention::kEntryName);
  EXPECT_EQ(0u, entries.size());

  // Trigger an ads violation and renavigate the page. Should trigger
  // subresource filter activation.
  current_throttle_manager()->OnAdsViolationTriggered(
      web_contents()->GetPrimaryMainFrame(),
      mojom::AdsViolation::kMobileAdDensityByHeightAbove30);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  EXPECT_FALSE(
      WasParsedScriptElementLoaded(web_contents()->GetPrimaryMainFrame()));
  histogram_tester.ExpectBucketCount(
      kSubresourceFilterActionsHistogram,
      subresource_filter::SubresourceFilterAction::kUIShown, 1);
  histogram_tester.ExpectBucketCount(
      kAdsInterventionRecordedHistogram,
      static_cast<int>(mojom::AdsViolation::kMobileAdDensityByHeightAbove30),
      1);
  entries = ukm_recorder.GetEntriesByName(
      ukm::builders::AdsIntervention_LastIntervention::kEntryName);
  EXPECT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntryMetric(
      entries.front(),
      ukm::builders::AdsIntervention_LastIntervention::kInterventionTypeName,
      static_cast<int>(mojom::AdsViolation::kMobileAdDensityByHeightAbove30));
  ukm_recorder.ExpectEntryMetric(
      entries.front(),
      ukm::builders::AdsIntervention_LastIntervention::kInterventionStatusName,
      static_cast<int>(AdsInterventionStatus::kBlocking));

  // Advance the clock by less than kAdsInterventionDuration and trigger another
  // intervention. This intervention is a no-op.
  test_clock->Advance(subresource_filter::kAdsInterventionDuration.Get() -
                      base::Minutes(30));
  current_throttle_manager()->OnAdsViolationTriggered(
      web_contents()->GetPrimaryMainFrame(),
      mojom::AdsViolation::kMobileAdDensityByHeightAbove30);

  // Advance the clock to to kAdsInterventionDuration from the first
  // intervention, this clear the intervention.
  test_clock->Advance(base::Minutes(30));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  EXPECT_TRUE(
      WasParsedScriptElementLoaded(web_contents()->GetPrimaryMainFrame()));
  histogram_tester.ExpectBucketCount(
      kAdsInterventionRecordedHistogram,
      static_cast<int>(mojom::AdsViolation::kMobileAdDensityByHeightAbove30),
      1);
  entries = ukm_recorder.GetEntriesByName(
      ukm::builders::AdsIntervention_LastIntervention::kEntryName);
  EXPECT_EQ(2u, entries.size());

  // One of the entries is kBlocking, verify that the other is kExpired after
  // the intervention is cleared.
  EXPECT_TRUE(
      (*ukm_recorder.GetEntryMetric(
           entries.front(), ukm::builders::AdsIntervention_LastIntervention::
                                kInterventionStatusName) ==
       static_cast<int>(AdsInterventionStatus::kExpired)) ||
      (*ukm_recorder.GetEntryMetric(
           entries.back(), ukm::builders::AdsIntervention_LastIntervention::
                               kInterventionStatusName) ==
       static_cast<int>(AdsInterventionStatus::kExpired)));
}

class AdsInterventionManagerTestWithoutEnforcement
    : public SubresourceFilterBrowserTest {
 public:
  AdsInterventionManagerTestWithoutEnforcement() {
    feature_list_.InitAndDisableFeature(
        subresource_filter::kAdsInterventionsEnforced);
  }

  subresource_filter::ContentSubresourceFilterThrottleManager*
  current_throttle_manager() {
    return subresource_filter::ContentSubresourceFilterThrottleManager::
        FromPage(web_contents()->GetPrimaryPage());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(AdsInterventionManagerTestWithoutEnforcement,
                       AdsInterventionNotEnforced_NoPageActivation) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  auto test_clock = std::make_unique<base::SimpleTestClock>();
  ads_intervention_manager()->set_clock_for_testing(test_clock.get());

  const GURL url(
      GetTestUrl("subresource_filter/frame_with_included_script.html"));
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));

  // Should not trigger activation as the URL is not on the blocklist and
  // has no active ads interventions.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_TRUE(
      WasParsedScriptElementLoaded(web_contents()->GetPrimaryMainFrame()));
  histogram_tester.ExpectBucketCount(
      kSubresourceFilterActionsHistogram,
      subresource_filter::SubresourceFilterAction::kUIShown, 0);
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::AdsIntervention_LastIntervention::kEntryName);
  EXPECT_EQ(0u, entries.size());

  // Trigger an ads violation and renavigate to the page. Interventions are not
  // enforced so no activation should occur.
  current_throttle_manager()->OnAdsViolationTriggered(
      web_contents()->GetPrimaryMainFrame(),
      mojom::AdsViolation::kMobileAdDensityByHeightAbove30);

  const base::TimeDelta kRenavigationDelay = base::Hours(2);
  test_clock->Advance(kRenavigationDelay);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  EXPECT_TRUE(
      WasParsedScriptElementLoaded(web_contents()->GetPrimaryMainFrame()));
  histogram_tester.ExpectBucketCount(
      kSubresourceFilterActionsHistogram,
      subresource_filter::SubresourceFilterAction::kUIShown, 0);
  histogram_tester.ExpectBucketCount(
      kAdsInterventionRecordedHistogram,
      static_cast<int>(mojom::AdsViolation::kMobileAdDensityByHeightAbove30),
      1);
  entries = ukm_recorder.GetEntriesByName(
      ukm::builders::AdsIntervention_LastIntervention::kEntryName);
  EXPECT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntryMetric(
      entries.front(),
      ukm::builders::AdsIntervention_LastIntervention::kInterventionTypeName,
      static_cast<int>(mojom::AdsViolation::kMobileAdDensityByHeightAbove30));
  ukm_recorder.ExpectEntryMetric(
      entries.front(),
      ukm::builders::AdsIntervention_LastIntervention::kInterventionStatusName,
      static_cast<int>(AdsInterventionStatus::kWouldBlock));
}

}  // namespace subresource_filter
