// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_ACCESSIBILITY_ACCESSIBILITY_INFO_DATA_WRAPPER_H_
#define CHROME_BROWSER_ASH_ARC_ACCESSIBILITY_ACCESSIBILITY_INFO_DATA_WRAPPER_H_

#include "ash/components/arc/mojom/accessibility_helper.mojom.h"

#include <cstdint>
#include <string>
#include <vector>

namespace ui {
struct AXNodeData;
}  // namespace ui

namespace arc {
class AXTreeSourceArc;

// AccessibilityInfoDataWrapper represents a single ARC++ node or window. This
// class can be used by AXTreeSourceArc to encapsulate ARC-side information
// which maps to a single AXNodeData.
class AccessibilityInfoDataWrapper {
 public:
  explicit AccessibilityInfoDataWrapper(AXTreeSourceArc* tree_source);
  virtual ~AccessibilityInfoDataWrapper();

  // True if this AccessibilityInfoDataWrapper represents an Android node, false
  // if it represents an Android window.
  virtual bool IsNode() const = 0;

  // These getters return nullptr if the class doesn't hold the specified type
  // of data.
  virtual mojom::AccessibilityNodeInfoData* GetNode() const = 0;
  virtual mojom::AccessibilityWindowInfoData* GetWindow() const = 0;

  virtual int32_t GetId() const = 0;
  virtual const gfx::Rect GetBounds() const = 0;
  virtual bool IsVisibleToUser() const = 0;
  virtual bool IsVirtualNode() const = 0;
  virtual bool IsIgnored() const = 0;
  virtual bool IsImportantInAndroid() const = 0;
  virtual bool IsFocusableInFullFocusMode() const = 0;
  virtual bool IsAccessibilityFocusableContainer() const = 0;
  virtual void PopulateAXRole(ui::AXNodeData* out_data) const = 0;
  virtual void PopulateAXState(ui::AXNodeData* out_data) const = 0;
  virtual void Serialize(ui::AXNodeData* out_data) const;
  virtual std::string ComputeAXName(bool do_recursive) const = 0;
  virtual void GetChildren(
      std::vector<AccessibilityInfoDataWrapper*>* children) const = 0;
  virtual int32_t GetWindowId() const = 0;

  void ReplaceChild(int32_t current_child_id, int32_t new_child_id);
  void AppendChild(int32_t new_child_id);
  void RemoveChild(int32_t child_id);

  // Gets the node that the input node should come before.
  // A.k.a Gets the node that should come after the input node.
  // node.traversalBefore = other_node
  virtual AccessibilityInfoDataWrapper* GetTraversalBefore() const = 0;

  // Gets the node that the input node should come after.
  // A.k.a Gets the node that should come before the input node.
  // node.traversalAfter = other_node
  virtual AccessibilityInfoDataWrapper* GetTraversalAfter() const = 0;

 protected:
  virtual void PopulateChildrenOverride() = 0;
  AXTreeSourceArc* tree_source_;
  absl::optional<std::vector<int>> children_override_;

 private:
  // Populate bounds of a node which can be passed to AXNodeData.location.
  // Bounds are returned in the following coordinates depending on whether it's
  // root or not.
  // - Root node is relative to its container, i.e. focused window.
  // - Non-root node is relative to the root node of this tree.
  void PopulateBounds(ui::AXNodeData* out_data) const;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_ACCESSIBILITY_ACCESSIBILITY_INFO_DATA_WRAPPER_H_
