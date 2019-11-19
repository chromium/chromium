// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/tab_restore_page_load_metrics_observer.h"

#include <string>

#include "base/metrics/histogram_macros.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/restore_type.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/page_transition_types.h"

namespace {

const char kHistogramTabRestoreNetworkBytes[] =
    "PageLoad.Clients.TabRestore.Experimental.Bytes.Network";
const char kHistogramTabRestoreCacheBytes[] =
    "PageLoad.Clients.TabRestore.Experimental.Bytes.Cache";
const char kHistogramTabRestoreTotalBytes[] =
    "PageLoad.Clients.TabRestore.Experimental.Bytes.Total";

}  // namespace

TabRestorePageLoadMetricsObserver::TabRestorePageLoadMetricsObserver()
    : cache_bytes_(0), network_bytes_(0) {}

TabRestorePageLoadMetricsObserver::~TabRestorePageLoadMetricsObserver() {}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
TabRestorePageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  return IsTabRestore(navigation_handle) ? CONTINUE_OBSERVING : STOP_OBSERVING;
}

void TabRestorePageLoadMetricsObserver::OnResourceDataUseObserved(
    content::RenderFrameHost* rfh,
    const std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr>&
        resources) {
  for (auto const& resource : resources) {
    if (resource->is_complete) {
      if (resource->cache_type ==
          page_load_metrics::mojom::CacheType::kNotCached)
        network_bytes_ += resource->encoded_body_length;
      else
        cache_bytes_ += resource->encoded_body_length;
    }
  }
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
TabRestorePageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  // FlushMetricsOnAppEnterBackground is invoked on Android in cases where the
  // app is about to be backgrounded, as part of the Activity.onPause()
  // flow. After this method is invoked, Chrome may be killed without further
  // notification, so we record final metrics collected up to this point.
  if (GetDelegate().DidCommit()) {
    RecordByteHistograms();
  }
  return STOP_OBSERVING;
}

void TabRestorePageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  RecordByteHistograms();
}

void TabRestorePageLoadMetricsObserver::RecordByteHistograms() {
  PAGE_BYTES_HISTOGRAM(kHistogramTabRestoreNetworkBytes, network_bytes_);
  PAGE_BYTES_HISTOGRAM(kHistogramTabRestoreCacheBytes, cache_bytes_);
  PAGE_BYTES_HISTOGRAM(kHistogramTabRestoreTotalBytes,
                       network_bytes_ + cache_bytes_);
}

bool TabRestorePageLoadMetricsObserver::IsTabRestore(
    content::NavigationHandle* navigation_handle) {
  // Only count restored tabs, and eliminate forward-back navigations, as
  // restored tab history is considered a restored navigation until they are
  // loaded the first time.
  return navigation_handle->GetRestoreType() != content::RestoreType::NONE &&
         !(navigation_handle->GetPageTransition() &
           ui::PAGE_TRANSITION_FORWARD_BACK);
}
