// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/histogram_functions.h"

#include "base/metrics/histogram_macros.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

enum UmaHistogramTestingEnum {
  UMA_HISTOGRAM_TESTING_ENUM_FIRST,
  UMA_HISTOGRAM_TESTING_ENUM_SECOND,
  UMA_HISTOGRAM_TESTING_ENUM_THIRD
};

TEST(HistogramFunctionsTest, ExactLinear) {
  std::string histogram("Testing.UMA.HistogramExactLinear");
  HistogramTester tester;
  UmaHistogramExactLinear(histogram, 10, 100);
  tester.ExpectUniqueSample(histogram, 10, 1);
  UmaHistogramExactLinear(histogram, 20, 100);
  UmaHistogramExactLinear(histogram, 10, 100);
  tester.ExpectBucketCount(histogram, 10, 2);
  tester.ExpectBucketCount(histogram, 20, 1);
  tester.ExpectTotalCount(histogram, 3);
  // Test linear buckets overflow.
  UmaHistogramExactLinear(histogram, 200, 100);
  tester.ExpectBucketCount(histogram, 101, 1);
  tester.ExpectTotalCount(histogram, 4);
  // Test linear buckets underflow.
  UmaHistogramExactLinear(histogram, 0, 100);
  tester.ExpectBucketCount(histogram, 0, 1);
  tester.ExpectTotalCount(histogram, 5);
}

TEST(HistogramFunctionsTest, Enumeration) {
  std::string histogram("Testing.UMA.HistogramEnumeration");
  HistogramTester tester;
  UmaHistogramEnumeration(histogram, UMA_HISTOGRAM_TESTING_ENUM_FIRST,
                          UMA_HISTOGRAM_TESTING_ENUM_THIRD);
  tester.ExpectUniqueSample(histogram, UMA_HISTOGRAM_TESTING_ENUM_FIRST, 1);

  // Verify the overflow & underflow bucket exists.
  UMA_HISTOGRAM_ENUMERATION(
      histogram, static_cast<int>(UMA_HISTOGRAM_TESTING_ENUM_THIRD) + 10,
      static_cast<int>(UMA_HISTOGRAM_TESTING_ENUM_THIRD));
  tester.ExpectBucketCount(
      histogram, static_cast<int>(UMA_HISTOGRAM_TESTING_ENUM_THIRD) + 1, 1);
  tester.ExpectTotalCount(histogram, 2);
}

TEST(HistogramFunctionsTest, Boolean) {
  std::string histogram("Testing.UMA.HistogramBoolean");
  HistogramTester tester;
  UmaHistogramBoolean(histogram, true);
  tester.ExpectUniqueSample(histogram, 1, 1);
  UmaHistogramBoolean(histogram, false);
  tester.ExpectBucketCount(histogram, 0, 1);
  tester.ExpectTotalCount(histogram, 2);
}

TEST(HistogramFunctionsTest, Percentage) {
  std::string histogram("Testing.UMA.HistogramPercentage");
  HistogramTester tester;
  UmaHistogramPercentage(histogram, 1);
  tester.ExpectBucketCount(histogram, 1, 1);
  tester.ExpectTotalCount(histogram, 1);

  UmaHistogramPercentage(histogram, 50);
  tester.ExpectBucketCount(histogram, 50, 1);
  tester.ExpectTotalCount(histogram, 2);

  UmaHistogramPercentage(histogram, 100);
  tester.ExpectBucketCount(histogram, 100, 1);
  tester.ExpectTotalCount(histogram, 3);
  // Test overflows.
  UmaHistogramPercentage(histogram, 101);
  tester.ExpectBucketCount(histogram, 101, 1);
  tester.ExpectTotalCount(histogram, 4);

  UmaHistogramPercentage(histogram, 500);
  tester.ExpectBucketCount(histogram, 101, 2);
  tester.ExpectTotalCount(histogram, 5);
}

TEST(HistogramFunctionsTest, Counts) {
  std::string histogram("Testing.UMA.HistogramCount.Custom");
  HistogramTester tester;

  // Add a sample that should go into the underflow bucket.
  UmaHistogramCustomCounts(histogram, 0, 1, 100, 10);

  // Add a sample that should go into the first bucket.
  UmaHistogramCustomCounts(histogram, 1, 1, 100, 10);

  // Add multiple samples that should go into the same bucket.
  UmaHistogramCustomCounts(histogram, 20, 1, 100, 10);
  UmaHistogramCustomCounts(histogram, 20, 1, 100, 10);
  UmaHistogramCustomCounts(histogram, 21, 1, 100, 10);

  // Add a sample that should go into the last bucket.
  UmaHistogramCustomCounts(histogram, 99, 1, 100, 10);

  // Add some samples that should go into the overflow bucket.
  UmaHistogramCustomCounts(histogram, 100, 1, 100, 10);
  UmaHistogramCustomCounts(histogram, 101, 1, 100, 10);

  // Verify the number of samples.
  tester.ExpectTotalCount(histogram, 8);

  // Verify the following:
  // (a) The underflow bucket [0, 1) contains one sample.
  // (b) The first and last buckets each contain one sample.
  // (c) The bucket for values in [16, 29) contains three samples.
  // (d) The overflow bucket contains two samples.
  EXPECT_THAT(tester.GetAllSamples(histogram),
              testing::ElementsAre(Bucket(0, 1), Bucket(1, 1), Bucket(16, 3),
                                   Bucket(54, 1), Bucket(100, 2)));
}

TEST(HistogramFunctionsTest, Times) {
  std::string histogram("Testing.UMA.HistogramTimes");
  HistogramTester tester;
  UmaHistogramTimes(histogram, Seconds(1));
  tester.ExpectTimeBucketCount(histogram, Seconds(1), 1);
  tester.ExpectTotalCount(histogram, 1);
  UmaHistogramTimes(histogram, Seconds(9));
  tester.ExpectTimeBucketCount(histogram, Seconds(9), 1);
  tester.ExpectTotalCount(histogram, 2);
  UmaHistogramTimes(histogram, Seconds(10));  // Overflows
  tester.ExpectTimeBucketCount(histogram, Seconds(10), 1);
  UmaHistogramTimes(histogram, Seconds(20));  // Overflows.
  // Check the value by picking any overflow time.
  tester.ExpectTimeBucketCount(histogram, Seconds(11), 2);
  tester.ExpectTotalCount(histogram, 4);
}

TEST(HistogramFunctionsTest, ScopedTimes) {
  std::string histogram("Testing.UMA.HistogramScopedTimes");
  HistogramTester tester;
  { ScopedUmaHistogramTimer scoped_uma_histogram_timer(histogram); }
  tester.ExpectTotalCount(histogram, 1);
  { ScopedUmaHistogramTimer scoped_uma_histogram_timer(histogram); }
  tester.ExpectTotalCount(histogram, 2);
}

TEST(HistogramFunctionsTest, Sparse_SupportsLargeRange) {
  std::string histogram("Testing.UMA.HistogramSparse");
  HistogramTester tester;
  UmaHistogramSparse(histogram, 0);
  UmaHistogramSparse(histogram, 123456789);
  UmaHistogramSparse(histogram, 123456789);
  EXPECT_THAT(tester.GetAllSamples(histogram),
              testing::ElementsAre(Bucket(0, 1), Bucket(123456789, 2)));
}

TEST(HistogramFunctionsTest, Sparse_SupportsNegativeValues) {
  std::string histogram("Testing.UMA.HistogramSparse");
  HistogramTester tester;
  UmaHistogramSparse(histogram, -1);
  tester.ExpectUniqueSample(histogram, -1, 1);
}

}  // namespace base.
