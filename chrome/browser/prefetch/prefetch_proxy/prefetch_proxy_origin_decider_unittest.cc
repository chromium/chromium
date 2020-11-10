// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefetch/prefetch_proxy/prefetch_proxy_origin_decider.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "chrome/browser/prefetch/prefetch_proxy/prefetch_proxy_features.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(PrefetchProxyOriginDeciderTest, DefaultEligible) {
  PrefetchProxyOriginDecider decider;
  EXPECT_TRUE(decider.IsOriginOutsideRetryAfterWindow(GURL("http://foo.com")));
}

TEST(PrefetchProxyOriginDeciderTest, NegativeIgnored) {
  GURL url("http://foo.com");
  PrefetchProxyOriginDecider decider;
  decider.ReportOriginRetryAfter(url, base::TimeDelta::FromSeconds(-1));
  EXPECT_TRUE(decider.IsOriginOutsideRetryAfterWindow(url));
}

TEST(PrefetchProxyOriginDeciderTest, MaxCap) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kIsolatePrerenders, {{"max_retry_after_duration_secs", "10"}});

  GURL url("http://foo.com");
  PrefetchProxyOriginDecider decider;
  base::SimpleTestClock clock;
  decider.SetClockForTesting(&clock);

  decider.ReportOriginRetryAfter(url, base::TimeDelta::FromSeconds(15));
  EXPECT_FALSE(decider.IsOriginOutsideRetryAfterWindow(url));

  clock.Advance(base::TimeDelta::FromSeconds(11));
  EXPECT_TRUE(decider.IsOriginOutsideRetryAfterWindow(url));
}

TEST(PrefetchProxyOriginDeciderTest, WaitsForDelta) {
  GURL url("http://foo.com");
  PrefetchProxyOriginDecider decider;
  base::SimpleTestClock clock;
  decider.SetClockForTesting(&clock);

  decider.ReportOriginRetryAfter(url, base::TimeDelta::FromSeconds(15));

  for (size_t i = 0; i <= 15; i++) {
    EXPECT_FALSE(decider.IsOriginOutsideRetryAfterWindow(url));
    clock.Advance(base::TimeDelta::FromSeconds(1));
  }

  EXPECT_TRUE(decider.IsOriginOutsideRetryAfterWindow(url));
}

TEST(PrefetchProxyOriginDeciderTest, ByOrigin) {
  PrefetchProxyOriginDecider decider;

  decider.ReportOriginRetryAfter(GURL("http://foo.com"),
                                 base::TimeDelta::FromSeconds(1));

  // Any url for the origin should be ineligible.
  for (const GURL& url : {
           GURL("http://foo.com"),
           GURL("http://foo.com/path"),
           GURL("http://foo.com/?query=yes"),
       }) {
    SCOPED_TRACE(url);
    EXPECT_FALSE(decider.IsOriginOutsideRetryAfterWindow(url));
  }

  // Other origins are eligible.
  for (const GURL& url : {
           GURL("http://foo.com:1234"),
           GURL("https://foo.com/"),
           GURL("http://test.com/"),
       }) {
    SCOPED_TRACE(url);
    EXPECT_TRUE(decider.IsOriginOutsideRetryAfterWindow(url));
  }
}

TEST(PrefetchProxyOriginDeciderTest, Clear) {
  GURL url("http://foo.com");
  PrefetchProxyOriginDecider decider;

  decider.ReportOriginRetryAfter(url, base::TimeDelta::FromSeconds(1));
  EXPECT_FALSE(decider.IsOriginOutsideRetryAfterWindow(url));

  decider.OnBrowsingDataCleared();
  EXPECT_TRUE(decider.IsOriginOutsideRetryAfterWindow(url));
}
