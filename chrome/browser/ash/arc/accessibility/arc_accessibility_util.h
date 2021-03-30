// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_ACCESSIBILITY_ARC_ACCESSIBILITY_UTIL_H_
#define CHROME_BROWSER_ASH_ARC_ACCESSIBILITY_ARC_ACCESSIBILITY_UTIL_H_

#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "components/arc/mojom/accessibility_helper.mojom-forward.h"
#include "ui/accessibility/ax_enum_util.h"

namespace aura {
class Window;
}

namespace arc {

class AccessibilityInfoDataWrapper;

// This function is only called when EventType is WINDOW_STATE_CHANGED or
// WINDOW_CONTENT_CHANGED.
base::Optional<ax::mojom::Event> FromContentChangeTypesToAXEvent(
    const std::vector<int>& arc_content_change_types);

ax::mojom::Event ToAXEvent(
    mojom::AccessibilityEventType arc_event_type,
    const base::Optional<std::vector<int>>& arc_content_change_types,
    AccessibilityInfoDataWrapper* source_node,
    AccessibilityInfoDataWrapper* focused_node);

base::Optional<mojom::AccessibilityActionType> ConvertToAndroidAction(
    ax::mojom::Action action);

AccessibilityInfoDataWrapper* GetSelectedNodeInfoFromAdapterViewEvent(
    const mojom::AccessibilityEventData& event_data,
    AccessibilityInfoDataWrapper* source_node);

std::string ToLiveStatusString(mojom::AccessibilityLiveRegionType type);

template <class DataType, class PropType>
bool GetBooleanProperty(DataType* node, PropType prop) {
  if (!node || !node->boolean_properties)
    return false;

  auto it = node->boolean_properties->find(prop);
  if (it == node->boolean_properties->end())
    return false;

  return it->second;
}

template <class PropMTypeMap, class PropType>
bool HasProperty(const PropMTypeMap& properties, const PropType prop) {
  if (!properties)
    return false;

  return properties->find(prop) != properties->end();
}

template <class PropMTypeMap, class PropType, class OutType>
bool GetProperty(const PropMTypeMap& properties,
                 const PropType prop,
                 OutType* out_value) {
  if (!properties)
    return false;

  auto it = properties->find(prop);
  if (it == properties->end())
    return false;

  *out_value = it->second;
  return true;
}

template <class PropType, class OutType>
base::Optional<OutType> GetPropertyOrNull(
    const base::Optional<base::flat_map<PropType, OutType>>& properties,
    const PropType prop) {
  OutType out_value;
  if (GetProperty(properties, prop, &out_value))
    return out_value;
  return base::nullopt;
}

template <class InfoDataType, class PropType>
bool HasNonEmptyStringProperty(InfoDataType* node, PropType prop) {
  if (!node || !node->string_properties)
    return false;

  auto it = node->string_properties->find(prop);
  if (it == node->string_properties->end())
    return false;

  return !it->second.empty();
}

aura::Window* FindArcWindow(aura::Window* child);

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_ACCESSIBILITY_ARC_ACCESSIBILITY_UTIL_H_
