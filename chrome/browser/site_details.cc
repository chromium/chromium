// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/site_details.h"

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

using content::BrowserContext;
using content::BrowserThread;
using content::RenderFrameHost;
using content::RenderProcessHost;
using content::SiteInstance;
using content::WebContents;

namespace {

bool ShouldIsolate(BrowserContext* browser_context,
                   const IsolationScenario& scenario,
                   const GURL& site) {
  switch (scenario.policy) {
    case ISOLATE_NOTHING:
      return false;
    case ISOLATE_ALL_SITES:
      return true;
    case ISOLATE_HTTPS_SITES:
      // Note: For estimation purposes "isolate https sites" is really
      // implemented as "isolate non-http sites". This means that, for example,
      // the New Tab Page gets counted as two processes under this policy, and
      // extensions are isolated as well.
      return !site.SchemeIs(url::kHttpScheme);
    case ISOLATE_EXTENSIONS: {
#if !BUILDFLAG(ENABLE_EXTENSIONS)
      return false;
#else
      if (!site.SchemeIs(extensions::kExtensionScheme))
        return false;
      extensions::ExtensionRegistry* registry =
          extensions::ExtensionRegistry::Get(browser_context);
      const extensions::Extension* extension =
          registry->enabled_extensions().GetExtensionOrAppByURL(site);
      return extension && !extension->is_hosted_app();
#endif
    }
  }
  NOTREACHED();
  return true;
}

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

ScenarioBrowsingInstanceInfo::ScenarioBrowsingInstanceInfo() {}

ScenarioBrowsingInstanceInfo::ScenarioBrowsingInstanceInfo(
    const ScenarioBrowsingInstanceInfo& other) = default;

ScenarioBrowsingInstanceInfo::~ScenarioBrowsingInstanceInfo() {}

BrowsingInstanceInfo::BrowsingInstanceInfo() {}

BrowsingInstanceInfo::BrowsingInstanceInfo(const BrowsingInstanceInfo& other) =
    default;

BrowsingInstanceInfo::~BrowsingInstanceInfo() {}

IsolationScenario::IsolationScenario() {}

IsolationScenario::IsolationScenario(const IsolationScenario& other) = default;

IsolationScenario::~IsolationScenario() {}

SiteData::SiteData() {
  for (int i = 0; i <= ISOLATION_SCENARIO_LAST; i++)
    scenarios[i].policy = static_cast<IsolationScenarioType>(i);
}

SiteData::SiteData(const SiteData& other) = default;

SiteData::~SiteData() {}

SiteDetails::SiteDetails() {}

SiteDetails::~SiteDetails() {}

void SiteDetails::CollectSiteInfo(WebContents* contents,
                                  SiteData* site_data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserContext* context = contents->GetBrowserContext();

  // The primary should be the same for the whole tab.
  SiteInstance* primary =
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

  // Now keep track of how many sites we have in this BrowsingInstance (and
  // overall), including sites in iframes.
  for (IsolationScenario& scenario : site_data->scenarios) {
    std::map<RenderFrameHost*, GURL> frame_urls;
    for (RenderFrameHost* frame : contents->GetAllFrames()) {
      // Determine the site from the frame's origin, with a fallback to the
      // frame's URL.  In cases like <iframe sandbox>, we can wind up with an
      // http URL but a unique origin.  The origin of the resource will still
      // determine process placement.
      url::Origin origin = frame->GetLastCommittedOrigin();
      GURL site = SiteInstance::GetSiteForURL(
          context,
          origin.opaque() ? frame->GetLastCommittedURL() : origin.GetURL());

      bool should_isolate = ShouldIsolate(context, scenario, site);

      // Treat a subframe as part of its parent site if neither needs isolation.
      if (!should_isolate && frame->GetParent()) {
        GURL parent_site = frame_urls[frame->GetParent()];
        if (!ShouldIsolate(context, scenario, parent_site))
          site = parent_site;
      }

      bool process_per_site =
          site.is_valid() &&
          RenderProcessHost::ShouldUseProcessPerSite(context, site);

      // If we don't need a dedicated process, and aren't living in a process-
      // per-site process, we are nothing special: collapse our URL to a dummy
      // site.
      if (!process_per_site && !should_isolate)
        site = GURL("http://");

      // We model process-per-site by only inserting those sites into the first
      // browsing instance in which they appear.
      if (scenario.all_sites.insert(site).second || !process_per_site)
        scenario.browsing_instances[primary->GetId()].sites.insert(site);

      // Record our result in |frame_urls| for use by children.
      frame_urls[frame] = site;
    }
  }

}

void SiteDetails::UpdateHistograms(
    const BrowserContextSiteDataMap& site_data_map,
    int all_renderer_process_count,
    int non_renderer_process_count) {
  // Reports a set of site-based process metrics to UMA.
  int process_limit = RenderProcessHost::GetMaxRendererProcessCount();

  // Sum the number of sites and SiteInstances in each BrowserContext and
  // the total number of out-of-process iframes.
  int num_sites[ISOLATION_SCENARIO_LAST + 1] = {};
  int num_isolated_site_instances[ISOLATION_SCENARIO_LAST + 1] = {};
  int num_browsing_instances = 0;
  int num_oopifs = 0;
  int num_proxies = 0;
  for (auto& site_data_map_entry : site_data_map) {
    const SiteData& site_data = site_data_map_entry.second;
    for (const IsolationScenario& scenario : site_data.scenarios) {
      num_sites[scenario.policy] += scenario.all_sites.size();
      for (auto& entry : scenario.browsing_instances) {
        const ScenarioBrowsingInstanceInfo& scenario_browsing_instance_info =
            entry.second;
        num_isolated_site_instances[scenario.policy] +=
            scenario_browsing_instance_info.sites.size();
      }
    }
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

  // Predict the number of processes needed when isolating all sites, when
  // isolating only HTTPS sites, and when isolating extensions.
  int process_count_lower_bound[ISOLATION_SCENARIO_LAST + 1];
  int process_count_upper_bound[ISOLATION_SCENARIO_LAST + 1];
  int process_count_estimate[ISOLATION_SCENARIO_LAST + 1];
  for (int policy = 0; policy <= ISOLATION_SCENARIO_LAST; policy++) {
    process_count_lower_bound[policy] = num_sites[policy];
    process_count_upper_bound[policy] = num_sites[policy] + process_limit - 1;
    process_count_estimate[policy] = std::min(
        num_isolated_site_instances[policy], process_count_upper_bound[policy]);
  }

  // Just renderer process count:
  UMA_HISTOGRAM_COUNTS_100("SiteIsolation.CurrentRendererProcessCount",
                           all_renderer_process_count);
  UMA_HISTOGRAM_COUNTS_100(
      "SiteIsolation.BrowsingInstanceCount",
      num_browsing_instances);
  UMA_HISTOGRAM_COUNTS_10000("SiteIsolation.ProxyCount", num_proxies);
  UMA_HISTOGRAM_COUNTS_100("SiteIsolation.OutOfProcessIframes", num_oopifs);

  // ISOLATE_NOTHING metrics.
  UMA_HISTOGRAM_COUNTS_100("SiteIsolation.IsolateNothingProcessCountNoLimit",
                           num_isolated_site_instances[ISOLATE_NOTHING]);
  UMA_HISTOGRAM_COUNTS_100("SiteIsolation.IsolateNothingProcessCountLowerBound",
                           process_count_lower_bound[ISOLATE_NOTHING]);
  UMA_HISTOGRAM_COUNTS_100("SiteIsolation.IsolateNothingProcessCountEstimate",
                           process_count_estimate[ISOLATE_NOTHING]);
  UMA_HISTOGRAM_COUNTS_100(
      "SiteIsolation.IsolateNothingTotalProcessCountEstimate",
      process_count_estimate[ISOLATE_NOTHING] + non_renderer_process_count);

  // ISOLATE_ALL_SITES metrics.
  UMA_HISTOGRAM_COUNTS_100("SiteIsolation.IsolateAllSitesProcessCountNoLimit",
                           num_isolated_site_instances[ISOLATE_ALL_SITES]);
  UMA_HISTOGRAM_COUNTS_100(
      "SiteIsolation.IsolateAllSitesProcessCountLowerBound",
      process_count_lower_bound[ISOLATE_ALL_SITES]);
  UMA_HISTOGRAM_COUNTS_100("SiteIsolation.IsolateAllSitesProcessCountEstimate",
                           process_count_estimate[ISOLATE_ALL_SITES]);
  UMA_HISTOGRAM_COUNTS_100(
      "SiteIsolation.IsolateAllSitesTotalProcessCountEstimate",
      process_count_estimate[ISOLATE_ALL_SITES] + non_renderer_process_count);

  // ISOLATE_HTTPS_SITES metrics.
  UMA_HISTOGRAM_COUNTS_100("SiteIsolation.IsolateHttpsSitesProcessCountNoLimit",
                           num_isolated_site_instances[ISOLATE_HTTPS_SITES]);
  UMA_HISTOGRAM_COUNTS_100(
      "SiteIsolation.IsolateHttpsSitesProcessCountLowerBound",
      process_count_lower_bound[ISOLATE_HTTPS_SITES]);
  UMA_HISTOGRAM_COUNTS_100(
      "SiteIsolation.IsolateHttpsSitesProcessCountEstimate",
      process_count_estimate[ISOLATE_HTTPS_SITES]);
  UMA_HISTOGRAM_COUNTS_100(
      "SiteIsolation.IsolateHttpsSitesTotalProcessCountEstimate",
      process_count_estimate[ISOLATE_HTTPS_SITES] + non_renderer_process_count);

  // ISOLATE_EXTENSIONS metrics.
  UMA_HISTOGRAM_COUNTS_100("SiteIsolation.IsolateExtensionsProcessCountNoLimit",
                           num_isolated_site_instances[ISOLATE_EXTENSIONS]);
  UMA_HISTOGRAM_COUNTS_100(
      "SiteIsolation.IsolateExtensionsProcessCountLowerBound",
      process_count_lower_bound[ISOLATE_EXTENSIONS]);
  UMA_HISTOGRAM_COUNTS_100(
      "SiteIsolation.IsolateExtensionsProcessCountEstimate",
      process_count_estimate[ISOLATE_EXTENSIONS]);
  UMA_HISTOGRAM_COUNTS_100(
      "SiteIsolation.IsolateExtensionsTotalProcessCountEstimate",
      process_count_estimate[ISOLATE_EXTENSIONS] + non_renderer_process_count);
}
