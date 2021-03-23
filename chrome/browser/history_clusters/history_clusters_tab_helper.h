// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_CLUSTERS_HISTORY_CLUSTERS_TAB_HELPER_H_
#define CHROME_BROWSER_HISTORY_CLUSTERS_HISTORY_CLUSTERS_TAB_HELPER_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/history/core/browser/history_types.h"
#include "components/memories/core/visit_data.h"
#include "components/page_load_metrics/common/page_end_reason.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace history {
struct HistoryAddPageArgs;
}

namespace memories {
class MemoriesService;
}

class HistoryClustersTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<HistoryClustersTabHelper> {
 public:
  ~HistoryClustersTabHelper() override;

  HistoryClustersTabHelper(const HistoryClustersTabHelper&) = delete;
  HistoryClustersTabHelper& operator=(const HistoryClustersTabHelper&) = delete;

  // Called when the user copies the URL from the location bar.
  void LogUrlCopied();

  // Called by HistoryTabHelper right after submitting a new navigation for
  // |web_contents()| to HistoryService. We need close coordination with
  // History's conception of the visit lifetime.
  void DidUpdateHistoryForNavigation(
      content::NavigationHandle* navigation_handle,
      const history::HistoryAddPageArgs& add_page_args);

  // Updates the visit with |navigation_id| with |page_end_reason|.
  // This also records the page end metrics, if necessary.
  // It returns a copy of the completed MemoriesVisit, if available.
  //
  // This should only be called once per navigation, as this may flush the visit
  // to MemoriesService.
  base::Optional<memories::MemoriesVisit> UpdatePageEndReasonAndGetVisitForUkm(
      int64_t navigation_id,
      const page_load_metrics::PageEndReason page_end_reason);

 private:
  FRIEND_TEST_ALL_PREFIXES(UkmPageLoadMetricsObserverTest,
                           DurationSinceLastVisitSeconds);

  explicit HistoryClustersTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<HistoryClustersTabHelper>;

  // Computes and stores the page-end metrics. Can be called multiple times,
  // because we have a flag to prevent multiple recordings.
  void RecordPageEndMetricsIfNeeded(memories::MemoriesVisit& visit);

  // content::WebContentsObserver implementation.
  void WebContentsDestroyed() override;

  // Callback for HistoryService::GetLastVisitToURL.
  void PreviousVisitToUrlCallback(int64_t navigation_id,
                                  history::HistoryLastVisitResult result);

  // Vector of recorded visits from this tab helper. We have to store more than
  // one here, because UKMPageLoadMetricsObserver may ask us to update a visit
  // AFTER we have started the navigation for a new visit, so we can't keep
  // just one around.
  //
  // TODO(tommycli): Long term, it would be best if these incomplete visits
  // are directly managed by MemoriesService rather than the tab helper.
  // The main obstacle to that are these:
  //  - Currently the visits are stored by MemoriesService only if the kMemories
  //    feature flag is Enabled. We need to record UKM even if the flag is off.
  //  - But we DO NOT want normal users with the flag Disabled to be growing
  //    an in-memory vector to an unbounded size. It's a memory leak.
  //  - At least being stored here, it's scoped to the lifetime of a tab, not
  //    to the browser as a whole, so it will be periodically freed from memory.
  //  - Once we have a good persistence strategy, let the Service manage these
  //    directly.
  std::vector<memories::MemoriesVisit> visits_;

  // Task tracker for calls for the history service.
  base::CancelableTaskTracker task_tracker_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_HISTORY_CLUSTERS_HISTORY_CLUSTERS_TAB_HELPER_H_
