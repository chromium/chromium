// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NAVIGATION_PREDICTOR_NAVIGATION_PREDICTOR_H_
#define CHROME_BROWSER_NAVIGATION_PREDICTOR_NAVIGATION_PREDICTOR_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "third_party/blink/public/mojom/loader/navigation_predictor.mojom.h"

namespace content {
class BrowserContext;
class RenderFrameHost;
}

class SiteEngagementService;

// This class gathers metrics of anchor elements from both renderer process
// and browser process. Then it uses these metrics to make predictions on what
// are the most likely anchor elements that the user will click.
class NavigationPredictor : public blink::mojom::AnchorElementMetricsHost {
 public:
  // |render_frame_host| is the host associated with the render frame. It is
  // used to retrieve metrics at the browser side.
  explicit NavigationPredictor(content::RenderFrameHost* render_frame_host);
  ~NavigationPredictor() override;

  // Create and bind NavigationPredictor.
  static void Create(mojo::InterfaceRequest<AnchorElementMetricsHost> request,
                     content::RenderFrameHost* render_frame_host);

 private:
  // Struct holding navigation score, rank and other info of the anchor element.
  // Used for look up when an anchor element is clicked.
  struct NavigationScore;

  // blink::mojom::AnchorElementMetricsHost:
  void ReportAnchorElementMetricsOnClick(
      blink::mojom::AnchorElementMetricsPtr metrics) override;
  void ReportAnchorElementMetricsOnLoad(
      std::vector<blink::mojom::AnchorElementMetricsPtr> metrics) override;

  // Returns true if the anchor element metric from the renderer process is
  // valid.
  bool IsValidMetricFromRenderer(
      const blink::mojom::AnchorElementMetrics& metric) const;

  // Returns site engagement service, which can be used to get site engagement
  // score. Return value is guaranteed to be non-null.
  SiteEngagementService* GetEngagementService() const;

  // Merge anchor element metrics that have the same target url (href).
  void MergeMetricsSameTargetUrl(
      std::vector<blink::mojom::AnchorElementMetricsPtr>* metrics) const;

  // Computes and stores document level metrics, including |number_of_anchors_|,
  // |document_engagement_score_|, etc.
  void ComputeDocumentMetricsOnLoad(
      const std::vector<blink::mojom::AnchorElementMetricsPtr>& metrics);

  // Given metrics of an anchor element from both renderer and browser process,
  // returns navigation score. Virtual for testing purposes.
  virtual double CalculateAnchorNavigationScore(
      const blink::mojom::AnchorElementMetrics& metrics,
      double document_engagement_score,
      double target_engagement_score,
      int area_rank,
      int number_of_anchors) const;

  // Given a vector of navigation scores sorted in descending order, decide what
  // action to take, or decide not to do anything. Example actions including
  // preresolve, preload, prerendering, etc.
  void MaybeTakeActionOnLoad(
      const std::vector<std::unique_ptr<NavigationScore>>&
          sorted_navigation_scores) const;

  // Record anchor element metrics on page load.
  void RecordMetricsOnLoad(
      const blink::mojom::AnchorElementMetrics& metric) const;

  // Record timing information when an anchor element is clicked.
  void RecordTimingOnClick();

  // Used to get keyed services.
  content::BrowserContext* const browser_context_;

  // Maps from target url (href) to navigation score.
  std::unordered_map<std::string, std::unique_ptr<NavigationScore>>
      navigation_scores_map_;

  // Total number of anchors that: href has the same host as the document,
  // contains image, inside an iframe, href incremented by 1 from document url.
  int number_of_anchors_same_host_ = 0;
  int number_of_anchors_contains_image_ = 0;
  int number_of_anchors_in_iframe_ = 0;
  int number_of_anchors_url_incremented_ = 0;

  // Scaling factors used to compute navigation scores.
  const int ratio_area_scale_;
  const int is_in_iframe_scale_;
  const int is_same_host_scale_;
  const int contains_image_scale_;
  const int is_url_incremented_scale_;
  const int source_engagement_score_scale_;
  const int target_engagement_score_scale_;
  const int area_rank_scale_;

  // Sum of all scales. Used to normalize the final computed weight.
  const int sum_scales_;

  // Timing of document loaded and last click.
  base::TimeTicks document_loaded_timing_;
  base::TimeTicks last_click_timing_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(NavigationPredictor);
};

#endif  // CHROME_BROWSER_NAVIGATION_PREDICTOR_NAVIGATION_PREDICTOR_H_
