// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/accessibility/accessibility_window_info_data_wrapper.h"

#include "chrome/browser/chromeos/arc/accessibility/ax_tree_source_arc.h"
#include "components/exo/wm_helper.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/platform/ax_android_constants.h"

namespace arc {

AccessibilityWindowInfoDataWrapper::AccessibilityWindowInfoDataWrapper(
    AXTreeSourceArc* tree_source,
    mojom::AccessibilityWindowInfoData* window)
    : AccessibilityInfoDataWrapper(tree_source), window_ptr_(window) {}

bool AccessibilityWindowInfoDataWrapper::IsNode() const {
  return false;
}

mojom::AccessibilityNodeInfoData* AccessibilityWindowInfoDataWrapper::GetNode()
    const {
  return nullptr;
}

mojom::AccessibilityWindowInfoData*
AccessibilityWindowInfoDataWrapper::GetWindow() const {
  return window_ptr_;
}

int32_t AccessibilityWindowInfoDataWrapper::GetId() const {
  return window_ptr_->window_id;
}

const gfx::Rect AccessibilityWindowInfoDataWrapper::GetBounds() const {
  return window_ptr_->bounds_in_screen;
}

bool AccessibilityWindowInfoDataWrapper::IsVisibleToUser() const {
  return true;
}

bool AccessibilityWindowInfoDataWrapper::IsFocused() const {
  // In Talkback, Android windows themselves cannot be focused. Only individual
  // nodes within these windows can have focus.
  return false;
}

bool AccessibilityWindowInfoDataWrapper::CanBeAccessibilityFocused() const {
  // Windows are too generic to be Accessibility focused in Chrome, although
  // they can be Accessibility focused in Android by virtue of having
  // accessibility focus on nodes within themselves.
  return false;
}

void AccessibilityWindowInfoDataWrapper::PopulateAXRole(
    ui::AXNodeData* out_data) const {
  switch (window_ptr_->window_type) {
    case mojom::AccessibilityWindowType::TYPE_ACCESSIBILITY_OVERLAY:
      out_data->role = ax::mojom::Role::kWindow;
      return;
    case mojom::AccessibilityWindowType::TYPE_APPLICATION:
      out_data->role = ax::mojom::Role::kApplication;
      return;
    case mojom::AccessibilityWindowType::TYPE_INPUT_METHOD:
      out_data->role = ax::mojom::Role::kKeyboard;
      return;
    case mojom::AccessibilityWindowType::TYPE_SPLIT_SCREEN_DIVIDER:
      // A system window used to divide the screen in split-screen mode. This
      // type of window is present only in split-screen mode.
      out_data->role = ax::mojom::Role::kSplitter;
      return;
    case mojom::AccessibilityWindowType::TYPE_SYSTEM:
      out_data->role = ax::mojom::Role::kWindow;
      return;
  }
}

void AccessibilityWindowInfoDataWrapper::PopulateAXState(
    ui::AXNodeData* out_data) const {
  // ARC++ window states are not reflected in ax::mojom::State, and for the
  // most part aren't needed.
  // Focusable in Android simply means a node within the window is focusable.
  // Since the window itself is not focusable in Android, it doesn't make sense
  // to include Focusable as an AXState.
}

void AccessibilityWindowInfoDataWrapper::Serialize(
    ui::AXNodeData* out_data) const {
  if (!tree_source_->GetRoot())
    return;

  AccessibilityInfoDataWrapper::Serialize(out_data);

  // String properties.
  std::string title;
  if (GetProperty(mojom::AccessibilityWindowStringProperty::TITLE, &title)) {
    out_data->SetName(title);
    out_data->SetNameFrom(ax::mojom::NameFrom::kTitle);
  }

  // Not all properties are currently used in Chrome Accessibility.

  // Boolean properties:
  // Someday we may want to have a IN_PICTURE_IN_PICTURE_MODE state or a
  // WINDOW_ACTIVE state, or to map the FOCUSED (i.e. has input focus) or
  // ACCESSIBILITY_FOCUSED (i.e. some node within this window has accessibility
  // focus) to new types.

  // Integer properties:
  // We could reflect ARC++ window properties like ANCHOR_NODE_ID,
  // and LAYER_ORDER in ax::mojom::IntAttributes.
}

void AccessibilityWindowInfoDataWrapper::GetChildren(
    std::vector<AccessibilityInfoDataWrapper*>* children) const {
  // Populate the children vector by combining the child window IDs with the
  // root node ID.
  if (window_ptr_->int_list_properties) {
    auto it = window_ptr_->int_list_properties->find(
        mojom::AccessibilityWindowIntListProperty::CHILD_WINDOW_IDS);
    if (it != window_ptr_->int_list_properties->end()) {
      for (int32_t id : it->second)
        children->push_back(tree_source_->GetFromId(id));
    }
  }

  if (window_ptr_->root_node_id)
    children->push_back(tree_source_->GetFromId(window_ptr_->root_node_id));
}

bool AccessibilityWindowInfoDataWrapper::GetProperty(
    mojom::AccessibilityWindowBooleanProperty prop) const {
  if (!window_ptr_->boolean_properties)
    return false;

  auto it = window_ptr_->boolean_properties->find(prop);
  if (it == window_ptr_->boolean_properties->end())
    return false;

  return it->second;
}

bool AccessibilityWindowInfoDataWrapper::GetProperty(
    mojom::AccessibilityWindowIntProperty prop,
    int32_t* out_value) const {
  if (!window_ptr_->int_properties)
    return false;

  auto it = window_ptr_->int_properties->find(prop);
  if (it == window_ptr_->int_properties->end())
    return false;

  *out_value = it->second;
  return true;
}

bool AccessibilityWindowInfoDataWrapper::HasProperty(
    mojom::AccessibilityWindowStringProperty prop) const {
  if (!window_ptr_->string_properties)
    return false;

  auto it = window_ptr_->string_properties->find(prop);
  return it != window_ptr_->string_properties->end();
}

bool AccessibilityWindowInfoDataWrapper::GetProperty(
    mojom::AccessibilityWindowStringProperty prop,
    std::string* out_value) const {
  if (!HasProperty(prop))
    return false;

  auto it = window_ptr_->string_properties->find(prop);
  *out_value = it->second;
  return true;
}

bool AccessibilityWindowInfoDataWrapper::GetProperty(
    mojom::AccessibilityWindowIntListProperty prop,
    std::vector<int32_t>* out_value) const {
  if (!window_ptr_->int_list_properties)
    return false;

  auto it = window_ptr_->int_list_properties->find(prop);
  if (it == window_ptr_->int_list_properties->end())
    return false;

  *out_value = it->second;
  return true;
}

}  // namespace arc
