// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/metrics_memory_details.h"

#include <stddef.h>

#include <vector>

#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
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

MetricsMemoryDetails::MetricsMemoryDetails(base::OnceClosure callback)
    : callback_(std::move(callback)) {}

MetricsMemoryDetails::~MetricsMemoryDetails() {
}

void MetricsMemoryDetails::OnDetailsAvailable() {
  UpdateHistograms();
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(callback_));
}

void MetricsMemoryDetails::UpdateHistograms() {
  // Reports a set of memory metrics to UMA.

  const ProcessData& browser = *ChromeBrowser();
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
            continue;
          case ProcessMemoryInformation::RENDERER_CHROME:
            if (num_open_fds != -1)
              UMA_HISTOGRAM_COUNTS_10000("Memory.Chrome.OpenFDs", num_open_fds);
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
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Chrome OS exposes system-wide graphics driver memory which has historically
  // been a source of leak/bloat.
  base::GraphicsMemoryInfoKB meminfo;
  if (base::GetGraphicsMemoryInfo(&meminfo)) {
    UMA_HISTOGRAM_MEMORY_MB("Memory.Graphics",
                            meminfo.gpu_memory_size / 1024 / 1024);
  }
#endif

  size_t initialized_and_not_dead_rphs;
  size_t all_rphs;
  CountRenderProcessHosts(&initialized_and_not_dead_rphs, &all_rphs);
  UpdateSiteIsolationMetrics(initialized_and_not_dead_rphs);

  UMA_HISTOGRAM_COUNTS_100("Memory.ProcessCount",
                           static_cast<int>(browser.processes.size()));
  UMA_HISTOGRAM_COUNTS_100("Memory.RendererProcessCount", renderer_count);

  UMA_HISTOGRAM_COUNTS_100("Memory.RenderProcessHost.Count.All", all_rphs);
  UMA_HISTOGRAM_COUNTS_100(
      "Memory.RenderProcessHost.Count.InitializedAndNotDead",
      initialized_and_not_dead_rphs);
}

void MetricsMemoryDetails::UpdateSiteIsolationMetrics(
    size_t live_process_count) {
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

    // Skip if this is not the RVH for the primary main frame of |contents|, as
    // we want to call SiteDetails::CollectSiteInfo() once per WebContents.
    if (rvh !=
        contents->GetPrimaryPage().GetMainDocument().GetRenderViewHost()) {
      // |rvh| is for a subframe document or a non primary page main document.
      continue;
    }

    // The rest of this block will happen only once per WebContents.
    SiteData& site_data = site_data_map[contents->GetBrowserContext()];
    SiteDetails::CollectSiteInfo(contents->GetPrimaryPage(), &site_data);
  }
  SiteDetails::UpdateHistograms(site_data_map, live_process_count);
}
