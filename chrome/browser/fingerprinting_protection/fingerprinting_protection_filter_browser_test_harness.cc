// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fingerprinting_protection/fingerprinting_protection_filter_browser_test_harness.h"

#include "chrome/test/base/chrome_test_utils.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_features.h"
#include "components/subresource_filter/content/browser/test_ruleset_publisher.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace fingerprinting_protection_filter {

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

void FingerprintingProtectionFilterBrowserTest::SetUpOnMainThread() {
  SubresourceFilterSharedBrowserTest::SetUpOnMainThread();
#if !BUILDFLAG(IS_ANDROID)
  embedded_test_server()->ServeFilesFromSourceDirectory(
      "components/test/data/subresource_filter");
#else
  embedded_test_server()->ServeFilesFromSourceDirectory(
      "chrome/test/data/fingerprinting_protection");
#endif  // !BUILDFLAG(IS_ANDROID)
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

}  // namespace fingerprinting_protection_filter
