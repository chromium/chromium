// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NAVIGATION_PREDICTOR_NAVIGATION_PREDICTOR_H_
#define CHROME_BROWSER_NAVIGATION_PREDICTOR_NAVIGATION_PREDICTOR_H_

#include <deque>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/browser/prerender/prerender_handle.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/mojom/loader/navigation_predictor.mojom.h"
#include "ui/gfx/geometry/size.h"
#include "url/origin.h"

namespace content {
class BrowserContext;
class NavigationHandle;
class RenderFrameHost;
}

namespace prerender {
class PrerenderManager;
}

class TemplateURLService;

// This class gathers metrics of anchor elements from both renderer process
// and browser process. Then it uses these metrics to make predictions on what
// are the most likely anchor elements that the user will click.
class NavigationPredictor : public blink::mojom::AnchorElementMetricsHost,
                            public content::WebContentsObserver,
                            public prerender::PrerenderHandle::Observer {
 public:
  // |render_frame_host| is the host associated with the render frame. It is
  // used to retrieve metrics at the browser side.
  explicit NavigationPredictor(content::RenderFrameHost* render_frame_host);
  ~NavigationPredictor() override;

  // Create and bind NavigationPredictor.
  static void Create(content::RenderFrameHost* render_frame_host,
                     mojo::PendingReceiver<AnchorElementMetricsHost> receiver);

  // Enum describing the possible set of actions that navigation predictor may
  // take. This enum should remain synchronized with enum
  // NavigationPredictorActionTaken in enums.xml. Order of enum values should
  // not be changed since the values are recorded in UMA.
  enum class Action {
    kUnknown = 0,
    kNone = 1,
    // DEPRECATED: kPreresolve = 2,
    // DEPRECATED: kPreconnect = 3,
    kPrefetch = 4,
    // DEPRECATED: kPreconnectOnVisibilityChange = 5,
    // DEPRECATED: kPreconnectOnAppForeground = 6,  // Deprecated.
    // DEPRECATED: kPreconnectAfterTimeout = 7,
    kMaxValue = kPrefetch,
  };

  // Enum to report the prerender result of the clicked link. Changes must be
  // propagated to enums.xml, and the enum should not be re-ordered.
  enum class PrerenderResult {
    // The prerender finished entirely before the link was clicked.
    kSameOriginPrefetchFinished = 0,
    // The prerender was started but not finished before the user navigated or
    // backgrounded the page.
    kSameOriginPrefetchPartiallyComplete = 1,
    // The link was waiting to be prerendered while another prerender was in
    // progress.
    kSameOriginPrefetchInQueue = 2,
    // The prerender was attempted, but a prerender mechanism skipped the
    // prerender.
    kSameOriginPrefetchSkipped = 3,
    // The link was same origin, but scored poorly in the decider logic.
    kSameOriginBelowThreshold = 4,
    // The URL was not seen in the load event.
    kSameOriginNotSeen = 5,
    // The link was cross origin and scored above the threshold, but we did not
    // prerender it.
    kCrossOriginAboveThreshold = 6,
    // The link was cross origin and scored below the threshold.
    kCrossOriginBelowThreshold = 7,
    // The URL was not seen in the load event.
    kCrossOriginNotSeen = 8,
    kMaxValue = kCrossOriginNotSeen,
  };

 private:
  // Struct holding navigation score, rank and other info of the anchor element.
  // Used for look up when an anchor element is clicked.
  struct NavigationScore;

  // blink::mojom::AnchorElementMetricsHost:
  void ReportAnchorElementMetricsOnClick(
      blink::mojom::AnchorElementMetricsPtr metrics) override;
  void ReportAnchorElementMetricsOnLoad(
      std::vector<blink::mojom::AnchorElementMetricsPtr> metrics,
      const gfx::Size& viewport_size) override;

  // content::WebContentsObserver:
  void OnVisibilityChanged(content::Visibility visibility) override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;

  // prerender::PrerenderHandle::Observer:
  void OnPrerenderStop(prerender::PrerenderHandle* handle) override;
  void OnPrerenderStart(prerender::PrerenderHandle* handle) override {}
  void OnPrerenderStopLoading(prerender::PrerenderHandle* handle) override {}
  void OnPrerenderDomContentLoaded(
      prerender::PrerenderHandle* handle) override {}
  void OnPrerenderNetworkBytesChanged(
      prerender::PrerenderHandle* handle) override {}

  // Returns true if the anchor element metric from the renderer process is
  // valid.
  bool IsValidMetricFromRenderer(
      const blink::mojom::AnchorElementMetrics& metric) const;

  // Returns template URL service. Guaranteed to be non-null.
  TemplateURLService* GetTemplateURLService() const;

  // Merge anchor element metrics that have the same target url (href).
  void MergeMetricsSameTargetUrl(
      std::vector<blink::mojom::AnchorElementMetricsPtr>* metrics) const;

  // Computes and stores document level metrics, including |number_of_anchors_|
  // etc.
  void ComputeDocumentMetricsOnLoad(
      const std::vector<blink::mojom::AnchorElementMetricsPtr>& metrics);

  // Given metrics of an anchor element from both renderer and browser process,
  // returns navigation score. Virtual for testing purposes.
  virtual double CalculateAnchorNavigationScore(
      const blink::mojom::AnchorElementMetrics& metrics,
      int area_rank) const;

  // If |sum_page_scales_| is non-zero, return the page-wide score to add to
  // all the navigation scores. Computed once per page.
  double GetPageMetricsScore() const;

  // Given a vector of navigation scores sorted in descending order, decide what
  // action to take, or decide not to do anything. Example actions including
  // preresolve, preload, prerendering, etc.
  void MaybeTakeActionOnLoad(
      const GURL& document_url,
      const std::vector<std::unique_ptr<NavigationScore>>&
          sorted_navigation_scores);

  // Decides whether to prefetch a URL and, if yes, calls Prefetch.
  void MaybePrefetch();

  // Given a url to prefetch, uses PrerenderManager to start a NoStatePrefetch
  // of that URL.
  virtual void Prefetch(prerender::PrerenderManager* prerender_manager,
                        const GURL& url_to_prefetch);

  // Returns a collection of URLs that can be prefetched. Only one should be
  // prefetched at a time.
  std::deque<GURL> GetUrlsToPrefetch(
      const GURL& document_url,
      const std::vector<std::unique_ptr<NavigationScore>>&
          sorted_navigation_scores);

  // Record anchor element metrics on page load.
  void RecordMetricsOnLoad(
      const blink::mojom::AnchorElementMetrics& metric) const;

  // Record timing information when an anchor element is clicked.
  void RecordTimingOnClick();

  // Records the accuracy of the action taken by the navigator predictor based
  // on the action taken as well as the URL that was navigated to.
  // |target_url| is the URL navigated to by the user.
  void RecordActionAccuracyOnClick(const GURL& target_url) const;

  // Records metrics on which action the predictor is taking.
  void RecordAction(Action log_action);

  // Sends metrics to the UKM id at |ukm_source_id_|.
  void MaybeSendMetricsToUkm() const;

  // After an in-page click, sends the index of the url that was clicked to the
  // UKM id at |ukm_source_id_|.
  void MaybeSendClickMetricsToUkm(const std::string& clicked_url) const;

  // Returns the minimum of the bucket that |value| belongs in, for page-wide
  // metrics, excluding |median_link_location_|.
  int GetBucketMinForPageMetrics(int value) const;

  // Returns the minimum of the bucket that |value| belongs in, used for
  // |median_link_location_| and the |ratio_distance_root_top|.
  int GetLinearBucketForLinkLocation(int value) const;

  // Returns the minimum of the bucket that |value| belongs in, used for
  // |ratio_area|.
  int GetLinearBucketForRatioArea(int value) const;

  // Notifies the keyed service of the updated predicted navigation.
  void NotifyPredictionUpdated(
      const std::vector<std::unique_ptr<NavigationScore>>&
          sorted_navigation_scores);

  // Record metrics about how many prerenders were started and finished.
  void RecordActionAccuracyOnTearDown();

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
  int number_of_anchors_ = 0;

  // Viewport-related metrics for anchor elements: the viewport size,
  // the median distance down the viewport of all the links, and the
  // total clickable space for first viewport links. |total_clickable_space_| is
  // a percent (between 0 and 100).
  gfx::Size viewport_size_;
  int median_link_location_ = 0;
  float total_clickable_space_ = 0;

  // Anchor-specific scaling factors used to compute navigation scores.
  const int ratio_area_scale_;
  const int is_in_iframe_scale_;
  const int is_same_host_scale_;
  const int contains_image_scale_;
  const int is_url_incremented_scale_;
  const int area_rank_scale_;
  const int ratio_distance_root_top_scale_;

  // Page-wide scaling factors used to compute navigation scores.
  const int link_total_scale_;
  const int iframe_link_total_scale_;
  const int increment_link_total_scale_;
  const int same_origin_link_total_scale_;
  const int image_link_total_scale_;
  const int clickable_space_scale_;
  const int median_link_location_scale_;
  const int viewport_height_scale_;
  const int viewport_width_scale_;

  // Sum of all scales for individual anchor metrics.
  // Used to normalize the final computed weight.
  const int sum_link_scales_;

  // Sum of all scales for page-wide metrics.
  const int sum_page_scales_;

  // True if device is a low end device.
  const bool is_low_end_device_;

  // Minimum score that a URL should have for it to be prefetched. Note
  // that scores of origins are computed differently from scores of URLs, so
  // they are not comparable.
  const int prefetch_url_score_threshold_;

  // True if |this| should use the PrerenderManager to prefetch.
  const bool prefetch_enabled_;

  // True by default, otherwise navigation scores will not be normalized
  // by the sum of metrics weights nor normalized from 0 to 100 across
  // all navigation scores for a page.
  const bool normalize_navigation_scores_;

  // A count of clicks to prevent reporting more than 10 clicks to UKM.
  size_t clicked_count_ = 0;

  // Whether a new navigation has started (only set if load event comes before
  // DidStartNavigation).
  bool next_navigation_started_ = false;

  // Timing of document loaded and last click.
  base::TimeTicks document_loaded_timing_;
  base::TimeTicks last_click_timing_;

  // True if the source webpage (i.e., the page on which we are trying to
  // predict the next navigation) is a page from user's default search engine.
  bool source_is_default_search_engine_page_ = false;

  // Current visibility state of the web contents.
  content::Visibility current_visibility_;

  // Current prerender handle.
  std::unique_ptr<prerender::PrerenderHandle> prerender_handle_;

  // URL that we decided to prefetch, and are currently prefetching.
  base::Optional<GURL> prefetch_url_;

  // An ordered list of URLs that should be prefetched in succession.
  std::deque<GURL> urls_to_prefetch_;

  // URLs that were successfully prefetched.
  std::set<GURL> urls_prefetched_;

  // URLs that scored above the threshold in sorted order.
  std::vector<GURL> urls_above_threshold_;

  // URLs that had a prerender started, but were canceled due to background or
  // next navigation.
  std::set<GURL> partial_prerfetches_;

  // UKM ID for navigation
  ukm::SourceId ukm_source_id_;

  // UKM recorder
  ukm::UkmRecorder* ukm_recorder_ = nullptr;

  // The URL of the current page.
  GURL document_url_;

  // Render frame host of the current page.
  const content::RenderFrameHost* render_frame_host_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(NavigationPredictor);
};

#endif  // CHROME_BROWSER_NAVIGATION_PREDICTOR_NAVIGATION_PREDICTOR_H_
