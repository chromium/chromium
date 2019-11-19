// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_DECORATORS_PROCESS_METRICS_DECORATOR_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_DECORATORS_PROCESS_METRICS_DECORATOR_H_

#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "components/performance_manager/public/graph/graph.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"

namespace performance_manager {

// The ProcessMetricsDecorator is responsible for adorning process nodes with
// performance metrics.
class ProcessMetricsDecorator : public GraphOwned {
 public:
  ProcessMetricsDecorator();
  ~ProcessMetricsDecorator() override;

  // GraphOwned:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  void SetGraphForTesting(Graph* graph) { graph_ = graph; }
  bool IsTimerRunningForTesting() const { return refresh_timer_.IsRunning(); }

 protected:
  // Starts/Stop the timer responsible for refreshing the process nodes metrics.
  void StartTimer();
  void StopTimer();

  // Schedule a refresh of the metrics for all the process nodes.
  void RefreshMetrics();

  // Query the MemoryInstrumentation service to get the memory metrics for all
  // processes and run |callback| with the result. Virtual to make a test seam.
  virtual void RequestProcessesMemoryMetrics(
      memory_instrumentation::MemoryInstrumentation::RequestGlobalDumpCallback
          callback);

  // Function that should be used as a callback to
  // MemoryInstrumentation::RequestPrivateMemoryFootprint. |success| will
  // indicate if the data has been retrieved successfully and |process_dumps|
  // will contain the data for all the Chrome processes for which this data was
  // available.
  void DidGetMemoryUsage(
      bool success,
      std::unique_ptr<memory_instrumentation::GlobalMemoryDump> process_dumps);

 private:
  // The timer responsible for refreshing the metrics.
  base::RetainingOneShotTimer refresh_timer_;

  // The Graph instance owning this decorator.
  Graph* graph_;

  base::WeakPtrFactory<ProcessMetricsDecorator> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(ProcessMetricsDecorator);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_DECORATORS_PROCESS_METRICS_DECORATOR_H_
