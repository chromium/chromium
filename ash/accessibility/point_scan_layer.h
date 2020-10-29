// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_POINT_SCAN_LAYER_H_
#define ASH_ACCESSIBILITY_POINT_SCAN_LAYER_H_

#include "ash/accessibility/accessibility_layer.h"
#include "ash/accessibility/layer_animation_info.h"
#include "ash/ash_export.h"

namespace ash {

class PointScanLayer : public AccessibilityLayer {
 public:
  explicit PointScanLayer(AccessibilityLayerDelegate* delegate);
  ~PointScanLayer() override = default;

  PointScanLayer(const PointScanLayer&) = delete;
  PointScanLayer& operator=(const PointScanLayer&) = delete;

  // Begins sweeping a line horizontally across the screen, for the user to pick
  // an x-coordinate.
  // TODO(crbug/1061537): Animate the line across the screen.
  void StartHorizontalScanning();
  void PauseHorizontalScanning();
  void StartVerticalScanning();
  void PauseVerticalScanning();
  gfx::Rect GetBounds() const;
  bool IsMoving() const;

  // AccessibilityLayer overrides:
  bool CanAnimate() const override;
  bool NeedToAnimate() const override;
  int GetInset() const override;

 private:
  // ui:LayerDelegate overrides:
  void OnPaintLayer(const ui::PaintContext& context) override;
  void OnLayerChange(LayerAnimationInfo* animation_info);

  struct Line {
    gfx::Point start;
    gfx::Point end;
  };

  // The bounds within which we are scanning.
  gfx::Rect bounds_;

  // The line currently being drawn.
  Line line_;

  bool is_moving_;
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_POINT_SCAN_LAYER_H_
