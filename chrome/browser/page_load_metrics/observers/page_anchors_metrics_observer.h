// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_PAGE_ANCHORS_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_PAGE_ANCHORS_METRICS_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/render_frame_host.h"

// Tracks anchor information in content::NavigationPredictor to report gathered
// data on navigating out the page. Ideally this should be managed by
// per outermost page manner. However we ensure that this structure is not
// created and accessed during prerendering as we have a DCHECK in
// content::NavigationPredictor::ReportNewAnchorElements. So, we can manage it
// as per WebContents without polluting gathered data.
class PageAnchorsMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  PageAnchorsMetricsObserver(const PageAnchorsMetricsObserver&) = delete;
  explicit PageAnchorsMetricsObserver(content::WebContents* web_contents) {}
  PageAnchorsMetricsObserver& operator=(const PageAnchorsMetricsObserver&) =
      delete;

  // page_load_metrics::PageLoadMetricsObserver:
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;
  page_load_metrics::PageLoadMetricsObserver::ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  page_load_metrics::PageLoadMetricsObserver::ObservePolicy
  FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void DidActivatePrerenderedPage(
      content::NavigationHandle* navigation_handle) override;
  page_load_metrics::PageLoadMetricsObserver::ObservePolicy OnCommit(
      content::NavigationHandle* navigation_handle) override;
  void OnRenderFrameDeleted(content::RenderFrameHost* rfh) override;
  page_load_metrics::PageLoadMetricsObserver::ObservePolicy
  OnEnterBackForwardCache(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnRestoreFromBackForwardCache(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      content::NavigationHandle* navigation_handle) override;

 private:
  void UpdateRenderFrameHostAndSourceId(
      content::NavigationHandle* navigation_handle);

  void RecordDataToUkm();
  void RecordAnchorElementMetricsDataToUkm();

  content::RenderFrameHost* render_frame_host() const {
    return render_frame_host_id_.has_value()
               ? content::RenderFrameHost::FromID(render_frame_host_id_.value())
               : nullptr;
  }

  bool is_in_prerendered_page_ = false;

  std::optional<content::GlobalRenderFrameHostId> render_frame_host_id_;
  ukm::SourceId ukm_source_id_;
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_PAGE_ANCHORS_METRICS_OBSERVER_H_
