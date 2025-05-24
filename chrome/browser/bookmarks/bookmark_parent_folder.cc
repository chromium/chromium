// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_parent_folder.h"

#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_uuids.h"

namespace {

using bookmarks::BookmarkNode;
using PermanentFolderType = BookmarkParentFolder::PermanentFolderType;

BookmarkParentFolder GetBookmarkParentFolderFromPermanentNode(
    const BookmarkNode* node) {
  CHECK(node);
  CHECK(node->is_permanent_node());
  switch (node->type()) {
    case bookmarks::BookmarkNode::URL:
      NOTREACHED();
    case bookmarks::BookmarkNode::FOLDER:
      // TODO(crbug.com/381252292): Consider extending type with a value
      // `MANAGED_NODE`.
      // Only other possible permanent node is the managed one.
      CHECK_EQ(node->uuid(),
               base::Uuid::ParseLowercase(bookmarks::kManagedNodeUuid));
      return BookmarkParentFolder::ManagedFolder();
    case bookmarks::BookmarkNode::BOOKMARK_BAR:
      return BookmarkParentFolder::BookmarkBarFolder();
    case bookmarks::BookmarkNode::OTHER_NODE:
      return BookmarkParentFolder::OtherFolder();
    case bookmarks::BookmarkNode::MOBILE:
      return BookmarkParentFolder::MobileFolder();
  }
  NOTREACHED();
}

}  // namespace

// static
BookmarkParentFolder BookmarkParentFolder::BookmarkBarFolder() {
  return BookmarkParentFolder(PermanentFolderType::kBookmarkBarNode);
}

// static
BookmarkParentFolder BookmarkParentFolder::OtherFolder() {
  return BookmarkParentFolder(PermanentFolderType::kOtherNode);
}

// static
BookmarkParentFolder BookmarkParentFolder::MobileFolder() {
  return BookmarkParentFolder(PermanentFolderType::kMobileNode);
}

// static
BookmarkParentFolder BookmarkParentFolder::ManagedFolder() {
  return BookmarkParentFolder(PermanentFolderType::kManagedNode);
}

// static
BookmarkParentFolder BookmarkParentFolder::FromFolderNode(
    const bookmarks::BookmarkNode* node) {
  CHECK(node);
  CHECK(node->is_folder());
  if (node->is_permanent_node()) {
    return GetBookmarkParentFolderFromPermanentNode(node);
  }
  return BookmarkParentFolder(node);
}

BookmarkParentFolder::BookmarkParentFolder(
    std::variant<PermanentFolderType, raw_ptr<const bookmarks::BookmarkNode>>
        parent)
    : bookmark_(parent) {}

BookmarkParentFolder::~BookmarkParentFolder() = default;

BookmarkParentFolder::BookmarkParentFolder(const BookmarkParentFolder& other) =
    default;
BookmarkParentFolder& BookmarkParentFolder::operator=(
    const BookmarkParentFolder& other) = default;

bool BookmarkParentFolder::HoldsNonPermanentFolder() const {
  return bookmark_.index() == 1;
}

std::optional<PermanentFolderType> BookmarkParentFolder::as_permanent_folder()
    const {
  if (HoldsNonPermanentFolder()) {
    return std::nullopt;
  }
  return std::get<0>(bookmark_);
}

const bookmarks::BookmarkNode* BookmarkParentFolder::as_non_permanent_folder()
    const {
  if (HoldsNonPermanentFolder()) {
    return std::get<1>(bookmark_);
  }
  return nullptr;
}

bool BookmarkParentFolder::HasDirectChildNode(
    const bookmarks::BookmarkNode* node) const {
  CHECK(node);
  if (node->is_permanent_node()) {
    return false;
  }

  BookmarkParentFolder parent(
      BookmarkParentFolder::FromFolderNode(node->parent()));
  return parent == *this;
}

bool BookmarkParentFolder::HasAncestor(
    const BookmarkParentFolder& ancestor) const {
  if (ancestor == *this) {
    return true;
  }

  if (as_permanent_folder().has_value()) {
    // `ancestor` can't be the root node.
    return false;
  }

  const BookmarkNode* node = as_non_permanent_folder();
  CHECK(node);
  BookmarkParentFolder parent(
      BookmarkParentFolder::FromFolderNode(node->parent()));
  return parent.HasAncestor(ancestor);
}
