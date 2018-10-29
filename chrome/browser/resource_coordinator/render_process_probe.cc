// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/render_process_probe.h"

#include <vector>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/service_manager_connection.h"
#include "services/resource_coordinator/public/cpp/process_resource_coordinator.h"
#include "services/resource_coordinator/public/mojom/coordination_unit.mojom.h"

#if defined(OS_MACOSX)
#include "content/public/browser/browser_child_process_host.h"
#endif

namespace resource_coordinator {

constexpr base::TimeDelta RenderProcessProbeImpl::kUninitializedCPUTime;

// static
RenderProcessProbe* RenderProcessProbe::GetInstance() {
  static base::NoDestructor<RenderProcessProbeImpl> probe;
  return probe.get();
}

// static
bool RenderProcessProbe::IsEnabled() {
  // Check that service_manager is active and GRC is enabled.
  return content::ServiceManagerConnection::GetForProcess() != nullptr;
}

RenderProcessProbeImpl::RenderProcessInfo::RenderProcessInfo() = default;

RenderProcessProbeImpl::RenderProcessInfo::~RenderProcessInfo() = default;

RenderProcessProbeImpl::RenderProcessProbeImpl() {}

RenderProcessProbeImpl::~RenderProcessProbeImpl() = default;

void RenderProcessProbeImpl::StartSingleGather() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (is_gathering_)
    return;

  RegisterAliveRenderProcessesOnUIThread();
}

void RenderProcessProbeImpl::RegisterAliveRenderProcessesOnUIThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!is_gathering_);

  ++current_gather_cycle_;

  RegisterRenderProcesses();

  is_gathering_ = true;

  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(
          &RenderProcessProbeImpl::
              CollectRenderProcessMetricsAndStartMemoryDumpOnIOThread,
          base::Unretained(this)));
}

void RenderProcessProbeImpl::
    CollectRenderProcessMetricsAndStartMemoryDumpOnIOThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(is_gathering_);

  base::TimeTicks collection_start_time = base::TimeTicks::Now();

  StartMemoryMeasurement(collection_start_time);

  auto iter = render_process_info_map_.begin();
  while (iter != render_process_info_map_.end()) {
    auto& render_process_info = iter->second;

    // If the last gather cycle the render process was marked as active is
    // not current then it is assumed dead and should not be measured anymore.
    if (render_process_info.last_gather_cycle_active == current_gather_cycle_) {
      render_process_info.cpu_usage =
          render_process_info.metrics->GetCumulativeCPUUsage();
      ++iter;
    } else {
      render_process_info_map_.erase(iter++);
      continue;
    }
  }
}

void RenderProcessProbeImpl::ProcessGlobalMemoryDumpAndDispatchOnIOThread(
    base::TimeTicks collection_start_time,
    bool global_success,
    std::unique_ptr<memory_instrumentation::GlobalMemoryDump> dump) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(is_gathering_);
  // Create the measurement batch.
  mojom::ProcessResourceMeasurementBatchPtr batch =
      mojom::ProcessResourceMeasurementBatch::New();

  // Start by adding the render process hosts we know about to the batch.
  for (const auto& render_process_info_map_entry : render_process_info_map_) {
    auto& render_process_info = render_process_info_map_entry.second;
    // TODO(oysteine): Move the multiplier used to avoid precision loss
    // into a shared location, when this property gets used.
    mojom::ProcessResourceMeasurementPtr measurement =
        mojom::ProcessResourceMeasurement::New();

    measurement->pid =
        GetProcessId(render_process_info_map_entry.first, render_process_info);
    measurement->cpu_usage = render_process_info.cpu_usage;

    batch->measurements.push_back(std::move(measurement));
  }

  // Record the overall outcome of the measurement.
  MeasurementOutcome outcome = MeasurementOutcome::kMeasurementSuccess;
  if (!global_success)
    outcome = MeasurementOutcome::kMeasurementPartialSuccess;
  if (!dump)
    outcome = MeasurementOutcome::kMeasurementFailure;

  UMA_HISTOGRAM_ENUMERATION("ResourceCoordinator.Measurement.Memory.Outcome",
                            outcome);

  if (dump) {
    // Then amend the ones we have memory metrics for with their private
    // footprint. The global dump may contain non-renderer processes, it may
    // contain renderer processes we didn't capture at the start of the cycle,
    // and it may not contain all the renderer processes we know about.
    // This may happen due to the inherent race between the request and
    // starting/stopping renderers, or because of other failures
    // This may therefore provide incomplete information.
    size_t num_non_measured_processes = batch->measurements.size();
    size_t num_unexpected_processes = 0;
    for (const auto& dump_entry : dump->process_dumps()) {
      base::ProcessId pid = dump_entry.pid();

      bool used_entry = false;
      for (const auto& measurement : batch->measurements) {
        if (measurement->pid != pid)
          continue;

        used_entry = true;

        // The only way this could fail is if there are multiple measurements
        // for the same PID in the memory dump.
        DCHECK_LT(0u, num_non_measured_processes);
        --num_non_measured_processes;

        measurement->private_footprint_kb =
            dump_entry.os_dump().private_footprint_kb;
        break;
      }

      if (!used_entry)
        ++num_unexpected_processes;
    }

    // Record the number of processes we unexpectedly did or didn't get memory
    // measurements for.
    UMA_HISTOGRAM_COUNTS_1000(
        "ResourceCoordinator.Measurement.Memory.UnmeasuredProcesses",
        num_non_measured_processes);
    // TODO(siggi): will this count extension/utility/background worker
    //     processes?
    UMA_HISTOGRAM_COUNTS_1000(
        "ResourceCoordinator.Measurement.Memory.ExtraProcesses",
        num_unexpected_processes);
  } else {
    // We should only get a nullptr in case of failure.
    DCHECK(!global_success);
  }

  // Record the number of processes encountered.
  UMA_HISTOGRAM_COUNTS_1000("ResourceCoordinator.Measurement.TotalProcesses",
                            batch->measurements.size());

  batch->batch_started_time = collection_start_time;
  batch->batch_ended_time = base::TimeTicks::Now();

  // Record the duration of the measurement process.
  UMA_HISTOGRAM_TIMES("ResourceCoordinator.Measurement.Duration",
                      batch->batch_ended_time - batch->batch_started_time);

  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&RenderProcessProbeImpl::FinishCollectionOnUIThread,
                     base::Unretained(this), std::move(batch)));
}

void RenderProcessProbeImpl::FinishCollectionOnUIThread(
    mojom::ProcessResourceMeasurementBatchPtr batch) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(is_gathering_);
  is_gathering_ = false;

  DispatchMetricsOnUIThread(std::move(batch));
}

void RenderProcessProbeImpl::RegisterRenderProcesses() {
  for (content::RenderProcessHost::iterator rph_iter =
           content::RenderProcessHost::AllHostsIterator();
       !rph_iter.IsAtEnd(); rph_iter.Advance()) {
    content::RenderProcessHost* host = rph_iter.GetCurrentValue();
    // Process may not be valid yet.
    if (!host->GetProcess().IsValid()) {
      continue;
    }

    auto& render_process_info = render_process_info_map_[host->GetID()];
    render_process_info.last_gather_cycle_active = current_gather_cycle_;
    if (render_process_info.metrics.get() == nullptr) {
      DCHECK(!render_process_info.process.IsValid());

      // Duplicate the process to retain ownership of it through the thread
      // bouncing.
      render_process_info.process = host->GetProcess().Duplicate();
    }

#if defined(OS_MACOSX)
    render_process_info.metrics = base::ProcessMetrics::CreateProcessMetrics(
        render_process_info.process.Handle(),
        content::BrowserChildProcessHost::GetPortProvider());
#else
    render_process_info.metrics = base::ProcessMetrics::CreateProcessMetrics(
        render_process_info.process.Handle());
#endif
  }
}

void RenderProcessProbeImpl::StartMemoryMeasurement(
    base::TimeTicks collection_start_time) {
  // Dispatch the memory collection request.
  memory_instrumentation::MemoryInstrumentation::GetInstance()
      ->RequestPrivateMemoryFootprint(
          base::kNullProcessId,
          base::BindRepeating(&RenderProcessProbeImpl::
                                  ProcessGlobalMemoryDumpAndDispatchOnIOThread,
                              base::Unretained(this), collection_start_time));
}

base::ProcessId RenderProcessProbeImpl::GetProcessId(
    int /*host_id*/,
    const RenderProcessInfo& info) {
  return info.process.Pid();
}

SystemResourceCoordinator*
RenderProcessProbeImpl::EnsureSystemResourceCoordinator() {
  if (!system_resource_coordinator_) {
    content::ServiceManagerConnection* connection =
        content::ServiceManagerConnection::GetForProcess();
    if (connection)
      system_resource_coordinator_ =
          std::make_unique<SystemResourceCoordinator>(
              connection->GetConnector());
  }

  return system_resource_coordinator_.get();
}

void RenderProcessProbeImpl::DispatchMetricsOnUIThread(
    mojom::ProcessResourceMeasurementBatchPtr batch) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  SystemResourceCoordinator* system_resource_coordinator =
      EnsureSystemResourceCoordinator();

  if (system_resource_coordinator && !batch->measurements.empty())
    system_resource_coordinator->DistributeMeasurementBatch(std::move(batch));
}

}  // namespace resource_coordinator
