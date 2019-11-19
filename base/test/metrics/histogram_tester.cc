// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"

#include <stddef.h>

#include "base/metrics/histogram.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/metrics_hashes.h"
#include "base/metrics/sample_map.h"
#include "base/metrics/sparse_histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

HistogramTester::HistogramTester() {
  // Record any histogram data that exists when the object is created so it can
  // be subtracted later.
  for (const auto* const histogram : StatisticsRecorder::GetHistograms()) {
    histograms_snapshot_[histogram->histogram_name()] =
        histogram->SnapshotSamples();
  }
}

HistogramTester::~HistogramTester() = default;

void HistogramTester::ExpectUniqueSample(
    StringPiece name,
    HistogramBase::Sample sample,
    HistogramBase::Count expected_count) const {
  HistogramBase* histogram = StatisticsRecorder::FindHistogram(name);
  if (histogram) {
    std::unique_ptr<HistogramSamples> samples = histogram->SnapshotSamples();
    CheckBucketCount(name, sample, expected_count, *samples);
    CheckTotalCount(name, expected_count, *samples);
  } else {
    // No histogram means there were zero samples.
    EXPECT_EQ(0, expected_count)
        << "Histogram \"" << name << "\" does not exist.";
  }
}

void HistogramTester::ExpectBucketCount(
    StringPiece name,
    HistogramBase::Sample sample,
    HistogramBase::Count expected_count) const {
  HistogramBase* histogram = StatisticsRecorder::FindHistogram(name);
  if (histogram) {
    std::unique_ptr<HistogramSamples> samples = histogram->SnapshotSamples();
    CheckBucketCount(name, sample, expected_count, *samples);
  } else {
    // No histogram means there were zero samples.
    EXPECT_EQ(0, expected_count)
        << "Histogram \"" << name << "\" does not exist.";
  }
}

void HistogramTester::ExpectTotalCount(StringPiece name,
                                       HistogramBase::Count count) const {
  HistogramBase* histogram = StatisticsRecorder::FindHistogram(name);
  if (histogram) {
    std::unique_ptr<HistogramSamples> samples = histogram->SnapshotSamples();
    CheckTotalCount(name, count, *samples);
  } else {
    // No histogram means there were zero samples.
    EXPECT_EQ(count, 0) << "Histogram \"" << name << "\" does not exist.";
  }
}

void HistogramTester::ExpectTimeBucketCount(StringPiece name,
                                            TimeDelta sample,
                                            HistogramBase::Count count) const {
  ExpectBucketCount(name, sample.InMilliseconds(), count);
}

std::vector<Bucket> HistogramTester::GetAllSamples(StringPiece name) const {
  std::vector<Bucket> samples;
  std::unique_ptr<HistogramSamples> snapshot =
      GetHistogramSamplesSinceCreation(name);
  if (snapshot) {
    for (auto it = snapshot->Iterator(); !it->Done(); it->Next()) {
      HistogramBase::Sample sample;
      HistogramBase::Count count;
      it->Get(&sample, nullptr, &count);
      samples.push_back(Bucket(sample, count));
    }
  }
  return samples;
}

HistogramBase::Count HistogramTester::GetBucketCount(
    StringPiece name,
    HistogramBase::Sample sample) const {
  HistogramBase* histogram = StatisticsRecorder::FindHistogram(name);
  HistogramBase::Count count = 0;
  if (histogram) {
    std::unique_ptr<HistogramSamples> samples = histogram->SnapshotSamples();
    GetBucketCountForSamples(name, sample, *samples, &count);
  }
  return count;
}

void HistogramTester::GetBucketCountForSamples(
    StringPiece name,
    HistogramBase::Sample sample,
    const HistogramSamples& samples,
    HistogramBase::Count* count) const {
  *count = samples.GetCount(sample);
  auto histogram_data = histograms_snapshot_.find(name);
  if (histogram_data != histograms_snapshot_.end())
    *count -= histogram_data->second->GetCount(sample);
}

HistogramTester::CountsMap HistogramTester::GetTotalCountsForPrefix(
    StringPiece prefix) const {
  EXPECT_TRUE(prefix.find('.') != StringPiece::npos)
      << "|prefix| ought to contain at least one period, to avoid matching too"
      << " many histograms.";

  CountsMap result;

  // Find candidate matches by using the logic built into GetSnapshot().
  for (const HistogramBase* histogram : StatisticsRecorder::GetHistograms()) {
    if (!StartsWith(histogram->histogram_name(), prefix,
                    CompareCase::SENSITIVE)) {
      continue;
    }
    std::unique_ptr<HistogramSamples> new_samples =
        GetHistogramSamplesSinceCreation(histogram->histogram_name());
    // Omit unchanged histograms from the result.
    if (new_samples->TotalCount()) {
      result[histogram->histogram_name()] = new_samples->TotalCount();
    }
  }
  return result;
}

std::unique_ptr<HistogramSamples>
HistogramTester::GetHistogramSamplesSinceCreation(
    StringPiece histogram_name) const {
  HistogramBase* histogram = StatisticsRecorder::FindHistogram(histogram_name);
  // Whether the histogram exists or not may not depend on the current test
  // calling this method, but rather on which tests ran before and possibly
  // generated a histogram or not (see http://crbug.com/473689). To provide a
  // response which is independent of the previously run tests, this method
  // creates empty samples in the absence of the histogram, rather than
  // returning null.
  if (!histogram) {
    return std::unique_ptr<HistogramSamples>(
        new SampleMap(HashMetricName(histogram_name)));
  }
  std::unique_ptr<HistogramSamples> named_samples =
      histogram->SnapshotSamples();
  auto original_samples_it = histograms_snapshot_.find(histogram_name);
  if (original_samples_it != histograms_snapshot_.end())
    named_samples->Subtract(*original_samples_it->second.get());
  return named_samples;
}

std::string HistogramTester::GetAllHistogramsRecorded() const {
  std::string output;

  for (const auto* const histogram : StatisticsRecorder::GetHistograms()) {
    std::unique_ptr<HistogramSamples> named_samples =
        histogram->SnapshotSamples();

    for (const auto& histogram_data : histograms_snapshot_) {
      if (histogram_data.first == histogram->histogram_name())
        named_samples->Subtract(*histogram_data.second);
    }

    if (named_samples->TotalCount()) {
      auto current_count = histogram->SnapshotSamples()->TotalCount();
      StringAppendF(&output, "Histogram: %s recorded %d new samples.\n",
                    histogram->histogram_name(), named_samples->TotalCount());
      if (current_count != named_samples->TotalCount()) {
        StringAppendF(&output,
                      "WARNING: There were samples recorded to this histogram "
                      "before tester instantiation.\n");
      }
      histogram->WriteAscii(&output);
      StringAppendF(&output, "\n");
    }
  }

  return output;
}

void HistogramTester::CheckBucketCount(StringPiece name,
                                       HistogramBase::Sample sample,
                                       HistogramBase::Count expected_count,
                                       const HistogramSamples& samples) const {
  int actual_count;
  GetBucketCountForSamples(name, sample, samples, &actual_count);

  EXPECT_EQ(expected_count, actual_count)
      << "Histogram \"" << name
      << "\" does not have the right number of samples (" << expected_count
      << ") in the expected bucket (" << sample << "). It has (" << actual_count
      << ").";
}

void HistogramTester::CheckTotalCount(StringPiece name,
                                      HistogramBase::Count expected_count,
                                      const HistogramSamples& samples) const {
  int actual_count = samples.TotalCount();
  auto histogram_data = histograms_snapshot_.find(name);
  if (histogram_data != histograms_snapshot_.end())
    actual_count -= histogram_data->second->TotalCount();

  EXPECT_EQ(expected_count, actual_count)
      << "Histogram \"" << name
      << "\" does not have the right total number of samples ("
      << expected_count << "). It has (" << actual_count << ").";
}

bool Bucket::operator==(const Bucket& other) const {
  return min == other.min && count == other.count;
}

void PrintTo(const Bucket& bucket, std::ostream* os) {
  *os << "Bucket " << bucket.min << ": " << bucket.count;
}

}  // namespace base
