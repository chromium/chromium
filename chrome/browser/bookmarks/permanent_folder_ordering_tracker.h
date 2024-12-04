// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_PERMANENT_FOLDER_ORDERING_TRACKER_H_
#define CHROME_BROWSER_BOOKMARKS_PERMANENT_FOLDER_ORDERING_TRACKER_H_

#include "base/scoped_observation.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_node.h"

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

// Tracks any custom order across child nodes of a particular local and account
// permanent bookmark node of a certain type `bookmarks::BookmarkNode::Type`
// (bookmark bar, other, mobile). Manages operations across children of local
// and account equivalent permanent node e.g. add, move, remove bookmark node.
// It also allows querying their direct children while respecting the custom
// order between the children of the two permanent nodes.
// If only local or syncable node exists, this class just forwards operations to
// the `BookmarkModel`.
class PermanentFolderOrderingTracker : public bookmarks::BookmarkModelObserver {
 public:
  // `tracked_type` must reflect the type of the permanent node, it must be
  // one of the following: BOOKMARK_BAR, OTHER_NODE, MOBILE. Other node types
  // are invalid.
  PermanentFolderOrderingTracker(bookmarks::BookmarkModel* model,
                                 bookmarks::BookmarkNode::Type tracked_type);

  ~PermanentFolderOrderingTracker() override;

  PermanentFolderOrderingTracker(const PermanentFolderOrderingTracker&) =
      delete;
  PermanentFolderOrderingTracker& operator=(
      const PermanentFolderOrderingTracker&) = delete;

  // Returns underlying permanent nodes.
  // The order of the returned nodes:
  // - first the account node if one exists
  // - then the local or syncable node.
  // If the bookmark model is not loaded, it returns empty.
  std::vector<const bookmarks::BookmarkNode*> GetUnderlyingPermanentNodes()
      const;

  // Returns index of `node`.
  // `node` must be a direct child of one of the tracked permanent
  // nodes in `this`.
  size_t GetIndexOf(const bookmarks::BookmarkNode* node) const;

  // bookmarks::BookmarkModelObserver:
  void BookmarkModelLoaded(bool ids_reassigned) override;
  void BookmarkNodeMoved(const bookmarks::BookmarkNode* old_parent,
                         size_t old_index,
                         const bookmarks::BookmarkNode* new_parent,
                         size_t new_index) override;
  void BookmarkNodeAdded(const bookmarks::BookmarkNode* parent,
                         size_t index,
                         bool added_by_user) override;
  void BookmarkNodeRemoved(const bookmarks::BookmarkNode* parent,
                           size_t old_index,
                           const bookmarks::BookmarkNode* node,
                           const std::set<GURL>& removed_urls,
                           const base::Location& location) override;
  void OnWillRemoveAllUserBookmarks(const base::Location& location) override;
  void BookmarkAllUserNodesRemoved(const std::set<GURL>& removed_urls,
                                   const base::Location& location) override;
  void BookmarkNodeChanged(const bookmarks::BookmarkNode* node) override {}
  void BookmarkNodeFaviconChanged(
      const bookmarks::BookmarkNode* node) override {}
  void BookmarkNodeChildrenReordered(
      const bookmarks::BookmarkNode* node) override;

  // Public for testing.
  void SetNodesOrderingForTesting(
      std::vector<raw_ptr<const bookmarks::BookmarkNode>> ordering);

 private:
  void SetTrackedPermanentNodes();
  bool IsTrackedPermanentNode(const bookmarks::BookmarkNode* node) const;
  void ResetOrderingToDefault();
  bool ShouldTrackOrdering() const;
  size_t GetExpectedChildrenCount() const;

  const raw_ptr<bookmarks::BookmarkModel> model_;
  const bookmarks::BookmarkNode::Type tracked_type_;
  raw_ptr<const bookmarks::BookmarkNode> local_or_syncable_node_ = nullptr;
  raw_ptr<const bookmarks::BookmarkNode> account_node_ = nullptr;

  // Non-empty if both `local_or_syncable_node_` and
  // `account_node_` have children.
  std::vector<raw_ptr<const bookmarks::BookmarkNode>> ordering_;

  bool all_user_bookmarks_remove_in_progress_ = false;

  base::ScopedObservation<bookmarks::BookmarkModel,
                          bookmarks::BookmarkModelObserver>
      model_observation_{this};
};

#endif  // CHROME_BROWSER_BOOKMARKS_PERMANENT_FOLDER_ORDERING_TRACKER_H_
