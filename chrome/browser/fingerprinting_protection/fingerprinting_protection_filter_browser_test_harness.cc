// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fingerprinting_protection/fingerprinting_protection_filter_browser_test_harness.h"

#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_features.h"
#include "components/subresource_filter/content/browser/test_ruleset_publisher.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

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
  ASSERT_TRUE(embedded_test_server()->Start());
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

// ======= FingerprintingProtectionFilterEnabledInIncognitoBrowserTest ========

FingerprintingProtectionFilterEnabledInIncognitoBrowserTest::
    FingerprintingProtectionFilterEnabledInIncognitoBrowserTest() {
  scoped_feature_list_.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {{features::kEnableFingerprintingProtectionFilterInIncognito,
        {{"activation_level", "enabled"},
         {"performance_measurement_rate", "0.0"}}}},
      /*disabled_features=*/{});
}

FingerprintingProtectionFilterEnabledInIncognitoBrowserTest::
    ~FingerprintingProtectionFilterEnabledInIncognitoBrowserTest() = default;

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

}  // namespace fingerprinting_protection_filter
