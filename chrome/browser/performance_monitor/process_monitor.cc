// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/process_monitor.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/observer_list.h"
#include "base/process/process_iterator.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/performance_monitor/process_metrics_history.h"
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

void GetProcessSubtypeForRenderProcess(content::RenderProcessHost* host,
                                       ProcessMetadata* data) {
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
    return;

  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(browser_context);

  const extensions::Extension* extension =
      extension_registry->enabled_extensions().GetByID(*extension_ids.begin());

  if (!extension)
    return;

  data->process_subtype =
      extensions::BackgroundInfo::HasPersistentBackgroundPage(extension)
          ? kProcessSubtypeExtensionPersistent
          : kProcessSubtypeExtensionEvent;
#endif
}

// Adds the values from |rhs| to |lhs|.
ProcessMonitor::Metrics& operator+=(ProcessMonitor::Metrics& lhs,
                                    const ProcessMonitor::Metrics& rhs) {
  lhs.cpu_usage += rhs.cpu_usage;

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

ProcessMonitor::ProcessMonitor() {
  DCHECK(!g_process_monitor);
  g_process_monitor = this;

  // Ensure ProcessMonitor is created before any child process so that none is
  // missed.
  DCHECK(content::BrowserChildProcessHostIterator().Done());
  DCHECK(content::RenderProcessHost::AllHostsIterator().IsAtEnd());

  content::BrowserChildProcessObserver::Add(this);

  // Add the current (browser) process.
  browser_process_metrics_ = std::make_unique<ProcessMetricsHistory>();
  ProcessMetadata browser_process_data;
  browser_process_data.process_type = content::PROCESS_TYPE_BROWSER;
  browser_process_data.handle = base::GetCurrentProcessHandle();
  browser_process_metrics_->Initialize(browser_process_data);

  // TODO(pmonette): Do an initial call to SampleMetrics() so that the next one
  //                 returns meaningful data.
  // browser_process_metrics_->SampleMetrics();
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
  auto [it, inserted] = render_process_metrics_.emplace(
      std::piecewise_construct, std::forward_as_tuple(render_process_host),
      std::forward_as_tuple(std::make_unique<ProcessMetricsHistory>()));
  DCHECK(inserted);

  ProcessMetricsHistory* process_metrics_history = it->second.get();

  ProcessMetadata process_metadata;
  process_metadata.process_type = content::PROCESS_TYPE_RENDERER;
  process_metadata.handle = render_process_host->GetProcess().Handle();
  GetProcessSubtypeForRenderProcess(render_process_host, &process_metadata);
  process_metrics_history->Initialize(process_metadata);

  // TODO(pmonette): Do an initial call to SampleMetrics() so that the next one
  //                 returns meaningful data.
  // process_metrics_history->SampleMetrics();
}

void ProcessMonitor::RenderProcessExited(
    content::RenderProcessHost* render_process_host,
    const content::ChildProcessTerminationInfo& info) {
  auto it = render_process_metrics_.find(render_process_host);
  if (it == render_process_metrics_.end()) {
    // This process was never ready.
    return;
  }
  render_process_metrics_.erase(it);
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

  auto [it, inserted] = browser_child_process_metrics_.emplace(
      std::piecewise_construct, std::forward_as_tuple(data.id),
      std::forward_as_tuple(std::make_unique<ProcessMetricsHistory>()));
  DCHECK(inserted);

  ProcessMetricsHistory* process_metrics_history = it->second.get();

  ProcessMetadata process_metadata;
  process_metadata.handle = data.GetProcess().Handle();
  process_metadata.process_type = data.process_type;
  if (data.metrics_name == network::mojom::NetworkService::Name_)
    process_metadata.process_subtype = kProcessSubtypeNetworkProcess;
  process_metrics_history->Initialize(process_metadata);

  // TODO(pmonette): Do an initial call to SampleMetrics() so that the next one
  //                 returns meaningful data.
  // process_metrics_history->SampleMetrics();
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

  auto it = browser_child_process_metrics_.find(data.id);
  if (it == browser_child_process_metrics_.end()) {
    // It is possible to receive this notification without a launch-and-connect
    // notification. See https://crbug.com/942500 for a similar issue.
    return;
  }

  browser_child_process_metrics_.erase(it);
}

void ProcessMonitor::SampleAllProcesses() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::vector<performance_monitor::ProcessMetricsHistory*>
      process_metrics_histories;
  process_metrics_histories.reserve(1 + render_process_metrics_.size() +
                                    browser_child_process_metrics_.size());
  process_metrics_histories.push_back(browser_process_metrics_.get());
  for (auto& [_, process_metrics_history] : render_process_metrics_)
    process_metrics_histories.push_back(process_metrics_history.get());
  for (auto& [_, process_metrics_history] : browser_child_process_metrics_)
    process_metrics_histories.push_back(process_metrics_history.get());

  Metrics aggregated_metrics;
  for (auto* process_metrics_history : process_metrics_histories) {
    Metrics metrics = process_metrics_history->SampleMetrics();
    aggregated_metrics += metrics;
    for (auto& observer : observer_list_)
      observer.OnMetricsSampled(process_metrics_history->metadata(), metrics);
  }
  for (auto& observer : observer_list_)
    observer.OnAggregatedMetricsSampled(aggregated_metrics);
}

}  // namespace performance_monitor
