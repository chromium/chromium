// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/psi_memory_metrics.h"

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

// Name of the histogram that represents the success and various failure modes
// for parsing PSI memory data.
const char kPSIMemoryPressureSomeName[] = "ChromeOS.CWP.PSIMemPressure.Some";
const char kPSIMemoryPressureFullName[] = "ChromeOS.CWP.PSIMemPressure.Full";

// File path that stores PSI Memory data.
const char kPSIMemoryPath[] = "/proc/pressure/memory";

}  // namespace

PSIMemoryMetrics::PSIMemoryMetrics(uint32_t period)
    : memory_psi_file_(kPSIMemoryPath), parser_(period) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DETACH_FROM_SEQUENCE(background_sequence_checker_);

  collection_interval_ = base::Seconds(parser_.GetPeriod());
  runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
}

PSIMemoryMetrics::~PSIMemoryMetrics() = default;

void PSIMemoryMetrics::Start() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Start the collection on the background sequence.
  runner_->PostTask(FROM_HERE,
                    base::BindOnce(&PSIMemoryMetrics::ScheduleCollector, this));
}

void PSIMemoryMetrics::Stop() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Stop the collection on the background sequence.
  runner_->PostTask(FROM_HERE,
                    base::BindOnce(&PSIMemoryMetrics::CancelTimer, this));
}

void PSIMemoryMetrics::CollectEvents() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(background_sequence_checker_);

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
}

void PSIMemoryMetrics::ScheduleCollector() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(background_sequence_checker_);
  timer_ = std::make_unique<base::RepeatingTimer>();
  timer_->Start(FROM_HERE, collection_interval_,
                base::BindRepeating(&PSIMemoryMetrics::CollectEvents, this));
}

void PSIMemoryMetrics::CancelTimer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(background_sequence_checker_);
  timer_.reset();
}

}  // namespace ash
