// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_PAGE_ANCHORS_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_PAGE_ANCHORS_METRICS_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "content/public/browser/web_contents_user_data.h"

// Tracks anchor information in content::NavigationPredictor to report gathered
// data on navigating out the page. Ideally this should be managed by
// per outermost page manner. However we ensure that this structure is not
// created and accessed during prerendering as we have a DCHECK in
// content::NavigationPredictor::ReportNewAnchorElements. So, we can manage it
// as per WebContents without polluting gathered data.
class PageAnchorsMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  class AnchorsData : public content::WebContentsUserData<AnchorsData> {
   public:
    AnchorsData(const AnchorsData&) = delete;
    ~AnchorsData() override;
    AnchorsData& operator=(const AnchorsData&) = delete;

    int MedianLinkLocation();

    void Clear();

    size_t number_of_anchors_same_host_ = 0;
    size_t number_of_anchors_contains_image_ = 0;
    size_t number_of_anchors_in_iframe_ = 0;
    size_t number_of_anchors_url_incremented_ = 0;
    size_t number_of_anchors_ = 0;
    int total_clickable_space_ = 0;
    int viewport_height_ = 0;
    int viewport_width_ = 0;
    std::vector<int> link_locations_;

   private:
    friend class content::WebContentsUserData<AnchorsData>;
    explicit AnchorsData(content::WebContents* contents);
    WEB_CONTENTS_USER_DATA_KEY_DECL();
  };

  PageAnchorsMetricsObserver(const PageAnchorsMetricsObserver&) = delete;
  explicit PageAnchorsMetricsObserver(content::WebContents* web_contents)
      : web_contents_(web_contents) {}
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

 private:
  void RecordUkm();

  bool is_in_prerendered_page_ = false;

  raw_ptr<content::WebContents> web_contents_;
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_PAGE_ANCHORS_METRICS_OBSERVER_H_
