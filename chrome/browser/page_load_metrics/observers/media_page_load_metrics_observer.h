// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_MEDIA_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_MEDIA_PAGE_LOAD_METRICS_OBSERVER_H_

#include <stdint.h>

#include "base/macros.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "content/public/browser/web_contents_observer.h"

// Observer responsible for recording metrics on pages that play at least one
// MEDIA request.
class MediaPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  MediaPageLoadMetricsObserver();
  ~MediaPageLoadMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver:
  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  page_load_metrics::PageLoadMetricsObserver::ObservePolicy
  FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnResourceDataUseObserved(
      content::RenderFrameHost* rfh,
      const std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr>&
          resources) override;
  void MediaStartedPlaying(
      const content::WebContentsObserver::MediaPlayerInfo& video_type,
      content::RenderFrameHost* render_frame_host) override;

 private:
  // Records histograms for byte information.
  void RecordByteHistograms();

  // The number of body (not header) prefilter bytes consumed by requests for
  // the page.
  int64_t cache_bytes_;
  int64_t network_bytes_;

  // Whether the page load played a media element.
  bool played_media_;

  DISALLOW_COPY_AND_ASSIGN(MediaPageLoadMetricsObserver);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_MEDIA_PAGE_LOAD_METRICS_OBSERVER_H_
