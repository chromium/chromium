// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_MERGED_SURFACE_SERVICE_OBSERVER_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_MERGED_SURFACE_SERVICE_OBSERVER_H_

#include <cstddef>

#include "base/containers/flat_map.h"
#include "base/observer_list_types.h"

struct BookmarkParentFolder;

namespace bookmarks {
class BookmarkNode;
}  // namespace bookmarks

class BookmarkMergedSurfaceServiceObserver : public base::CheckedObserver {
 public:
  ~BookmarkMergedSurfaceServiceObserver() override = default;

  virtual void BookmarkNodesRemoved(
      const BookmarkParentFolder& parent,
      const base::flat_map<size_t, raw_ptr<const bookmarks::BookmarkNode>>&
          old_indices_to_nodes) = 0;
};

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_MERGED_SURFACE_SERVICE_OBSERVER_H_
