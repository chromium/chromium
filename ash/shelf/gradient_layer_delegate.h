// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_GRADIENT_LAYER_DELEGATE_H_
#define ASH_SHELF_GRADIENT_LAYER_DELEGATE_H_

#include "ash/ash_export.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/gfx/geometry/rect.h"

namespace gfx {
class Canvas;
}  // namespace gfx

namespace ash {

// A layer delegate that paints optional fade-in/out gradient zones at the ends
// of its layer.
// TODO(jamescook): Move into //ash/controls. This class is used outside of the
// shelf (e.g. app list, desks).
class ASH_EXPORT GradientLayerDelegate : public ui::LayerDelegate {
 public:
  struct FadeZone {
    // Bounds of the fade in/out zone.
    gfx::Rect zone_rect;

    // Specifies the type of FadeZone: fade in or fade out.
    bool fade_in = false;

    // Indicates the drawing direction.
    bool is_horizontal = false;
  };

  GradientLayerDelegate();
  GradientLayerDelegate(const GradientLayerDelegate&) = delete;
  GradientLayerDelegate& operator=(const GradientLayerDelegate&) = delete;
  ~GradientLayerDelegate() override;

  void set_start_fade_zone(const FadeZone& fade_zone) {
    start_fade_zone_ = fade_zone;
  }
  void set_end_fade_zone(const FadeZone& fade_zone) {
    end_fade_zone_ = fade_zone;
  }
  const gfx::Rect& start_fade_zone_bounds() const {
    return start_fade_zone_.zone_rect;
  }
  const gfx::Rect& end_fade_zone_bounds() const {
    return end_fade_zone_.zone_rect;
  }
  ui::Layer* layer() { return &layer_; }

 private:
  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override;
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}

  void DrawFadeZone(const FadeZone& fade_zone, gfx::Canvas* canvas);

  ui::Layer layer_;
  FadeZone start_fade_zone_;
  FadeZone end_fade_zone_;
};

}  // namespace ash

#endif  // ASH_SHELF_GRADIENT_LAYER_DELEGATE_H_
