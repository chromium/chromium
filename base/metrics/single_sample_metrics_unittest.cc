// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/single_sample_metrics.h"

#include "base/metrics/dummy_histogram.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

const HistogramBase::Sample kMin = 1;
const HistogramBase::Sample kMax = 10;
const uint32_t kBucketCount = 10;
const char kMetricName[] = "Single.Sample.Metric";

class SingleSampleMetricsTest : public testing::Test {
 public:
  SingleSampleMetricsTest() = default;

  SingleSampleMetricsTest(const SingleSampleMetricsTest&) = delete;
  SingleSampleMetricsTest& operator=(const SingleSampleMetricsTest&) = delete;

  ~SingleSampleMetricsTest() override {
    // Ensure we cleanup after ourselves.
    SingleSampleMetricsFactory::DeleteFactoryForTesting();
  }
};

}  // namespace

TEST_F(SingleSampleMetricsTest, DefaultFactoryGetSet) {
  SingleSampleMetricsFactory* factory = SingleSampleMetricsFactory::Get();
  ASSERT_TRUE(factory);

  // Same factory should be returned evermore.
  EXPECT_EQ(factory, SingleSampleMetricsFactory::Get());

  // Setting a factory after the default has been instantiated should fail.
  EXPECT_DCHECK_DEATH(SingleSampleMetricsFactory::SetFactory(nullptr));
}

TEST_F(SingleSampleMetricsTest, CustomFactoryGetSet) {
  auto factory = std::make_unique<DefaultSingleSampleMetricsFactory>();
  SingleSampleMetricsFactory* factory_raw = factory.get();
  SingleSampleMetricsFactory::SetFactory(std::move(factory));
  EXPECT_EQ(factory_raw, SingleSampleMetricsFactory::Get());
}

TEST_F(SingleSampleMetricsTest, DefaultSingleSampleMetricNoValue) {
  SingleSampleMetricsFactory* factory = SingleSampleMetricsFactory::Get();

  HistogramTester tester;
  std::unique_ptr<SingleSampleMetric> metric =
      factory->CreateCustomCountsMetric(kMetricName, kMin, kMax, kBucketCount);
  metric.reset();

  // Verify that no sample is recorded if SetSample() is never called.
  tester.ExpectTotalCount(kMetricName, 0);
}

TEST_F(SingleSampleMetricsTest, DefaultSingleSampleMetricWithValue) {
  SingleSampleMetricsFactory* factory = SingleSampleMetricsFactory::Get();

  HistogramTester tester;
  std::unique_ptr<SingleSampleMetric> metric =
      factory->CreateCustomCountsMetric(kMetricName, kMin, kMax, kBucketCount);

  const HistogramBase::Sample kLastSample = 9;
  metric->SetSample(1);
  metric->SetSample(3);
  metric->SetSample(5);
  metric->SetSample(kLastSample);
  metric.reset();

  // Verify only the last sample sent to SetSample() is recorded.
  tester.ExpectUniqueSample(kMetricName, kLastSample, 1);

  // Verify construction implicitly by requesting a histogram with the same
  // parameters; this test relies on the fact that histogram objects are unique
  // per name. Different parameters will result in a Dummy histogram returned.
  EXPECT_EQ(
      DummyHistogram::GetInstance(),
      Histogram::FactoryGet(kMetricName, 1, 3, 3, HistogramBase::kNoFlags));
  EXPECT_NE(DummyHistogram::GetInstance(),
            Histogram::FactoryGet(kMetricName, kMin, kMax, kBucketCount,
                                  HistogramBase::kUmaTargetedHistogramFlag));
}

TEST_F(SingleSampleMetricsTest, MultipleMetricsAreDistinct) {
  SingleSampleMetricsFactory* factory = SingleSampleMetricsFactory::Get();

  HistogramTester tester;
  std::unique_ptr<SingleSampleMetric> metric =
      factory->CreateCustomCountsMetric(kMetricName, kMin, kMax, kBucketCount);
  std::unique_ptr<SingleSampleMetric> metric2 =
      factory->CreateCustomCountsMetric(kMetricName, kMin, kMax, kBucketCount);
  const char kMetricName2[] = "Single.Sample.Metric.2";
  std::unique_ptr<SingleSampleMetric> metric3 =
      factory->CreateCustomCountsMetric(kMetricName2, kMin, kMax, kBucketCount);

  const HistogramBase::Sample kSample1 = 5;
  metric->SetSample(kSample1);
  metric2->SetSample(kSample1);

  const HistogramBase::Sample kSample2 = 7;
  metric3->SetSample(kSample2);

  metric.reset();
  tester.ExpectUniqueSample(kMetricName, kSample1, 1);

  metric2.reset();
  tester.ExpectUniqueSample(kMetricName, kSample1, 2);

  metric3.reset();
  tester.ExpectUniqueSample(kMetricName2, kSample2, 1);
}

}  // namespace base
