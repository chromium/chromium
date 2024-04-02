// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_EMBEDDINGS_HISTORY_EMBEDDINGS_TAB_HELPER_H_
#define CHROME_BROWSER_HISTORY_EMBEDDINGS_HISTORY_EMBEDDINGS_TAB_HELPER_H_

#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

namespace history {
class HistoryService;
}
namespace history_embeddings {
class HistoryEmbeddingsService;
}
namespace content {
class NavigationHandle;
}

class HistoryEmbeddingsTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<HistoryEmbeddingsTabHelper> {
 public:
  ~HistoryEmbeddingsTabHelper() override;

  HistoryEmbeddingsTabHelper(const HistoryEmbeddingsTabHelper&) = delete;
  HistoryEmbeddingsTabHelper& operator=(const HistoryEmbeddingsTabHelper&) =
      delete;

  // Called by `HistoryTabHelper` right after submitting a new navigation for
  // `web_contents()` to HistoryService. We need close coordination with
  // History's conception of the visit lifetime.
  void OnUpdatedHistoryForNavigation(
      content::NavigationHandle* navigation_handle,
      base::Time timestamp,
      const GURL& url);

 private:
  explicit HistoryEmbeddingsTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<HistoryEmbeddingsTabHelper>;

  // Helper functions to return the embeddings and history services.
  // `GetHistoryClustersService()` may return nullptr (in tests).
  history_embeddings::HistoryEmbeddingsService* GetHistoryEmbeddingsService();
  // `GetHistoryService()` may return nullptr.
  history::HistoryService* GetHistoryService();

  // Task tracker for calls for the history service.
  base::CancelableTaskTracker task_tracker_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_HISTORY_EMBEDDINGS_HISTORY_EMBEDDINGS_TAB_HELPER_H_
