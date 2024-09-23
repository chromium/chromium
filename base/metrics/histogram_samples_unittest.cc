// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/histogram_samples.h"

#include <limits>

#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

using SingleSample = HistogramSamples::SingleSample;
using AtomicSingleSample = HistogramSamples::AtomicSingleSample;

TEST(SingleSampleTest, Load) {
  AtomicSingleSample sample;
  ASSERT_TRUE(sample.Accumulate(9, 1));

  SingleSample s = sample.Load();
  EXPECT_EQ(9U, s.bucket);
  EXPECT_EQ(1U, s.count);

  s = sample.Load();
  EXPECT_EQ(9U, s.bucket);
  EXPECT_EQ(1U, s.count);

  ASSERT_TRUE(sample.Accumulate(9, 1));
  s = sample.Load();
  EXPECT_EQ(9U, s.bucket);
  EXPECT_EQ(2U, s.count);
}

TEST(SingleSampleTest, Extract) {
  AtomicSingleSample sample;
  ASSERT_TRUE(sample.Accumulate(9, 1));

  SingleSample s = sample.Extract();
  EXPECT_EQ(9U, s.bucket);
  EXPECT_EQ(1U, s.count);

  s = sample.Extract();
  EXPECT_EQ(0U, s.bucket);
  EXPECT_EQ(0U, s.count);

  ASSERT_TRUE(sample.Accumulate(1, 2));
  s = sample.Extract();
  EXPECT_EQ(1U, s.bucket);
  EXPECT_EQ(2U, s.count);
}

TEST(SingleSampleTest, Disable) {
  AtomicSingleSample sample;
  EXPECT_EQ(0U, sample.Extract().count);
  EXPECT_FALSE(sample.IsDisabled());

  ASSERT_TRUE(sample.Accumulate(9, 1));
  EXPECT_EQ(1U, sample.ExtractAndDisable().count);
  EXPECT_TRUE(sample.IsDisabled());

  ASSERT_FALSE(sample.Accumulate(9, 1));
  EXPECT_EQ(0U, sample.Extract().count);
  // The sample should still be disabled.
  EXPECT_TRUE(sample.IsDisabled());
}

TEST(SingleSampleTest, Accumulate) {
  AtomicSingleSample sample;

  ASSERT_TRUE(sample.Accumulate(9, 1));
  ASSERT_TRUE(sample.Accumulate(9, 2));
  ASSERT_TRUE(sample.Accumulate(9, 4));
  ASSERT_FALSE(sample.Accumulate(10, 1));
  EXPECT_EQ(7U, sample.Extract().count);

  ASSERT_TRUE(sample.Accumulate(9, 4));
  ASSERT_TRUE(sample.Accumulate(9, -2));
  ASSERT_TRUE(sample.Accumulate(9, 1));
  ASSERT_FALSE(sample.Accumulate(10, 1));
  EXPECT_EQ(3U, sample.Extract().count);
}

TEST(SingleSampleTest, Overflow) {
  AtomicSingleSample sample;

  ASSERT_TRUE(sample.Accumulate(9, 1));
  ASSERT_FALSE(sample.Accumulate(9, -2));
  EXPECT_EQ(1U, sample.Extract().count);

  ASSERT_TRUE(sample.Accumulate(9, std::numeric_limits<uint16_t>::max()));
  ASSERT_FALSE(sample.Accumulate(9, 1));
  EXPECT_EQ(std::numeric_limits<uint16_t>::max(), sample.Extract().count);
}

TEST(HistogramSamplesTest, WriteAsciiBucketGraph) {
  constexpr int kLineLength = 72;
  constexpr size_t kOutputSize = kLineLength + 1;

  std::string output;

  HistogramSamples::WriteAsciiBucketGraph(0.0, kLineLength, &output);
  ASSERT_EQ(output.size(), kOutputSize);
  output.clear();

  HistogramSamples::WriteAsciiBucketGraph(-1.0, kLineLength, &output);
  ASSERT_EQ(output.size(), kOutputSize);
  output.clear();

  HistogramSamples::WriteAsciiBucketGraph(kLineLength - 1, kLineLength,
                                          &output);
  ASSERT_EQ(output.size(), kOutputSize);
  output.clear();

  HistogramSamples::WriteAsciiBucketGraph(kLineLength, kLineLength, &output);
  ASSERT_EQ(output.size(), kOutputSize);
  output.clear();

  HistogramSamples::WriteAsciiBucketGraph(kLineLength + 1, kLineLength,
                                          &output);
  ASSERT_EQ(output.size(), kOutputSize + 1);
}

}  // namespace base
