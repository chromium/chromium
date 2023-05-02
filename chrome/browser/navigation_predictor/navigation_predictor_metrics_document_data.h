// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NAVIGATION_PREDICTOR_NAVIGATION_PREDICTOR_METRICS_DOCUMENT_DATA_H_
#define CHROME_BROWSER_NAVIGATION_PREDICTOR_NAVIGATION_PREDICTOR_METRICS_DOCUMENT_DATA_H_

#include "base/time/time.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/render_frame_host.h"

class NavigationPredictorMetricsDocumentData
    : public content::DocumentUserData<NavigationPredictorMetricsDocumentData> {
 public:
  struct AnchorsData {
    AnchorsData();
    ~AnchorsData();

    int MedianLinkLocation();

    size_t number_of_anchors_same_host_ = 0;
    size_t number_of_anchors_contains_image_ = 0;
    size_t number_of_anchors_in_iframe_ = 0;
    size_t number_of_anchors_url_incremented_ = 0;
    size_t number_of_anchors_ = 0;
    int total_clickable_space_ = 0;
    int viewport_height_ = 0;
    int viewport_width_ = 0;
    std::vector<int> link_locations_;
  };
  struct PageLinkClickData {
    int anchor_element_index_;
    absl::optional<bool> href_unchanged_;
    base::TimeDelta navigation_start_to_link_clicked_;
  };
  struct AnchorElementMetricsData {
    AnchorElementMetricsData();
    AnchorElementMetricsData(AnchorElementMetricsData&&);

    AnchorElementMetricsData& operator=(AnchorElementMetricsData&&);

    size_t bucketed_path_hash_;
    bool contains_image_;
    size_t font_size_;
    bool has_text_sibling_;
    bool is_bold_;
    bool is_in_iframe_;
    bool is_url_incremented_by_one_;
    base::TimeDelta navigation_start_to_link_logged;
    size_t path_depth_;
    size_t path_length_;
    int percent_clickable_area_;
    int percent_vertical_distance_;
    bool is_same_origin_;
  };

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
  struct UserInteractionsData {
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
    absl::optional<base::TimeDelta> last_navigation_start_to_last_pointer_down;
    // The maximum the pointer hover dwell time over the anchor element.
    absl::optional<base::TimeDelta> max_hover_dwell_time;
  };

  struct PreloadOnHoverData {
    bool taken = false;
    absl::optional<base::TimeDelta> hover_dwell_time;
    absl::optional<base::TimeDelta> pointer_down_duration;
  };

  NavigationPredictorMetricsDocumentData(
      const NavigationPredictorMetricsDocumentData&) = delete;
  NavigationPredictorMetricsDocumentData& operator=(
      const NavigationPredictorMetricsDocumentData&) = delete;
  ~NavigationPredictorMetricsDocumentData() override;

  void SetUkmSourceId(ukm::SourceId ukm_source_id) {
    ukm_source_id_ = ukm_source_id;
  }
  void ResetUkmSourceId() { ukm_source_id_.reset(); }

  AnchorsData& GetAnchorsData() { return anchor_data_; }
  void RecordAnchorData(ukm::SourceId ukm_source_id);

  void AddPageLinkClickData(PageLinkClickData data);
  void ClearPageLinkClickData();
  void RecordPageLinkClickData(ukm::SourceId ukm_source_id);

  void AddAnchorElementMetricsData(int anchor_index,
                                   AnchorElementMetricsData data);
  void RecordAnchorElementMetricsData(ukm::SourceId ukm_source_id);

  void AddUserInteractionsData(int anchor_index, UserInteractionsData data);
  void SetNavigationStartToClick(
      const base::TimeDelta& navigation_start_to_click) {
    navigation_start_to_click_ = navigation_start_to_click;
  }
  void SetNavigationStartTime(const base::TimeTicks& navigation_start_time) {
    navigation_start_time_ = navigation_start_time;
  }
  void RecordUserInteractionsData(ukm::SourceId ukm_source_id);
  void ClearUserInteractionsData();
  std::unordered_map<int, UserInteractionsData>& GetUserInteractionsData() {
    return user_interactions_;
  }

  void AddPreloadOnHoverData(PreloadOnHoverData data);
  void RecordPreloadOnHoverData(ukm::SourceId ukm_source_id);

  void RecordDataToUkm(ukm::SourceId ukm_source_id);

 private:
  friend class content::DocumentUserData<
      NavigationPredictorMetricsDocumentData>;
  explicit NavigationPredictorMetricsDocumentData(
      content::RenderFrameHost* render_frame_host);

  void RecordAnchorElementMetricsData(int anchor_index,
                                      const AnchorElementMetricsData& data);

  // TODO(isaboori): Right now we keep track of UKM source ID here as a member
  // variable and we also receive it as an argument in Record.*() methods. It is
  // to make sure that `NavigationPredictorMetricsDocumentData` and
  // 'PageAnchorMetricsObserver` are not getting out of sync. In future, we
  // should remove the `ukm_source_id` from the methods' arguments.
  absl::optional<ukm::SourceId> ukm_source_id_;
  AnchorsData anchor_data_;
  std::vector<PageLinkClickData> page_link_clicks_;
  std::map<int, AnchorElementMetricsData> anchor_element_metrics_;
  std::unordered_map<int, UserInteractionsData> user_interactions_;
  std::vector<PreloadOnHoverData> preload_on_hover_;
  // The time between navigation start and the last time user clicked on a
  // link.
  absl::optional<base::TimeDelta> navigation_start_to_click_;
  base::TimeTicks navigation_start_time_;
  DOCUMENT_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_NAVIGATION_PREDICTOR_NAVIGATION_PREDICTOR_METRICS_DOCUMENT_DATA_H_
