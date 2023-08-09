// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_UI_ACCESSIBILITY_FOCUS_RING_GROUP_H_
#define ASH_ACCESSIBILITY_UI_ACCESSIBILITY_FOCUS_RING_GROUP_H_

#include <memory>
#include <vector>

#include "ash/accessibility/ui/accessibility_animation_one_shot.h"
#include "ash/accessibility/ui/accessibility_focus_ring.h"
#include "ash/accessibility/ui/accessibility_focus_ring_layer.h"
#include "ash/accessibility/ui/accessibility_layer.h"
#include "ash/accessibility/ui/layer_animation_info.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/accessibility_focus_ring_info.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

// AccessibilityFocusRingGroup handles tracking all the elements of a group of
// focus rings, including their positions, colors, and animation behavior.
// In general each extension or caller will have only one
// AccessibilityFocusRingGroup.
class ASH_EXPORT AccessibilityFocusRingGroup {
 public:
  AccessibilityFocusRingGroup();

  AccessibilityFocusRingGroup(const AccessibilityFocusRingGroup&) = delete;
  AccessibilityFocusRingGroup& operator=(const AccessibilityFocusRingGroup&) =
      delete;

  virtual ~AccessibilityFocusRingGroup();

  void UpdateFocusRingsFromInfo(AccessibilityLayerDelegate* delegate);
  bool AnimateFocusRings(base::TimeTicks timestamp);

  // Returns true if the focus ring has changed, false if there were no changes.
  bool UpdateFocusRing(std::unique_ptr<AccessibilityFocusRingInfo> focus_ring,
                       AccessibilityLayerDelegate* delegate);

  void ClearFocusRects(AccessibilityLayerDelegate* delegate);

  static void ComputeOpacity(LayerAnimationInfo* animation_info,
                             base::TimeTicks timestamp);

  LayerAnimationInfo* focus_animation_info() { return &focus_animation_info_; }

  void set_no_fade_for_testing() { no_fade_for_testing_ = true; }

  const std::vector<std::unique_ptr<AccessibilityFocusRingLayer>>&
  focus_layers_for_testing() const {
    return focus_layers_;
  }

  AccessibilityFocusRingInfo* focus_ring_info_for_testing() const {
    return focus_ring_info_.get();
  }

 protected:
  virtual int GetMargin() const;

  // Given an unordered vector of bounding rectangles that cover everything
  // that currently has focus, populate a vector of one or more
  // AccessibilityFocusRings that surround the rectangles. Adjacent or
  // overlapping rectangles are combined first. This function is protected
  // so it can be unit-tested.
  void RectsToRings(const std::vector<gfx::Rect>& rects,
                    std::vector<AccessibilityFocusRing>* rings) const;

 private:
  AccessibilityFocusRing RingFromSortedRects(
      const std::vector<gfx::Rect>& rects) const;
  void SplitIntoParagraphShape(const std::vector<gfx::Rect>& rects,
                               gfx::Rect* top,
                               gfx::Rect* middle,
                               gfx::Rect* bottom) const;
  bool Intersects(const gfx::Rect& r1, const gfx::Rect& r2) const;

  std::unique_ptr<AccessibilityFocusRingInfo> focus_ring_info_;
  std::vector<AccessibilityFocusRing> previous_focus_rings_;
  std::vector<std::unique_ptr<AccessibilityFocusRingLayer>> focus_layers_;
  std::unique_ptr<AccessibilityAnimationOneShot> focus_animation_;
  std::vector<AccessibilityFocusRing> focus_rings_;
  LayerAnimationInfo focus_animation_info_;
  bool no_fade_for_testing_ = false;
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_UI_ACCESSIBILITY_FOCUS_RING_GROUP_H_
