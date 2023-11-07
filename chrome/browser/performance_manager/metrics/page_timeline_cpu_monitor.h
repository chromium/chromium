// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_METRICS_PAGE_TIMELINE_CPU_MONITOR_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_METRICS_PAGE_TIMELINE_CPU_MONITOR_H_

#include <map>
#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/resource_attribution/cpu_measurement_delegate.h"
#include "components/performance_manager/public/resource_attribution/query_results.h"
#include "components/performance_manager/public/resource_attribution/resource_contexts.h"
#include "components/performance_manager/public/resource_attribution/scoped_cpu_query.h"

namespace performance_manager {

class Graph;
class PageNode;

namespace metrics {

// Periodically collect CPU usage from process nodes, for the UKM logged in
// PageTimelineCPUMonitor.
class PageTimelineCPUMonitor : public ProcessNode::ObserverDefaultImpl {
 public:
  // A shim to request CPU measurements for a process. A new
  // CPUMeasurementDelegate object will be created for each ProcessNode to be
  // measured. Can be overridden for testing by passing a factory to
  // SetCPUMeasurementDelegateFactoryForTesting().
  using CPUMeasurementDelegate = resource_attribution::CPUMeasurementDelegate;

  // A map from FrameNode's or WorkerNode's to the estimated CPU usage of each.
  // The estimate is a fraction in the range 0% to 100% *
  // SysInfo::NumberOfProcessors(), the same as the return value of
  // ProcessMetrics::GetPlatformIndependentCPUUsage().
  //
  // If the kUseResourceAttributionCPUMonitor feature parameter is enabled, the
  // map is keyed by PageNode instead since `cpu_measurement_monitor_` returns
  // page estimates. In production the FrameNode and WorkerNode values are only
  // ever used as inputs to EstimatePageCPUUsage() to get page estimates, so
  // it's more efficient to only store the final page estimates when they're
  // available.
  using CPUUsageMap = std::map<resource_attribution::ResourceContext, double>;

  PageTimelineCPUMonitor();
  ~PageTimelineCPUMonitor() override;

  PageTimelineCPUMonitor(const PageTimelineCPUMonitor& other) = delete;
  PageTimelineCPUMonitor& operator=(const PageTimelineCPUMonitor&) = delete;

  // The given `factory` will be used to create a CPUMeasurementDelegate for
  // each ProcessNode in `graph` to be measured.
  void SetCPUMeasurementDelegateFactoryForTesting(
      Graph* graph,
      CPUMeasurementDelegate::Factory* factory);

  // Starts monitoring CPU usage for all renderer ProcessNode's in `graph`.
  void StartMonitoring(Graph* graph);

  // Stops monitoring ProcessNode's in `graph`.
  void StopMonitoring(Graph* graph);

  // Updates the CPU measurements for each ProcessNode being tracked and invokes
  // `callback` with the estimated CPU usage of each frame and worker in those
  // processes since the last time UpdateCPUMeasurements() was called .
  void UpdateCPUMeasurements(
      base::OnceCallback<void(const CPUUsageMap&)> callback);

  // Helper to estimate the CPU usage of a PageNode given the estimates for all
  // frames and workers. If the kUseResourceAttributionCPUMonitor feature
  // parameter is enabled, this simply looks up `page_node` in `cpu_usage_map`
  // which already includes page estimates.
  static double EstimatePageCPUUsage(const PageNode* page_node,
                                     const CPUUsageMap& cpu_usage_map);

  // ProcessNode::Observer:
  void OnProcessLifetimeChange(const ProcessNode* process_node) override;
  void OnBeforeProcessNodeRemoved(const ProcessNode* process_node) override;

 private:
  friend class PageTimelineCPUMonitorTest;

  // Holds a CPUMeasurementDelegate object to measure CPU usage and metadata
  // about the measurements. One CPUMeasurement will be created for each
  // ProcessNode being measured.
  class CPUMeasurement {
   public:
    explicit CPUMeasurement(std::unique_ptr<CPUMeasurementDelegate> delegate);
    ~CPUMeasurement();

    // Move-only type.
    CPUMeasurement(const CPUMeasurement& other) = delete;
    CPUMeasurement& operator=(const CPUMeasurement& other) = delete;
    CPUMeasurement(CPUMeasurement&& other);
    CPUMeasurement& operator=(CPUMeasurement&& other);

    // Returns the most recent measurement that was taken during
    // MeasureAndDistributeCPUUsage().
    base::TimeDelta most_recent_measurement() const {
      return most_recent_measurement_;
    }

    // Measures the CPU usage of `process_node`, calculates the proportion of
    // usage over the period `measurement_interval_end` -
    // `measurement_interval_start`, and allocates the results to frames and
    // workers in the process.
    void MeasureAndDistributeCPUUsage(
        const ProcessNode* process_node,
        base::TimeTicks measurement_interval_start,
        base::TimeTicks measurement_interval_end,
        CPUUsageMap& cpu_usage_map);

   private:
    std::unique_ptr<CPUMeasurementDelegate> delegate_;
    base::TimeDelta most_recent_measurement_;
  };

  // Creates a CPUMeasurement tracker for `process_node` and adds it to
  // `cpu_measurement_map_`.
  void MonitorCPUUsage(const ProcessNode* process_node);

  // Uses results from `cpu_measurement_monitor_` to update CPU measurements.
  // Called from UpdateCPUMeasurements() if the
  // kUseResourceAttributionCPUMonitor feature parameter is enabled.
  void UpdateResourceAttributionCPUMeasurements(
      base::OnceCallback<void(const CPUUsageMap&)> callback,
      base::TimeDelta measurement_interval,
      const resource_attribution::QueryResultMap& results);

  SEQUENCE_CHECKER(sequence_checker_);

  // Map of process nodes to ProcessMetrics used to measure CPU usage.
  std::map<const ProcessNode*, CPUMeasurement> cpu_measurement_map_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Last time CPU measurements were taken (for calculating the total length of
  // a measurement interval).
  base::TimeTicks last_measurement_time_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Factory that creates CPUMeasurementDelegate objects for each ProcessNode
  // being measured.
  raw_ptr<CPUMeasurementDelegate::Factory> cpu_measurement_delegate_factory_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // If the kUseResourceAttributionCPUMonitor feature parameter is enabled,
  // PageTimelineCPUMonitor will get CPU measurements from this, otherwise it
  // will perform its own measurements.
  std::unique_ptr<resource_attribution::ScopedCPUQuery> cpu_query_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // If the kUseResourceAttributionCPUMonitor feature parameter is enabled, this
  // will cache the measurements of each page when UpdateCPUMeasurements is
  // called. Otherwise it's unused.
  resource_attribution::QueryResultMap cached_cpu_measurements_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<PageTimelineCPUMonitor> weak_factory_{this};
};

}  // namespace metrics
}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_METRICS_PAGE_TIMELINE_CPU_MONITOR_H_
