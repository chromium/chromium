// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fast_checkout/fast_checkout_capabilities_results_cache.h"

#include <array>
#include <vector>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/autofill/core/common/signatures.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

using autofill::FormSignature;

constexpr char kOrigin1[] = "example.co.uk";
constexpr char kOrigin2[] = "example.com";
constexpr char kOrigin3[] = "another-example.com";

constexpr std::array<FormSignature, 3> kSignatures1 = {
    FormSignature(1), FormSignature(2456), FormSignature(365)};
constexpr std::array<FormSignature, 2> kSignatures2 = {FormSignature(10),
                                                       FormSignature(246)};
constexpr std::array<FormSignature, 4> kSignatures3 = {
    FormSignature(1), FormSignature(23), FormSignature(39), FormSignature(100)};
constexpr std::array<FormSignature, 0> kEmptySignatures = {};

constexpr FormSignature kSignatureNotIn1 = FormSignature(5);

// Tests of `FastCheckoutCapabilitiesResultsCache`.
class FastCheckoutCapabilitiesResultsCacheTest : public ::testing::Test {
 protected:
  FastCheckoutCapabilitiesResultsCache& cache() { return cache_; }

  void AdvanceClock(base::TimeDelta duration) {
    task_environment_.AdvanceClock(duration);
  }

 private:
  // Test support.
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  // The object to test.
  FastCheckoutCapabilitiesResultsCache cache_;
};

TEST(CapabilitiesResultTest, SupportsForm) {
  FastCheckoutCapabilitiesResult result{kSignatures1, false};

  for (auto signature : kSignatures1) {
    EXPECT_TRUE(result.SupportsForm(signature))
        << "signature should be supported: " << signature;
  }
  EXPECT_FALSE(result.SupportsForm(kSignatureNotIn1));
}

TEST(CapabilitiesResultTest, SupportsConsentlessExecution) {
  FastCheckoutCapabilitiesResult result{kSignatures1, true};

  EXPECT_TRUE(result.SupportsConsentlessExecution());
}

TEST_F(FastCheckoutCapabilitiesResultsCacheTest, AddToCache) {
  url::Origin origin1 = url::Origin::Create(GURL(kOrigin1));
  url::Origin origin2 = url::Origin::Create(GURL(kOrigin2));
  url::Origin origin3 = url::Origin::Create(GURL(kOrigin3));

  EXPECT_FALSE(cache().ContainsOrigin(origin1));
  EXPECT_FALSE(cache().ContainsTriggerForm(origin1, kSignatures1.front()));
  EXPECT_FALSE(cache().ContainsOrigin(origin2));
  EXPECT_FALSE(cache().ContainsTriggerForm(origin2, kSignatures2.front()));
  EXPECT_FALSE(cache().ContainsOrigin(origin3));
  EXPECT_FALSE(cache().ContainsTriggerForm(origin3, kSignatures3.front()));

  cache().AddToCache(origin1,
                     FastCheckoutCapabilitiesResult(kSignatures1, false));
  cache().AddToCache(origin2,
                     FastCheckoutCapabilitiesResult(kSignatures2, false));
  cache().AddToCache(origin3,
                     FastCheckoutCapabilitiesResult(kSignatures3, false));

  EXPECT_TRUE(cache().ContainsOrigin(origin1));
  EXPECT_TRUE(cache().ContainsTriggerForm(origin1, kSignatures1.front()));
  EXPECT_FALSE(cache().ContainsTriggerForm(origin1, kSignatureNotIn1));
  EXPECT_TRUE(cache().ContainsOrigin(origin2));
  EXPECT_TRUE(cache().ContainsTriggerForm(origin2, kSignatures2.front()));
  EXPECT_TRUE(cache().ContainsOrigin(origin3));
  EXPECT_TRUE(cache().ContainsTriggerForm(origin3, kSignatures3.front()));
}

TEST_F(FastCheckoutCapabilitiesResultsCacheTest, AddToCacheWithAdvancedClock) {
  url::Origin origin1 = url::Origin::Create(GURL(kOrigin1));
  url::Origin origin2 = url::Origin::Create(GURL(kOrigin2));

  EXPECT_FALSE(cache().ContainsOrigin(origin1));
  EXPECT_FALSE(cache().ContainsTriggerForm(origin1, kSignatures1.front()));
  EXPECT_FALSE(cache().ContainsOrigin(origin2));
  EXPECT_FALSE(cache().ContainsTriggerForm(origin2, kSignatures2.front()));

  cache().AddToCache(origin1,
                     FastCheckoutCapabilitiesResult(kSignatures1, false));

  EXPECT_TRUE(cache().ContainsOrigin(origin1));
  EXPECT_FALSE(cache().ContainsOrigin(origin2));

  AdvanceClock(base::Minutes(6));
  cache().AddToCache(origin2,
                     FastCheckoutCapabilitiesResult(kSignatures2, false));

  EXPECT_TRUE(cache().ContainsOrigin(origin1));
  EXPECT_TRUE(cache().ContainsOrigin(origin2));

  AdvanceClock(base::Minutes(6));

  EXPECT_FALSE(cache().ContainsOrigin(origin1));
  EXPECT_TRUE(cache().ContainsOrigin(origin2));

  AdvanceClock(base::Minutes(6));
  EXPECT_FALSE(cache().ContainsOrigin(origin1));
  EXPECT_FALSE(cache().ContainsOrigin(origin2));
}

TEST_F(FastCheckoutCapabilitiesResultsCacheTest, AddToCacheWithMaxSizeReached) {
  url::Origin origin1 = url::Origin::Create(GURL(kOrigin1));
  cache().AddToCache(origin1,
                     FastCheckoutCapabilitiesResult(kSignatures1, false));
  EXPECT_TRUE(cache().ContainsOrigin(origin1));

  // Add generic origins until the cache is full.
  for (size_t index = 1; index < FastCheckoutCapabilitiesResultsCache::kMaxSize;
       ++index) {
    cache().AddToCache(
        url::Origin::Create(GURL(base::StrCat(
            {"example-page", base::NumberToString(index), ".de"}))),
        FastCheckoutCapabilitiesResult(kEmptySignatures, false));
  }

  // The earliest entry should still be contained in the cache.
  EXPECT_TRUE(cache().ContainsOrigin(origin1));

  // Adding another entry purges the earliest one.
  url::Origin origin2 = url::Origin::Create(GURL(kOrigin2));
  cache().AddToCache(origin2,
                     FastCheckoutCapabilitiesResult(kSignatures2, false));
  EXPECT_FALSE(cache().ContainsOrigin(origin1));
  EXPECT_TRUE(cache().ContainsOrigin(origin2));
}

TEST_F(FastCheckoutCapabilitiesResultsCacheTest, SupportsConsentlessExecution) {
  url::Origin originConsentless = url::Origin::Create(GURL(kOrigin1));
  url::Origin originNotConsentless = url::Origin::Create(GURL(kOrigin2));

  EXPECT_FALSE(cache().ContainsOrigin(originConsentless));
  EXPECT_FALSE(cache().SupportsConsentlessExecution(originConsentless));
  EXPECT_FALSE(cache().ContainsOrigin(originNotConsentless));
  EXPECT_FALSE(cache().SupportsConsentlessExecution(originNotConsentless));

  cache().AddToCache(originConsentless,
                     FastCheckoutCapabilitiesResult(kSignatures1, true));
  cache().AddToCache(originNotConsentless,
                     FastCheckoutCapabilitiesResult(kSignatures2, false));

  EXPECT_TRUE(cache().SupportsConsentlessExecution(originConsentless));
  EXPECT_FALSE(cache().SupportsConsentlessExecution(originNotConsentless));
}

}  // namespace
