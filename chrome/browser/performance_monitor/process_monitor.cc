// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/process_monitor.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/process/process_iterator.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/performance_monitor/process_metrics_history.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_features.h"
#include "extensions/buildflags/buildflags.h"
#include "services/network/public/mojom/network_service.mojom.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_map.h"
#include "extensions/common/manifest_handlers/background_info.h"
#endif

using content::BrowserThread;

namespace performance_monitor {

namespace {

// The default interval at which ProcessMonitor performs its timed
// collections.
constexpr base::TimeDelta kGatherInterval = base::TimeDelta::FromSeconds(120);

// The global instance.
ProcessMonitor* g_process_monitor = nullptr;

void GatherMetricsForRenderProcess(content::RenderProcessHost* host,
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

#if defined(OS_MAC) || defined(OS_LINUX) || defined(OS_CHROMEOS) || \
    defined(OS_AIX)
  lhs.idle_wakeups += rhs.idle_wakeups;
#endif

#if defined(OS_MAC)
  lhs.package_idle_wakeups += rhs.package_idle_wakeups;
  lhs.energy_impact += rhs.energy_impact;
#endif

  return lhs;
}

}  // namespace

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
}

void ProcessMonitor::StartGatherCycle() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  repeating_timer_.Start(FROM_HERE, kGatherInterval, this,
                         &ProcessMonitor::GatherProcesses);
}

base::TimeDelta ProcessMonitor::GetScheduledSamplingInterval() const {
  return kGatherInterval;
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
}

void ProcessMonitor::MarkProcessAsAlive(const ProcessMetadata& process_data,
                                        int current_update_sequence) {
  const base::ProcessHandle& handle = process_data.handle;
  if (handle == base::kNullProcessHandle) {
    // Process may not be valid yet.
    return;
  }

  auto process_metrics_iter = metrics_map_.find(handle);
  if (process_metrics_iter == metrics_map_.end()) {
    // If we're not already watching the process, let's initialize it.
    metrics_map_[handle] = std::make_unique<ProcessMetricsHistory>();
    metrics_map_[handle]->Initialize(process_data, current_update_sequence);
  } else {
    // If we are watching the process, touch it to keep it alive.
    ProcessMetricsHistory* process_metrics = process_metrics_iter->second.get();
    process_metrics->set_last_update_sequence(current_update_sequence);
  }
}

// static
std::vector<ProcessMetadata> ProcessMonitor::GatherProcessesOnUIThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::vector<ProcessMetadata> processes;

  // Find all render child processes; has to be done on the UI thread.
  for (content::RenderProcessHost::iterator rph_iter =
           content::RenderProcessHost::AllHostsIterator();
       !rph_iter.IsAtEnd(); rph_iter.Advance()) {
    content::RenderProcessHost* host = rph_iter.GetCurrentValue();
    ProcessMetadata data;
    data.process_type = content::PROCESS_TYPE_RENDERER;
    data.handle = host->GetProcess().Handle();

    GatherMetricsForRenderProcess(host, &data);

    processes.push_back(data);
  }

  return processes;
}

// static
std::vector<ProcessMetadata> ProcessMonitor::GatherProcessesOnProcessThread() {
  DCHECK_CURRENTLY_ON(base::FeatureList::IsEnabled(features::kProcessHostOnUI)
                          ? BrowserThread::UI
                          : BrowserThread::IO);

  std::vector<ProcessMetadata> processes;

  // Find all child processes (does not include renderers), which has to be
  // done on the IO thread.
  for (content::BrowserChildProcessHostIterator iter; !iter.Done(); ++iter) {
    ProcessMetadata child_process_data;
    child_process_data.handle = iter.GetData().GetProcess().Handle();
    child_process_data.process_type = iter.GetData().process_type;

    if (iter.GetData().metrics_name == network::mojom::NetworkService::Name_) {
      child_process_data.process_subtype = kProcessSubtypeNetworkProcess;
    }

    processes.push_back(child_process_data);
  }

  // Add the current (browser) process.
  ProcessMetadata browser_process_data;
  browser_process_data.process_type = content::PROCESS_TYPE_BROWSER;
  browser_process_data.handle = base::GetCurrentProcessHandle();

  processes.push_back(browser_process_data);

  // Update metrics for all watched processes; remove dead entries from the map.

  return processes;
}

void ProcessMonitor::GatherProcesses() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  static uint32_t current_update_sequence = 0;
  // Even in the "somewhat" unlikely event this wraps around,
  // it doesn't matter. We just check it for inequality.
  current_update_sequence++;

  // This function is already running on the UI thread, so gather all ui thread
  // processes.
  std::vector<ProcessMetadata> ui_thread_processes =
      GatherProcessesOnUIThread();

  auto task_runner = base::FeatureList::IsEnabled(features::kProcessHostOnUI)
                         ? content::GetUIThreadTaskRunner({})
                         : content::GetIOThreadTaskRunner({});
  // Then retrieve process thread processes and invoke GatherMetrics() with both
  // set of processes.
  task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ProcessMonitor::GatherProcessesOnProcessThread),
      base::BindOnce(&ProcessMonitor::GatherMetrics,
                     weak_ptr_factory_.GetWeakPtr(), current_update_sequence,
                     std::move(ui_thread_processes)));
}

void ProcessMonitor::GatherMetrics(
    int current_update_sequence,
    std::vector<ProcessMetadata> ui_thread_processes,
    std::vector<ProcessMetadata> io_thread_processes) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (const auto& process : ui_thread_processes)
    MarkProcessAsAlive(process, current_update_sequence);
  for (const auto& process : io_thread_processes)
    MarkProcessAsAlive(process, current_update_sequence);

  // Update metrics for all watched processes; remove dead entries from the map.
  Metrics aggregated_metrics;
  auto iter = metrics_map_.begin();
  while (iter != metrics_map_.end()) {
    ProcessMetricsHistory* process_metrics = iter->second.get();
    if (process_metrics->last_update_sequence() != current_update_sequence) {
      // Not touched this iteration; let's get rid of it.
      metrics_map_.erase(iter++);
    } else {
      Metrics metrics = process_metrics->SampleMetrics();
      aggregated_metrics += metrics;
      for (auto& observer : observer_list_)
        observer.OnMetricsSampled(process_metrics->metadata(), metrics);
      ++iter;
    }
  }

  for (auto& observer : observer_list_)
    observer.OnAggregatedMetricsSampled(aggregated_metrics);
}

}  // namespace performance_monitor
