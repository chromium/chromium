// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/permanent_folder_ordering_tracker.h"

#include <cstddef>

#include "base/notreached.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace {

bool IsValidTrackedType(BookmarkNode::Type type) {
  switch (type) {
    case bookmarks::BookmarkNode::URL:
    case bookmarks::BookmarkNode::FOLDER:
      NOTREACHED();

    case bookmarks::BookmarkNode::BOOKMARK_BAR:
    case bookmarks::BookmarkNode::OTHER_NODE:
    case bookmarks::BookmarkNode::MOBILE:
      return true;
  }
  NOTREACHED();
}

}  // namespace

PermanentFolderOrderingTracker::PermanentFolderOrderingTracker(
    BookmarkModel* model,
    BookmarkNode::Type tracked_type)
    : model_(model), tracked_type_(tracked_type) {
  CHECK(model);
  CHECK(IsValidTrackedType(tracked_type))
      << "Invalid tracked type : " << tracked_type;
  model_observation_.Observe(model);
  if (model->loaded()) {
    BookmarkModelLoaded(/*ids_reassigned=*/false);
  }
}

PermanentFolderOrderingTracker::~PermanentFolderOrderingTracker() = default;

std::vector<const bookmarks::BookmarkNode*>
PermanentFolderOrderingTracker::GetUnderlyingPermanentNodes() const {
  std::vector<const bookmarks::BookmarkNode*> nodes;
  if (account_node_) {
    nodes.push_back(account_node_);
  }

  if (local_or_syncable_node_) {
    nodes.push_back(local_or_syncable_node_);
  }
  return nodes;
}

size_t PermanentFolderOrderingTracker::GetIndexOf(
    const bookmarks::BookmarkNode* node) const {
  CHECK(node);
  CHECK(node->parent());
  CHECK_EQ(node->parent()->type(), tracked_type_);
  CHECK(node->parent() == account_node_ ||
        node->parent() == local_or_syncable_node_);

  if (ordering_.empty()) {
    return *node->parent()->GetIndexOf(node);
  }

  for (size_t i = 0; i < ordering_.size(); i++) {
    if (ordering_[i] == node) {
      return i;
    }
  }
  NOTREACHED();
}

void PermanentFolderOrderingTracker::SetTrackedPermanentNodes() {
  switch (tracked_type_) {
    case bookmarks::BookmarkNode::URL:
    case bookmarks::BookmarkNode::FOLDER:
      NOTREACHED();

    case bookmarks::BookmarkNode::BOOKMARK_BAR:
      local_or_syncable_node_ = model_->bookmark_bar_node();
      account_node_ = model_->account_bookmark_bar_node();
      return;

    case bookmarks::BookmarkNode::OTHER_NODE:
      local_or_syncable_node_ = model_->other_node();
      account_node_ = model_->account_other_node();
      return;

    case bookmarks::BookmarkNode::MOBILE:
      local_or_syncable_node_ = model_->mobile_node();
      account_node_ = model_->account_mobile_node();
      return;
  }
  NOTREACHED();
}

bool PermanentFolderOrderingTracker::IsTrackedPermanentNode(
    const bookmarks::BookmarkNode* node) const {
  return node->type() == tracked_type_;
}

void PermanentFolderOrderingTracker::ResetOrderingToDefault() {
  ordering_.clear();
  if (!ShouldTrackOrdering()) {
    return;
  }
  for (const auto& node : account_node_->children()) {
    ordering_.push_back(node.get());
  }

  for (const auto& node : local_or_syncable_node_->children()) {
    ordering_.push_back(node.get());
  }
  CHECK_EQ(GetExpectedChildrenCount(), ordering_.size());
}

void PermanentFolderOrderingTracker::BookmarkModelLoaded(bool ids_reassigned) {
  SetTrackedPermanentNodes();
  ResetOrderingToDefault();

  // TODO(crbug.com/364594278): Handle `ids_reassigned == true`.
}

void PermanentFolderOrderingTracker::BookmarkNodeMoved(
    const bookmarks::BookmarkNode* old_parent,
    size_t old_index,
    const bookmarks::BookmarkNode* new_parent,
    size_t new_index) {
  // TODO(crbug.com/364594278): Update ordering.
}

void PermanentFolderOrderingTracker::BookmarkNodeAdded(
    const bookmarks::BookmarkNode* parent,
    size_t index,
    bool added_by_user) {
  const BookmarkNode* new_node = parent->children()[index].get();
  if (IsTrackedPermanentNode(new_node)) {
    // Account node created.
    SetTrackedPermanentNodes();
    ResetOrderingToDefault();
    return;
  }

  if (!IsTrackedPermanentNode(parent)) {
    // Not a direct child of `tracked_type_`.
    return;
  }

  if (!ShouldTrackOrdering()) {
    CHECK(ordering_.empty());
    return;
  }

  if (ordering_.empty()) {
    // The creation of a node just made `ShouldTrackOrdering()` return true.
    CHECK_EQ(parent->children().size(), 1u);
    ResetOrderingToDefault();
    return;
  }

  // Ordering not empty.
  CHECK_GT(parent->children().size(), 1u);

  // Insert at the end of an existing block unless the `index` is 0,
  // then insert at the beginning of the first block of the same parent.
  if (index == 0) {
    size_t next_node_index = GetIndexOf(parent->children()[1].get());
    ordering_.insert(ordering_.cbegin() + next_node_index, new_node);
  } else {
    size_t previous_node_index =
        GetIndexOf(parent->children()[index - 1].get());
    ordering_.insert(ordering_.cbegin() + previous_node_index + 1, new_node);
  }
  CHECK_EQ(GetExpectedChildrenCount(), ordering_.size());
}

void PermanentFolderOrderingTracker::BookmarkNodeRemoved(
    const bookmarks::BookmarkNode* parent,
    size_t old_index,
    const bookmarks::BookmarkNode* node,
    const std::set<GURL>& removed_urls,
    const base::Location& location) {
  if (IsTrackedPermanentNode(node)) {
    // Account node removed.
    SetTrackedPermanentNodes();
    ResetOrderingToDefault();
    return;
  }

  if (!IsTrackedPermanentNode(parent)) {
    // Not a direct child of `tracked_type_`.
    return;
  }

  if (!ShouldTrackOrdering()) {
    ordering_.clear();
    return;
  }

  if (all_user_bookmarks_remove_in_progress_) {
    CHECK(ordering_.empty());
    return;
  }

  // std::erase is a no-op unless present.
  std::erase(ordering_, node);
  CHECK_EQ(GetExpectedChildrenCount(), ordering_.size());
}

void PermanentFolderOrderingTracker::OnWillRemoveAllUserBookmarks(
    const base::Location& location) {
  all_user_bookmarks_remove_in_progress_ = true;
  ordering_.clear();
}

void PermanentFolderOrderingTracker::BookmarkAllUserNodesRemoved(
    const std::set<GURL>& removed_urls,
    const base::Location& location) {
  all_user_bookmarks_remove_in_progress_ = false;
  CHECK(ordering_.empty());
}

void PermanentFolderOrderingTracker::BookmarkNodeChildrenReordered(
    const bookmarks::BookmarkNode* node) {
  // TODO(crbug.com/364594278): Update ordering.
}

void PermanentFolderOrderingTracker::SetNodesOrderingForTesting(
    std::vector<raw_ptr<const bookmarks::BookmarkNode>> ordering) {
  ordering_ = std::move(ordering);
}

bool PermanentFolderOrderingTracker::ShouldTrackOrdering() const {
  bool has_local_children =
      local_or_syncable_node_ && !local_or_syncable_node_->children().empty();
  bool has_account_children =
      account_node_ && !account_node_->children().empty();
  return has_local_children && has_account_children;
}

size_t PermanentFolderOrderingTracker::GetExpectedChildrenCount() const {
  size_t count = 0;
  if (local_or_syncable_node_) {
    count += local_or_syncable_node_->children().size();
  }

  if (account_node_) {
    count += account_node_->children().size();
  }
  return count;
}
