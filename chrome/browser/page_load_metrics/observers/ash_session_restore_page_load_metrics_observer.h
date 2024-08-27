// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_ASH_SESSION_RESTORE_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_ASH_SESSION_RESTORE_PAGE_LOAD_METRICS_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

namespace content {
class WebContents;
}  // namespace content

// Only active for chromeos_ash builds.
//
// Records the "first input delay" for the active tab of the active browser
// window that's opened after a full session restore. In all other cases, this
// class is a no-op. In fact, once the metric is recorded, this class shouldn't
// even be instantiated.
//
// This metric is exactly the same as
// `PageLoad.InteractiveTiming.FirstInputDelay4` in `UmaPageLoadMetricsObserver`
// but is only recorded specifically under the conditions stated above.
class AshSessionRestorePageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  static constexpr char kFirstInputDelayName[] =
      "Ash.FullRestore.Browser.FirstInputDelay";

  // Whether a `AshSessionRestorePageLoadMetricsObserver` should be instantiated
  // for the given `profile`. If false, the metric recording is already complete
  // or a full restore is not possible to begin with, so it's wasteful to create
  // a no-op observer for every future page load.
  static bool ShouldBeInstantiated(Profile* profile);

  explicit AshSessionRestorePageLoadMetricsObserver(
      content::WebContents* web_contents);
  AshSessionRestorePageLoadMetricsObserver(
      const AshSessionRestorePageLoadMetricsObserver&) = delete;
  AshSessionRestorePageLoadMetricsObserver& operator=(
      const AshSessionRestorePageLoadMetricsObserver&) = delete;
  ~AshSessionRestorePageLoadMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver
  const char* GetObserverName() const override;
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;
  void OnFirstInputInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle) override;

 private:
  void TryRecordFirstInputDelay(
      const page_load_metrics::mojom::PageLoadTiming& timing) const;

  const raw_ptr<content::WebContents> web_contents_;
  // Whether the `web_contents_` originated from a browser session restore.
  const bool is_web_contents_from_restore_ = false;
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_ASH_SESSION_RESTORE_PAGE_LOAD_METRICS_OBSERVER_H_
