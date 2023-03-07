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
  class UserInteractionsData
      : public content::WebContentsUserData<UserInteractionsData> {
   public:
    UserInteractionsData(const UserInteractionsData&) = delete;
    ~UserInteractionsData() override;
    UserInteractionsData& operator=(const UserInteractionsData&) = delete;

    // This structure holds the user interactions with a given anchor element.
    // Whenever, the user clicks on a link, we iterate over all
    // |UserInteractions| data and check if the anchor element is still in
    // viewport or not. If it is still in viewport, we use
    // |last_navigation_start_to_entered_viewport| and
    // |navigation_start_to_click_| to update |max_time_in_viewport|. Similarly,
    // we also check if the pointer is still hovering over the anchor element,
    // and use |last_navigation_start_to_pointer_over| and
    // |navigation_start_to_click_| to update |max_hover_dwell_time|. We then
    // record |max_time_in_viewport|, and |max_hover_dwell_time| to UKM.
    struct UserInteractions {
      // True if the anchor element is still in viewport, otherwise false.
      bool is_in_viewport = false;
      // True if the pointer is still hovering over the anchor element,
      // otherwise false;
      bool is_hovered = false;
      // Number of times the pointer was hovering over the anchor element.
      int pointer_hovering_over_count = 0;
      // If the anchor element is still in viewport, it is the TimeDelta between
      // the navigation start of the anchor element's root document and the last
      // time the anchor element entered the viewport, otherwise empty.
      absl::optional<base::TimeDelta> last_navigation_start_to_entered_viewport;
      // The maximum duration that the anchor element was in the viewport.
      absl::optional<base::TimeDelta> max_time_in_viewport;
      // TimeDelta between the navigation start of the anchor element's root
      // document and the last time the pointer started to hover over the anchor
      // element, otherwise empty.
      absl::optional<base::TimeDelta> last_navigation_start_to_pointer_over;
      // TimeDelta between the navigation start of the anchor element's root
      // document and the last time the pointer down event happened over the
      // anchor element, otherwise empty.
      absl::optional<base::TimeDelta>
          last_navigation_start_to_last_pointer_down;
      // The maximum the pointer hover dwell time over the anchor element.
      absl::optional<base::TimeDelta> max_hover_dwell_time;
    };

    // Records user interaction metrics to UKM.
    void RecordUserInteractionMetrics(
        ukm::SourceId ukm_source_id,
        absl::optional<base::TimeDelta> navigation_start_to_now);

    // User interaction data for the tracked anchor index.
    std::unordered_map<int, UserInteractions> user_interactions_;

    // The time between navigation start and the last time user clicked on a
    // link.
    absl::optional<base::TimeDelta> navigation_start_to_click_;

   private:
    friend class content::WebContentsUserData<UserInteractionsData>;
    explicit UserInteractionsData(content::WebContents* contents);
    WEB_CONTENTS_USER_DATA_KEY_DECL();
  };

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
  void RecordAnchorDataToUkm();
  void RecordUserInteractionDataToUkm();

  bool is_in_prerendered_page_ = false;

  raw_ptr<content::WebContents> web_contents_;
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_PAGE_ANCHORS_METRICS_OBSERVER_H_
