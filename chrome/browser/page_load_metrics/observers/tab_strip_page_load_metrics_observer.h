// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_TAB_STRIP_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_TAB_STRIP_PAGE_LOAD_METRICS_OBSERVER_H_

#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace internal {

extern const char kTabsActiveAbsolutePosition[];
extern const char kTabsActiveRelativePosition[];
extern const char kTabsPageLoadTimeSinceActive[];
extern const char kTabsPageLoadTimeSinceCreated[];

}  // namespace internal

// Observer responsible for notifying the tab strip of the status of a page
// load. This information is used to log UMA metrics at a page load level.
class TabStripPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  explicit TabStripPageLoadMetricsObserver(content::WebContents* web_contents);
  ~TabStripPageLoadMetricsObserver() override;

  TabStripPageLoadMetricsObserver(const TabStripPageLoadMetricsObserver&) =
      delete;
  TabStripPageLoadMetricsObserver& operator=(
      const TabStripPageLoadMetricsObserver&) = delete;

  // page_load_metrics::PageLoadMetricsObserver
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  ObservePolicy OnShown() override;

 private:
  raw_ptr<content::WebContents> web_contents_ = nullptr;
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_TAB_STRIP_PAGE_LOAD_METRICS_OBSERVER_H_
