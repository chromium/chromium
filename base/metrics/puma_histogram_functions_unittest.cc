// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/puma_histogram_functions.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/statistics_recorder.h"
#include "base/notreached.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

enum class PumaHistogramTestingEnum1 {
  kFirst,
  kSecond,
  kThird,
};

enum class PumaHistogramTestingEnum2 {
  kFirst = 0,
  kSecond = 1,
  kThird = 2,
  kMaxValue = kThird,
};

enum class HistogramFunctionType {
  kConstCharStar = 0,
  kStringView = 1,
};

class PumaHistogramFunctionsTest
    : public ::testing::TestWithParam<HistogramFunctionType> {
 public:
  PumaHistogramFunctionsTest() = default;
  ~PumaHistogramFunctionsTest() override = default;

 private:
  // PUMA histograms will be recorded in the global StatisticsRecorder.
  // This is needed to properly initialize it for tests.
  std::unique_ptr<StatisticsRecorder> statistics_recorder_ =
      StatisticsRecorder::CreateTemporaryForTesting();
};

}  // namespace

TEST_F(PumaHistogramFunctionsTest, PumaHistogramsAreFlaggedProperly) {
  PumaHistogramBoolean(PumaType::kRc, "PUMA.Test.HistBooleanFlag", true);
  PumaHistogramExactLinear(PumaType::kRc, "PUMA.Test.HistExactLinearFlag", 1,
                           20);

  std::vector<HistogramBase*> histograms = {
      StatisticsRecorder::FindHistogram("PUMA.Test.HistBooleanFlag"),
      StatisticsRecorder::FindHistogram("PUMA.Test.HistExactLinearFlag"),
  };

  for (HistogramBase* histogram : histograms) {
    ASSERT_NE(histogram, nullptr);

    // PUMA histograms should have an additional flag set.
    EXPECT_TRUE(histogram->HasFlags(
        HistogramBase::Flags::kPumaRcTargetedHistogramFlag));

    // PUMA histograms should not have the kUmaTargetedHistogramFlag fag set,
    // as they are uploaded separately from UMA.
    EXPECT_FALSE(
        histogram->HasFlags(HistogramBase::Flags::kUmaTargetedHistogramFlag));

    // It shouldn't also have other known flags set.
    EXPECT_FALSE(
        histogram->HasFlags(HistogramBase::Flags::kIPCSerializationSourceFlag));
    EXPECT_FALSE(histogram->HasFlags(HistogramBase::Flags::kCallbackExists));
    EXPECT_FALSE(histogram->HasFlags(HistogramBase::Flags::kIsPersistent));
  }
}

TEST_F(PumaHistogramFunctionsTest, PumaAndUmaHistogramsAreFlaggedProperly) {
  PumaHistogramBoolean(PumaType::kRc, "PUMA.Test.HistBooleanFlag", true);
  base::UmaHistogramBoolean("UMA.Test.HistBooleanFlag", true);

  HistogramBase* puma_histogram =
      StatisticsRecorder::FindHistogram("PUMA.Test.HistBooleanFlag");
  HistogramBase* uma_histogram =
      StatisticsRecorder::FindHistogram("UMA.Test.HistBooleanFlag");

  ASSERT_NE(puma_histogram, nullptr);
  ASSERT_NE(uma_histogram, nullptr);

  EXPECT_TRUE(puma_histogram->HasFlags(
      HistogramBase::Flags::kPumaRcTargetedHistogramFlag));
  EXPECT_FALSE(uma_histogram->HasFlags(
      HistogramBase::Flags::kPumaRcTargetedHistogramFlag));

  EXPECT_FALSE(puma_histogram->HasFlags(
      HistogramBase::Flags::kUmaTargetedHistogramFlag));
  EXPECT_TRUE(
      uma_histogram->HasFlags(HistogramBase::Flags::kUmaTargetedHistogramFlag));
}

TEST_F(PumaHistogramFunctionsTest, Boolean) {
  const char* histogram = "PUMA.Testing.HistogramBoolean";
  HistogramTester tester;

  PumaHistogramBoolean(PumaType::kRc, histogram, true);

  tester.ExpectUniqueSample(histogram, 1, 1);

  PumaHistogramBoolean(PumaType::kRc, histogram, false);

  tester.ExpectBucketCount(histogram, 0, 1);
  tester.ExpectBucketCount(histogram, 1, 1);
  tester.ExpectTotalCount(histogram, 2);
}

TEST_F(PumaHistogramFunctionsTest, BooleanFlags) {
  const char* histogram_name = "PUMA.Testing.HistogramBooleanFlag";

  PumaHistogramBoolean(PumaType::kRc, histogram_name, true);

  HistogramBase* histogram = StatisticsRecorder::FindHistogram(histogram_name);
  ASSERT_NE(histogram, nullptr);

  // PUMA histograms should have an additional flag set.
  EXPECT_TRUE(
      histogram->HasFlags(HistogramBase::Flags::kPumaRcTargetedHistogramFlag));

  // PUMA histograms should not have the kUmaTargetedHistogramFlag fag set,
  // as they are uploaded separately from UMA.
  EXPECT_FALSE(
      histogram->HasFlags(HistogramBase::Flags::kUmaTargetedHistogramFlag));
}

TEST_F(PumaHistogramFunctionsTest, ExactLinear) {
  const char* histogram = "PUMA.Testing.HistogramExactLinear";

  HistogramTester tester;
  PumaHistogramExactLinear(PumaType::kRc, histogram, 10, 100);
  tester.ExpectUniqueSample(histogram, 10, 1);
  PumaHistogramExactLinear(PumaType::kRc, histogram, 20, 100);
  PumaHistogramExactLinear(PumaType::kRc, histogram, 10, 100);
  tester.ExpectBucketCount(histogram, 10, 2);
  tester.ExpectBucketCount(histogram, 20, 1);
  tester.ExpectTotalCount(histogram, 3);

  PumaHistogramExactLinear(PumaType::kRc, histogram, 200, 100);
  tester.ExpectBucketCount(histogram, 101, 1);
  tester.ExpectTotalCount(histogram, 4);
}

TEST_F(PumaHistogramFunctionsTest, EnumerationWithSize) {
  const char* histogram = "PUMA.Testing.HistogramEnumeration";
  HistogramTester tester;
  PumaHistogramEnumeration(PumaType::kRc, histogram,
                           PumaHistogramTestingEnum1::kFirst,
                           PumaHistogramTestingEnum1::kThird);
  tester.ExpectUniqueSample(histogram, PumaHistogramTestingEnum1::kFirst, 1);
}

TEST_F(PumaHistogramFunctionsTest, EnumerationWithoutSize) {
  const char* histogram = "PUMA.Testing.HistogramEnumeration";
  HistogramTester tester;
  PumaHistogramEnumeration(PumaType::kRc, histogram,
                           PumaHistogramTestingEnum2::kSecond);
  tester.ExpectUniqueSample(histogram, PumaHistogramTestingEnum2::kSecond, 1);
}

}  // namespace base
