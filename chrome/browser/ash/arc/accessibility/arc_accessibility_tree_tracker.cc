// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/accessibility/arc_accessibility_tree_tracker.h"

namespace arc {

// static
ArcAccessibilityTreeTracker::TreeKey
ArcAccessibilityTreeTracker::KeyForInputMethod() {
  return {TreeKeyType::kInputMethod, 0, {}};
}

// static
ArcAccessibilityTreeTracker::TreeKey
ArcAccessibilityTreeTracker::KeyForNotification(std::string notification_key) {
  return {TreeKeyType::kNotificationKey, 0, std::move(notification_key)};
}

// static
ArcAccessibilityTreeTracker::TreeKey ArcAccessibilityTreeTracker::KeyForTaskId(
    int32_t task_id) {
  return {TreeKeyType::kTaskId, task_id, {}};
}

ArcAccessibilityTreeTracker::ArcAccessibilityTreeTracker() {}
ArcAccessibilityTreeTracker::~ArcAccessibilityTreeTracker() {}

bool ArcAccessibilityTreeTracker::Erase(const TreeKey& key) {
  return trees_.erase(key);
}

void ArcAccessibilityTreeTracker::Clear() {
  trees_.clear();
}

AXTreeSourceArc* ArcAccessibilityTreeTracker::GetFromTreeId(
    ui::AXTreeID tree_id) const {
  for (auto it = trees_.begin(); it != trees_.end(); ++it) {
    ui::AXTreeData tree_data;
    it->second->GetTreeData(&tree_data);
    if (tree_data.tree_id == tree_id)
      return it->second.get();
  }
  return nullptr;
}

AXTreeSourceArc* ArcAccessibilityTreeTracker::GetFromKey(const TreeKey& key) {
  auto tree_it = trees_.find(key);
  if (tree_it == trees_.end())
    return nullptr;

  return tree_it->second.get();
}

AXTreeSourceArc* ArcAccessibilityTreeTracker::CreateFromKey(
    TreeKey key,
    AXTreeSourceArc::Delegate* delegate) {
  auto tree = std::make_unique<AXTreeSourceArc>(delegate);
  AXTreeSourceArc* tree_ptr = tree.get();
  trees_.insert(std::make_pair(std::move(key), std::move(tree)));
  return tree_ptr;
}

}  // namespace arc
