// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_LCP_CRITICAL_PATH_PREDICTOR_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_LCP_CRITICAL_PATH_PREDICTOR_PAGE_LOAD_METRICS_OBSERVER_H_

#include <vector>

#include "chrome/browser/predictors/lcp_critical_path_predictor/lcp_critical_path_predictor_util.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "content/public/browser/page_user_data.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "url/origin.h"

namespace internal {

// Expose metrics for tests.
extern const char kHistogramLCPPFirstContentfulPaint[];
extern const char kHistogramLCPPLargestContentfulPaint[];
extern const char kHistogramLCPPPredictResult[];
extern const char kHistogramLCPPPredictHitIndex[];
extern const char kHistogramLCPPActualLCPIndex[];

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class LCPPPredictResult {
  kSuccess = 0,
  // Prediction failed. No learned locator hit the actual LCP.
  kFailureNoHit = 1,
  // Below three failures are false positive cases.
  // The actual LCP is unrecorded one.
  kFailureActuallyUnrecordedLCP = 2,
  // The actual LCP is one with the same locator.
  kFailureActuallySameButLaterLCP = 3,
  // The actual LCP is one with another leaned locator.
  kFailureActuallySecondaryLCP = 4,
  kMaxValue = kFailureActuallySecondaryLCP,
};

// Since histogram counts only positive numbers but the indexes origin 0,
// add 1 for offset.
const int kLCPIndexHistogramOffset = 1;
}  // namespace internal

// PageLoadMetricsObserver responsible for:
// - Staging LCP element locator, LCP influencer scripts, used fonts and other
//   information until LCP is finalized, and
// - Reporting "PageLoad.Clients.LCPP." UMAs
class LcpCriticalPathPredictorPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  class PageData : public content::PageUserData<PageData> {
   public:
    ~PageData() override;

    void SetLcpCriticalPathPredictorPageLoadMetricsObserver(
        base::WeakPtr<LcpCriticalPathPredictorPageLoadMetricsObserver>
            lcpp_page_load_metrics_observer) {
      lcpp_page_load_metrics_observer_ =
          std::move(lcpp_page_load_metrics_observer);
    }
    LcpCriticalPathPredictorPageLoadMetricsObserver*
    GetLcpCriticalPathPredictorPageLoadMetricsObserver() const {
      return lcpp_page_load_metrics_observer_.get();
    }

   private:
    explicit PageData(content::Page& page);
    friend content::PageUserData<PageData>;
    PAGE_USER_DATA_KEY_DECL();

    base::WeakPtr<LcpCriticalPathPredictorPageLoadMetricsObserver>
        lcpp_page_load_metrics_observer_;
  };

  LcpCriticalPathPredictorPageLoadMetricsObserver();
  LcpCriticalPathPredictorPageLoadMetricsObserver(
      const LcpCriticalPathPredictorPageLoadMetricsObserver&) = delete;
  LcpCriticalPathPredictorPageLoadMetricsObserver& operator=(
      const LcpCriticalPathPredictorPageLoadMetricsObserver&) = delete;
  ~LcpCriticalPathPredictorPageLoadMetricsObserver() override;

  void SetLcpElementLocator(const std::string& lcp_element_locator,
                            std::optional<uint32_t> predicted_lcp_index);
  void SetLcpInfluencerScriptUrls(
      const std::vector<GURL>& lcp_influencer_scripts);
  void SetPreconnectOrigins(const std::vector<GURL>& origins);
  void SetUnusedPreloads(const std::vector<GURL>& unused_preloads);
  // Append fetched font URLs to the list to be passed to LCPP.
  void AppendFetchedFontUrl(const GURL& font_url, bool hit);
  void AppendFetchedSubresourceUrl(
      const GURL& subresource_url,
      const base::TimeDelta& subresource_load_start,
      network::mojom::RequestDestination request_destination);

 private:
  // PageLoadMetricsObserver implementation:
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle) override;
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;
  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  PageLoadMetricsObserver::ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void RecordTimingHistograms();
  void FinalizeLCP();
  void OnFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void ReportUMAForTimingPredictor(
      std::optional<predictors::LcppStat> lcpp_stat_prelearn);

  // True if the page is prerendered.
  bool is_prerender_ = false;

  // The URL of the last navigation commit.
  std::optional<GURL> commit_url_;
  std::optional<url::Origin> initiator_origin_;

  // Flipped to true iff the navigation had associated non-empty LCPP hint data.
  bool is_lcpp_hinted_navigation_ = false;

  std::optional<predictors::LcppDataInputs> lcpp_data_inputs_;

  // Prediction result. This keeps SetLcpElementLocator's second argument.
  // `predicted_lcp_index` is predicted index of `lcp_element_locators` in
  // LCPCriticalPathPredictorNavigationTimeHint.
  // std::nullopt value means the LCP didn't hit any of `lcp_element_locators`.
  std::vector<std::optional<uint32_t>> predicted_lcp_indexes_;

  base::WeakPtrFactory<LcpCriticalPathPredictorPageLoadMetricsObserver>
      weak_factory_{this};
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_LCP_CRITICAL_PATH_PREDICTOR_PAGE_LOAD_METRICS_OBSERVER_H_
