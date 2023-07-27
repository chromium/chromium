// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_LCP_CRITICAL_PATH_PREDICTOR_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_LCP_CRITICAL_PATH_PREDICTOR_PAGE_LOAD_METRICS_OBSERVER_H_

#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "content/public/browser/page_user_data.h"

namespace content {
class WebContents;
}  // namespace content

namespace predictors {
class ResourcePrefetchPredictor;
}  // namespace predictors

// Observer responsible for accumulating and storing LCP element locator
// information.
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

  // Returns a LcpCriticalPathPredictorPageLoadMetricsObserver, or nullptr if it
  // is not needed.
  static std::unique_ptr<LcpCriticalPathPredictorPageLoadMetricsObserver>
  CreateIfNeeded(content::WebContents* web_contents);
  LcpCriticalPathPredictorPageLoadMetricsObserver(
      const LcpCriticalPathPredictorPageLoadMetricsObserver&) = delete;
  LcpCriticalPathPredictorPageLoadMetricsObserver& operator=(
      const LcpCriticalPathPredictorPageLoadMetricsObserver&) = delete;
  ~LcpCriticalPathPredictorPageLoadMetricsObserver() override;

  void SetLcpElementLocator(const std::string& lcp_element_locator) {
    lcp_element_locator_ = lcp_element_locator;
  }

 private:
  explicit LcpCriticalPathPredictorPageLoadMetricsObserver(
      predictors::ResourcePrefetchPredictor& predictor);
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
  void FinalizeLCPPSignals();

  absl::optional<GURL> commit_url_;
  absl::optional<std::string> lcp_element_locator_;
  const raw_ref<predictors::ResourcePrefetchPredictor> predictor_;
  base::WeakPtrFactory<LcpCriticalPathPredictorPageLoadMetricsObserver>
      weak_factory_{this};
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_LCP_CRITICAL_PATH_PREDICTOR_PAGE_LOAD_METRICS_OBSERVER_H_
