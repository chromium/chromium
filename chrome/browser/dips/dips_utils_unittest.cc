// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_utils.h"

#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/test/base/browser_with_test_window_test.h"
#endif  // !BUILDFLAG(IS_ANDROID)

using ::testing::Eq;
using ::testing::Pair;

TEST(TimestampRangeTest, UpdateTimestampRangeEmpty) {
  const base::Time time = base::Time::FromDoubleT(1);

  TimestampRange range;
  EXPECT_TRUE(UpdateTimestampRange(range, time));
  EXPECT_EQ(range.value(), std::make_pair(time, time));
}

TEST(TimestampRangeTest, UpdateTimestampRange_SetLast) {
  const base::Time time1 = base::Time::FromDoubleT(1);
  const base::Time time2 = base::Time::FromDoubleT(2);
  const base::Time time3 = base::Time::FromDoubleT(3);

  TimestampRange range = {{time1, time2}};
  EXPECT_TRUE(UpdateTimestampRange(range, time3));
  EXPECT_EQ(range.value(), std::make_pair(time1, time3));
}

TEST(TimestampRangeTest, UpdateTimestampRange_SetFirst) {
  const base::Time time1 = base::Time::FromDoubleT(1);
  const base::Time time2 = base::Time::FromDoubleT(2);
  const base::Time time3 = base::Time::FromDoubleT(3);

  TimestampRange range = {{time2, time3}};
  EXPECT_TRUE(UpdateTimestampRange(range, time1));
  EXPECT_EQ(range.value(), std::make_pair(time1, time3));
}

TEST(TimestampRangeTest, UpdateTimestampRange_Unmodified) {
  const base::Time time1 = base::Time::FromDoubleT(1);
  const base::Time time2 = base::Time::FromDoubleT(2);
  const base::Time time3 = base::Time::FromDoubleT(3);

  TimestampRange range = {{time1, time3}};
  EXPECT_FALSE(UpdateTimestampRange(range, time2));
  EXPECT_EQ(range.value(), std::make_pair(time1, time3));
}

TEST(TimestampRangeTest, IsNullOrWithin_BothEmpty) {
  EXPECT_TRUE(IsNullOrWithin(/*inner=*/{}, /*outer=*/{}));
}

TEST(TimestampRangeTest, IsNullOrWithin_NothingWithinEmptyOuter) {
  TimestampRange inner = {
      {base::Time::FromDoubleT(1), base::Time::FromDoubleT(1)}};
  TimestampRange outer = {};
  EXPECT_FALSE(IsNullOrWithin(inner, outer));
}

TEST(TimestampRangeTest, IsNullOrWithin_EmptyInnerWithin) {
  TimestampRange inner = {};
  TimestampRange outer = {
      {base::Time::FromDoubleT(1), base::Time::FromDoubleT(1)}};
  EXPECT_TRUE(IsNullOrWithin(inner, outer));
}

TEST(TimestampRangeTest, IsNullOrWithin_ChecksLowerBound) {
  TimestampRange outer = {
      {base::Time::FromDoubleT(2), base::Time::FromDoubleT(5)}};
  TimestampRange starts_on_time = {
      {base::Time::FromDoubleT(3), base::Time::FromDoubleT(4)}};
  TimestampRange starts_too_early = {
      {base::Time::FromDoubleT(1), base::Time::FromDoubleT(4)}};

  EXPECT_FALSE(IsNullOrWithin(starts_too_early, outer));
  EXPECT_TRUE(IsNullOrWithin(starts_on_time, outer));
}

TEST(TimestampRangeTest, IsNullOrWithin_ChecksUpperBound) {
  TimestampRange outer = {
      {base::Time::FromDoubleT(2), base::Time::FromDoubleT(5)}};
  TimestampRange ends_in_time = {
      {base::Time::FromDoubleT(3), base::Time::FromDoubleT(4)}};
  TimestampRange ends_too_late = {
      {base::Time::FromDoubleT(3), base::Time::FromDoubleT(10)}};

  EXPECT_TRUE(IsNullOrWithin(ends_in_time, outer));
  EXPECT_FALSE(IsNullOrWithin(ends_too_late, outer));
}

TEST(TimestampRangeTest, IsNullOrWithin_AllowsEquals) {
  TimestampRange range = {
      {base::Time::FromDoubleT(1), base::Time::FromDoubleT(1)}};
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

// BrowserWithTestWindowTest is not available on Android.
#if !BUILDFLAG(IS_ANDROID)
class DoesFirstPartyPrecedeThirdPartyTest : public BrowserWithTestWindowTest {
 public:
  DoesFirstPartyPrecedeThirdPartyTest()
      : first_party_url_("http://first_party.com"),
        third_party_url_("http://third_party.com"),
        other_url_("http://other.com") {}

 protected:
  GURL first_party_url_;
  GURL third_party_url_;
  GURL other_url_;
};

TEST_F(DoesFirstPartyPrecedeThirdPartyTest,
       FirstPartyBeforeThirdParty_ReturnsTrue) {
  AddTab(browser(), first_party_url_);
  NavigateAndCommitActiveTab(third_party_url_);

  EXPECT_TRUE(DoesFirstPartyPrecedeThirdParty(
      browser()->tab_strip_model()->GetWebContentsAt(0), first_party_url_,
      third_party_url_));
}

TEST_F(DoesFirstPartyPrecedeThirdPartyTest, MultipleThirdParties_ReturnsTrue) {
  AddTab(browser(), third_party_url_);
  NavigateAndCommitActiveTab(other_url_);
  NavigateAndCommitActiveTab(first_party_url_);
  NavigateAndCommitActiveTab(third_party_url_);
  NavigateAndCommitActiveTab(third_party_url_);

  EXPECT_TRUE(DoesFirstPartyPrecedeThirdParty(
      browser()->tab_strip_model()->GetWebContentsAt(0), first_party_url_,
      third_party_url_));
}

TEST_F(DoesFirstPartyPrecedeThirdPartyTest, NoThirdParty_ReturnsFalse) {
  AddTab(browser(), first_party_url_);
  NavigateAndCommitActiveTab(other_url_);

  EXPECT_FALSE(DoesFirstPartyPrecedeThirdParty(
      browser()->tab_strip_model()->GetWebContentsAt(0), first_party_url_,
      third_party_url_));
}

TEST_F(DoesFirstPartyPrecedeThirdPartyTest,
       NothingBeforeThirdParty_ReturnsFalse) {
  AddTab(browser(), third_party_url_);

  EXPECT_FALSE(DoesFirstPartyPrecedeThirdParty(
      browser()->tab_strip_model()->GetWebContentsAt(0), first_party_url_,
      third_party_url_));
}

TEST_F(DoesFirstPartyPrecedeThirdPartyTest,
       DifferentSiteBeforeThirdParty_ReturnsFalse) {
  AddTab(browser(), first_party_url_);
  NavigateAndCommitActiveTab(other_url_);
  NavigateAndCommitActiveTab(third_party_url_);

  EXPECT_FALSE(DoesFirstPartyPrecedeThirdParty(
      browser()->tab_strip_model()->GetWebContentsAt(0), first_party_url_,
      third_party_url_));
}

TEST_F(DoesFirstPartyPrecedeThirdPartyTest, EmptyWebContents_ReturnsFalse) {
  EXPECT_FALSE(DoesFirstPartyPrecedeThirdParty(nullptr, first_party_url_,
                                               third_party_url_));
}
#endif  // !BUILDFLAG(IS_ANDROID)
