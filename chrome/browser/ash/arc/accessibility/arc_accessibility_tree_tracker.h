// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_ACCESSIBILITY_ARC_ACCESSIBILITY_TREE_TRACKER_H_
#define CHROME_BROWSER_ASH_ARC_ACCESSIBILITY_ARC_ACCESSIBILITY_TREE_TRACKER_H_

#include <map>
#include <memory>
#include <string>
#include <tuple>

#include "chrome/browser/ash/arc/accessibility/ax_tree_source_arc.h"

namespace arc {

// ArcAccessibilityTreeTracker is responsible for mapping accessibility tree
// from android to exo window / surfaces.
class ArcAccessibilityTreeTracker {
 public:
  enum class TreeKeyType {
    kTaskId,
    kNotificationKey,
    kInputMethod,
  };

  using TreeKey = std::tuple<TreeKeyType, int32_t, std::string>;
  using TreeMap = std::map<TreeKey, std::unique_ptr<AXTreeSourceArc>>;

  static TreeKey KeyForInputMethod();
  static TreeKey KeyForNotification(std::string notification_key);
  static TreeKey KeyForTaskId(int32_t value);

  ArcAccessibilityTreeTracker();
  ~ArcAccessibilityTreeTracker();

  ArcAccessibilityTreeTracker(ArcAccessibilityTreeTracker&&) = delete;
  ArcAccessibilityTreeTracker& operator=(ArcAccessibilityTreeTracker&&) =
      delete;

  bool Erase(const TreeKey& key);
  void Clear();

  // Returns a tree source for the specified AXTreeID.
  AXTreeSourceArc* GetFromTreeId(ui::AXTreeID tree_id) const;

  AXTreeSourceArc* GetFromKey(const TreeKey&);
  AXTreeSourceArc* CreateFromKey(TreeKey, AXTreeSourceArc::Delegate*);

  const TreeMap& trees_for_test() const { return trees_; }

 private:
  TreeMap trees_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_ACCESSIBILITY_ARC_ACCESSIBILITY_TREE_TRACKER_H_
