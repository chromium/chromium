// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_BOOKMARKS_BOOKMARKS_HELPERS_H_
#define CHROME_BROWSER_EXTENSIONS_BOOKMARKS_BOOKMARKS_HELPERS_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "chrome/common/extensions/api/bookmarks.h"

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
class ManagedBookmarkService;
}  // namespace bookmarks

// Helper functions.
namespace extensions {
namespace bookmarks_helpers {

api::bookmarks::BookmarkTreeNode GetBookmarkTreeNode(
    bookmarks::BookmarkModel* model,
    bookmarks::ManagedBookmarkService* managed,
    const bookmarks::BookmarkNode* node,
    bool recurse,
    bool only_folders);

// Populates |out_bookmark_tree_node| with given |node|.
void PopulateBookmarkTreeNode(
    bookmarks::BookmarkModel* model,
    bookmarks::ManagedBookmarkService* managed,
    const bookmarks::BookmarkNode* node,
    bool recurse,
    bool only_folders,
    api::bookmarks::BookmarkTreeNode* out_bookmark_tree_node);

// Adds a JSON representation of |node| to the JSON |nodes|.
void AddNode(bookmarks::BookmarkModel* model,
             bookmarks::ManagedBookmarkService* managed,
             const bookmarks::BookmarkNode* node,
             std::vector<api::bookmarks::BookmarkTreeNode>* nodes,
             bool recurse);

// Adds a JSON representation of |node| of folder type to the JSON |nodes|.
void AddNodeFoldersOnly(bookmarks::BookmarkModel* model,
                        bookmarks::ManagedBookmarkService* managed,
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

// Return the API index of |node| among its siblings on the extensions API.
//
// Nodes which are not visible (see `BookmarkModel::IsNodeVisible()`) are not
// exposed on the extensions API. This means that the child indices of nodes as
// viewed on the extensions API differ from the BookmarkModel. For example:
//
// Model:
// - Root
//   - [0] A (visible)
//   - [1] B (not visible)
//   - [2] C (visible)
//
// Extensions API:
// - Root
//   - [0] A
//   - [1] C
size_t GetAPIIndexOf(const bookmarks::BookmarkModel& model,
                     const bookmarks::BookmarkNode& node);

}  // namespace bookmarks_helpers
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_BOOKMARKS_BOOKMARKS_HELPERS_H_
