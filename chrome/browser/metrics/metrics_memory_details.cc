// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/metrics_memory_details.h"

#include <stddef.h>

#include <vector>

#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/site_isolation/site_details.h"
#include "components/nacl/common/nacl_process_type.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/process_type.h"
#include "ppapi/buildflags/buildflags.h"
#include "third_party/leveldatabase/leveldb_chrome.h"

namespace {

void CountRenderProcessHosts(size_t* initialized_and_not_dead, size_t* all) {
  *initialized_and_not_dead = *all = 0;

  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  for (auto iter = content::RenderProcessHost::AllHostsIterator();
       !iter.IsAtEnd(); iter.Advance()) {
    content::RenderProcessHost& render_process_host = *iter.GetCurrentValue();
    ++*all;
    if (render_process_host.IsInitializedAndNotDead())
      ++*initialized_and_not_dead;
  }
}

}  // namespace

MetricsMemoryDetails::MetricsMemoryDetails(const base::Closure& callback)
    : callback_(callback) {}

MetricsMemoryDetails::~MetricsMemoryDetails() {
}

void MetricsMemoryDetails::OnDetailsAvailable() {
  UpdateHistograms();
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback_);
}

void MetricsMemoryDetails::UpdateHistograms() {
  // Reports a set of memory metrics to UMA.

  const ProcessData& browser = *ChromeBrowser();
  int chrome_count = 0;
  int extension_count = 0;
  int renderer_count = 0;
  for (size_t index = 0; index < browser.processes.size(); index++) {
    int num_open_fds = browser.processes[index].num_open_fds;
    int open_fds_soft_limit = browser.processes[index].open_fds_soft_limit;
    switch (browser.processes[index].process_type) {
      case content::PROCESS_TYPE_BROWSER:
        if (num_open_fds != -1 && open_fds_soft_limit != -1) {
          UMA_HISTOGRAM_COUNTS_10000("Memory.Browser.OpenFDs", num_open_fds);
          UMA_HISTOGRAM_COUNTS_10000("Memory.Browser.OpenFDsSoftLimit",
                                     open_fds_soft_limit);
        }
        continue;
      case content::PROCESS_TYPE_RENDERER: {
        if (num_open_fds != -1 && open_fds_soft_limit != -1) {
          UMA_HISTOGRAM_COUNTS_10000("Memory.RendererAll.OpenFDs",
                                     num_open_fds);
          UMA_HISTOGRAM_COUNTS_10000("Memory.RendererAll.OpenFDsSoftLimit",
                                     open_fds_soft_limit);
        }
        ProcessMemoryInformation::RendererProcessType renderer_type =
            browser.processes[index].renderer_type;
        switch (renderer_type) {
          case ProcessMemoryInformation::RENDERER_EXTENSION:
            if (num_open_fds != -1) {
              UMA_HISTOGRAM_COUNTS_10000("Memory.Extension.OpenFDs",
                                         num_open_fds);
            }
            extension_count++;
            continue;
          case ProcessMemoryInformation::RENDERER_CHROME:
            if (num_open_fds != -1)
              UMA_HISTOGRAM_COUNTS_10000("Memory.Chrome.OpenFDs", num_open_fds);
            chrome_count++;
            continue;
          case ProcessMemoryInformation::RENDERER_UNKNOWN:
            NOTREACHED() << "Unknown renderer process type.";
            continue;
          case ProcessMemoryInformation::RENDERER_NORMAL:
          default:
            if (num_open_fds != -1) {
              UMA_HISTOGRAM_COUNTS_10000("Memory.Renderer.OpenFDs",
                                         num_open_fds);
            }
            renderer_count++;
            continue;
        }
      }
      case content::PROCESS_TYPE_UTILITY:
        if (num_open_fds != -1)
          UMA_HISTOGRAM_COUNTS_10000("Memory.Utility.OpenFDs", num_open_fds);
        continue;
      case content::PROCESS_TYPE_ZYGOTE:
        if (num_open_fds != -1)
          UMA_HISTOGRAM_COUNTS_10000("Memory.Zygote.OpenFDs", num_open_fds);
        continue;
      case content::PROCESS_TYPE_SANDBOX_HELPER:
        if (num_open_fds != -1) {
          UMA_HISTOGRAM_COUNTS_10000("Memory.SandboxHelper.OpenFDs",
                                     num_open_fds);
        }
        continue;
      case content::PROCESS_TYPE_GPU:
        if (num_open_fds != -1 && open_fds_soft_limit != -1) {
          UMA_HISTOGRAM_COUNTS_10000("Memory.Gpu.OpenFDs", num_open_fds);
          UMA_HISTOGRAM_COUNTS_10000("Memory.Gpu.OpenFDsSoftLimit",
                                     open_fds_soft_limit);
        }
        continue;
#if BUILDFLAG(ENABLE_PLUGINS)
      case content::PROCESS_TYPE_PPAPI_PLUGIN: {
        if (num_open_fds != -1) {
          UMA_HISTOGRAM_COUNTS_10000("Memory.PepperPlugin.OpenFDs",
                                     num_open_fds);
        }
        continue;
      }
      case content::PROCESS_TYPE_PPAPI_BROKER:
        if (num_open_fds != -1) {
          UMA_HISTOGRAM_COUNTS_10000("Memory.PepperPluginBroker.OpenFDs",
                                     num_open_fds);
        }
        continue;
#endif
      case PROCESS_TYPE_NACL_LOADER:
        if (num_open_fds != -1) {
          UMA_HISTOGRAM_COUNTS_10000("Memory.NativeClient.OpenFDs",
                                     num_open_fds);
        }
        continue;
      case PROCESS_TYPE_NACL_BROKER:
        if (num_open_fds != -1) {
          UMA_HISTOGRAM_COUNTS_10000("Memory.NativeClientBroker.OpenFDs",
                                     num_open_fds);
        }
        continue;
      default:
        NOTREACHED();
        continue;
    }
  }
#if defined(OS_CHROMEOS)
  // Chrome OS exposes system-wide graphics driver memory which has historically
  // been a source of leak/bloat.
  base::SystemMemoryInfoKB meminfo;
  if (base::GetSystemMemoryInfo(&meminfo) && meminfo.gem_size != -1)
    UMA_HISTOGRAM_MEMORY_MB("Memory.Graphics", meminfo.gem_size / 1024 / 1024);
#endif

  // Predict the number of processes needed when isolating all sites and when
  // isolating only HTTPS sites.
  int all_renderer_count = renderer_count + chrome_count + extension_count;
  int non_renderer_count = browser.processes.size() - all_renderer_count;
  DCHECK_GE(non_renderer_count, 1);
  UpdateSiteIsolationMetrics(all_renderer_count, non_renderer_count);

  UMA_HISTOGRAM_COUNTS_100("Memory.ProcessCount",
                           static_cast<int>(browser.processes.size()));
  UMA_HISTOGRAM_COUNTS_100("Memory.ExtensionProcessCount", extension_count);
  UMA_HISTOGRAM_COUNTS_100("Memory.RendererProcessCount", renderer_count);

  size_t initialized_and_not_dead_rphs, all_rphs;
  CountRenderProcessHosts(&initialized_and_not_dead_rphs, &all_rphs);
  UMA_HISTOGRAM_COUNTS_100("Memory.RenderProcessHost.Count.All", all_rphs);
  UMA_HISTOGRAM_COUNTS_100(
      "Memory.RenderProcessHost.Count.InitializedAndNotDead",
      initialized_and_not_dead_rphs);

  leveldb_chrome::UpdateHistograms();
}

void MetricsMemoryDetails::UpdateSiteIsolationMetrics(int all_renderer_count,
                                                      int non_renderer_count) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // Track site data for predicting process counts with out-of-process iframes.
  // See site_details.h.
  BrowserContextSiteDataMap site_data_map;

  // First pass, collate the widgets by process ID.
  std::unique_ptr<content::RenderWidgetHostIterator> widget_it(
      content::RenderWidgetHost::GetRenderWidgetHosts());
  while (content::RenderWidgetHost* widget = widget_it->GetNextHost()) {
    // Ignore processes that don't have a connection, such as crashed tabs,
    // or processes that are still launching.
    if (!widget->GetProcess()->IsReady())
      continue;

    content::RenderViewHost* rvh = content::RenderViewHost::From(widget);
    if (!rvh)
      continue;

    content::WebContents* contents =
        content::WebContents::FromRenderViewHost(rvh);
    if (!contents)
      continue;

    // If this is a RVH for a subframe; skip it to avoid double-counting the
    // WebContents.
    if (rvh != contents->GetRenderViewHost())
      continue;

    // The rest of this block will happen only once per WebContents.
    SiteData& site_data = site_data_map[contents->GetBrowserContext()];
    SiteDetails::CollectSiteInfo(contents, &site_data);
  }
  SiteDetails::UpdateHistograms(site_data_map, all_renderer_count,
                                non_renderer_count);
}
