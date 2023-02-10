// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NAVIGATION_PREDICTOR_NAVIGATION_PREDICTOR_H_
#define CHROME_BROWSER_NAVIGATION_PREDICTOR_NAVIGATION_PREDICTOR_H_

#include <deque>
#include <set>
#include <unordered_map>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "content/public/browser/document_service.h"
#include "content/public/browser/visibility.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/mojom/loader/navigation_predictor.mojom.h"
#include "ui/gfx/geometry/size.h"
#include "url/origin.h"

namespace content {
class RenderFrameHost;
}  // namespace content

// This class gathers metrics of anchor elements from both renderer process
// and browser process. Then it uses these metrics to make predictions on what
// are the most likely anchor elements that the user will click.
//
// This class derives from WebContentsObserver so that it can keep track of when
// WebContents is being destroyed via web_contents().
class NavigationPredictor
    : public content::DocumentService<blink::mojom::AnchorElementMetricsHost> {
 public:
  NavigationPredictor(const NavigationPredictor&) = delete;
  NavigationPredictor& operator=(const NavigationPredictor&) = delete;

  // Create and bind NavigationPredictor.
  static void Create(content::RenderFrameHost* render_frame_host,
                     mojo::PendingReceiver<AnchorElementMetricsHost> receiver);

 private:
  friend class MockNavigationPredictorForTesting;
  using AnchorId = base::StrongAlias<class AnchorId, uint32_t>;
  // This structure holds the user interactions with a given anchor element.
  // Whenever, the user clicks on a link, we iterate over all |UserInteractions|
  // data and check if the anchor element is still in viewport or not. If it is
  // still in viewport, we use |last_navigation_start_to_entered_viewport| and
  // |navigation_start_to_click_| to update |max_time_in_viewport|.  Similarly,
  // we also check if the pointer is still hovering over the anchor element, and
  // use |last_navigation_start_to_pointer_over| and
  // |navigation_start_to_click_| to update |max_hover_dwell_time|. We then
  // record |max_time_in_viewport|, and |max_hover_dwell_time| to UKM.
  struct UserInteractions {
    // True if the anchor element is still in viewport, otherwise false.
    bool is_in_viewport = false;
    // True if the pointer is still hovering over the anchor element, otherwise
    // false;
    bool is_hovered = false;
    // If the anchor element is still in viewport, it is the TimeDelta between
    // the navigation start of the anchor element's root document and the last
    // time the anchor element entered the viewport, otherwise empty.
    absl::optional<base::TimeDelta> last_navigation_start_to_entered_viewport;
    // The maximum duration that the anchor element was in the viewport.
    absl::optional<base::TimeDelta> max_time_in_viewport;
    // If the anchor element is still in viewport, it is the TimeDelta between
    // the navigation start of the anchor element's root document and the last
    // time the pointer started to hover over the anchor element, otherwise
    // empty.
    absl::optional<base::TimeDelta> last_navigation_start_to_pointer_over;
    // The maximum the pointer hover dwell time over the anchor element.
    absl::optional<base::TimeDelta> max_hover_dwell_time;
  };

  NavigationPredictor(content::RenderFrameHost& render_frame_host,
                      mojo::PendingReceiver<AnchorElementMetricsHost> receiver);
  ~NavigationPredictor() override;

  // blink::mojom::AnchorElementMetricsHost:
  void ReportAnchorElementClick(
      blink::mojom::AnchorElementClickPtr click) override;
  void ReportAnchorElementsEnteredViewport(
      std::vector<blink::mojom::AnchorElementEnteredViewportPtr> elements)
      override;
  void ReportAnchorElementsLeftViewport(
      std::vector<blink::mojom::AnchorElementLeftViewportPtr> elements)
      override;
  void ReportAnchorElementPointerOver(
      blink::mojom::AnchorElementPointerOverPtr pointer_over_event) override;
  void ReportAnchorElementPointerOut(
      blink::mojom::AnchorElementPointerOutPtr hover_event) override;
  void ReportNewAnchorElements(
      std::vector<blink::mojom::AnchorElementMetricsPtr> elements) override;

  // Computes and stores document level metrics, including |number_of_anchors_|
  // etc.
  void ComputeDocumentMetricsOnLoad(
      const std::vector<blink::mojom::AnchorElementMetricsPtr>& metrics);

  // Record anchor element metrics on page load.
  void RecordMetricsOnLoad(
      const blink::mojom::AnchorElementMetrics& metric) const;

  // Returns the minimum of the bucket that |value| belongs in, for page-wide
  // metrics, excluding |median_link_location_|.
  int GetBucketMinForPageMetrics(int value) const;

  // Returns the minimum of the bucket that |value| belongs in, used for
  // |median_link_location_| and the |ratio_distance_root_top|.
  int GetLinearBucketForLinkLocation(int value) const;

  // Returns the minimum of the bucket that |value| belongs in, used for
  // |ratio_area|.
  int GetLinearBucketForRatioArea(int value) const;

  // A count of clicks to prevent reporting more than 10 clicks to UKM.
  size_t clicked_count_ = 0;

  // Stores the anchor element metrics for each anchor ID that we track.
  std::unordered_map<AnchorId,
                     blink::mojom::AnchorElementMetricsPtr,
                     typename AnchorId::Hasher>
      anchors_;

  // User interaction data for anchor ID that we track.
  std::unordered_map<AnchorId, UserInteractions, typename AnchorId::Hasher>
      user_interactions_;

  // The time between navigation start and the last time user clicked on a link.
  absl::optional<base::TimeDelta> navigation_start_to_click_;

  // Mapping between the anchor ID for the anchors that we track and the index
  // that this anchor will have in the UKM logs.
  std::unordered_map<AnchorId, int, typename AnchorId::Hasher>
      tracked_anchor_id_to_index_;

  // URLs that were sent to the prediction service.
  std::set<GURL> predicted_urls_;

  // UKM ID for navigation
  ukm::SourceId ukm_source_id_;

  // UKM recorder
  raw_ptr<ukm::UkmRecorder> ukm_recorder_ = nullptr;

  // The time at which the navigation started.
  base::TimeTicks navigation_start_;

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // CHROME_BROWSER_NAVIGATION_PREDICTOR_NAVIGATION_PREDICTOR_H_
