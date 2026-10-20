// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/variations/aw_entropy_providers.h"

#include <memory>
#include <string>

#include "components/variations/entropy_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace android_webview {

namespace {

// Mock entropy value for low entropy (nonembedded) source.
const uint32_t kNonembeddedLowEntropySource = 1234;
// Mock entropy value for standard low entropy source.
const uint32_t kStandardLowEntropySource = 5678;

// A study name only used for testing.
const char kTestDenylistedStudyName[] = "MyTestStudy";

}  // namespace

class AwEntropyProvidersTest : public ::testing::Test {
 public:
  AwEntropyProvidersTest()
      : standard_providers_(std::make_unique<variations::EntropyProviders>(
            /*high_entropy_source=*/"",
            /*low_entropy_source=*/
            variations::ValueInRange{kStandardLowEntropySource, 8000},
            /*limited_entropy_source=*/"")) {}

 protected:
  const std::set<std::string_view> kTestDenylist_{kTestDenylistedStudyName};
  std::unique_ptr<variations::EntropyProviders> standard_providers_;
};

TEST_F(AwEntropyProvidersTest, TestNonembeddedEntropyProvider_DenylistedStudy) {
  AwEntropyProviders providers(
      std::move(standard_providers_), kNonembeddedLowEntropySource,
      std::make_unique<const std::set<std::string_view>>(kTestDenylist_));
  const auto& low_entropy_provider = providers.low_entropy();

  // This study name is defined only in this test file and is in the denylist.
  std::string trial_name = kTestDenylistedStudyName;

  // Standard source should be used for denylisted study.
  variations::NormalizedMurmurHashEntropyProvider nonembedded_provider(
      {kNonembeddedLowEntropySource, 8000});
  variations::NormalizedMurmurHashEntropyProvider standard_provider(
      {kStandardLowEntropySource, 8000});

  double expected = standard_provider.GetEntropyForTrial(trial_name, 0);
  double actual = low_entropy_provider.GetEntropyForTrial(trial_name, 0);

  EXPECT_EQ(expected, actual);
  EXPECT_NE(nonembedded_provider.GetEntropyForTrial(trial_name, 0), actual);
}

TEST_F(AwEntropyProvidersTest,
       TestNonembeddedEntropyProvider_NonDenylistedStudy) {
  AwEntropyProviders providers(
      std::move(standard_providers_), kNonembeddedLowEntropySource,
      std::make_unique<const std::set<std::string_view>>(kTestDenylist_));
  const auto& low_entropy_provider = providers.low_entropy();

  // "SomeRandomTrial" is NOT in the denylist.
  std::string trial_name = "SomeRandomTrial";

  // Nonembedded source should be used for non-denylisted study.
  variations::NormalizedMurmurHashEntropyProvider nonembedded_provider(
      {kNonembeddedLowEntropySource, 8000});
  variations::NormalizedMurmurHashEntropyProvider standard_provider(
      {kStandardLowEntropySource, 8000});

  double expected = nonembedded_provider.GetEntropyForTrial(trial_name, 0);
  double actual = low_entropy_provider.GetEntropyForTrial(trial_name, 0);

  EXPECT_EQ(expected, actual);
  EXPECT_NE(standard_provider.GetEntropyForTrial(trial_name, 0), actual);
}

TEST_F(AwEntropyProvidersTest, TestDefaultEntropy) {
  // Verify default_entropy() delegates to low_entropy() (which is the
  // nonembedded wrapper).
  AwEntropyProviders providers(
      std::move(standard_providers_), kNonembeddedLowEntropySource,
      std::make_unique<const std::set<std::string_view>>(kTestDenylist_));

  variations::NormalizedMurmurHashEntropyProvider nonembedded_provider(
      {kNonembeddedLowEntropySource, 8000});
  variations::NormalizedMurmurHashEntropyProvider standard_provider(
      {kStandardLowEntropySource, 8000});

  // "SomeRandomTrial" is NOT in the denylist and should use nonembedded source
  // via default_entropy too.
  std::string non_denylisted_trial = "SomeRandomTrial";
  EXPECT_EQ(
      nonembedded_provider.GetEntropyForTrial(non_denylisted_trial, 0),
      providers.default_entropy().GetEntropyForTrial(non_denylisted_trial, 0));

  // Denylisted study should use standard source via default_entropy.
  std::string denylisted_trial = kTestDenylistedStudyName;
  EXPECT_EQ(
      standard_provider.GetEntropyForTrial(denylisted_trial, 0),
      providers.default_entropy().GetEntropyForTrial(denylisted_trial, 0));
}

TEST_F(AwEntropyProvidersTest, TestLimitedEntropy) {
  const std::string kLimitedEntropySource = "0123456789ABCDEF0123456789ABCDEF";
  AwEntropyProviders providers(
      std::move(standard_providers_), kNonembeddedLowEntropySource,
      std::make_unique<const std::set<std::string_view>>(kTestDenylist_),
      kLimitedEntropySource);
  EXPECT_TRUE(providers.has_limited_entropy());

  variations::SHA1EntropyProvider expected_provider(kLimitedEntropySource);
  const std::string trial_name = "LimitedEntropyTrial";
  EXPECT_EQ(providers.limited_entropy().GetEntropyForTrial(trial_name, 0),
            expected_provider.GetEntropyForTrial(trial_name, 0));
}

TEST_F(AwEntropyProvidersTest, TestNoLimitedEntropy) {
  AwEntropyProviders providers(
      std::move(standard_providers_), kNonembeddedLowEntropySource,
      std::make_unique<const std::set<std::string_view>>(kTestDenylist_));
  EXPECT_FALSE(providers.has_limited_entropy());
}

}  // namespace android_webview
