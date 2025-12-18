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
constexpr const char kTraceCategoryForPressureEvents[] = "resources";

}  // anonymous namespace

class PressureMetricsWorker final
    : public base::trace_event::TraceSessionObserver {
 public:
  PressureMetricsWorker();
  ~PressureMetricsWorker() override;

  // base::trace_event::TraceSessionObserver implementation:
  void OnStart(const perfetto::DataSourceBase::StartArgs&) override;

 private:
  void ReadAndEmitCounters();
  void ReadAndEmitUMA();

  std::vector<PressureMetrics> pressure_metrics_;
  base::RepeatingTimer metrics_sampling_timer_;
  base::RepeatingTimer tracing_sampling_timer_;
};

PressureMetricsWorker::PressureMetricsWorker() {
  base::trace_event::TraceSessionObserverList::AddObserver(this);
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
  // It is possible with startup tracing that tracing was enabled before this
  // class has register its observer.
  OnStart({});
}

PressureMetricsWorker::~PressureMetricsWorker() {
  base::trace_event::TraceSessionObserverList::RemoveObserver(this);
}

void PressureMetricsWorker::OnStart(
    const perfetto::DataSourceBase::StartArgs&) {
  if (tracing_sampling_timer_.IsRunning()) {
    return;
  }
  if (TRACE_EVENT_CATEGORY_ENABLED(kTraceCategoryForPressureEvents)) {
    // It is safe to use base::Unretained(this) since the timer is own by this
    // class.
    tracing_sampling_timer_.Start(
        FROM_HERE, kDelayBetweenTracingSamples,
        base::BindRepeating(&PressureMetricsWorker::ReadAndEmitCounters,
                            base::Unretained(this)));
  }
}

void PressureMetricsWorker::ReadAndEmitCounters() {
  if (!TRACE_EVENT_CATEGORY_ENABLED(kTraceCategoryForPressureEvents)) {
    tracing_sampling_timer_.Stop();
    return;
  }
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
}

PressureMetricsReporter::~PressureMetricsReporter() {
}
