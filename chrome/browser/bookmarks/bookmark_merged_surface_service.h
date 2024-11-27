// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_MERGED_SURFACE_SERVICE_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_MERGED_SURFACE_SERVICE_H_

#include <variant>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "components/bookmarks/browser/bookmark_node_data.h"
#include "components/keyed_service/core/keyed_service.h"

class PermanentFolderOrderingTracker;

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
class ManagedBookmarkService;
}  // namespace bookmarks

// Holds a `PermanentFolderType` or a non-permanent node folder `BookmarkNode`.
// `PermanentFolderType/ const BookmarkNode*` should be passed by value.
struct BookmarkParentFolder {
  // Represents a combined view of account and local bookmark permanent nodes.
  // Note: Managed node is an exception as it has only local data.
  enum class PermanentFolderType {
    kBookmarkBarNode,
    kOtherNode,
    kMobileNode,
    kManagedNode
  };

  static BookmarkParentFolder BookmarkBarFolder();
  static BookmarkParentFolder OtherFolder();
  static BookmarkParentFolder MobileFolder();
  static BookmarkParentFolder ManagedFolder();

  // `node` must be not null, not root node and it must be a folder.
  static BookmarkParentFolder FromFolderNode(
      const bookmarks::BookmarkNode* node);

  ~BookmarkParentFolder();

  BookmarkParentFolder(const BookmarkParentFolder& other);
  BookmarkParentFolder& operator=(const BookmarkParentFolder& other);

  friend bool operator==(const BookmarkParentFolder&,
                         const BookmarkParentFolder&) = default;

  // Returns `true` if `this` hols a non-permanent folder.
  bool HoldsNonPermanentFolder() const;

  // Returns null if `this` is not a permanent folder.
  std::optional<PermanentFolderType> as_permanent_folder() const;

  // Returns null if `this` is a permanent folder.
  const bookmarks::BookmarkNode* as_non_permanent_folder() const;

  // Returns true if `node` is a direct child of `this`.
  // `node` must not be null.
  bool HasDirectChildNode(const bookmarks::BookmarkNode* node) const;

 private:
  explicit BookmarkParentFolder(
      std::variant<PermanentFolderType, raw_ptr<const bookmarks::BookmarkNode>>
          parent);

  std::variant<PermanentFolderType, raw_ptr<const bookmarks::BookmarkNode>>
      bookmark_;
};

// Used in UI surfaces that combines local and account bookmarks in a merged
// view.
// It maintains the order between local and account bookmark children
// nodes of permanent bookmark nodes.
// Merged UI surfaces should use this class for bookmark operations.
// TODO(crbug.com/364594278): This class is under development, it currently only
// handles `NodeTypeForUuidLookup::kLocalOrSyncableNodes`.
class BookmarkMergedSurfaceService : public KeyedService {
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

  // Returns underlying nodes in `folder`. This is either:
  // - a single bookmark folder node or
  // - two permanent folder nodes representing local and account bookmark nodes
  //   of `*folder.as_permanent_folder()` in the following order:
  //   (1) the account node if one exists
  //   (2) then the local or syncable node.
  std::vector<const bookmarks::BookmarkNode*> GetUnderlyingNodes(
      const BookmarkParentFolder& folder) const;

  // Returns the index of node with respect to its parent.
  // `node` must not be null.
  // TODO(crbug.com/364594278): When account nodes are supported within this
  // class, `this` will return the index within `BookmarkParentFolder`.
  size_t GetIndexOf(const bookmarks::BookmarkNode* node) const;

  const bookmarks::BookmarkNode* GetNodeAtIndex(
      const BookmarkParentFolder& folder,
      size_t index) const;

  bool loaded() const;

  size_t GetChildrenCount(const BookmarkParentFolder& folder) const;

  // TODO(crbug.com/364594278): This function will need to return a wrapper that
  // provides access to children as in some cases, child nodes will be a
  // combination of the two bookmark nodes's children that would be tracked in a
  // vector of non-owning pointers to child bookmark nodes. The wrapper would
  // hide the type difference vector of unique/raw pointers.
  const std::vector<std::unique_ptr<bookmarks::BookmarkNode>>& GetChildren(
      const BookmarkParentFolder& folder) const;

  // Moves `node` to `new_parent` at position `index`.
  // Note: If `BookmarkParentFolder` is a permanent bookmark folder, `index` is
  // expected to be the position across storages. This can result in a move
  // operation within the local/account storage and within the
  // `BookmarkPermanentFolderOrderingTracker`.
  void Move(const bookmarks::BookmarkNode* node,
            const BookmarkParentFolder& new_parent,
            size_t index);

  // Same as `Copy()` but copies `element`.
  void CopyBookmarkNodeDataElement(
      const bookmarks::BookmarkNodeData::Element& element,
      const BookmarkParentFolder& new_parent,
      size_t index);

  // Returns true if `parent` is managed.
  bool IsParentFolderManaged(const BookmarkParentFolder& folder) const;

  // Returns true if `parent` is managed.
  bool IsNodeManaged(const bookmarks::BookmarkNode* node) const;

  bookmarks::BookmarkModel* bookmark_model() { return model_; }

 private:
  // TODO(crbug.com/364594278): This function will be replaced once this class
  // supports account nodes.
  const bookmarks::BookmarkNode* PermanentFolderToNode(
      BookmarkParentFolder::PermanentFolderType folder) const;

  const bookmarks::BookmarkNode* managed_permanent_node() const;

  const PermanentFolderOrderingTracker& GetPermanentFolderOrderingTracker(
      BookmarkParentFolder::PermanentFolderType folder_type) const;

  const raw_ptr<bookmarks::BookmarkModel> model_;
  const raw_ptr<bookmarks::ManagedBookmarkService> managed_bookmark_service_;
  const base::flat_map<BookmarkParentFolder::PermanentFolderType,
                       std::unique_ptr<PermanentFolderOrderingTracker>>
      permanent_folder_to_tracker_;
};

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_MERGED_SURFACE_SERVICE_H_
