// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_EXPANDED_STATE_TRACKER_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_EXPANDED_STATE_TRACKER_H_

#include <set>

#include "base/memory/raw_ptr.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/keyed_service/core/keyed_service.h"

class PrefService;

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

// BookmarkExpandedStateTracker is used to track a set of expanded nodes. The
// nodes are persisted in preferences. If an expanded node is removed from the
// model BookmarkExpandedStateTracker removes the node.
class BookmarkExpandedStateTracker
    : public bookmarks::BaseBookmarkModelObserver,
      public KeyedService {
 public:
  typedef std::set<const bookmarks::BookmarkNode*> Nodes;

  explicit BookmarkExpandedStateTracker(PrefService* pref_service);

  BookmarkExpandedStateTracker(const BookmarkExpandedStateTracker&) = delete;
  BookmarkExpandedStateTracker& operator=(const BookmarkExpandedStateTracker&) =
      delete;

  ~BookmarkExpandedStateTracker() override;

  // Should be invoked during the corresponding BookmarkModel creating - before
  // that model is loaded.
  void Init(bookmarks::BookmarkModel* bookmark_model);

  // The set of expanded nodes.
  void SetExpandedNodes(const Nodes& nodes);
  Nodes GetExpandedNodes();

 private:
  // BaseBookmarkModelObserver:
  void BookmarkModelLoaded(bool ids_reassigned) override;
  void BookmarkModelChanged() override;
  void BookmarkModelBeingDeleted() override;
  void BookmarkNodeRemoved(const bookmarks::BookmarkNode* parent,
                           size_t old_index,
                           const bookmarks::BookmarkNode* node,
                           const std::set<GURL>& removed_urls,
                           const base::Location& location) override;
  void BookmarkAllUserNodesRemoved(const std::set<GURL>& removed_urls,
                                   const base::Location& location) override;

  // Updates the value for |prefs::kBookmarkEditorExpandedNodes| from
  // GetExpandedNodes().
  void UpdatePrefs(const Nodes& nodes);

  raw_ptr<bookmarks::BookmarkModel> bookmark_model_ = nullptr;
  const raw_ptr<PrefService> pref_service_;
};

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_EXPANDED_STATE_TRACKER_H_
