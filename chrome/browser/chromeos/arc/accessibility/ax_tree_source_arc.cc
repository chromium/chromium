// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/accessibility/ax_tree_source_arc.h"

#include <stack>
#include <string>

#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/arc/accessibility/accessibility_node_info_data_wrapper.h"
#include "chrome/browser/chromeos/arc/accessibility/accessibility_window_info_data_wrapper.h"
#include "chrome/browser/chromeos/arc/accessibility/arc_accessibility_util.h"
#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"
#include "extensions/browser/api/automation_internal/automation_event_router.h"
#include "extensions/common/extension_messages.h"
#include "ui/accessibility/platform/ax_android_constants.h"
#include "ui/aura/window.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace arc {

using AXBooleanProperty = mojom::AccessibilityBooleanProperty;
using AXEventData = mojom::AccessibilityEventData;
using AXEventType = mojom::AccessibilityEventType;
using AXIntListProperty = mojom::AccessibilityIntListProperty;
using AXNodeInfoData = mojom::AccessibilityNodeInfoData;
using AXNodeInfoDataPtr = mojom::AccessibilityNodeInfoDataPtr;
using AXWindowInfoData = mojom::AccessibilityWindowInfoData;
using AXWindowIntListProperty = mojom::AccessibilityWindowIntListProperty;

AXTreeSourceArc::AXTreeSourceArc(Delegate* delegate)
    : current_tree_serializer_(new AXTreeArcSerializer(this)),
      is_notification_(false),
      is_input_method_window_(false),
      delegate_(delegate) {}

AXTreeSourceArc::~AXTreeSourceArc() {
  Reset();
}

void AXTreeSourceArc::NotifyAccessibilityEvent(AXEventData* event_data) {
  tree_map_.clear();
  parent_map_.clear();
  cached_computed_bounds_.clear();
  root_id_.reset();

  window_id_ = event_data->window_id;
  is_notification_ = event_data->notification_key.has_value();
  is_input_method_window_ = event_data->is_input_method_window;

  // The following loops perform caching to prepare for AXTreeSerializer.
  // First, we cache the windows.
  // Next, we cache the nodes. To compute IsClickableLeaf, we want a mapping
  // from node id to an index in event_data->node_data is needed to avoid O(N^2)
  // computation, iterate it twice.
  // Finally, we cache each node's computed bounds, based on its descendants.
  CHECK(event_data->window_data);
  root_id_ = event_data->window_data->at(0)->window_id;
  for (size_t i = 0; i < event_data->window_data->size(); ++i) {
    int32_t window_id = event_data->window_data->at(i)->window_id;
    int32_t root_node_id = event_data->window_data->at(i)->root_node_id;
    AXWindowInfoData* window = event_data->window_data->at(i).get();
    if (root_node_id)
      parent_map_[root_node_id] = window_id;

    tree_map_[window_id] =
        std::make_unique<AccessibilityWindowInfoDataWrapper>(this, window);

    std::vector<int32_t> children;
    if (GetIntListProperty(window, AXWindowIntListProperty::CHILD_WINDOW_IDS,
                           &children)) {
      for (const int32_t child : children) {
        DCHECK(child != root_id_);
        parent_map_[child] = window_id;
      }
    }
  }

  // Mapping from node id to index in event_data->node_data.
  std::map<int32_t, int32_t> node_data_index_map;
  for (size_t i = 0; i < event_data->node_data.size(); ++i) {
    int32_t node_id = event_data->node_data[i]->id;
    node_data_index_map[node_id] = i;
    std::vector<int32_t> children;
    if (GetIntListProperty(event_data->node_data[i].get(),
                           AXIntListProperty::CHILD_NODE_IDS, &children)) {
      for (const int32_t child : children)
        parent_map_[child] = node_id;
    }
  }
  for (size_t i = 0; i < event_data->node_data.size(); ++i) {
    int32_t id = event_data->node_data[i]->id;
    AXNodeInfoData* node = event_data->node_data[i].get();
    bool is_clickable_leaf =
        ComputeIsClickableLeaf(i, event_data->node_data, node_data_index_map);
    tree_map_[id] = std::make_unique<AccessibilityNodeInfoDataWrapper>(
        this, node, is_clickable_leaf);

    if (tree_map_[id]->IsFocused())
      focused_id_ = id;
  }

  // Assuming |nodeData| is in pre-order, compute cached bounds in post-order to
  // avoid an O(n^2) amount of work as the computed bounds uses descendant
  // bounds.
  for (int i = event_data->node_data.size() - 1; i >= 0; --i) {
    int32_t id = event_data->node_data[i]->id;
    cached_computed_bounds_[id] = ComputeEnclosingBounds(tree_map_[id].get());
  }
  for (int i = event_data->window_data->size() - 1; i >= 0; --i) {
    int32_t id = event_data->window_data->at(i)->window_id;
    cached_computed_bounds_[id] = ComputeEnclosingBounds(tree_map_[id].get());
  }

  // Calculate the focused ID.
  if (!focused_id_.has_value()) {
    if (root_id_.has_value()) {
      AccessibilityInfoDataWrapper* root = GetRoot();
      // TODO (sarakato): Add proper fix once cause of invalid node is known.
      if (!IsValid(root)) {
        return;
      } else if (root->IsNode()) {
        focused_id_ = root_id_;
      } else {
        std::vector<AccessibilityInfoDataWrapper*> children;
        root->GetChildren(&children);
        if (!children.empty()) {
          for (size_t i = 0; i < children.size(); ++i) {
            if (children[i]->IsNode()) {
              focused_id_ = children[i]->GetId();
              break;
            }
          }
        }
      }
    }
  }

  ExtensionMsg_AccessibilityEventBundleParams event_bundle;
  event_bundle.tree_id = ax_tree_id();

  event_bundle.events.emplace_back();
  ui::AXEvent& event = event_bundle.events.back();
  // When the focused node exists, give it as a hint to decide a Chrome
  // automation event type.
  AXNodeInfoData* opt_focused_node = nullptr;
  if (focused_id_.has_value() &&
      tree_map_.find(*focused_id_) != tree_map_.end())
    opt_focused_node = tree_map_[*focused_id_]->GetNode();
  event.event_type = ToAXEvent(event_data->event_type, opt_focused_node);
  event.id = event_data->source_id;

  event_bundle.updates.emplace_back();

  // Force the tree, starting at the target of the event, to update, so
  // unignored fields get updated.
  event_bundle.updates[0].node_id_to_clear = event_data->source_id;
  current_tree_serializer_->InvalidateSubtree(GetFromId(event_data->source_id));

  current_tree_serializer_->SerializeChanges(GetFromId(event_data->source_id),
                                             &event_bundle.updates.back());

  GetAutomationEventRouter()->DispatchAccessibilityEvents(event_bundle);
}

void AXTreeSourceArc::NotifyActionResult(const ui::AXActionData& data,
                                         bool result) {
  GetAutomationEventRouter()->DispatchActionResult(data, result);
}

void AXTreeSourceArc::NotifyGetTextLocationDataResult(
    const ui::AXActionData& data,
    const base::Optional<gfx::Rect>& rect) {
  GetAutomationEventRouter()->DispatchGetTextLocationDataResult(data, rect);
}

const gfx::Rect AXTreeSourceArc::GetBounds(
    AccessibilityInfoDataWrapper* info_data,
    aura::Window* active_window) const {
  DCHECK(root_id_.has_value());

  gfx::Rect info_data_bounds = info_data->GetBounds();

  if (!active_window) {
    gfx::Rect root_bounds = GetRoot()->GetBounds();
    info_data_bounds.Offset(-1 * root_bounds.x(), -1 * root_bounds.y());
    return info_data_bounds;
  }

  // TODO(katie): offset_container_id should work and we shouldn't have to
  // go into this code path for each node.
  aura::Window* toplevel_window = active_window->GetToplevelWindow();
  float scale = toplevel_window->layer()->device_scale_factor();

  views::Widget* widget = views::Widget::GetWidgetForNativeView(active_window);
  DCHECK(widget);
  DCHECK(widget->widget_delegate());
  DCHECK(widget->widget_delegate()->GetContentsView());
  const gfx::Rect bounds =
      widget->widget_delegate()->GetContentsView()->GetBoundsInScreen();

  // Bounds of root node is relative to its container, i.e. contents view
  // (ShellSurfaceBase).
  info_data_bounds.Offset(
      static_cast<int>(-1.0f * scale * static_cast<float>(bounds.x())),
      static_cast<int>(-1.0f * scale * static_cast<float>(bounds.y())));

  // On Android side, content is rendered without considering height of
  // caption bar, e.g. content is rendered at y:0 instead of y:32 where 32 is
  // height of caption bar. Add back height of caption bar here.
  if (widget->IsMaximized()) {
    info_data_bounds.Offset(
        0, static_cast<int>(scale *
                            static_cast<float>(widget->non_client_view()
                                                   ->frame_view()
                                                   ->GetBoundsForClientView()
                                                   .y())));
  }
  return info_data_bounds;
}

void AXTreeSourceArc::InvalidateTree() {
  current_tree_serializer_->Reset();
}

bool AXTreeSourceArc::IsRootOfNodeTree(int32_t id) const {
  const auto& node_it = tree_map_.find(id);
  if (node_it == tree_map_.end())
    return false;

  if (!node_it->second->IsNode())
    return false;

  const auto& parent_it = parent_map_.find(id);
  if (parent_it == parent_map_.end())
    return true;

  const auto& parent_tree_it = tree_map_.find(parent_it->second);
  CHECK(parent_tree_it != tree_map_.end());
  return !parent_tree_it->second->IsNode();
}

int32_t AXTreeSourceArc::GetWindowId() const {
  CHECK(window_id_.has_value());
  return *window_id_;
}

bool AXTreeSourceArc::GetTreeData(ui::AXTreeData* data) const {
  data->tree_id = ax_tree_id();
  if (focused_id_.has_value())
    data->focus_id = *focused_id_;
  return true;
}

AccessibilityInfoDataWrapper* AXTreeSourceArc::GetRoot() const {
  return root_id_.has_value() ? GetFromId(*root_id_) : nullptr;
}

AccessibilityInfoDataWrapper* AXTreeSourceArc::GetFromId(int32_t id) const {
  auto it = tree_map_.find(id);
  if (it == tree_map_.end())
    return nullptr;
  return it->second.get();
}

AccessibilityInfoDataWrapper* AXTreeSourceArc::GetParent(
    AccessibilityInfoDataWrapper* info_data) const {
  if (!info_data)
    return nullptr;
  auto it = parent_map_.find(info_data->GetId());
  if (it != parent_map_.end())
    return GetFromId(it->second);
  return nullptr;
}

void AXTreeSourceArc::SerializeNode(AccessibilityInfoDataWrapper* info_data,
                                    ui::AXNodeData* out_data) const {
  if (!info_data)
    return;

  info_data->Serialize(out_data);
}

extensions::AutomationEventRouterInterface*
AXTreeSourceArc::GetAutomationEventRouter() const {
  return extensions::AutomationEventRouter::GetInstance();
}

gfx::Rect AXTreeSourceArc::ComputeEnclosingBounds(
    AccessibilityInfoDataWrapper* info_data) const {
  DCHECK(info_data);
  gfx::Rect computed_bounds;
  // Exit early if the node or window is invisible.
  if (!info_data->IsVisibleToUser())
    return computed_bounds;

  ComputeEnclosingBoundsInternal(info_data, computed_bounds);
  return computed_bounds;
}

void AXTreeSourceArc::ComputeEnclosingBoundsInternal(
    AccessibilityInfoDataWrapper* info_data,
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
  std::vector<AccessibilityInfoDataWrapper*> children;
  info_data->GetChildren(&children);
  if (children.empty())
    return;
  for (AccessibilityInfoDataWrapper* child : children)
    ComputeEnclosingBoundsInternal(child, computed_bounds);
  return;
}

bool AXTreeSourceArc::ComputeIsClickableLeaf(
    int32_t root_index,
    const std::vector<AXNodeInfoDataPtr>& nodes,
    const std::map<int32_t, int32_t>& node_id_to_array_index) const {
  AXNodeInfoData* node = nodes[root_index].get();
  if (!GetBooleanProperty(node, AXBooleanProperty::CLICKABLE))
    return false;

  std::vector<int32_t> children;
  if (!GetIntListProperty(node, AXIntListProperty::CHILD_NODE_IDS, &children))
    return true;

  std::stack<int32_t, std::vector<int32_t>> stack(children);
  while (!stack.empty()) {
    int32_t parent_id = stack.top();
    stack.pop();
    AXNodeInfoData* parent_node =
        nodes[node_id_to_array_index.at(parent_id)].get();
    if (GetBooleanProperty(parent_node, AXBooleanProperty::CLICKABLE))
      return false;
    if (GetIntListProperty(parent_node, AXIntListProperty::CHILD_NODE_IDS,
                           &children)) {
      for (const int32_t child : children)
        stack.push(child);
    }
  }
  return true;
}

void AXTreeSourceArc::Reset() {
  tree_map_.clear();
  parent_map_.clear();
  cached_computed_bounds_.clear();
  current_tree_serializer_.reset(new AXTreeArcSerializer(this));
  root_id_.reset();
  focused_id_.reset();
  extensions::AutomationEventRouterInterface* router =
      GetAutomationEventRouter();
  if (!router)
    return;

  router->DispatchTreeDestroyedEvent(ax_tree_id(), nullptr);
}

int32_t AXTreeSourceArc::GetId(AccessibilityInfoDataWrapper* info_data) const {
  if (!info_data)
    return ui::AXNode::kInvalidAXID;
  return info_data->GetId();
}

void AXTreeSourceArc::GetChildren(
    AccessibilityInfoDataWrapper* info_data,
    std::vector<AccessibilityInfoDataWrapper*>* out_children) const {
  if (!info_data)
    return;

  info_data->GetChildren(out_children);
  if (out_children->empty())
    return;

  std::map<int32_t, size_t> id_to_index;
  for (size_t i = 0; i < out_children->size(); i++)
    id_to_index[out_children->at(i)->GetId()] = i;

  // Sort children based on their enclosing bounding rectangles, based on their
  // descendants.
  std::sort(
      out_children->begin(), out_children->end(),
      [this, &id_to_index](auto left, auto right) {
        auto left_bounds = ComputeEnclosingBounds(left);
        auto right_bounds = ComputeEnclosingBounds(right);

        if (left_bounds.IsEmpty() || right_bounds.IsEmpty()) {
          return id_to_index.at(left->GetId()) < id_to_index.at(right->GetId());
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
        int height_difference = left_bounds.height() - right_bounds.height();
        if (height_difference != 0)
          return height_difference > 0;

        int width_difference = left_bounds.width() - right_bounds.width();
        if (width_difference != 0)
          return width_difference > 0;

        // The rects are equal.
        return id_to_index.at(left->GetId()) < id_to_index.at(right->GetId());
      });
}

bool AXTreeSourceArc::IsIgnored(AccessibilityInfoDataWrapper* info_data) const {
  return false;
}

bool AXTreeSourceArc::IsValid(AccessibilityInfoDataWrapper* info_data) const {
  return info_data;
}

bool AXTreeSourceArc::IsEqual(AccessibilityInfoDataWrapper* info_data1,
                              AccessibilityInfoDataWrapper* info_data2) const {
  if (!info_data1 || !info_data2)
    return false;
  return info_data1->GetId() == info_data2->GetId();
}

AccessibilityInfoDataWrapper* AXTreeSourceArc::GetNull() const {
  return nullptr;
}

void AXTreeSourceArc::PerformAction(const ui::AXActionData& data) {
  delegate_->OnAction(data);
}

}  // namespace arc
