// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/fingerprinting_protection/fingerprinting_protection_filter_browser_test_harness.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_constants.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_features.h"
#include "components/subresource_filter/core/browser/subresource_filter_features_test_support.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/url_pattern_index/proto/rules.pb.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

// TODO(https://crbug.com/358371545): Add browser tests for Histogram population
// for histograms that relate to time measurements as well as renderer
// functionality once blocking is fully implemented.
namespace fingerprinting_protection_filter {

GURL GetURLWithFragment(const GURL& url, std::string_view fragment) {
  GURL::Replacements replacements;
  replacements.SetRefStr(fragment);
  return url.ReplaceComponents(replacements);
}

// =================================== Tests ==================================
//
// Note: Similar to the FPF component, these tests leverage Subresource Filter
// helpers for testing purposes and sample test data files.

IN_PROC_BROWSER_TEST_F(FingerprintingProtectionFilterBrowserTest,
                       SubframeDocumentLoadFiltering) {
  // TODO(https://crbug.com/358371545): Test console messaging for subframe
  // blocking once its implementation is resolved.
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  GURL url(GetTestUrl(kTestFrameSetPath));

  // Disallow loading child frame documents that in turn would end up
  // loading included_script.js, unless the document is loaded from an allowed
  // (not in the blocklist) domain. This enables the third part of this test
  // disallowing a load only after the first redirect.
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  const std::vector<const char*> kSubframeNames{"one", "two", "three"};
  const std::vector<bool> kExpectOnlySecondSubframe{false, true, false};
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectOnlySecondSubframe));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectOnlySecondSubframe);

  // Now navigate the first subframe to an allowed URL and ensure that the load
  // successfully commits and the frame gets restored (no longer collapsed).
  GURL allowed_subdocument_url(
      GetTestUrl("subresource_filter/frame_with_allowed_script.html"));
  NavigateFrame(kSubframeNames[0], allowed_subdocument_url);

  const std::vector<bool> kExpectFirstAndSecondSubframe{true, true, false};
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectFirstAndSecondSubframe));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectFirstAndSecondSubframe);

  // Navigate the first subframe to a document that does not load the probe JS.
  GURL allowed_empty_subdocument_url(
      GetTestUrl("subresource_filter/frame_with_no_subresources.html"));
  NavigateFrame(kSubframeNames[0], allowed_empty_subdocument_url);

  // Finally, navigate the first subframe to an allowed URL that redirects to a
  // disallowed URL, and verify that the navigation gets blocked and the frame
  // collapsed.
  const char kAllowedDomain[] = "allowed.com";
  GURL disallowed_subdocument_url(
      GetTestUrl("subresource_filter/frame_with_included_script.html"));
  GURL redirect_to_disallowed_subdocument_url(embedded_test_server()->GetURL(
      kAllowedDomain, "/server-redirect?" + disallowed_subdocument_url.spec()));
  NavigateFrame(kSubframeNames[0], redirect_to_disallowed_subdocument_url);

  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectOnlySecondSubframe));

  content::RenderFrameHost* frame = FindFrameByName(kSubframeNames[0]);
  const auto last_committed_url = frame->GetLastCommittedURL();

  ASSERT_TRUE(frame);
  AssertUrlContained(last_committed_url,
                     redirect_to_disallowed_subdocument_url);
  AssertUrlContained(last_committed_url, disallowed_subdocument_url);

  ExpectFramesIncludedInLayout(kSubframeNames, kExpectOnlySecondSubframe);

  // Check test UKM recorder contains event with expected metrics.
  const auto& entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::FingerprintingProtection::kEntryName);
  // 1 entry for every frame_with_included_script.html (2 from initial load, 1
  // from redirect)
  EXPECT_EQ(3u, entries.size());
  for (const ukm::mojom::UkmEntry* entry : entries) {
    test_ukm_recorder.ExpectEntryMetric(
        entry, ukm::builders::FingerprintingProtection::kActivationDecisionName,
        static_cast<int64_t>(
            subresource_filter::ActivationDecision::ACTIVATED));
    EXPECT_FALSE(test_ukm_recorder.EntryHasMetric(
        entry, ukm::builders::FingerprintingProtection::kDryRunName));
  }

  histogram_tester.ExpectBucketCount(
      ActivationDecisionHistogramName,
      subresource_filter::ActivationDecision::ACTIVATED, 1);
  histogram_tester.ExpectBucketCount(
      ActivationLevelHistogramName,
      subresource_filter::mojom::ActivationLevel::kEnabled, 1);
  histogram_tester.ExpectTotalCount(kSubresourceLoadsTotalForPage, 1);
  histogram_tester.ExpectTotalCount(kSubresourceLoadsEvaluatedForPage, 1);
  histogram_tester.ExpectTotalCount(kSubresourceLoadsMatchedRulesForPage, 1);
  histogram_tester.ExpectTotalCount(kSubresourceLoadsDisallowedForPage, 1);
}

IN_PROC_BROWSER_TEST_F(
    FingerprintingProtectionFilterEnabledInIncognitoBrowserTest,
    SubframeDocumentLoadFiltering) {
  // TODO(https://crbug.com/358371545): Test console messaging for subframe
  // blocking once its implementation is resolved.
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  GURL url(GetTestUrl(kTestFrameSetPath));

  // Disallow loading child frame documents that in turn would end up
  // loading included_script.js, unless the document is loaded from an allowed
  // (not in the blocklist) domain. This enables the third part of this test
  // disallowing a load only after the first redirect.
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  const std::vector<const char*> kSubframeNames{"one", "two", "three"};
  const std::vector<bool> kExpectOnlySecondSubframe{false, true, false};
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectOnlySecondSubframe));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectOnlySecondSubframe);

  // Now navigate the first subframe to an allowed URL and ensure that the load
  // successfully commits and the frame gets restored (no longer collapsed).
  GURL allowed_subdocument_url(
      GetTestUrl("subresource_filter/frame_with_allowed_script.html"));
  NavigateFrame(kSubframeNames[0], allowed_subdocument_url);

  const std::vector<bool> kExpectFirstAndSecondSubframe{true, true, false};
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectFirstAndSecondSubframe));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectFirstAndSecondSubframe);

  // Navigate the first subframe to a document that does not load the probe JS.
  GURL allowed_empty_subdocument_url(
      GetTestUrl("subresource_filter/frame_with_no_subresources.html"));
  NavigateFrame(kSubframeNames[0], allowed_empty_subdocument_url);

  // Finally, navigate the first subframe to an allowed URL that redirects to a
  // disallowed URL, and verify that the navigation gets blocked and the frame
  // collapsed.
  const char kAllowedDomain[] = "allowed.com";
  GURL disallowed_subdocument_url(
      GetTestUrl("subresource_filter/frame_with_included_script.html"));
  GURL redirect_to_disallowed_subdocument_url(embedded_test_server()->GetURL(
      kAllowedDomain, "/server-redirect?" + disallowed_subdocument_url.spec()));
  NavigateFrame(kSubframeNames[0], redirect_to_disallowed_subdocument_url);

  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectOnlySecondSubframe));

  content::RenderFrameHost* frame = FindFrameByName(kSubframeNames[0]);
  const auto last_committed_url = frame->GetLastCommittedURL();

  ASSERT_TRUE(frame);
  AssertUrlContained(last_committed_url,
                     redirect_to_disallowed_subdocument_url);
  AssertUrlContained(last_committed_url, disallowed_subdocument_url);

  ExpectFramesIncludedInLayout(kSubframeNames, kExpectOnlySecondSubframe);

  // Check test UKM recorder contains event with expected metrics.
  const auto& entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::FingerprintingProtection::kEntryName);
  // 1 entry for every frame_with_included_script.html (2 from initial load, 1
  // from redirect)
  EXPECT_EQ(3u, entries.size());
  for (const ukm::mojom::UkmEntry* entry : entries) {
    test_ukm_recorder.ExpectEntryMetric(
        entry, ukm::builders::FingerprintingProtection::kActivationDecisionName,
        static_cast<int64_t>(
            subresource_filter::ActivationDecision::ACTIVATED));
    EXPECT_FALSE(test_ukm_recorder.EntryHasMetric(
        entry, ukm::builders::FingerprintingProtection::kDryRunName));
  }

  histogram_tester.ExpectBucketCount(
      ActivationDecisionHistogramName,
      subresource_filter::ActivationDecision::ACTIVATED, 1);
  histogram_tester.ExpectBucketCount(
      ActivationLevelHistogramName,
      subresource_filter::mojom::ActivationLevel::kEnabled, 1);
  histogram_tester.ExpectTotalCount(kSubresourceLoadsTotalForPage, 1);
  histogram_tester.ExpectTotalCount(kSubresourceLoadsEvaluatedForPage, 1);
  histogram_tester.ExpectTotalCount(kSubresourceLoadsMatchedRulesForPage, 1);
  histogram_tester.ExpectTotalCount(kSubresourceLoadsDisallowedForPage, 1);
}

IN_PROC_BROWSER_TEST_F(FingerprintingProtectionFilterDryRunBrowserTest,
                       SubframeDocumentLoadFiltering) {
  // TODO(https://crbug.com/358371545): Test console messaging for subframe
  // blocking once its implementation is resolved.
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  GURL url(GetTestUrl(kTestFrameSetPath));

  // Would disallow loading child frame documents that in turn would end up
  // loading included_script.js, unless the document is loaded from an allowed
  // (not in the blocklist) domain to enable the third part of the test dealing
  // with redirects. However, in dry run mode, all framees are expected as
  // nothing is blocked.
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.html"));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  const std::vector<const char*> kSubframeNames{"one", "two", "three"};
  const std::vector<bool> kExpectAllSubframes{true, true, true};
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectAllSubframes));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectAllSubframes);

  // Now navigate the first subframe to an allowed URL and ensure that the load
  // successfully commits.
  GURL allowed_subdocument_url(
      GetTestUrl("subresource_filter/frame_with_allowed_script.html"));
  NavigateFrame(kSubframeNames[0], allowed_subdocument_url);

  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectAllSubframes));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectAllSubframes);

  // Navigate the first subframe to a document that does not load the probe JS.
  GURL allowed_empty_subdocument_url(
      GetTestUrl("subresource_filter/frame_with_no_subresources.html"));
  NavigateFrame(kSubframeNames[0], allowed_empty_subdocument_url);

  // Finally, navigate the first subframe to an allowed URL that redirects to a
  // URL that would be disallowed, and verify that the navigation does not get
  // blocked and the frame doesn't collapse under dry run mode.
  const char kAllowedDomain[] = "allowed.com";
  GURL disallowed_subdocument_url(
      GetTestUrl("subresource_filter/frame_with_included_script.html"));
  GURL redirect_to_disallowed_subdocument_url(embedded_test_server()->GetURL(
      kAllowedDomain, "/server-redirect?" + disallowed_subdocument_url.spec()));
  NavigateFrame(kSubframeNames[0], redirect_to_disallowed_subdocument_url);

  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectAllSubframes));

  content::RenderFrameHost* frame = FindFrameByName(kSubframeNames[0]);

  ASSERT_TRUE(frame);
  EXPECT_EQ(disallowed_subdocument_url, frame->GetLastCommittedURL());
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectAllSubframes);

  // Check test UKM recorder contains event with expected metrics.
  const auto& entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::FingerprintingProtection::kEntryName);
  // 1 entry for every frame_with_included_script.html (2 from initial load, 1
  // from redirect)
  EXPECT_EQ(3u, entries.size());
  for (const ukm::mojom::UkmEntry* entry : entries) {
    test_ukm_recorder.ExpectEntryMetric(
        entry, ukm::builders::FingerprintingProtection::kActivationDecisionName,
        static_cast<int64_t>(
            subresource_filter::ActivationDecision::ACTIVATED));
    EXPECT_TRUE(test_ukm_recorder.EntryHasMetric(
        entry, ukm::builders::FingerprintingProtection::kDryRunName));
  }

  histogram_tester.ExpectBucketCount(
      ActivationDecisionHistogramName,
      subresource_filter::ActivationDecision::ACTIVATED, 1);
  histogram_tester.ExpectBucketCount(
      ActivationLevelHistogramName,
      subresource_filter::mojom::ActivationLevel::kDryRun, 1);
  histogram_tester.ExpectTotalCount(kSubresourceLoadsTotalForPage, 1);
  histogram_tester.ExpectTotalCount(kSubresourceLoadsEvaluatedForPage, 1);
  histogram_tester.ExpectTotalCount(kSubresourceLoadsMatchedRulesForPage, 1);
  histogram_tester.ExpectTotalCount(kSubresourceLoadsDisallowedForPage, 1);
}

class FingerprintingProtectionFilterBrowserTestPerformanceMeasurementsEnabled
    : public FingerprintingProtectionFilterBrowserTest {
 public:
  FingerprintingProtectionFilterBrowserTestPerformanceMeasurementsEnabled() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{features::kEnableFingerprintingProtectionFilter,
          {{"activation_level", "enabled"},
           {"performance_measurement_rate", "1.0"}}}},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

#if BUILDFLAG(IS_MAC)
// TODO(https://crbug.com/40236757): Flaky on Mac.
#define MAYBE_PerformanceMeasurementsHistogramsAreRecorded \
  DISABLED_PerformanceMeasurementsHistogramsAreRecorded
#else
#define MAYBE_PerformanceMeasurementsHistogramsAreRecorded \
  PerformanceMeasurementsHistogramsAreRecorded
#endif

// TODO(https://crbug.com/371981583): Add browser test(s) for Incognito mode
// equivalent.
IN_PROC_BROWSER_TEST_F(
    FingerprintingProtectionFilterBrowserTestPerformanceMeasurementsEnabled,
    MAYBE_PerformanceMeasurementsHistogramsAreRecorded) {
  base::HistogramTester histogram_tester;

  GURL url(GetTestUrl(kTestFrameSetPath));

  // Disallow loading child frame documents that in turn would end up
  // loading included_script.js.
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  const std::vector<const char*> kSubframeNames{"one", "two", "three"};
  const std::vector<bool> kExpectOnlySecondSubframe{false, true, false};
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectOnlySecondSubframe));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectOnlySecondSubframe);

  // Now navigate the first subframe to an allowed URL and ensure that the load
  // successfully commits and the frame gets restored (no longer collapsed).
  GURL allowed_subdocument_url(
      GetTestUrl("subresource_filter/frame_with_allowed_script.html"));
  NavigateFrame(kSubframeNames[0], allowed_subdocument_url);

  const std::vector<bool> kExpectFirstAndSecondSubframe{true, true, false};
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectFirstAndSecondSubframe));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectFirstAndSecondSubframe);

  histogram_tester.ExpectBucketCount(
      ActivationDecisionHistogramName,
      subresource_filter::ActivationDecision::ACTIVATED, 1);
  histogram_tester.ExpectBucketCount(
      ActivationLevelHistogramName,
      subresource_filter::mojom::ActivationLevel::kEnabled, 1);
  histogram_tester.ExpectTotalCount(kSubresourceLoadsTotalForPage, 1);
  histogram_tester.ExpectTotalCount(kSubresourceLoadsEvaluatedForPage, 1);
  histogram_tester.ExpectTotalCount(kSubresourceLoadsMatchedRulesForPage, 1);
  histogram_tester.ExpectTotalCount(kSubresourceLoadsDisallowedForPage, 1);
  histogram_tester.ExpectTotalCount(kEvaluationTotalWallDurationForPage, 1);
  histogram_tester.ExpectTotalCount(kEvaluationTotalCPUDurationForPage, 1);
}

}  // namespace fingerprinting_protection_filter
