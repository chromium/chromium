// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SITE_ISOLATION_SITE_DETAILS_H_
#define CHROME_BROWSER_SITE_ISOLATION_SITE_DETAILS_H_

#include <stdint.h>

#include <map>
#include <set>

#include "base/memory/raw_ptr.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/site_instance.h"

namespace content {
class Page;
}  // namespace content

// Collects metrics about an actual browsing instance in the current session.
struct BrowsingInstanceInfo {
  BrowsingInstanceInfo();
  BrowsingInstanceInfo(const BrowsingInstanceInfo& other);
  ~BrowsingInstanceInfo();

  std::set<raw_ptr<content::SiteInstance, SetExperimental>> site_instances;
  int proxy_count = 0;
};
using BrowsingInstanceMap =
    std::map<content::SiteInstance*, BrowsingInstanceInfo>;

// Information about the sites and SiteInstances in each BrowsingInstance, for
// use in estimating the number of processes needed for various process models.
struct SiteData {
  SiteData();
  SiteData(const SiteData& other);
  ~SiteData();

  // This map groups related SiteInstances together into BrowsingInstances. The
  // first SiteInstance we see in a BrowsingInstance is designated as the
  // 'primary' SiteInstance, and becomes the key of this map.
  BrowsingInstanceMap browsing_instances;

  // A count of all RenderFrameHosts, which are in a different SiteInstance from
  // their parents. This does not include guestviews or fencedframes.
  int out_of_process_frames = 0;

  // A count of all inner frame trees which are in a different RenderProcessHost
  // from their parents.
  int out_of_process_inner_frame_trees = 0;

  // Estimated increase in process count due to OriginAgentCluster (OAC)
  // SiteInstances over all elements in |browsing_instances|.
  int oac_overhead;
};

// Maps a BrowserContext to information about the sites it contains.
typedef std::map<content::BrowserContext*, SiteData> BrowserContextSiteDataMap;

class SiteDetails {
 public:
  SiteDetails(const SiteDetails&) = delete;
  SiteDetails& operator=(const SiteDetails&) = delete;

  // Collect information about all committed sites in the given Page
  // on the UI thread.
  static void CollectSiteInfo(content::Page& page, SiteData* site_data);

  // Collect count of OriginAgentCluster SiteInstances, and compare to what we
  // would expect with OAC off (and no coalescing different BrowsingInstances
  // into shared RenderProcesses).
  static int EstimateOriginAgentClusterOverhead(const SiteData& site_data);

  // Updates the global histograms for tracking memory usage.
  static void UpdateHistograms(const BrowserContextSiteDataMap& site_data_map,
                               size_t live_process_count);

 private:
  // Only static methods - never needs to be constructed.
  SiteDetails() = delete;
  ~SiteDetails() = delete;
};

#endif  // CHROME_BROWSER_SITE_ISOLATION_SITE_DETAILS_H_
