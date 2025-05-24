// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_PARENT_FOLDER_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_PARENT_FOLDER_H_

#include <optional>
#include <variant>

#include "base/memory/raw_ptr.h"

namespace bookmarks {
class BookmarkNode;
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

  friend auto operator<=>(const BookmarkParentFolder&,
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

  // Returns true if this == ancestor, or one of this folder's parents is
  // ancestor.
  bool HasAncestor(const BookmarkParentFolder& ancestor) const;

 private:
  explicit BookmarkParentFolder(
      std::variant<PermanentFolderType, raw_ptr<const bookmarks::BookmarkNode>>
          parent);

  std::variant<PermanentFolderType, raw_ptr<const bookmarks::BookmarkNode>>
      bookmark_;
};

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_PARENT_FOLDER_H_
