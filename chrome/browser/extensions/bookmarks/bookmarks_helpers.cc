// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/bookmarks/bookmarks_helpers.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/bookmarks/bookmarks_error_constants.h"
#include "chrome/browser/extensions/bookmarks/bookmarks_features.h"
#include "chrome/common/extensions/api/bookmarks.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace extensions {

using api::bookmarks::BookmarkTreeNode;

namespace bookmarks_helpers {

namespace {

void AddNodeHelper(bookmarks::BookmarkModel* model,
                   bookmarks::ManagedBookmarkService* managed,
                   const BookmarkNode* node,
                   std::vector<BookmarkTreeNode>* nodes,
                   bool recurse,
                   bool only_folders) {
  if (node->IsVisible()) {
    nodes->push_back(
        GetBookmarkTreeNode(model, managed, node, recurse, only_folders));
  }
}

}  // namespace

BookmarkTreeNode GetBookmarkTreeNode(bookmarks::BookmarkModel* model,
                                     bookmarks::ManagedBookmarkService* managed,
                                     const BookmarkNode* node,
                                     bool recurse,
                                     bool only_folders,
                                     std::optional<size_t> visible_index) {
  BookmarkTreeNode bookmark_tree_node;
  PopulateBookmarkTreeNode(model, managed, node, recurse, only_folders,
                           visible_index, &bookmark_tree_node);
  return bookmark_tree_node;
}

void PopulateBookmarkTreeNode(
    bookmarks::BookmarkModel* model,
    bookmarks::ManagedBookmarkService* managed,
    const bookmarks::BookmarkNode* node,
    bool recurse,
    bool only_folders,
    std::optional<size_t> visible_index,
    api::bookmarks::BookmarkTreeNode* out_bookmark_tree_node) {
  DCHECK(out_bookmark_tree_node);

  out_bookmark_tree_node->id = base::NumberToString(node->id());

  const BookmarkNode* parent = node->parent();
  if (parent) {
    out_bookmark_tree_node->parent_id = base::NumberToString(parent->id());

    if (visible_index.has_value()) {
      out_bookmark_tree_node->index = *visible_index;
    } else if (!base::FeatureList::IsEnabled(
                   kEnforceBookmarkVisibilityOnExtensionsAPI)) {
      out_bookmark_tree_node->index =
          base::checked_cast<int>(parent->GetIndexOf(node).value());
    } else {
      out_bookmark_tree_node->index = GetAPIIndexOf(*node);
    }
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

  out_bookmark_tree_node->syncing = !model->IsLocalOnlyNode(*node);

  if (node->type() == BookmarkNode::Type::BOOKMARK_BAR) {
    out_bookmark_tree_node->folder_type =
        api::bookmarks::FolderType::kBookmarksBar;
  } else if (node->type() == BookmarkNode::Type::OTHER_NODE) {
    out_bookmark_tree_node->folder_type = api::bookmarks::FolderType::kOther;
  } else if (node->type() == BookmarkNode::Type::MOBILE) {
    out_bookmark_tree_node->folder_type = api::bookmarks::FolderType::kMobile;
  } else if (node == managed->managed_node()) {
    out_bookmark_tree_node->folder_type = api::bookmarks::FolderType::kManaged;
  }

  if (bookmarks::IsDescendantOf(node, managed->managed_node())) {
    out_bookmark_tree_node->unmodifiable =
        api::bookmarks::BookmarkTreeNodeUnmodifiable::kManaged;
  }

  if (recurse && node->is_folder()) {
    std::vector<BookmarkTreeNode> children;
    size_t child_visible_index = 0;
    for (const auto& child : node->children()) {
      // Check IsVisible() here to match the logic of GetAPIIndexOf().
      if (child->IsVisible()) {
        if (!only_folders || child->is_folder()) {
          children.push_back(GetBookmarkTreeNode(model, managed, child.get(),
                                                 /*recurse=*/true, only_folders,
                                                 child_visible_index));
        }
        ++child_visible_index;
      }
    }
    out_bookmark_tree_node->children = std::move(children);
  }
}

void AddNode(bookmarks::BookmarkModel* model,
             bookmarks::ManagedBookmarkService* managed,
             const BookmarkNode* node,
             std::vector<BookmarkTreeNode>* nodes,
             bool recurse) {
  return AddNodeHelper(model, managed, node, nodes, recurse, false);
}

void AddNodeFoldersOnly(bookmarks::BookmarkModel* model,
                        bookmarks::ManagedBookmarkService* managed,
                        const BookmarkNode* node,
                        std::vector<BookmarkTreeNode>* nodes,
                        bool recurse) {
  return AddNodeHelper(model, managed, node, nodes, recurse, true);
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
  if (!node.IsVisible()) {
    return;
  }

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
    for (const auto& child : node.children()) {
      GetMetaInfo(*child, id_to_meta_info_map);
    }
  }
}

size_t GetAPIIndexOf(const BookmarkNode& node) {
  CHECK(node.parent());

  size_t api_index = 0;
  for (const auto& child : node.parent()->children()) {
    if (child.get() == &node) {
      return api_index;
    }
    if (child->IsVisible()) {
      ++api_index;
    }
  }

  NOTREACHED() << "node is not a child of its parent.";
}

size_t GetAPIIndexOf(const BookmarkNode& parent, size_t previous_model_index) {
  size_t api_index = 0;
  for (size_t ix = 0;
       ix < std::min(parent.children().size(), previous_model_index); ++ix) {
    if (parent.children()[ix]->IsVisible()) {
      ++api_index;
    }
  }
  return api_index;
}

}  // namespace bookmarks_helpers
}  // namespace extensions
