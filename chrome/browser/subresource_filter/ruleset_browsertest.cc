// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_features.h"
#include "components/subresource_filter/content/shared/browser/ruleset_service.h"
#include "components/subresource_filter/core/browser/async_document_subresource_filter.h"
#include "components/subresource_filter/core/browser/async_document_subresource_filter_test_utils.h"
#include "components/subresource_filter/core/common/common_features.h"
#include "components/subresource_filter/core/common/indexed_ruleset.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter {

namespace {

const mojom::ActivationState kDisabled;

RulesetVerificationStatus GetRulesetVerification() {
  RulesetService* service =
      g_browser_process->subresource_filter_ruleset_service();
  VerifiedRulesetDealer::Handle* dealer_handle = service->GetRulesetDealer();

  auto callback_method = [](base::OnceClosure quit_closure,
                            RulesetVerificationStatus* status,
                            VerifiedRulesetDealer* verified_dealer) {
    *status = verified_dealer->status();
    std::move(quit_closure).Run();
  };

  RulesetVerificationStatus status;
  base::RunLoop run_loop;
  auto callback =
      base::BindRepeating(callback_method, run_loop.QuitClosure(), &status);

  dealer_handle->GetDealerAsync(callback);
  run_loop.Run();
  return status;
}

const char kIndexedRulesetVerifyHistogram[] =
    "SubresourceFilter.IndexRuleset.Verify.Status";

}  // namespace

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       RulesetVerified_Activation) {
  base::HistogramTester histogram_tester;
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  RulesetService* service =
      g_browser_process->subresource_filter_ruleset_service();
  ASSERT_TRUE(service->GetRulesetDealer());
  auto ruleset_handle =
      std::make_unique<VerifiedRuleset::Handle>(service->GetRulesetDealer());
  AsyncDocumentSubresourceFilter::InitializationParams params(
      GURL("https://example.com/"), mojom::ActivationLevel::kEnabled, false);

  testing::TestActivationStateCallbackReceiver receiver;
  AsyncDocumentSubresourceFilter filter(ruleset_handle.get(), std::move(params),
                                        receiver.GetCallback());
  receiver.WaitForActivationDecision();
  mojom::ActivationState expected_state;
  expected_state.activation_level = mojom::ActivationLevel::kEnabled;
  receiver.ExpectReceivedOnce(expected_state);
  histogram_tester.ExpectUniqueSample(kIndexedRulesetVerifyHistogram,
                                      VerifyStatus::kPassValidChecksum, 1);
}

// TODO(ericrobinson): Add a test using a PRE_ phase that corrupts the ruleset
// on disk to test something closer to an actual execution path for checksum.

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest, NoRuleset_NoActivation) {
  base::HistogramTester histogram_tester;
  // Do not set the ruleset, which results in an invalid ruleset.
  RulesetService* service =
      g_browser_process->subresource_filter_ruleset_service();
  ASSERT_TRUE(service->GetRulesetDealer());
  auto ruleset_handle =
      std::make_unique<VerifiedRuleset::Handle>(service->GetRulesetDealer());
  AsyncDocumentSubresourceFilter::InitializationParams params(
      GURL("https://example.com/"), mojom::ActivationLevel::kEnabled, false);

  testing::TestActivationStateCallbackReceiver receiver;
  AsyncDocumentSubresourceFilter filter(ruleset_handle.get(), std::move(params),
                                        receiver.GetCallback());
  receiver.WaitForActivationDecision();
  receiver.ExpectReceivedOnce(kDisabled);
  histogram_tester.ExpectTotalCount(kIndexedRulesetVerifyHistogram, 0);
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest, InvalidRuleset_Checksum) {
  base::HistogramTester histogram_tester;
  const char kTestRulesetSuffix[] = "foo";
  const int kNumberOfRules = 500;
  TestRulesetCreator ruleset_creator;
  TestRulesetPair test_ruleset_pair;
  ASSERT_NO_FATAL_FAILURE(
      ruleset_creator.CreateRulesetToDisallowURLsWithManySuffixes(
          kTestRulesetSuffix, kNumberOfRules, &test_ruleset_pair));
  RulesetService* service =
      g_browser_process->subresource_filter_ruleset_service();

  // Publish the good ruleset.
  TestRulesetPublisher publisher(service);
  publisher.SetRuleset(test_ruleset_pair.unindexed);

  // Now corrupt it by flipping one entry.  This can only be detected
  // via the checksum, and not the Flatbuffer Verifier.  This was determined
  // at random by flipping elements until this test failed, then adding
  // the checksum code and ensuring it passed.
  testing::TestRuleset::CorruptByFilling(test_ruleset_pair.indexed, 28246,
                                         28247, 32);
  OpenAndPublishRuleset(service, test_ruleset_pair.indexed.path);
  ASSERT_TRUE(service->GetRulesetDealer());

  auto ruleset_handle =
      std::make_unique<VerifiedRuleset::Handle>(service->GetRulesetDealer());
  AsyncDocumentSubresourceFilter::InitializationParams params(
      GURL("https://example.com/"), mojom::ActivationLevel::kEnabled, false);

  testing::TestActivationStateCallbackReceiver receiver;
  AsyncDocumentSubresourceFilter filter(ruleset_handle.get(), std::move(params),
                                        receiver.GetCallback());
  receiver.WaitForActivationDecision();
  receiver.ExpectReceivedOnce(kDisabled);
  RulesetVerificationStatus dealer_status = GetRulesetVerification();
  EXPECT_EQ(RulesetVerificationStatus::kCorrupt, dealer_status);
  // If AdTagging is enabled, then the initial SetRuleset will trigger
  // a call to Verify.  Make sure we see that and the later failure.
  if (base::FeatureList::IsEnabled(kAdTagging)) {
    histogram_tester.ExpectBucketCount(kIndexedRulesetVerifyHistogram,
                                       VerifyStatus::kPassValidChecksum, 1);
    histogram_tester.ExpectBucketCount(kIndexedRulesetVerifyHistogram,
                                       VerifyStatus::kChecksumFailVerifierPass,
                                       1);
    histogram_tester.ExpectTotalCount(kIndexedRulesetVerifyHistogram, 2);
  } else {
    // Otherwise we see only a single Verify when the new ruleset is accessed,
    // and that should be a failure.
    histogram_tester.ExpectUniqueSample(kIndexedRulesetVerifyHistogram,
                                        VerifyStatus::kChecksumFailVerifierPass,
                                        1);
  }
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       InvalidRuleset_NoActivation) {
  base::HistogramTester histogram_tester;
  const char kTestRulesetSuffix[] = "foo";
  const int kNumberOfRules = 500;
  TestRulesetCreator ruleset_creator;
  TestRulesetPair test_ruleset_pair;
  ASSERT_NO_FATAL_FAILURE(
      ruleset_creator.CreateRulesetToDisallowURLsWithManySuffixes(
          kTestRulesetSuffix, kNumberOfRules, &test_ruleset_pair));
  testing::TestRuleset::CorruptByTruncating(test_ruleset_pair.indexed, 123);

  // Just publish the corrupt indexed file directly, to simulate it being
  // corrupt on startup.
  RulesetService* service =
      g_browser_process->subresource_filter_ruleset_service();
  ASSERT_TRUE(service->GetRulesetDealer());
  OpenAndPublishRuleset(service, test_ruleset_pair.indexed.path);

  auto ruleset_handle =
      std::make_unique<VerifiedRuleset::Handle>(service->GetRulesetDealer());
  AsyncDocumentSubresourceFilter::InitializationParams params(
      GURL("https://example.com/"), mojom::ActivationLevel::kEnabled, false);

  testing::TestActivationStateCallbackReceiver receiver;
  AsyncDocumentSubresourceFilter filter(ruleset_handle.get(), std::move(params),
                                        receiver.GetCallback());
  receiver.WaitForActivationDecision();
  receiver.ExpectReceivedOnce(kDisabled);
  RulesetVerificationStatus dealer_status = GetRulesetVerification();
  EXPECT_EQ(RulesetVerificationStatus::kCorrupt, dealer_status);
  histogram_tester.ExpectUniqueSample(kIndexedRulesetVerifyHistogram,
                                      VerifyStatus::kVerifierFailChecksumZero,
                                      1);
}

class SubresourceFilterBrowserTestWithoutAdTagging
    : public SubresourceFilterBrowserTest {
 public:
  SubresourceFilterBrowserTestWithoutAdTagging() {
    feature_list_.InitAndDisableFeature(subresource_filter::kAdTagging);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTestWithoutAdTagging,
                       LazyRulesetValidation) {
  // The ruleset shouldn't be validated until it's used, unless ad tagging is
  // enabled.
  SetRulesetToDisallowURLsWithPathSuffix("included_script.js");
  RulesetVerificationStatus dealer_status = GetRulesetVerification();
  EXPECT_EQ(RulesetVerificationStatus::kNotVerified, dealer_status);
}

class SubresourceFilterBrowserTestWithAdTagging
    : public SubresourceFilterBrowserTest {
 public:
  SubresourceFilterBrowserTestWithAdTagging() {
    feature_list_.InitAndEnableFeature(subresource_filter::kAdTagging);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTestWithAdTagging,
                       AdsTaggingImmediateRulesetValidation) {
  // When Ads Tagging is enabled, the ruleset should be validated as soon as
  // it's published.
  SetRulesetToDisallowURLsWithPathSuffix("included_script.js");
  RulesetVerificationStatus dealer_status = GetRulesetVerification();
  EXPECT_EQ(RulesetVerificationStatus::kIntact, dealer_status);
}

}  // namespace subresource_filter
