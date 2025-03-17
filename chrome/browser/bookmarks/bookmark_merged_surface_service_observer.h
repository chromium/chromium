// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_MERGED_SURFACE_SERVICE_OBSERVER_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_MERGED_SURFACE_SERVICE_OBSERVER_H_

#include <cstddef>

#include "base/containers/flat_set.h"
#include "base/observer_list_types.h"

struct BookmarkParentFolder;

namespace bookmarks {
class BookmarkNode;
}  // namespace bookmarks

class BookmarkMergedSurfaceServiceObserver : public base::CheckedObserver {
 public:
  ~BookmarkMergedSurfaceServiceObserver() override = default;

  // Invoked when the service has finished loading.
  virtual void BookmarkMergedSurfaceServiceLoaded() = 0;

  // Invoked from the destructor of the `BookmarkMergedSurfaceService`.
  virtual void BookmarkMergedSurfaceServiceBeingDeleted() {}

  // Invoked when a node has been added. If the added node has any descendants,
  // `BookmarkNodeAdded` will be invoked for all of the descendants.
  virtual void BookmarkNodeAdded(const BookmarkParentFolder& parent,
                                 size_t index) = 0;

  virtual void BookmarkNodesRemoved(
      const BookmarkParentFolder& parent,
      const base::flat_set<const bookmarks::BookmarkNode*>& nodes) = 0;

  virtual void BookmarkNodeMoved(const BookmarkParentFolder& old_parent,
                                 size_t old_index,
                                 const BookmarkParentFolder& new_parent,
                                 size_t new_index) = 0;

  // Invoked when the title or url of a node changes.
  virtual void BookmarkNodeChanged(const bookmarks::BookmarkNode* node) = 0;

  // Invoked when a favicon has been loaded or changed.
  virtual void BookmarkNodeFaviconChanged(
      const bookmarks::BookmarkNode* node) = 0;

  // Invoked when the children (just direct children, not descendants) of
  // `folder` have been reordered in some way, such as sorted.
  virtual void BookmarkParentFolderChildrenReordered(
      const BookmarkParentFolder& folder) = 0;

  // Invoked when all non-permanent bookmark nodes that are editable by the
  // user have been removed.
  virtual void BookmarkAllUserNodesRemoved() = 0;
};

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_MERGED_SURFACE_SERVICE_OBSERVER_H_
