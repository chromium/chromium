// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/media_page_load_metrics_observer.h"

#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/common/page_load_timing.h"

namespace {

const char kHistogramMediaPageLoadNetworkBytes[] =
    "PageLoad.Clients.MediaPageLoad2.Experimental.Bytes.Network";
const char kHistogramMediaPageLoadCacheBytes[] =
    "PageLoad.Clients.MediaPageLoad2.Experimental.Bytes.Cache2";
const char kHistogramMediaPageLoadTotalBytes[] =
    "PageLoad.Clients.MediaPageLoad2.Experimental.Bytes.Total2";

}  // namespace

MediaPageLoadMetricsObserver::MediaPageLoadMetricsObserver()
    : cache_bytes_(0), network_bytes_(0), played_media_(false) {}

MediaPageLoadMetricsObserver::~MediaPageLoadMetricsObserver() = default;

const char* MediaPageLoadMetricsObserver::GetObserverName() const {
  static const char kName[] = "MediaPageLoadMetricsObserver";
  return kName;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
MediaPageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // This class needs forwarding for the events MediaStartedPlaying.
  return FORWARD_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
MediaPageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // Records if prerendered page is activated.
  return CONTINUE_OBSERVING;
}

void MediaPageLoadMetricsObserver::OnResourceDataUseObserved(
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
MediaPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  // FlushMetricsOnAppEnterBackground is invoked on Android in cases where the
  // app is about to be backgrounded, as part of the Activity.onPause()
  // flow. After this method is invoked, Chrome may be killed without further
  // notification, so we record final metrics collected up to this point.
  if (!GetDelegate().DidCommit())
    return STOP_OBSERVING;
  if (GetDelegate().GetPrerenderingState() ==
      page_load_metrics::PrerenderingState::kInPrerendering)
    return STOP_OBSERVING;
  if (!played_media_)
    return STOP_OBSERVING;

  RecordByteHistograms();
  return STOP_OBSERVING;
}

void MediaPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (GetDelegate().GetPrerenderingState() ==
      page_load_metrics::PrerenderingState::kInPrerendering)
    return;
  if (!played_media_)
    return;
  RecordByteHistograms();
}

void MediaPageLoadMetricsObserver::MediaStartedPlaying(
    const content::WebContentsObserver::MediaPlayerInfo& video_type,
    content::RenderFrameHost* render_frame_host) {
  if (played_media_)
    return;
  // Track media (audio or video) in all frames of the page load.
  played_media_ = true;
}

void MediaPageLoadMetricsObserver::RecordByteHistograms() {
  DCHECK_NE(GetDelegate().GetPrerenderingState(),
            page_load_metrics::PrerenderingState::kInPrerendering);
  DCHECK(played_media_);

  PAGE_BYTES_HISTOGRAM(kHistogramMediaPageLoadNetworkBytes, network_bytes_);
  PAGE_BYTES_HISTOGRAM(kHistogramMediaPageLoadCacheBytes, cache_bytes_);
  PAGE_BYTES_HISTOGRAM(kHistogramMediaPageLoadTotalBytes,
                       network_bytes_ + cache_bytes_);
}
