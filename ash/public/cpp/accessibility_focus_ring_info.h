// Copyright 2019 The Chromium Authors. All rights reserved.
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

// Defines a specific focus ring by specifying:
// - |rects_in_screen| the regions around which to draw the focus ring (in
//       screen coordinates).
// - |focus_ring_behavior| whether it should persist or fade.
// - |focus_ring_type| the visual style of the focus ring.
// - |color| the color of the focus ring.
// - |secondary_color| a secondary color, used by some visual styles.
// TODO: This struct could possibly be merged with ash::AccessibilityFocusRing.
struct ASH_PUBLIC_EXPORT AccessibilityFocusRingInfo {
  AccessibilityFocusRingInfo();
  ~AccessibilityFocusRingInfo();

  bool operator==(const AccessibilityFocusRingInfo& other) const;

  std::vector<gfx::Rect> rects_in_screen;
  FocusRingBehavior behavior = FocusRingBehavior::FADE_OUT;
  FocusRingType type = FocusRingType::GLOW;
  SkColor color = SK_ColorTRANSPARENT;
  SkColor secondary_color = SK_ColorTRANSPARENT;

 private:
  DISALLOW_COPY_AND_ASSIGN(AccessibilityFocusRingInfo);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ACCESSIBILITY_FOCUS_RING_INFO_H_
