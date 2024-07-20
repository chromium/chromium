// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NAVIGATION_PREDICTOR_NAVIGATION_PREDICTOR_H_
#define CHROME_BROWSER_NAVIGATION_PREDICTOR_NAVIGATION_PREDICTOR_H_

#include <set>
#include <unordered_map>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "chrome/browser/navigation_predictor/preloading_model_keyed_service.h"
#include "chrome/browser/page_load_metrics/observers/page_anchors_metrics_observer.h"
#include "content/public/browser/document_service.h"
#include "content/public/browser/visibility.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "navigation_predictor_metrics_document_data.h"
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
  using ModelScoreCallbackForTesting = base::OnceCallback<void(
      const PreloadingModelKeyedService::Inputs& inputs)>;

  // These values are persisted to logs.
  enum FontSizeBucket : uint8_t {
    kLessThanTen = 1,
    kTenToSeventeen = 2,
    kEighteenOrGreater = 3,
  };

  NavigationPredictor(const NavigationPredictor&) = delete;
  NavigationPredictor& operator=(const NavigationPredictor&) = delete;

  // Create and bind NavigationPredictor.
  static void Create(content::RenderFrameHost* render_frame_host,
                     mojo::PendingReceiver<AnchorElementMetricsHost> receiver);

  void SetModelScoreCallbackForTesting(ModelScoreCallbackForTesting callback);

  void SetTaskRunnerForTesting(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      const base::TickClock* clock);

  static void DisableRendererMetricSendingDelayForTesting();

 private:
  friend class MockNavigationPredictorForTesting;
  using AnchorId = base::StrongAlias<class AnchorId, uint32_t>;
  struct AnchorElementData;

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
  void ReportAnchorElementsPositionUpdate(
      std::vector<blink::mojom::AnchorElementPositionUpdatePtr> elements)
      override;
  void ReportAnchorElementPointerDataOnHoverTimerFired(
      blink::mojom::AnchorElementPointerDataOnHoverTimerFiredPtr pointer_data)
      override;
  void ReportAnchorElementPointerOver(
      blink::mojom::AnchorElementPointerOverPtr pointer_over_event) override;
  void ReportAnchorElementPointerOut(
      blink::mojom::AnchorElementPointerOutPtr hover_event) override;
  void ReportAnchorElementPointerDown(
      blink::mojom::AnchorElementPointerDownPtr pointer_down_event) override;
  void ReportNewAnchorElements(
      std::vector<blink::mojom::AnchorElementMetricsPtr> elements,
      const std::vector<uint32_t>& removed_elements) override;
  void ProcessPointerEventUsingMLModel(
      blink::mojom::AnchorElementPointerEventForMLModelPtr pointer_event)
      override;
  void ShouldSkipUpdateDelays(ShouldSkipUpdateDelaysCallback callback) override;

  void OnMLModelExecutionTimerFired();

  // Computes and stores document level metrics, including |number_of_anchors_|
  // etc.
  void ComputeDocumentMetricsOnLoad(
      const std::vector<blink::mojom::AnchorElementMetricsPtr>& metrics);

  // Record anchor element metrics on page load.
  void RecordMetricsOnLoad(
      const blink::mojom::AnchorElementMetrics& metric) const;

  // Returns `NavigationPredictorMetricsDocumentData` for the current page.
  NavigationPredictorMetricsDocumentData&
  GetNavigationPredictorMetricsDocumentData() const;

  // Called when the async preloading heuristics model is done running and the
  // returned the result.
  virtual void OnPreloadingHeuristicsModelDone(
      GURL url,
      PreloadingModelKeyedService::Result result);

  bool IsTargetURLTheSameAsDocument(const AnchorElementData& anchor);

  base::TimeTicks NowTicks() const { return clock_->NowTicks(); }

  // A count of clicks to prevent reporting more than 10 clicks to UKM.
  size_t clicked_count_ = 0;

  // Stores the anchor element metrics for each anchor ID that we track.
  struct AnchorElementData {
    AnchorElementData(blink::mojom::AnchorElementMetricsPtr metrics,
                      base::TimeTicks first_report_timestamp);
    ~AnchorElementData();

    // The following fields mirror `blink::mojom::AnchorElementMetrics`, but
    // unused fields are omitted and fields are arranged to minimize memory
    // usage.
    float ratio_distance_root_top;
    // `ratio_area` is [0,100], so we can store this more compactly than with a
    // float.
    uint8_t ratio_area;
    bool is_in_iframe : 1;
    bool contains_image : 1;
    bool is_same_host : 1;
    bool is_url_incremented_by_one : 1;
    bool has_text_sibling : 1;
    bool is_bold_font : 1;
    FontSizeBucket font_size;
    GURL target_url;

    // Following fields are used for computing timing inputs of the ML model.
    base::TimeTicks first_report_timestamp;
    std::optional<base::TimeTicks> pointer_over_timestamp;
    size_t pointer_hovering_over_count = 0u;
  };
  std::unordered_map<AnchorId, AnchorElementData, typename AnchorId::Hasher>
      anchors_;
  // It is the anchor element that the user has recently interacted
  // with and is a good candidate for the ML model to predict the next user
  // click.
  std::optional<AnchorId> ml_model_candidate_;

  // The time between navigation start and the last time user clicked on a link.
  std::optional<base::TimeDelta> navigation_start_to_click_;

  // Mapping between the anchor ID for the anchors that we track and the index
  // that this anchor will have in the UKM logs.
  std::unordered_map<AnchorId, int, typename AnchorId::Hasher>
      tracked_anchor_id_to_index_;

  // URLs that were sent to the prediction service.
  // We store hashes of URLs, rather than URLs themselves, to save memory.
  // TODO(mcnee): Would it be better to use a bloom filter to have a limit on
  // the memory usage needed for this?
  std::set<size_t> predicted_urls_;

  // UKM ID for navigation
  ukm::SourceId ukm_source_id_;

  // UKM recorder
  raw_ptr<ukm::UkmRecorder> ukm_recorder_ = nullptr;

  // The time at which the navigation started.
  base::TimeTicks navigation_start_;

  // Used to cancel ML model execution requests sent to
  // `PreloadingModelKeyedService`.
  base::CancelableTaskTracker scoring_model_task_tracker_;

  raw_ptr<const base::TickClock> clock_;

  static bool disable_renderer_metric_sending_delay_for_testing_;

  base::OneShotTimer ml_model_execution_timer_;

  ModelScoreCallbackForTesting model_score_callback_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<NavigationPredictor> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_NAVIGATION_PREDICTOR_NAVIGATION_PREDICTOR_H_
