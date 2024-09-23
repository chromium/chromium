// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/bookmarks/bookmark_api_helpers.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/bookmarks/bookmarks_error_constants.h"
#include "chrome/common/extensions/api/bookmarks.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace extensions {

using api::bookmarks::BookmarkTreeNode;

namespace bookmark_api_helpers {

namespace {

void AddNodeHelper(bookmarks::ManagedBookmarkService* managed,
                   const BookmarkNode* node,
                   std::vector<BookmarkTreeNode>* nodes,
                   bool recurse,
                   bool only_folders) {
  if (node->IsVisible())
    nodes->push_back(GetBookmarkTreeNode(managed, node, recurse, only_folders));
}

}  // namespace

BookmarkTreeNode GetBookmarkTreeNode(bookmarks::ManagedBookmarkService* managed,
                                     const BookmarkNode* node,
                                     bool recurse,
                                     bool only_folders) {
  BookmarkTreeNode bookmark_tree_node;
  PopulateBookmarkTreeNode(managed, node, recurse, only_folders,
                           &bookmark_tree_node);
  return bookmark_tree_node;
}

void PopulateBookmarkTreeNode(
    bookmarks::ManagedBookmarkService* managed,
    const bookmarks::BookmarkNode* node,
    bool recurse,
    bool only_folders,
    api::bookmarks::BookmarkTreeNode* out_bookmark_tree_node) {
  DCHECK(out_bookmark_tree_node);

  out_bookmark_tree_node->id = base::NumberToString(node->id());

  const BookmarkNode* parent = node->parent();
  if (parent) {
    out_bookmark_tree_node->parent_id = base::NumberToString(parent->id());
    out_bookmark_tree_node->index =
        static_cast<int>(parent->GetIndexOf(node).value());
  }

  if (!node->is_folder()) {
    out_bookmark_tree_node->url = node->url().spec();
    base::Time t = node->date_last_used();
    if (!t.is_null()) {
      out_bookmark_tree_node->date_last_used = t.InMillisecondsSinceUnixEpoch();
    }
  } else {
    base::Time t = node->date_folder_modified();
    if (!t.is_null()) {
      out_bookmark_tree_node->date_group_modified =
          t.InMillisecondsSinceUnixEpoch();
    }
  }

  out_bookmark_tree_node->title = base::UTF16ToUTF8(node->GetTitle());
  if (!node->date_added().is_null()) {
    out_bookmark_tree_node->date_added =
        node->date_added().InMillisecondsSinceUnixEpoch();
  }

  if (bookmarks::IsDescendantOf(node, managed->managed_node())) {
    out_bookmark_tree_node->unmodifiable =
        api::bookmarks::BookmarkTreeNodeUnmodifiable::kManaged;
  }

  if (recurse && node->is_folder()) {
    std::vector<BookmarkTreeNode> children;
    for (const auto& child : node->children()) {
      if (child->IsVisible() && (!only_folders || child->is_folder())) {
        children.push_back(
            GetBookmarkTreeNode(managed, child.get(), true, only_folders));
      }
    }
    out_bookmark_tree_node->children = std::move(children);
  }
}

void AddNode(bookmarks::ManagedBookmarkService* managed,
             const BookmarkNode* node,
             std::vector<BookmarkTreeNode>* nodes,
             bool recurse) {
  return AddNodeHelper(managed, node, nodes, recurse, false);
}

void AddNodeFoldersOnly(bookmarks::ManagedBookmarkService* managed,
                        const BookmarkNode* node,
                        std::vector<BookmarkTreeNode>* nodes,
                        bool recurse) {
  return AddNodeHelper(managed, node, nodes, recurse, true);
}

bool RemoveNode(BookmarkModel* model,
                bookmarks::ManagedBookmarkService* managed,
                int64_t id,
                bool recursive,
                std::string* error) {
  const BookmarkNode* node = bookmarks::GetBookmarkNodeByID(model, id);
  if (!node) {
    *error = bookmarks_errors::kNoNodeError;
    return false;
  }
  if (model->is_permanent_node(node)) {
    *error = bookmarks_errors::kModifySpecialError;
    return false;
  }
  if (bookmarks::IsDescendantOf(node, managed->managed_node())) {
    *error = bookmarks_errors::kModifyManagedError;
    return false;
  }
  if (node->is_folder() && !node->children().empty() && !recursive) {
    *error = bookmarks_errors::kFolderNotEmptyError;
    return false;
  }

  model->Remove(node, bookmarks::metrics::BookmarkEditSource::kExtension,
                FROM_HERE);
  return true;
}

void GetMetaInfo(const BookmarkNode& node,
                 base::Value::Dict& id_to_meta_info_map) {
  if (!node.IsVisible())
    return;

  const BookmarkNode::MetaInfoMap* meta_info = node.GetMetaInfoMap();
  base::Value::Dict value;
  if (meta_info) {
    BookmarkNode::MetaInfoMap::const_iterator itr;
    for (itr = meta_info->begin(); itr != meta_info->end(); ++itr) {
      value.Set(itr->first, itr->second);
    }
  }
  id_to_meta_info_map.Set(base::NumberToString(node.id()), std::move(value));

  if (node.is_folder()) {
    for (const auto& child : node.children())
      GetMetaInfo(*child, id_to_meta_info_map);
  }
}

}  // namespace bookmark_api_helpers
}  // namespace extensions
