// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_BOOKMARKS_BOOKMARKS_HELPERS_H_
#define CHROME_BROWSER_EXTENSIONS_BOOKMARKS_BOOKMARKS_HELPERS_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "chrome/common/extensions/api/bookmarks.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

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
    bool only_folders,
    std::optional<size_t> visible_index = std::nullopt);

// Populates `out_bookmark_tree_node` with given `node`.
// `visible_index` is the index position of this node among only the visible
// siblings in its parent folder, if not provided, will use GetAPIIndexOf() to
// calculate the index.
void PopulateBookmarkTreeNode(
    bookmarks::BookmarkModel* model,
    bookmarks::ManagedBookmarkService* managed,
    const bookmarks::BookmarkNode* node,
    bool recurse,
    bool only_folders,
    std::optional<size_t> visible_index,
    api::bookmarks::BookmarkTreeNode* out_bookmark_tree_node);

// Adds a JSON representation of `node` to the JSON `nodes`.
void AddNode(bookmarks::BookmarkModel* model,
             bookmarks::ManagedBookmarkService* managed,
             const bookmarks::BookmarkNode* node,
             std::vector<api::bookmarks::BookmarkTreeNode>* nodes,
             bool recurse);

// Adds a JSON representation of `node` of folder type to the JSON `nodes`.
void AddNodeFoldersOnly(bookmarks::BookmarkModel* model,
                        bookmarks::ManagedBookmarkService* managed,
                        const bookmarks::BookmarkNode* node,
                        std::vector<api::bookmarks::BookmarkTreeNode>* nodes,
                        bool recurse);

// Remove node of `id`.
bool RemoveNode(bookmarks::BookmarkModel* model,
                bookmarks::ManagedBookmarkService* managed,
                int64_t id,
                bool recursive,
                std::string* error);

// Get meta info from `node` and all it's children recursively.
void GetMetaInfo(const bookmarks::BookmarkNode& node,
                 base::Value::Dict& id_to_meta_info_map);

// Return the API index of `node` among its siblings on the extensions API.
//
// Nodes which are not visible (see `BookmarkNode::IsVisible()`) are not
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
size_t GetAPIIndexOf(const bookmarks::BookmarkNode& node);

// Similar to GetAPIIndexOf(node), but for the case where the node has already
// been removed from its parent.
// The parent and previous model index of the node are provided.
size_t GetAPIIndexOf(const bookmarks::BookmarkNode& parent,
                     size_t previous_model_index);

}  // namespace bookmarks_helpers
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_BOOKMARKS_BOOKMARKS_HELPERS_H_
