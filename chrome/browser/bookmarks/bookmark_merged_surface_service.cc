// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"

#include <variant>

#include "base/notreached.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"

namespace {

using bookmarks::BookmarkNode;
using PermanentFolderType = BookmarkParentFolder::PermanentFolderType;

std::optional<PermanentFolderType> GetIfPermanentFolderType(
    const BookmarkNode* node) {
  switch (node->type()) {
    case BookmarkNode::Type::BOOKMARK_BAR:
      return PermanentFolderType::kBookmarkBarNode;
    case BookmarkNode::Type::OTHER_NODE:
      return PermanentFolderType::kOtherNode;
    case BookmarkNode::Type::MOBILE:
      return PermanentFolderType::kMobileNode;

    case BookmarkNode::Type::URL:
    case BookmarkNode::Type::FOLDER:
      return std::nullopt;
  }
  NOTREACHED();
}

}  // namespace

// static
BookmarkParentFolder BookmarkParentFolder::FromNonPermanentNode(
    const bookmarks::BookmarkNode* parent_node) {
  CHECK(parent_node);
  CHECK(parent_node->is_folder()) << "Constructing BookmarkParentFolder from a "
                                     "non-folder node.";
  CHECK(!parent_node->is_permanent_node())
      << "Node is permanent: " << parent_node->uuid();

  return BookmarkParentFolder(parent_node);
}

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

BookmarkParentFolder::BookmarkParentFolder(
    std::variant<PermanentFolderType, raw_ptr<const bookmarks::BookmarkNode>>
        parent)
    : bookmark_(parent) {}

BookmarkParentFolder::~BookmarkParentFolder() = default;

BookmarkParentFolder::BookmarkParentFolder(const BookmarkParentFolder& other) =
    default;
BookmarkParentFolder& BookmarkParentFolder::operator=(
    const BookmarkParentFolder& other) = default;

const bookmarks::BookmarkNode* BookmarkParentFolder::as_non_permanent_folder()
    const {
  if (HoldsNonPermanentFolder()) {
    return std::get<1>(bookmark_);
  }
  return nullptr;
}

std::optional<PermanentFolderType> BookmarkParentFolder::as_permanent_folder()
    const {
  if (HoldsNonPermanentFolder()) {
    return std::nullopt;
  }
  return std::get<0>(bookmark_);
}

bool BookmarkParentFolder::HoldsNonPermanentFolder() const {
  return bookmark_.index() == 1;
}

// BookmarkMergedSurfaceService:
BookmarkMergedSurfaceService::BookmarkMergedSurfaceService(
    bookmarks::BookmarkModel* model)
    : model_(model) {
  CHECK(model_);
}

BookmarkMergedSurfaceService::~BookmarkMergedSurfaceService() = default;

// static
bool BookmarkMergedSurfaceService::IsPermanentNodeOfType(
    const BookmarkNode* node,
    PermanentFolderType folder) {
  return GetIfPermanentFolderType(node) == folder;
}

bool BookmarkMergedSurfaceService::loaded() const {
  return model_->loaded();
}

size_t BookmarkMergedSurfaceService::GetChildrenCount(
    const BookmarkParentFolder& bookmark) const {
  if (bookmark.as_permanent_folder()) {
    return PermanentFolderToNode(*bookmark.as_permanent_folder())
        ->children()
        .size();
  }
  return bookmark.as_non_permanent_folder()->children().size();
}

void BookmarkMergedSurfaceService::Move(const bookmarks::BookmarkNode* node,
                                        const BookmarkParentFolder& new_parent,
                                        size_t index) {
  if (new_parent.as_permanent_folder()) {
    model_->Move(node, PermanentFolderToNode(*new_parent.as_permanent_folder()),
                 index);
  } else {
    model_->Move(node, new_parent.as_non_permanent_folder(), index);
  }
}

const BookmarkNode* BookmarkMergedSurfaceService::PermanentFolderToNode(
    PermanentFolderType folder) const {
  switch (folder) {
    case PermanentFolderType::kBookmarkBarNode:
      return model_->bookmark_bar_node();
    case PermanentFolderType::kOtherNode:
      return model_->other_node();
    case PermanentFolderType::kMobileNode:
      return model_->mobile_node();
  }
  NOTREACHED();
}
