// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/accessibility/arc_accessibility_util.h"

#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/mojom/accessibility_helper.mojom-shared.h"
#include "ash/components/arc/mojom/accessibility_helper.mojom.h"
#include "ash/public/cpp/app_types_util.h"
#include "base/containers/contains.h"
#include "chrome/browser/ash/arc/accessibility/accessibility_info_data_wrapper.h"
#include "chrome/browser/ash/arc/accessibility/accessibility_node_info_data_wrapper.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/aura/window.h"

namespace arc {

namespace {

aura::Window* FindWindowToParent(bool (*predicate)(const aura::Window*),
                                 aura::Window* window) {
  while (window) {
    if (predicate(window))
      return window;
    window = window->parent();
  }
  return nullptr;
}

}  // namespace

using AXBooleanProperty = mojom::AccessibilityBooleanProperty;
using AXEventIntProperty = mojom::AccessibilityEventIntProperty;
using AXIntProperty = mojom::AccessibilityIntProperty;
using AXNodeInfoData = mojom::AccessibilityNodeInfoData;

absl::optional<ax::mojom::Event> FromContentChangeTypesToAXEvent(
    const std::vector<int32_t>& arc_content_change_types) {
  if (base::Contains(
          arc_content_change_types,
          static_cast<int32_t>(mojom::ContentChangeType::STATE_DESCRIPTION))) {
    return ax::mojom::Event::kAriaAttributeChanged;
  }
  return absl::nullopt;
}

ax::mojom::Event ToAXEvent(
    mojom::AccessibilityEventType arc_event_type,
    const absl::optional<std::vector<int>>& arc_content_change_types,
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
        const absl::optional<ax::mojom::Event> event_or_null =
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
        const absl::optional<ax::mojom::Event> event_or_null =
            FromContentChangeTypesToAXEvent(arc_content_change_types.value());
        if (event_or_null.has_value()) {
          return event_or_null.value();
        }
      }
      int live_region_type_int;
      if (source_node && source_node->GetNode() &&
          GetProperty(source_node->GetNode()->int_properties,
                      AXIntProperty::LIVE_REGION, &live_region_type_int)) {
        mojom::AccessibilityLiveRegionType live_region_type =
            static_cast<mojom::AccessibilityLiveRegionType>(
                live_region_type_int);
        if (live_region_type != mojom::AccessibilityLiveRegionType::NONE) {
          // Dispatch a kLiveRegionChanged event to ensure that all liveregions
          // (inc. snackbar) will get announced. It is currently difficult to
          // determine when liveregions need to be announced, in particular
          // differentiaiting between when they first appear (vs text changed).
          // This case is made evident with snackbar handling, which needs to be
          // announced when it appears.
          // TODO(b/187465133): Revisit this liveregion handling logic, once
          // the talkback spec has been clarified. There is a proposal to write
          // an API to expose attributes similar to aria-relevant, which will
          // eventually allow liveregions to be handled similar to how it gets
          // handled on the web.
          return ax::mojom::Event::kLiveRegionChanged;
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

absl::optional<mojom::AccessibilityActionType> ConvertToAndroidAction(
    ax::mojom::Action action) {
  switch (action) {
    case ax::mojom::Action::kDoDefault:
      return arc::mojom::AccessibilityActionType::CLICK;
    case ax::mojom::Action::kFocus:
      return arc::mojom::AccessibilityActionType::FOCUS;
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
    case ax::mojom::Action::kScrollToPositionAtRowColumn:
      return arc::mojom::AccessibilityActionType::SCROLL_TO_POSITION;
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
    case ax::mojom::Action::kLongClick:
      return arc::mojom::AccessibilityActionType::LONG_CLICK;
    default:
      return absl::nullopt;
  }
}

ax::mojom::Action ConvertToChromeAction(
    const mojom::AccessibilityActionType action) {
  switch (action) {
    case arc::mojom::AccessibilityActionType::CLICK:
      return ax::mojom::Action::kDoDefault;
    case arc::mojom::AccessibilityActionType::FOCUS:
      return ax::mojom::Action::kFocus;
    case arc::mojom::AccessibilityActionType::ACCESSIBILITY_FOCUS:
      // TODO(hirokisato): there are multiple actions converted to
      // ACCESSIBILITY_FOCUS. Consider if this is appropriate.
      return ax::mojom::Action::kSetSequentialFocusNavigationStartingPoint;
    case arc::mojom::AccessibilityActionType::SHOW_ON_SCREEN:
      return ax::mojom::Action::kScrollToMakeVisible;
    case arc::mojom::AccessibilityActionType::SCROLL_BACKWARD:
      return ax::mojom::Action::kScrollBackward;
    case arc::mojom::AccessibilityActionType::SCROLL_FORWARD:
      return ax::mojom::Action::kScrollForward;
    case arc::mojom::AccessibilityActionType::SCROLL_UP:
      return ax::mojom::Action::kScrollUp;
    case arc::mojom::AccessibilityActionType::SCROLL_DOWN:
      return ax::mojom::Action::kScrollDown;
    case arc::mojom::AccessibilityActionType::SCROLL_LEFT:
      return ax::mojom::Action::kScrollLeft;
    case arc::mojom::AccessibilityActionType::SCROLL_RIGHT:
      return ax::mojom::Action::kScrollRight;
    case arc::mojom::AccessibilityActionType::CUSTOM_ACTION:
      return ax::mojom::Action::kCustomAction;
    case arc::mojom::AccessibilityActionType::CLEAR_ACCESSIBILITY_FOCUS:
      return ax::mojom::Action::kClearAccessibilityFocus;
    case arc::mojom::AccessibilityActionType::GET_TEXT_LOCATION:
      return ax::mojom::Action::kGetTextLocation;
    case arc::mojom::AccessibilityActionType::SHOW_TOOLTIP:
      return ax::mojom::Action::kShowTooltip;
    case arc::mojom::AccessibilityActionType::HIDE_TOOLTIP:
      return ax::mojom::Action::kHideTooltip;
    case arc::mojom::AccessibilityActionType::COLLAPSE:
      return ax::mojom::Action::kCollapse;
    case arc::mojom::AccessibilityActionType::EXPAND:
      return ax::mojom::Action::kExpand;
    case arc::mojom::AccessibilityActionType::LONG_CLICK:
      return ax::mojom::Action::kLongClick;
    case arc::mojom::AccessibilityActionType::SCROLL_TO_POSITION:
      return ax::mojom::Action::kScrollToPositionAtRowColumn;
    // Below are actions not mapped in ConvertToAndroidAction().
    case arc::mojom::AccessibilityActionType::CLEAR_FOCUS:
    case arc::mojom::AccessibilityActionType::SELECT:
    case arc::mojom::AccessibilityActionType::CLEAR_SELECTION:
    case arc::mojom::AccessibilityActionType::NEXT_AT_MOVEMENT_GRANULARITY:
    case arc::mojom::AccessibilityActionType::PREVIOUS_AT_MOVEMENT_GRANULARITY:
    case arc::mojom::AccessibilityActionType::NEXT_HTML_ELEMENT:
    case arc::mojom::AccessibilityActionType::PREVIOUS_HTML_ELEMENT:
    case arc::mojom::AccessibilityActionType::COPY:
    case arc::mojom::AccessibilityActionType::PASTE:
    case arc::mojom::AccessibilityActionType::CUT:
    case arc::mojom::AccessibilityActionType::SET_SELECTION:
    case arc::mojom::AccessibilityActionType::DISMISS:
    case arc::mojom::AccessibilityActionType::SET_TEXT:
    case arc::mojom::AccessibilityActionType::CONTEXT_CLICK:
    case arc::mojom::AccessibilityActionType::SET_PROGRESS:
      return ax::mojom::Action::kNone;
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
      return "off";
    case mojom::AccessibilityLiveRegionType::POLITE:
      return "polite";
    case mojom::AccessibilityLiveRegionType::ASSERTIVE:
      return "assertive";
    default:
      NOTREACHED();
  }
  return std::string();  // Placeholder.
}

bool IsArcOrGhostWindow(const aura::Window* window) {
  return window && (ash::IsArcWindow(window) ||
                    arc::GetWindowTaskOrSessionId(window).has_value());
}

aura::Window* FindArcWindow(aura::Window* window) {
  return FindWindowToParent(ash::IsArcWindow, window);
}

aura::Window* FindArcOrGhostWindow(aura::Window* window) {
  return FindWindowToParent(IsArcOrGhostWindow, window);
}

}  // namespace arc
