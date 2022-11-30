// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_TRANSLATE_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_TRANSLATE_PAGE_LOAD_METRICS_OBSERVER_H_

#include <memory>

#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

namespace translate {
class TranslateMetricsLogger;
}  // namespace translate

namespace content {
class NavigationHandle;
}  // namespace content

// Observer responsible for notifying Translate of the status of a page load.
// This information is used to log UKM and UMA metrics at a page load level, as
// well as tracking the time a page is in the foreground and either translated
// or not translated.
class TranslatePageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  static std::unique_ptr<TranslatePageLoadMetricsObserver> CreateIfNeeded(
      content::WebContents* web_contents);

  explicit TranslatePageLoadMetricsObserver(
      std::unique_ptr<translate::TranslateMetricsLogger>
          translate_metrics_logger);
  ~TranslatePageLoadMetricsObserver() override;

  TranslatePageLoadMetricsObserver(const TranslatePageLoadMetricsObserver&) =
      delete;
  TranslatePageLoadMetricsObserver& operator=(
      const TranslatePageLoadMetricsObserver&) = delete;

  // page_load_metrics::PageLoadMetricsObserver
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle) override;
  ObservePolicy OnHidden(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  ObservePolicy OnShown() override;
  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void DidActivatePrerenderedPage(
      content::NavigationHandle* navigation_handle) override;

 private:
  bool is_in_primary_page_ = false;
  std::unique_ptr<translate::TranslateMetricsLogger> translate_metrics_logger_;
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_TRANSLATE_PAGE_LOAD_METRICS_OBSERVER_H_
