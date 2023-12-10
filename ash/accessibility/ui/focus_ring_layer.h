// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_UI_FOCUS_RING_LAYER_H_
#define ASH_ACCESSIBILITY_UI_FOCUS_RING_LAYER_H_

#include <optional>

#include "ash/accessibility/ui/accessibility_layer.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/compositor_animation_observer.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

// FocusRingLayer draws a focus ring at a given global rectangle.
class FocusRingLayer : public AccessibilityLayer {
 public:
  explicit FocusRingLayer(AccessibilityLayerDelegate* delegate);

  FocusRingLayer(const FocusRingLayer&) = delete;
  FocusRingLayer& operator=(const FocusRingLayer&) = delete;

  ~FocusRingLayer() override;

  // AccessibilityLayer overrides:
  int GetInset() const override;

  // Set a custom color, or reset to the default.
  void SetColor(SkColor color);
  void ResetColor();

 protected:
  bool has_custom_color() { return custom_color_.has_value(); }
  SkColor custom_color() { return *custom_color_; }

 private:
  // ui::LayerDelegate overrides:
  void OnPaintLayer(const ui::PaintContext& context) override;

  std::optional<SkColor> custom_color_;
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_UI_FOCUS_RING_LAYER_H_
