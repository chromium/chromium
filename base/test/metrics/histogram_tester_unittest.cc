// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"

#include <memory>
#include <string>

#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_samples.h"
#include "testing/gmock/include/gmock/gmock.h"
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

  // Record into a sample thrice
  UMA_HISTOGRAM_COUNTS_100(kHistogram2, 2);
  UMA_HISTOGRAM_COUNTS_100(kHistogram2, 2);
  UMA_HISTOGRAM_COUNTS_100(kHistogram2, 2);

  tester.ExpectUniqueSample(kHistogram2, 2, 3);
}

TEST_F(HistogramTesterTest, TestBucketsSample) {
  HistogramTester tester;

  // Record into a sample twice
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
  // Record into a sample twice, once before the tester creation and once after.
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

TEST_F(HistogramTesterTest, TestGetTotalCountsForPrefix) {
  HistogramTester tester;
  UMA_HISTOGRAM_ENUMERATION("Test1.Test2.Test3", 2, 5);

  // Regression check for bug https://crbug.com/659977.
  EXPECT_TRUE(tester.GetTotalCountsForPrefix("Test2.").empty());

  EXPECT_EQ(1u, tester.GetTotalCountsForPrefix("Test1.").size());
}

TEST_F(HistogramTesterTest, TestGetAllChangedHistograms) {
  // Record into a sample twice, once before the tester creation.
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
  EXPECT_EQ(0, tester.GetBucketCount(kHistogram, 42));
  EXPECT_EQ(0,
            tester.GetHistogramSamplesSinceCreation(kHistogram)->TotalCount());
}

}  // namespace base
