// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_PRESSURE_PRESSURE_METRICS_REPORTER_H_
#define CHROME_BROWSER_METRICS_PRESSURE_PRESSURE_METRICS_REPORTER_H_

#include "chrome/browser/metrics/pressure/pressure_metrics.h"

#include "base/sequence_checker.h"
#include "base/threading/sequence_bound.h"
#include "base/timer/timer.h"
#include "base/trace_event/trace_log.h"

// Contains the code working on the worker sequence.
class PressureMetricsWorker;

// MemoryMetrics periodically collects and reports pressure metrics (Pressure
// Stall Information). This class is responsible for retrieving memory pressure
// stall information.
//
// Pressure metrics are sampled and reported through UMA
// (see: System.Pressure.*).
//
// Metrics are also reported through tracing as counters.
class PressureMetricsReporter
    : public base::trace_event::TraceLog::EnabledStateObserver {
 public:
  PressureMetricsReporter();
  ~PressureMetricsReporter() override;

  // trace_event::TraceLog::EnabledStateObserver implementation:
  void OnTraceLogEnabled() override;
  void OnTraceLogDisabled() override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);
  base::SequenceBound<PressureMetricsWorker> worker_;
};

#endif  // CHROME_BROWSER_METRICS_PRESSURE_PRESSURE_METRICS_REPORTER_H_
