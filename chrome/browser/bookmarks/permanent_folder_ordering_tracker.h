// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_PERMANENT_FOLDER_ORDERING_TRACKER_H_
#define CHROME_BROWSER_BOOKMARKS_PERMANENT_FOLDER_ORDERING_TRACKER_H_

#include "base/scoped_observation.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_node_data.h"

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
  class Delegate {
   public:
    // Called every time the ordering of the nodes in
    // `PermanentFolderOrderingTracker` changes.
    virtual void TrackedOrderingChanged() = 0;

    virtual ~Delegate() = default;
  };

  // `tracked_type` must reflect the type of the permanent node, it must be
  // one of the following: BOOKMARK_BAR, OTHER_NODE, MOBILE. Other node types
  // are invalid.
  // `delegate` must not be null and must outlive this class.
  PermanentFolderOrderingTracker(bookmarks::BookmarkModel* model,
                                 bookmarks::BookmarkNode::Type tracked_type,
                                 Delegate* delegate);

  ~PermanentFolderOrderingTracker() override;

  PermanentFolderOrderingTracker(const PermanentFolderOrderingTracker&) =
      delete;
  PermanentFolderOrderingTracker& operator=(
      const PermanentFolderOrderingTracker&) = delete;

  // This function must be invoked, and ordering will only be tracked
  // afterwards.
  void Init(std::vector<int64_t> in_order_node_ids);

  // Returns underlying permanent nodes.
  // If the bookmark model is not loaded, it returns empty.
  std::vector<const bookmarks::BookmarkNode*> GetUnderlyingPermanentNodes()
      const;

  // Returns default parent node for new nodes created as a child of
  // `tracked_type_`.
  // This will return the account node if one exists, otherwise it returns the
  // local/syncable node.
  // `BookmarkModel` must be loaded.
  const bookmarks::BookmarkNode* GetDefaultParentForNewNodes() const;

  // Returns index of `node`.
  // `node` must be a direct child of one of the tracked permanent
  // nodes in `this`.
  size_t GetIndexOf(const bookmarks::BookmarkNode* node) const;

  // Returns node at `index`.
  const bookmarks::BookmarkNode* GetNodeAtIndex(size_t index) const;

  // Returns children count for nodes tracked in this tracker.
  size_t GetChildrenCount() const;

  // Moves node from an arbitrary parent to become a child of the permanent
  // folder tracked by this at `index`.
  // If `node` is local or account bookmark, it will remain local/account after
  // the move.
  // Note that if node is already being tracked by this, there are two possible
  // target indices (index) that result in a no-op. This is similar to what
  // `BookmarkModel::Move()` does
  // Returns the new index or `std::nullopt` if `MoveToIndex` is no-op.
  std::optional<size_t> MoveToIndex(const bookmarks::BookmarkNode* node,
                                    size_t index);

  // Copies nodes in `elements` to be new child nodes of the permanent
  // folder tracked by this starting at `index`.
  // The new nodes will be child nodes of `GetDefaultParentForNewNodes()`.
  void AddNodesAsCopiesOfNodeData(
      const std::vector<bookmarks::BookmarkNodeData::Element>& elements,
      size_t index);

  // Helper function for optimization purposes, it returns the same value as
  // `GetIndexOf()`.
  // Returns `in_storage_index` if ordering is not tracked
  // `ShouldTrackOrdering()` is false. Otherwise, returns `GetIndexOf()`
  size_t GetIndexAcrossStorage(const bookmarks::BookmarkNode* node,
                               size_t in_storage_index) const;

  // Returns true if `ordering` is not empty and has non-default order.
  // Default order is all account child nodes then local child nodes.
  bool IsNonDefaultOrderingTracked() const;

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

 private:
  void NotifyTrackedOrderingChanged();
  void SetTrackedPermanentNodes();
  bool IsTrackedPermanentNode(const bookmarks::BookmarkNode* node) const;
  void ResetOrderingToDefault();
  bool ShouldTrackOrdering() const;
  size_t GetExpectedOrderingSize() const;
  std::vector<raw_ptr<const bookmarks::BookmarkNode>> GetDefaultOrderIfTracked()
      const;

  void RemoveBookmarkNodeIfTracked(const bookmarks::BookmarkNode* parent,
                                   size_t old_index,
                                   const bookmarks::BookmarkNode* node);

  void AddBookmarkNodeIfTracked(const bookmarks::BookmarkNode* parent,
                                size_t index);

  // This function counts bookmarks within the permanent bookmarks folder
  // tracked by this. If account_storage is true, it counts bookmarks whose
  // parent is account_node_. Otherwise, it counts bookmarks with a parent of
  // local_or_syncable_node_, considering only children indexed from 0 to index
  // - 1.
  size_t GetInStorageBookmarkCountBeforeIndex(bool account_storage,
                                              size_t index) const;

  void ReconcileLoadedNodeIds();

  const raw_ptr<bookmarks::BookmarkModel> model_;
  const bookmarks::BookmarkNode::Type tracked_type_;
  const raw_ptr<Delegate> delegate_;

  bool initialized_ = false;

  raw_ptr<const bookmarks::BookmarkNode> local_or_syncable_node_ = nullptr;
  raw_ptr<const bookmarks::BookmarkNode> account_node_ = nullptr;

  // Loaded node Ids from disk.
  // Only needed while the bookmark model is being loaded.
  std::vector<int64_t> loaded_node_ids_during_model_load_;

  // Non-empty if both `local_or_syncable_node_` and
  // `account_node_` have children (`ShouldTrackOrdering()` returns true).
  // If ordering is tracked, the size of the vector is the sum of direct
  // children of the `local_or_syncable_node_` and `account_node_` (as returned
  // by `GetExpectedOrderingSize()`).
  std::vector<raw_ptr<const bookmarks::BookmarkNode>> ordering_;

  bool all_user_bookmarks_remove_in_progress_ = false;

  base::ScopedObservation<bookmarks::BookmarkModel,
                          bookmarks::BookmarkModelObserver>
      model_observation_{this};
};

#endif  // CHROME_BROWSER_BOOKMARKS_PERMANENT_FOLDER_ORDERING_TRACKER_H_
