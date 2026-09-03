// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_NODE_TYPES_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_NODE_TYPES_H_

#include <stdint.h>

#include <variant>

namespace bookmarks {

// Represents a combined view of account and local bookmark permanent nodes.
// Note: Managed node is an exception as it has only local data.
enum class PermanentFolderType {
  kBookmarkBarNode,
  kOtherNode,
  kMobileNode,
  kManagedNode
};

// Represents an identifier for a bookmark node/surface across UI boundaries:
// - PermanentFolderType: For merged permanent roots (Bookmark Bar, Other, etc.)
// - int64_t: For specific native bookmark nodes (folders or URLs).
using BookmarkNodeId = std::variant<PermanentFolderType, int64_t>;

}  // namespace bookmarks

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_NODE_TYPES_H_
