// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"

#include <optional>
#include <variant>

#include "base/check_is_test.h"
#include "base/notreached.h"
#include "base/uuid.h"
#include "chrome/browser/bookmarks/bookmark_parent_folder_children.h"
#include "chrome/browser/bookmarks/permanent_folder_ordering_tracker.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_account_storage_move_dialog.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
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

base::flat_map<BookmarkParentFolder::PermanentFolderType,
               std::unique_ptr<PermanentFolderOrderingTracker>>
CreatePermanentFolderToTrackerMap(bookmarks::BookmarkModel* model) {
  base::flat_map<BookmarkParentFolder::PermanentFolderType,
                 std::unique_ptr<PermanentFolderOrderingTracker>>
      permanent_folder_to_tracker;
  permanent_folder_to_tracker[PermanentFolderType::kBookmarkBarNode] =
      std::make_unique<PermanentFolderOrderingTracker>(
          model, BookmarkNode::BOOKMARK_BAR);
  permanent_folder_to_tracker[PermanentFolderType::kOtherNode] =
      std::make_unique<PermanentFolderOrderingTracker>(
          model, BookmarkNode::OTHER_NODE);
  permanent_folder_to_tracker[PermanentFolderType::kMobileNode] =
      std::make_unique<PermanentFolderOrderingTracker>(model,
                                                       BookmarkNode::MOBILE);
  return permanent_folder_to_tracker;
}

bool IsPermanentManagedFolder(const BookmarkParentFolder& folder) {
  return folder.as_permanent_folder() == PermanentFolderType::kManagedNode;
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
  CHECK(!node->is_root());
  CHECK(node->is_folder());
  if (!node->is_permanent_node()) {
    return BookmarkParentFolder(node);
  }
  switch (node->type()) {
    case bookmarks::BookmarkNode::URL:
      NOTREACHED();
    case bookmarks::BookmarkNode::FOLDER:
      // TODO(crbug.com/381252292): Consider extending type with a value
      // `MANAGED_NODE`.
      // Only other possible permanent node is the managed one.
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
    : model_(model),
      managed_bookmark_service_(managed_bookmark_service),
      permanent_folder_to_tracker_(CreatePermanentFolderToTrackerMap(model)),
      dummy_empty_node_(/*id=*/0, base::Uuid::GenerateRandomV4(), GURL()) {
  CHECK(model_);
}

BookmarkMergedSurfaceService::~BookmarkMergedSurfaceService() = default;

std::vector<const BookmarkNode*>
BookmarkMergedSurfaceService::GetUnderlyingNodes(
    const BookmarkParentFolder& folder) const {
  if (folder.HoldsNonPermanentFolder()) {
    return {folder.as_non_permanent_folder()};
  }

  // Permanent folder.

  if (IsParentFolderManaged(folder)) {
    return {managed_permanent_node()};
  }
  auto tracker =
      permanent_folder_to_tracker_.find(*folder.as_permanent_folder());
  return tracker->second->GetUnderlyingPermanentNodes();
}

size_t BookmarkMergedSurfaceService::GetIndexOf(
    const bookmarks::BookmarkNode* node) const {
  CHECK(node);
  std::optional<PermanentFolderType> permanent_folder =
      GetIfPermanentFolderType(node->parent());
  if (!permanent_folder ||
      permanent_folder == PermanentFolderType::kManagedNode) {
    return *node->parent()->GetIndexOf(node);
  }

  return GetPermanentFolderOrderingTracker(*permanent_folder).GetIndexOf(node);
}

const bookmarks::BookmarkNode* BookmarkMergedSurfaceService::GetNodeAtIndex(
    const BookmarkParentFolder& folder,
    size_t index) const {
  CHECK_LT(index, GetChildrenCount(folder));
  return GetChildren(folder)[index];
}

bool BookmarkMergedSurfaceService::loaded() const {
  return model_->loaded();
}

size_t BookmarkMergedSurfaceService::GetChildrenCount(
    const BookmarkParentFolder& folder) const {
  return GetChildren(folder).size();
}

BookmarkParentFolderChildren BookmarkMergedSurfaceService::GetChildren(
    const BookmarkParentFolder& folder) const {
  if (folder.HoldsNonPermanentFolder()) {
    return BookmarkParentFolderChildren(folder.as_non_permanent_folder());
  }

  if (IsPermanentManagedFolder(folder)) {
    const BookmarkNode* node = managed_permanent_node()
                                   ? managed_permanent_node()
                                   : &dummy_empty_node_;
    return BookmarkParentFolderChildren(node);
  }

  return BookmarkParentFolderChildren(
      &GetPermanentFolderOrderingTracker(*folder.as_permanent_folder()));
}

const bookmarks::BookmarkNode*
BookmarkMergedSurfaceService::GetDefaultParentForNewNodes(
    const BookmarkParentFolder& folder) const {
  CHECK(model_->loaded());
  if (folder.HoldsNonPermanentFolder()) {
    return folder.as_non_permanent_folder();
  }

  // Managed nodes can't be edited.
  CHECK(!IsPermanentManagedFolder(folder));
  return GetPermanentFolderOrderingTracker(*folder.as_permanent_folder())
      .GetDefaultParentForNewNodes();
}

void BookmarkMergedSurfaceService::Move(const bookmarks::BookmarkNode* node,
                                        const BookmarkParentFolder& new_parent,
                                        size_t index,
                                        Browser* browser) {
  CHECK(!IsParentFolderManaged(new_parent));

  if (new_parent.as_permanent_folder()) {
    GetPermanentFolderOrderingTracker(*new_parent.as_permanent_folder())
        .MoveToIndex(node, index);
    return;
  }

  bool node_and_parent_have_same_storage =
      model_->IsLocalOnlyNode(*node) ==
      model_->IsLocalOnlyNode(*new_parent.as_non_permanent_folder());

  // Move the bookmark if no user action is required.
  if (node_and_parent_have_same_storage) {
    model_->Move(node, new_parent.as_non_permanent_folder(), index);
    return;
  }

  if (show_move_storage_dialog_for_testing_) {
    show_move_storage_dialog_for_testing_.Run(
        browser, node, new_parent.as_non_permanent_folder(), index);
    return;
  }

  // This will show a dialog which asks the user to confirm whether they would
  // like to move their bookmark to a different storage.
  CHECK(browser);
  ShowBookmarkAccountStorageMoveDialog(
      browser, node, new_parent.as_non_permanent_folder(), index);
}

void BookmarkMergedSurfaceService::SetShowMoveStorageDialogCallbackForTesting(
    ShowMoveStorageDialogCallback show_move_storage_dialog_for_testing) {
  CHECK_IS_TEST();
  show_move_storage_dialog_for_testing_ =
      std::move(show_move_storage_dialog_for_testing);
}

void BookmarkMergedSurfaceService::AddNodesAsCopiesOfNodeData(
    const std::vector<bookmarks::BookmarkNodeData::Element>& elements,
    const BookmarkParentFolder& new_parent,
    size_t index) {
  CHECK(!IsParentFolderManaged(new_parent));
  if (new_parent.as_permanent_folder()) {
    GetPermanentFolderOrderingTracker(*new_parent.as_permanent_folder())
        .AddNodesAsCopiesOfNodeData(elements, index);
  } else {
    bookmarks::CloneBookmarkNode(model_, elements,
                                 new_parent.as_non_permanent_folder(), index,
                                 /*reset_node_times=*/true);
  }
}

bool BookmarkMergedSurfaceService::IsParentFolderManaged(
    const BookmarkParentFolder& folder) const {
  if (folder.HoldsNonPermanentFolder()) {
    return IsNodeManaged(folder.as_non_permanent_folder());
  }

  if (IsPermanentManagedFolder(folder)) {
    CHECK(managed_permanent_node());
    return true;
  }
  return false;
}

bool BookmarkMergedSurfaceService::IsNodeManaged(
    const bookmarks::BookmarkNode* node) const {
  return managed_bookmark_service_ &&
         managed_bookmark_service_->IsNodeManaged(node);
}

const BookmarkNode* BookmarkMergedSurfaceService::managed_permanent_node()
    const {
  if (managed_bookmark_service_) {
    return managed_bookmark_service_->managed_node();
  }
  return nullptr;
}

const PermanentFolderOrderingTracker&
BookmarkMergedSurfaceService::GetPermanentFolderOrderingTracker(
    PermanentFolderType folder_type) const {
  CHECK_NE(folder_type, PermanentFolderType::kManagedNode);
  return *permanent_folder_to_tracker_.find(folder_type)->second;
}

PermanentFolderOrderingTracker&
BookmarkMergedSurfaceService::GetPermanentFolderOrderingTracker(
    PermanentFolderType folder_type) {
  CHECK_NE(folder_type, PermanentFolderType::kManagedNode);
  return *permanent_folder_to_tracker_.find(folder_type)->second;
}
