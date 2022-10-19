// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_BOOKMARKS_BOOKMARK_API_HELPERS_H_
#define CHROME_BROWSER_EXTENSIONS_API_BOOKMARKS_BOOKMARK_API_HELPERS_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "chrome/common/extensions/api/bookmarks.h"

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
class ManagedBookmarkService;
}

// Helper functions.
namespace extensions {
namespace bookmark_api_helpers {

api::bookmarks::BookmarkTreeNode GetBookmarkTreeNode(
    bookmarks::ManagedBookmarkService* managed,
    const bookmarks::BookmarkNode* node,
    bool recurse,
    bool only_folders);

// Populates |out_bookmark_tree_node| with given |node|.
void PopulateBookmarkTreeNode(
    bookmarks::ManagedBookmarkService* managed,
    const bookmarks::BookmarkNode* node,
    bool recurse,
    bool only_folders,
    api::bookmarks::BookmarkTreeNode* out_bookmark_tree_node);

// Adds a JSON representation of |node| to the JSON |nodes|.
void AddNode(bookmarks::ManagedBookmarkService* managed,
             const bookmarks::BookmarkNode* node,
             std::vector<api::bookmarks::BookmarkTreeNode>* nodes,
             bool recurse);

// Adds a JSON representation of |node| of folder type to the JSON |nodes|.
void AddNodeFoldersOnly(bookmarks::ManagedBookmarkService* managed,
                        const bookmarks::BookmarkNode* node,
                        std::vector<api::bookmarks::BookmarkTreeNode>* nodes,
                        bool recurse);

// Remove node of |id|.
bool RemoveNode(bookmarks::BookmarkModel* model,
                bookmarks::ManagedBookmarkService* managed,
                int64_t id,
                bool recursive,
                std::string* error);

// Get meta info from |node| and all it's children recursively.
void GetMetaInfo(const bookmarks::BookmarkNode& node,
                 base::Value::Dict& id_to_meta_info_map);

}  // namespace bookmark_api_helpers
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_BOOKMARKS_BOOKMARK_API_HELPERS_H_
