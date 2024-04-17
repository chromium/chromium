// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_CHROME_HISTORY_CLIENT_H_
#define CHROME_BROWSER_HISTORY_CHROME_HISTORY_CLIENT_H_

#include <memory>
#include <set>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/history/core/browser/history_client.h"

class GURL;

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}

// This class implements history::HistoryClient to abstract operations that
// depend on Chrome environment.
class ChromeHistoryClient : public history::HistoryClient,
                            public bookmarks::BaseBookmarkModelObserver {
 public:
  explicit ChromeHistoryClient(bookmarks::BookmarkModel* bookmark_model);

  ChromeHistoryClient(const ChromeHistoryClient&) = delete;
  ChromeHistoryClient& operator=(const ChromeHistoryClient&) = delete;

  ~ChromeHistoryClient() override;

  // history::HistoryClient implementation.
  void OnHistoryServiceCreated(
      history::HistoryService* history_service) override;
  void Shutdown() override;
  history::CanAddURLCallback GetThreadSafeCanAddURLCallback() const override;
  void NotifyProfileError(sql::InitStatus init_status,
                          const std::string& diagnostics) override;
  std::unique_ptr<history::HistoryBackendClient> CreateBackendClient() override;
  void UpdateBookmarkLastUsedTime(int64_t bookmark_node_id,
                                  base::Time time) override;

 private:
  void StopObservingBookmarkModel();

  // bookmarks::BaseBookmarkModelObserver implementation.
  void BookmarkModelChanged() override;
  void BookmarkModelBeingDeleted() override;
  void BookmarkNodeRemoved(const bookmarks::BookmarkNode* parent,
                           size_t old_index,
                           const bookmarks::BookmarkNode* node,
                           const std::set<GURL>& removed_url,
                           const base::Location& location) override;
  void BookmarkAllUserNodesRemoved(const std::set<GURL>& removed_urls,
                                   const base::Location& location) override;

  // BookmarkModel instance providing access to bookmarks. May be null during
  // testing, and is null while shutting down.
  raw_ptr<bookmarks::BookmarkModel> bookmark_model_;

  // Callback invoked when URLs are removed from BookmarkModel.
  base::RepeatingCallback<void(const std::set<GURL>&)> on_bookmarks_removed_;

  // Subscription for notifications of changes to favicons.
  base::CallbackListSubscription favicons_changed_subscription_;
};

#endif  // CHROME_BROWSER_HISTORY_CHROME_HISTORY_CLIENT_H_
