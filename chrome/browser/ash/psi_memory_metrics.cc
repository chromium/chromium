// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/psi_memory_metrics.h"

#include <stddef.h>

#include <cinttypes>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ash/constants/ash_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/statistics_recorder.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/metrics/chromeos_metrics_provider.h"
#include "components/metrics/serialization/metric_sample.h"
#include "components/metrics/serialization/serialization_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace ash {

namespace {

// Default interval between externally-reported metrics being collected.
constexpr base::TimeDelta kMinCollectionInterval = base::Seconds(10);
constexpr base::TimeDelta kMidCollectionInterval = base::Seconds(60);
constexpr base::TimeDelta kMaxCollectionInterval = base::Seconds(300);

constexpr base::TimeDelta kDefaultCollectionInterval = kMinCollectionInterval;

// Name of the histogram that represents the success and various failure modes
// for parsing PSI memory data.
const char kParsePSIMemoryHistogramName[] = "ChromeOS.CWP.ParsePSIMemory";
const char kPSIMemoryPressureSomeName[] = "ChromeOS.CWP.PSIMemPressure.Some";
const char kPSIMemoryPressureFullName[] = "ChromeOS.CWP.PSIMemPressure.Full";

// File path that stores PSI Memory data.
const char kPSIMemoryPath[] = "/proc/pressure/memory";

constexpr base::StringPiece kContentPrefixSome = "some";
constexpr base::StringPiece kContentPrefixFull = "full";
constexpr base::StringPiece kContentTerminator = " total=";
constexpr base::StringPiece kMetricTerminator = " ";

const char kMetricPrefixFormat[] = "avg%" PRId64 "=";

// Values as logged in the histogram for memory pressure.
constexpr int kMemPressureMin = 1;  // As 0 is for underflow.
constexpr int kMemPressureExclusiveMax = 10000;
constexpr int kMemPressureHistogramBuckets = 100;

}  // namespace

PSIMemoryMetrics::PSIMemoryMetrics(uint32_t period)
    : memory_psi_file_(kPSIMemoryPath),
      collection_interval_(kDefaultCollectionInterval) {
  if (period == kMinCollectionInterval.InSeconds() ||
      period == kMidCollectionInterval.InSeconds() ||
      period == kMaxCollectionInterval.InSeconds()) {
    collection_interval_ = base::Seconds(period);
  } else {
    LOG(WARNING) << "Ignoring invalid interval [" << period << "]";
  }

  metric_prefix_ =
      base::StringPrintf(kMetricPrefixFormat, collection_interval_.InSeconds());

  runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
}

PSIMemoryMetrics::~PSIMemoryMetrics() = default;

void PSIMemoryMetrics::Start() {
  ScheduleCollector();
}

void PSIMemoryMetrics::CancelTimer() {
  if (last_timer_.IsValid()) {
    last_timer_.CancelTask();
  }
}

void PSIMemoryMetrics::Stop() {
  stopped_.Set();

  // Note that you can't call last_timer.CancelTask() from here,
  // as we may not be running in the correct sequence.
  runner_->PostTask(FROM_HERE,
                    base::BindOnce(&PSIMemoryMetrics::CancelTimer, this));
}

int PSIMemoryMetrics::GetMetricValue(const std::string& content,
                                     size_t start,
                                     size_t end) {
  size_t value_start;
  size_t value_end;
  if (!internal::FindMiddleString(content, start, metric_prefix_,
                                  kMetricTerminator, &value_start,
                                  &value_end)) {
    return -1;
  }
  if (value_end > end) {
    return -1;  // Out of bounds of the search area.
  }

  double n;
  const base::StringPiece metric_value_text(content.c_str() + value_start,
                                            value_end - value_start);
  if (!base::StringToDouble(metric_value_text, &n)) {
    return -1;  // Unable to convert string to number
  }

  // Want to multiply by 100, but to avoid integer truncation,
  // do best-effort rounding.
  const int preround = static_cast<int>(n * 1000);
  return (preround + 5) / 10;
}

PSIMemoryMetrics::ParsePSIMemStatus PSIMemoryMetrics::ParseMetrics(
    const std::string& content,
    int* metric_some,
    int* metric_full) {
  size_t str_some_start;
  size_t str_some_end;
  size_t str_full_start;
  size_t str_full_end;

  DCHECK_NE(metric_some, nullptr);
  DCHECK_NE(metric_full, nullptr);

  if (!internal::FindMiddleString(content, 0, kContentPrefixSome,
                                  kContentTerminator, &str_some_start,
                                  &str_some_end)) {
    return ParsePSIMemStatus::kUnexpectedDataFormat;
  }

  if (!internal::FindMiddleString(content,
                                  str_some_end + kContentTerminator.length(),
                                  kContentPrefixFull, kContentTerminator,
                                  &str_full_start, &str_full_end)) {
    return ParsePSIMemStatus::kUnexpectedDataFormat;
  }

  int compute_some = GetMetricValue(content, str_some_start, str_some_end);
  if (compute_some < 0) {
    return ParsePSIMemStatus::kInvalidMetricFormat;
  }

  int compute_full = GetMetricValue(content, str_full_start, str_full_end);
  if (compute_full < 0) {
    return ParsePSIMemStatus::kInvalidMetricFormat;
  }

  *metric_some = compute_some;
  *metric_full = compute_full;

  return ParsePSIMemStatus::kSuccess;
}

PSIMemoryMetrics::ParsePSIMemStatus PSIMemoryMetrics::CollectEvents() {
  // Example file content:
  // some avg10=0.00 avg60=0.00 avg300=0.00 total=417963
  //  full avg10=0.00 avg60=0.00 avg300=0.00 total=205933
  // we will pick one of the columns depending on the colleciton period set
  std::string content;
  int metric_some;
  int metric_full;
  PSIMemoryMetrics::ParsePSIMemStatus stat;

  if (!base::ReadFileToString(base::FilePath(memory_psi_file_), &content)) {
    return ParsePSIMemStatus::kReadFileFailed;
  }

  stat = ParseMetrics(content, &metric_some, &metric_full);

  if (stat != ParsePSIMemStatus::kSuccess) {
    return stat;
  }

  base::UmaHistogramCustomCounts(kPSIMemoryPressureSomeName, metric_some,
                                 kMemPressureMin, kMemPressureExclusiveMax,
                                 kMemPressureHistogramBuckets);

  base::UmaHistogramCustomCounts(kPSIMemoryPressureFullName, metric_full,
                                 kMemPressureMin, kMemPressureExclusiveMax,
                                 kMemPressureHistogramBuckets);

  return ParsePSIMemStatus::kSuccess;
}

void PSIMemoryMetrics::CollectEventsAndReschedule() {
  if (stopped_.IsSet()) {
    return;
  }

  ParsePSIMemStatus stat = CollectEvents();
  constexpr int statCeiling =
      static_cast<int>(ParsePSIMemStatus::kMaxValue) + 1;
  base::UmaHistogramExactLinear(kParsePSIMemoryHistogramName,
                                static_cast<int>(stat), statCeiling);

  ScheduleCollector();
}

void PSIMemoryMetrics::ScheduleCollector() {
  if (stopped_.IsSet()) {
    return;
  }

  last_timer_ = runner_->PostCancelableDelayedTask(
      FROM_HERE,
      base::BindOnce(&PSIMemoryMetrics::CollectEventsAndReschedule, this),
      collection_interval_);
}

namespace internal {

bool FindMiddleString(const base::StringPiece& content,
                      size_t search_start,
                      const base::StringPiece& prefix,
                      const base::StringPiece& suffix,
                      size_t* start,
                      size_t* end) {
  DCHECK_NE(start, nullptr);
  DCHECK_NE(end, nullptr);

  size_t compute_start = content.find(prefix, search_start);
  if (compute_start == std::string::npos) {
    return false;
  }
  compute_start += prefix.length();

  size_t compute_end = content.find(suffix, compute_start);
  if (compute_end == std::string::npos) {
    return false;
  }

  *start = compute_start;
  *end = compute_end;

  return true;
}

}  // namespace internal

}  // namespace ash
