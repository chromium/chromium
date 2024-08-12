// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/histogram_functions.h"

#include <string_view>

#include "base/metrics/histogram.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/sparse_histogram.h"
#include "base/time/time.h"

namespace base {

void UmaHistogramBoolean(std::string_view name, bool sample) {
  HistogramBase* histogram = BooleanHistogram::FactoryGet(
      name, HistogramBase::kUmaTargetedHistogramFlag);
  histogram->Add(sample);
}

void UmaHistogramBoolean(const std::string& name, bool sample) {
  HistogramBase* histogram = BooleanHistogram::FactoryGet(
      name, HistogramBase::kUmaTargetedHistogramFlag);
  histogram->Add(sample);
}

void UmaHistogramBoolean(const char* name, bool sample) {
  HistogramBase* histogram = BooleanHistogram::FactoryGet(
      name, HistogramBase::kUmaTargetedHistogramFlag);
  histogram->Add(sample);
}

void UmaHistogramExactLinear(std::string_view name,
                             int sample,
                             int exclusive_max) {
  HistogramBase* histogram = LinearHistogram::FactoryGet(
      name, 1, exclusive_max, static_cast<size_t>(exclusive_max + 1),
      HistogramBase::kUmaTargetedHistogramFlag);
  histogram->Add(sample);
}

void UmaHistogramExactLinear(const std::string& name,
                             int sample,
                             int exclusive_max) {
  HistogramBase* histogram = LinearHistogram::FactoryGet(
      name, 1, exclusive_max, static_cast<size_t>(exclusive_max + 1),
      HistogramBase::kUmaTargetedHistogramFlag);
  histogram->Add(sample);
}

void UmaHistogramExactLinear(const char* name, int sample, int exclusive_max) {
  HistogramBase* histogram = LinearHistogram::FactoryGet(
      name, 1, exclusive_max, static_cast<size_t>(exclusive_max + 1),
      HistogramBase::kUmaTargetedHistogramFlag);
  histogram->Add(sample);
}

void UmaHistogramPercentage(std::string_view name, int percent) {
  UmaHistogramExactLinear(name, percent, 101);
}

void UmaHistogramPercentage(const std::string& name, int percent) {
  UmaHistogramExactLinear(name, percent, 101);
}

void UmaHistogramPercentage(const char* name, int percent) {
  UmaHistogramExactLinear(name, percent, 101);
}

void UmaHistogramPercentageObsoleteDoNotUse(std::string_view name,
                                            int percent) {
  UmaHistogramExactLinear(name, percent, 100);
}

void UmaHistogramPercentageObsoleteDoNotUse(const std::string& name,
                                            int percent) {
  UmaHistogramExactLinear(name, percent, 100);
}

void UmaHistogramPercentageObsoleteDoNotUse(const char* name, int percent) {
  UmaHistogramExactLinear(name, percent, 100);
}

void UmaHistogramCustomCounts(std::string_view name,
                              int sample,
                              int min,
                              int exclusive_max,
                              size_t buckets) {
  HistogramBase* histogram =
      Histogram::FactoryGet(name, min, exclusive_max, buckets,
                            HistogramBase::kUmaTargetedHistogramFlag);
  histogram->Add(sample);
}

void UmaHistogramCustomCounts(const std::string& name,
                              int sample,
                              int min,
                              int exclusive_max,
                              size_t buckets) {
  HistogramBase* histogram =
      Histogram::FactoryGet(name, min, exclusive_max, buckets,
                            HistogramBase::kUmaTargetedHistogramFlag);
  histogram->Add(sample);
}

void UmaHistogramCustomCounts(const char* name,
                              int sample,
                              int min,
                              int exclusive_max,
                              size_t buckets) {
  HistogramBase* histogram =
      Histogram::FactoryGet(name, min, exclusive_max, buckets,
                            HistogramBase::kUmaTargetedHistogramFlag);
  histogram->Add(sample);
}

void UmaHistogramCounts100(std::string_view name, int sample) {
  UmaHistogramCustomCounts(name, sample, 1, 100, 50);
}

void UmaHistogramCounts100(const std::string& name, int sample) {
  UmaHistogramCustomCounts(name, sample, 1, 100, 50);
}

void UmaHistogramCounts100(const char* name, int sample) {
  UmaHistogramCustomCounts(name, sample, 1, 100, 50);
}

void UmaHistogramCounts1000(std::string_view name, int sample) {
  UmaHistogramCustomCounts(name, sample, 1, 1000, 50);
}

void UmaHistogramCounts1000(const std::string& name, int sample) {
  UmaHistogramCustomCounts(name, sample, 1, 1000, 50);
}

void UmaHistogramCounts1000(const char* name, int sample) {
  UmaHistogramCustomCounts(name, sample, 1, 1000, 50);
}

void UmaHistogramCounts10000(std::string_view name, int sample) {
  UmaHistogramCustomCounts(name, sample, 1, 10000, 50);
}

void UmaHistogramCounts10000(const std::string& name, int sample) {
  UmaHistogramCustomCounts(name, sample, 1, 10000, 50);
}

void UmaHistogramCounts10000(const char* name, int sample) {
  UmaHistogramCustomCounts(name, sample, 1, 10000, 50);
}

void UmaHistogramCounts100000(std::string_view name, int sample) {
  UmaHistogramCustomCounts(name, sample, 1, 100000, 50);
}

void UmaHistogramCounts100000(const std::string& name, int sample) {
  UmaHistogramCustomCounts(name, sample, 1, 100000, 50);
}

void UmaHistogramCounts100000(const char* name, int sample) {
  UmaHistogramCustomCounts(name, sample, 1, 100000, 50);
}

void UmaHistogramCounts1M(std::string_view name, int sample) {
  UmaHistogramCustomCounts(name, sample, 1, 1000000, 50);
}

void UmaHistogramCounts1M(const std::string& name, int sample) {
  UmaHistogramCustomCounts(name, sample, 1, 1000000, 50);
}

void UmaHistogramCounts1M(const char* name, int sample) {
  UmaHistogramCustomCounts(name, sample, 1, 1000000, 50);
}

void UmaHistogramCounts10M(std::string_view name, int sample) {
  UmaHistogramCustomCounts(name, sample, 1, 10000000, 50);
}

void UmaHistogramCounts10M(const std::string& name, int sample) {
  UmaHistogramCustomCounts(name, sample, 1, 10000000, 50);
}

void UmaHistogramCounts10M(const char* name, int sample) {
  UmaHistogramCustomCounts(name, sample, 1, 10000000, 50);
}

void UmaHistogramCustomTimes(std::string_view name,
                             TimeDelta sample,
                             TimeDelta min,
                             TimeDelta max,
                             size_t buckets) {
  HistogramBase* histogram = Histogram::FactoryTimeGet(
      name, min, max, buckets, HistogramBase::kUmaTargetedHistogramFlag);
  histogram->AddTimeMillisecondsGranularity(sample);
}

void UmaHistogramCustomTimes(const std::string& name,
                             TimeDelta sample,
                             TimeDelta min,
                             TimeDelta max,
                             size_t buckets) {
  HistogramBase* histogram = Histogram::FactoryTimeGet(
      name, min, max, buckets, HistogramBase::kUmaTargetedHistogramFlag);
  histogram->AddTimeMillisecondsGranularity(sample);
}

void UmaHistogramCustomTimes(const char* name,
                             TimeDelta sample,
                             TimeDelta min,
                             TimeDelta max,
                             size_t buckets) {
  HistogramBase* histogram = Histogram::FactoryTimeGet(
      name, min, max, buckets, HistogramBase::kUmaTargetedHistogramFlag);
  histogram->AddTimeMillisecondsGranularity(sample);
}

void UmaHistogramTimes(std::string_view name, TimeDelta sample) {
  UmaHistogramCustomTimes(name, sample, Milliseconds(1), Seconds(10), 50);
}

void UmaHistogramTimes(const std::string& name, TimeDelta sample) {
  UmaHistogramCustomTimes(name, sample, Milliseconds(1), Seconds(10), 50);
}

void UmaHistogramTimes(const char* name, TimeDelta sample) {
  UmaHistogramCustomTimes(name, sample, Milliseconds(1), Seconds(10), 50);
}

void UmaHistogramMediumTimes(std::string_view name, TimeDelta sample) {
  UmaHistogramCustomTimes(name, sample, Milliseconds(1), Minutes(3), 50);
}

void UmaHistogramMediumTimes(const std::string& name, TimeDelta sample) {
  UmaHistogramCustomTimes(name, sample, Milliseconds(1), Minutes(3), 50);
}

void UmaHistogramMediumTimes(const char* name, TimeDelta sample) {
  UmaHistogramCustomTimes(name, sample, Milliseconds(1), Minutes(3), 50);
}

void UmaHistogramLongTimes(std::string_view name, TimeDelta sample) {
  UmaHistogramCustomTimes(name, sample, Milliseconds(1), Hours(1), 50);
}

void UmaHistogramLongTimes(const std::string& name, TimeDelta sample) {
  UmaHistogramCustomTimes(name, sample, Milliseconds(1), Hours(1), 50);
}

void UmaHistogramLongTimes(const char* name, TimeDelta sample) {
  UmaHistogramCustomTimes(name, sample, Milliseconds(1), Hours(1), 50);
}

void UmaHistogramLongTimes100(std::string_view name, TimeDelta sample) {
  UmaHistogramCustomTimes(name, sample, Milliseconds(1), Hours(1), 100);
}

void UmaHistogramLongTimes100(const std::string& name, TimeDelta sample) {
  UmaHistogramCustomTimes(name, sample, Milliseconds(1), Hours(1), 100);
}

void UmaHistogramLongTimes100(const char* name, TimeDelta sample) {
  UmaHistogramCustomTimes(name, sample, Milliseconds(1), Hours(1), 100);
}

void UmaHistogramCustomMicrosecondsTimes(std::string_view name,
                                         TimeDelta sample,
                                         TimeDelta min,
                                         TimeDelta max,
                                         size_t buckets) {
  HistogramBase* histogram = Histogram::FactoryMicrosecondsTimeGet(
      name, min, max, buckets, HistogramBase::kUmaTargetedHistogramFlag);
  histogram->AddTimeMicrosecondsGranularity(sample);
}

void UmaHistogramCustomMicrosecondsTimes(const std::string& name,
                                         TimeDelta sample,
                                         TimeDelta min,
                                         TimeDelta max,
                                         size_t buckets) {
  HistogramBase* histogram = Histogram::FactoryMicrosecondsTimeGet(
      name, min, max, buckets, HistogramBase::kUmaTargetedHistogramFlag);
  histogram->AddTimeMicrosecondsGranularity(sample);
}

void UmaHistogramCustomMicrosecondsTimes(const char* name,
                                         TimeDelta sample,
                                         TimeDelta min,
                                         TimeDelta max,
                                         size_t buckets) {
  HistogramBase* histogram = Histogram::FactoryMicrosecondsTimeGet(
      name, min, max, buckets, HistogramBase::kUmaTargetedHistogramFlag);
  histogram->AddTimeMicrosecondsGranularity(sample);
}

void UmaHistogramMicrosecondsTimes(std::string_view name, TimeDelta sample) {
  UmaHistogramCustomMicrosecondsTimes(name, sample, Microseconds(1),
                                      Seconds(10), 50);
}

void UmaHistogramMicrosecondsTimes(const std::string& name, TimeDelta sample) {
  UmaHistogramCustomMicrosecondsTimes(name, sample, Microseconds(1),
                                      Seconds(10), 50);
}

void UmaHistogramMicrosecondsTimes(const char* name, TimeDelta sample) {
  UmaHistogramCustomMicrosecondsTimes(name, sample, Microseconds(1),
                                      Seconds(10), 50);
}

void UmaHistogramMemoryKB(std::string_view name, int sample) {
  UmaHistogramCustomCounts(name, sample, 1000, 500000, 50);
}

void UmaHistogramMemoryKB(const std::string& name, int sample) {
  UmaHistogramCustomCounts(name, sample, 1000, 500000, 50);
}

void UmaHistogramMemoryKB(const char* name, int sample) {
  UmaHistogramCustomCounts(name, sample, 1000, 500000, 50);
}

void UmaHistogramMemoryMB(std::string_view name, int sample) {
  UmaHistogramCustomCounts(name, sample, 1, 1000, 50);
}

void UmaHistogramMemoryMB(const std::string& name, int sample) {
  UmaHistogramCustomCounts(name, sample, 1, 1000, 50);
}

void UmaHistogramMemoryMB(const char* name, int sample) {
  UmaHistogramCustomCounts(name, sample, 1, 1000, 50);
}

void UmaHistogramMemoryLargeMB(std::string_view name, int sample) {
  UmaHistogramCustomCounts(name, sample, 1, 64000, 100);
}

void UmaHistogramMemoryLargeMB(const std::string& name, int sample) {
  UmaHistogramCustomCounts(name, sample, 1, 64000, 100);
}

void UmaHistogramMemoryLargeMB(const char* name, int sample) {
  UmaHistogramCustomCounts(name, sample, 1, 64000, 100);
}

void UmaHistogramSparse(std::string_view name, int sample) {
  HistogramBase* histogram = SparseHistogram::FactoryGet(
      name, HistogramBase::kUmaTargetedHistogramFlag);
  histogram->Add(sample);
}

void UmaHistogramSparse(const std::string& name, int sample) {
  HistogramBase* histogram = SparseHistogram::FactoryGet(
      name, HistogramBase::kUmaTargetedHistogramFlag);
  histogram->Add(sample);
}

void UmaHistogramSparse(const char* name, int sample) {
  HistogramBase* histogram = SparseHistogram::FactoryGet(
      name, HistogramBase::kUmaTargetedHistogramFlag);
  histogram->Add(sample);
}

ScopedUmaHistogramTimer::ScopedUmaHistogramTimer(std::string_view name,
                                                 ScopedHistogramTiming timing)
    : constructed_(base::TimeTicks::Now()), timing_(timing), name_(name) {}

ScopedUmaHistogramTimer::~ScopedUmaHistogramTimer() {
  base::TimeDelta elapsed = base::TimeTicks::Now() - constructed_;
  switch (timing_) {
    case ScopedHistogramTiming::kMicrosecondTimes:
      UmaHistogramMicrosecondsTimes(name_, elapsed);
      break;
    case ScopedHistogramTiming::kShortTimes:
      UmaHistogramTimes(name_, elapsed);
      break;
    case ScopedHistogramTiming::kMediumTimes:
      UmaHistogramMediumTimes(name_, elapsed);
      break;
    case ScopedHistogramTiming::kLongTimes:
      UmaHistogramLongTimes(name_, elapsed);
      break;
  }
}

}  // namespace base
