// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fingerprinting_protection/fingerprinting_protection_filter_browser_test_harness.h"

#include <cstdint>
#include <string>
#include <vector>

#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_features.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/tracking_protection_prefs.h"
#include "components/subresource_filter/content/browser/test_ruleset_publisher.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace fingerprinting_protection_filter {

// Cross-origin base url definitions.
const std::string& embedder_base = "embedder.test";
const std::string& cross_site_base = "cross-site.test";

FingerprintingProtectionFilterBrowserTest::
    FingerprintingProtectionFilterBrowserTest() {
  scoped_feature_list_.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {{features::kEnableFingerprintingProtectionFilter,
        {{"activation_level", "enabled"},
         {"performance_measurement_rate", "0.0"}}}},
      /*disabled_features=*/{});
}

FingerprintingProtectionFilterBrowserTest::
    ~FingerprintingProtectionFilterBrowserTest() = default;

GURL FingerprintingProtectionFilterBrowserTest::GetTestUrl(
    const std::string& relative_url) const {
  return embedded_test_server()->GetURL(embedder_base, relative_url);
}

GURL FingerprintingProtectionFilterBrowserTest::GetCrossSiteTestUrl(
    const std::string& relative_url) const {
  return embedded_test_server()->GetURL(cross_site_base, relative_url);
}

void FingerprintingProtectionFilterBrowserTest::
    NavigateSubframesToCrossOriginSite() {
  NavigateFrame(kSubframeNames[0],
                GetCrossSiteTestUrl("/frame_with_included_script.html"));
  NavigateFrame(kSubframeNames[1],
                GetCrossSiteTestUrl("/frame_with_allowed_script.html"));
  NavigateFrame(kSubframeNames[2],
                GetCrossSiteTestUrl("/frame_with_included_script.html"));
}
void FingerprintingProtectionFilterBrowserTest::UpdateIncludedScriptSource(
    const GURL& url) {
  ASSERT_TRUE(content::ExecJs(web_contents()->GetPrimaryMainFrame(),
                              (content::JsReplace(
                                  R"(script_element = document.querySelector(
      'script[src=\"included_script.js\"]');
      script_element = '$1';)",
                                  url))));
}

void FingerprintingProtectionFilterBrowserTest::SetUpOnMainThread() {
  SubresourceFilterSharedBrowserTest::SetUpOnMainThread();
  content::SetupCrossSiteRedirector(embedded_test_server());
  embedded_test_server()->ServeFilesFromSourceDirectory(
      "chrome/test/data/fingerprinting_protection");
  host_resolver()->AddRule(embedder_base, "127.0.0.1");
  host_resolver()->AddRule(cross_site_base, "127.0.0.1");
  // Allow derived classes to start the server on their own.
}

void FingerprintingProtectionFilterBrowserTest::
    SetRulesetToDisallowURLsWithPathSuffix(const std::string& suffix) {
  subresource_filter::testing::TestRulesetPair test_ruleset_pair;
  ruleset_creator_.CreateRulesetToDisallowURLsWithPathSuffix(
      suffix, &test_ruleset_pair);

  subresource_filter::testing::TestRulesetPublisher test_ruleset_publisher(
      g_browser_process->fingerprinting_protection_ruleset_service());
  ASSERT_NO_FATAL_FAILURE(
      test_ruleset_publisher.SetRuleset(test_ruleset_pair.unindexed));
}

void FingerprintingProtectionFilterBrowserTest::
    SetRulesetToDisallowURLsWithSubstring(const std::string& substring) {
  subresource_filter::testing::TestRulesetPair test_ruleset_pair;
  ruleset_creator_.CreateRulesetToDisallowURLWithSubstrings({substring},
                                                            &test_ruleset_pair);

  subresource_filter::testing::TestRulesetPublisher test_ruleset_publisher(
      g_browser_process->fingerprinting_protection_ruleset_service());
  ASSERT_NO_FATAL_FAILURE(
      test_ruleset_publisher.SetRuleset(test_ruleset_pair.unindexed));
}

void FingerprintingProtectionFilterBrowserTest::SetRulesetWithRules(
    const std::vector<proto::UrlRule>& rules) {
  subresource_filter::testing::TestRulesetPair test_ruleset_pair;
  ruleset_creator_.CreateRulesetWithRules(rules, &test_ruleset_pair);

  TestRulesetPublisher test_ruleset_publisher(
      g_browser_process->fingerprinting_protection_ruleset_service());
  ASSERT_NO_FATAL_FAILURE(
      test_ruleset_publisher.SetRuleset(test_ruleset_pair.unindexed));
}

void FingerprintingProtectionFilterBrowserTest::AssertUrlContained(
    const GURL& full_url,
    const GURL& sub_url) {
  ASSERT_NE(full_url.spec().find(std::string(sub_url.spec())),
            std::string::npos);
}

bool FingerprintingProtectionFilterBrowserTest::NavigateToDestination(
    const GURL& url) {
#if !BUILDFLAG(IS_ANDROID)
  return ui_test_utils::NavigateToURL(browser(), url);
#else
  return content::NavigateToURL(chrome_test_utils::GetActiveWebContents(this),
                                url);
#endif  // !BUILDFLAG(IS_ANDROID)
}

void FingerprintingProtectionFilterBrowserTest::ExpectFpfActivatedUkms(
    const ukm::TestAutoSetUkmRecorder& recorder,
    const unsigned long& expected_count,
    bool is_dry_run) {
  const auto& entries = recorder.GetEntriesByName(
      ukm::builders::FingerprintingProtection::kEntryName);
  // Expect the size of the existing entries list matches the expected
  // `expected_count`.
  EXPECT_EQ(expected_count, entries.size());
  for (const ukm::mojom::UkmEntry* entry : entries) {
    // Expect the value of the ActivationDecision metric is `ACTIVATED`.
    EXPECT_EQ(
        *recorder.GetEntryMetric(
            entry,
            ukm::builders::FingerprintingProtection::kActivationDecisionName),
        static_cast<int64_t>(
            subresource_filter::ActivationDecision::ACTIVATED));
    // Expect the DryRun metric logged matches the expected `is_dry_run` value.
    EXPECT_EQ(recorder.EntryHasMetric(
                  entry, ukm::builders::FingerprintingProtection::kDryRunName),
              is_dry_run);
  }
}

void FingerprintingProtectionFilterBrowserTest::ExpectNoFpfExceptionUkms(
    const ukm::TestAutoSetUkmRecorder& recorder) {
  EXPECT_TRUE(
      recorder
          .GetEntriesByName(
              ukm::builders::FingerprintingProtectionException::kEntryName)
          .size() == 0u);
}

void FingerprintingProtectionFilterBrowserTest::ExpectFpfExceptionUkms(
    const ukm::TestAutoSetUkmRecorder& recorder,
    const unsigned long& expected_count,
    const int64_t& expected_source) {
  const auto& entries = recorder.GetEntriesByName(
      ukm::builders::FingerprintingProtectionException::kEntryName);
  EXPECT_EQ(expected_count, entries.size());
  for (const ukm::mojom::UkmEntry* entry : entries) {
    recorder.ExpectEntryMetric(
        entry, ukm::builders::FingerprintingProtectionException::kSourceName,
        expected_source);
  }
}

void FingerprintingProtectionFilterBrowserTest::
    NavigateMultiFrameSubframesAndLoad3pScripts() {
  NavigateFrame(kSubframeNames[0],
                GetTestUrl("/frame_with_included_script.html"));
  UpdateIncludedScriptSource(GetCrossSiteTestUrl("/included_script.js"));
  NavigateFrame(kSubframeNames[1],
                GetTestUrl("/frame_with_allowed_script.html"));
  UpdateIncludedScriptSource(
      GetCrossSiteTestUrl("/included_allowed_script.js"));
  NavigateFrame(kSubframeNames[2],
                GetTestUrl("/frame_with_included_script.html"));
  UpdateIncludedScriptSource(GetCrossSiteTestUrl("/included_script.js"));
}

// ============= FingerprintingProtectionFilterDryRunBrowserTest ==============

FingerprintingProtectionFilterDryRunBrowserTest::
    FingerprintingProtectionFilterDryRunBrowserTest() {
  scoped_feature_list_.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {{features::kEnableFingerprintingProtectionFilter,
        {{"activation_level", "dry_run"},
         {"performance_measurement_rate", "0.0"}}}},
      /*disabled_features=*/{});
}

FingerprintingProtectionFilterDryRunBrowserTest::
    ~FingerprintingProtectionFilterDryRunBrowserTest() = default;

void FingerprintingProtectionFilterDryRunBrowserTest::SetUpOnMainThread() {
  FingerprintingProtectionFilterBrowserTest::SetUpOnMainThread();
  ASSERT_TRUE(embedded_test_server()->Start());
}

// ======= FingerprintingProtectionFilterEnabledInIncognitoBrowserTest ========

FingerprintingProtectionFilterEnabledInIncognitoBrowserTest::
    FingerprintingProtectionFilterEnabledInIncognitoBrowserTest() {
  scoped_feature_list_.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {{features::kEnableFingerprintingProtectionFilterInIncognito,
        {{"performance_measurement_rate", "0.0"}}}},
      /*disabled_features=*/{});
}

FingerprintingProtectionFilterEnabledInIncognitoBrowserTest::
    ~FingerprintingProtectionFilterEnabledInIncognitoBrowserTest() = default;

void FingerprintingProtectionFilterEnabledInIncognitoBrowserTest::
    SetUpOnMainThread() {
  FingerprintingProtectionFilterBrowserTest::SetUpOnMainThread();
  ASSERT_TRUE(embedded_test_server()->Start());
}

// ============= FingerprintingProtectionFilterDisabledBrowserTest ============

FingerprintingProtectionFilterDisabledBrowserTest::
    FingerprintingProtectionFilterDisabledBrowserTest() {
  scoped_feature_list_.InitWithFeatureStates(
      {{fingerprinting_protection_filter::features::
            kEnableFingerprintingProtectionFilterInIncognito,
        false},
       {fingerprinting_protection_filter::features::
            kEnableFingerprintingProtectionFilter,
        false}});
}

FingerprintingProtectionFilterDisabledBrowserTest::
    ~FingerprintingProtectionFilterDisabledBrowserTest() = default;

void FingerprintingProtectionFilterDisabledBrowserTest::SetUpOnMainThread() {
  FingerprintingProtectionFilterBrowserTest::SetUpOnMainThread();
  ASSERT_TRUE(embedded_test_server()->Start());
}

// ==== FingerprintingProtectionFilterRefreshHeuristicExceptionBrowserTest ====

FingerprintingProtectionFilterRefreshHeuristicExceptionBrowserTest::
    FingerprintingProtectionFilterRefreshHeuristicExceptionBrowserTest() {
  // Enable refresh heuristic after 2 refreshes in both regular and incognito.
  scoped_feature_list_.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {{features::kEnableFingerprintingProtectionFilter,
        {{features::kRefreshHeuristicExceptionThresholdParam, "2"}}},
       {features::kEnableFingerprintingProtectionFilterInIncognito,
        {{features::kRefreshHeuristicExceptionThresholdParam, "2"}}}},
      /*disabled_features=*/{});
}

FingerprintingProtectionFilterRefreshHeuristicExceptionBrowserTest::
    ~FingerprintingProtectionFilterRefreshHeuristicExceptionBrowserTest() =
        default;

void FingerprintingProtectionFilterRefreshHeuristicExceptionBrowserTest::
    SetUpOnMainThread() {
  FingerprintingProtectionFilterBrowserTest::SetUpOnMainThread();
  ASSERT_TRUE(embedded_test_server()->Start());

  // These tests depend on eTLD+1, so we need the browser to navigate to
  // a URL with domain name, not an IP address - it breaks if the URL is
  // 127.0.0.1 as it is by default in these tests.
  // Resolve "google.test" to 127.0.0.1 so that these tests can navigate to
  // "google.test" and work as desired.
  host_resolver()->AddRule("google.test",
                           embedded_test_server()->base_url().host_piece());
}

// ==== FingerprintingProtectionFilterTrackingProtectionSettingBrowserTest ====

FingerprintingProtectionFilterTrackingProtectionSettingBrowserTest::
    FingerprintingProtectionFilterTrackingProtectionSettingBrowserTest() {
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/

      {// Enable FPP setting in Tracking Protection UX.
       // This flag isn't used together with
       // `EnableFingerprintingProtectionFilter(InIncognito)`.
       privacy_sandbox::kFingerprintingProtectionUx},
      /*disabled_features=*/{});
}

FingerprintingProtectionFilterTrackingProtectionSettingBrowserTest::
    ~FingerprintingProtectionFilterTrackingProtectionSettingBrowserTest() =
        default;

void FingerprintingProtectionFilterTrackingProtectionSettingBrowserTest::
    SetUpOnMainThread() {
  FingerprintingProtectionFilterBrowserTest::SetUpOnMainThread();
  ASSERT_TRUE(embedded_test_server()->Start());
}

}  // namespace fingerprinting_protection_filter
