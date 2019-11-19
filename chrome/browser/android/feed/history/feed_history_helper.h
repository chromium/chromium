// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_FEED_HISTORY_FEED_HISTORY_HELPER_H_
#define CHROME_BROWSER_ANDROID_FEED_HISTORY_FEED_HISTORY_HELPER_H_

#include "base/task/cancelable_task_tracker.h"
#include "components/feed/core/feed_logging_metrics.h"
#include "components/history/core/browser/history_types.h"
#include "url/gurl.h"

namespace history {
class HistoryService;
struct QueryURLResult;
}  // namespace history

namespace feed {

// This class helps components/feed to check history service without directly
// depends on components/history. This class holds a raw pointer of history
// service, which means |history_service_| should outlive of this class. Whoever
// instantiates this class needs to guarantee the history service outlives this
// helper class.
class FeedHistoryHelper {
 public:
  explicit FeedHistoryHelper(history::HistoryService* history_service);
  ~FeedHistoryHelper();

  // Check if |url| is visited by querying history service, and return the
  // result to |callback|.
  void CheckURL(const GURL& url,
                FeedLoggingMetrics::CheckURLVisitCallback callback);

 private:
  history::HistoryService* history_service_;
  base::CancelableTaskTracker tracker_;

  void OnCheckURLDone(FeedLoggingMetrics::CheckURLVisitCallback callback,
                      history::QueryURLResult result);

  base::WeakPtrFactory<FeedHistoryHelper> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FeedHistoryHelper);
};

}  // namespace feed

#endif  // CHROME_BROWSER_ANDROID_FEED_HISTORY_FEED_HISTORY_HELPER_H_
