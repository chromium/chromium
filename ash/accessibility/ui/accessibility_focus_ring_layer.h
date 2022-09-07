// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_UI_ACCESSIBILITY_FOCUS_RING_LAYER_H_
#define ASH_ACCESSIBILITY_UI_ACCESSIBILITY_FOCUS_RING_LAYER_H_

#include "ash/accessibility/ui/accessibility_focus_ring.h"
#include "ash/accessibility/ui/focus_ring_layer.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/accessibility_focus_ring_info.h"
#include "ui/compositor/paint_recorder.h"

namespace ash {

// A subclass of FocusRingLayer intended for use by ChromeVox, Select to Speak
// and Switch Access; it supports nonrectangular focus rings in order to
// highlight groups of elements or a range of text on a page.
class ASH_EXPORT AccessibilityFocusRingLayer : public FocusRingLayer {
 public:
  explicit AccessibilityFocusRingLayer(AccessibilityLayerDelegate* delegate);

  AccessibilityFocusRingLayer(const AccessibilityFocusRingLayer&) = delete;
  AccessibilityFocusRingLayer& operator=(const AccessibilityFocusRingLayer&) =
      delete;

  ~AccessibilityFocusRingLayer() override;

  // Create the layer and update its bounds and position in the hierarchy.
  void Set(const AccessibilityFocusRing& ring);

  void SetAppearance(FocusRingType type,
                     FocusRingStackingOrder stacking_order,
                     SkColor color,
                     SkColor secondary_color,
                     SkColor background_alpha);

  SkColor color_for_testing() { return custom_color(); }

 private:
  // ui::LayerDelegate overrides:
  void OnPaintLayer(const ui::PaintContext& context) override;

  void DrawGlowFocusRing(ui::PaintRecorder& recorder, cc::PaintFlags& flags);
  void DrawSolidFocusRing(ui::PaintRecorder& recorder, cc::PaintFlags& flags);
  void DrawDashedFocusRing(ui::PaintRecorder& recorder, cc::PaintFlags& flags);
  void DrawFocusBackground(ui::PaintRecorder& recorder);

  // The outline of the current focus ring.
  AccessibilityFocusRing ring_;
  // The type of focus ring.
  FocusRingType type_;
  // How the focus ring should be stacked relative to other layers.
  FocusRingStackingOrder stacking_order_ =
      FocusRingStackingOrder::ABOVE_ACCESSIBILITY_BUBBLES;
  // The secondary color.
  SkColor secondary_color_;
  // The color of the background. When fully transparent, no background will be
  // drawn.
  SkColor background_color_ = SK_ColorTRANSPARENT;
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_UI_ACCESSIBILITY_FOCUS_RING_LAYER_H_
