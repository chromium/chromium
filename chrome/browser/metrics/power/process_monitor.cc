// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/power/process_monitor.h"

#include <stddef.h>

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/not_fatal_until.h"
#include "base/observer_list.h"
#include "base/process/process_handle.h"
#include "base/process/process_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/optional_util.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/chrome_content_browser_client_extensions_part.h"
#include "chrome/browser/metrics/power/power_metrics_constants.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_constants.h"
#include "extensions/buildflags/buildflags.h"
#include "services/network/public/mojom/network_service.mojom.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_map.h"
#include "extensions/common/manifest_handlers/background_info.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "sandbox/policy/mojom/sandbox.mojom-shared.h"
#endif

using content::BrowserThread;

namespace {

std::unique_ptr<base::ProcessMetrics> CreateProcessMetrics(
    base::ProcessHandle process_handle) {
#if BUILDFLAG(IS_MAC)
  return base::ProcessMetrics::CreateProcessMetrics(
      process_handle, content::BrowserChildProcessHost::GetPortProvider());
#else
  return base::ProcessMetrics::CreateProcessMetrics(process_handle);
#endif
}

// Samples the process metrics the ProcessMonitor cares about.
ProcessMonitor::Metrics SampleMetrics(base::ProcessMetrics& process_metrics) {
  ProcessMonitor::Metrics metrics;

  metrics.cpu_usage = base::OptionalFromExpected(
      process_metrics.GetPlatformIndependentCPUUsage());

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_AIX)
  metrics.idle_wakeups = process_metrics.GetIdleWakeupsPerSecond();
#endif
#if BUILDFLAG(IS_MAC)
  metrics.package_idle_wakeups =
      process_metrics.GetPackageIdleWakeupsPerSecond();
#endif

  return metrics;
}

// Scales every metrics by |factor|.
void ScaleMetrics(ProcessMonitor::Metrics* metrics, double factor) {
  if (metrics->cpu_usage.has_value()) {
    metrics->cpu_usage.value() *= factor;
  }

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_AIX)
  metrics->idle_wakeups *= factor;
#endif

#if BUILDFLAG(IS_MAC)
  metrics->package_idle_wakeups *= factor;
#endif
}

ProcessMonitor::Metrics GetLastIntervalMetrics(
    base::ProcessMetrics& process_metrics,
    base::TimeDelta cumulative_cpu_usage) {
  ProcessMonitor::Metrics metrics;
  metrics.cpu_usage =
      process_metrics.GetPlatformIndependentCPUUsage(cumulative_cpu_usage);
  // TODO: Add other values in ProcessMonitor::Metrics.
  return metrics;
}

MonitoredProcessType GetMonitoredProcessTypeForRenderProcess(
    content::RenderProcessHost* host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  content::BrowserContext* browser_context = host->GetBrowserContext();
  if (extensions::ChromeContentBrowserClientExtensionsPart::
          AreExtensionsDisabledForProfile(browser_context)) {
    return MonitoredProcessType::kRenderer;
  }

  const extensions::Extension* extension =
      extensions::ProcessMap::Get(browser_context)
          ->GetEnabledExtensionByProcessID(host->GetID());
  if (!extension) {
    return kRenderer;
  }

  return extensions::BackgroundInfo::HasPersistentBackgroundPage(extension)
             ? MonitoredProcessType::kExtensionPersistent
             : MonitoredProcessType::kExtensionEvent;
#else
  return MonitoredProcessType::kRenderer;
#endif
}

MonitoredProcessType GetMonitoredProcessTypeForNonRendererChildProcess(
    const content::ChildProcessData& data) {
  switch (data.process_type) {
    case content::PROCESS_TYPE_BROWSER:
    case content::PROCESS_TYPE_RENDERER:
      // Not a non-renderer child process.
      NOTREACHED_IN_MIGRATION();
      return kCount;
    case content::PROCESS_TYPE_GPU:
      return MonitoredProcessType::kGpu;
    case content::PROCESS_TYPE_UTILITY: {
      // Special case for the network process.
      if (data.metrics_name == network::mojom::NetworkService::Name_)
        return MonitoredProcessType::kNetwork;
      return MonitoredProcessType::kUtility;
    }
    default:
      return MonitoredProcessType::kOther;
  }
}

// Adds the values from |rhs| to |lhs|. If both parameters have nullopt for
// `cpu_usage`, the result will also have nullopt, otherwise the result will
// have the sum of all non-nullopt `cpu_usage`.
ProcessMonitor::Metrics& operator+=(ProcessMonitor::Metrics& lhs,
                                    const ProcessMonitor::Metrics& rhs) {
  if (lhs.cpu_usage.has_value() || rhs.cpu_usage.has_value()) {
    lhs.cpu_usage = lhs.cpu_usage.value_or(0.0) + rhs.cpu_usage.value_or(0.0);
  }

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_AIX)
  lhs.idle_wakeups += rhs.idle_wakeups;
#endif

#if BUILDFLAG(IS_MAC)
  lhs.package_idle_wakeups += rhs.package_idle_wakeups;
#endif

  return lhs;
}

}  // namespace

MonitoredProcessType
GetMonitoredProcessTypeForNonRendererChildProcessForTesting(
    const content::ChildProcessData& data) {
  return GetMonitoredProcessTypeForNonRendererChildProcess(data);
}

ProcessInfo::ProcessInfo(MonitoredProcessType type,
                         std::unique_ptr<base::ProcessMetrics> process_metrics)
    : type(type),
      process_metrics(std::move(process_metrics)),
      first_sample_time(base::TimeTicks::Now()) {
  // Do an initial call to SampleMetrics() so that the next one returns
  // meaningful data.
  SampleMetrics(*this->process_metrics);

#if BUILDFLAG(IS_WIN) && !defined(ARCH_CPU_ARM64)
  // Record the value of HasConstantRateTSC to get a feel of the proportion of
  // users that don't record the average CPU usage histogram.
  base::UmaHistogramBoolean("PerformanceMonitor.HasPreciseCPUUsage",
                            base::time_internal::HasConstantRateTSC());
#endif
}
ProcessInfo::~ProcessInfo() = default;

ProcessMonitor::Metrics::Metrics() = default;
ProcessMonitor::Metrics::Metrics(const ProcessMonitor::Metrics& other) =
    default;
ProcessMonitor::Metrics& ProcessMonitor::Metrics::operator=(
    const ProcessMonitor::Metrics& other) = default;
ProcessMonitor::Metrics::~Metrics() = default;

ProcessMonitor::ProcessMonitor()
    : browser_process_info_(
          MonitoredProcessType::kBrowser,
          CreateProcessMetrics(base::GetCurrentProcessHandle())) {
  // Ensure ProcessMonitor is created before any child process so that none is
  // missed.
  DCHECK(content::BrowserChildProcessHostIterator().Done());
  DCHECK(content::RenderProcessHost::AllHostsIterator().IsAtEnd());

  content::BrowserChildProcessObserver::Add(this);
}

ProcessMonitor::~ProcessMonitor() {
  content::BrowserChildProcessObserver::Remove(this);
}

void ProcessMonitor::SampleAllProcesses(Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Accumulate all the different processes.
  std::vector<ProcessInfo*> process_infos;
  process_infos.reserve(1 + render_process_infos_.size() +
                        browser_child_process_infos_.size());
  process_infos.push_back(&browser_process_info_);
  for (auto& [_, process_info] : render_process_infos_)
    process_infos.push_back(&process_info);
  for (auto& [_, process_info] : browser_child_process_infos_)
    process_infos.push_back(&process_info);

  const base::TimeTicks now = base::TimeTicks::Now();

  // Aggregate all metrics into a single sum, but also per their process type.
  Metrics aggregated_metrics;
  std::array<Metrics, MonitoredProcessType::kCount> per_type_metrics;
  for (auto* process_info : process_infos) {
    Metrics metrics = SampleMetrics(*process_info->process_metrics);

    // If this is the first interval calculated for this process, then the
    // metrics values must be scaled down over the
    // |kLongPowerMetricsIntervalDuration|.
    if (process_info->first_sample_time.has_value()) {
      // Scale the amount.
      auto first_interval_duration = now - *process_info->first_sample_time;
      ScaleMetrics(&metrics,
                   first_interval_duration / kLongPowerMetricsIntervalDuration);

      // No longer the first interval after this one.
      process_info->first_sample_time = std::nullopt;
    }

    aggregated_metrics += metrics;
    per_type_metrics[process_info->type] += metrics;
  }

  for (int i = 0; i < MonitoredProcessType::kCount; i++) {
    // Add the metrics for the processes that exited during this interval and
    // zero out.
    per_type_metrics[i] += exited_processes_metrics_[i];
    exited_processes_metrics_[i] = Metrics();

    observer->OnMetricsSampled(static_cast<MonitoredProcessType>(i),
                               per_type_metrics[i]);
  }

  observer->OnAggregatedMetricsSampled(aggregated_metrics);
}

void ProcessMonitor::OnRenderProcessHostCreated(
    content::RenderProcessHost* render_process_host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // If the host is reused after the process exited, it is possible to get a
  // second created notification for the same host.
  if (!render_process_host_observations_.IsObservingSource(render_process_host))
    render_process_host_observations_.AddObservation(render_process_host);
}

void ProcessMonitor::RenderProcessReady(
    content::RenderProcessHost* render_process_host) {
  // TODO(pmonette): It's possible for a process to be launched and then teared
  //                 down without it being ever ready, which mean they will not
  //                 affect the performance metrics, even though they should.
  //                 Consider using `OnRenderProcessHostCreated()` instead of
  //                 `RenderProcessReady()`.
  bool inserted =
      render_process_infos_
          .emplace(
              std::piecewise_construct,
              std::forward_as_tuple(render_process_host),
              std::forward_as_tuple(
                  GetMonitoredProcessTypeForRenderProcess(render_process_host),
                  CreateProcessMetrics(
                      render_process_host->GetProcess().Handle())))
          .second;
  DCHECK(inserted);
}

void ProcessMonitor::RenderProcessExited(
    content::RenderProcessHost* render_process_host,
    const content::ChildProcessTerminationInfo& info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto it = render_process_infos_.find(render_process_host);
  if (it == render_process_infos_.end()) {
    // This process was never ready.
    return;
  }

  // Remember the metrics from when the process exited, if available.
  if (info.cpu_usage.has_value()) {
    const ProcessInfo& process_info = it->second;
    exited_processes_metrics_[process_info.type] += GetLastIntervalMetrics(
        *process_info.process_metrics, info.cpu_usage.value());
  }

  render_process_infos_.erase(it);
}

void ProcessMonitor::RenderProcessHostDestroyed(
    content::RenderProcessHost* render_process_host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  render_process_host_observations_.RemoveObservation(render_process_host);
}

void ProcessMonitor::BrowserChildProcessLaunchedAndConnected(
    const content::ChildProcessData& data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#if BUILDFLAG(IS_WIN)
  // Cannot gather process metrics for elevated process as browser has no
  // access to them.
  if (data.sandbox_type ==
      sandbox::mojom::Sandbox::kNoSandboxAndElevatedPrivileges) {
    return;
  }
#endif

  MonitoredProcessType type =
      GetMonitoredProcessTypeForNonRendererChildProcess(data);
  bool inserted =
      browser_child_process_infos_
          .emplace(std::piecewise_construct, std::forward_as_tuple(data.id),
                   std::forward_as_tuple(
                       type, CreateProcessMetrics(data.GetProcess().Handle())))
          .second;
  DCHECK(inserted);
}

void ProcessMonitor::BrowserChildProcessHostDisconnected(
    const content::ChildProcessData& data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#if BUILDFLAG(IS_WIN)
  // Cannot gather process metrics for elevated process as browser has no
  // access to them.
  if (data.sandbox_type ==
      sandbox::mojom::Sandbox::kNoSandboxAndElevatedPrivileges) {
    return;
  }
#endif

  DCHECK(browser_child_process_infos_.find(data.id) ==
         browser_child_process_infos_.end());
}

void ProcessMonitor::BrowserChildProcessCrashed(
    const content::ChildProcessData& data,
    const content::ChildProcessTerminationInfo& info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  OnBrowserChildProcessExited(data, info);
}

void ProcessMonitor::BrowserChildProcessKilled(
    const content::ChildProcessData& data,
    const content::ChildProcessTerminationInfo& info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  OnBrowserChildProcessExited(data, info);
}

void ProcessMonitor::BrowserChildProcessExitedNormally(
    const content::ChildProcessData& data,
    const content::ChildProcessTerminationInfo& info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  OnBrowserChildProcessExited(data, info);
}

void ProcessMonitor::OnBrowserChildProcessExited(
    const content::ChildProcessData& data,
    const content::ChildProcessTerminationInfo& info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#if BUILDFLAG(IS_WIN)
  // Cannot gather process metrics for elevated process as browser has no
  // access to them.
  if (data.sandbox_type ==
      sandbox::mojom::Sandbox::kNoSandboxAndElevatedPrivileges) {
    return;
  }
#endif
  auto it = browser_child_process_infos_.find(data.id);
  if (it == browser_child_process_infos_.end()) {
    // It is possible to receive this notification without a launch-and-connect
    // notification. See https://crbug.com/942500 for a similar issue.
    return;
  }

  CHECK(it != browser_child_process_infos_.end(), base::NotFatalUntil::M130);
  // Remember the metrics from when the process exited, if available.
  if (info.cpu_usage.has_value()) {
    const ProcessInfo& process_info = it->second;
    exited_processes_metrics_[process_info.type] += GetLastIntervalMetrics(
        *process_info.process_metrics, info.cpu_usage.value());
  }

  browser_child_process_infos_.erase(it);
}
