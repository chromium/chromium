// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/accessibility/arc_accessibility_util.h"

#include "ui/accessibility/ax_enums.mojom.h"

namespace arc {

ax::mojom::Event ToAXEvent(
    mojom::AccessibilityEventType arc_event_type,
    mojom::AccessibilityNodeInfoData* focused_node_info_data) {
  switch (arc_event_type) {
    case mojom::AccessibilityEventType::VIEW_FOCUSED:
    case mojom::AccessibilityEventType::VIEW_ACCESSIBILITY_FOCUSED:
      return ax::mojom::Event::kFocus;
    case mojom::AccessibilityEventType::VIEW_CLICKED:
    case mojom::AccessibilityEventType::VIEW_LONG_CLICKED:
      return ax::mojom::Event::kClicked;
    case mojom::AccessibilityEventType::VIEW_TEXT_CHANGED:
      return ax::mojom::Event::kTextChanged;
    case mojom::AccessibilityEventType::VIEW_TEXT_SELECTION_CHANGED:
      return ax::mojom::Event::kTextSelectionChanged;
    case mojom::AccessibilityEventType::WINDOW_STATE_CHANGED:
    case mojom::AccessibilityEventType::NOTIFICATION_STATE_CHANGED:
    case mojom::AccessibilityEventType::WINDOW_CONTENT_CHANGED:
    case mojom::AccessibilityEventType::WINDOWS_CHANGED:
      return ax::mojom::Event::kLayoutComplete;
    case mojom::AccessibilityEventType::VIEW_HOVER_ENTER:
      return ax::mojom::Event::kHover;
    case mojom::AccessibilityEventType::ANNOUNCEMENT:
      return ax::mojom::Event::kAlert;
    case mojom::AccessibilityEventType::VIEW_SCROLLED:
      return ax::mojom::Event::kScrollPositionChanged;
    case mojom::AccessibilityEventType::VIEW_SELECTED: {
      // In Android, VIEW_SELECTED event is fired in the two cases below:
      // 1. Changing a value in ProgressBar or TimePicker.
      //    (this usage is NOT documented)
      // 2. Selecting an item in the context of an AdapterView.
      //    (officially documented in Android Developer doc below)
      //    https://developer.android.com/reference/android/view/accessibility/AccessibilityEvent#TYPE_VIEW_SELECTED
      if (focused_node_info_data && focused_node_info_data->range_info)
        return ax::mojom::Event::kValueChanged;
      else
        return ax::mojom::Event::kSelection;
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

bool GetBooleanProperty(mojom::AccessibilityNodeInfoData* node,
                        mojom::AccessibilityBooleanProperty prop) {
  if (!node || !node->boolean_properties)
    return false;

  auto it = node->boolean_properties->find(prop);
  if (it == node->boolean_properties->end())
    return false;

  return it->second;
}

}  // namespace arc
