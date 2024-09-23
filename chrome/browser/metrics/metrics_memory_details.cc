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
  for (auto& process : browser.processes) {
    if (process.process_type == content::PROCESS_TYPE_RENDERER) {
      ProcessMemoryInformation::RendererProcessType renderer_type =
          process.renderer_type;
      switch (renderer_type) {
        case ProcessMemoryInformation::RENDERER_EXTENSION:
        case ProcessMemoryInformation::RENDERER_CHROME:
          continue;
        case ProcessMemoryInformation::RENDERER_UNKNOWN:
          NOTREACHED_IN_MIGRATION() << "Unknown renderer process type.";
          continue;
        case ProcessMemoryInformation::RENDERER_NORMAL:
        default:
          renderer_count++;
          continue;
      }
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
