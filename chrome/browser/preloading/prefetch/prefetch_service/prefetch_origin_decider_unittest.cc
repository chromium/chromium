// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/prefetch_service/prefetch_origin_decider.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

class PrefetchOriginDeciderTest : public testing::Test {
 public:
  void SetUp() override {
    PrefetchOriginDecider::RegisterPrefs(pref_service_.registry());
  }

  std::unique_ptr<PrefetchOriginDecider> NewDecider() {
    return std::make_unique<PrefetchOriginDecider>(&pref_service_, &clock_);
  }

  base::SimpleTestClock* clock() { return &clock_; }

  base::HistogramTester* histogram_tester() { return &histogram_tester_; }

 private:
  TestingPrefServiceSimple pref_service_;
  base::SimpleTestClock clock_;
  base::HistogramTester histogram_tester_;
};

TEST_F(PrefetchOriginDeciderTest, DefaultEligible) {
  auto decider = NewDecider();
  EXPECT_TRUE(decider->IsOriginOutsideRetryAfterWindow(GURL("http://foo.com")));
}

TEST_F(PrefetchOriginDeciderTest, NegativeIgnored) {
  GURL url("http://foo.com");

  auto decider = NewDecider();
  decider->ReportOriginRetryAfter(url, base::Seconds(-1));
  EXPECT_TRUE(decider->IsOriginOutsideRetryAfterWindow(url));
}

TEST_F(PrefetchOriginDeciderTest, WaitsForDelta) {
  GURL url("http://foo.com");

  auto decider = NewDecider();

  decider->ReportOriginRetryAfter(url, base::Seconds(15));
  histogram_tester()->ExpectUniqueTimeSample(
      "PrefetchProxy.Prefetch.Mainframe.RetryAfter", base::Seconds(15), 1);

  for (size_t i = 0; i <= 15; i++) {
    EXPECT_FALSE(decider->IsOriginOutsideRetryAfterWindow(url));
    clock()->Advance(base::Seconds(1));
  }

  EXPECT_TRUE(decider->IsOriginOutsideRetryAfterWindow(url));
}

TEST_F(PrefetchOriginDeciderTest, ByOrigin) {
  auto decider = NewDecider();

  decider->ReportOriginRetryAfter(GURL("http://foo.com"), base::Seconds(1));
  histogram_tester()->ExpectUniqueTimeSample(
      "PrefetchProxy.Prefetch.Mainframe.RetryAfter", base::Seconds(1), 1);

  // Any url for the origin should be ineligible.
  for (const GURL& url : {
           GURL("http://foo.com"),
           GURL("http://foo.com/path"),
           GURL("http://foo.com/?query=yes"),
       }) {
    SCOPED_TRACE(url);
    EXPECT_FALSE(decider->IsOriginOutsideRetryAfterWindow(url));
  }

  // Other origins are eligible.
  for (const GURL& url : {
           GURL("http://foo.com:1234"),
           GURL("https://foo.com/"),
           GURL("http://test.com/"),
       }) {
    SCOPED_TRACE(url);
    EXPECT_TRUE(decider->IsOriginOutsideRetryAfterWindow(url));
  }
}

TEST_F(PrefetchOriginDeciderTest, Clear) {
  GURL url("http://foo.com");

  auto decider = NewDecider();

  decider->ReportOriginRetryAfter(url, base::Seconds(1));
  histogram_tester()->ExpectUniqueTimeSample(
      "PrefetchProxy.Prefetch.Mainframe.RetryAfter", base::Seconds(1), 1);
  EXPECT_FALSE(decider->IsOriginOutsideRetryAfterWindow(url));

  decider->OnBrowsingDataCleared();
  EXPECT_TRUE(decider->IsOriginOutsideRetryAfterWindow(url));
}

TEST_F(PrefetchOriginDeciderTest, PersistentPrefs) {
  {
    auto decider = NewDecider();

    decider->ReportOriginRetryAfter(GURL("http://expired.com"),
                                    base::Seconds(1));
    decider->ReportOriginRetryAfter(GURL("http://foo.com"), base::Seconds(3));
    histogram_tester()->ExpectTimeBucketCount(
        "PrefetchProxy.Prefetch.Mainframe.RetryAfter", base::Seconds(1), 1);
    histogram_tester()->ExpectTimeBucketCount(
        "PrefetchProxy.Prefetch.Mainframe.RetryAfter", base::Seconds(3), 1);
    histogram_tester()->ExpectTotalCount(
        "PrefetchProxy.Prefetch.Mainframe.RetryAfter", 2);
  }

  clock()->Advance(base::Seconds(2));

  {
    auto decider = NewDecider();

    EXPECT_TRUE(
        decider->IsOriginOutsideRetryAfterWindow(GURL("http://expired.com")));
    EXPECT_FALSE(
        decider->IsOriginOutsideRetryAfterWindow(GURL("http://foo.com")));
  }
}
