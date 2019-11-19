// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_AMP_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_AMP_PAGE_LOAD_METRICS_OBSERVER_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "services/metrics/public/cpp/ukm_source.h"

namespace content {
class NavigationHandle;
}

// Observer responsible for recording metrics for AMP documents. This includes
// both AMP documents loaded in the main frame, and AMP documents loaded in a
// subframe (e.g. in an AMP viewer in the main frame).
//
// For AMP documents loaded in a subframe, recording works like so:
//
// * whenever the main frame URL gets updated with an AMP viewer
//   URL (e.g. https://www.google.com/amp/...), we track that in
//   OnCommitSameDocumentNavigation. we use the time of the main
//   frame URL update as the baseline time from which the
//   user-perceived performance metrics like FCP are computed.
//
// * whenever an iframe AMP document is loaded, we track that in
//   OnDidFinishSubFrameNavigation. we also keep track of the
//   performance timing metrics such as FCP reported in the frame.
//
// * we associate the main frame AMP navigation with its associated
//   AMP document in an iframe by comparing URLs between the main
//   frame and the iframe documents.
//
// * we record AMP metrics at the times when an AMP viewer URL is
//   navigated away from. when a user swipes to change the AMP
//   document, closes a tab, types a new URL in the URL bar, etc,
//   we will record AMP metrics for the AMP doc in an iframe that
//   the user is navigating away from.
class AMPPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  AMPPageLoadMetricsObserver();
  ~AMPPageLoadMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver:
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle,
                         ukm::SourceId source_id) override;
  void OnCommitSameDocumentNavigation(
      content::NavigationHandle* navigation_handle) override;
  void OnDidFinishSubFrameNavigation(
      content::NavigationHandle* navigation_handle) override;
  void OnFrameDeleted(content::RenderFrameHost* rfh) override;
  void OnTimingUpdate(
      content::RenderFrameHost* subframe_rfh,
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnSubFrameRenderDataUpdate(
      content::RenderFrameHost* subframe_rfh,
      const page_load_metrics::mojom::FrameRenderDataUpdate& render_data)
      override;
  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnLoadingBehaviorObserved(content::RenderFrameHost* subframe_rfh,
                                 int behavior_flags) override;

 private:
  // Information about an AMP navigation in the main frame. Both regular and
  // same document navigations are included.
  struct MainFrameNavigationInfo {
    GURL url;

    ukm::SourceId ukm_source_id = ukm::kInvalidSourceId;

    // Pointer to the RenderViewHost for the iframe hosting the AMP document
    // associated with the main frame AMP navigation.
    content::RenderFrameHost* subframe_rfh = nullptr;

    // Navigation start time for the main frame AMP navigation. We use this time
    // as an approximation of the time the user initiated the navigation.
    base::TimeTicks navigation_start;

    // Whether the main frame navigation is a same document navigation.
    bool is_same_document_navigation = false;
  };

  // Information about an AMP subframe.
  struct SubFrameInfo {
    SubFrameInfo();
    ~SubFrameInfo();

    // The AMP viewer URL of the currently committed AMP document. Used for
    // matching the MainFrameNavigationInfo url.
    GURL viewer_url;

    // The navigation start time of the non-same-document navigation in the AMP
    // iframe. Timestamps in |timing| below are relative to this value.
    base::TimeTicks navigation_start;

    // Performance metrics observed in the AMP iframe.
    page_load_metrics::mojom::PageLoadTimingPtr timing;
    page_load_metrics::PageRenderData render_data;

    // Whether an AMP document was loaded, based on observed
    // LoadingBehaviorFlags for this frame.
    bool amp_document_loaded = false;
  };

  void RecordLoadingBehaviorObserved();

  void ProcessMainFrameNavigation(content::NavigationHandle* navigation_handle);
  void MaybeRecordAmpDocumentMetrics();

  // Information about the currently active AMP navigation in the main
  // frame. Will be null if there isn't an active AMP navigation in the main
  // frame.
  std::unique_ptr<MainFrameNavigationInfo> current_main_frame_nav_info_;

  // Information about each active AMP iframe in the main document.
  std::map<content::RenderFrameHost*, SubFrameInfo> amp_subframe_info_;

  GURL current_url_;

  bool observed_amp_main_frame_ = false;
  bool observed_amp_sub_frame_ = false;

  DISALLOW_COPY_AND_ASSIGN(AMPPageLoadMetricsObserver);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_AMP_PAGE_LOAD_METRICS_OBSERVER_H_
