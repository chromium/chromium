// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_ACCESSIBILITY_FOCUS_RING_LAYER_H_
#define ASH_ACCESSIBILITY_ACCESSIBILITY_FOCUS_RING_LAYER_H_

#include "ash/accessibility/accessibility_focus_ring.h"
#include "ash/accessibility/focus_ring_layer.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/accessibility_focus_ring_info.h"
#include "base/macros.h"
#include "ui/compositor/paint_recorder.h"

namespace ash {

// A subclass of FocusRingLayer intended for use by ChromeVox; it supports
// nonrectangular focus rings in order to highlight groups of elements or
// a range of text on a page.
class ASH_EXPORT AccessibilityFocusRingLayer : public FocusRingLayer {
 public:
  explicit AccessibilityFocusRingLayer(AccessibilityLayerDelegate* delegate);
  ~AccessibilityFocusRingLayer() override;

  // Create the layer and update its bounds and position in the hierarchy.
  void Set(const AccessibilityFocusRing& ring);

  void SetAppearance(FocusRingType type,
                     SkColor color,
                     SkColor secondary_color);

  SkColor color_for_testing() { return custom_color(); }

 private:
  // ui::LayerDelegate overrides:
  void OnPaintLayer(const ui::PaintContext& context) override;

  void DrawGlowFocusRing(ui::PaintRecorder& recorder, cc::PaintFlags& flags);
  void DrawSolidFocusRing(ui::PaintRecorder& recorder, cc::PaintFlags& flags);
  void DrawDashedFocusRing(ui::PaintRecorder& recorder, cc::PaintFlags& flags);

  // The outline of the current focus ring.
  AccessibilityFocusRing ring_;
  // The type of focus ring.
  FocusRingType type_;
  // The secondary color.
  SkColor secondary_color_;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityFocusRingLayer);
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_ACCESSIBILITY_FOCUS_RING_LAYER_H_
