// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_MERGED_SURFACE_SERVICE_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_MERGED_SURFACE_SERVICE_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/keyed_service/core/keyed_service.h"

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

// Holds a `PermanentFolderType` or a non-permanent node folder `BookmarkNode`.
// `PermanentFolderType/ const BookmarkNode*` should be passed by value.
struct BookmarkParentFolder {
  // Represents a combined view of account and local bookmark permanent nodes.
  // Note: Managed node is not handled as part of this class.
  enum class PermanentFolderType { kBookmarkBarNode, kOtherNode, kMobileNode };

  static BookmarkParentFolder FromNonPermanentNode(
      const bookmarks::BookmarkNode* parent_node);

  static BookmarkParentFolder BookmarkBarFolder();
  static BookmarkParentFolder OtherFolder();
  static BookmarkParentFolder MobileFolder();

  ~BookmarkParentFolder();

  BookmarkParentFolder(const BookmarkParentFolder& other);
  BookmarkParentFolder& operator=(const BookmarkParentFolder& other);

  // Returns null if `this` is not permanent folder.
  std::optional<PermanentFolderType> as_permanent_folder() const;

  // Returns null if `this` is a permanent folder.
  const bookmarks::BookmarkNode* as_non_permanent_folder() const;

 private:
  explicit BookmarkParentFolder(
      std::variant<PermanentFolderType, raw_ptr<const bookmarks::BookmarkNode>>
          parent);

  bool HoldsNonPermanentFolder() const;

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
  explicit BookmarkMergedSurfaceService(bookmarks::BookmarkModel* model);
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

  const raw_ptr<bookmarks::BookmarkModel> model_;
};

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_MERGED_SURFACE_SERVICE_H_
