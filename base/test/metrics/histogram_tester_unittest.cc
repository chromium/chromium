// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"

#include <memory>
#include <string>

#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_samples.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;
using ::testing::IsEmpty;

namespace {

const char kHistogram1[] = "Test1";
const char kHistogram2[] = "Test2";
const char kHistogram3[] = "Test3";
const char kHistogram4[] = "Test4";
const char kHistogram5[] = "Test5";
const char kHistogram6[] = "Test6";

}  // namespace

namespace base {

typedef testing::Test HistogramTesterTest;

TEST_F(HistogramTesterTest, Scope) {
  // Record a histogram before the creation of the recorder.
  UMA_HISTOGRAM_BOOLEAN(kHistogram1, true);

  HistogramTester tester;

  // Verify that no histogram is recorded.
  tester.ExpectTotalCount(kHistogram1, 0);

  // Record a histogram after the creation of the recorder.
  UMA_HISTOGRAM_BOOLEAN(kHistogram1, true);

  // Verify that one histogram is recorded.
  std::unique_ptr<HistogramSamples> samples(
      tester.GetHistogramSamplesSinceCreation(kHistogram1));
  EXPECT_TRUE(samples);
  EXPECT_EQ(1, samples->TotalCount());
}

TEST_F(HistogramTesterTest, GetHistogramSamplesSinceCreationNotNull) {
  // Chose the histogram name uniquely, to ensure nothing was recorded for it so
  // far.
  static const char kHistogram[] =
      "GetHistogramSamplesSinceCreationNotNullHistogram";
  HistogramTester tester;

  // Verify that the returned samples are empty but not null.
  std::unique_ptr<HistogramSamples> samples(
      tester.GetHistogramSamplesSinceCreation(kHistogram1));
  EXPECT_TRUE(samples);
  tester.ExpectTotalCount(kHistogram, 0);
}

TEST_F(HistogramTesterTest, TestUniqueSample) {
  HistogramTester tester;

  // Emit '2' three times.
  UMA_HISTOGRAM_COUNTS_100(kHistogram2, 2);
  UMA_HISTOGRAM_COUNTS_100(kHistogram2, 2);
  UMA_HISTOGRAM_COUNTS_100(kHistogram2, 2);

  tester.ExpectUniqueSample(kHistogram2, 2, 3);
  tester.ExpectUniqueTimeSample(kHistogram2, base::Milliseconds(2), 3);
}

// Verify that the expectation is violated if the bucket contains an incorrect
// number of samples.
TEST_F(HistogramTesterTest, TestUniqueSample_TooManySamplesInActualBucket) {
  auto failing_code = [] {
    HistogramTester tester;

    // Emit '2' four times.
    UMA_HISTOGRAM_COUNTS_100(kHistogram2, 2);
    UMA_HISTOGRAM_COUNTS_100(kHistogram2, 2);
    UMA_HISTOGRAM_COUNTS_100(kHistogram2, 2);
    UMA_HISTOGRAM_COUNTS_100(kHistogram2, 2);

    // Expect exactly three samples in bucket 2. This is supposed to fail.
    tester.ExpectUniqueSample(kHistogram2, 2, 3);
  };
  EXPECT_NONFATAL_FAILURE(failing_code(),
                          "Histogram \"Test2\" did not meet its expectations.");
}

// Verify that the expectation is violated if the bucket contains the correct
// number of samples but another bucket contains extra samples.
TEST_F(HistogramTesterTest, TestUniqueSample_OneExtraSampleInWrongBucket) {
  auto failing_code = [] {
    HistogramTester tester;

    // Emit '2' three times.
    UMA_HISTOGRAM_COUNTS_100(kHistogram2, 2);
    UMA_HISTOGRAM_COUNTS_100(kHistogram2, 2);
    UMA_HISTOGRAM_COUNTS_100(kHistogram2, 2);
    // Emit one unexpected '3'.
    UMA_HISTOGRAM_COUNTS_100(kHistogram2, 3);

    // Expect exactly three samples in bucket 2. This is supposed to fail.
    tester.ExpectUniqueSample(kHistogram2, 2, 3);
  };
  EXPECT_NONFATAL_FAILURE(failing_code(),
                          "Histogram \"Test2\" did not meet its expectations.");
}

TEST_F(HistogramTesterTest, TestBucketsSample) {
  HistogramTester tester;

  UMA_HISTOGRAM_COUNTS_100(kHistogram3, 2);
  UMA_HISTOGRAM_COUNTS_100(kHistogram3, 2);
  UMA_HISTOGRAM_COUNTS_100(kHistogram3, 2);
  UMA_HISTOGRAM_COUNTS_100(kHistogram3, 2);
  UMA_HISTOGRAM_COUNTS_100(kHistogram3, 3);

  tester.ExpectBucketCount(kHistogram3, 2, 4);
  tester.ExpectBucketCount(kHistogram3, 3, 1);

  tester.ExpectTotalCount(kHistogram3, 5);
}

TEST_F(HistogramTesterTest, TestBucketsSampleWithScope) {
  // Emit values twice, once before the tester creation and once after.
  UMA_HISTOGRAM_COUNTS_100(kHistogram4, 2);

  HistogramTester tester;
  UMA_HISTOGRAM_COUNTS_100(kHistogram4, 3);

  tester.ExpectBucketCount(kHistogram4, 2, 0);
  tester.ExpectBucketCount(kHistogram4, 3, 1);

  tester.ExpectTotalCount(kHistogram4, 1);
}

TEST_F(HistogramTesterTest, TestGetAllSamples) {
  HistogramTester tester;
  UMA_HISTOGRAM_ENUMERATION(kHistogram5, 2, 5);
  UMA_HISTOGRAM_ENUMERATION(kHistogram5, 3, 5);
  UMA_HISTOGRAM_ENUMERATION(kHistogram5, 3, 5);
  UMA_HISTOGRAM_ENUMERATION(kHistogram5, 5, 5);

  EXPECT_THAT(tester.GetAllSamples(kHistogram5),
              ElementsAre(Bucket(2, 1), Bucket(3, 2), Bucket(5, 1)));
}

TEST_F(HistogramTesterTest, TestGetAllSamples_NoSamples) {
  HistogramTester tester;
  EXPECT_THAT(tester.GetAllSamples(kHistogram5), IsEmpty());
}

TEST_F(HistogramTesterTest, TestGetTotalSum) {
  // Emit values twice, once before the tester creation and once after.
  UMA_HISTOGRAM_COUNTS_100(kHistogram4, 2);

  HistogramTester tester;
  UMA_HISTOGRAM_COUNTS_100(kHistogram4, 3);
  UMA_HISTOGRAM_COUNTS_100(kHistogram4, 4);

  EXPECT_EQ(7, tester.GetTotalSum(kHistogram4));
}

TEST_F(HistogramTesterTest, TestGetTotalCountsForPrefix) {
  HistogramTester tester;
  UMA_HISTOGRAM_ENUMERATION("Test1.Test2.Test3", 2, 5);

  // Regression check for bug https://crbug.com/659977.
  EXPECT_TRUE(tester.GetTotalCountsForPrefix("Test2.").empty());

  EXPECT_EQ(1u, tester.GetTotalCountsForPrefix("Test1.").size());
}

TEST_F(HistogramTesterTest, TestGetAllChangedHistograms) {
  // Emit multiple values, some before tester creation.
  UMA_HISTOGRAM_COUNTS_100(kHistogram6, true);
  UMA_HISTOGRAM_COUNTS_100(kHistogram4, 4);

  HistogramTester tester;
  UMA_HISTOGRAM_COUNTS_100(kHistogram4, 3);

  UMA_HISTOGRAM_ENUMERATION(kHistogram5, 2, 5);
  UMA_HISTOGRAM_ENUMERATION(kHistogram5, 3, 5);
  UMA_HISTOGRAM_ENUMERATION(kHistogram5, 4, 5);
  UMA_HISTOGRAM_ENUMERATION(kHistogram5, 5, 5);

  UMA_HISTOGRAM_ENUMERATION("Test1.Test2.Test3", 2, 5);
  std::string results = tester.GetAllHistogramsRecorded();

  EXPECT_EQ(std::string::npos, results.find("Histogram: Test1 recorded"));
  EXPECT_NE(std::string::npos,
            results.find("Histogram: Test4 recorded 1 new samples"));
  EXPECT_NE(std::string::npos,
            results.find("Histogram: Test5 recorded 4 new samples"));
  EXPECT_NE(
      std::string::npos,
      results.find("Histogram: Test1.Test2.Test3 recorded 1 new samples"));
}

TEST_F(HistogramTesterTest, MissingHistogramMeansEmptyBuckets) {
  // When a histogram hasn't been instantiated, expecting counts of zero should
  // still succeed.
  static const char kHistogram[] = "MissingHistogramMeansEmptyBucketsHistogram";
  HistogramTester tester;

  tester.ExpectBucketCount(kHistogram, 42, 0);
  tester.ExpectTotalCount(kHistogram, 0);
  EXPECT_TRUE(tester.GetAllSamples(kHistogram).empty());
  EXPECT_EQ(0, tester.GetTotalSum(kHistogram));
  EXPECT_EQ(0, tester.GetBucketCount(kHistogram, 42));
  EXPECT_EQ(0,
            tester.GetHistogramSamplesSinceCreation(kHistogram)->TotalCount());
}

TEST_F(HistogramTesterTest, BucketsAre) {
  // Auxiliary functions for keeping the lines short.
  auto a = [](std::vector<Bucket> b) { return b; };
  auto b = [](base::Histogram::Sample min, base::Histogram::Count count) {
    return Bucket(min, count);
  };
  using ::testing::Not;

  EXPECT_THAT(a({}), BucketsAre());
  EXPECT_THAT(a({}), BucketsAre(b(0, 0)));
  EXPECT_THAT(a({}), BucketsAre(b(1, 0)));
  EXPECT_THAT(a({}), BucketsAre(b(0, 0), b(1, 0)));
  EXPECT_THAT(a({}), Not(BucketsAre(b(1, 1))));

  EXPECT_THAT(a({b(1, 1)}), BucketsAre(b(1, 1)));
  EXPECT_THAT(a({b(1, 1)}), BucketsAre(b(0, 0), b(1, 1)));
  EXPECT_THAT(a({b(1, 1)}), Not(BucketsAre()));
  EXPECT_THAT(a({b(1, 1)}), Not(BucketsAre(b(0, 0))));
  EXPECT_THAT(a({b(1, 1)}), Not(BucketsAre(b(1, 0))));
  EXPECT_THAT(a({b(1, 1)}), Not(BucketsAre(b(2, 1))));
  EXPECT_THAT(a({b(1, 1)}), Not(BucketsAre(b(2, 2))));
  EXPECT_THAT(a({b(1, 1)}), Not(BucketsAre(b(0, 0), b(1, 0))));
  EXPECT_THAT(a({b(1, 1)}), Not(BucketsAre(b(0, 0), b(1, 1), b(2, 2))));
  EXPECT_THAT(a({b(1, 1)}), Not(BucketsAre(b(0, 0), b(1, 0), b(2, 0))));

  EXPECT_THAT(a({b(1, 1), b(2, 2)}), BucketsAre(b(1, 1), b(2, 2)));
  EXPECT_THAT(a({b(1, 1), b(2, 2)}), BucketsAre(b(0, 0), b(1, 1), b(2, 2)));
  EXPECT_THAT(a({b(1, 1), b(2, 2)}), Not(BucketsAre()));
  EXPECT_THAT(a({b(1, 1), b(2, 2)}), Not(BucketsAre(b(0, 0))));
  EXPECT_THAT(a({b(1, 1), b(2, 2)}), Not(BucketsAre(b(1, 1))));
  EXPECT_THAT(a({b(1, 1), b(2, 2)}), Not(BucketsAre(b(2, 2))));
  EXPECT_THAT(a({b(1, 1), b(2, 2)}), Not(BucketsAre(b(0, 0), b(1, 1))));
  EXPECT_THAT(a({b(1, 1), b(2, 2)}), Not(BucketsAre(b(1, 0))));
  EXPECT_THAT(a({b(1, 1), b(2, 2)}), Not(BucketsAre(b(2, 1))));
  EXPECT_THAT(a({b(1, 1), b(2, 2)}), Not(BucketsAre(b(0, 0), b(1, 0))));
  EXPECT_THAT(a({b(1, 1), b(2, 2)}),
              Not(BucketsAre(b(0, 0), b(1, 0), b(2, 0))));
}

TEST_F(HistogramTesterTest, BucketsInclude) {
  // Auxiliary function for the "actual" values to shorten lines.
  auto a = [](std::vector<Bucket> b) { return b; };
  auto b = [](base::Histogram::Sample min, base::Histogram::Count count) {
    return Bucket(min, count);
  };
  using ::testing::Not;

  EXPECT_THAT(a({}), BucketsInclude());
  EXPECT_THAT(a({}), BucketsInclude(b(0, 0)));
  EXPECT_THAT(a({}), BucketsInclude(b(1, 0)));
  EXPECT_THAT(a({}), BucketsInclude(b(0, 0), b(1, 0)));
  EXPECT_THAT(a({}), Not(BucketsInclude(b(1, 1))));

  EXPECT_THAT(a({b(1, 1)}), BucketsInclude());
  EXPECT_THAT(a({b(1, 1)}), BucketsInclude(b(0, 0)));
  EXPECT_THAT(a({b(1, 1)}), BucketsInclude(b(1, 1)));
  EXPECT_THAT(a({b(1, 1)}), BucketsInclude(b(0, 0), b(1, 1)));
  EXPECT_THAT(a({b(1, 1)}), Not(BucketsInclude(b(1, 0))));
  EXPECT_THAT(a({b(1, 1)}), Not(BucketsInclude(b(2, 1))));
  EXPECT_THAT(a({b(1, 1)}), Not(BucketsInclude(b(2, 2))));
  EXPECT_THAT(a({b(1, 1)}), Not(BucketsInclude(b(0, 0), b(1, 0))));
  EXPECT_THAT(a({b(1, 1)}), Not(BucketsInclude(b(0, 0), b(1, 1), b(2, 2))));
  EXPECT_THAT(a({b(1, 1)}), Not(BucketsInclude(b(0, 0), b(1, 0), b(2, 0))));

  EXPECT_THAT(a({b(1, 1), b(2, 2)}), BucketsInclude());
  EXPECT_THAT(a({b(1, 1), b(2, 2)}), BucketsInclude(b(0, 0)));
  EXPECT_THAT(a({b(1, 1), b(2, 2)}), BucketsInclude(b(1, 1)));
  EXPECT_THAT(a({b(1, 1), b(2, 2)}), BucketsInclude(b(2, 2)));
  EXPECT_THAT(a({b(1, 1), b(2, 2)}), BucketsInclude(b(0, 0), b(1, 1)));
  EXPECT_THAT(a({b(1, 1), b(2, 2)}), BucketsInclude(b(1, 1), b(2, 2)));
  EXPECT_THAT(a({b(1, 1), b(2, 2)}), BucketsInclude(b(0, 0), b(1, 1), b(2, 2)));
  EXPECT_THAT(a({b(1, 1), b(2, 2)}), Not(BucketsInclude(b(1, 0))));
  EXPECT_THAT(a({b(1, 1), b(2, 2)}), Not(BucketsInclude(b(2, 1))));
  EXPECT_THAT(a({b(1, 1), b(2, 2)}), Not(BucketsInclude(b(0, 0), b(1, 0))));
  EXPECT_THAT(a({b(1, 1), b(2, 2)}),
              Not(BucketsInclude(b(0, 0), b(1, 0), b(2, 0))));
}

}  // namespace base
