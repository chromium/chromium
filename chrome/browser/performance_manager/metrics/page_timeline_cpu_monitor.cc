// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/metrics/page_timeline_cpu_monitor.h"

#include <map>
#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/graph_operations.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/graph/worker_node.h"
#include "components/performance_manager/public/resource_attribution/attribution_helpers.h"
#include "components/performance_manager/public/resource_attribution/query_results.h"
#include "components/performance_manager/public/resource_attribution/resource_contexts.h"
#include "content/public/common/process_type.h"

namespace performance_manager::metrics {

using resource_attribution::PageContext;
using resource_attribution::ResourceContext;

PageTimelineCPUMonitor::PageTimelineCPUMonitor()
    : cpu_measurement_delegate_factory_(
          CPUMeasurementDelegate::GetDefaultFactory()) {}

PageTimelineCPUMonitor::~PageTimelineCPUMonitor() = default;

void PageTimelineCPUMonitor::SetCPUMeasurementDelegateFactoryForTesting(
    Graph* graph,
    CPUMeasurementDelegate::Factory* factory) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (features::kUseResourceAttributionCPUMonitor.Get()) {
    CPUMeasurementDelegate::SetDelegateFactoryForTesting(  // IN_TEST
        graph, factory);
    return;
  }
  // Ensure that all CPU measurements use the same delegate.
  CHECK(cpu_measurement_map_.empty());
  CHECK(factory);
  cpu_measurement_delegate_factory_ = factory;
}

void PageTimelineCPUMonitor::StartMonitoring(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(last_measurement_time_.is_null());
  last_measurement_time_ = base::TimeTicks::Now();

  if (features::kUseResourceAttributionCPUMonitor.Get()) {
    CHECK(cached_cpu_measurements_.empty());
    cpu_query_ = std::make_unique<resource_attribution::ScopedCPUQuery>();
    return;
  }

  graph->AddProcessNodeObserver(this);

  // Start monitoring CPU usage for all existing processes. Can't read their
  // CPU usage until they have a pid assigned.
  for (const ProcessNode* process_node : graph->GetAllProcessNodes()) {
    if (cpu_measurement_delegate_factory_->ShouldMeasureProcess(process_node)) {
      MonitorCPUUsage(process_node);
    }
  }
}

void PageTimelineCPUMonitor::StopMonitoring(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!last_measurement_time_.is_null());
  last_measurement_time_ = base::TimeTicks();

  if (features::kUseResourceAttributionCPUMonitor.Get()) {
    cpu_query_.reset();
    cached_cpu_measurements_.clear();
  } else {
    cpu_measurement_map_.clear();
    graph->RemoveProcessNodeObserver(this);
  }
}

void PageTimelineCPUMonitor::UpdateCPUMeasurements(
    base::OnceCallback<void(const CPUUsageMap&)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Update CPU metrics, attributing the cumulative CPU of each process to its
  // frames and workers.
  CHECK(!last_measurement_time_.is_null());
  const base::TimeTicks now = base::TimeTicks::Now();
  if (features::kUseResourceAttributionCPUMonitor.Get()) {
    cpu_query_->QueryOnce(base::BindOnce(
        &PageTimelineCPUMonitor::UpdateResourceAttributionCPUMeasurements,
        weak_factory_.GetWeakPtr(), std::move(callback),
        now - last_measurement_time_));
  } else {
    CPUUsageMap cpu_usage_map;
    for (auto& [process_node, cpu_measurement] : cpu_measurement_map_) {
      cpu_measurement.MeasureAndDistributeCPUUsage(
          process_node, last_measurement_time_, now, cpu_usage_map);
    }
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), std::move(cpu_usage_map)));
  }
  last_measurement_time_ = now;
}

// static
double PageTimelineCPUMonitor::EstimatePageCPUUsage(
    const PageNode* page_node,
    const CPUUsageMap& cpu_usage_map) {
  if (features::kUseResourceAttributionCPUMonitor.Get()) {
    // Page estimates are stored directly in `cpu_usage_map`.
    const auto it = cpu_usage_map.find(page_node->GetResourceContext());
    return it == cpu_usage_map.end() ? 0.0 : it->second;
  }

  double page_cpu_usage = 0.0;
  auto accumulate_cpu_usage = [&page_cpu_usage,
                               &cpu_usage_map](const ResourceContext& context) {
    const auto it = cpu_usage_map.find(context);
    // A context might be missing from the map if there was an error measuring
    // the CPU usage of its process.
    if (it != cpu_usage_map.end()) {
      page_cpu_usage += it->second;
    }
  };
  GraphOperations::VisitFrameTreePreOrder(page_node, [&accumulate_cpu_usage](
                                                         const FrameNode* f) {
    accumulate_cpu_usage(f->GetResourceContext());
    // TODO(crbug.com/1410503): Handle non-dedicated workers, which could appear
    // as children of multiple frames.
    f->VisitChildDedicatedWorkers([&accumulate_cpu_usage](const WorkerNode* w) {
      accumulate_cpu_usage(w->GetResourceContext());
      return true;
    });
    return true;
  });
  return page_cpu_usage;
}

void PageTimelineCPUMonitor::OnProcessLifetimeChange(
    const ProcessNode* process_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!features::kUseResourceAttributionCPUMonitor.Get());
  if (last_measurement_time_.is_null()) {
    // Not monitoring CPU usage yet.
    CHECK(cpu_measurement_map_.empty());
    return;
  }
  if (cpu_measurement_delegate_factory_->ShouldMeasureProcess(process_node)) {
    auto it = cpu_measurement_map_.find(process_node);
    if (it == cpu_measurement_map_.end()) {
      // Process isn't being measured yet so it must have been created while
      // measurements were already started.
      MonitorCPUUsage(process_node);
    }
  }
}

void PageTimelineCPUMonitor::OnBeforeProcessNodeRemoved(
    const ProcessNode* process_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!features::kUseResourceAttributionCPUMonitor.Get());
  cpu_measurement_map_.erase(process_node);
}

void PageTimelineCPUMonitor::MonitorCPUUsage(const ProcessNode* process_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!features::kUseResourceAttributionCPUMonitor.Get());
  // Only measure renderers.
  CHECK_EQ(process_node->GetProcessType(), content::PROCESS_TYPE_RENDERER);
  const auto& [it, was_inserted] = cpu_measurement_map_.emplace(
      process_node,
      CPUMeasurement(
          cpu_measurement_delegate_factory_->CreateDelegateForProcess(
              process_node)));
  CHECK(was_inserted);
}

void PageTimelineCPUMonitor::UpdateResourceAttributionCPUMeasurements(
    base::OnceCallback<void(const CPUUsageMap&)> callback,
    base::TimeDelta measurement_interval,
    const resource_attribution::QueryResultMap& results) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(features::kUseResourceAttributionCPUMonitor.Get());
  if (measurement_interval.is_zero()) {
    // No time passed to measure. Ignore the results to avoid division by zero.
    std::move(callback).Run({});
    return;
  }
  CHECK(measurement_interval.is_positive());

  // Swap a new measurement into `cached_cpu_measurements_`, storing the
  // previous contents in `previous_measurements`.
  resource_attribution::QueryResultMap previous_measurements =
      std::exchange(cached_cpu_measurements_, results);

  CPUUsageMap cpu_usage_map;
  for (const auto& [context, query_result] : cached_cpu_measurements_) {
    using CPUTimeResult = resource_attribution::CPUTimeResult;
    if (!resource_attribution::ContextIs<PageContext>(context)) {
      continue;
    }
    const auto& result =
        resource_attribution::AsResult<CPUTimeResult>(query_result).value();

    // Let time A be the last time UpdateCPUMeasurements() was called (with the
    // results saved in `previous_measurements`), or the time when
    // StartMonitoring() was called if this is the first one
    // (`previous_measurements` will be empty).
    //
    // Let time B be current time. (`measurement_interval` is A..B.)
    //
    // There are 4 cases:
    //
    // 1. The context was created at time C, between A and B. (It will not be
    // found in `previous_measurements`).
    //
    // This snapshot should include 0% CPU for time A..C, and the measured % of
    // CPU for time C..B.
    //
    // A    C         B
    // |----+---------|
    // | 0% |   X%    |
    //
    // CPU(C..B) is `result.cumulative_cpu`.
    // `result.start_time` is C.
    // `result.metadata.measurement_time` is B.
    //
    // 2. The context existed for the entire duration A..B.
    //
    // This snapshot should include the measured % of CPU for the whole time
    // A..B.
    //
    // A              B
    // |--------------|
    // |      X%      |
    //
    // CPU(A..B) is `result.cumulative_cpu -
    // previous_measurements[context].cumulative_cpu`.
    // `result.start_time` <= A.
    // `result.metadata.measurement_time` is B.
    //
    // 3. Context created before time A, exited at time D, between A and B.
    //
    // The snapshot should include the measured % of CPU for time A..D, and 0%
    // CPU for time D..B.
    //
    // A         D    B
    // |---------+----|
    // |    X%   | 0% |
    //
    // CPU(A..D) is `result.cumulative_cpu -
    // previous_measurements[context].cumulative_cpu`.
    // `result.start_time` <= A.
    // `result.metadata.measurement_time` is D.
    //
    // 4. Context created at time C and exited at time D, both between A and B.
    // (context is not found in `previous_measurements`.
    // `result.cumulative_cpu` ends at time D, which is
    // `result.metadata.measurement_time`.)
    //
    // The snapshot should include the measured % of CPU for time C..D, and 0%
    // CPU for the rest.
    //
    // A    C    D    B
    // |----+----+----|
    // | 0% | X% | 0% |
    //
    // CPU(C..D) is `result.cumulative_cpu`.
    // `result.start_time` is C.
    // `result.metadata.measurement_time` is D.
    base::TimeDelta current_cpu = result.cumulative_cpu;
    const auto it = previous_measurements.find(context);
    if (it != previous_measurements.end()) {
      const auto& previous_result =
          resource_attribution::AsResult<CPUTimeResult>(it->second).value();
      current_cpu -= previous_result.cumulative_cpu;
    }
    CHECK(!current_cpu.is_negative());
    cpu_usage_map.emplace(context, current_cpu / measurement_interval);
  }
  std::move(callback).Run(std::move(cpu_usage_map));
}

PageTimelineCPUMonitor::CPUMeasurement::CPUMeasurement(
    std::unique_ptr<CPUMeasurementDelegate> delegate)
    : delegate_(std::move(delegate)),
      // Record the CPU usage immediately on starting to measure a process, so
      // that the first call to MeasureAndDistributeCPUUsage() will cover the
      // time between the measurement starting and the snapshot.
      most_recent_measurement_(delegate_->GetCumulativeCPUUsage()) {}

PageTimelineCPUMonitor::CPUMeasurement::~CPUMeasurement() = default;

PageTimelineCPUMonitor::CPUMeasurement::CPUMeasurement(
    PageTimelineCPUMonitor::CPUMeasurement&& other) = default;

PageTimelineCPUMonitor::CPUMeasurement&
PageTimelineCPUMonitor::CPUMeasurement::operator=(
    PageTimelineCPUMonitor::CPUMeasurement&& other) = default;

void PageTimelineCPUMonitor::CPUMeasurement::MeasureAndDistributeCPUUsage(
    const ProcessNode* process_node,
    base::TimeTicks measurement_interval_start,
    base::TimeTicks measurement_interval_end,
    CPUUsageMap& cpu_usage_map) {
  // TODO(crbug.com/1410503): There isn't a good way to get the process CPU
  // usage after it exits here:
  //
  // 1. Attempts to measure it with GetCumulativeCPUUsage() will fail because
  //    the process info is already reaped.
  // 2. For these cases the ChildProcessTerminationInfo struct contains a final
  //    `cpu_usage` member. This needs to be collected by a
  //    RenderProcessHostObserver (either PM's RenderProcessUserData or a
  //    dedicated observer). But:
  // 3. MeasureAndDistributeCPUUsage() distributes the process measurements
  // among FrameNodes and
  //    by the time the final `cpu_usage` is available, the FrameNodes for the
  //    process are often gone already. The reason is that FrameNodes are
  //    removed on process exit by another RenderProcessHostObserver, and the
  //    observers can fire in any order.
  //
  // For the record, the call stack that removes a FrameNode is:
  //
  // performance_manager::PerformanceManagerImpl::DeleteNode()
  // performance_manager::PerformanceManagerTabHelper::RenderFrameDeleted()
  // content::WebContentsImpl::WebContentsObserverList::NotifyObservers<>()
  // content::WebContentsImpl::RenderFrameDeleted()
  // content::RenderFrameHostImpl::RenderFrameDeleted()
  // content::RenderFrameHostImpl::RenderProcessGone()
  // content::SiteInstanceGroup::RenderProcessExited() <-- observer
  //
  // So it's not possible to attribute the final CPU usage of a process to its
  // frames without a refactor of PerformanceManager to keep the FrameNodes
  // alive slightly longer.
  //
  // A better and more complete way to handle this would be to update the CPU
  // usage of a PageNode every time a frame or worker is created or deleted.
  // This would keep the estimate up to date with the page topology, which is
  // important to avoid under-estimating the CPU usage of pages that create a
  // lot of short-lived iframes.

  CHECK(!measurement_interval_start.is_null());
  const base::TimeDelta measurement_interval =
      measurement_interval_end - measurement_interval_start;
  if (measurement_interval.is_zero()) {
    // No time has passed to measure.
    return;
  }
  CHECK(measurement_interval.is_positive());

  // Assume a measurement period running from time A
  // (`measurement_interval_start`) to time B (`measurement_interval_end`).
  //
  // Let CPU(T) be the cpu measurement at time T.
  //
  // Note that the process is only measured after it's passed to the graph,
  // which is shortly after it's created, so at "process creation time" C,
  // CPU(C) may have a small value instead of 0. On the first call to
  // MeasureAndDistributeCPUUsage(), `most_recent_measurement_` will be CPU(C).
  //
  // There are 4 cases:
  //
  // 1. The process is created at time C, between A and B.
  //
  // This snapshot should include 0% CPU for time A..C, and the measured % of
  // CPU for time C..B.
  //
  // A    C         B
  // |----+---------|
  // | 0% |   X%    |
  //
  // The overall CPU usage at this snapshot is (CPU(B) - CPU(C)) / (B-A)
  // CPU(B) = GetCumulativeCPUUsage()
  // CPU(C) = `most_recent_measurement_`
  //
  // 2. The process existed for the entire duration A..B.
  //
  // This snapshot should include the measured % of CPU for the whole time
  // A..B.
  //
  // A              B
  // |--------------|
  // |      X%      |
  //
  // The overall CPU usage at this snapshot is (CPU(B) - CPU(A)) / (B-A)
  // CPU(B) = GetCumulativeCPUUsage()
  // CPU(A) = `most_recent_measurement_`
  //
  // 3. Process created before time A, but exited at time D, between A and B.
  //
  // The snapshot should include the measured % of CPU for time A..D, and 0%
  // CPU for time D..B.
  //
  // A         D    B
  // |---------+----|
  // |    X%   | 0% |
  //
  // The overall CPU usage at this snapshot is (CPU(D) - CPU(A)) / (B-A)
  // CPU(D) = ChildProcessTerminationInfo::cpu_usage (currently unavailable)
  // CPU(A) = `most_recent_measurement_`
  //
  // 4. Process created at time C and exited at time D, both between A and B.
  //
  // The snapshot should include the measured % of CPU for time C..D, and 0%
  // CPU for the rest.
  //
  // A    C    D    B
  // |----+----+----|
  // | 0% | X% | 0% |
  //
  // The overall CPU usage at this snapshot is (CPU(D) - CPU(C) / (B-A)
  // CPU(D) = ChildProcessTerminationInfo::cpu_usage (currently unavailable)
  // CPU(C) = `most_recent_measurement_`
  //
  // In case 1 and case 2, the numerator is `GetCumulativeCPUUsage() -
  // most_recent_measurement_`. In case 3 and 4, GetCumulativeCPUUsage() will
  // return an error code.
  base::TimeDelta current_cpu_usage = delegate_->GetCumulativeCPUUsage();
  if (!current_cpu_usage.is_positive()) {
    // GetCumulativeCPUUsage() failed. Don't update the measurement state.
    // Most platforms return a zero TimeDelta on error, Linux returns a
    // negative.
    return;
  }
  const base::TimeDelta current_measurement =
      current_cpu_usage - most_recent_measurement_;
  most_recent_measurement_ = current_cpu_usage;

  const double current_cpu_proportion =
      current_measurement / measurement_interval;
  resource_attribution::SplitResourceAmongFramesAndWorkers(
      current_cpu_proportion, process_node,
      [&cpu_usage_map](const FrameNode* f, double cpu) {
        cpu_usage_map.emplace(f->GetResourceContext(), cpu);
      },
      [&cpu_usage_map](const WorkerNode* w, double cpu) {
        cpu_usage_map.emplace(w->GetResourceContext(), cpu);
      });
}

}  // namespace performance_manager::metrics
