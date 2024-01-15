// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/pressure/pressure_metrics_reporter.h"

#include <vector>

#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"

namespace {

// Histograms names for the pressure metrics.
const char kCPUPressureHistogramName[] = "System.Pressure.CPU";
const char kIOPressureHistogramName[] = "System.Pressure.IO";
const char kMemoryPressureHistogramName[] = "System.Pressure.Memory";

// Paths for the pressure metrics (Pressure Stall Information).
const base::FilePath::CharType kCPUPressureFilePath[] =
    FILE_PATH_LITERAL("/proc/pressure/cpu");
const base::FilePath::CharType kIOPressureFilePath[] =
    FILE_PATH_LITERAL("/proc/pressure/io");
const base::FilePath::CharType kMemoryPressureFilePath[] =
    FILE_PATH_LITERAL("/proc/pressure/memory");

// The time to wait between UMA samples.
constexpr base::TimeDelta kDelayBetweenSamples = base::Minutes(10);

// The time to wait between trace counters.
constexpr base::TimeDelta kDelayBetweenTracingSamples = base::Seconds(1);

// Tracing caterogies to emit the memory pressure related trace events.
const char kTraceCategoryForPressureEvents[] = "resources";

}  // anonymous namespace

class PressureMetricsWorker {
 public:
  PressureMetricsWorker();
  ~PressureMetricsWorker();

  void OnTracingEnabled();
  void OnTracingDisabled();

 private:
  void ReadAndEmitCounters();
  void ReadAndEmitUMA();

  std::vector<PressureMetrics> pressure_metrics_;
  base::RepeatingTimer metrics_sampling_timer_;
  base::RepeatingTimer tracing_sampling_timer_;
};

PressureMetricsWorker::PressureMetricsWorker() {
  pressure_metrics_.emplace_back(kCPUPressureHistogramName,
                                 base::FilePath(kCPUPressureFilePath));
  pressure_metrics_.emplace_back(kIOPressureHistogramName,
                                 base::FilePath(kIOPressureFilePath));
  pressure_metrics_.emplace_back(kMemoryPressureHistogramName,
                                 base::FilePath(kMemoryPressureFilePath));

  // It is safe to use base::Unretained(this) since the timer is own by this
  // class.
  metrics_sampling_timer_.Start(
      FROM_HERE, kDelayBetweenSamples,
      base::BindRepeating(&PressureMetricsWorker::ReadAndEmitUMA,
                          base::Unretained(this)));
}

PressureMetricsWorker::~PressureMetricsWorker() = default;

void PressureMetricsWorker::OnTracingEnabled() {
  const uint8_t* category_enabled = TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(
      kTraceCategoryForPressureEvents);
  if (*category_enabled) {
    // It is safe to use base::Unretained(this) since the timer is own by this
    // class.
    tracing_sampling_timer_.Start(
        FROM_HERE, kDelayBetweenTracingSamples,
        base::BindRepeating(&PressureMetricsWorker::ReadAndEmitCounters,
                            base::Unretained(this)));
  }
}

void PressureMetricsWorker::OnTracingDisabled() {
  tracing_sampling_timer_.Stop();
}

void PressureMetricsWorker::ReadAndEmitCounters() {
  for (const auto& metric : pressure_metrics_) {
    std::optional<PressureMetrics::Sample> current_pressure =
        metric.CollectCurrentPressure();
    if (current_pressure.has_value()) {
      metric.EmitCounters(current_pressure.value());
    }
  }
}

void PressureMetricsWorker::ReadAndEmitUMA() {
  for (const auto& metric : pressure_metrics_) {
    std::optional<PressureMetrics::Sample> current_pressure =
        metric.CollectCurrentPressure();
    if (current_pressure.has_value()) {
      metric.ReportToUMA(current_pressure.value());
    }
  }
}

PressureMetricsReporter::PressureMetricsReporter()
    : worker_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::trace_event::TraceLog::GetInstance()->AddEnabledStateObserver(this);
  // It is possible with startup tracing that tracing was enabled before this
  // class has register its observer.
  base::trace_event::TraceLog* trace_log =
      base::trace_event::TraceLog::GetInstance();
  if (trace_log->IsEnabled()) {
    OnTraceLogEnabled();
  }
}

PressureMetricsReporter::~PressureMetricsReporter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::trace_event::TraceLog::GetInstance()->RemoveEnabledStateObserver(this);
}

void PressureMetricsReporter::OnTraceLogEnabled() {
  worker_.AsyncCall(&PressureMetricsWorker::OnTracingEnabled);
}

void PressureMetricsReporter::OnTraceLogDisabled() {
  worker_.AsyncCall(&PressureMetricsWorker::OnTracingDisabled);
}
