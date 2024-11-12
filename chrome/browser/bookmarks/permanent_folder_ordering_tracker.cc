// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/permanent_folder_ordering_tracker.h"

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
    SetTrackedPermanentNodes();
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

void PermanentFolderOrderingTracker::BookmarkModelLoaded(bool ids_reassigned) {
  SetTrackedPermanentNodes();
  // TODO(crbug.com/364594278): Handle `ids_reassigned == true`.
}

void PermanentFolderOrderingTracker::BookmarkNodeAdded(
    const bookmarks::BookmarkNode* parent,
    size_t index,
    bool added_by_user) {
  if (parent->children()[index]->type() == tracked_type_) {
    // Account node created.
    SetTrackedPermanentNodes();
  }
}

void PermanentFolderOrderingTracker::BookmarkNodeRemoved(
    const bookmarks::BookmarkNode* parent,
    size_t old_index,
    const bookmarks::BookmarkNode* node,
    const std::set<GURL>& removed_urls,
    const base::Location& location) {
  if (node->type() == tracked_type_) {
    SetTrackedPermanentNodes();
  }
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
