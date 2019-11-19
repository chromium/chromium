// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_AX_TREE_SOURCE_ARC_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_AX_TREE_SOURCE_ARC_H_

#include <map>
#include <memory>
#include <vector>

#include "chrome/browser/chromeos/arc/accessibility/accessibility_info_data_wrapper.h"
#include "components/arc/mojom/accessibility_helper.mojom.h"
#include "extensions/browser/api/automation_internal/automation_event_router.h"
#include "ui/accessibility/ax_action_handler.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/ax_tree_serializer.h"
#include "ui/accessibility/ax_tree_source.h"
#include "ui/views/view.h"

namespace aura {
class Window;
}

namespace arc {
class AXTreeSourceArcTest;

using AXTreeArcSerializer = ui::AXTreeSerializer<AccessibilityInfoDataWrapper*,
                                                 ui::AXNodeData,
                                                 ui::AXTreeData>;

// This class represents the accessibility tree from the focused ARC window.
class AXTreeSourceArc : public ui::AXTreeSource<AccessibilityInfoDataWrapper*,
                                                ui::AXNodeData,
                                                ui::AXTreeData>,
                        public ui::AXActionHandler {
 public:
  class Delegate {
   public:
    virtual void OnAction(const ui::AXActionData& data) const = 0;
  };

  explicit AXTreeSourceArc(Delegate* delegate);
  ~AXTreeSourceArc() override;

  // Notify automation of an accessibility event.
  void NotifyAccessibilityEvent(mojom::AccessibilityEventData* event_data);

  // Notify automation of a result to an action.
  void NotifyActionResult(const ui::AXActionData& data, bool result);

  // Notify automation of result to getTextLocation.
  void NotifyGetTextLocationDataResult(const ui::AXActionData& data,
                                       const base::Optional<gfx::Rect>& rect);

  // Returns bounds of a node which can be passed to AXNodeData.location. Bounds
  // are returned in the following coordinates depending on whether it's root or
  // not.
  // - Root node is relative to its container, i.e. focused window.
  // - Non-root node is relative to the root node of this tree.
  //
  // focused_window is nullptr for notification.
  const gfx::Rect GetBounds(AccessibilityInfoDataWrapper* info_data,
                            aura::Window* focused_window) const;

  // Invalidates the tree serializer.
  void InvalidateTree();

  // Returns true if the node id is the root of the node tree (which can have a
  // parent window).
  bool IsRootOfNodeTree(int32_t id) const;

  // Gets the window id of this tree.
  int32_t GetWindowId() const;

  // AXTreeSource:
  bool GetTreeData(ui::AXTreeData* data) const override;
  AccessibilityInfoDataWrapper* GetRoot() const override;
  AccessibilityInfoDataWrapper* GetFromId(int32_t id) const override;
  AccessibilityInfoDataWrapper* GetParent(
      AccessibilityInfoDataWrapper* info_data) const override;
  void SerializeNode(AccessibilityInfoDataWrapper* info_data,
                     ui::AXNodeData* out_data) const override;

  bool is_notification() { return is_notification_; }

  bool is_input_method_window() { return is_input_method_window_; }

 private:
  friend class arc::AXTreeSourceArcTest;

  // virtual for testing.
  virtual extensions::AutomationEventRouterInterface* GetAutomationEventRouter()
      const;

  // Computes the smallest rect that encloses all of the descendants of
  // |info_data|.
  gfx::Rect ComputeEnclosingBounds(
      AccessibilityInfoDataWrapper* info_data) const;

  // Helper to recursively compute bounds for |info_data|. Returns true if
  // non-empty bounds were encountered.
  void ComputeEnclosingBoundsInternal(AccessibilityInfoDataWrapper* info_data,
                                      gfx::Rect& computed_bounds) const;

  // Computes the node is clickable and there is no clickable descendant of it.
  bool ComputeIsClickableLeaf(
      int32_t root_index,
      const std::vector<mojom::AccessibilityNodeInfoDataPtr>& nodes,
      const std::map<int32_t, int32_t>& node_id_to_array_index) const;

  // Resets tree state.
  void Reset();

  // AXTreeSource:
  int32_t GetId(AccessibilityInfoDataWrapper* info_data) const override;
  void GetChildren(
      AccessibilityInfoDataWrapper* info_data,
      std::vector<AccessibilityInfoDataWrapper*>* out_children) const override;
  bool IsIgnored(AccessibilityInfoDataWrapper* info_data) const override;
  bool IsValid(AccessibilityInfoDataWrapper* info_data) const override;
  bool IsEqual(AccessibilityInfoDataWrapper* info_data1,
               AccessibilityInfoDataWrapper* info_data2) const override;
  AccessibilityInfoDataWrapper* GetNull() const override;

  // AXActionHandler:
  void PerformAction(const ui::AXActionData& data) override;

  // Maps an AccessibilityInfoDataWrapper ID to its tree data.
  std::map<int32_t, std::unique_ptr<AccessibilityInfoDataWrapper>> tree_map_;

  // Maps an AccessibilityInfoDataWrapper ID to its parent.
  std::map<int32_t, int32_t> parent_map_;
  std::unique_ptr<AXTreeArcSerializer> current_tree_serializer_;
  base::Optional<int32_t> root_id_;
  base::Optional<int32_t> window_id_;
  base::Optional<int32_t> focused_id_;
  bool is_notification_;
  bool is_input_method_window_;

  // A delegate that handles accessibility actions on behalf of this tree. The
  // delegate is valid during the lifetime of this tree.
  const Delegate* const delegate_;
  std::string package_name_;

  // Mapping from AccessibilityInfoDataWrapper ID to its cached computed bounds.
  // This simplifies bounds calculations.
  std::map<int32_t, gfx::Rect> cached_computed_bounds_;

  DISALLOW_COPY_AND_ASSIGN(AXTreeSourceArc);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_AX_TREE_SOURCE_ARC_H_
