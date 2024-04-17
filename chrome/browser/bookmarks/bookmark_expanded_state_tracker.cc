// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_expanded_state_tracker.h"

#include <stdint.h>

#include <cstddef>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/prefs/pref_service.h"

BookmarkExpandedStateTracker::BookmarkExpandedStateTracker(
    PrefService* pref_service)
    : pref_service_(pref_service) {}

BookmarkExpandedStateTracker::~BookmarkExpandedStateTracker() = default;

void BookmarkExpandedStateTracker::Init(
    bookmarks::BookmarkModel* bookmark_model) {
  DCHECK(!bookmark_model_);
  bookmark_model_ = bookmark_model;
  bookmark_model_->AddObserver(this);
}

void BookmarkExpandedStateTracker::SetExpandedNodes(const Nodes& nodes) {
  UpdatePrefs(nodes);
}

BookmarkExpandedStateTracker::Nodes
BookmarkExpandedStateTracker::GetExpandedNodes() {
  Nodes nodes;
  if (!bookmark_model_ || !bookmark_model_->loaded()) {
    return nodes;
  }

  if (!pref_service_) {
    return nodes;
  }

  const base::Value::List& value =
      pref_service_->GetList(bookmarks::prefs::kBookmarkEditorExpandedNodes);

  bool changed = false;
  for (const auto& entry : value) {
    int64_t node_id;
    const bookmarks::BookmarkNode* node;
    const std::string* value_str = entry.GetIfString();
    if (value_str && base::StringToInt64(*value_str, &node_id) &&
        (node = GetBookmarkNodeByID(bookmark_model_, node_id)) != nullptr &&
        node->is_folder()) {
      nodes.insert(node);
    } else {
      changed = true;
    }
  }
  if (changed) {
    UpdatePrefs(nodes);
  }
  return nodes;
}

void BookmarkExpandedStateTracker::BookmarkModelLoaded(
    bool ids_reassigned) {
  if (ids_reassigned) {
    // If the ids change we can't trust the value in preferences and need to
    // reset it.
    SetExpandedNodes(Nodes());
  }
}

void BookmarkExpandedStateTracker::BookmarkModelChanged() {}

void BookmarkExpandedStateTracker::BookmarkModelBeingDeleted() {
  bookmark_model_->RemoveObserver(this);
  bookmark_model_ = nullptr;
}

void BookmarkExpandedStateTracker::BookmarkNodeRemoved(
    const bookmarks::BookmarkNode* parent,
    size_t old_index,
    const bookmarks::BookmarkNode* node,
    const std::set<GURL>& removed_urls,
    const base::Location& location) {
  if (!node->is_folder()) {
    return;  // Only care about folders.
  }

  // Ask for the nodes again, which removes any nodes that were deleted.
  GetExpandedNodes();
}

void BookmarkExpandedStateTracker::BookmarkAllUserNodesRemoved(
    const std::set<GURL>& removed_urls,
    const base::Location& location) {
  // Ask for the nodes again, which removes any nodes that were deleted.
  GetExpandedNodes();
}

void BookmarkExpandedStateTracker::UpdatePrefs(const Nodes& nodes) {
  if (!pref_service_) {
    return;
  }

  base::Value::List values;
  values.reserve(nodes.size());
  for (const auto* node : nodes) {
    values.Append(base::NumberToString(node->id()));
  }

  pref_service_->SetList(bookmarks::prefs::kBookmarkEditorExpandedNodes,
                         std::move(values));
}
