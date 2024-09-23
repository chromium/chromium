// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"

#include <stddef.h>

#include <string_view>

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
    std::string_view name,
    HistogramBase::Sample sample,
    HistogramBase::Count expected_bucket_count,
    const Location& location) const {
  HistogramBase* histogram = StatisticsRecorder::FindHistogram(name);
  if (histogram) {
    int actual_bucket_count;
    int actual_total_count;
    GetBucketCountForSamples(*histogram, sample, &actual_bucket_count,
                             &actual_total_count);

    EXPECT_TRUE(expected_bucket_count == actual_bucket_count &&
                expected_bucket_count == actual_total_count)
        << "Histogram \"" << name << "\" did not meet its expectations.\n"
        << "Bucket " << sample << " should contain " << expected_bucket_count
        << " samples and contained " << actual_bucket_count << " samples.\n"
        << "The total count of samples in the histogram should be "
        << expected_bucket_count << " and was " << actual_total_count << ".\n"
        << SnapshotToString(*histogram) << "\n"
        << "(expected at " << location.ToString() << ")";
  } else {
    // No histogram means there were zero samples.
    EXPECT_EQ(0, expected_bucket_count)
        << "Zero samples found for Histogram \"" << name << "\".\n"
        << "(expected at " << location.ToString() << ")";
    return;
  }
}

void HistogramTester::ExpectUniqueTimeSample(
    std::string_view name,
    TimeDelta sample,
    HistogramBase::Count expected_bucket_count,
    const Location& location) const {
  ExpectUniqueSample(name, sample.InMilliseconds(), expected_bucket_count,
                     location);
}

void HistogramTester::ExpectBucketCount(std::string_view name,
                                        HistogramBase::Sample sample,
                                        HistogramBase::Count expected_count,
                                        const Location& location) const {
  HistogramBase* histogram = StatisticsRecorder::FindHistogram(name);
  if (histogram) {
    int actual_count;
    GetBucketCountForSamples(*histogram, sample, &actual_count,
                             /*total_count=*/nullptr);

    EXPECT_EQ(expected_count, actual_count)
        << "Histogram \"" << name
        << "\" does not have the right number of samples (" << expected_count
        << ") in the expected bucket (" << sample << "). It has ("
        << actual_count << ").\n"
        << SnapshotToString(*histogram) << "\n"
        << "(expected at " << location.ToString() << ")";
  } else {
    // No histogram means there were zero samples.
    EXPECT_EQ(0, expected_count)
        << "Histogram \"" << name << "\" does not exist. "
        << "(expected at " << location.ToString() << ")";
  }
}

void HistogramTester::ExpectTotalCount(std::string_view name,
                                       HistogramBase::Count expected_count,
                                       const Location& location) const {
  HistogramBase* histogram = StatisticsRecorder::FindHistogram(name);
  if (histogram) {
    int actual_count = GetTotalCountForSamples(*histogram);

    EXPECT_EQ(expected_count, actual_count)
        << "Histogram \"" << name
        << "\" does not have the right total number of samples ("
        << expected_count << "). It has (" << actual_count << ").\n"
        << SnapshotToString(*histogram) << "\n"
        << "(expected at " << location.ToString() << ")";
  } else {
    // No histogram means there were zero samples.
    EXPECT_EQ(0, expected_count)
        << "Histogram \"" << name << "\" does not exist. "
        << "(expected at " << location.ToString() << ")";
  }
}

void HistogramTester::ExpectTimeBucketCount(std::string_view name,
                                            TimeDelta sample,
                                            HistogramBase::Count count,
                                            const Location& location) const {
  ExpectBucketCount(name, sample.InMilliseconds(), count, location);
}

int64_t HistogramTester::GetTotalSum(std::string_view name) const {
  HistogramBase* histogram = StatisticsRecorder::FindHistogram(name);
  if (!histogram)
    return 0;

  int64_t original_sum = 0;
  auto original_samples_it = histograms_snapshot_.find(name);
  if (original_samples_it != histograms_snapshot_.end())
    original_sum = original_samples_it->second->sum();

  std::unique_ptr<HistogramSamples> samples = histogram->SnapshotSamples();
  return samples->sum() - original_sum;
}

std::vector<Bucket> HistogramTester::GetAllSamples(
    std::string_view name) const {
  std::vector<Bucket> samples;
  std::unique_ptr<HistogramSamples> snapshot =
      GetHistogramSamplesSinceCreation(name);
  if (snapshot) {
    for (auto it = snapshot->Iterator(); !it->Done(); it->Next()) {
      HistogramBase::Sample sample;
      int64_t max;
      HistogramBase::Count count;
      it->Get(&sample, &max, &count);
      samples.emplace_back(sample, count);
    }
  }
  return samples;
}

HistogramBase::Count HistogramTester::GetBucketCount(
    std::string_view name,
    HistogramBase::Sample sample) const {
  HistogramBase* histogram = StatisticsRecorder::FindHistogram(name);
  HistogramBase::Count count = 0;
  if (histogram) {
    GetBucketCountForSamples(*histogram, sample, &count,
                             /*total_count=*/nullptr);
  }
  return count;
}

void HistogramTester::GetBucketCountForSamples(
    const HistogramBase& histogram,
    HistogramBase::Sample sample,
    HistogramBase::Count* count,
    HistogramBase::Count* total_count) const {
  std::unique_ptr<HistogramSamples> samples = histogram.SnapshotSamples();
  *count = samples->GetCount(sample);
  if (total_count)
    *total_count = samples->TotalCount();
  auto histogram_data = histograms_snapshot_.find(histogram.histogram_name());
  if (histogram_data != histograms_snapshot_.end()) {
    *count -= histogram_data->second->GetCount(sample);
    if (total_count)
      *total_count -= histogram_data->second->TotalCount();
  }
}

HistogramTester::CountsMap HistogramTester::GetTotalCountsForPrefix(
    std::string_view prefix) const {
  EXPECT_TRUE(prefix.find('.') != std::string_view::npos)
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
    std::string_view histogram_name) const {
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

int HistogramTester::GetTotalCountForSamples(
    const base::HistogramBase& histogram) const {
  std::unique_ptr<HistogramSamples> samples = histogram.SnapshotSamples();
  int actual_count = samples->TotalCount();
  auto histogram_data = histograms_snapshot_.find(histogram.histogram_name());
  if (histogram_data != histograms_snapshot_.end())
    actual_count -= histogram_data->second->TotalCount();
  return actual_count;
}

std::string HistogramTester::SnapshotToString(
    const base::HistogramBase& histogram) const {
  std::unique_ptr<HistogramSamples> snapshot =
      GetHistogramSamplesSinceCreation(histogram.histogram_name());

  base::Value::Dict graph_dict =
      snapshot->ToGraphDict(histogram.histogram_name(), histogram.flags());
  std::string tmp;
  // The header message describes this histogram samples (name of the histogram
  // and median of the samples). The body contains an ASCII art histogram of the
  // samples.
  tmp.append(*graph_dict.FindString("header"));
  tmp.append("\n");
  tmp.append(*graph_dict.FindString("body"));
  return tmp;
}

void PrintTo(const Bucket& bucket, std::ostream* os) {
  *os << "Bucket " << bucket.min << ": " << bucket.count;
}

}  // namespace base
