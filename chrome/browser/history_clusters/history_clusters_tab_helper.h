// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_CLUSTERS_HISTORY_CLUSTERS_TAB_HELPER_H_
#define CHROME_BROWSER_HISTORY_CLUSTERS_HISTORY_CLUSTERS_TAB_HELPER_H_

#include <vector>

#include "base/gtest_prod_util.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/page_load_metrics/common/page_end_reason.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace history_clusters {
class HistoryClustersService;
}

class HistoryClustersTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<HistoryClustersTabHelper> {
 public:
  ~HistoryClustersTabHelper() override;

  HistoryClustersTabHelper(const HistoryClustersTabHelper&) = delete;
  HistoryClustersTabHelper& operator=(const HistoryClustersTabHelper&) = delete;

  // Called when the user copies the URL from the location bar.
  void OnOmniboxUrlCopied();
  // Called when the user shares the URL via mobile sharing hub.
  void OnOmniboxUrlShared();

  // Called by `HistoryTabHelper` right after submitting a new navigation for
  // `web_contents()` to HistoryService. We need close coordination with
  // History's conception of the visit lifetime.
  void OnUpdatedHistoryForNavigation(int64_t navigation_id,
                                     base::Time timestamp,
                                     const GURL& url);

  // Invoked for navigations that are tracked by UKM. Specifically, same-app
  // navigations aren't tracked individually in UKM and therefore won't receive
  // UKM's `page_end_reason` signal. Visits for such navigations will be
  // completed as soon as both the history rows query completes and the history
  // navigation ends. Visits that are tracked by UKM will additionally wait for
  // a UKM `page_end_reason`.
  void TagNavigationAsExpectingUkmNavigationComplete(int64_t navigation_id);

  // Updates the visit with `navigation_id` with `total_foreground_duration` and
  // `page_end_reason`. This also records the page end metrics, if necessary. It
  // returns a copy of the completed `AnnotatedVisit`'s
  // `VisitContextAnnotations`, if available.
  //
  // This should only be called once per navigation, as this may flush the visit
  // to HistoryClustersService.
  history::VisitContextAnnotations OnUkmNavigationComplete(
      int64_t navigation_id,
      base::TimeDelta total_foreground_duration,
      const page_load_metrics::PageEndReason page_end_reason);

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void WebContentsDestroyed() override;
  void OnVisibilityChanged(content::Visibility visibility) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(UkmPageLoadMetricsObserverTest,
                           DurationSinceLastVisitSeconds);

  explicit HistoryClustersTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<HistoryClustersTabHelper>;

  void StartNewNavigationIfNeeded(int64_t navigation_id);

  // Computes and stores the page-end metrics. Can be called multiple times,
  // because we have a flag to prevent multiple recordings. Returns true if it
  // recorded page end metrics.
  void RecordPageEndMetricsIfNeeded(int64_t navigation_id);

  // Helper functions to return the memories and history services.
  // `GetHistoryClustersService()` may return nullptr (in tests).
  history_clusters::HistoryClustersService* GetHistoryClustersService();
  // `GetHistoryService()` may return nullptr.
  history::HistoryService* GetHistoryService();

  // The navigations initiated in this tab. Used for:
  // 1) On `OnUpdatedHistoryForNavigation()`, the last navigation will be
  //    assumed ended and its page end metrics will be recorded.
  // 2) On `OnOmniboxUrlCopied()`, the last navigation will be assumed to be the
  //    subject.
  // 3) On `WebContentsDestroyed()`, the `AnnotatedVisit` corresponding to these
  //    IDs will be assumed ended and their page end metrics will be recorded if
  //    they haven't already.
  std::vector<int64_t> navigation_ids_;

  // Task tracker for calls for the history service.
  base::CancelableTaskTracker task_tracker_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_HISTORY_CLUSTERS_HISTORY_CLUSTERS_TAB_HELPER_H_
