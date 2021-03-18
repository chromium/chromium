// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/accessibility/arc_accessibility_util.h"

#include "ash/public/cpp/app_types.h"
#include "base/optional.h"
#include "chrome/browser/ash/arc/accessibility/accessibility_info_data_wrapper.h"
#include "components/arc/mojom/accessibility_helper.mojom.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/aura/window.h"

namespace arc {

using AXBooleanProperty = mojom::AccessibilityBooleanProperty;
using AXEventIntProperty = mojom::AccessibilityEventIntProperty;
using AXNodeInfoData = mojom::AccessibilityNodeInfoData;

base::Optional<ax::mojom::Event> FromContentChangeTypesToAXEvent(
    const std::vector<int32_t>& arc_content_change_types) {
  if (base::Contains(
          arc_content_change_types,
          static_cast<int32_t>(mojom::ContentChangeType::STATE_DESCRIPTION))) {
    return ax::mojom::Event::kAriaAttributeChanged;
  }
  return base::nullopt;
}

ax::mojom::Event ToAXEvent(
    mojom::AccessibilityEventType arc_event_type,
    const base::Optional<std::vector<int>>& arc_content_change_types,
    AccessibilityInfoDataWrapper* source_node,
    AccessibilityInfoDataWrapper* focused_node) {
  switch (arc_event_type) {
    case mojom::AccessibilityEventType::VIEW_FOCUSED:
    case mojom::AccessibilityEventType::VIEW_ACCESSIBILITY_FOCUSED:
      return ax::mojom::Event::kFocus;
    case mojom::AccessibilityEventType::VIEW_CLICKED:
    case mojom::AccessibilityEventType::VIEW_LONG_CLICKED:
      return ax::mojom::Event::kClicked;
    case mojom::AccessibilityEventType::VIEW_TEXT_CHANGED:
      return ax::mojom::Event::kAriaAttributeChanged;
    case mojom::AccessibilityEventType::VIEW_TEXT_SELECTION_CHANGED:
      return ax::mojom::Event::kTextSelectionChanged;
    case mojom::AccessibilityEventType::WINDOW_STATE_CHANGED: {
      if (source_node && arc_content_change_types.has_value()) {
        const base::Optional<ax::mojom::Event> event_or_null =
            FromContentChangeTypesToAXEvent(arc_content_change_types.value());
        if (event_or_null.has_value()) {
          return event_or_null.value();
        }
      }
      if (focused_node)
        return ax::mojom::Event::kFocus;
      else
        return ax::mojom::Event::kLayoutComplete;
    }
    case mojom::AccessibilityEventType::NOTIFICATION_STATE_CHANGED:
      return ax::mojom::Event::kLayoutComplete;
    case mojom::AccessibilityEventType::WINDOW_CONTENT_CHANGED:
      if (source_node && arc_content_change_types.has_value()) {
        const base::Optional<ax::mojom::Event> event_or_null =
            FromContentChangeTypesToAXEvent(arc_content_change_types.value());
        if (event_or_null.has_value()) {
          return event_or_null.value();
        }
      }
      return ax::mojom::Event::kLayoutComplete;
    case mojom::AccessibilityEventType::WINDOWS_CHANGED:
      return ax::mojom::Event::kLayoutComplete;
    case mojom::AccessibilityEventType::VIEW_HOVER_ENTER:
      return ax::mojom::Event::kHover;
    case mojom::AccessibilityEventType::ANNOUNCEMENT: {
      // NOTE: Announcement event is handled in
      // ArcAccessibilityHelperBridge::OnAccessibilityEvent.
      NOTREACHED();
      break;
    }
    case mojom::AccessibilityEventType::VIEW_SCROLLED:
      return ax::mojom::Event::kScrollPositionChanged;
    case mojom::AccessibilityEventType::VIEW_SELECTED: {
      // VIEW_SELECTED event is not selection event in Chrome.
      // See the comment on AXTreeSourceArc::NotifyAccessibilityEvent.
      if (source_node && source_node->IsNode() &&
          source_node->GetNode()->range_info) {
        return ax::mojom::Event::kAriaAttributeChanged;
      } else {
        return ax::mojom::Event::kFocus;
      }
    }
    case mojom::AccessibilityEventType::VIEW_HOVER_EXIT:
    case mojom::AccessibilityEventType::TOUCH_EXPLORATION_GESTURE_START:
    case mojom::AccessibilityEventType::TOUCH_EXPLORATION_GESTURE_END:
    case mojom::AccessibilityEventType::
        VIEW_TEXT_TRAVERSED_AT_MOVEMENT_GRANULARITY:
    case mojom::AccessibilityEventType::GESTURE_DETECTION_START:
    case mojom::AccessibilityEventType::GESTURE_DETECTION_END:
    case mojom::AccessibilityEventType::TOUCH_INTERACTION_START:
    case mojom::AccessibilityEventType::TOUCH_INTERACTION_END:
    case mojom::AccessibilityEventType::VIEW_CONTEXT_CLICKED:
    case mojom::AccessibilityEventType::ASSIST_READING_CONTEXT:
      return ax::mojom::Event::kChildrenChanged;
    default:
      return ax::mojom::Event::kChildrenChanged;
  }
  return ax::mojom::Event::kChildrenChanged;
}

base::Optional<mojom::AccessibilityActionType> ConvertToAndroidAction(
    ax::mojom::Action action) {
  switch (action) {
    case ax::mojom::Action::kDoDefault:
      return arc::mojom::AccessibilityActionType::CLICK;
    case ax::mojom::Action::kFocus:
      // Fallthrough
    case ax::mojom::Action::kSetSequentialFocusNavigationStartingPoint:
      return arc::mojom::AccessibilityActionType::ACCESSIBILITY_FOCUS;
    case ax::mojom::Action::kScrollToMakeVisible:
      return arc::mojom::AccessibilityActionType::SHOW_ON_SCREEN;
    case ax::mojom::Action::kScrollBackward:
      return arc::mojom::AccessibilityActionType::SCROLL_BACKWARD;
    case ax::mojom::Action::kScrollForward:
      return arc::mojom::AccessibilityActionType::SCROLL_FORWARD;
    case ax::mojom::Action::kScrollUp:
      return arc::mojom::AccessibilityActionType::SCROLL_UP;
    case ax::mojom::Action::kScrollDown:
      return arc::mojom::AccessibilityActionType::SCROLL_DOWN;
    case ax::mojom::Action::kScrollLeft:
      return arc::mojom::AccessibilityActionType::SCROLL_LEFT;
    case ax::mojom::Action::kScrollRight:
      return arc::mojom::AccessibilityActionType::SCROLL_RIGHT;
    case ax::mojom::Action::kCustomAction:
      return arc::mojom::AccessibilityActionType::CUSTOM_ACTION;
    case ax::mojom::Action::kSetAccessibilityFocus:
      return arc::mojom::AccessibilityActionType::ACCESSIBILITY_FOCUS;
    case ax::mojom::Action::kClearAccessibilityFocus:
      return arc::mojom::AccessibilityActionType::CLEAR_ACCESSIBILITY_FOCUS;
    case ax::mojom::Action::kGetTextLocation:
      return arc::mojom::AccessibilityActionType::GET_TEXT_LOCATION;
    case ax::mojom::Action::kShowTooltip:
      return arc::mojom::AccessibilityActionType::SHOW_TOOLTIP;
    case ax::mojom::Action::kHideTooltip:
      return arc::mojom::AccessibilityActionType::HIDE_TOOLTIP;
    case ax::mojom::Action::kCollapse:
      return arc::mojom::AccessibilityActionType::COLLAPSE;
    case ax::mojom::Action::kExpand:
      return arc::mojom::AccessibilityActionType::EXPAND;
    case ax::mojom::Action::kShowContextMenu:
      return arc::mojom::AccessibilityActionType::LONG_CLICK;
    default:
      return base::nullopt;
  }
}

AccessibilityInfoDataWrapper* GetSelectedNodeInfoFromAdapterViewEvent(
    const mojom::AccessibilityEventData& event_data,
    AccessibilityInfoDataWrapper* source_node) {
  if (!source_node || !source_node->IsNode())
    return nullptr;

  AXNodeInfoData* node_info = source_node->GetNode();
  if (!node_info)
    return nullptr;

  AccessibilityInfoDataWrapper* selected_node = source_node;
  if (!node_info->collection_item_info) {
    // The event source is not an item of AdapterView. If the event source is
    // AdapterView, select the child. Otherwise, this is an unrelated event.
    int item_count, from_index, current_item_index;
    if (!GetProperty(event_data.int_properties, AXEventIntProperty::ITEM_COUNT,
                     &item_count) ||
        !GetProperty(event_data.int_properties, AXEventIntProperty::FROM_INDEX,
                     &from_index) ||
        !GetProperty(event_data.int_properties,
                     AXEventIntProperty::CURRENT_ITEM_INDEX,
                     &current_item_index)) {
      return nullptr;
    }

    int index = current_item_index - from_index;
    if (index < 0)
      return nullptr;

    std::vector<AccessibilityInfoDataWrapper*> children;
    source_node->GetChildren(&children);
    if (index >= static_cast<int>(children.size()))
      return nullptr;

    selected_node = children[index];
  }

  // Sometimes a collection item is wrapped by a non-focusable node.
  // Find a node with focusable property.
  while (selected_node && !GetBooleanProperty(selected_node->GetNode(),
                                              AXBooleanProperty::FOCUSABLE)) {
    std::vector<AccessibilityInfoDataWrapper*> children;
    selected_node->GetChildren(&children);
    if (children.size() != 1)
      break;
    selected_node = children[0];
  }
  return selected_node;
}

std::string ToLiveStatusString(mojom::AccessibilityLiveRegionType type) {
  switch (type) {
    case mojom::AccessibilityLiveRegionType::NONE:
      return "none";
    case mojom::AccessibilityLiveRegionType::POLITE:
      return "polite";
    case mojom::AccessibilityLiveRegionType::ASSERTIVE:
      return "assertive";
    default:
      NOTREACHED();
  }
  return std::string();  // Placeholder.
}

aura::Window* FindArcWindow(aura::Window* window) {
  while (window) {
    if (ash::IsArcWindow(window))
      return window;
    window = window->parent();
  }
  return nullptr;
}

}  // namespace arc
