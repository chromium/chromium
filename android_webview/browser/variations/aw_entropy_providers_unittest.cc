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
const char kTestAllowlistedStudyName[] = "MyTestStudy";

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
  const std::set<std::string_view> kTestAllowlist_{kTestAllowlistedStudyName};
  std::unique_ptr<variations::EntropyProviders> standard_providers_;
};

TEST_F(AwEntropyProvidersTest,
       TestNonembeddedEntropyProvider_AllowlistedStudy) {
  AwEntropyProviders providers(
      std::move(standard_providers_), kNonembeddedLowEntropySource,
      std::make_unique<const std::set<std::string_view>>(kTestAllowlist_));
  const auto& low_entropy_provider = providers.low_entropy();

  // This study name is defined only in this test file.
  std::string trial_name = kTestAllowlistedStudyName;

  // Nonembedded source should be used.
  // We can verify it differs from the standard source if we construct
  // a standard provider with the same seed.

  variations::NormalizedMurmurHashEntropyProvider nonembedded_provider(
      {kNonembeddedLowEntropySource, 8000});
  variations::NormalizedMurmurHashEntropyProvider standard_provider(
      {kStandardLowEntropySource, 8000});

  double expected = nonembedded_provider.GetEntropyForTrial(trial_name, 0);
  double actual = low_entropy_provider.GetEntropyForTrial(trial_name, 0);

  EXPECT_EQ(expected, actual);
  EXPECT_NE(standard_provider.GetEntropyForTrial(trial_name, 0), actual);
}

TEST_F(AwEntropyProvidersTest,
       TestNonembeddedEntropyProvider_NonAllowlistedStudy) {
  AwEntropyProviders providers(
      std::move(standard_providers_), kNonembeddedLowEntropySource,
      std::make_unique<const std::set<std::string_view>>(kTestAllowlist_));
  const auto& low_entropy_provider = providers.low_entropy();

  // "SomeRandomTrial" is NOT in the allowlist.
  std::string trial_name = "SomeRandomTrial";

  variations::NormalizedMurmurHashEntropyProvider standard_provider(
      {kStandardLowEntropySource, 8000});

  double expected = standard_provider.GetEntropyForTrial(trial_name, 0);
  double actual = low_entropy_provider.GetEntropyForTrial(trial_name, 0);

  EXPECT_EQ(expected, actual);
}

TEST_F(AwEntropyProvidersTest, TestDefaultEntropy) {
  // Verify default_entropy() delegates to low_entropy() (which is the
  // nonembedded wrapper).
  AwEntropyProviders providers(
      std::move(standard_providers_), kNonembeddedLowEntropySource,
      std::make_unique<const std::set<std::string_view>>(kTestAllowlist_));

  // "MyTestStudy" should use nonembedded source via
  // default_entropy too.
  std::string trial_name = kTestAllowlistedStudyName;

  variations::NormalizedMurmurHashEntropyProvider nonembedded_provider(
      {kNonembeddedLowEntropySource, 8000});

  double expected = nonembedded_provider.GetEntropyForTrial(trial_name, 0);
  double actual = providers.default_entropy().GetEntropyForTrial(trial_name, 0);

  EXPECT_EQ(expected, actual);
}

}  // namespace android_webview
