// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_UI_ACCESSIBILITY_HIGHLIGHT_LAYER_H_
#define ASH_ACCESSIBILITY_UI_ACCESSIBILITY_HIGHLIGHT_LAYER_H_

#include <vector>

#include "ash/accessibility/ui/accessibility_layer.h"
#include "ash/ash_export.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

// A subclass of LayerDelegate that can highlight regions on the screen.
class ASH_EXPORT AccessibilityHighlightLayer : public AccessibilityLayer {
 public:
  explicit AccessibilityHighlightLayer(AccessibilityLayerDelegate* delegate);

  AccessibilityHighlightLayer(const AccessibilityHighlightLayer&) = delete;
  AccessibilityHighlightLayer& operator=(const AccessibilityHighlightLayer&) =
      delete;

  ~AccessibilityHighlightLayer() override;

  // Create the layer and update its bounds and position in the hierarchy.
  void Set(const std::vector<gfx::Rect>& rects, SkColor color);

  // AccessibilityLayer overrides:
  int GetInset() const override;

  std::vector<gfx::Rect> rects_for_test() { return rects_; }

  SkColor color_for_test() { return highlight_color_; }

 private:
  // ui::LayerDelegate overrides:
  void OnPaintLayer(const ui::PaintContext& context) override;

  // The current rects to be highlighted, relative to the
  // AccessibilityPanelContainer window.
  std::vector<gfx::Rect> rects_;

  // The highlight color.
  SkColor highlight_color_;
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_UI_ACCESSIBILITY_HIGHLIGHT_LAYER_H_
