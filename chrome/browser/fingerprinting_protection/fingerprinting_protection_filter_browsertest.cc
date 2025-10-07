// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/fingerprinting_protection/fingerprinting_protection_filter_browser_test_harness.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_page_activation_throttle.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_constants.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_features.h"
#include "components/fingerprinting_protection_filter/common/throttle_creation_result.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/tracking_protection_prefs.h"
#include "components/subresource_filter/core/browser/subresource_filter_features_test_support.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/url_pattern_index/proto/rules.pb.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/url_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"  // nogncheck crbug.com/40147906
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"  // nogncheck crbug.com/40147906
#include "chrome/test/base/ui_test_utils.h"
#endif  // !BUILDFLAG(IS_ANDROID)

// TODO(https://crbug.com/358371545): Add browser tests for Histogram population
// for histograms that relate to time measurements as well as renderer
// functionality once blocking is fully implemented.
namespace fingerprinting_protection_filter {

constexpr const char kRendererThrottleCreationResultMetricName[] =
    "FingerprintingProtection.RendererThrottleCreationResult";
constexpr const char kRendererThrottleRedirectsMetricName[] =
    "FingerprintingProtection.RendererThrottleRedirects";

constexpr const char kAllowedDomain[] = "allowed.com";

// =================================== Tests ==================================
//
// Note: Similar to the FPF component, these tests leverage Subresource Filter
// helpers for testing purposes and sample test data files.

IN_PROC_BROWSER_TEST_F(FingerprintingProtectionFilterBrowserTest,
                       MainFrameActivation) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_url =
      GetFrameWithScriptUrl(GetTestUrl("/frame_with_included_script.html"),
                            GetCrossSiteTestUrl("/included_script.js"));

  ASSERT_NO_FATAL_FAILURE(SetRulesetToDisallowURLsWithSubstring(
      "suffix-that-does-not-match-anything"));
  ASSERT_TRUE(NavigateToDestination(test_url));
  EXPECT_TRUE(
      WasParsedScriptElementLoaded(web_contents()->GetPrimaryMainFrame()));

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kRendererThrottleCreationResultMetricName),
      base::BucketsAre(
          base::Bucket(RendererThrottleCreationResult::kCreate, 1)));

  // Navigate to about:blank first to avoid reusing the previous ruleset for
  // the next check.
  ASSERT_TRUE(NavigateToDestination(GURL(url::kAboutBlankURL)));

  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithSubstring("included_script.js"));
  ASSERT_TRUE(NavigateToDestination(test_url));
  EXPECT_FALSE(
      WasParsedScriptElementLoaded(web_contents()->GetPrimaryMainFrame()));

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kRendererThrottleCreationResultMetricName),
      base::BucketsAre(
          base::Bucket(RendererThrottleCreationResult::kCreate, 2)));

  // Navigate to about:blank first to avoid reusing the previous ruleset for
  // the next check.
  ASSERT_TRUE(NavigateToDestination(GURL(url::kAboutBlankURL)));
  SetRulesetToDisallowURLsWithSubstring("frame_with_included_script.html");
  ASSERT_TRUE(NavigateToDestination(test_url));

  // The root frame document should never be filtered.
  EXPECT_TRUE(
      WasParsedScriptElementLoaded(web_contents()->GetPrimaryMainFrame()));

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kRendererThrottleCreationResultMetricName),
      base::BucketsAre(
          base::Bucket(RendererThrottleCreationResult::kCreate, 3)));
}

// There should be no activation on localhosts, except for when
// --enable-benchmarking switch is active.
IN_PROC_BROWSER_TEST_F(FingerprintingProtectionFilterBrowserTest,
                       NoMainFrameActivation_Localhost) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(embedded_test_server()->Start());
  // Use embedded_test_server()->GetURL without a host so it returns a
  // localhost URL.
  GURL test_url =
      embedded_test_server()->GetURL("/frame_with_included_script.html");

  ASSERT_NO_FATAL_FAILURE(SetRulesetToDisallowURLsWithSubstring(
      "suffix-that-does-not-match-anything"));
  ASSERT_TRUE(NavigateToDestination(test_url));
  EXPECT_TRUE(
      WasParsedScriptElementLoaded(web_contents()->GetPrimaryMainFrame()));

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kRendererThrottleCreationResultMetricName),
      base::BucketsAre(
          base::Bucket(RendererThrottleCreationResult::kSkipLocalHost, 1)));

  // Navigate to about:blank first to avoid reusing the previous ruleset for
  // the next check.
  ASSERT_TRUE(NavigateToDestination(GURL(url::kAboutBlankURL)));

  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithSubstring("included_script.js"));
  ASSERT_TRUE(NavigateToDestination(test_url));
  EXPECT_TRUE(
      WasParsedScriptElementLoaded(web_contents()->GetPrimaryMainFrame()));

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kRendererThrottleCreationResultMetricName),
      base::BucketsAre(
          base::Bucket(RendererThrottleCreationResult::kSkipLocalHost, 2)));

  // Navigate to about:blank first to avoid reusing the previous ruleset for
  // the next check.
  ASSERT_TRUE(NavigateToDestination(GURL(url::kAboutBlankURL)));

  SetRulesetToDisallowURLsWithSubstring("frame_with_included_script.html");
  ASSERT_TRUE(NavigateToDestination(test_url));

  // The root frame document should never be filtered.
  EXPECT_TRUE(
      WasParsedScriptElementLoaded(web_contents()->GetPrimaryMainFrame()));

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kRendererThrottleCreationResultMetricName),
      base::BucketsAre(
          base::Bucket(RendererThrottleCreationResult::kSkipLocalHost, 3)));
}

// There should be no activation on localhosts, except for when
// --enable-benchmarking switch is active.
IN_PROC_BROWSER_TEST_F(FingerprintingProtectionFilterBrowserTest,
                       NoMainFrameActivation_LocalhostCrossSite) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_url = GetFrameWithScriptUrl(
      // Use embedded_test_server()->GetURL without a host so it returns a
      // localhost URL.
      embedded_test_server()->GetURL("/frame_with_included_script.html"),
      GetTestUrl("/included_script.js"));

  ASSERT_NO_FATAL_FAILURE(SetRulesetToDisallowURLsWithSubstring(
      "suffix-that-does-not-match-anything"));
  ASSERT_TRUE(NavigateToDestination(test_url));
  EXPECT_TRUE(
      WasParsedScriptElementLoaded(web_contents()->GetPrimaryMainFrame()));

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kRendererThrottleCreationResultMetricName),
      base::BucketsAre(
          base::Bucket(RendererThrottleCreationResult::kCreate, 1)));

  // Navigate to about:blank first to avoid reusing the previous ruleset for
  // the next check.
  ASSERT_TRUE(NavigateToDestination(GURL(url::kAboutBlankURL)));

  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithSubstring("included_script.js"));
  ASSERT_TRUE(NavigateToDestination(test_url));
  EXPECT_TRUE(
      WasParsedScriptElementLoaded(web_contents()->GetPrimaryMainFrame()));

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kRendererThrottleCreationResultMetricName),
      base::BucketsAre(
          base::Bucket(RendererThrottleCreationResult::kCreate, 2)));
}

IN_PROC_BROWSER_TEST_F(FingerprintingProtectionFilterBrowserTest,
                       MainFrameActivation_NotActivatedSameSite) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_url = GetTestUrl("/frame_with_included_script.html");

  ASSERT_NO_FATAL_FAILURE(SetRulesetToDisallowURLsWithSubstring(
      "suffix-that-does-not-match-anything"));
  ASSERT_TRUE(NavigateToDestination(test_url));
  EXPECT_TRUE(
      WasParsedScriptElementLoaded(web_contents()->GetPrimaryMainFrame()));

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kRendererThrottleCreationResultMetricName),
      base::BucketsAre(
          base::Bucket(RendererThrottleCreationResult::kSkipSameSite, 1)));

  // Navigate to about:blank first to avoid reusing the previous ruleset for
  // the next check.
  ASSERT_TRUE(NavigateToDestination(GURL(url::kAboutBlankURL)));

  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithSubstring("included_script.js"));
  ASSERT_TRUE(NavigateToDestination(test_url));
  EXPECT_TRUE(
      WasParsedScriptElementLoaded(web_contents()->GetPrimaryMainFrame()));

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kRendererThrottleCreationResultMetricName),
      base::BucketsAre(
          base::Bucket(RendererThrottleCreationResult::kSkipSameSite, 2)));

  // Navigate to about:blank first to avoid reusing the previous ruleset for
  // the next check.
  ASSERT_TRUE(NavigateToDestination(GURL(url::kAboutBlankURL)));
  SetRulesetToDisallowURLsWithSubstring("frame_with_included_script.html");
  ASSERT_TRUE(NavigateToDestination(test_url));

  // The root frame document should never be filtered.
  EXPECT_TRUE(
      WasParsedScriptElementLoaded(web_contents()->GetPrimaryMainFrame()));

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kRendererThrottleCreationResultMetricName),
      base::BucketsAre(
          base::Bucket(RendererThrottleCreationResult::kSkipSameSite, 3)));
}

IN_PROC_BROWSER_TEST_F(FingerprintingProtectionFilterBrowserTest,
                       SubresourceRedirect_SameSiteToSameSite) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL same_site_to_same_site_redirect_url = GetTestUrl(
      "/server-redirect?" + GetTestUrl("/included_script.js").spec());
  GURL test_url =
      GetFrameWithScriptUrl(GetTestUrl("/frame_with_included_script.html"),
                            same_site_to_same_site_redirect_url);

  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithSubstring("included_script.js"));
  ASSERT_TRUE(NavigateToDestination(test_url));

  EXPECT_TRUE(
      WasParsedScriptElementLoaded(web_contents()->GetPrimaryMainFrame()));

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kRendererThrottleCreationResultMetricName),
      base::BucketsAre(
          base::Bucket(RendererThrottleCreationResult::kSkipSameSite, 1)));

  EXPECT_THAT(
      histogram_tester.GetAllSamples(kRendererThrottleRedirectsMetricName),
      base::BucketsAre(base::Bucket(
          RendererThrottleRedirects::kSameSiteToSameSiteRedirect, 1)));
}

IN_PROC_BROWSER_TEST_F(FingerprintingProtectionFilterBrowserTest,
                       SubresourceRedirect_SameSiteToCrossSite) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL same_site_to_cross_site_redirect_url = GetTestUrl(
      "/server-redirect?" + GetCrossSiteTestUrl("/included_script.js").spec());
  GURL test_url =
      GetFrameWithScriptUrl(GetTestUrl("/frame_with_included_script.html"),
                            same_site_to_cross_site_redirect_url);

  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithSubstring("included_script.js"));
  ASSERT_TRUE(NavigateToDestination(test_url));

  // TODO(crbug.com/444595008): Change to EXPECT_FALSE when we correctly block
  // cross-site redirects.
  EXPECT_TRUE(
      WasParsedScriptElementLoaded(web_contents()->GetPrimaryMainFrame()));

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kRendererThrottleCreationResultMetricName),
      base::BucketsAre(
          base::Bucket(RendererThrottleCreationResult::kSkipSameSite, 1)));

  EXPECT_THAT(
      histogram_tester.GetAllSamples(kRendererThrottleRedirectsMetricName),
      base::BucketsAre(base::Bucket(
          RendererThrottleRedirects::kSameSiteToCrossSiteRedirect, 1)));
}

IN_PROC_BROWSER_TEST_F(FingerprintingProtectionFilterBrowserTest,
                       SubresourceRedirect_CrossSiteToSameSite) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL cross_site_to_same_site_redirect_url = GetCrossSiteTestUrl(
      "/server-redirect?" + GetTestUrl("/included_script.js").spec());
  GURL test_url =
      GetFrameWithScriptUrl(GetTestUrl("/frame_with_included_script.html"),
                            cross_site_to_same_site_redirect_url);

  // We combine an allowed suffix rule to allow the redirecting URL to load
  // and a disallowed suffix rule to block the final `included_script.js` URL.
  auto allowed_suffix = subresource_filter::testing::CreateAllowlistSuffixRule(
      cross_site_to_same_site_redirect_url.spec());
  auto disallowed_suffix =
      subresource_filter::testing::CreateSuffixRule("/included_script.js");
  SetRulesetWithRules(
      {std::move(disallowed_suffix), std::move(allowed_suffix)});

  ASSERT_TRUE(NavigateToDestination(test_url));

  // TODO(crbug.com/444588124): Change to EXPECT_TRUE when we don't block
  // same-site requests that went through a redirect.
  EXPECT_FALSE(
      WasParsedScriptElementLoaded(web_contents()->GetPrimaryMainFrame()));

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kRendererThrottleCreationResultMetricName),
      base::BucketsAre(
          base::Bucket(RendererThrottleCreationResult::kCreate, 1)));

  EXPECT_THAT(
      histogram_tester.GetAllSamples(kRendererThrottleRedirectsMetricName),
      base::BucketsAre(base::Bucket(
          RendererThrottleRedirects::kCrossSiteToSameSiteRedirect, 1)));
}

IN_PROC_BROWSER_TEST_F(FingerprintingProtectionFilterBrowserTest,
                       SubresourceRedirect_CrossSiteToCrossSite) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL cross_site_to_cross_site_redirect_url = GetCrossSiteTestUrl(
      "/server-redirect?" + GetCrossSiteTestUrl("/included_script.js").spec());
  GURL test_url =
      GetFrameWithScriptUrl(GetTestUrl("/frame_with_included_script.html"),
                            cross_site_to_cross_site_redirect_url);

  // We combine an allowed suffix rule to allow the redirecting URL to load
  // and a disallowed suffix rule to block the final `included_script.js` URL.
  auto allowed_suffix = subresource_filter::testing::CreateAllowlistSuffixRule(
      cross_site_to_cross_site_redirect_url.spec());
  auto disallowed_suffix =
      subresource_filter::testing::CreateSuffixRule("/included_script.js");
  ASSERT_NO_FATAL_FAILURE(SetRulesetWithRules(
      {std::move(disallowed_suffix), std::move(allowed_suffix)}));

  ASSERT_TRUE(NavigateToDestination(test_url));

  EXPECT_FALSE(
      WasParsedScriptElementLoaded(web_contents()->GetPrimaryMainFrame()));

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kRendererThrottleCreationResultMetricName),
      base::BucketsAre(
          base::Bucket(RendererThrottleCreationResult::kCreate, 1)));

  EXPECT_THAT(
      histogram_tester.GetAllSamples(kRendererThrottleRedirectsMetricName),
      base::BucketsAre(base::Bucket(
          RendererThrottleRedirects::kCrossSiteToCrossSiteRedirect, 1)));
}

IN_PROC_BROWSER_TEST_F(FingerprintingProtectionFilterBrowserTest,
                       SubframeDocumentLoadFiltering) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // TODO(https://crbug.com/358371545): Test console messaging for subframe
  // blocking once its implementation is resolved.
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  GURL url(GetTestUrl(kMultiPlatformTestFrameSetPath));

  // Disallow loading child frame documents that in turn would end up loading
  // included_script.html, unless the document is loaded from an allowed (not in
  // the blocklist) domain. This enables the third part of this test disallowing
  // a load only after the first redirect.
  auto allowed_substring =
      subresource_filter::testing::CreateAllowlistSubstringRule(
          embedded_test_server()->GetURL(kAllowedDomain, "/").spec());
  auto disallowed_suffix = subresource_filter::testing::CreateSuffixRule(
      "/frame_with_included_script.html");
  ASSERT_NO_FATAL_FAILURE(SetRulesetWithRules(
      {std::move(disallowed_suffix), std::move(allowed_substring)}));

  // `url` will load three subframes:
  //   1. frame_with_included_script.html
  //   2. frame_with_allowed_script.html
  //   3. frame_with_included_script.html
  //
  // These are all same-site iframes so they and their scripts won't be blocked.
  ASSERT_TRUE(NavigateToDestination(url));
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kRendererThrottleCreationResultMetricName),
      base::BucketsAre(
          base::Bucket(RendererThrottleCreationResult::kSkipSameSite, 3)));

  // Navigate all three subframes to:
  //  1. http://cross-site.test/frame_with_included_script.html
  //  2. http://cross-site.test/frame_with_allowed_script.html
  //  3. http://cross-site.test/frame_with_included_script.html
  //
  // Since 1. and 3. are cross-site navigations to disallowed substrings, they
  // get blocked. 2. and its script are allowed.
  NavigateSubframesToCrossOriginSite();

  // TODO(crbug.com/444949848): Remove if() once associated bug is fixed.
  std::vector<base::Bucket> expected_buckets;
  if (web_contents()->GetPrimaryMainFrame()->GetProcess() ==
      content::ChildFrameAt(web_contents(), 1)->GetProcess()) {
    expected_buckets.emplace_back(RendererThrottleCreationResult::kSkipSameSite,
                                  4);
  } else {
    expected_buckets.emplace_back(RendererThrottleCreationResult::kSkipSameSite,
                                  3);
    expected_buckets.emplace_back(
        RendererThrottleCreationResult::kSkipDisabledForCrossSiteSubframe, 1);
  }
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kRendererThrottleCreationResultMetricName),
      base::BucketsAreArray(expected_buckets));

  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectOnlySecondSubframe));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectOnlySecondSubframe);

  // Now navigate the first subframe to an allowed URL and ensure that the load
  // successfully commits and the frame gets restored (no longer collapsed).
  GURL allowed_subdocument_url(
      GetCrossSiteTestUrl("/frame_with_allowed_script.html"));
  NavigateFrame(kSubframeNames[0], allowed_subdocument_url);

  // TODO(crbug.com/444949848): Remove if() once associated bug is fixed.
  if (web_contents()->GetPrimaryMainFrame()->GetProcess() ==
      content::ChildFrameAt(web_contents(), 0)->GetProcess()) {
    expected_buckets[0].count += 1;
  } else {
    expected_buckets[1].count += 1;
  }
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kRendererThrottleCreationResultMetricName),
      base::BucketsAreArray(expected_buckets));

  const std::vector<bool> kExpectFirstAndSecondSubframe{true, true, false};
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectFirstAndSecondSubframe));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectFirstAndSecondSubframe);

  // Navigate the first subframe to a document that does not load the probe JS.
  GURL allowed_empty_subdocument_url(
      GetCrossSiteTestUrl("/frame_with_no_subresources.html"));
  NavigateFrame(kSubframeNames[0], allowed_empty_subdocument_url);

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kRendererThrottleCreationResultMetricName),
      base::BucketsAreArray(expected_buckets));

  // Finally, navigate the first subframe to an allowed URL that redirects to a
  // disallowed URL, and verify that the navigation gets blocked and the frame
  // collapsed.
  GURL disallowed_subdocument_url(
      GetCrossSiteTestUrl("/frame_with_included_script.html"));
  GURL redirect_to_disallowed_subdocument_url(embedded_test_server()->GetURL(
      kAllowedDomain, "/server-redirect?" + disallowed_subdocument_url.spec()));
  NavigateFrame(kSubframeNames[0], redirect_to_disallowed_subdocument_url);

  EXPECT_THAT(
      histogram_tester.GetAllSamples(kRendererThrottleCreationResultMetricName),
      base::BucketsAreArray(expected_buckets));

  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectOnlySecondSubframe));

  content::RenderFrameHost* frame = FindFrameByName(kSubframeNames[0]);
  ASSERT_TRUE(frame);
  const auto last_committed_url = frame->GetLastCommittedURL();
  EXPECT_EQ(last_committed_url, disallowed_subdocument_url);

  ExpectFramesIncludedInLayout(kSubframeNames, kExpectOnlySecondSubframe);

  // Check that `ACTIVATED` UKM events logged 1 entry for every
  // frame_with_included_script.html (2 from initial load, 1 from redirect)
  ExpectFpfActivatedUkms(test_ukm_recorder, 3u,
                         /*is_dry_run=*/false);

  // Check no exceptions have been found and logged to UKM.
  ExpectNoFpfExceptionUkms(test_ukm_recorder);

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

  // No incognito-specific metrics logged.
  histogram_tester.ExpectTotalCount(kSubresourceLoadsTotalForIncognitoPage, 0);
  histogram_tester.ExpectTotalCount(kSubresourceLoadsEvaluatedForIncognitoPage,
                                    0);
  histogram_tester.ExpectTotalCount(
      kSubresourceLoadsMatchedRulesForIncognitoPage, 0);
  histogram_tester.ExpectTotalCount(kSubresourceLoadsDisallowedForIncognitoPage,
                                    0);
}

IN_PROC_BROWSER_TEST_F(FingerprintingProtectionFilterDryRunBrowserTest,
                       SubframeDocumentLoadFiltering) {
  // TODO(https://crbug.com/358371545): Test console messaging for subframe
  // blocking once its implementation is resolved.
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  GURL url(GetTestUrl(kMultiPlatformTestFrameSetPath));

  // Would disallow loading child frame documents that in turn would end up
  // loading included_script.js, unless the document is loaded from an allowed
  // (not in the blocklist) domain to enable the third part of the test dealing
  // with redirects. However, in dry run mode, all frames are expected as
  // nothing is blocked.
  auto allowed_substring =
      subresource_filter::testing::CreateAllowlistSubstringRule(
          embedded_test_server()->GetURL(kAllowedDomain, "/").spec());
  auto disallowed_suffix = subresource_filter::testing::CreateSuffixRule(
      "/frame_with_included_script.html");
  ASSERT_NO_FATAL_FAILURE(SetRulesetWithRules(
      {std::move(disallowed_suffix), std::move(allowed_substring)}));

  ASSERT_TRUE(NavigateToDestination(url));
  NavigateSubframesToCrossOriginSite();

  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectAllSubframes));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectAllSubframes);

  // Now navigate the first subframe to an allowed URL and ensure that the load
  // successfully commits.
  GURL allowed_subdocument_url(
      GetCrossSiteTestUrl("/frame_with_allowed_script.html"));
  NavigateFrame(kSubframeNames[0], allowed_subdocument_url);

  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectAllSubframes));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectAllSubframes);

  // Navigate the first subframe to a document that does not load the probe
  // JS.
  GURL allowed_empty_subdocument_url(
      GetCrossSiteTestUrl("/frame_with_no_subresources.html"));
  NavigateFrame(kSubframeNames[0], allowed_empty_subdocument_url);

  // Finally, navigate the first subframe to an allowed URL that redirects to a
  // URL that would be disallowed, and verify that the navigation does not get
  // blocked and the frame doesn't collapse under dry run mode.
  GURL disallowed_subdocument_url(
      GetCrossSiteTestUrl("/frame_with_included_script.html"));
  GURL redirect_to_disallowed_subdocument_url(embedded_test_server()->GetURL(
      kAllowedDomain, "/server-redirect?" + disallowed_subdocument_url.spec()));
  NavigateFrame(kSubframeNames[0], redirect_to_disallowed_subdocument_url);

  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectAllSubframes));

  content::RenderFrameHost* frame = FindFrameByName(kSubframeNames[0]);

  ASSERT_TRUE(frame);
  EXPECT_EQ(disallowed_subdocument_url, frame->GetLastCommittedURL());
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectAllSubframes);

  // Check that `ACTIVATED` UKM events logged 1 entry for every
  // frame_with_included_script.html (2 from initial load, 1 from redirect)
  ExpectFpfActivatedUkms(test_ukm_recorder, 3u,
                         /*is_dry_run=*/true);

  // Check no exceptions have been found and logged to UKM.
  ExpectNoFpfExceptionUkms(test_ukm_recorder);

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

  // No incognito-specific metrics logged.
  histogram_tester.ExpectTotalCount(kSubresourceLoadsTotalForIncognitoPage, 0);
  histogram_tester.ExpectTotalCount(kSubresourceLoadsEvaluatedForIncognitoPage,
                                    0);
  histogram_tester.ExpectTotalCount(
      kSubresourceLoadsMatchedRulesForIncognitoPage, 0);
  histogram_tester.ExpectTotalCount(kSubresourceLoadsDisallowedForIncognitoPage,
                                    0);
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
        /*disabled_features=*/{
            {features::kEnableFingerprintingProtectionFilterInIncognito},
            {privacy_sandbox::kFingerprintingProtectionUx}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    FingerprintingProtectionFilterBrowserTestPerformanceMeasurementsEnabled,
    PerformanceMeasurementsHistogramsAreRecorded) {
  ASSERT_TRUE(embedded_test_server()->Start());
  base::HistogramTester histogram_tester;

  GURL url(GetTestUrl(kMultiPlatformTestFrameSetPath));

  // Disallow loading child frame documents that in turn would end up
  // loading included_script.js.
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithSubstring("included_script.html"));
  ASSERT_TRUE(NavigateToDestination(url));
  NavigateSubframesToCrossOriginSite();

  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectOnlySecondSubframe));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectOnlySecondSubframe);

  // Now navigate the first subframe to an allowed URL and ensure that the load
  // successfully commits and the frame gets restored (no longer collapsed).
  GURL allowed_subdocument_url(
      GetCrossSiteTestUrl("/frame_with_allowed_script.html"));
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

  // No incognito-specific metrics logged.
  histogram_tester.ExpectTotalCount(kSubresourceLoadsTotalForIncognitoPage, 0);
  histogram_tester.ExpectTotalCount(kSubresourceLoadsEvaluatedForIncognitoPage,
                                    0);
  histogram_tester.ExpectTotalCount(
      kSubresourceLoadsMatchedRulesForIncognitoPage, 0);
  histogram_tester.ExpectTotalCount(kSubresourceLoadsDisallowedForIncognitoPage,
                                    0);
  histogram_tester.ExpectTotalCount(
      kEvaluationTotalWallDurationForIncognitoPage, 0);
  histogram_tester.ExpectTotalCount(kEvaluationTotalCPUDurationForIncognitoPage,
                                    0);

  // TODO(https://crbug.com/376308447): Potentially add histogram assertions for
  // FP performance measurements from DocumentSubresourceFilter. Currently, the
  // codepath is not triggered in FP browser tests because requests from
  // localhost are ignored in RendererUrlLoaderThrottle.

  // Expect 4 subresource loads, 1 per frame in
  // `kMultiPlatformTestFrameSetPath`: "one", "two", "three" + 1 from
  // `NavigateFrame` call above.
  histogram_tester.ExpectTotalCount(kSubresourceLoadEvaluationWallDuration, 4);
  histogram_tester.ExpectTotalCount(kSubresourceLoadEvaluationCpuDuration, 4);
}

// TODO(https://crbug.com/379336042): The following tests cannot be included for
// Android because of the usage of `Browser` (its header cannot be included for
// Android targets). See if there is a potential workaround.
#if !BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_F(
    FingerprintingProtectionFilterEnabledInIncognitoBrowserTest,
    SubframeDocumentLoadFiltering) {
  // Close normal browser and switch the test's browser instance to an incognito
  // instance.
  BrowserWindowInterface* const incognito =
      CreateIncognitoBrowser(browser()->profile());
  CloseBrowserSynchronously(browser());
  SetBrowser(incognito);
  ASSERT_EQ(browser(), incognito);

  // TODO(https://crbug.com/358371545): Test console messaging for subframe
  // blocking once its implementation is resolved.
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  GURL url(GetTestUrl(kMultiPlatformTestFrameSetPath));

  // Disallow loading child frame documents that in turn would end up
  // loading included_script.js, unless the document is loaded from an allowed
  // (not in the blocklist) domain. This enables the third part of this test
  // disallowing a load only after the first redirect.
  auto allowed_substring =
      subresource_filter::testing::CreateAllowlistSubstringRule(
          embedded_test_server()->GetURL(kAllowedDomain, "/").spec());
  auto disallowed_suffix = subresource_filter::testing::CreateSuffixRule(
      "/frame_with_included_script.html");
  ASSERT_NO_FATAL_FAILURE(SetRulesetWithRules(
      {std::move(disallowed_suffix), std::move(allowed_substring)}));

  ASSERT_TRUE(NavigateToDestination(url));
  NavigateSubframesToCrossOriginSite();

  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectOnlySecondSubframe));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectOnlySecondSubframe);

  // Now navigate the first subframe to an allowed URL and ensure that the load
  // successfully commits and the frame gets restored (no longer collapsed).
  GURL allowed_subdocument_url(
      GetCrossSiteTestUrl("/frame_with_allowed_script.html"));
  NavigateFrame(kSubframeNames[0], allowed_subdocument_url);

  const std::vector<bool> kExpectFirstAndSecondSubframe{true, true, false};
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectFirstAndSecondSubframe));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectFirstAndSecondSubframe);

  // Navigate the first subframe to a document that does not load the probe JS.
  GURL allowed_empty_subdocument_url(
      GetCrossSiteTestUrl("/frame_with_no_subresources.html"));
  NavigateFrame(kSubframeNames[0], allowed_empty_subdocument_url);

  // Finally, navigate the first subframe to an allowed URL that redirects to a
  // disallowed URL, and verify that the navigation gets blocked and the frame
  // collapsed.
  GURL disallowed_subdocument_url(
      GetCrossSiteTestUrl("/frame_with_included_script.html"));
  GURL redirect_to_disallowed_subdocument_url(embedded_test_server()->GetURL(
      kAllowedDomain, "/server-redirect?" + disallowed_subdocument_url.spec()));
  NavigateFrame(kSubframeNames[0], redirect_to_disallowed_subdocument_url);

  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectOnlySecondSubframe));

  content::RenderFrameHost* frame = FindFrameByName(kSubframeNames[0]);
  ASSERT_TRUE(frame);
  const auto last_committed_url = frame->GetLastCommittedURL();
  EXPECT_EQ(last_committed_url, disallowed_subdocument_url);

  ExpectFramesIncludedInLayout(kSubframeNames, kExpectOnlySecondSubframe);

  // Check that `ACTIVATED` UKM events logged 1 entry for every
  // frame_with_included_script.html (2 from initial load, 1 from redirect)
  ExpectFpfActivatedUkms(test_ukm_recorder, 3u,
                         /*is_dry_run=*/false);

  // Check no exceptions have been found and logged to UKM.
  ExpectNoFpfExceptionUkms(test_ukm_recorder);

  histogram_tester.ExpectBucketCount(
      ActivationDecisionHistogramName,
      subresource_filter::ActivationDecision::ACTIVATED, 1);
  histogram_tester.ExpectBucketCount(
      ActivationLevelHistogramName,
      subresource_filter::mojom::ActivationLevel::kEnabled, 1);

  // Incognito page-specific metrics emitted.
  histogram_tester.ExpectTotalCount(kSubresourceLoadsDisallowedForIncognitoPage,
                                    1);
  histogram_tester.ExpectTotalCount(kSubresourceLoadsEvaluatedForIncognitoPage,
                                    1);
  histogram_tester.ExpectTotalCount(
      kSubresourceLoadsMatchedRulesForIncognitoPage, 1);
  histogram_tester.ExpectTotalCount(kSubresourceLoadsTotalForIncognitoPage, 1);

  // Expect total-metrics emitted to be the same as incognito metrics emitted.
  histogram_tester.ExpectTotalCount(kSubresourceLoadsDisallowedForPage, 1);
  histogram_tester.ExpectTotalCount(kSubresourceLoadsEvaluatedForPage, 1);
  histogram_tester.ExpectTotalCount(kSubresourceLoadsMatchedRulesForPage, 1);
  histogram_tester.ExpectTotalCount(kSubresourceLoadsTotalForPage, 1);
}

IN_PROC_BROWSER_TEST_F(
    FingerprintingProtectionFilterEnabledInIncognitoBrowserTest,
    NoSubresourcesEvaluatedInRegularBrowsing) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  // Open an incognito instance but keep using the non-incognito browser for
  // testing.
  SetBrowser(GetLastActiveBrowserWindowInterfaceWithAnyProfile());
  BrowserWindowInterface* const incognito =
      CreateIncognitoBrowser(browser()->profile());
  ASSERT_NE(browser(), incognito);

  GURL url(GetTestUrl(kMultiPlatformTestFrameSetPath));

  // Disallow loading included_script.js as a subresource.
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithSubstring("included_script.js"));

  ASSERT_TRUE(NavigateToDestination(url));
  NavigateSubframesToCrossOriginSite();

  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectAllSubframes));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectAllSubframes);

  // Check that `ACTIVATED` UKM logged no entries.
  ExpectFpfActivatedUkms(test_ukm_recorder, 0u,
                         /*is_dry_run=*/false);

  // No feature activations.
  histogram_tester.ExpectBucketCount(
      ActivationDecisionHistogramName,
      subresource_filter::ActivationDecision::ACTIVATED, 0);
  histogram_tester.ExpectBucketCount(
      ActivationLevelHistogramName,
      subresource_filter::mojom::ActivationLevel::kEnabled, 0);

  // No Incognito page-specific metrics emitted.
  histogram_tester.ExpectTotalCount(kSubresourceLoadsDisallowedForIncognitoPage,
                                    0);
  histogram_tester.ExpectTotalCount(kSubresourceLoadsEvaluatedForIncognitoPage,
                                    0);
  histogram_tester.ExpectTotalCount(
      kSubresourceLoadsMatchedRulesForIncognitoPage, 0);
  histogram_tester.ExpectTotalCount(kSubresourceLoadsTotalForIncognitoPage, 0);

  // No other metrics emitted.
  histogram_tester.ExpectTotalCount(kSubresourceLoadsDisallowedForPage, 0);
  histogram_tester.ExpectTotalCount(kSubresourceLoadsEvaluatedForPage, 0);
  histogram_tester.ExpectTotalCount(kSubresourceLoadsMatchedRulesForPage, 0);
  histogram_tester.ExpectTotalCount(kSubresourceLoadsTotalForPage, 0);
}

class
    FingerprintingProtectionFilterBrowserTestPerformanceMeasurementsEnabledInIncognito
    : public FingerprintingProtectionFilterEnabledInIncognitoBrowserTest {
 public:
  FingerprintingProtectionFilterBrowserTestPerformanceMeasurementsEnabledInIncognito() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{features::kEnableFingerprintingProtectionFilterInIncognito,
          {{"performance_measurement_rate", "1.0"}}},
         {privacy_sandbox::kFingerprintingProtectionUx, {}}},
        /*disabled_features=*/{
            {features::kEnableFingerprintingProtectionFilter}});
  }

 protected:
  void SetUpOnMainThread() override {
    FingerprintingProtectionFilterBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    FingerprintingProtectionFilterBrowserTestPerformanceMeasurementsEnabledInIncognito,
    PerformanceMeasurementsHistogramsAreRecorded) {
  // Close normal browser and switch the test's browser instance to an incognito
  // instance.
  BrowserWindowInterface* const incognito =
      CreateIncognitoBrowser(browser()->profile());
  CloseBrowserSynchronously(browser());
  SetBrowser(incognito);
  ASSERT_EQ(browser(), incognito);

  base::HistogramTester histogram_tester;

  GURL url(GetTestUrl(kMultiPlatformTestFrameSetPath));

  // Disallow loading child frame documents that in turn would end up
  // loading included_script.js.
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithSubstring("included_script.html"));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  NavigateSubframesToCrossOriginSite();

  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectOnlySecondSubframe));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectOnlySecondSubframe);

  // Now navigate the first subframe to an allowed URL and ensure that the load
  // successfully commits and the frame gets restored (no longer collapsed).
  GURL allowed_subdocument_url(
      GetCrossSiteTestUrl("/frame_with_allowed_script.html"));
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

  // Incognito page-specific metrics emitted.
  histogram_tester.ExpectTotalCount(kSubresourceLoadsTotalForIncognitoPage, 1);
  histogram_tester.ExpectTotalCount(kSubresourceLoadsEvaluatedForIncognitoPage,
                                    1);
  histogram_tester.ExpectTotalCount(
      kSubresourceLoadsMatchedRulesForIncognitoPage, 1);
  histogram_tester.ExpectTotalCount(kSubresourceLoadsDisallowedForIncognitoPage,
                                    1);
  histogram_tester.ExpectTotalCount(
      kEvaluationTotalWallDurationForIncognitoPage, 1);
  histogram_tester.ExpectTotalCount(kEvaluationTotalCPUDurationForIncognitoPage,
                                    1);

  // Expect total-metrics emitted to be the same as incognito metrics emitted.
  histogram_tester.ExpectTotalCount(kSubresourceLoadsTotalForPage, 1);
  histogram_tester.ExpectTotalCount(kSubresourceLoadsEvaluatedForPage, 1);
  histogram_tester.ExpectTotalCount(kSubresourceLoadsMatchedRulesForPage, 1);
  histogram_tester.ExpectTotalCount(kSubresourceLoadsDisallowedForPage, 1);
  histogram_tester.ExpectTotalCount(kEvaluationTotalWallDurationForPage, 1);
  histogram_tester.ExpectTotalCount(kEvaluationTotalCPUDurationForPage, 1);

  // Expect 4 subresource loads, 1 per frame in
  // `kMultiPlatformTestFrameSetPath`: "one", "two", "three" + 1
  // from `NavigateFrame` call above.
  histogram_tester.ExpectTotalCount(kSubresourceLoadEvaluationWallDuration, 4);
  histogram_tester.ExpectTotalCount(kSubresourceLoadEvaluationCpuDuration, 4);
}

// TODO(https://crbug.com/382055410): Adjust
// `FingerprintingProtectionFilterRefreshHeuristicExceptionBrowserTest` tests so
// they can also run on android.
class FPFRefreshHeuristicExceptionBrowserTestParamEnabledOnlyNonIncognito
    : public FingerprintingProtectionFilterRefreshHeuristicExceptionBrowserTest {
 public:
  FPFRefreshHeuristicExceptionBrowserTestParamEnabledOnlyNonIncognito() {
    // Enable refresh heuristic after 2 refreshes in nonincognito.
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{features::kEnableFingerprintingProtectionFilter,
          {{features::kRefreshHeuristicExceptionThresholdParam, "2"}}},
         {features::kEnableFingerprintingProtectionFilterInIncognito, {}}},
        /*disabled_features=*/{{privacy_sandbox::kFingerprintingProtectionUx}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    FPFRefreshHeuristicExceptionBrowserTestParamEnabledOnlyNonIncognito,
    ExceptionIsAddedInNonIncognito) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  // Refresh exception code depends on eTLD+1, so we need to navigate to a
  // host with a domain name.
  GURL url(embedded_test_server()->GetURL("google.test",
                                          kMultiPlatformTestFrameSetPath));

  // Disallow child frame documents.
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithSubstring("included_script.html"));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  NavigateMultiFrameSubframesAndLoad3pScripts();

  // Expect initially only second subframe loads due to blocking.
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectOnlySecondSubframe));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectOnlySecondSubframe);

  // Check that UKM contains all entries where a resource's load policy is
  // `DISALLOW`, subframe "one" and "three".
  ExpectFpfActivatedUkms(test_ukm_recorder, 2u,
                         /*is_dry_run=*/false);

  // Check that no exception UKMs are logged.
  ExpectNoFpfExceptionUkms(test_ukm_recorder);

  // Reload.
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  ASSERT_TRUE(content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents()));
  NavigateMultiFrameSubframesAndLoad3pScripts();
  // Blocking still has effect.
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectOnlySecondSubframe));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectOnlySecondSubframe);

  // +2 activation UKMs for subframes "one" and "three" again.
  ExpectFpfActivatedUkms(test_ukm_recorder, 4u,
                         /*is_dry_run=*/false);

  // Reload again.
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  ASSERT_TRUE(content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents()));
  NavigateMultiFrameSubframesAndLoad3pScripts();
  // Exception added - expect all subframes to be visible.

  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectAllSubframes));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectAllSubframes);

  // +0 activation UKMs since refresh heuristic is applied.
  ExpectFpfActivatedUkms(test_ukm_recorder, 4u,
                         /*is_dry_run=*/false);

  // Check that exception UKM is logged, as refresh heuristic is applied.
  ExpectFpfExceptionUkms(
      test_ukm_recorder, 1u,
      static_cast<int64_t>(ExceptionSource::REFRESH_HEURISTIC));
}

IN_PROC_BROWSER_TEST_F(
    FPFRefreshHeuristicExceptionBrowserTestParamEnabledOnlyNonIncognito,
    ExceptionIsNotAddedInIncognito) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  // Close normal browser and switch the test's browser instance to an incognito
  // instance.
  BrowserWindowInterface* const incognito =
      CreateIncognitoBrowser(browser()->profile());
  CloseBrowserSynchronously(browser());
  SetBrowser(incognito);
  ASSERT_EQ(browser(), incognito);

  // Refresh exception code depends on eTLD+1, so we need to navigate to a
  // host with a domain name.
  GURL url(embedded_test_server()->GetURL("google.test",
                                          kMultiPlatformTestFrameSetPath));

  // Disallow child frame documents.
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithSubstring("included_script.html"));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  NavigateMultiFrameSubframesAndLoad3pScripts();

  // Expect initially only second subframe loads due to blocking.
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectOnlySecondSubframe));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectOnlySecondSubframe);

  // Reload.
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  ASSERT_TRUE(content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents()));
  NavigateMultiFrameSubframesAndLoad3pScripts();
  // Blocking still has effect.
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectOnlySecondSubframe));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectOnlySecondSubframe);

  // Reload again.
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  ASSERT_TRUE(content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents()));
  NavigateMultiFrameSubframesAndLoad3pScripts();
  // Blocking still has effect.
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectOnlySecondSubframe));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectOnlySecondSubframe);

  // Expect 2 activation UKMS, one each for blocked subframes "one" and "three",
  // x3 loads.
  ExpectFpfActivatedUkms(test_ukm_recorder, 6u,
                         /*is_dry_run=*/false);

  // Check that no exception UKMs are logged.
  ExpectNoFpfExceptionUkms(test_ukm_recorder);
}

class FPFRefreshHeuristicExceptionBrowserTestParamEnabledOnlyIncognito
    : public FingerprintingProtectionFilterRefreshHeuristicExceptionBrowserTest {
 public:
  FPFRefreshHeuristicExceptionBrowserTestParamEnabledOnlyIncognito() {
    // Enable refresh heuristic after 2 refreshes in incognito.
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{features::kEnableFingerprintingProtectionFilterInIncognito,
          {{features::kRefreshHeuristicExceptionThresholdParam, "2"}}},
         {features::kEnableFingerprintingProtectionFilter, {}}},
        /*disabled_features=*/{{privacy_sandbox::kFingerprintingProtectionUx}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    FPFRefreshHeuristicExceptionBrowserTestParamEnabledOnlyIncognito,
    ExceptionIsNotAddedInNonIncognito) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  // Refresh exception code depends on eTLD+1, so we need to navigate to a
  // host with a domain name.
  GURL url(embedded_test_server()->GetURL("google.test",
                                          kMultiPlatformTestFrameSetPath));

  // Disallow child frame documents.
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithSubstring("included_script.html"));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  NavigateMultiFrameSubframesAndLoad3pScripts();

  // Expect initially only second subframe loads due to blocking.

  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectOnlySecondSubframe));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectOnlySecondSubframe);

  // Reload.
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  ASSERT_TRUE(content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents()));
  NavigateMultiFrameSubframesAndLoad3pScripts();
  // Blocking still has effect.
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectOnlySecondSubframe));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectOnlySecondSubframe);

  // Reload again.
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  ASSERT_TRUE(content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents()));
  NavigateMultiFrameSubframesAndLoad3pScripts();
  // Blocking still has effect.
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectOnlySecondSubframe));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectOnlySecondSubframe);

  // Expect 2 activation UKMS, one each for blocked subframes "one" and "three",
  // x3 loads.
  ExpectFpfActivatedUkms(test_ukm_recorder, 6u,
                         /*is_dry_run=*/false);

  // Check that no exception UKMs are logged.
  ExpectNoFpfExceptionUkms(test_ukm_recorder);
}

IN_PROC_BROWSER_TEST_F(
    FPFRefreshHeuristicExceptionBrowserTestParamEnabledOnlyIncognito,
    ExceptionIsAddedInIncognito) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  // Close normal browser and switch the test's browser instance to an incognito
  // instance.
  BrowserWindowInterface* const incognito =
      CreateIncognitoBrowser(browser()->profile());
  CloseBrowserSynchronously(browser());
  SetBrowser(incognito);
  ASSERT_EQ(browser(), incognito);

  // Refresh exception code depends on eTLD+1, so we need to navigate to a
  // host with a domain name.
  GURL url(embedded_test_server()->GetURL("google.test",
                                          kMultiPlatformTestFrameSetPath));

  // Disallow child frame documents.
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithSubstring("included_script.html"));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  NavigateMultiFrameSubframesAndLoad3pScripts();

  // Expect initially only second subframe loads due to blocking.
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectOnlySecondSubframe));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectOnlySecondSubframe);

  // Check that activated UKMs are logged, 1 for each subframe "one" and "three"
  // containing "included_script.html".
  ExpectFpfActivatedUkms(test_ukm_recorder, 2u,
                         /*is_dry_run=*/false);

  // Reload.
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  ASSERT_TRUE(content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents()));
  NavigateMultiFrameSubframesAndLoad3pScripts();
  // Blocking still has effect.
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectOnlySecondSubframe));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectOnlySecondSubframe);

  // +2 activation UKMs for subframes "one" and "three" again.
  ExpectFpfActivatedUkms(test_ukm_recorder, 4u,
                         /*is_dry_run=*/false);

  // Reload again.
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  ASSERT_TRUE(content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents()));
  NavigateMultiFrameSubframesAndLoad3pScripts();
  // Exception added - expect all subframes to be visible.

  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectAllSubframes));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectAllSubframes);

  // +0 activation UKMs since refresh heuristic is applied.
  ExpectFpfActivatedUkms(test_ukm_recorder, 4u,
                         /*is_dry_run=*/false);

  // Check that exception UKM is logged, as refresh heuristic is applied.
  ExpectFpfExceptionUkms(
      test_ukm_recorder, 1u,
      static_cast<int64_t>(ExceptionSource::REFRESH_HEURISTIC));
}

class FPFRefreshHeuristicExceptionBrowserTestParamEnabledBoth
    : public FingerprintingProtectionFilterRefreshHeuristicExceptionBrowserTest {
 public:
  FPFRefreshHeuristicExceptionBrowserTestParamEnabledBoth() {
    // Enable refresh heuristic after 2 refreshes in nonincognito and incognito.
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{features::kEnableFingerprintingProtectionFilter,
          {{features::kRefreshHeuristicExceptionThresholdParam, "2"}}},
         {features::kEnableFingerprintingProtectionFilterInIncognito,
          {{features::kRefreshHeuristicExceptionThresholdParam, "2"}}}},
        /*disabled_features=*/{{privacy_sandbox::kFingerprintingProtectionUx}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(FPFRefreshHeuristicExceptionBrowserTestParamEnabledBoth,
                       ExceptionAddedInNonIncognitoPersistsIntoIncognito) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  // Refresh exception code depends on eTLD+1, so we need to navigate to a
  // host with a domain name.
  GURL url(embedded_test_server()->GetURL("google.test",
                                          kMultiPlatformTestFrameSetPath));

  // Disallow child frame documents.
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithSubstring("included_script.html"));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  NavigateMultiFrameSubframesAndLoad3pScripts();

  // Expect only second subframe loads due to blocking.
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectOnlySecondSubframe));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectOnlySecondSubframe);

  // Check that UKM is logged, one per frame with included_script.html ("one"
  // and "three").
  ExpectFpfActivatedUkms(test_ukm_recorder, 2u,
                         /*is_dry_run=*/false);
  ExpectNoFpfExceptionUkms(test_ukm_recorder);

  // Reload twice.
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  ASSERT_TRUE(content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents()));
  NavigateMultiFrameSubframesAndLoad3pScripts();
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  ASSERT_TRUE(content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents()));
  NavigateMultiFrameSubframesAndLoad3pScripts();
  // Exception added - expect all subframes to be visible.

  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectAllSubframes));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectAllSubframes);

  // +2 for frames "one" and "three" again.
  ExpectFpfActivatedUkms(test_ukm_recorder, 4u,
                         /*is_dry_run=*/false);

  // Check that exception UKM is logged, as refresh heuristic is applied.
  ExpectFpfExceptionUkms(
      test_ukm_recorder, 1u,
      static_cast<int64_t>(ExceptionSource::REFRESH_HEURISTIC));

  // Close normal browser and switch the test's browser instance to an incognito
  // instance.
  BrowserWindowInterface* const incognito =
      CreateIncognitoBrowser(browser()->profile());
  CloseBrowserSynchronously(browser());
  SetBrowser(incognito);
  ASSERT_EQ(browser(), incognito);

  // Go to same URL.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  // Exception persists into incognito.
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectAllSubframes));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectAllSubframes);

  // +0 since refresh heuristic exception persists.
  ExpectFpfActivatedUkms(test_ukm_recorder, 4u,
                         /*is_dry_run=*/false);
  // +1 for the persisted refresh heuristic  applied to navigation in incognito.
  ExpectFpfExceptionUkms(
      test_ukm_recorder, 2u,
      static_cast<int64_t>(ExceptionSource::REFRESH_HEURISTIC));
}

IN_PROC_BROWSER_TEST_F(
    FPFRefreshHeuristicExceptionBrowserTestParamEnabledBoth,
    ExceptionAddedInIncognitoDoesNotPersistIntoNonIncognito) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  // Hold a reference to the nonincognito profile so we can create another
  // nonincognito window later.
  Profile* nonincognito_profile = browser()->profile();
  // Close normal browser and switch the test's browser instance to an incognito
  // instance.
  BrowserWindowInterface* const incognito =
      CreateIncognitoBrowser(nonincognito_profile);
  CloseBrowserSynchronously(browser());
  SetBrowser(incognito);
  ASSERT_EQ(browser(), incognito);

  // Refresh exception code depends on eTLD+1, so we need to navigate to a
  // host with a domain name.
  GURL url(embedded_test_server()->GetURL("google.test",
                                          kMultiPlatformTestFrameSetPath));

  // Disallow child frame documents.
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithSubstring("included_script.html"));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  NavigateMultiFrameSubframesAndLoad3pScripts();

  // Expect only second subframe loads due to blocking.
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectOnlySecondSubframe));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectOnlySecondSubframe);

  // Reload twice.
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  ASSERT_TRUE(content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents()));
  NavigateMultiFrameSubframesAndLoad3pScripts();
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  ASSERT_TRUE(content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents()));
  NavigateMultiFrameSubframesAndLoad3pScripts();

  // Exception added - expect all subframes to be visible.
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectAllSubframes));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectAllSubframes);

  // Check that UKM is logged, one for each frame with "included_script.html" is
  // blocked, until exception is present.
  ExpectFpfActivatedUkms(test_ukm_recorder, 4u,
                         /*is_dry_run=*/false);

  // Check that exception UKM is logged, for incognito, as refresh heuristic is
  // applied.
  ExpectFpfExceptionUkms(
      test_ukm_recorder, 1u,
      static_cast<int64_t>(ExceptionSource::REFRESH_HEURISTIC));

  // Close incognito and open nonincognito browser instance.
  BrowserWindowInterface* const nonincognito =
      CreateBrowser(nonincognito_profile);
  CloseBrowserSynchronously(browser());
  SetBrowser(nonincognito);
  ASSERT_EQ(browser(), nonincognito);

  // Go to same URL.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  NavigateMultiFrameSubframesAndLoad3pScripts();

  // Exception doesn't persist into nonincognito.
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectOnlySecondSubframe));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectOnlySecondSubframe);

  // Expect 2 activation UKMS, one each for blocked subframes "one" and "three",
  // x3 loads.
  ExpectFpfActivatedUkms(test_ukm_recorder, 6u,
                         /*is_dry_run=*/false);
  // Check that the UKM exception log is unchanged, not persisted and relogged.
  ExpectFpfExceptionUkms(
      test_ukm_recorder, 1u,
      static_cast<int64_t>(ExceptionSource::REFRESH_HEURISTIC));
}

class FPFRefreshHeuristicExceptionBrowserTestParamDisabledBoth
    : public FingerprintingProtectionFilterRefreshHeuristicExceptionBrowserTest {
 public:
  FPFRefreshHeuristicExceptionBrowserTestParamDisabledBoth() {
    // Disable refresh heuristic.
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{features::kEnableFingerprintingProtectionFilter, {}},
         {features::kEnableFingerprintingProtectionFilterInIncognito, {}}},
        /*disabled_features=*/{{privacy_sandbox::kFingerprintingProtectionUx}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(FPFRefreshHeuristicExceptionBrowserTestParamDisabledBoth,
                       NoExceptionAddedInNonIncognito) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  // Refresh exception code depends on eTLD+1, so we need to navigate to a
  // host with a domain name.
  GURL url(embedded_test_server()->GetURL("google.test",
                                          kMultiPlatformTestFrameSetPath));

  // Disallow child frame documents.
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithSubstring("included_script.html"));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  NavigateMultiFrameSubframesAndLoad3pScripts();

  // Expect only second subframe loads due to blocking.
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectOnlySecondSubframe));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectOnlySecondSubframe);

  // Reload.
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  ASSERT_TRUE(content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents()));
  NavigateMultiFrameSubframesAndLoad3pScripts();
  // Blocking still has effect.
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectOnlySecondSubframe));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectOnlySecondSubframe);

  // Reload again.
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  ASSERT_TRUE(content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents()));
  NavigateMultiFrameSubframesAndLoad3pScripts();
  // Blocking still has effect.
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectOnlySecondSubframe));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectOnlySecondSubframe);

  // Expect 2 activation UKMS, one each for blocked subframes "one" and "three",
  // x3 loads.
  ExpectFpfActivatedUkms(test_ukm_recorder, 6u,
                         /*is_dry_run=*/false);

  // Check that no exception UKMs are logged.
  ExpectNoFpfExceptionUkms(test_ukm_recorder);
}

IN_PROC_BROWSER_TEST_F(FPFRefreshHeuristicExceptionBrowserTestParamDisabledBoth,
                       NoExceptionAddedInIncognito) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  // Close normal browser and switch the test's browser instance to an incognito
  // instance.
  BrowserWindowInterface* const incognito =
      CreateIncognitoBrowser(browser()->profile());
  CloseBrowserSynchronously(browser());
  SetBrowser(incognito);
  ASSERT_EQ(browser(), incognito);

  // Refresh exception code depends on eTLD+1, so we need to navigate to a
  // host with a domain name.
  GURL url(embedded_test_server()->GetURL("google.test",
                                          kMultiPlatformTestFrameSetPath));

  // Disallow child frame documents.
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithSubstring("included_script.html"));

  // Expect only second subframe loads due to blocking.

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  NavigateMultiFrameSubframesAndLoad3pScripts();

  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectOnlySecondSubframe));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectOnlySecondSubframe);

  // Reload.
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  ASSERT_TRUE(content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents()));
  NavigateMultiFrameSubframesAndLoad3pScripts();
  // Blocking still has effect.
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectOnlySecondSubframe));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectOnlySecondSubframe);

  // Reload again.
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  ASSERT_TRUE(content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents()));
  NavigateMultiFrameSubframesAndLoad3pScripts();
  // Blocking still has effect.
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectOnlySecondSubframe));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectOnlySecondSubframe);

  // Expect 2 activation UKMS, one each for blocked subframes "one" and "three",
  // x3 loads.
  ExpectFpfActivatedUkms(test_ukm_recorder, 6u,
                         /*is_dry_run=*/false);

  // Check that no exception UKMs are logged.
  ExpectNoFpfExceptionUkms(test_ukm_recorder);
}

IN_PROC_BROWSER_TEST_F(
    FingerprintingProtectionFilterTrackingProtectionSettingBrowserTest,
    SubframeDocumentLoadFilteringInIncognito) {
  // TODO(https://crbug.com/358371545): Test console messaging for subframe
  // blocking once its implementation is resolved.
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  // Enable FPP in TrackingProtectionSettings.
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kFingerprintingProtectionEnabled, true);

  // Close normal browser and switch the test's browser instance to an incognito
  // instance.
  BrowserWindowInterface* const incognito =
      CreateIncognitoBrowser(browser()->profile());
  CloseBrowserSynchronously(browser());
  SetBrowser(incognito);
  ASSERT_EQ(browser(), incognito);

  GURL url(GetTestUrl(kMultiPlatformTestFrameSetPath));

  // Disallow loading child frame documents that in turn would end up
  // loading included_script.js, unless the document is loaded from an allowed
  // (not in the blocklist) domain. This enables the third part of this test
  // disallowing a load only after the first redirect.
  auto allowed_substring =
      subresource_filter::testing::CreateAllowlistSubstringRule(
          embedded_test_server()->GetURL(kAllowedDomain, "/").spec());
  auto disallowed_suffix = subresource_filter::testing::CreateSuffixRule(
      "/frame_with_included_script.html");
  ASSERT_NO_FATAL_FAILURE(SetRulesetWithRules(
      {std::move(disallowed_suffix), std::move(allowed_substring)}));

  ASSERT_TRUE(NavigateToDestination(url));
  NavigateSubframesToCrossOriginSite();

  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectOnlySecondSubframe));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectOnlySecondSubframe);

  // Now navigate the first subframe to an allowed URL and ensure that the load
  // successfully commits and the frame gets restored (no longer collapsed).
  GURL allowed_subdocument_url(
      GetCrossSiteTestUrl("/frame_with_allowed_script.html"));
  NavigateFrame(kSubframeNames[0], allowed_subdocument_url);

  const std::vector<bool> kExpectFirstAndSecondSubframe{true, true, false};
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectFirstAndSecondSubframe));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectFirstAndSecondSubframe);

  // Navigate the first subframe to a document that does not load the probe JS.
  GURL allowed_empty_subdocument_url(
      GetCrossSiteTestUrl("/frame_with_no_subresources.html"));
  NavigateFrame(kSubframeNames[0], allowed_empty_subdocument_url);

  // Finally, navigate the first subframe to an allowed URL that redirects to a
  // disallowed URL, and verify that the navigation gets blocked and the frame
  // collapsed.
  GURL disallowed_subdocument_url(
      GetCrossSiteTestUrl("/frame_with_included_script.html"));
  GURL redirect_to_disallowed_subdocument_url(embedded_test_server()->GetURL(
      kAllowedDomain, "/server-redirect?" + disallowed_subdocument_url.spec()));
  NavigateFrame(kSubframeNames[0], redirect_to_disallowed_subdocument_url);

  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectOnlySecondSubframe));

  content::RenderFrameHost* frame = FindFrameByName(kSubframeNames[0]);
  ASSERT_TRUE(frame);
  const auto last_committed_url = frame->GetLastCommittedURL();
  EXPECT_EQ(last_committed_url, disallowed_subdocument_url);

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
    FingerprintingProtectionFilterTrackingProtectionSettingBrowserTest,
    NoFilteringInNonIncognito) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  // Enable FPP in TrackingProtectionSettings.
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kFingerprintingProtectionEnabled, true);

  GURL url(GetTestUrl(kMultiPlatformTestFrameSetPath));

  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithSubstring("included_script.html"));
  ASSERT_TRUE(NavigateToDestination(url));
  NavigateMultiFrameSubframesAndLoad3pScripts();

  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectAllSubframes));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectAllSubframes);

  // No filtering => no UKMs logged.
  const auto& entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::FingerprintingProtection::kEntryName);
  EXPECT_TRUE(entries.empty());

  // Expect no activation UMAs since filtering objects should not be created
  // outside of incognito.
  histogram_tester.ExpectTotalCount(ActivationDecisionHistogramName, 0);
  histogram_tester.ExpectTotalCount(ActivationLevelHistogramName, 0);
}

IN_PROC_BROWSER_TEST_F(
    FingerprintingProtectionFilterTrackingProtectionSettingBrowserTest,
    FilteringBehaviorChangesWhenSettingToggled) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  // Close normal browser and switch the test's browser instance to an incognito
  // instance.
  BrowserWindowInterface* const incognito =
      CreateIncognitoBrowser(browser()->profile());
  CloseBrowserSynchronously(browser());
  SetBrowser(incognito);
  ASSERT_EQ(browser(), incognito);

  // Disable FPP in TrackingProtectionSettings.
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kFingerprintingProtectionEnabled, false);

  GURL url(GetTestUrl(kMultiPlatformTestFrameSetPath));

  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithSubstring("included_script.html"));
  ASSERT_TRUE(NavigateToDestination(url));
  NavigateSubframesToCrossOriginSite();

  // Filtering off
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectAllSubframes));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectAllSubframes);

  // Enable FPP in TrackingProtectionSettings.
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kFingerprintingProtectionEnabled, true);

  // Refresh
  ASSERT_TRUE(NavigateToDestination(url));
  NavigateSubframesToCrossOriginSite();

  // Filtering on
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectOnlySecondSubframe));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectOnlySecondSubframe);

  // Disable FPP in TrackingProtectionSettings.
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kFingerprintingProtectionEnabled, false);

  // Refresh
  ASSERT_TRUE(NavigateToDestination(url));
  NavigateSubframesToCrossOriginSite();

  // Filtering off
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectAllSubframes));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectAllSubframes);

  // Enable FPP in TrackingProtectionSettings.
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kFingerprintingProtectionEnabled, true);

  // Refresh
  ASSERT_TRUE(NavigateToDestination(url));
  NavigateSubframesToCrossOriginSite();

  // Filtering on
  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectOnlySecondSubframe));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectOnlySecondSubframe);
}

// Filtering should work outside of incognito if the corresponding flag is
// enabled, even if it is controlled via Tracking Protection settings in
// incognito.
IN_PROC_BROWSER_TEST_F(
    FingerprintingProtectionFilterTrackingProtectionSettingAndNonIncognitoFilteringBrowserTest,
    FilteringInNonIncognito) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  // Enable FPP in TrackingProtectionSettings.
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kFingerprintingProtectionEnabled, true);

  GURL url(GetTestUrl(kMultiPlatformTestFrameSetPath));

  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithSubstring("included_script.html"));
  ASSERT_TRUE(NavigateToDestination(url));
  NavigateMultiFrameSubframesAndLoad3pScripts();

  ASSERT_NO_FATAL_FAILURE(ExpectParsedScriptElementLoadedStatusInFrames(
      kSubframeNames, kExpectAllSubframes));
  ExpectFramesIncludedInLayout(kSubframeNames, kExpectAllSubframes);

  // Expect enabled UMAs.
  histogram_tester.ExpectBucketCount(
      ActivationDecisionHistogramName,
      subresource_filter::ActivationDecision::ACTIVATED, 1);
  histogram_tester.ExpectBucketCount(
      ActivationLevelHistogramName,
      subresource_filter::mojom::ActivationLevel::kEnabled, 1);
}

#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace fingerprinting_protection_filter
