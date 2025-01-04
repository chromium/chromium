// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_INSERT_MODEL_QUICK_INSERT_LINK_SUGGESTER_H_
#define ASH_QUICK_INSERT_MODEL_QUICK_INSERT_LINK_SUGGESTER_H_

#include <vector>

#include "ash/quick_insert/quick_insert_search_result.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"

namespace favicon {
class FaviconService;
}

namespace favicon_base {
struct FaviconImageResult;
}

namespace history {
class HistoryService;
class QueryResults;
class URLResult;
}  // namespace history

// A class to suggest links based on recent browsing history.
class ASH_EXPORT QuickInsertLinkSuggester {
 public:
  using SuggestedLinksCallback =
      base::RepeatingCallback<void(std::vector<ash::QuickInsertSearchResult>)>;

  QuickInsertLinkSuggester();
  ~QuickInsertLinkSuggester();
  QuickInsertLinkSuggester(const QuickInsertLinkSuggester&) = delete;
  QuickInsertLinkSuggester& operator=(const QuickInsertLinkSuggester&) = delete;

  void GetSuggestedLinks(history::HistoryService* history_service,
                         favicon::FaviconService* favicon_service,
                         size_t max_links,
                         SuggestedLinksCallback callback);

 private:
  void OnGetBrowsingHistory(favicon::FaviconService* favicon_service,
                            SuggestedLinksCallback callback,
                            history::QueryResults results);
  void OnGetFaviconImage(
      history::URLResult result,
      base::OnceCallback<void(ash::QuickInsertSearchResult)> callback,
      const favicon_base::FaviconImageResult& favicon_image_result);

  base::CancelableTaskTracker history_query_tracker_;
  std::vector<base::CancelableTaskTracker> favicon_query_trackers_;
  std::vector<ash::QuickInsertSearchResult> suggested_links_;

  base::WeakPtrFactory<QuickInsertLinkSuggester> weak_factory_{this};
};

#endif  // ASH_QUICK_INSERT_MODEL_QUICK_INSERT_LINK_SUGGESTER_H_
