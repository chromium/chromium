// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/bookmarks/bookmark_api_helpers.h"

#include <math.h>  // For floor()

#include <memory>
#include <utility>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/bookmarks/bookmark_api_constants.h"
#include "chrome/common/extensions/api/bookmarks.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
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
    out_bookmark_tree_node->parent_id.reset(
        new std::string(base::NumberToString(parent->id())));
    out_bookmark_tree_node->index.reset(new int(parent->GetIndexOf(node)));
  }

  if (!node->is_folder()) {
    out_bookmark_tree_node->url.reset(new std::string(node->url().spec()));
  } else {
    // Javascript Date wants milliseconds since the epoch, ToDoubleT is seconds.
    base::Time t = node->date_folder_modified();
    if (!t.is_null()) {
      out_bookmark_tree_node->date_group_modified.reset(
          new double(floor(t.ToDoubleT() * 1000)));
    }
  }

  out_bookmark_tree_node->title = base::UTF16ToUTF8(node->GetTitle());
  if (!node->date_added().is_null()) {
    // Javascript Date wants milliseconds since the epoch, ToDoubleT is seconds.
    out_bookmark_tree_node->date_added.reset(
        new double(floor(node->date_added().ToDoubleT() * 1000)));
  }

  if (bookmarks::IsDescendantOf(node, managed->managed_node())) {
    out_bookmark_tree_node->unmodifiable =
        api::bookmarks::BOOKMARK_TREE_NODE_UNMODIFIABLE_MANAGED;
  }

  if (recurse && node->is_folder()) {
    std::vector<BookmarkTreeNode> children;
    for (const auto& child : node->children()) {
      if (child->IsVisible() && (!only_folders || child->is_folder())) {
        children.push_back(
            GetBookmarkTreeNode(managed, child.get(), true, only_folders));
      }
    }
    out_bookmark_tree_node->children.reset(
        new std::vector<BookmarkTreeNode>(std::move(children)));
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
    *error = bookmark_api_constants::kNoNodeError;
    return false;
  }
  if (model->is_permanent_node(node)) {
    *error = bookmark_api_constants::kModifySpecialError;
    return false;
  }
  if (bookmarks::IsDescendantOf(node, managed->managed_node())) {
    *error = bookmark_api_constants::kModifyManagedError;
    return false;
  }
  if (node->is_folder() && !node->children().empty() && !recursive) {
    *error = bookmark_api_constants::kFolderNotEmptyError;
    return false;
  }

  model->Remove(node);
  return true;
}

void GetMetaInfo(const BookmarkNode& node,
                 base::DictionaryValue* id_to_meta_info_map) {
  if (!node.IsVisible())
    return;

  const BookmarkNode::MetaInfoMap* meta_info = node.GetMetaInfoMap();
  auto value = std::make_unique<base::DictionaryValue>();
  if (meta_info) {
    BookmarkNode::MetaInfoMap::const_iterator itr;
    for (itr = meta_info->begin(); itr != meta_info->end(); ++itr) {
      value->SetKey(itr->first, base::Value(itr->second));
    }
  }
  id_to_meta_info_map->Set(base::NumberToString(node.id()), std::move(value));

  if (node.is_folder()) {
    for (const auto& child : node.children())
      GetMetaInfo(*child, id_to_meta_info_map);
  }
}

}  // namespace bookmark_api_helpers
}  // namespace extensions
