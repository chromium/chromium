// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/accessibility/ax_tree_source_arc.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <set>
#include <stack>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/cxx20_erase.h"
#include "base/dcheck_is_on.h"
#include "chrome/browser/ash/arc/accessibility/accessibility_info_data_wrapper.h"
#include "chrome/browser/ash/arc/accessibility/accessibility_window_info_data_wrapper.h"
#include "chrome/browser/ash/arc/accessibility/arc_accessibility_util.h"
#include "chrome/browser/ash/arc/accessibility/auto_complete_handler.h"
#include "chrome/browser/ash/arc/accessibility/drawer_layout_handler.h"
#include "extensions/browser/api/automation_internal/automation_event_router.h"
#include "extensions/common/extension_messages.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_source_checker.h"
#include "ui/gfx/geometry/rect.h"

namespace arc {

using AXBooleanProperty = mojom::AccessibilityBooleanProperty;
using AXEventData = mojom::AccessibilityEventData;
using AXEventType = mojom::AccessibilityEventType;
using AXIntProperty = mojom::AccessibilityIntProperty;
using AXIntListProperty = mojom::AccessibilityIntListProperty;
using AXNodeInfoData = mojom::AccessibilityNodeInfoData;
using AXWindowBooleanProperty = mojom::AccessibilityWindowBooleanProperty;
using AXWindowInfoData = mojom::AccessibilityWindowInfoData;
using AXWindowIntListProperty = mojom::AccessibilityWindowIntListProperty;

// TODO(hirokisato): Enable AXTreeArcSerializer's |crash_on_error| once
// Android becomes able to send reliable trees.
AXTreeSourceArc::AXTreeSourceArc(Delegate* delegate, aura::Window* window)
    : current_tree_serializer_(new AXTreeArcSerializer(this, DCHECK_IS_ON())),
      is_notification_(false),
      is_input_method_window_(false),
      window_(window),
      delegate_(delegate) {}

AXTreeSourceArc::~AXTreeSourceArc() {
  Reset();
}

void AXTreeSourceArc::NotifyAccessibilityEvent(AXEventData* event_data) {
  root_id_.reset();
  DCHECK(event_data);

  NotifyAccessibilityEventInternal(*event_data);

  // Clear maps in order to prevent invalid access from dead pointers.
  tree_map_.clear();
  parent_map_.clear();
  computed_bounds_.clear();
}

void AXTreeSourceArc::NotifyActionResult(const ui::AXActionData& data,
                                         bool result) {
  GetAutomationEventRouter()->DispatchActionResult(data, result);
}

void AXTreeSourceArc::NotifyGetTextLocationDataResult(
    const ui::AXActionData& data,
    const absl::optional<gfx::Rect>& rect) {
  GetAutomationEventRouter()->DispatchGetTextLocationDataResult(data, rect);
}

bool AXTreeSourceArc::UseFullFocusMode() const {
  return delegate_->UseFullFocusMode();
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

AccessibilityInfoDataWrapper* AXTreeSourceArc::GetFirstImportantAncestor(
    AccessibilityInfoDataWrapper* info_data) const {
  AccessibilityInfoDataWrapper* parent = GetParent(info_data);
  while (parent && parent->IsNode() && !parent->IsImportantInAndroid()) {
    parent = GetParent(parent);
  }
  return parent;
}

bool AXTreeSourceArc::GetTreeData(ui::AXTreeData* data) const {
  data->tree_id = ax_tree_id();
  if (android_focused_id_.has_value())
    data->focus_id = *android_focused_id_;
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

  const auto& itr = hooks_.find(info_data->GetId());
  if (itr != hooks_.end())
    itr->second->PostSerializeNode(out_data);
}

void AXTreeSourceArc::BuildNodeTree(
    const std::vector<mojom::AccessibilityNodeInfoDataPtr>& node_data,
    std::vector<AccessibilityNodeInfoDataWrapper*>& nodes_to_reorder) {
  for (auto& node_ptr : node_data) {
    int32_t node_id = node_ptr->id;
    AXNodeInfoData* node = node_ptr.get();
    AccessibilityNodeInfoDataWrapper* node_wrapper_ptr =
        new AccessibilityNodeInfoDataWrapper(this, node);
    tree_map_[node_id] =
        base::WrapUnique<AccessibilityNodeInfoDataWrapper>(node_wrapper_ptr);
    std::vector<int32_t> children;
    if (GetProperty(node->int_list_properties,
                    AXIntListProperty::CHILD_NODE_IDS, &children)) {
      for (const int32_t child : children)
        parent_map_[child] = node_id;
    }
    if (HasProperty(node->int_properties, AXIntProperty::TRAVERSAL_BEFORE) ||
        HasProperty(node->int_properties, AXIntProperty::TRAVERSAL_AFTER)) {
      nodes_to_reorder.push_back(node_wrapper_ptr);
    }
  }
}

void AXTreeSourceArc::NotifyAccessibilityEventInternal(
    const AXEventData& event_data) {
  if (window_id_ != event_data.window_id) {
    android_focused_id_.reset();
    window_id_ = event_data.window_id;
  }
  is_notification_ = event_data.notification_key.has_value();
  if (is_notification_)
    notification_key_ = event_data.notification_key;
  is_input_method_window_ = event_data.is_input_method_window;

  // Prepare the wrapper objects of mojom data from Android.
  CHECK(event_data.window_data);
  root_id_ = event_data.window_data->at(0)->window_id;
  for (size_t i = 0; i < event_data.window_data->size(); ++i) {
    int32_t window_id = event_data.window_data->at(i)->window_id;
    int32_t root_node_id = event_data.window_data->at(i)->root_node_id;
    AXWindowInfoData* window = event_data.window_data->at(i).get();
    if (root_node_id)
      parent_map_[root_node_id] = window_id;

    tree_map_[window_id] =
        std::make_unique<AccessibilityWindowInfoDataWrapper>(this, window);

    std::vector<int32_t> children;
    if (GetProperty(window->int_list_properties,
                    AXWindowIntListProperty::CHILD_WINDOW_IDS, &children)) {
      for (const int32_t child : children) {
        DCHECK(child != root_id_);
        parent_map_[child] = window_id;
      }
    }
  }

  std::vector<AccessibilityNodeInfoDataWrapper*> nodes_to_reorder;
  BuildNodeTree(event_data.node_data, nodes_to_reorder);

  // Compute each node's bounds, based on its descendants.
  // Assuming |nodeData| is in pre-order, compute cached bounds in post-order to
  // avoid an O(n^2) amount of work as the computed bounds uses descendant
  // bounds.
  for (int i = event_data.node_data.size() - 1; i >= 0; --i) {
    int32_t id = event_data.node_data[i]->id;
    computed_bounds_[id] = ComputeEnclosingBounds(tree_map_[id].get());
  }
  for (int i = event_data.window_data->size() - 1; i >= 0; --i) {
    int32_t id = event_data.window_data->at(i)->window_id;
    computed_bounds_[id] = ComputeEnclosingBounds(tree_map_[id].get());
  }

  // This call contains the only code path that will sort the nodes by bounds.
  // Thus the reorder by traversal must occur after this.
  if (!UpdateAndroidFocusedId(event_data)) {
    // Exit this function if the focused node doesn't exist nor isn't visible.
    return;
  }

  std::vector<int32_t> update_ids = ProcessHooksOnEvent(event_data);

  // Reorder for traversal
  if (nodes_to_reorder.size() > 0) {
    TreeOrderer orderer(*this);
    orderer.ReorderTree(nodes_to_reorder);
    auto lcas = orderer.GetLeastCommonAncestors(nodes_to_reorder);
    update_ids.insert(update_ids.end(), lcas.begin(), lcas.end());
  }

  // Prep the event and send it to automation.
  AccessibilityInfoDataWrapper* focused_node =
      android_focused_id_.has_value() ? GetFromId(*android_focused_id_)
                                      : nullptr;
  std::vector<ui::AXEvent> events;
  ui::AXEvent event;
  event.event_type = ToAXEvent(
      event_data.event_type,
      GetPropertyOrNull(
          event_data.int_list_properties,
          arc::mojom::AccessibilityEventIntListProperty::CONTENT_CHANGE_TYPES),
      GetFromId(event_data.source_id), focused_node);
  event.id = event_data.source_id;

  int event_from_action;
  if (GetProperty(event_data.int_properties,
                  arc::mojom::AccessibilityEventIntProperty::ACTION,
                  &event_from_action)) {
    event.event_from = ax::mojom::EventFrom::kAction;

    event.event_from_action = ConvertToChromeAction(
        static_cast<mojom::AccessibilityActionType>(event_from_action));
  }

  events.push_back(std::move(event));

  // On event type of WINDOW_STATE_CHANGED, update the entire tree so that
  // window location is correctly calculated.
  int32_t node_id_to_clear =
      (event_data.event_type == AXEventType::WINDOW_STATE_CHANGED)
          ? *root_id_
          : event_data.source_id;

  update_ids.push_back(node_id_to_clear);

  for (const int32_t update_id : update_ids)
    current_tree_serializer_->InvalidateSubtree(GetFromId(update_id));

  std::vector<ui::AXTreeUpdate> updates;
  for (const int32_t update_id : update_ids) {
    ui::AXTreeUpdate update;
    if (!current_tree_serializer_->SerializeChanges(GetFromId(update_id),
                                                    &update)) {
      std::string error_string;
      ui::AXTreeSourceChecker<AccessibilityInfoDataWrapper*> checker(this);
      checker.CheckAndGetErrorString(&error_string);

      LOG(ERROR) << "Unable to serialize accessibility event\n"
                 << "Error: " << error_string << "\n"
                 << "Update: " << update.ToString();
    } else {
      updates.push_back(std::move(update));
    }
  }

  GetAutomationEventRouter()->DispatchAccessibilityEvents(
      ax_tree_id(), std::move(updates), gfx::Point(), std::move(events));
}

extensions::AutomationEventRouterInterface*
AXTreeSourceArc::GetAutomationEventRouter() const {
  if (automation_event_router_for_test_)
    return automation_event_router_for_test_;

  return extensions::AutomationEventRouter::GetInstance();
}

gfx::Rect AXTreeSourceArc::ComputeEnclosingBounds(
    AccessibilityInfoDataWrapper* info_data) const {
  DCHECK(info_data);
  gfx::Rect computed_bounds;
  // Exit early if the node or window is invisible.
  if (!info_data->IsVisibleToUser())
    return computed_bounds;

  ComputeEnclosingBoundsInternal(info_data, &computed_bounds);
  return computed_bounds;
}

void AXTreeSourceArc::ComputeEnclosingBoundsInternal(
    AccessibilityInfoDataWrapper* info_data,
    gfx::Rect* computed_bounds) const {
  DCHECK(computed_bounds);
  auto cached_bounds = computed_bounds_.find(info_data->GetId());
  if (cached_bounds != computed_bounds_.end()) {
    computed_bounds->Union(cached_bounds->second);
    return;
  }

  if (!info_data->IsVisibleToUser())
    return;
  // Only consider nodes that can possibly be accessibility focused.
  if (info_data->IsFocusableInFullFocusMode())
    computed_bounds->Union(info_data->GetBounds());

  std::vector<AccessibilityInfoDataWrapper*> children;
  info_data->GetChildren(&children);
  for (AccessibilityInfoDataWrapper* child : children)
    ComputeEnclosingBoundsInternal(child, computed_bounds);
  return;
}

AccessibilityInfoDataWrapper*
AXTreeSourceArc::FindFirstFocusableNodeInFullFocusMode(
    AccessibilityInfoDataWrapper* info_data) const {
  if (!IsValid(info_data))
    return nullptr;

  if (info_data->IsVisibleToUser() && info_data->IsFocusableInFullFocusMode())
    return info_data;

  std::vector<AccessibilityInfoDataWrapper*> children;
  GetChildren(info_data, &children);
  for (AccessibilityInfoDataWrapper* child : children) {
    AccessibilityInfoDataWrapper* candidate =
        FindFirstFocusableNodeInFullFocusMode(child);
    if (candidate)
      return candidate;
  }

  return nullptr;
}

bool AXTreeSourceArc::UpdateAndroidFocusedId(const AXEventData& event_data) {
  AccessibilityInfoDataWrapper* source_node = GetFromId(event_data.source_id);
  if (source_node) {
    AccessibilityInfoDataWrapper* source_window =
        GetFromId(source_node->GetWindowId());
    if (!source_window ||
        !GetBooleanProperty(source_window->GetWindow(),
                            AXWindowBooleanProperty::FOCUSED)) {
      // Don't update focus in this task for events from non-focused window.
      return true;
    }
  }

  if (event_data.event_type == AXEventType::VIEW_FOCUSED) {
    if (source_node && source_node->IsVisibleToUser() &&
        GetBooleanProperty(source_node->GetNode(),
                           AXBooleanProperty::FOCUSED)) {
      // Sometimes Android sets focus on unfocusable node, e.g. ListView.
      AccessibilityInfoDataWrapper* adjusted_node =
          UseFullFocusMode()
              ? FindFirstFocusableNodeInFullFocusMode(source_node)
              : source_node;
      if (IsValid(adjusted_node))
        android_focused_id_ = adjusted_node->GetId();
    }
  } else if (event_data.event_type == AXEventType::VIEW_ACCESSIBILITY_FOCUSED &&
             UseFullFocusMode()) {
    if (source_node && source_node->IsVisibleToUser())
      android_focused_id_ = source_node->GetId();
  } else if (event_data.event_type ==
                 AXEventType::VIEW_ACCESSIBILITY_FOCUS_CLEARED &&
             UseFullFocusMode()) {
    int event_from_action;
    GetProperty(event_data.int_properties,
                mojom::AccessibilityEventIntProperty::ACTION,
                &event_from_action);
    const mojom::AccessibilityActionType action =
        static_cast<mojom::AccessibilityActionType>(event_from_action);
    if (action != mojom::AccessibilityActionType::FOCUS &&
        action != mojom::AccessibilityActionType::ACCESSIBILITY_FOCUS) {
      android_focused_id_.reset();
    }
  } else if (event_data.event_type == AXEventType::VIEW_SELECTED) {
    // In Android, VIEW_SELECTED event is dispatched in the two cases below:
    // 1. Changing a value in ProgressBar or TimePicker in ARC P.
    // 2. Selecting an item in the context of an AdapterView.
    if (!source_node || !source_node->IsNode())
      return false;

    AXNodeInfoData* node_info = source_node->GetNode();
    DCHECK(node_info);

    bool is_range_change = !node_info->range_info.is_null();
    if (!is_range_change) {
      AccessibilityInfoDataWrapper* selected_node =
          GetSelectedNodeInfoFromAdapterViewEvent(event_data, source_node);
      if (!selected_node || !selected_node->IsVisibleToUser())
        return false;

      android_focused_id_ = selected_node->GetId();
    }
  } else if (event_data.event_type == AXEventType::WINDOW_STATE_CHANGED) {
    // When accessibility window changed, a11y event of WINDOW_CONTENT_CHANGED
    // is fired from Android multiple times.
    // The event of WINDOW_STATE_CHANGED is fired only once for each window
    // change and use it as a trigger to move the a11y focus to the first node.
    AccessibilityInfoDataWrapper* new_focus = nullptr;

    // If the current window has ever been visited in the current task, try
    // focus on the last focus node in this window.
    // We do it for WINDOW_STATE_CHANGED event from a window or a root node.
    bool from_root_or_window = (source_node && !source_node->IsNode()) ||
                               IsRootOfNodeTree(event_data.source_id);
    if (from_root_or_window) {
      auto itr = window_id_to_last_focus_node_id_.find(event_data.window_id);
      if (itr != window_id_to_last_focus_node_id_.end())
        new_focus = GetFromId(itr->second);
    } else if (UseFullFocusMode()) {
      // Otherwise, try focus on the first focusable node.
      new_focus = FindFirstFocusableNodeInFullFocusMode(
          GetFromId(event_data.source_id));
    }

    if (IsValid(new_focus))
      android_focused_id_ = new_focus->GetId();
  }

  if (!android_focused_id_ || !GetFromId(*android_focused_id_)) {
    // Because we only handle events from the focused window, let's reset the
    // focus to the root window.
    AccessibilityInfoDataWrapper* root = GetRoot();
    DCHECK(IsValid(root));
    android_focused_id_ = root_id_;
  }

  if (android_focused_id_.has_value()) {
    window_id_to_last_focus_node_id_[event_data.window_id] =
        *android_focused_id_;
  } else {
    window_id_to_last_focus_node_id_.erase(event_data.window_id);
  }

  AccessibilityInfoDataWrapper* focused_node =
      android_focused_id_.has_value() ? GetFromId(*android_focused_id_)
                                      : nullptr;

  // Ensure that the focused node correctly gets focus.
  while (focused_node && !focused_node->IsImportantInAndroid()) {
    AccessibilityInfoDataWrapper* parent = GetParent(focused_node);
    if (parent) {
      android_focused_id_ = parent->GetId();
      focused_node = parent;
    } else {
      break;
    }
  }

  return true;
}

std::vector<int32_t> AXTreeSourceArc::ProcessHooksOnEvent(
    const AXEventData& event_data) {
  base::EraseIf(hooks_, [this](const auto& it) {
    return this->GetFromId(it.first) == nullptr;
  });

  std::vector<int32_t> serialization_needed_ids;
  for (const auto& modifier : hooks_) {
    if (modifier.second->PreDispatchEvent(this, event_data))
      serialization_needed_ids.push_back(modifier.first);
  }

  // Add new hook implementations if necessary.
  auto drawer_layout_hook =
      DrawerLayoutHandler::CreateIfNecessary(this, event_data);
  if (drawer_layout_hook.has_value())
    hooks_.insert(std::move(*drawer_layout_hook));

  auto auto_complete_hooks =
      AutoCompleteHandler::CreateIfNecessary(this, event_data);
  for (auto& modifier : auto_complete_hooks) {
    if (hooks_.count(modifier.first) == 0)
      hooks_.insert(std::move(modifier));
  }

  return serialization_needed_ids;
}

void AXTreeSourceArc::Reset() {
  tree_map_.clear();
  parent_map_.clear();
  computed_bounds_.clear();
  current_tree_serializer_ = std::make_unique<AXTreeArcSerializer>(this);
  root_id_.reset();
  window_id_.reset();
  android_focused_id_.reset();
  extensions::AutomationEventRouterInterface* router =
      GetAutomationEventRouter();
  if (!router)
    return;

  router->DispatchTreeDestroyedEvent(ax_tree_id());
}

bool AXTreeSourceArc::NeedReorder(AccessibilityInfoDataWrapper* left,
                                  AccessibilityInfoDataWrapper* right) const {
  auto left_bounds = ComputeEnclosingBounds(left);
  auto right_bounds = ComputeEnclosingBounds(right);
  return !CompareBounds(left_bounds, right_bounds) &&
         CompareBounds(left->GetBounds(), right->GetBounds());
}

bool AXTreeSourceArc::CompareBounds(const gfx::Rect& left,
                                    const gfx::Rect& right) const {
  if (left.IsEmpty() || right.IsEmpty())
    return true;

  // Non-intersecting vertical check.
  if (left.bottom() <= right.y())
    return true;

  if (left.y() >= right.bottom())
    return false;

  // Vertically overlapping. Left one comes first.
  // TODO consider right-to-left
  int left_difference = left.x() - right.x();
  if (left_difference != 0)
    return left_difference < 0;

  // Top to bottom.
  int top_difference = left.y() - right.y();
  if (top_difference != 0)
    return top_difference < 0;

  // Larger to smaller.
  int height_difference = left.height() - right.height();
  if (height_difference != 0)
    return height_difference > 0;

  int width_difference = left.width() - right.width();
  if (width_difference != 0)
    return width_difference > 0;

  // The rects are equal. Respect the original order.
  return true;
}

int32_t AXTreeSourceArc::GetId(AccessibilityInfoDataWrapper* info_data) const {
  if (!info_data)
    return ui::kInvalidAXNodeID;
  return info_data->GetId();
}

void AXTreeSourceArc::GetChildren(
    AccessibilityInfoDataWrapper* info_data,
    std::vector<AccessibilityInfoDataWrapper*>* out_children) const {
  if (!info_data)
    return;

  info_data->GetChildren(out_children);
  if (out_children->size() < 2)
    return;

  // We sort output nodes only in full focus mode.
  // Also don't sort for virtual nodes (e.g. WebView).
  if (!UseFullFocusMode() || info_data->IsVirtualNode())
    return;

  for (size_t i = 0; i < out_children->size(); i++) {
    if (out_children->at(i)->IsVirtualNode())
      return;
  }

  // This is a kind of bubble sort, but we reorder nodes only when the original
  // node bounds (which is from node->GetBounds()) and child enclosing bounds
  // (which is from ComputeEnclosingBounds()) are different.
  // This algorithm takes O(N^2) time, but we practically don't expect that
  // there's a node that contains hundreds of child nodes that require
  // reordering.
  //
  // The concept here is taken from Android accessibility's similar logic in
  // com.google.android.accessibility.utils.traversal.ReorderedChildrenIterator.
  //
  // Note that NeedReorder method is not transitive, so we cannot sort with it.
  // For example, consider bounds below:
  //   a = (0,11)-(10x10)
  //   b = (20,5)-(10x10)
  //   c = (40,0)-(10x10)
  // Here, NeedReorder(a, b) = false, NeedReorder(b, c) = false, but
  // NeedReorder(a, c) = true.

  for (int i = out_children->size() - 2; i >= 0; i--) {
    auto original_bounds = out_children->at(i)->GetBounds();
    auto enclosing_bounds = ComputeEnclosingBounds(out_children->at(i));
    if (original_bounds == enclosing_bounds)
      continue;

    // move the current node to be visited later if necessary.
    for (size_t j = i;
         j + 1 < out_children->size() &&
         NeedReorder(out_children->at(j), out_children->at(j + 1));
         j++) {
      std::swap(out_children->at(j), out_children->at(j + 1));
    }
  }
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

// class TreeOrderer

AXTreeSourceArc::TreeOrderer::TreeOrderer(AXTreeSourceArc& tree_source)
    : tree_source_(tree_source) {}

void AXTreeSourceArc::TreeOrderer::ReorderTree(
    std::vector<AccessibilityNodeInfoDataWrapper*>& nodes_to_reorder) {
  for (AccessibilityNodeInfoDataWrapper* node_wrapper_ptr : nodes_to_reorder) {
    if (AccessibilityInfoDataWrapper* before_node =
            node_wrapper_ptr->GetTraversalBefore();
        before_node) {
      MoveNodeBefore(*node_wrapper_ptr, *before_node);
    } else if (AccessibilityInfoDataWrapper* after_node =
                   node_wrapper_ptr->GetTraversalAfter();
               after_node) {
      MoveNodeAfter(*node_wrapper_ptr, *after_node);
    }
  }
}

void AXTreeSourceArc::TreeOrderer::AppendChild(
    AccessibilityInfoDataWrapper& new_parent,
    AccessibilityInfoDataWrapper& new_child) {
  new_parent.AppendChild(new_child.GetId());
  tree_source_.parent_map_[new_child.GetId()] = new_parent.GetId();
}

std::set<int> AXTreeSourceArc::TreeOrderer::GetLeastCommonAncestors(
    std::vector<AccessibilityNodeInfoDataWrapper*>& nodes) const {
  // Find LCA using parent_map
  auto& parent_map = tree_source_.parent_map_;
  int32_t lowest_level = INT32_MAX;
  int len = nodes.size();
  std::vector<int> levels(len);
  // Loop through the nodes to find the one at the lowest level where root = 0.
  for (int i = 0; i < len; ++i) {
    AccessibilityInfoDataWrapper* node = nodes[i];
    // Replace any node that has traversal after with the node it comes after,
    // it will never be an LCA.
    auto* after_node = node->GetTraversalAfter();
    // Once there are no more traversal afters, or a cycle is detected.
    while (after_node && after_node->GetId() != nodes[i]->GetId()) {
      node = after_node;
      after_node = node->GetTraversalAfter();
    }
    // Replace the existing node with the traversal after.
    // Traversal after will always be a NodeInfoDataWrapper.
    nodes[i] = static_cast<AccessibilityNodeInfoDataWrapper*>(node);
    int32_t level = GetLevel(*node);
    levels[i] = level;
    if (level < lowest_level)
      lowest_level = level;
  }
  // Find the ancestors of the nodes not already at the lowest level
  std::set<int> nodes_at_lowest_level;
  for (int i = 0; i < len; ++i) {
    int32_t node_id = nodes[i]->GetId();
    int level = levels[i];
    int current_id = node_id;
    while (level > lowest_level) {
      current_id = parent_map[current_id];
      level--;
    }
    nodes_at_lowest_level.insert(current_id);
  }
  // The only items left should be the LCAs
  return nodes_at_lowest_level;
}

int32_t AXTreeSourceArc::TreeOrderer::GetLevel(
    AccessibilityInfoDataWrapper& node) const {
  auto& parent_map = tree_source_.parent_map_;
  auto current_it = parent_map.find(node.GetId());
  int level = 0;
  while (current_it != parent_map.end()) {
    level++;
    current_it = parent_map.find(current_it->second);
  }

  return level;
}

AccessibilityInfoDataWrapper&
AXTreeSourceArc::TreeOrderer::GetParentsThatAreMovedBeforeOrSameNode(
    AccessibilityInfoDataWrapper& moving_node) {
  auto* parent = tree_source_.GetParent(&moving_node);
  if (!parent)
    return moving_node;

  std::set<int> visited;
  if (InTraversalChain(parent, &moving_node, visited))
    return GetParentsThatAreMovedBeforeOrSameNode(*parent);

  return moving_node;
}

void AXTreeSourceArc::TreeOrderer::DetachFromParent(
    AccessibilityInfoDataWrapper& node) {
  auto& parent_map = tree_source_.parent_map_;
  auto* parent_wrapper = tree_source_.GetParent(&node);
  if (parent_wrapper) {
    parent_wrapper->RemoveChild(node.GetId());
    parent_map.erase(node.GetId());
  }
}

bool AXTreeSourceArc::TreeOrderer::HasDescendant(int parent_id,
                                                 int child_id) const {
  while (child_id != parent_id) {
    if (tree_source_.parent_map_.find(child_id) !=
        tree_source_.parent_map_.end()) {
      child_id = tree_source_.parent_map_[child_id];
    } else {
      return false;
    }
  }
  return true;
}

void AXTreeSourceArc::TreeOrderer::MoveNodeBefore(
    AccessibilityInfoDataWrapper& moving_node,
    AccessibilityInfoDataWrapper& target_node) {
  // traversal order: |moving_node| -> |target_node|

  if (HasDescendant(moving_node.GetId(), target_node.GetId())) {
    // no-op already a descendant.
    return;
  }

  AccessibilityInfoDataWrapper& moving_root =
      GetParentsThatAreMovedBeforeOrSameNode(moving_node);
  if (tree_source_.IsEqual(&moving_root, &target_node))
    return;

  auto* target_parent = tree_source_.GetParent(&target_node);
  if (target_parent &&
      HasDescendant(moving_root.GetId(), target_parent->GetId())) {
    // Moving moving_root under its own descendant would create a loop.
    return;
  }

  auto& parent_map = tree_source_.parent_map_;
  // detachSubtree
  DetachFromParent(moving_root);
  // If the parent exists replace target with moving_root since we will move the
  // target under moving root.
  if (target_parent && !tree_source_.IsEqual(&moving_root, target_parent)) {
    target_parent->ReplaceChild(target_node.GetId(), moving_root.GetId());
    // Set moving root parent = target_parent
    parent_map[moving_root.GetId()] = target_parent->GetId();
    parent_map.erase(target_node.GetId());
  }

  // Make target node a child of moving root
  if (!tree_source_.IsEqual(&moving_root, &target_node))
    AppendChild(moving_root, target_node);
}

void AXTreeSourceArc::TreeOrderer::MoveNodeAfter(
    AccessibilityInfoDataWrapper& moving_node,
    AccessibilityInfoDataWrapper& target_node) {
  // traversal order: |target_node| -> |moving_node|
  if (HasDescendant(moving_node.GetId(), target_node.GetId())) {
    // Moving moving_node under its own descendant would create a loop.
    return;
  }

  AccessibilityInfoDataWrapper& moving_root =
      GetParentsThatAreMovedBeforeOrSameNode(moving_node);

  if (tree_source_.IsEqual(&moving_root, &target_node)) {
    // Cycle detected, stop processing for this node.
    return;
  }

  if (HasDescendant(moving_root.GetId(), target_node.GetId())) {
    // Moving moving_root under its own descendant would create a loop.
    return;
  }
  // Remove moving_root from parent since we will move it underneath target.
  DetachFromParent(moving_root);

  if (!tree_source_.IsEqual(&target_node, &moving_root))
    AppendChild(target_node, moving_root);
}

bool AXTreeSourceArc::TreeOrderer::InTraversalChain(
    AccessibilityInfoDataWrapper* before,
    AccessibilityInfoDataWrapper* after,
    std::set<int>& visited) const {
  if (!before || !after)
    return false;

  AXNodeInfoData* before_node = before->GetNode();
  AXNodeInfoData* after_node = after->GetNode();
  if (!before_node || !after_node)
    return false;

  int id = -1;

  auto& tree_map = tree_source_.tree_map_;
  if (visited.find(before_node->id) == visited.end() &&
      GetProperty(before_node->int_properties, AXIntProperty::TRAVERSAL_BEFORE,
                  &id)) {
    if (after_node->id == id)
      return true;

    visited.insert(before_node->id);
    if (auto it = tree_map.find(id); it != tree_map.end()) {
      const auto& next_node_ptr = it->second;
      return InTraversalChain(next_node_ptr.get(), after, visited);
    }
  } else if (visited.find(after_node->id) == visited.end() &&
             GetProperty(after_node->int_properties,
                         AXIntProperty::TRAVERSAL_AFTER, &id)) {
    if (before_node->id == id)
      return true;

    visited.insert(after_node->id);
    if (auto it = tree_map.find(id); it != tree_map.end()) {
      const auto& prev_node_ptr = it->second;
      return InTraversalChain(before, prev_node_ptr.get(), visited);
    }
  }
  return false;
}

}  // namespace arc
