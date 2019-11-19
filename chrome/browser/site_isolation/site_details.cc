// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/site_isolation/site_details.h"

#include "base/metrics/histogram_macros.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
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

void SiteDetails::CollectSiteInfo(content::WebContents* contents,
                                  SiteData* site_data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // The primary should be the same for the whole tab.
  content::SiteInstance* primary =
      DeterminePrimarySiteInstance(contents->GetSiteInstance(), site_data);
  BrowsingInstanceInfo* browsing_instance =
      &site_data->browsing_instances[primary];

  for (RenderFrameHost* frame : contents->GetAllFrames()) {
    // Ensure that we add the frame's SiteInstance to |site_instances|.
    DCHECK(frame->GetSiteInstance()->IsRelatedSiteInstance(primary));
    browsing_instance->site_instances.insert(frame->GetSiteInstance());
    browsing_instance->proxy_count += frame->GetProxyCount();

    if (frame->GetParent()) {
      if (frame->GetSiteInstance() != frame->GetParent()->GetSiteInstance())
        site_data->out_of_process_frames++;
    }
  }
}

void SiteDetails::UpdateHistograms(
    const BrowserContextSiteDataMap& site_data_map,
    int all_renderer_process_count,
    int non_renderer_process_count) {
  // Sum the number of sites and SiteInstances in each BrowserContext and
  // the total number of out-of-process iframes.
  int num_browsing_instances = 0;
  int num_oopifs = 0;
  int num_proxies = 0;
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
  }

  // Just renderer process count:
  UMA_HISTOGRAM_COUNTS_100("SiteIsolation.CurrentRendererProcessCount",
                           all_renderer_process_count);
  UMA_HISTOGRAM_COUNTS_100("SiteIsolation.BrowsingInstanceCount",
                           num_browsing_instances);
  UMA_HISTOGRAM_COUNTS_10000("SiteIsolation.ProxyCount", num_proxies);
  UMA_HISTOGRAM_COUNTS_100("SiteIsolation.OutOfProcessIframes", num_oopifs);
}
