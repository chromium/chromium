// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_utils.h"

#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "content/public/browser/cookie_access_details.h"
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Eq;
using ::testing::Pair;

TEST(TimestampRangeTest, UpdateTimestampRangeEmpty) {
  const base::Time time = base::Time::FromSecondsSinceUnixEpoch(1);

  TimestampRange range;
  EXPECT_TRUE(UpdateTimestampRange(range, time));
  EXPECT_EQ(range.value(), std::make_pair(time, time));
}

TEST(TimestampRangeTest, UpdateTimestampRange_SetLast) {
  const base::Time time1 = base::Time::FromSecondsSinceUnixEpoch(1);
  const base::Time time2 = base::Time::FromSecondsSinceUnixEpoch(2);
  const base::Time time3 = base::Time::FromSecondsSinceUnixEpoch(3);

  TimestampRange range = {{time1, time2}};
  EXPECT_TRUE(UpdateTimestampRange(range, time3));
  EXPECT_EQ(range.value(), std::make_pair(time1, time3));
}

TEST(TimestampRangeTest, UpdateTimestampRange_SetFirst) {
  const base::Time time1 = base::Time::FromSecondsSinceUnixEpoch(1);
  const base::Time time2 = base::Time::FromSecondsSinceUnixEpoch(2);
  const base::Time time3 = base::Time::FromSecondsSinceUnixEpoch(3);

  TimestampRange range = {{time2, time3}};
  EXPECT_TRUE(UpdateTimestampRange(range, time1));
  EXPECT_EQ(range.value(), std::make_pair(time1, time3));
}

TEST(TimestampRangeTest, UpdateTimestampRange_Unmodified) {
  const base::Time time1 = base::Time::FromSecondsSinceUnixEpoch(1);
  const base::Time time2 = base::Time::FromSecondsSinceUnixEpoch(2);
  const base::Time time3 = base::Time::FromSecondsSinceUnixEpoch(3);

  TimestampRange range = {{time1, time3}};
  EXPECT_FALSE(UpdateTimestampRange(range, time2));
  EXPECT_EQ(range.value(), std::make_pair(time1, time3));
}

TEST(TimestampRangeTest, IsNullOrWithin_BothEmpty) {
  EXPECT_TRUE(IsNullOrWithin(/*inner=*/{}, /*outer=*/{}));
}

TEST(TimestampRangeTest, IsNullOrWithin_NothingWithinEmptyOuter) {
  TimestampRange inner = {{base::Time::FromSecondsSinceUnixEpoch(1),
                           base::Time::FromSecondsSinceUnixEpoch(1)}};
  TimestampRange outer = {};
  EXPECT_FALSE(IsNullOrWithin(inner, outer));
}

TEST(TimestampRangeTest, IsNullOrWithin_EmptyInnerWithin) {
  TimestampRange inner = {};
  TimestampRange outer = {{base::Time::FromSecondsSinceUnixEpoch(1),
                           base::Time::FromSecondsSinceUnixEpoch(1)}};
  EXPECT_TRUE(IsNullOrWithin(inner, outer));
}

TEST(TimestampRangeTest, IsNullOrWithin_ChecksLowerBound) {
  TimestampRange outer = {{base::Time::FromSecondsSinceUnixEpoch(2),
                           base::Time::FromSecondsSinceUnixEpoch(5)}};
  TimestampRange starts_on_time = {{base::Time::FromSecondsSinceUnixEpoch(3),
                                    base::Time::FromSecondsSinceUnixEpoch(4)}};
  TimestampRange starts_too_early = {
      {base::Time::FromSecondsSinceUnixEpoch(1),
       base::Time::FromSecondsSinceUnixEpoch(4)}};

  EXPECT_FALSE(IsNullOrWithin(starts_too_early, outer));
  EXPECT_TRUE(IsNullOrWithin(starts_on_time, outer));
}

TEST(TimestampRangeTest, IsNullOrWithin_ChecksUpperBound) {
  TimestampRange outer = {{base::Time::FromSecondsSinceUnixEpoch(2),
                           base::Time::FromSecondsSinceUnixEpoch(5)}};
  TimestampRange ends_in_time = {{base::Time::FromSecondsSinceUnixEpoch(3),
                                  base::Time::FromSecondsSinceUnixEpoch(4)}};
  TimestampRange ends_too_late = {{base::Time::FromSecondsSinceUnixEpoch(3),
                                   base::Time::FromSecondsSinceUnixEpoch(10)}};

  EXPECT_TRUE(IsNullOrWithin(ends_in_time, outer));
  EXPECT_FALSE(IsNullOrWithin(ends_too_late, outer));
}

TEST(TimestampRangeTest, IsNullOrWithin_AllowsEquals) {
  TimestampRange range = {{base::Time::FromSecondsSinceUnixEpoch(1),
                           base::Time::FromSecondsSinceUnixEpoch(1)}};
  EXPECT_TRUE(IsNullOrWithin(range, range));
}

TEST(BucketizeBounceDelayTest, BucketizeBounceDelay) {
  // any TimeDelta in (-inf, 1s) should return 0
  EXPECT_EQ(0, BucketizeBounceDelay(base::Days(-1)));
  EXPECT_EQ(0, BucketizeBounceDelay(base::Milliseconds(0)));
  EXPECT_EQ(0, BucketizeBounceDelay(base::Milliseconds(999)));
  // anything in [1s, 2s) should return 1
  EXPECT_EQ(1, BucketizeBounceDelay(base::Milliseconds(1000)));
  EXPECT_EQ(1, BucketizeBounceDelay(base::Milliseconds(1999)));
  // similarly for [2s, 3s)
  EXPECT_EQ(2, BucketizeBounceDelay(base::Milliseconds(2000)));
  EXPECT_EQ(2, BucketizeBounceDelay(base::Milliseconds(2999)));
  // ...
  EXPECT_EQ(9, BucketizeBounceDelay(base::Milliseconds(9999)));
  // anything in [10s, inf) should return 10
  EXPECT_EQ(10, BucketizeBounceDelay(base::Milliseconds(10000)));
  EXPECT_EQ(10, BucketizeBounceDelay(base::Milliseconds(10001)));
  EXPECT_EQ(10, BucketizeBounceDelay(base::Days(1)));
}

TEST(UpdateTimestampTest, AlwaysReplaceNullOpt) {
  const base::Time new_value = base::Time::FromTimeT(42);
  std::optional<base::Time> time;

  ASSERT_EQ(time, std::nullopt);
  EXPECT_TRUE(UpdateTimestamp(time, new_value));
  EXPECT_THAT(time, testing::Optional(new_value));
}

TEST(UpdateTimestampTest, DontReplaceBeforeIntervalPasses) {
  const base::Time old_value = base::Time::FromTimeT(42);
  const base::Time new_value =
      old_value + kDIPSTimestampUpdateInterval - base::Milliseconds(1);
  std::optional<base::Time> time = old_value;

  ASSERT_THAT(time, testing::Optional(old_value));
  EXPECT_FALSE(UpdateTimestamp(time, new_value));
  EXPECT_THAT(time, testing::Optional(old_value));
}

TEST(UpdateTimestampTest, ReplaceAfterIntervalPasses) {
  const base::Time old_value = base::Time::FromTimeT(42);
  const base::Time new_value = old_value + kDIPSTimestampUpdateInterval;
  std::optional<base::Time> time = old_value;

  ASSERT_THAT(time, testing::Optional(old_value));
  EXPECT_TRUE(UpdateTimestamp(time, new_value));
  EXPECT_THAT(time, testing::Optional(new_value));
}

TEST(IsAdTaggedCookieForHeuristics, ReturnsCorrectlyInExperiment) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      network::features::kSkipTpcdMitigationsForAds,
      {{"SkipTpcdMitigationsForAdsHeuristics", "true"}});

  content::CookieAccessDetails details;
  EXPECT_EQ(IsAdTaggedCookieForHeuristics(details), OptionalBool::kFalse);

  details.cookie_setting_overrides.Put(
      net::CookieSettingOverride::kSkipTPCDHeuristicsGrant);
  EXPECT_EQ(IsAdTaggedCookieForHeuristics(details), OptionalBool::kTrue);
}

TEST(IsAdTaggedCookieForHeuristics, ReturnsCorrectlyWithoutExperimentFeature) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(network::features::kSkipTpcdMitigationsForAds);

  content::CookieAccessDetails details;
  EXPECT_EQ(IsAdTaggedCookieForHeuristics(details), OptionalBool::kUnknown);

  details.cookie_setting_overrides.Put(
      net::CookieSettingOverride::kSkipTPCDHeuristicsGrant);
  EXPECT_EQ(IsAdTaggedCookieForHeuristics(details), OptionalBool::kUnknown);
}

TEST(IsAdTaggedCookieForHeuristics, ReturnsCorrectlyWithoutExperimentParam) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      network::features::kSkipTpcdMitigationsForAds,
      {{"SkipTpcdMitigationsForAdsHeuristics", "false"}});

  content::CookieAccessDetails details;
  EXPECT_EQ(IsAdTaggedCookieForHeuristics(details), OptionalBool::kUnknown);

  details.cookie_setting_overrides.Put(
      net::CookieSettingOverride::kSkipTPCDHeuristicsGrant);
  EXPECT_EQ(IsAdTaggedCookieForHeuristics(details), OptionalBool::kUnknown);
}

TEST(HasCHIPS, TrueOnlyWhenHasAtLeastOnePartitionedCookie) {
  auto unpartitioned_cookie = net::CanonicalCookie::CreateForTesting(
      GURL("https://example.com"), "name=value;", base::Time::Now());
  auto partitioned_cookie = net::CanonicalCookie::CreateForTesting(
      GURL("https://example.com"), "name=value; Partitioned; Path=/; Secure",
      base::Time::Now(), std::nullopt,
      net::CookiePartitionKey::FromURLForTesting(GURL("https://example.org")));

  net::CookieAccessResultList cookie_access_result_list_without_partitioned{
      {*(unpartitioned_cookie.get())}};
  EXPECT_FALSE(HasCHIPS(cookie_access_result_list_without_partitioned));

  net::CookieAccessResultList cookie_access_result_list_with_partitioned{
      {*unpartitioned_cookie.get()}, {*partitioned_cookie.get()}};
  EXPECT_TRUE(HasCHIPS(cookie_access_result_list_with_partitioned));
}
