// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_EMBEDDINGS_HISTORY_EMBEDDINGS_TAB_HELPER_H_
#define CHROME_BROWSER_HISTORY_EMBEDDINGS_HISTORY_EMBEDDINGS_TAB_HELPER_H_

#include <optional>

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
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

namespace passage_embeddings {
class PassageEmbedderModelObserver;
}

class HistoryEmbeddingsTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<HistoryEmbeddingsTabHelper> {
 public:
  ~HistoryEmbeddingsTabHelper() override;

  HistoryEmbeddingsTabHelper(const HistoryEmbeddingsTabHelper&) = delete;
  HistoryEmbeddingsTabHelper& operator=(const HistoryEmbeddingsTabHelper&) =
      delete;

  // Called right after submitting a new navigation for `web_contents()` to
  // HistoryService. Allows close coordination with History's conception of the
  // visit lifetime.
  // Virtual for testing.
  virtual void OnUpdatedHistoryForNavigation(int64_t navigation_id,
                                             bool is_in_primary_main_frame,
                                             base::Time timestamp,
                                             const GURL& url);

  // content::WebContentsObserver:
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void WebContentsDestroyed() override;

  void SetHistoryTabHelperSubscription(
      base::CallbackListSubscription subscription);

  base::WeakPtr<HistoryEmbeddingsTabHelper> GetWeakPtr();

 protected:
  explicit HistoryEmbeddingsTabHelper(content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<HistoryEmbeddingsTabHelper>;

  // Callback for `ExtractPassages()`. It's in a member method to enable
  // cancellation via `weak_factory_`.
  void UpdateEmbeddingsServiceWithHistoryData(
      history::QueryURLAndVisitsResult result);

  // Invalidates weak pointers and cancels any pending history lookup callbacks.
  void CancelHistoryLookup();

  // Helper functions to return the embeddings and history services.
  // `GetHistoryClustersService()` may return nullptr (in tests).
  history_embeddings::HistoryEmbeddingsService* GetHistoryEmbeddingsService();
  passage_embeddings::PassageEmbedderModelObserver*
  GetPassageEmbedderModelObserver();
  // `GetHistoryService()` may return nullptr.
  history::HistoryService* GetHistoryService();

  // Data saved from the `HistoryTabHelper` call to
  // `OnUpdatedHistoryForNavigation` which happens in `DidFinishNavigation`
  // and precedes `DidFinishLoad`.
  std::optional<base::Time> history_visit_time_;
  std::optional<GURL> history_url_;

  // Task tracker for calls for the history service.
  base::CancelableTaskTracker task_tracker_;

  base::CallbackListSubscription history_tab_helper_subscription_;

  // This factory frequently invalidates existing weak pointers to cancel
  // delayed passage extraction.
  base::WeakPtrFactory<HistoryEmbeddingsTabHelper> extraction_weak_ptr_factory_{
      this};

  // A standard WeakPtrFactory with lifetime equal to the object itself.
  base::WeakPtrFactory<HistoryEmbeddingsTabHelper> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_HISTORY_EMBEDDINGS_HISTORY_EMBEDDINGS_TAB_HELPER_H_
