// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_EMBEDDINGS_HISTORY_EMBEDDINGS_TAB_HELPER_H_
#define CHROME_BROWSER_HISTORY_EMBEDDINGS_HISTORY_EMBEDDINGS_TAB_HELPER_H_

#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "chrome/browser/resource_coordinator/tab_load_tracker.h"
#include "components/history/core/browser/history_types.h"
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
      public content::WebContentsUserData<HistoryEmbeddingsTabHelper>,
      public resource_coordinator::TabLoadTracker::Observer {
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

  // content::WebContentsObserver:
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;

  // resource_coordinator::TabLoadTracker:
  void OnLoadingStateChange(content::WebContents* web_contents,
                            LoadingState old_loading_state,
                            LoadingState new_loading_state) override;

 private:
  explicit HistoryEmbeddingsTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<HistoryEmbeddingsTabHelper>;

  // Utility method to delay passage extraction until tabs are done loading.
  // Returns true if actually scheduled; false if weak pointer was invalidated.
  bool ScheduleExtraction(content::WeakDocumentPtr weak_render_frame_host);

  // This is called some time after `DidFinishLoad` to do passage extraction.
  // Calls may be canceled by weak pointer invalidation.
  void ExtractPassages(content::WeakDocumentPtr weak_render_frame_host);

  // Callback for `ExtractPassages()`. It's in a member method to enable
  // cancellation via `weak_factory_`.
  void ExtractPassagesWithHistoryData(
      content::WeakDocumentPtr weak_render_frame_host,
      history::QueryURLResult result);

  // Invalidates weak pointers and cancels any pending extraction callbacks.
  void CancelExtraction();

  // Helper functions to return the embeddings and history services.
  // `GetHistoryClustersService()` may return nullptr (in tests).
  history_embeddings::HistoryEmbeddingsService* GetHistoryEmbeddingsService();
  // `GetHistoryService()` may return nullptr.
  history::HistoryService* GetHistoryService();

  // Data saved from the `HistoryTabHelper` call to
  // `OnUpdatedHistoryForNavigation` which happens in `DidFinishNavigation`
  // and precedes `DidFinishLoad`.
  std::optional<base::Time> history_visit_time_;
  std::optional<GURL> history_url_;

  // Task tracker for calls for the history service.
  base::CancelableTaskTracker task_tracker_;

  // This factory frequently invalidates existing weak pointers to cancel
  // delayed passage extraction.
  base::WeakPtrFactory<HistoryEmbeddingsTabHelper> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_HISTORY_EMBEDDINGS_HISTORY_EMBEDDINGS_TAB_HELPER_H_
