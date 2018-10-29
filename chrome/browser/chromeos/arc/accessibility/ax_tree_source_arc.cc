// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/accessibility/ax_tree_source_arc.h"

#include <string>

#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/arc/accessibility/accessibility_node_info_data_wrapper.h"
#include "chrome/browser/chromeos/arc/accessibility/accessibility_window_info_data_wrapper.h"
#include "chrome/browser/chromeos/arc/accessibility/arc_accessibility_util.h"
#include "chrome/browser/extensions/api/automation_internal/automation_event_router.h"
#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"
#include "chrome/common/extensions/chrome_extension_messages.h"
#include "ui/accessibility/platform/ax_android_constants.h"
#include "ui/aura/window.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace arc {

using AXEventData = mojom::AccessibilityEventData;
using AXEventType = mojom::AccessibilityEventType;
using AXIntListProperty = mojom::AccessibilityIntListProperty;
using AXNodeInfoData = mojom::AccessibilityNodeInfoData;
using AXWindowInfoData = mojom::AccessibilityWindowInfoData;

AXTreeSourceArc::AXTreeSourceArc(Delegate* delegate)
    : current_tree_serializer_(new AXTreeArcSerializer(this)),
      root_id_(-1),
      window_id_(-1),
      focused_node_id_(-1),
      is_notification_(false),
      delegate_(delegate) {}

AXTreeSourceArc::~AXTreeSourceArc() {
  Reset();
}

void AXTreeSourceArc::NotifyAccessibilityEvent(AXEventData* event_data) {
  tree_map_.clear();
  parent_map_.clear();
  cached_computed_bounds_.clear();
  root_id_ = -1;

  window_id_ = event_data->window_id;
  is_notification_ = event_data->notification_key.has_value();

  // The following loops perform caching to prepare for AXTreeSerializer.
  // First, we need to cache parent links, which are implied by a node's child
  // ids.
  // Next, we cache the nodes by id. During this process, we can detect the root
  // node based upon the parent links we cached above.
  // Finally, we cache each node's computed bounds, based on its descendants.
  std::map<int32_t, int32_t> all_parent_map;
  std::map<int32_t, std::vector<int32_t>> all_children_map;
  for (size_t i = 0; i < event_data->node_data.size(); ++i) {
    if (!event_data->node_data[i]->int_list_properties)
      continue;
    auto it = event_data->node_data[i]->int_list_properties->find(
        AXIntListProperty::CHILD_NODE_IDS);
    if (it != event_data->node_data[i]->int_list_properties->end()) {
      all_children_map[event_data->node_data[i]->id] = it->second;
      for (size_t j = 0; j < it->second.size(); ++j)
        all_parent_map[it->second[j]] = event_data->node_data[i]->id;
    }
  }

  // Now copy just the relevant subtree containing the source_id into the
  // |parent_map_|.
  // TODO(katie): This step can probably be removed and all items added
  // directly to the parent map after window mapping is completed, because
  // we can again assume that there is only one subtree with one root.
  int32_t source_root = event_data->source_id;
  // Walk up to the root from the source_id.
  while (all_parent_map.find(source_root) != all_parent_map.end()) {
    int32_t parent = all_parent_map[source_root];
    source_root = parent;
  }
  root_id_ = source_root;
  // Walk back down through children map to populate parent_map_.
  std::stack<int32_t> stack;
  stack.push(root_id_);
  while (!stack.empty()) {
    int32_t parent = stack.top();
    stack.pop();
    const std::vector<int32_t>& children = all_children_map[parent];
    for (auto it = children.begin(); it != children.end(); ++it) {
      parent_map_[*it] = parent;
      stack.push(*it);
    }
  }

  for (size_t i = 0; i < event_data->node_data.size(); ++i) {
    int32_t id = event_data->node_data[i]->id;
    // Only map nodes in the parent_map and the root.
    // This avoids adding other subtrees that are not interesting.
    if (parent_map_.find(id) == parent_map_.end() && id != root_id_)
      continue;
    AXNodeInfoData* node = event_data->node_data[i].get();
    tree_map_[id] =
        std::make_unique<AccessibilityNodeInfoDataWrapper>(this, node);

    if (tree_map_[id]->IsFocused()) {
      focused_node_id_ = id;
    }
  }

  // Assuming |nodeData| is in pre-order, compute cached bounds in post-order to
  // avoid an O(n^2) amount of work as the computed bounds uses descendant
  // bounds.
  for (int i = event_data->node_data.size() - 1; i >= 0; --i) {
    int32_t id = event_data->node_data[i]->id;
    if (parent_map_.find(id) == parent_map_.end() && id != root_id_)
      continue;
    cached_computed_bounds_[id] = ComputeEnclosingBounds(tree_map_[id].get());
  }

  ExtensionMsg_AccessibilityEventBundleParams event_bundle;
  event_bundle.tree_id = tree_id();

  event_bundle.events.emplace_back();
  ui::AXEvent& event = event_bundle.events.back();
  event.event_type = ToAXEvent(event_data->event_type);
  event.id = event_data->source_id;

  event_bundle.updates.emplace_back();
  if (event_data->event_type == AXEventType::WINDOW_CONTENT_CHANGED) {
    current_tree_serializer_->InvalidateSubtree(
        GetFromId(event_data->source_id));
  }
  current_tree_serializer_->SerializeChanges(GetFromId(event_data->source_id),
                                             &event_bundle.updates.back());

  extensions::AutomationEventRouter* router =
      extensions::AutomationEventRouter::GetInstance();
  router->DispatchAccessibilityEvents(event_bundle);
}

void AXTreeSourceArc::NotifyActionResult(const ui::AXActionData& data,
                                         bool result) {
  extensions::AutomationEventRouter::GetInstance()->DispatchActionResult(
      data, result);
}

bool AXTreeSourceArc::GetTreeData(ui::AXTreeData* data) const {
  data->tree_id = tree_id();
  if (focused_node_id_ >= 0)
    data->focus_id = focused_node_id_;
  else if (root_id_ >= 0)
    data->focus_id = root_id_;
  return true;
}

ArcAccessibilityInfoData* AXTreeSourceArc::GetRoot() const {
  ArcAccessibilityInfoData* root = GetFromId(root_id_);
  return root;
}

ArcAccessibilityInfoData* AXTreeSourceArc::GetFromId(int32_t id) const {
  auto it = tree_map_.find(id);
  if (it == tree_map_.end())
    return nullptr;
  return it->second.get();
}

int32_t AXTreeSourceArc::GetId(ArcAccessibilityInfoData* info_data) const {
  if (!info_data)
    return -1;
  return info_data->GetId();
}

void AXTreeSourceArc::GetChildren(
    ArcAccessibilityInfoData* info_data,
    std::vector<ArcAccessibilityInfoData*>* out_children) const {
  if (!info_data)
    return;
  const std::vector<int32_t>* children = info_data->GetChildren();
  if (!children)
    return;

  std::map<int32_t, size_t> id_to_index;
  for (size_t i = 0; i < children->size(); ++i) {
    ArcAccessibilityInfoData* child = GetFromId((*children)[i]);
    out_children->push_back(child);
    id_to_index[child->GetId()] = i;
  }

  // Sort children based on their enclosing bounding rectangles, based on their
  // descendants.
  std::sort(out_children->begin(), out_children->end(),
            [this, id_to_index](auto left, auto right) {
              auto left_bounds = ComputeEnclosingBounds(left);
              auto right_bounds = ComputeEnclosingBounds(right);

              if (left_bounds.IsEmpty() || right_bounds.IsEmpty()) {
                return id_to_index.at(left->GetId()) <
                       id_to_index.at(right->GetId());
              }

              // Top to bottom sort (non-overlapping).
              if (!left_bounds.Intersects(right_bounds))
                return left_bounds.y() < right_bounds.y();

              // Overlapping
              // Left to right.
              int left_difference = left_bounds.x() - right_bounds.x();
              if (left_difference != 0)
                return left_difference < 0;

              // Top to bottom.
              int top_difference = left_bounds.y() - right_bounds.y();
              if (top_difference != 0)
                return top_difference < 0;

              // Larger to smaller.
              int height_difference =
                  left_bounds.height() - right_bounds.height();
              if (height_difference != 0)
                return height_difference > 0;

              int width_difference = left_bounds.width() - right_bounds.width();
              if (width_difference != 0)
                return width_difference > 0;

              // The rects are equal.
              return id_to_index.at(left->GetId()) <
                     id_to_index.at(right->GetId());
            });
}

ArcAccessibilityInfoData* AXTreeSourceArc::GetParent(
    ArcAccessibilityInfoData* info_data) const {
  if (!info_data)
    return nullptr;
  auto it = parent_map_.find(info_data->GetId());
  if (it != parent_map_.end())
    return GetFromId(it->second);
  return nullptr;
}

bool AXTreeSourceArc::IsValid(ArcAccessibilityInfoData* info_data) const {
  return info_data;
}

bool AXTreeSourceArc::IsEqual(ArcAccessibilityInfoData* info_data1,
                              ArcAccessibilityInfoData* info_data2) const {
  if (!info_data1 || !info_data2)
    return false;
  return info_data1->GetId() == info_data2->GetId();
}

ArcAccessibilityInfoData* AXTreeSourceArc::GetNull() const {
  return nullptr;
}

void AXTreeSourceArc::SerializeNode(ArcAccessibilityInfoData* info_data,
                                    ui::AXNodeData* out_data) const {
  if (!info_data)
    return;

  int32_t id = info_data->GetId();
  out_data->id = id;
  if (id == root_id_)
    out_data->role = ax::mojom::Role::kRootWebArea;
  else
    info_data->PopulateAXRole(out_data);

  info_data->Serialize(out_data);
}

const gfx::Rect AXTreeSourceArc::GetBounds(ArcAccessibilityInfoData* info_data,
                                           aura::Window* active_window) const {
  DCHECK_NE(root_id_, -1);

  gfx::Rect node_bounds = info_data->GetBounds();

  if (active_window && info_data->GetId() == root_id_) {
    // Top level window returns its bounds in dip.
    aura::Window* toplevel_window = active_window->GetToplevelWindow();
    float scale = toplevel_window->layer()->device_scale_factor();

    views::Widget* widget =
        views::Widget::GetWidgetForNativeView(active_window);
    DCHECK(widget);
    DCHECK(widget->widget_delegate());
    DCHECK(widget->widget_delegate()->GetContentsView());
    const gfx::Rect bounds =
        widget->widget_delegate()->GetContentsView()->GetBoundsInScreen();

    // Bounds of root node is relative to its container, i.e. contents view
    // (ShellSurfaceBase).
    node_bounds.Offset(
        static_cast<int>(-1.0f * scale * static_cast<float>(bounds.x())),
        static_cast<int>(-1.0f * scale * static_cast<float>(bounds.y())));

    // On Android side, content is rendered without considering height of
    // caption bar, e.g. content is rendered at y:0 instead of y:32 where 32 is
    // height of caption bar. Add back height of caption bar here.
    if (widget->IsMaximized()) {
      node_bounds.Offset(
          0, static_cast<int>(scale *
                              static_cast<float>(widget->non_client_view()
                                                     ->frame_view()
                                                     ->GetBoundsForClientView()
                                                     .y())));
    }

    return node_bounds;
  }

  // Bounds of non-root node is relative to its tree's root.
  gfx::Rect root_bounds = GetFromId(root_id_)->GetBounds();
  node_bounds.Offset(-1 * root_bounds.x(), -1 * root_bounds.y());
  return node_bounds;
}

gfx::Rect AXTreeSourceArc::ComputeEnclosingBounds(
    ArcAccessibilityInfoData* info_data) const {
  gfx::Rect computed_bounds;
  // Exit early if the node or window is invisible.
  if (!info_data->IsVisibleToUser())
    return computed_bounds;

  ComputeEnclosingBoundsInternal(info_data, computed_bounds);
  return computed_bounds;
}

void AXTreeSourceArc::ComputeEnclosingBoundsInternal(
    ArcAccessibilityInfoData* info_data,
    gfx::Rect& computed_bounds) const {
  auto cached_bounds = cached_computed_bounds_.find(info_data->GetId());
  if (cached_bounds != cached_computed_bounds_.end()) {
    computed_bounds.Union(cached_bounds->second);
    return;
  }

  if (!info_data->IsVisibleToUser())
    return;
  if (info_data->CanBeAccessibilityFocused()) {
    // Only consider nodes that can possibly be accessibility focused.
    computed_bounds.Union(info_data->GetBounds());
    return;
  }
  const std::vector<int32_t>* children = info_data->GetChildren();
  if (!children)
    return;
  for (size_t i = 0; i < children->size(); ++i) {
    ComputeEnclosingBoundsInternal(GetFromId((*children)[i]), computed_bounds);
  }

  return;
}

void AXTreeSourceArc::PerformAction(const ui::AXActionData& data) {
  delegate_->OnAction(data);
}

void AXTreeSourceArc::Reset() {
  tree_map_.clear();
  parent_map_.clear();
  cached_computed_bounds_.clear();
  current_tree_serializer_.reset(new AXTreeArcSerializer(this));
  root_id_ = -1;
  focused_node_id_ = -1;
  extensions::AutomationEventRouter* router =
      extensions::AutomationEventRouter::GetInstance();
  if (!router)
    return;

  router->DispatchTreeDestroyedEvent(tree_id(), nullptr);
}

}  // namespace arc
