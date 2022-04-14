// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/process_monitor.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/observer_list.h"
#include "base/process/process_handle.h"
#include "base/process/process_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
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

namespace performance_monitor {

namespace {

// The global instance.
ProcessMonitor* g_process_monitor = nullptr;

std::unique_ptr<base::ProcessMetrics> CreateProcessMetrics(
    base::ProcessHandle process_handle) {
#if BUILDFLAG(IS_MAC)
  return base::ProcessMetrics::CreateProcessMetrics(
      process_handle, content::BrowserChildProcessHost::GetPortProvider());
#else
  return base::ProcessMetrics::CreateProcessMetrics(process_handle);
#endif
}

ProcessMonitor::Metrics SampleMetrics(base::ProcessMetrics& process_metrics) {
  ProcessMonitor::Metrics metrics;

  metrics.cpu_usage = process_metrics.GetPlatformIndependentCPUUsage();
#if BUILDFLAG(IS_WIN)
  metrics.precise_cpu_usage = process_metrics.GetPreciseCPUUsage();
#endif
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_AIX)
  metrics.idle_wakeups = process_metrics.GetIdleWakeupsPerSecond();
#endif
#if BUILDFLAG(IS_MAC)
  metrics.package_idle_wakeups =
      process_metrics.GetPackageIdleWakeupsPerSecond();
  metrics.energy_impact = process_metrics.GetEnergyImpact();
#endif

  return metrics;
}

ProcessSubtypes GetProcessSubtypeForRenderProcess(
    content::RenderProcessHost* host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  content::BrowserContext* browser_context = host->GetBrowserContext();
  extensions::ProcessMap* extension_process_map =
      extensions::ProcessMap::Get(browser_context);

  std::set<std::string> extension_ids =
      extension_process_map->GetExtensionsInProcess(host->GetID());

  // We only collect more granular metrics when there's only one extension
  // running in a given renderer, to reduce noise.
  if (extension_ids.size() != 1)
    return kProcessSubtypeUnknown;

  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(browser_context);

  const extensions::Extension* extension =
      extension_registry->enabled_extensions().GetByID(*extension_ids.begin());

  if (!extension)
    return kProcessSubtypeUnknown;

  return extensions::BackgroundInfo::HasPersistentBackgroundPage(extension)
             ? kProcessSubtypeExtensionPersistent
             : kProcessSubtypeExtensionEvent;
#else
  return kProcessSubtypeUnknown;
#endif
}

// Adds the values from |rhs| to |lhs|.
ProcessMonitor::Metrics& operator+=(ProcessMonitor::Metrics& lhs,
                                    const ProcessMonitor::Metrics& rhs) {
  lhs.cpu_usage += rhs.cpu_usage;

#if BUILDFLAG(IS_WIN)
  lhs.precise_cpu_usage += rhs.precise_cpu_usage;
#endif

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_AIX)
  lhs.idle_wakeups += rhs.idle_wakeups;
#endif

#if BUILDFLAG(IS_MAC)
  lhs.package_idle_wakeups += rhs.package_idle_wakeups;
  lhs.energy_impact += rhs.energy_impact;
#endif

  return lhs;
}

}  // namespace

constexpr base::TimeDelta ProcessMonitor::kGatherInterval;

ProcessInfo::ProcessInfo(int process_type,
                         ProcessSubtypes process_subtype,
                         std::unique_ptr<base::ProcessMetrics> process_metrics)
    : process_type(process_type),
      process_subtype(process_subtype),
      process_metrics(std::move(process_metrics)) {}
ProcessInfo::~ProcessInfo() = default;

ProcessMonitor::Metrics::Metrics() = default;
ProcessMonitor::Metrics::Metrics(const ProcessMonitor::Metrics& other) =
    default;
ProcessMonitor::Metrics& ProcessMonitor::Metrics::operator=(
    const ProcessMonitor::Metrics& other) = default;
ProcessMonitor::Metrics::~Metrics() = default;

// static
std::unique_ptr<ProcessMonitor> ProcessMonitor::Create() {
  DCHECK(!g_process_monitor);
  return base::WrapUnique(new ProcessMonitor());
}

// static
ProcessMonitor* ProcessMonitor::Get() {
  return g_process_monitor;
}

ProcessMonitor::~ProcessMonitor() {
  DCHECK(g_process_monitor);
  g_process_monitor = nullptr;

  content::BrowserChildProcessObserver::Remove(this);
}

void ProcessMonitor::StartGatherCycle() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  repeating_timer_.Start(FROM_HERE, kGatherInterval, this,
                         &ProcessMonitor::SampleAllProcesses);
}

void ProcessMonitor::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ProcessMonitor::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

ProcessMonitor::ProcessMonitor()
    : browser_process_info_(
          content::PROCESS_TYPE_BROWSER,
          kProcessSubtypeUnknown,
          CreateProcessMetrics(base::GetCurrentProcessHandle())) {
  DCHECK(!g_process_monitor);
  g_process_monitor = this;

  // Ensure ProcessMonitor is created before any child process so that none is
  // missed.
  DCHECK(content::BrowserChildProcessHostIterator().Done());
  DCHECK(content::RenderProcessHost::AllHostsIterator().IsAtEnd());

  content::BrowserChildProcessObserver::Add(this);

  // TODO(pmonette): Do an initial call to SampleMetrics() so that the next one
  //                 returns meaningful data.
}

void ProcessMonitor::OnRenderProcessHostCreated(
    content::RenderProcessHost* render_process_host) {
  // If the host is reused after the process exited, it is possible to get a
  // second created notification for the same host.
  if (!render_process_host_observations_.IsObservingSource(render_process_host))
    render_process_host_observations_.AddObservation(render_process_host);
}

void ProcessMonitor::RenderProcessReady(
    content::RenderProcessHost* render_process_host) {
  bool inserted =
      render_process_infos_
          .emplace(std::piecewise_construct,
                   std::forward_as_tuple(render_process_host),
                   std::forward_as_tuple(
                       content::PROCESS_TYPE_RENDERER,
                       GetProcessSubtypeForRenderProcess(render_process_host),
                       CreateProcessMetrics(
                           render_process_host->GetProcess().Handle())))
          .second;
  DCHECK(inserted);

  // TODO(pmonette): Do an initial call to SampleMetrics() so that the next one
  //                 returns meaningful data.
}

void ProcessMonitor::RenderProcessExited(
    content::RenderProcessHost* render_process_host,
    const content::ChildProcessTerminationInfo& info) {
  auto it = render_process_infos_.find(render_process_host);
  if (it == render_process_infos_.end()) {
    // This process was never ready.
    return;
  }
  render_process_infos_.erase(it);
}

void ProcessMonitor::RenderProcessHostDestroyed(
    content::RenderProcessHost* render_process_host) {
  render_process_host_observations_.RemoveObservation(render_process_host);
}

void ProcessMonitor::BrowserChildProcessLaunchedAndConnected(
    const content::ChildProcessData& data) {
#if BUILDFLAG(IS_WIN)
  // Cannot gather process metrics for elevated process as browser has no
  // access to them.
  if (data.sandbox_type ==
      sandbox::mojom::Sandbox::kNoSandboxAndElevatedPrivileges) {
    return;
  }
#endif

  ProcessSubtypes process_subtype =
      data.metrics_name == network::mojom::NetworkService::Name_
          ? kProcessSubtypeNetworkProcess
          : kProcessSubtypeUnknown;
  bool inserted =
      browser_child_process_infos_
          .emplace(std::piecewise_construct, std::forward_as_tuple(data.id),
                   std::forward_as_tuple(
                       data.process_type, process_subtype,
                       CreateProcessMetrics(data.GetProcess().Handle())))
          .second;
  DCHECK(inserted);

  // TODO(pmonette): Do an initial call to SampleMetrics() so that the next one
  //                 returns meaningful data.
}

void ProcessMonitor::BrowserChildProcessHostDisconnected(
    const content::ChildProcessData& data) {
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

  browser_child_process_infos_.erase(it);
}

void ProcessMonitor::SampleAllProcesses() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::vector<ProcessInfo*> process_infos;
  process_infos.reserve(1 + render_process_infos_.size() +
                        browser_child_process_infos_.size());
  process_infos.push_back(&browser_process_info_);
  for (auto& [_, process_info] : render_process_infos_)
    process_infos.push_back(&process_info);
  for (auto& [_, process_info] : browser_child_process_infos_)
    process_infos.push_back(&process_info);

  Metrics aggregated_metrics;
  for (auto* process_info : process_infos) {
    Metrics metrics = SampleMetrics(*process_info->process_metrics);
    aggregated_metrics += metrics;
    for (auto& observer : observer_list_)
      observer.OnMetricsSampled(process_info->process_type,
                                process_info->process_subtype, metrics);
  }
  for (auto& observer : observer_list_)
    observer.OnAggregatedMetricsSampled(aggregated_metrics);
}

}  // namespace performance_monitor
