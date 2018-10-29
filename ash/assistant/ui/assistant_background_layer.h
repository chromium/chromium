// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_ASSISTANT_BACKGROUND_LAYER_H_
#define ASH_ASSISTANT_UI_ASSISTANT_BACKGROUND_LAYER_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/shadow_value.h"

namespace ui {
class LayerAnimationObserver;
}  // namespace ui

namespace views {
class BorderShadowLayerDelegate;
class CircleLayerDelegate;
class RectangleLayerDelegate;
}  // namespace views

namespace ash {

// TODO(wutao): This class is copied from assistant_overlay.cc for future
// animation smoothness improvement so that we can track the changes.
class ASH_EXPORT AssistantBackgroundLayer : public ui::Layer,
                                            public ui::LayerDelegate {
 public:
  AssistantBackgroundLayer();
  ~AssistantBackgroundLayer() override;

  void MoveLargeShadow(const gfx::PointF& new_center);

  void AnimateToLarge(const gfx::PointF& new_center,
                      ui::LayerAnimationObserver* animation_observer);

  void SetToLarge(const gfx::PointF& new_center);

  void ResetShape();

 private:
  // Enumeration of the different shapes that compose the background.
  enum PaintedShape {
    TOP_LEFT_CIRCLE = 0,
    TOP_RIGHT_CIRCLE,
    BOTTOM_RIGHT_CIRCLE,
    BOTTOM_LEFT_CIRCLE,
    HORIZONTAL_RECT,
    VERTICAL_RECT,
    // The total number of shapes, not an actual shape.
    PAINTED_SHAPE_COUNT
  };

  typedef gfx::Transform PaintedShapeTransforms[PAINTED_SHAPE_COUNT];

  void AddPaintLayer(PaintedShape painted_shape);

  void SetTransforms(const PaintedShapeTransforms transforms);

  void SetPaintedLayersVisible(bool visible);

  void CalculateCircleTransforms(const gfx::Size& size,
                                 PaintedShapeTransforms* transforms_out) const;

  void CalculateRectTransforms(const gfx::Size& desired_size,
                               float corner_radius,
                               PaintedShapeTransforms* transforms_out) const;

  gfx::Transform CalculateCircleTransform(float scale,
                                          float target_center_x,
                                          float target_center_y) const;

  gfx::Transform CalculateRectTransform(float x_scale, float y_scale) const;

  void AnimateToTransforms(
      const PaintedShapeTransforms transforms,
      base::TimeDelta duration,
      ui::LayerAnimator::PreemptionStrategy preemption_strategy,
      gfx::Tween::Type tween,
      ui::LayerAnimationObserver* animation_observer);

  std::string ToLayerName(PaintedShape painted_shape);

  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override;
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}

  // ui::Layers for all of the painted shape layers that compose the morphing
  // shape.
  std::unique_ptr<ui::Layer> painted_layers_[PAINTED_SHAPE_COUNT];

  const gfx::Size large_size_;

  const gfx::Size small_size_;

  // The center point of the painted shape.
  gfx::PointF center_point_;

  // ui::LayerDelegate to paint circles for all the circle layers.
  std::unique_ptr<views::CircleLayerDelegate> circle_layer_delegate_;

  // ui::LayerDelegate to paint rectangles for all the rectangle layers.
  std::unique_ptr<views::RectangleLayerDelegate> rect_layer_delegate_;

  // ui::LayerDelegate to paint rounded rectangle with shadow.
  std::unique_ptr<views::BorderShadowLayerDelegate> border_shadow_delegate_;

  gfx::ShadowValues shadow_values_;

  // This layer shows the small circle with shadow.
  std::unique_ptr<ui::Layer> shadow_layer_;

  // This layer shows the large rounded rectangle with shadow.
  std::unique_ptr<ui::Layer> large_shadow_layer_;

  DISALLOW_COPY_AND_ASSIGN(AssistantBackgroundLayer);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_ASSISTANT_BACKGROUND_LAYER_H_
