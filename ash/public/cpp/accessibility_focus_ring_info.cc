// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/accessibility_focus_ring_info.h"

namespace ash {

AccessibilityFocusRingInfo::AccessibilityFocusRingInfo() = default;
AccessibilityFocusRingInfo::~AccessibilityFocusRingInfo() = default;

bool AccessibilityFocusRingInfo::operator==(
    const AccessibilityFocusRingInfo& other) const {
  return rects_in_screen == other.rects_in_screen &&
         behavior == other.behavior && type == other.type &&
         stacking_order == other.stacking_order && color == other.color &&
         secondary_color == other.secondary_color &&
         background_color == other.background_color;
}

}  // namespace ash
