// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_MERGED_SURFACE_SERVICE_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_MERGED_SURFACE_SERVICE_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "components/keyed_service/core/keyed_service.h"

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

  static BookmarkParentFolder FromNonPermanentNode(
      const bookmarks::BookmarkNode* parent_node);

  static BookmarkParentFolder BookmarkBarFolder();
  static BookmarkParentFolder OtherFolder();
  static BookmarkParentFolder MobileFolder();
  static BookmarkParentFolder ManagedFolder();

  ~BookmarkParentFolder();

  BookmarkParentFolder(const BookmarkParentFolder& other);
  BookmarkParentFolder& operator=(const BookmarkParentFolder& other);

  // Returns `true` if `this` hols a non-permanent folder.
  bool HoldsNonPermanentFolder() const;

  // Returns null if `this` is not a permanent folder.
  std::optional<PermanentFolderType> as_permanent_folder() const;

  // Returns null if `this` is a permanent folder.
  const bookmarks::BookmarkNode* as_non_permanent_folder() const;

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

  // Returns true if `node` is of equivalent type to permanent `folder`.
  static bool IsPermanentNodeOfType(
      const bookmarks::BookmarkNode* node,
      BookmarkParentFolder::PermanentFolderType folder);

  bool loaded() const;

  size_t GetChildrenCount(const BookmarkParentFolder& bookmark) const;

  // Moves `node` to `new_parent` at position `index`.
  // Note: If `BookmarkParentFolder` is a permanent bookmark folder, `index` is
  // expected to be the position across storages. This can result in a move
  // operation within the local/account storage and within the
  // `BookmarkPermanentFolderOrderingTracker`.
  void Move(const bookmarks::BookmarkNode* node,
            const BookmarkParentFolder& new_parent,
            size_t index);

  bookmarks::BookmarkModel* bookmark_model() { return model_; }

 private:
  // TODO(crbug.com/364594278): This function will be replaced once this class
  // supports account nodes.
  const bookmarks::BookmarkNode* PermanentFolderToNode(
      BookmarkParentFolder::PermanentFolderType folder) const;

  const bookmarks::BookmarkNode* managed_permanent_node() const;

  const raw_ptr<bookmarks::BookmarkModel> model_;
  const raw_ptr<bookmarks::ManagedBookmarkService> managed_bookmark_service_;
};

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_MERGED_SURFACE_SERVICE_H_
