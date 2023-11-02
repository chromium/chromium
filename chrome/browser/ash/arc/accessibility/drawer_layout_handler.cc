// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/accessibility/drawer_layout_handler.h"

#include <vector>

#include "ash/components/arc/mojom/accessibility_helper.mojom-forward.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ash/arc/accessibility/accessibility_info_data_wrapper.h"
#include "chrome/browser/ash/arc/accessibility/arc_accessibility_util.h"
#include "chrome/browser/ash/arc/accessibility/ax_tree_source_arc.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"

namespace {

constexpr char kDrawerLayoutClassNameAndroidX[] =
    "androidx.drawerlayout.widget.DrawerLayout";
constexpr char kDrawerLayoutClassNameLegacy[] =
    "android.support.v4.widget.DrawerLayout";

bool IsDrawerLayout(arc::mojom::AccessibilityNodeInfoData* node) {
  if (!node || !node->string_properties)
    return false;

  auto it = node->string_properties->find(
      arc::mojom::AccessibilityStringProperty::CLASS_NAME);
  if (it == node->string_properties->end())
    return false;

  return it->second == kDrawerLayoutClassNameAndroidX ||
         it->second == kDrawerLayoutClassNameLegacy;
}

}  // namespace

namespace arc {

// static
absl::optional<std::pair<int32_t, std::unique_ptr<DrawerLayoutHandler>>>
DrawerLayoutHandler::CreateIfNecessary(
    AXTreeSourceArc* tree_source,
    const mojom::AccessibilityEventData& event_data) {
  if (event_data.event_type !=
      mojom::AccessibilityEventType::WINDOW_STATE_CHANGED) {
    return absl::nullopt;
  }

  AccessibilityInfoDataWrapper* source_node =
      tree_source->GetFromId(event_data.source_id);
  if (!source_node || !IsDrawerLayout(source_node->GetNode()))
    return absl::nullopt;

  // Find a node with accessibility importance. That is a menu node opened now.
  // Extract the accessibility name of the drawer menu from the event text.
  std::vector<AccessibilityInfoDataWrapper*> children;
  source_node->GetChildren(&children);
  for (auto* child : children) {
    if (!child->IsNode() || !child->IsVisibleToUser() ||
        !GetBooleanProperty(child->GetNode(),
                            mojom::AccessibilityBooleanProperty::IMPORTANCE)) {
      continue;
    }
    return std::make_pair(
        child->GetId(),
        std::make_unique<DrawerLayoutHandler>(base::JoinString(
            event_data.event_text.value_or<std::vector<std::string>>({}),
            " ")));
  }
  return absl::nullopt;
}

bool DrawerLayoutHandler::PreDispatchEvent(
    AXTreeSourceArc* tree_source,
    const mojom::AccessibilityEventData& event_data) {
  return false;
}

void DrawerLayoutHandler::PostSerializeNode(ui::AXNodeData* out_data) const {
  out_data->role = ax::mojom::Role::kMenu;
  if (!name_.empty())
    out_data->SetName(name_);
}

}  // namespace arc
