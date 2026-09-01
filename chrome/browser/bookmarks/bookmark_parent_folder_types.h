// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_PARENT_FOLDER_TYPES_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_PARENT_FOLDER_TYPES_H_

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

}  // namespace bookmarks

namespace bookmarks_api {

using BookmarkParentFolderId =
    std::variant<bookmarks::PermanentFolderType, int64_t>;

}  // namespace bookmarks_api

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_PARENT_FOLDER_TYPES_H_
