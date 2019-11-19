// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_ARC_ACCESSIBILITY_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_ARC_ACCESSIBILITY_UTIL_H_

#include <stdint.h>
#include <vector>

#include "components/arc/mojom/accessibility_helper.mojom.h"
#include "ui/accessibility/ax_enum_util.h"

namespace arc {

ax::mojom::Event ToAXEvent(mojom::AccessibilityEventType arc_event_type,
                           mojom::AccessibilityNodeInfoData* node_info_data);

// TODO(hirokisato) clean up GetProperty methods in AccessibilityNodeInfoData
// and AccessibilityWindowInfoData.
bool GetBooleanProperty(mojom::AccessibilityNodeInfoData* node,
                        mojom::AccessibilityBooleanProperty prop);

template <class InfoDataType, class PropType>
bool GetIntListProperty(InfoDataType* node,
                        PropType prop,
                        std::vector<int32_t>* out_value) {
  if (!node || !node->int_list_properties)
    return false;

  auto it = node->int_list_properties->find(prop);
  if (it == node->int_list_properties->end())
    return false;

  *out_value = it->second;
  return true;
}

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_ARC_ACCESSIBILITY_UTIL_H_
