// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_LCP_CRITICAL_PATH_PREDICTOR_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_LCP_CRITICAL_PATH_PREDICTOR_PAGE_LOAD_METRICS_OBSERVER_H_

#include "chrome/browser/predictors/lcp_critical_path_predictor/lcp_critical_path_predictor_util.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "content/public/browser/page_user_data.h"

namespace internal {

// Expose metrics for tests.
extern const char kHistogramLCPPFirstContentfulPaint[];
extern const char kHistogramLCPPLargestContentfulPaint[];

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

  void SetLcpElementLocator(const std::string& lcp_element_locator);
  void SetLcpInfluencerScriptUrls(
      const std::vector<GURL>& lcp_influencer_scripts);
  // Append fetched font URLs to the list to be passed to LCPP.
  void AppendFetchedFontUrl(const GURL& font_url);

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

  // The URL of the last navigation commit.
  absl::optional<GURL> commit_url_;

  // Flipped to true iff the navigation had associated non-empty LCPP hint data.
  bool is_lcpp_hinted_navigation_ = false;

  absl::optional<predictors::LcppDataInputs> lcpp_data_inputs_;

  base::WeakPtrFactory<LcpCriticalPathPredictorPageLoadMetricsObserver>
      weak_factory_{this};
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_LCP_CRITICAL_PATH_PREDICTOR_PAGE_LOAD_METRICS_OBSERVER_H_
