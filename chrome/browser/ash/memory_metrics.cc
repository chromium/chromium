// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/memory_metrics.h"

#include <stddef.h>

#include <cinttypes>
#include <map>
#include <string>
#include <vector>

#include "ash/constants/ash_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/statistics_recorder.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/metrics/chromeos_metrics_provider.h"
#include "chromeos/ash/components/memory/memory.h"
#include "components/metrics/serialization/metric_sample.h"
#include "components/metrics/serialization/serialization_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace ash {

namespace {

// Name of the histogram that represents the success and various failure modes
// for parsing PSI memory data.
const char kPSIMemoryPressureSomeName[] = "ChromeOS.CWP.PSIMemPressure.Some";
const char kPSIMemoryPressureFullName[] = "ChromeOS.CWP.PSIMemPressure.Full";

// File path that stores PSI Memory data.
const char kPSIMemoryPath[] = "/proc/pressure/memory";

}  // namespace

MemoryMetrics::MemoryMetrics(uint32_t period)
    : memory_psi_file_(kPSIMemoryPath), parser_(period) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DETACH_FROM_SEQUENCE(background_sequence_checker_);

  zram_metrics_ = base::MakeRefCounted<memory::ZramMetrics>();

  collection_interval_ = base::Seconds(parser_.GetPeriod());
  runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
}

MemoryMetrics::~MemoryMetrics() = default;

void MemoryMetrics::Start() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Start the collection on the background sequence.
  runner_->PostTask(FROM_HERE,
                    base::BindOnce(&MemoryMetrics::ScheduleCollector, this));
}

void MemoryMetrics::Stop() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Stop the collection on the background sequence.
  runner_->PostTask(FROM_HERE,
                    base::BindOnce(&MemoryMetrics::CancelTimer, this));
}

void MemoryMetrics::CollectEvents() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(background_sequence_checker_);

  const bool zram_success = zram_metrics_->CollectEvents();

  std::string content;
  int metric_some;
  int metric_full;

  if (!base::ReadFileToString(base::FilePath(memory_psi_file_), &content)) {
    parser_.LogParseStatus(metrics::ParsePSIMemStatus::kReadFileFailed);
    return;
  }

  auto stat = parser_.ParseMetrics(content, &metric_some, &metric_full);
  parser_.LogParseStatus(stat);  // Log success and failure, for histograms.
  if (stat != metrics::ParsePSIMemStatus::kSuccess) {
    return;
  }

  base::UmaHistogramCustomCounts(
      kPSIMemoryPressureSomeName, metric_some, metrics::kMemPressureMin,
      metrics::kMemPressureExclusiveMax, metrics::kMemPressureHistogramBuckets);
  base::UmaHistogramCustomCounts(
      kPSIMemoryPressureFullName, metric_full, metrics::kMemPressureMin,
      metrics::kMemPressureExclusiveMax, metrics::kMemPressureHistogramBuckets);

  // Emit a composite metric consisting of the cross product of
  // orig_data_size_mb_ and metric_some.
  if (zram_success) {
    // We use exactly 15 buckets for zram, each of size 1GB except for the last
    // which is unbounded. This means we have buckets: [0, 1), [1, 2), ..., [14,
    // infinity).
    constexpr int kZramBucketCount = 15;
    int zram_bucket = zram_metrics_->orig_data_size_mb() / 1024;
    zram_bucket = std::min(kZramBucketCount - 1, zram_bucket);

    // We use exactly 20 buckets for metric_some of width 5 between 0 and 100.
    constexpr int kPsiBucketWidth = 5;
    constexpr int kPsiBucketCount = 100 / kPsiBucketWidth;
    int psi_bucket = metric_some / kPsiBucketWidth;
    psi_bucket = std::min(kPsiBucketCount - 1, psi_bucket);

    int composite_bucket = zram_bucket * kPsiBucketCount + psi_bucket;

    UMA_HISTOGRAM_ENUMERATION("ChromeOS.Zram.PSISomeOrigDataSizeMB",
                              composite_bucket,
                              kZramBucketCount * kPsiBucketCount);
  }
}

void MemoryMetrics::ScheduleCollector() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(background_sequence_checker_);
  timer_ = std::make_unique<base::RepeatingTimer>();
  timer_->Start(FROM_HERE, collection_interval_,
                base::BindRepeating(&MemoryMetrics::CollectEvents, this));
}

void MemoryMetrics::CancelTimer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(background_sequence_checker_);
  timer_.reset();
}

}  // namespace ash
