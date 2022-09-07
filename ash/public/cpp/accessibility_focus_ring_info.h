// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ACCESSIBILITY_FOCUS_RING_INFO_H_
#define ASH_PUBLIC_CPP_ACCESSIBILITY_FOCUS_RING_INFO_H_

#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

// Whether the focus ring should persist or fade.
enum class ASH_PUBLIC_EXPORT FocusRingBehavior { FADE_OUT, PERSIST };

// The visual style of the focus ring.
enum class ASH_PUBLIC_EXPORT FocusRingType { GLOW, SOLID, DASHED };

// How focus rings are layered.
enum class ASH_PUBLIC_EXPORT FocusRingStackingOrder {
  // Above most UI, including accessibility bubble panels.
  ABOVE_ACCESSIBILITY_BUBBLES,
  // Above most UI, except below accessibility bubble panels.
  BELOW_ACCESSIBILITY_BUBBLES
};

// Defines a specific focus ring by specifying:
// - |rects_in_screen| the regions around which to draw the focus ring (in
//       screen coordinates).
// - |focus_ring_behavior| whether it should persist or fade.
// - |focus_ring_type| the visual style of the focus ring.
// - |color| the color of the focus ring.
// - |secondary_color| a secondary color, used by some visual styles.
// - |background_color| The color to draw a background outside of the focus
//       ring and over the rest of the display. Set to fully transparent
//       for none.
// TODO: This struct could possibly be merged with ash::AccessibilityFocusRing.
struct ASH_PUBLIC_EXPORT AccessibilityFocusRingInfo {
  AccessibilityFocusRingInfo();

  AccessibilityFocusRingInfo(const AccessibilityFocusRingInfo&) = delete;
  AccessibilityFocusRingInfo& operator=(const AccessibilityFocusRingInfo&) =
      delete;

  ~AccessibilityFocusRingInfo();

  bool operator==(const AccessibilityFocusRingInfo& other) const;

  std::vector<gfx::Rect> rects_in_screen;
  FocusRingBehavior behavior = FocusRingBehavior::FADE_OUT;
  FocusRingType type = FocusRingType::GLOW;
  FocusRingStackingOrder stacking_order =
      FocusRingStackingOrder::ABOVE_ACCESSIBILITY_BUBBLES;
  SkColor color = SK_ColorTRANSPARENT;
  SkColor secondary_color = SK_ColorTRANSPARENT;
  SkColor background_color = SK_ColorTRANSPARENT;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ACCESSIBILITY_FOCUS_RING_INFO_H_
