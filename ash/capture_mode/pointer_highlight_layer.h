// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_POINTER_HIGHLIGHT_LAYER_H_
#define ASH_CAPTURE_MODE_POINTER_HIGHLIGHT_LAYER_H_

#include "ui/compositor/layer_delegate.h"
#include "ui/compositor/layer_owner.h"
#include "ui/compositor/paint_context.h"

namespace gfx {
class PointF;
}  // namespace gfx

namespace ash {

// `PointerHighlightLayer` is a `LayerOwner` that owns a texture layer that is
// added as a descendant of the window being recorded and on top of it (z-order)
// such that it can be captured with it. This layer is used to highlight the
// mouse or touch press events by painting a ring centered at the event
// location. `PointerHighlightLayer` is owned by
// `CaptureModeDemoToolsController` which will be created when animation starts
// and destroyed when animation ends.
class PointerHighlightLayer : public ui::LayerOwner, public ui::LayerDelegate {
 public:
  PointerHighlightLayer(const gfx::PointF& event_location_in_window,
                        ui::Layer* parent_layer);
  PointerHighlightLayer(const PointerHighlightLayer&) = delete;
  PointerHighlightLayer& operator=(const PointerHighlightLayer&) = delete;
  ~PointerHighlightLayer() override;

  // Sets bounds of the layer() centered with `event_location_in_window`.
  void CenterAroundPoint(const gfx::PointF& event_location_in_window);

  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override;
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_POINTER_HIGHLIGHT_LAYER_H_