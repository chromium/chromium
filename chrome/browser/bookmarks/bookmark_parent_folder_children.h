// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_PARENT_FOLDER_CHILDREN_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_PARENT_FOLDER_CHILDREN_H_

#include <variant>

#include "base/memory/raw_ptr.h"

class PermanentFolderOrderingTracker;

namespace bookmarks {
class BookmarkNode;
}  // namespace bookmarks

// Provides access (not a copy) to the child nodes of a `BookmarkParentFolder`.
class BookmarkParentFolderChildren {
 public:
  class Iterator {
   public:
    Iterator(const BookmarkParentFolderChildren* parent, size_t index);
    Iterator& operator++();
    Iterator operator+(int offset) const;

    friend bool operator==(const Iterator&, const Iterator&) = default;
    friend auto operator<=>(const Iterator&, const Iterator&) = default;
    const bookmarks::BookmarkNode* operator*() const;

   private:
    raw_ptr<const BookmarkParentFolderChildren> parent_;
    size_t index_;
  };

  // `node` must be not null and outlive this.
  // All permanent nodes except the managed node are not accepted in this
  // constructor as the `PermanentFolderOrderingTracker` is required.
  explicit BookmarkParentFolderChildren(const bookmarks::BookmarkNode* node);

  // `tracker` must be not null and outlive this.
  explicit BookmarkParentFolderChildren(
      const PermanentFolderOrderingTracker* tracker);

  ~BookmarkParentFolderChildren();

  BookmarkParentFolderChildren(const BookmarkParentFolderChildren&) = delete;
  BookmarkParentFolderChildren& operator=(const BookmarkParentFolderChildren&) =
      delete;

  // Iterator methods to support range-based for loops.
  Iterator begin() const;
  Iterator end() const;

  const bookmarks::BookmarkNode* operator[](size_t index) const;

  size_t size() const;

 private:
  const std::variant<raw_ptr<const bookmarks::BookmarkNode>,
                     raw_ptr<const PermanentFolderOrderingTracker>>
      children_provider_;
};

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_PARENT_FOLDER_CHILDREN_H_
