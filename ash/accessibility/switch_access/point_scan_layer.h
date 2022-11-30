// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_SWITCH_ACCESS_POINT_SCAN_LAYER_H_
#define ASH_ACCESSIBILITY_SWITCH_ACCESS_POINT_SCAN_LAYER_H_

#include "ash/accessibility/switch_access/point_scan_layer_animation_info.h"
#include "ash/accessibility/ui/accessibility_layer.h"
#include "ash/ash_export.h"
#include "ui/compositor/layer.h"

namespace gfx {
class Canvas;
}

namespace ash {

class PointScanLayer : public AccessibilityLayer {
 public:
  enum Orientation {
    HORIZONTAL = 0,
    VERTICAL,
  };

  enum Type {
    LINE = 0,
    RANGE,
  };

  explicit PointScanLayer(AccessibilityLayerDelegate* delegate,
                          Orientation orientation,
                          Type type);
  ~PointScanLayer() override = default;

  PointScanLayer(const PointScanLayer&) = delete;
  PointScanLayer& operator=(const PointScanLayer&) = delete;

  void Start();
  void Pause();

  bool IsMoving() const;
  gfx::Rect bounds() { return layer()->bounds(); }

  // AccessibilityLayer overrides:
  int GetInset() const override;

 private:
  void DrawLineWithOffsets(gfx::Canvas* canvas,
                           cc::PaintFlags flags,
                           int x_offset,
                           int y_offset);

  // ui:LayerDelegate overrides:
  void OnPaintLayer(const ui::PaintContext& context) override;
  void OnLayerChange(PointScanLayerAnimationInfo* animation_info);

  struct Line {
    gfx::Point start;
    gfx::Point end;
  };

  Orientation orientation_;
  Type type_;
  Line line_;
  bool is_moving_ = false;
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_SWITCH_ACCESS_POINT_SCAN_LAYER_H_
