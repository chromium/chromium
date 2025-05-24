// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_MERGED_SURFACE_SERVICE_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_MERGED_SURFACE_SERVICE_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_ordering_storage.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service_observer.h"
#include "chrome/browser/bookmarks/bookmark_parent_folder.h"
#include "chrome/browser/bookmarks/bookmark_parent_folder_children.h"
#include "chrome/browser/bookmarks/permanent_folder_ordering_tracker.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_node_data.h"
#include "components/keyed_service/core/keyed_service.h"

class Browser;

namespace base {
class FilePath;
}  // namespace base

namespace bookmarks {
class BookmarkModel;
class ManagedBookmarkService;
}  // namespace bookmarks

// Used in UI surfaces that combines local and account bookmarks in a merged
// view.
// It maintains the order between local and account bookmark children
// nodes of permanent bookmark nodes.
// Merged UI surfaces should use this class for bookmark operations.
class BookmarkMergedSurfaceService
    : public KeyedService,
      public bookmarks::BookmarkModelObserver,
      public PermanentFolderOrderingTracker::Delegate {
 public:
  // `model` must not be null and must outlive this object.
  // `managed_bookmark_service` may be null.
  BookmarkMergedSurfaceService(
      bookmarks::BookmarkModel* model,
      bookmarks::ManagedBookmarkService* managed_bookmark_service);
  ~BookmarkMergedSurfaceService() override;

  BookmarkMergedSurfaceService(const BookmarkMergedSurfaceService&) = delete;
  BookmarkMergedSurfaceService& operator=(const BookmarkMergedSurfaceService&) =
      delete;

  // Must be called.
  // Triggers the loading of bookmarks ordering, which is an asynchronous
  // operation with most heavy-lifting taking place in a background sequence.
  void Load(const base::FilePath& profile_path);

  // Returns underlying nodes in `folder`. This is either:
  // - a single bookmark folder node or
  // - two permanent folder nodes representing local and account bookmark nodes
  //   of `*folder.as_permanent_folder()`.
  std::vector<const bookmarks::BookmarkNode*> GetUnderlyingNodes(
      const BookmarkParentFolder& folder) const;

  // Returns the index of node with respect to its parent folder.
  // `node` must not be null.
  size_t GetIndexOf(const bookmarks::BookmarkNode* node) const;

  // `index` must be less than the folder's children count.
  const bookmarks::BookmarkNode* GetNodeAtIndex(
      const BookmarkParentFolder& folder,
      size_t index) const;

  bool loaded() const;

  // Note: In case of managed folder, if `managed_permanent_node()` is null,
  // this will return `0`.
  size_t GetChildrenCount(const BookmarkParentFolder& folder) const;

  // `this` must outlive `BookmarkParentFolderChildren`.
  // Note: In case of managed folder, if `managed_permanent_node()` is null,
  // this will return empty children.
  BookmarkParentFolderChildren GetChildren(
      const BookmarkParentFolder& folder) const;

  // Returns default parent node for new nodes created in `folder`.
  // In case of permanent folder, this will return the account node if one
  // exists, otherwise it returns the local/syncable node.
  // The bookmark model must be loaded prior to calling this function.
  // Note: `folder` must be not managed, as the user adding nodes to the managed
  // folder is not allowed.
  const bookmarks::BookmarkNode* GetDefaultParentForNewNodes(
      const BookmarkParentFolder& folder) const;

  // Returns the node encapuslated in `managed_folder`.
  // The bookmark model must be loaded prior to calling this function.
  // Note: `managed_folder` should be a managed folder. This function should
  // only be used to determine the parent of existing managed nodes in a merged
  // surfaces, and not for adding new nodes.
  const bookmarks::BookmarkNode* GetParentForManagedNode(
      const BookmarkParentFolder& managed_folder) const;

  // Moves `node` to `new_parent` at position `index`.
  // If `BookmarkParentFolder` is a permanent bookmark folder:
  // - `index` is expected to be the position across storages.
  // - The node is moved to the account node in `GetUnderlyingNodes()` if `node`
  //   is an account node or to the local underlying node if the node is local.
  // - This can result in a move operation within the local/account storage and
  //   within the `BookmarkPermanentFolderOrderingTracker`.
  // Note: There are two possible target indices (index) that result in a no-op.
  //   This is similar to what `BookmarkModel::Move()` does.
  // - If the storages of `node` and `new_parent` don't match, the user will be
  //   asked to confirm their choice first before moving the bookmark. For this,
  //   a `browser` is needed.
  void Move(const bookmarks::BookmarkNode* node,
            const BookmarkParentFolder& new_parent,
            size_t index,
            Browser* browser);

  // Copies nodes in `elements` to be new child nodes of `new_parent` starting
  // at `index`. If `BookmarkParentFolder` is a permanent bookmark folder,
  // `index` is expected to be the position across storages. The new nodes will
  // be child nodes of `GetDefaultParentForNewNodes()` for the given
  // `new_parent` folder.
  void AddNodesAsCopiesOfNodeData(
      const std::vector<bookmarks::BookmarkNodeData::Element>& elements,
      const BookmarkParentFolder& new_parent,
      size_t index);

  // Returns true if `folder` is a permanent folder with custom order tracked
  // (default order : account child nodes followed by local child nodes).
  bool IsNonDefaultOrderingTracked(const BookmarkParentFolder& folder) const;

  // Returns true if `parent` is managed.
  bool IsParentFolderManaged(const BookmarkParentFolder& folder) const;

  // Returns true if `parent` is managed.
  bool IsNodeManaged(const bookmarks::BookmarkNode* node) const;

  bookmarks::BookmarkModel* bookmark_model() { return model_; }
  const bookmarks::BookmarkModel* bookmark_model() const { return model_; }
  const bookmarks::ManagedBookmarkService* managed_bookmark_service() const {
    return managed_bookmark_service_;
  }

  // Must be called for trackers to be initialized.
  // `BookmarkModel` also must complete loading for this to complete loading.
  // Resets any ongoing load operation.
  void LoadForTesting(
      BookmarkMergedSurfaceOrderingStorage::Loader::LoadResult result);

  using ShowMoveStorageDialogCallback =
      base::RepeatingCallback<void(Browser* browser,
                                   const bookmarks::BookmarkNode* node,
                                   const bookmarks::BookmarkNode* target_node,
                                   size_t index)>;
  void SetShowMoveStorageDialogCallbackForTesting(
      ShowMoveStorageDialogCallback show_move_storage_dialog_for_testing);

  void AddObserver(BookmarkMergedSurfaceServiceObserver* observer);
  void RemoveObserver(BookmarkMergedSurfaceServiceObserver* observer);

  // bookmarks::BookmarkModelObserver:
  void BookmarkModelLoaded(bool ids_reassigned) override;
  void OnWillMoveBookmarkNode(const bookmarks::BookmarkNode* old_parent,
                              size_t old_index,
                              const bookmarks::BookmarkNode* new_parent,
                              size_t new_index) override;
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
                           const std::set<GURL>& no_longer_bookmarked,
                           const base::Location& location) override;
  void BookmarkNodeChanged(const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeFaviconChanged(const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeChildrenReordered(
      const bookmarks::BookmarkNode* node) override;
  void BookmarkAllUserNodesRemoved(const std::set<GURL>& removed_urls,
                                   const base::Location& location) override;

  // PermanentFolderOrderingTracker::Delegate:
  void TrackedOrderingChanged() override;

 private:
  class BookmarkModelLoadedObserver;

  void OnLoadOrderingComplete(
      BookmarkMergedSurfaceOrderingStorage::Loader::LoadResult result);
  void NotifyLoaded();

  const bookmarks::BookmarkNode* managed_permanent_node() const;

  const PermanentFolderOrderingTracker& GetPermanentFolderOrderingTracker(
      BookmarkParentFolder::PermanentFolderType folder_type) const;

  PermanentFolderOrderingTracker& GetPermanentFolderOrderingTracker(
      BookmarkParentFolder::PermanentFolderType folder_type);

  // Helper function for optimization purposes.
  // Returns `in_storage_index` if node is not tracked in any of
  // `PermanentFolderOrderingTracker` or it is tracked but the ordering is not
  // tracked based on `PermanentFolderOrderingTracker::ShouldTrackOrdering()`.
  // Otherwise, returns `PermanentFolderOrderingTracker::GetIndexOf(node)`.
  size_t GetIndexAcrossStorage(const bookmarks::BookmarkNode* node,
                               size_t in_storage_index) const;

  void NotifyBookmarkNodeAddedForAllDescendants(
      const bookmarks::BookmarkNode* node);

  const raw_ptr<bookmarks::BookmarkModel> model_;
  const raw_ptr<bookmarks::ManagedBookmarkService> managed_bookmark_service_;
  const base::flat_map<BookmarkParentFolder::PermanentFolderType,
                       std::unique_ptr<PermanentFolderOrderingTracker>>
      permanent_folder_to_tracker_;

  // Used in `GetChildren()` to return empty when managed node is null.
  const bookmarks::BookmarkNode dummy_empty_node_;

  bool load_ordering_completed_ = false;
  // Not null during load.
  std::unique_ptr<BookmarkMergedSurfaceOrderingStorage::Loader> loader_;
  // Needed while loading ordering from disk has not completed to catch if
  // `ids_reassigned`. The full observer must be added after permanent folder
  // trackers are initialized.
  std::unique_ptr<BookmarkModelLoadedObserver> model_loaded_observer_;

  std::unique_ptr<BookmarkMergedSurfaceOrderingStorage> storage_;

  ShowMoveStorageDialogCallback show_move_storage_dialog_for_testing_;

  // Non-empty in the middle of moving a bookmark node.
  // It is set in `OnWillMoveBookmarkNode()` and cleared in
  // `BookmarkNodeMoved()`.
  std::optional<std::pair<size_t, raw_ptr<const bookmarks::BookmarkNode>>>
      cached_index_for_node_move_;

  // The service is making a move using `Move()`.
  // Delay `BookmarkModel` notifications as the index could change as a result
  // of custom reorder between account and local nodes.
  bool scoped_move_change_ = false;

  // The service is adding new nodes through `AddNodesAsCopiesOfNodeData()`.
  // Delay `BookmarkModel` notification as the index could change as a result
  // of custom reorder between account and local nodes.
  bool scoped_add_new_nodes_ = false;

  base::ObserverList<BookmarkMergedSurfaceServiceObserver> observers_;

  base::ScopedObservation<bookmarks::BookmarkModel,
                          bookmarks::BookmarkModelObserver>
      model_observation_{this};
};

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_MERGED_SURFACE_SERVICE_H_
