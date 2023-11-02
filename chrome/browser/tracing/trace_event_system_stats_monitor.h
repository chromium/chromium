// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRACING_TRACE_EVENT_SYSTEM_STATS_MONITOR_H_
#define CHROME_BROWSER_TRACING_TRACE_EVENT_SYSTEM_STATS_MONITOR_H_

#include "base/memory/weak_ptr.h"
#include "base/process/process_metrics.h"
#include "base/trace_event/trace_log.h"
#include "chrome/browser/performance_monitor/system_monitor.h"

namespace tracing {

// Watches for chrome://tracing to be enabled or disabled. When tracing is
// enabled, also enables system events profiling. This class is the preferred
// way to turn system tracing on and off.
class TraceEventSystemStatsMonitor
    : public base::trace_event::TraceLog::AsyncEnabledStateObserver,
      public performance_monitor::SystemMonitor::SystemObserver {
 public:
  TraceEventSystemStatsMonitor();

  TraceEventSystemStatsMonitor(const TraceEventSystemStatsMonitor&) = delete;
  TraceEventSystemStatsMonitor& operator=(const TraceEventSystemStatsMonitor&) =
      delete;

  ~TraceEventSystemStatsMonitor() override;

  // base::trace_event::TraceLog::EnabledStateChangedObserver overrides:
  void OnTraceLogEnabled() override;
  void OnTraceLogDisabled() override;

  bool is_profiling_for_testing() const { return is_profiling_; }

  void StartProfilingForTesting() { StartProfiling(); }
  void StopProfilingForTesting() { StopProfiling(); }

 private:
  void StartProfiling();

  void StopProfiling();

  // performance_monitor::SystemMonitor::SystemObserver:
  void OnSystemMetricsStruct(
      const base::SystemMetrics& system_metrics) override;

  // Indicates if profiling has started.
  bool is_profiling_ = false;

  base::WeakPtrFactory<TraceEventSystemStatsMonitor> weak_factory_{this};
};

}  // namespace tracing

#endif  // CHROME_BROWSER_TRACING_TRACE_EVENT_SYSTEM_STATS_MONITOR_H_
