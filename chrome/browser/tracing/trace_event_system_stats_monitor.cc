// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tracing/trace_event_system_stats_monitor.h"

#include <memory>
#include <string>
#include <utility>

#include "base/json/json_writer.h"
#include "base/process/process_metrics.h"
#include "base/trace_event/trace_event.h"

namespace tracing {

namespace {

using SamplingFrequency = performance_monitor::SystemMonitor::SamplingFrequency;
using MetricsRefreshFrequencies = performance_monitor::SystemMonitor::
    SystemObserver::MetricRefreshFrequencies;

/////////////////////////////////////////////////////////////////////////////
// Holds profiled system stats until the tracing system needs to serialize it.
class SystemStatsHolder : public base::trace_event::ConvertableToTraceFormat {
 public:
  explicit SystemStatsHolder(const base::SystemMetrics& system_metrics)
      : system_metrics_(system_metrics) {}

  SystemStatsHolder(const SystemStatsHolder&) = delete;
  SystemStatsHolder& operator=(const SystemStatsHolder&) = delete;

  ~SystemStatsHolder() override = default;

  // base::trace_event::ConvertableToTraceFormat overrides:
  void AppendAsTraceFormat(std::string* out) const override {
    std::string tmp;
    base::JSONWriter::Write(system_metrics_.ToDict(), &tmp);
    *out += tmp;
  }

 private:
  const base::SystemMetrics system_metrics_;
};

}  // namespace

//////////////////////////////////////////////////////////////////////////////

TraceEventSystemStatsMonitor::TraceEventSystemStatsMonitor() {
  // Force the "system_stats" category to show up in the trace viewer.
  base::trace_event::TraceLog::GetCategoryGroupEnabled(
      TRACE_DISABLED_BY_DEFAULT("system_stats"));

  // Allow this to be instantiated on unsupported platforms, but don't run.
  base::trace_event::TraceLog::GetInstance()->AddAsyncEnabledStateObserver(
      weak_factory_.GetWeakPtr());
}

TraceEventSystemStatsMonitor::~TraceEventSystemStatsMonitor() {
  if (is_profiling_)
    StopProfiling();
  base::trace_event::TraceLog::GetInstance()->RemoveAsyncEnabledStateObserver(
      this);
}

void TraceEventSystemStatsMonitor::OnTraceLogEnabled() {
  // Check to see if system tracing is enabled.
  bool enabled;

  TRACE_EVENT_CATEGORY_GROUP_ENABLED(TRACE_DISABLED_BY_DEFAULT("system_stats"),
                                     &enabled);
  if (!enabled)
    return;
  StartProfiling();
}

void TraceEventSystemStatsMonitor::OnTraceLogDisabled() {
  StopProfiling();
}

void TraceEventSystemStatsMonitor::StartProfiling() {
  // Watch for the tracing framework sending enabling more than once.
  if (is_profiling_)
    return;

  is_profiling_ = true;
  DCHECK(performance_monitor::SystemMonitor::Get());
  performance_monitor::SystemMonitor::Get()->AddOrUpdateObserver(
      this, MetricsRefreshFrequencies::Builder()
                .SetSystemMetricsSamplingFrequency(
                    SamplingFrequency::kDefaultFrequency)
                .Build());
}

void TraceEventSystemStatsMonitor::OnSystemMetricsStruct(
    const base::SystemMetrics& system_metrics) {
  std::unique_ptr<SystemStatsHolder> dump_holder =
      std::make_unique<SystemStatsHolder>(system_metrics);

  TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("system_stats"),
      "base::TraceEventSystemStatsMonitor::SystemStats",
      static_cast<void*>(this), std::move(dump_holder));
}

void TraceEventSystemStatsMonitor::StopProfiling() {
  // Watch for the tracing framework sending disabling more than once.
  if (is_profiling_) {
    is_profiling_ = false;
    if (auto* sys_monitor = performance_monitor::SystemMonitor::Get())
      sys_monitor->RemoveObserver(this);
  }
}

}  // namespace tracing
