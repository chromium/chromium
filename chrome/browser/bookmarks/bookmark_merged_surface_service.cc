// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"

#include <cstddef>
#include <optional>

#include "base/auto_reset.h"
#include "base/check_is_test.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/notreached.h"
#include "base/uuid.h"
#include "chrome/browser/bookmarks/bookmark_parent_folder_children.h"
#include "chrome/browser/bookmarks/permanent_folder_ordering_tracker.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_account_storage_move_dialog.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"

namespace {

using bookmarks::BookmarkNode;
using bookmarks::ManagedBookmarkService;
using PermanentFolderType = BookmarkParentFolder::PermanentFolderType;

const base::FilePath::CharType kMergedSurfaceOrderingFileName[] =
    FILE_PATH_LITERAL("BookmarkMergedSurfaceOrdering");

std::optional<PermanentFolderType> GetIfPermanentFolderType(
    const BookmarkNode* node) {
  if (!node->is_permanent_node()) {
    return std::nullopt;
  }
  return BookmarkParentFolder::FromFolderNode(node).as_permanent_folder();
}

base::flat_map<BookmarkParentFolder::PermanentFolderType,
               std::unique_ptr<PermanentFolderOrderingTracker>>
CreatePermanentFolderToTrackerMap(
    bookmarks::BookmarkModel* model,
    PermanentFolderOrderingTracker::Delegate* delegate) {
  base::flat_map<BookmarkParentFolder::PermanentFolderType,
                 std::unique_ptr<PermanentFolderOrderingTracker>>
      permanent_folder_to_tracker;
  permanent_folder_to_tracker[PermanentFolderType::kBookmarkBarNode] =
      std::make_unique<PermanentFolderOrderingTracker>(
          model, BookmarkNode::BOOKMARK_BAR, delegate);
  permanent_folder_to_tracker[PermanentFolderType::kOtherNode] =
      std::make_unique<PermanentFolderOrderingTracker>(
          model, BookmarkNode::OTHER_NODE, delegate);
  permanent_folder_to_tracker[PermanentFolderType::kMobileNode] =
      std::make_unique<PermanentFolderOrderingTracker>(
          model, BookmarkNode::MOBILE, delegate);
  return permanent_folder_to_tracker;
}

bool IsPermanentManagedFolder(const BookmarkParentFolder& folder) {
  return folder.as_permanent_folder() == PermanentFolderType::kManagedNode;
}

}  // namespace

class BookmarkMergedSurfaceService::BookmarkModelLoadedObserver
    : public bookmarks::BaseBookmarkModelObserver {
 public:
  // `model` must outlive this object.
  BookmarkModelLoadedObserver(bookmarks::BookmarkModel* model,
                              base::OnceCallback<void(bool)> on_load_completed)
      : on_load_completed_(std::move(on_load_completed)) {
    CHECK(model);
    CHECK(on_load_completed_);
    if (model->loaded()) {
      BookmarkModelLoaded(false);
      return;
    }
    observation_.Observe(model);
  }

  void BookmarkModelLoaded(bool ids_reassigned) override {
    std::move(on_load_completed_).Run(ids_reassigned);
  }

  void BookmarkModelChanged() override {}

 private:
  base::OnceCallback<void(bool)> on_load_completed_;
  base::ScopedObservation<bookmarks::BookmarkModel,
                          bookmarks::BaseBookmarkModelObserver>
      observation_{this};
};

BookmarkMergedSurfaceService::BookmarkMergedSurfaceService(
    bookmarks::BookmarkModel* model,
    bookmarks::ManagedBookmarkService* managed_bookmark_service)
    : model_(model),
      managed_bookmark_service_(managed_bookmark_service),
      permanent_folder_to_tracker_(
          CreatePermanentFolderToTrackerMap(model, /*delegate=*/this)),
      dummy_empty_node_(/*id=*/0, base::Uuid::GenerateRandomV4(), GURL()) {
  CHECK(model_);

  model_loaded_observer_ = std::make_unique<BookmarkModelLoadedObserver>(
      model_, base::BindOnce(&BookmarkMergedSurfaceService::BookmarkModelLoaded,
                             base::Unretained(this)));
}

BookmarkMergedSurfaceService::~BookmarkMergedSurfaceService() {
  for (auto& observer : observers_) {
    observer.BookmarkMergedSurfaceServiceBeingDeleted();
  }
}

void BookmarkMergedSurfaceService::Load(const base::FilePath& profile_path) {
  CHECK(!load_ordering_completed_);

  // `base::Unretained` is safe as `this` owns `load_from_disk_`.
  loader_ = BookmarkMergedSurfaceOrderingStorage::Loader::Create(
      profile_path.Append(kMergedSurfaceOrderingFileName),
      base::BindOnce(&BookmarkMergedSurfaceService::OnLoadOrderingComplete,
                     base::Unretained(this)));

  storage_ = std::make_unique<BookmarkMergedSurfaceOrderingStorage>(
      /*service=*/this, profile_path.Append(kMergedSurfaceOrderingFileName));
}

void BookmarkMergedSurfaceService::TrackedOrderingChanged() {
  CHECK(loaded());
  if (!storage_) {
    CHECK_IS_TEST();
    return;
  }
  storage_->ScheduleSave();
}

void BookmarkMergedSurfaceService::OnLoadOrderingComplete(
    BookmarkMergedSurfaceOrderingStorage::Loader::LoadResult result) {
  CHECK(!load_ordering_completed_);
  load_ordering_completed_ = true;
  loader_.reset();
  model_loaded_observer_.reset();

  for (const auto& [type, tracker] : permanent_folder_to_tracker_) {
    std::vector<int64_t> node_ids;
    auto loaded_node_ids = result.find(type);
    if (loaded_node_ids != result.end()) {
      node_ids = std::move(loaded_node_ids->second);
    }
    tracker->Init(std::move(node_ids));
  }

  // `PermanentFolderOrderingTracker` must precede this class in observing the
  // `BookmarkModel` to ensure changes are reflected in the tracker before
  // `this` notifies its observers.
  CHECK(!model_observation_.IsObserving());
  model_observation_.Observe(model_);

  if (loaded()) {
    NotifyLoaded();
  }
}

void BookmarkMergedSurfaceService::NotifyLoaded() {
  CHECK(loaded());
  for (auto& observer : observers_) {
    observer.BookmarkMergedSurfaceServiceLoaded();
  }
}

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
  return load_ordering_completed_ && model_->loaded();
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
  CHECK(loaded());
  if (folder.HoldsNonPermanentFolder()) {
    const bookmarks::BookmarkNode* node = folder.as_non_permanent_folder();
    CHECK(!IsNodeManaged(node));
    return folder.as_non_permanent_folder();
  }

  // Managed nodes can't be edited.
  CHECK(!IsPermanentManagedFolder(folder));
  return GetPermanentFolderOrderingTracker(*folder.as_permanent_folder())
      .GetDefaultParentForNewNodes();
}

const bookmarks::BookmarkNode*
BookmarkMergedSurfaceService::GetParentForManagedNode(
    const BookmarkParentFolder& folder) const {
  CHECK(loaded());
  if (folder.HoldsNonPermanentFolder()) {
    const bookmarks::BookmarkNode* node = folder.as_non_permanent_folder();
    CHECK(IsNodeManaged(node));
    return node;
  }

  CHECK(IsPermanentManagedFolder(folder));
  return managed_permanent_node();
}

void BookmarkMergedSurfaceService::Move(const bookmarks::BookmarkNode* node,
                                        const BookmarkParentFolder& new_parent,
                                        size_t index,
                                        Browser* browser) {
  CHECK(loaded());
  CHECK(!IsParentFolderManaged(new_parent));

  if (new_parent.as_permanent_folder()) {
    CHECK(!scoped_move_change_);
    base::AutoReset<bool> moving_nodes(&scoped_move_change_, true);
    const BookmarkParentFolder old_parent(
        BookmarkParentFolder::FromFolderNode(node->parent()));
    const size_t old_index = GetIndexOf(node);

    std::optional<size_t> new_index =
        GetPermanentFolderOrderingTracker(*new_parent.as_permanent_folder())
            .MoveToIndex(node, index);

    // Note: if moving within the same parent and `old_index` is less than
    // `index`, `new_index` will be off by one form `index`.
    if (!new_index.has_value()) {
      // `MoveToIndex()` is no-op.
      CHECK(old_parent == new_parent);
      return;
    }

    CHECK(old_parent != new_parent || old_index != new_index.value());
    for (auto& observer : observers_) {
      observer.BookmarkNodeMoved(old_parent, old_index, new_parent, *new_index);
    }
    return;
  }

  bool node_and_parent_have_same_storage =
      model_->IsLocalOnlyNode(*node) ==
      model_->IsLocalOnlyNode(*new_parent.as_non_permanent_folder());

  // Move the bookmark if no user action is required.
  if (node_and_parent_have_same_storage) {
    // Observer notifications triggered by `BookmarkModel` will be propagated to
    // `this` class's observers, see `BookmarkNodeMoved()`.
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

void BookmarkMergedSurfaceService::LoadForTesting(
    BookmarkMergedSurfaceOrderingStorage::Loader::LoadResult result) {
  CHECK_IS_TEST();
  OnLoadOrderingComplete(std::move(result));
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
  CHECK(loaded());
  CHECK(!IsParentFolderManaged(new_parent));
  if (new_parent.as_permanent_folder()) {
    CHECK(!scoped_add_new_nodes_);
    base::AutoReset<bool> adding_new_nodes(&scoped_add_new_nodes_, true);
    GetPermanentFolderOrderingTracker(*new_parent.as_permanent_folder())
        .AddNodesAsCopiesOfNodeData(elements, index);
    CHECK_GE(GetChildrenCount(new_parent), index + elements.size());

    // Notify after
    // `PermanentFolderOrderingTracker::AddNodesAsCopiesOfNodeData()` has
    // completed to ensure the correctness of the index.
    for (size_t i = index; i < index + elements.size(); i++) {
      for (auto& observer : observers_) {
        observer.BookmarkNodeAdded(new_parent, i);
      }
      NotifyBookmarkNodeAddedForAllDescendants(GetNodeAtIndex(new_parent, i));
    }
    return;
  }
  // Add new nodes to non-permanent folder.
  // `CloneBookmarkNode` will trigger `BookmarkNodeAdded()` which will notify
  // the observers of this class with the new nodes.
  bookmarks::CloneBookmarkNode(model_, elements,
                               new_parent.as_non_permanent_folder(), index,
                               /*reset_node_times=*/true);
}

bool BookmarkMergedSurfaceService::IsNonDefaultOrderingTracked(
    const BookmarkParentFolder& folder) const {
  return !folder.HoldsNonPermanentFolder() &&
         !IsPermanentManagedFolder(folder) &&
         GetPermanentFolderOrderingTracker(*folder.as_permanent_folder())
             .IsNonDefaultOrderingTracked();
}

void BookmarkMergedSurfaceService::NotifyBookmarkNodeAddedForAllDescendants(
    const BookmarkNode* node) {
  if (node->children().empty()) {
    return;
  }

  CHECK(node->is_folder());
  BookmarkParentFolder parent(BookmarkParentFolder::FromFolderNode(node));
  for (size_t i = 0; i < node->children().size(); i++) {
    for (auto& observer : observers_) {
      observer.BookmarkNodeAdded(parent, i);
    }
    NotifyBookmarkNodeAddedForAllDescendants(node->children()[i].get());
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

void BookmarkMergedSurfaceService::AddObserver(
    BookmarkMergedSurfaceServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void BookmarkMergedSurfaceService::RemoveObserver(
    BookmarkMergedSurfaceServiceObserver* observer) {
  observers_.RemoveObserver(observer);
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

size_t BookmarkMergedSurfaceService::GetIndexAcrossStorage(
    const bookmarks::BookmarkNode* node,
    size_t in_storage_index) const {
  // Note: this is done for optimization purposes, `GetIndexOf()` will return
  // the correct index.
  CHECK(node);
  CHECK(node->parent());
  std::optional<PermanentFolderType> type =
      GetIfPermanentFolderType(node->parent());
  if (type && type != PermanentFolderType::kManagedNode) {
    const PermanentFolderOrderingTracker& tracker =
        GetPermanentFolderOrderingTracker(*type);
    return tracker.GetIndexAcrossStorage(node, in_storage_index);
  }
  DCHECK_EQ(GetIndexOf(node), in_storage_index);
  return in_storage_index;
}

void BookmarkMergedSurfaceService::BookmarkModelLoaded(bool ids_reassigned) {
  if (load_ordering_completed_) {
    // Trackers already initialized and observing the model and will handle
    // `ids_reassigned`.
    CHECK(model_observation_.IsObserving());
    NotifyLoaded();
    return;
  }

  if (ids_reassigned) {
    OnLoadOrderingComplete({});
    return;
  }
}

void BookmarkMergedSurfaceService::OnWillMoveBookmarkNode(
    const BookmarkNode* old_parent,
    size_t old_index,
    const BookmarkNode* new_parent,
    size_t new_index) {
  if (scoped_move_change_) {
    return;
  }
  CHECK(!cached_index_for_node_move_);
  CHECK(old_parent);
  const BookmarkNode* node_to_move = old_parent->children()[old_index].get();
  size_t index = GetIndexAcrossStorage(node_to_move, old_index);
  cached_index_for_node_move_.emplace(index, node_to_move);
}

void BookmarkMergedSurfaceService::BookmarkNodeMoved(
    const BookmarkNode* old_parent,
    size_t old_index,
    const BookmarkNode* new_parent,
    size_t new_index) {
  if (scoped_move_change_) {
    return;
  }
  CHECK(cached_index_for_node_move_);
  const BookmarkNode* moved_node = new_parent->children()[new_index].get();
  CHECK_EQ(moved_node, cached_index_for_node_move_->second);
  const BookmarkParentFolder old_parent_folder(
      BookmarkParentFolder::FromFolderNode(old_parent));
  const BookmarkParentFolder new_parent_folder(
      BookmarkParentFolder::FromFolderNode(new_parent));
  // Trackers must have been updated already, because they are registered as
  // observers before `this`.
  const size_t new_index_across_storage =
      GetIndexAcrossStorage(moved_node, new_index);
  for (auto& observer : observers_) {
    observer.BookmarkNodeMoved(old_parent_folder,
                               cached_index_for_node_move_->first,
                               new_parent_folder, new_index_across_storage);
  }
  cached_index_for_node_move_.reset();
}

void BookmarkMergedSurfaceService::BookmarkNodeAdded(
    const bookmarks::BookmarkNode* parent,
    size_t index,
    bool added_by_user) {
  if (parent->is_root()) {
    // Observers will be notified for the child nodes. Account nodes are
    // invisible to merged surfaces, as they rely on the `BookmarkParentFolder`.
    return;
  }

  if (scoped_add_new_nodes_) {
    // Nodes are being added to a permanent folder through
    // `AddNodesAsCopiesOfNodeData()` which will notify the observers of this
    // class with the new nodes.
    return;
  }

  // Trackers must have been updated already, because they are registered as
  // observers before `this`.
  const BookmarkParentFolder folder(
      BookmarkParentFolder::FromFolderNode(parent));
  size_t index_across_storage =
      GetIndexAcrossStorage(parent->children()[index].get(), index);
  for (auto& observer : observers_) {
    observer.BookmarkNodeAdded(folder, index_across_storage);
  }
}

void BookmarkMergedSurfaceService::BookmarkNodeRemoved(
    const bookmarks::BookmarkNode* parent,
    size_t old_index,
    const bookmarks::BookmarkNode* node,
    const std::set<GURL>& no_longer_bookmarked,
    const base::Location& location) {
  if (parent->is_root()) {
    // Account node removed.
    CHECK(node->is_permanent_node());
    if (node->children().empty()) {
      return;
    }
    BookmarkParentFolder parent_folder =
        BookmarkParentFolder::FromFolderNode(node);
    base::flat_set<const BookmarkNode*> removed_nodes =
        base::MakeFlatSet<const BookmarkNode*>(
            node->children(),
            /*comp=*/{},
            [](const auto& bookmark_node) { return bookmark_node.get(); });
    for (auto& observer : observers_) {
      observer.BookmarkNodesRemoved(parent_folder, removed_nodes);
    }
    return;
  }

  CHECK(!parent->is_root());
  BookmarkParentFolder parent_folder(
      BookmarkParentFolder::FromFolderNode(parent));
  for (auto& observer : observers_) {
    observer.BookmarkNodesRemoved(parent_folder, {node});
  }
}

void BookmarkMergedSurfaceService::BookmarkNodeChanged(
    const bookmarks::BookmarkNode* node) {
  for (auto& observer : observers_) {
    observer.BookmarkNodeChanged(node);
  }
}

void BookmarkMergedSurfaceService::BookmarkNodeFaviconChanged(
    const bookmarks::BookmarkNode* node) {
  for (auto& observer : observers_) {
    observer.BookmarkNodeFaviconChanged(node);
  }
}

void BookmarkMergedSurfaceService::BookmarkNodeChildrenReordered(
    const bookmarks::BookmarkNode* node) {
  CHECK(node);
  CHECK(node->is_folder());
  const BookmarkParentFolder folder(BookmarkParentFolder::FromFolderNode(node));
  for (auto& observer : observers_) {
    observer.BookmarkParentFolderChildrenReordered(folder);
  }
}

void BookmarkMergedSurfaceService::BookmarkAllUserNodesRemoved(
    const std::set<GURL>& removed_urls,
    const base::Location& location) {
  for (auto& observer : observers_) {
    observer.BookmarkAllUserNodesRemoved();
  }
}

void BookmarkMergedSurfaceService::ExtensiveBookmarkChangesBeginning() {
  for (auto& observer : observers_) {
    observer.ExtensiveBookmarkChangesBeginning();
  }
}

void BookmarkMergedSurfaceService::ExtensiveBookmarkChangesEnded() {
  for (auto& observer : observers_) {
    observer.ExtensiveBookmarkChangesEnded();
  }
}
