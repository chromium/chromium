// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/site_isolation/site_details.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_isolation_policy.h"
#include "extensions/buildflags/buildflags.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#endif

using content::BrowserThread;
using content::RenderFrameHost;

namespace {

content::SiteInstance* DeterminePrimarySiteInstance(
    content::SiteInstance* site_instance,
    SiteData* site_data) {
  TRACE_EVENT1("navigation", "DeterminePrimarySiteInstance", "site_instance",
               site_instance);
  // Find the BrowsingInstance this WebContents belongs to by iterating over
  // the "primary" SiteInstances of each BrowsingInstance we've seen so far.
  for (auto& entry : site_data->browsing_instances) {
    BrowsingInstanceInfo* browsing_instance = &entry.second;
    content::SiteInstance* primary_for_browsing_instance = entry.first;

    if (site_instance->IsRelatedSiteInstance(primary_for_browsing_instance)) {
      browsing_instance->site_instances.insert(site_instance);
      return primary_for_browsing_instance;
    }
  }

  // Add |instance| as the "primary" SiteInstance of a new BrowsingInstance.
  BrowsingInstanceInfo* browsing_instance =
      &site_data->browsing_instances[site_instance];
  browsing_instance->site_instances.insert(site_instance);

  return site_instance;
}

}  // namespace

BrowsingInstanceInfo::BrowsingInstanceInfo() = default;
BrowsingInstanceInfo::BrowsingInstanceInfo(const BrowsingInstanceInfo& other) =
    default;
BrowsingInstanceInfo::~BrowsingInstanceInfo() = default;

SiteData::SiteData() = default;
SiteData::SiteData(const SiteData& other) = default;
SiteData::~SiteData() = default;

void SiteDetails::CollectSiteInfo(content::Page& page, SiteData* site_data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  page.GetMainDocument().ForEachRenderFrameHost(
      [site_data](RenderFrameHost* frame) {
        // Each Page (whether primary or nested) will have its own primary
        // SiteInstance and BrowsingInstance. With MPArch we may have multiple
        // pages which could be in separate BrowsingInstances for nested pages.
        content::SiteInstance* primary = DeterminePrimarySiteInstance(
            frame->GetPage().GetMainDocument().GetSiteInstance(), site_data);
        BrowsingInstanceInfo* browsing_instance =
            &site_data->browsing_instances[primary];

        // Ensure that we add the frame's SiteInstance to |site_instances|.
        DCHECK(frame->GetSiteInstance()->IsRelatedSiteInstance(primary));

        browsing_instance->site_instances.insert(frame->GetSiteInstance());
        browsing_instance->proxy_count += frame->GetProxyCount();

        if (frame->GetParent()) {
          if (frame->GetSiteInstance() != frame->GetParent()->GetSiteInstance())
            site_data->out_of_process_frames++;
        } else {
          // We are a main frame. If we are an inner frame tree and our parent's
          // process doesn't match ours, count it.
          if (frame->GetParentOrOuterDocument() &&
              frame->GetParentOrOuterDocument()->GetProcess() !=
                  frame->GetProcess()) {
            site_data->out_of_process_inner_frame_trees++;
          }
        }
      });
}

int SiteDetails::EstimateOriginAgentClusterOverhead(const SiteData& site_data) {
  if (!content::SiteIsolationPolicy::
          IsProcessIsolationForOriginAgentClusterEnabled()) {
    return 0;
  }

  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  int oac_overhead = 0;
  // We only want to call GetOacOverhead() once per BrowsingInstance, so only
  // call it using the primary SiteInstance.
  for (auto& entry : site_data.browsing_instances)
    oac_overhead += entry.first->EstimateOriginAgentClusterOverheadForMetrics();

  return oac_overhead;
}

void SiteDetails::UpdateHistograms(
    const BrowserContextSiteDataMap& site_data_map,
    size_t live_process_count) {
  // Sum the number of sites and SiteInstances in each BrowserContext and
  // the total number of out-of-process iframes.
  int num_browsing_instances = 0;
  int num_oopifs = 0;
  int num_proxies = 0;
  int num_oop_inner_frame_trees = 0;
  int extra_processes_from_oac = 0;
  for (auto& site_data_map_entry : site_data_map) {
    const SiteData& site_data = site_data_map_entry.second;
    for (const auto& entry : site_data.browsing_instances) {
      const BrowsingInstanceInfo& browsing_instance_info = entry.second;
      UMA_HISTOGRAM_COUNTS_100("SiteIsolation.SiteInstancesPerBrowsingInstance",
                               browsing_instance_info.site_instances.size());
      UMA_HISTOGRAM_COUNTS_10000("SiteIsolation.ProxyCountPerBrowsingInstance",
                                 browsing_instance_info.proxy_count);
      num_proxies += browsing_instance_info.proxy_count;
    }
    num_browsing_instances += site_data.browsing_instances.size();
    num_oopifs += site_data.out_of_process_frames;
    num_oop_inner_frame_trees += site_data.out_of_process_inner_frame_trees;
    extra_processes_from_oac += EstimateOriginAgentClusterOverhead(site_data);
  }

  int oac_overhead_percent =
      live_process_count == 0
          ? 0
          : static_cast<int>(100 *
                             (static_cast<float>(extra_processes_from_oac) /
                              static_cast<float>(live_process_count)));

  base::UmaHistogramCounts100("SiteIsolation.BrowsingInstanceCount",
                              num_browsing_instances);
  base::UmaHistogramCounts10000("SiteIsolation.ProxyCount", num_proxies);
  base::UmaHistogramCounts100("SiteIsolation.OutOfProcessIframes", num_oopifs);
  base::UmaHistogramCounts100("SiteIsolation.OutOfProcessInnerFrameTrees",
                              num_oop_inner_frame_trees);

  // Log metrics related to the actual & potential process overhead of isolated
  // sandboxed iframes.
  RenderFrameHost::LogSandboxedIframesIsolationMetrics();

  if (!content::SiteIsolationPolicy::
          IsProcessIsolationForOriginAgentClusterEnabled()) {
    return;
  }

  UMA_HISTOGRAM_COUNTS_100(
      "Memory.RenderProcessHost.Count.OriginAgentClusterOverhead",
      extra_processes_from_oac);
  UMA_HISTOGRAM_PERCENTAGE(
      "Memory.RenderProcessHost.Percent.OriginAgentClusterOverhead",
      oac_overhead_percent);
}
