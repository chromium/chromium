// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_OFFLINE_PAGE_BOOKMARK_OBSERVER_H_
#define CHROME_BROWSER_OFFLINE_PAGES_OFFLINE_PAGE_BOOKMARK_OBSERVER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/offline_pages/offline_page_model_factory.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/offline_pages/core/offline_page_types.h"

class GURL;

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}

namespace content {
class BrowserContext;
}

namespace offline_pages {

class OfflinePageBookmarkObserver
    : public bookmarks::BaseBookmarkModelObserver {
 public:
  explicit OfflinePageBookmarkObserver(content::BrowserContext* context);
  ~OfflinePageBookmarkObserver() override;

  // Implement bookmarks::OfflinePageBookmarkObserver
  void BookmarkModelChanged() override;
  void BookmarkNodeRemoved(bookmarks::BookmarkModel* model,
                           const bookmarks::BookmarkNode* parent,
                           size_t old_index,
                           const bookmarks::BookmarkNode* node,
                           const std::set<GURL>& removed_urls) override;

 private:
  // Does work for actual deleting removed bookmark pages.
  void DoDeleteRemovedBookmarkPages(const MultipleOfflineIdResult& offline_ids);

  // Callback for deleting removed bookmark pages.
  void OnDeleteRemovedBookmarkPagesDone(DeletePageResult result);

  content::BrowserContext* context_;

  OfflinePageModel* offline_page_model_;

  base::WeakPtrFactory<OfflinePageBookmarkObserver> weak_ptr_factory_{this};
};
}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_OFFLINE_PAGE_BOOKMARK_OBSERVER_H_
