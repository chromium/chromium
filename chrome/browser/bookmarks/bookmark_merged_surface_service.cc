// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"

#include <optional>
#include <variant>

#include "base/notreached.h"
#include "base/uuid.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_uuids.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"

namespace {

using bookmarks::BookmarkNode;
using bookmarks::ManagedBookmarkService;
using PermanentFolderType = BookmarkParentFolder::PermanentFolderType;

std::optional<PermanentFolderType> GetIfPermanentFolderType(
    const BookmarkNode* node) {
  if (!node->is_permanent_node()) {
    return std::nullopt;
  }

  // `node` is a permanent node.
  switch (node->type()) {
    case BookmarkNode::Type::BOOKMARK_BAR:
      return PermanentFolderType::kBookmarkBarNode;
    case BookmarkNode::Type::OTHER_NODE:
      return PermanentFolderType::kOtherNode;
    case BookmarkNode::Type::MOBILE:
      return PermanentFolderType::kMobileNode;
    case BookmarkNode::Type::FOLDER:
      // Only other possible permanent node is the managed one.
      CHECK_EQ(node->uuid(),
               base::Uuid::ParseLowercase(bookmarks::kManagedNodeUuid));
      return PermanentFolderType::kManagedNode;

    case BookmarkNode::Type::URL:
      NOTREACHED();
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

// static
BookmarkParentFolder BookmarkParentFolder::ManagedFolder() {
  return BookmarkParentFolder(PermanentFolderType::kManagedNode);
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
  if (HoldsNonPermanentFolder()) {
    return node->parent() == as_non_permanent_folder();
  }

  return GetIfPermanentFolderType(node->parent()) == as_permanent_folder();
}

// BookmarkMergedSurfaceService:
BookmarkMergedSurfaceService::BookmarkMergedSurfaceService(
    bookmarks::BookmarkModel* model,
    bookmarks::ManagedBookmarkService* managed_bookmark_service)
    : model_(model), managed_bookmark_service_(managed_bookmark_service) {
  CHECK(model_);
}

BookmarkMergedSurfaceService::~BookmarkMergedSurfaceService() = default;

// static
bool BookmarkMergedSurfaceService::IsPermanentNodeOfType(
    const BookmarkNode* node,
    PermanentFolderType folder) {
  return GetIfPermanentFolderType(node) == folder;
}

size_t BookmarkMergedSurfaceService::GetIndexOf(
    const bookmarks::BookmarkNode* node) const {
  CHECK(node);
  return *node->parent()->GetIndexOf(node);
}

const bookmarks::BookmarkNode* BookmarkMergedSurfaceService::GetNodeAtIndex(
    const BookmarkParentFolder& folder,
    size_t index) const {
  const BookmarkNode* node =
      folder.HoldsNonPermanentFolder()
          ? folder.as_non_permanent_folder()
          : PermanentFolderToNode(*folder.as_permanent_folder());
  CHECK_LT(index, node->children().size());
  return node->children()[index].get();
}

bool BookmarkMergedSurfaceService::loaded() const {
  return model_->loaded();
}

size_t BookmarkMergedSurfaceService::GetChildrenCount(
    const BookmarkParentFolder& bookmark) const {
  if (bookmark.as_permanent_folder()) {
    const BookmarkNode* node =
        PermanentFolderToNode(*bookmark.as_permanent_folder());
    // `PermanentFolderType::kManagedNode` can return null if the managed node
    // is null.
    return node ? node->children().size() : 0;
  }
  return bookmark.as_non_permanent_folder()->children().size();
}

void BookmarkMergedSurfaceService::Move(const bookmarks::BookmarkNode* node,
                                        const BookmarkParentFolder& new_parent,
                                        size_t index) {
  CHECK(!IsParentFolderManaged(new_parent));
  if (new_parent.as_permanent_folder()) {
    model_->Move(node, PermanentFolderToNode(*new_parent.as_permanent_folder()),
                 index);
  } else {
    model_->Move(node, new_parent.as_non_permanent_folder(), index);
  }
}

bool BookmarkMergedSurfaceService::IsParentFolderManaged(
    const BookmarkParentFolder& parent) const {
  if (parent.HoldsNonPermanentFolder()) {
    return IsNodeManaged(parent.as_non_permanent_folder());
  }
  return parent.as_permanent_folder() == PermanentFolderType::kManagedNode;
}

bool BookmarkMergedSurfaceService::IsNodeManaged(
    const bookmarks::BookmarkNode* parent) const {
  return managed_bookmark_service_ &&
         managed_bookmark_service_->IsNodeManaged(parent);
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
    case PermanentFolderType::kManagedNode:
      return managed_permanent_node();
  }
  NOTREACHED();
}

const BookmarkNode* BookmarkMergedSurfaceService::managed_permanent_node()
    const {
  if (managed_bookmark_service_) {
    return managed_bookmark_service_->managed_node();
  }
  return nullptr;
}
