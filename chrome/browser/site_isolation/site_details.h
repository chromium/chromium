// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SITE_ISOLATION_SITE_DETAILS_H_
#define CHROME_BROWSER_SITE_ISOLATION_SITE_DETAILS_H_

#include <stdint.h>

#include <map>
#include <set>

#include "base/macros.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"

// Collects metrics about an actual browsing instance in the current session.
struct BrowsingInstanceInfo {
  BrowsingInstanceInfo();
  BrowsingInstanceInfo(const BrowsingInstanceInfo& other);
  ~BrowsingInstanceInfo();

  std::set<content::SiteInstance*> site_instances;
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
  // their parents.
  int out_of_process_frames = 0;
};

// Maps a BrowserContext to information about the sites it contains.
typedef std::map<content::BrowserContext*, SiteData> BrowserContextSiteDataMap;

class SiteDetails {
 public:
  // Collect information about all committed sites in the given WebContents
  // on the UI thread.
  static void CollectSiteInfo(content::WebContents* contents,
                              SiteData* site_data);

  // Updates the global histograms for tracking memory usage.
  static void UpdateHistograms(const BrowserContextSiteDataMap& site_data_map,
                               int all_renderer_process_count,
                               int non_renderer_process_count);

 private:
  // Only static methods - never needs to be constructed.
  SiteDetails() = delete;
  ~SiteDetails() = delete;

  DISALLOW_COPY_AND_ASSIGN(SiteDetails);
};

#endif  // CHROME_BROWSER_SITE_ISOLATION_SITE_DETAILS_H_
