// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/accessibility/accessibility_window_info_data_wrapper.h"
#include "chrome/browser/chromeos/arc/accessibility/ax_tree_source_arc.h"

namespace arc {

AccessibilityWindowInfoDataWrapper::AccessibilityWindowInfoDataWrapper(
    AXTreeSourceArc* tree_source,
    mojom::AccessibilityWindowInfoData* window)
    : tree_source_(tree_source), window_ptr_(window) {}

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
  // TODO(katie): Calculate this from properties.
  return true;
}

bool AccessibilityWindowInfoDataWrapper::IsFocused() const {
  // TODO(katie): Calculate this from properties.
  // Is "input focus" the same as focus?
  // https://developer.android.com/reference/android/view/accessibility/AccessibilityWindowInfo.html#isFocused()
  return false;
}

bool AccessibilityWindowInfoDataWrapper::CanBeAccessibilityFocused() const {
  // TODO(katie): Calculate this for windows.
  // Can windows have a11y focus?
  return true;
}

void AccessibilityWindowInfoDataWrapper::PopulateAXRole(
    ui::AXNodeData* out_data) const {
  // TODO(katie): Populate for windows using the window_type enum.
}

void AccessibilityWindowInfoDataWrapper::PopulateAXState(
    ui::AXNodeData* out_data) const {
  // TODO(katie): Populate for windows.
}

void AccessibilityWindowInfoDataWrapper::Serialize(
    ui::AXNodeData* out_data) const {
  // TODO(katie): Serialize for windows.
  if (!tree_source_->GetRoot()) {
    return;
  }
}

const std::vector<int32_t>* AccessibilityWindowInfoDataWrapper::GetChildren()
    const {
  // TODO(katie): Combine the root_node_id with the int_list_properties
  // of AccessibilityWindowIntListProperty::CHILD_WINDOW_IDS.
  return nullptr;
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
