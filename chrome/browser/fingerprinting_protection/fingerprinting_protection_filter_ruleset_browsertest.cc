// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process.h"
#include "chrome/browser/fingerprinting_protection/fingerprinting_protection_filter_browser_test_harness.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_features.h"
#include "components/subresource_filter/content/shared/browser/ruleset_service.h"
#include "components/subresource_filter/core/browser/async_document_subresource_filter.h"
#include "components/subresource_filter/core/browser/async_document_subresource_filter_test_utils.h"
#include "components/subresource_filter/core/common/indexed_ruleset.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace fingerprinting_protection_filter {

const char kIndexRulesetVerifyHistogram[] =
    "FingerprintingProtection.IndexRuleset.Verify.Status";
const char kIndexRulesetNumUnsupportedRulesHistogram[] =
    "FingerprintingProtection.IndexRuleset.NumUnsupportedRules";
const char kIndexRulesetVerifyWallDurationHistogram[] =
    "FingerprintingProtection.IndexRuleset.Verify2.WallDuration";
const char kIndexRulesetCPUDurationHistogram[] =
    "FingerprintingProtection.IndexRuleset.CPUDuration";
const char kIndexRulesetWallDurationHistogram[] =
    "FingerprintingProtection.IndexRuleset.WallDuration";
const char kIndexRulesetWriteRulesetResultHistogram[] =
    "FingerprintingProtection.WriteRuleset.Result";

IN_PROC_BROWSER_TEST_F(
    FingerprintingProtectionFilterDisabledBrowserTest,
    RulesetServiceNotCreated_DisabledFingerprintingProtectionFlag) {
  EXPECT_EQ(g_browser_process->fingerprinting_protection_ruleset_service(),
            nullptr);
}

IN_PROC_BROWSER_TEST_F(FingerprintingProtectionFilterDisabledBrowserTest,
                       RulesetServiceNotCreated_DisabledIncognitoFlag) {
  EXPECT_EQ(g_browser_process->fingerprinting_protection_ruleset_service(),
            nullptr);
}

IN_PROC_BROWSER_TEST_F(FingerprintingProtectionFilterBrowserTest,
                       RulesetServiceCreated) {
  subresource_filter::RulesetService* service =
      g_browser_process->fingerprinting_protection_ruleset_service();
  ASSERT_NE(service, nullptr);
  EXPECT_NE(service->GetRulesetDealer(), nullptr);
}

IN_PROC_BROWSER_TEST_F(FingerprintingProtectionFilterBrowserTest,
                       RulesetVerified_Activation) {
  base::HistogramTester histogram_tester;
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  subresource_filter::RulesetService* service =
      g_browser_process->fingerprinting_protection_ruleset_service();
  ASSERT_TRUE(service->GetRulesetDealer());
  auto ruleset_handle =
      std::make_unique<subresource_filter::VerifiedRuleset::Handle>(
          service->GetRulesetDealer());
  subresource_filter::AsyncDocumentSubresourceFilter::InitializationParams
      params(GURL("https://example.com/"),
             subresource_filter::mojom::ActivationLevel::kEnabled, false);

  subresource_filter::testing::TestActivationStateCallbackReceiver receiver;
  subresource_filter::AsyncDocumentSubresourceFilter filter(
      ruleset_handle.get(), std::move(params), receiver.GetCallback());
  receiver.WaitForActivationDecision();
  subresource_filter::mojom::ActivationState expected_state;
  expected_state.activation_level =
      subresource_filter::mojom::ActivationLevel::kEnabled;
  receiver.ExpectReceivedOnce(expected_state);
  histogram_tester.ExpectUniqueSample(
      kIndexRulesetVerifyHistogram,
      subresource_filter::VerifyStatus::kPassValidChecksum, 1);
  histogram_tester.ExpectTotalCount(kIndexRulesetNumUnsupportedRulesHistogram,
                                    1);
  histogram_tester.ExpectTotalCount(kIndexRulesetVerifyWallDurationHistogram,
                                    1);
  histogram_tester.ExpectTotalCount(kIndexRulesetCPUDurationHistogram, 1);
  histogram_tester.ExpectTotalCount(kIndexRulesetWallDurationHistogram, 1);
  histogram_tester.ExpectUniqueSample(
      kIndexRulesetWriteRulesetResultHistogram,
      subresource_filter::RulesetService::IndexAndWriteRulesetResult::SUCCESS,
      1);
}

IN_PROC_BROWSER_TEST_F(
    FingerprintingProtectionFilterEnabledInIncognitoBrowserTest,
    RulesetServiceCreated) {
  subresource_filter::RulesetService* service =
      g_browser_process->fingerprinting_protection_ruleset_service();
  ASSERT_NE(service, nullptr);
  EXPECT_NE(service->GetRulesetDealer(), nullptr);
}

IN_PROC_BROWSER_TEST_F(FingerprintingProtectionFilterDryRunBrowserTest,
                       RulesetServiceCreated) {
  // Ruleset still gets created in Dry Run mode
  subresource_filter::RulesetService* service =
      g_browser_process->fingerprinting_protection_ruleset_service();
  ASSERT_NE(service, nullptr);
  EXPECT_NE(service->GetRulesetDealer(), nullptr);
}

}  // namespace fingerprinting_protection_filter
