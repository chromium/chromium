// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/resize_shadow.h"

#include "base/no_destructor.h"
#include "base/time/time.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/canvas_image_source.h"

namespace {

// This class simply draws a roundrect. The layout and tiling is handled by
// ResizeShadow and NinePatchLayer.
class ResizeShadowImageSource : public gfx::CanvasImageSource {
 public:
  ResizeShadowImageSource(int width)
      : gfx::CanvasImageSource(gfx::Size(width, width)) {}
  ResizeShadowImageSource(const ResizeShadowImageSource&) = delete;
  ResizeShadowImageSource& operator=(const ResizeShadowImageSource&) = delete;
  ~ResizeShadowImageSource() override = default;

  // gfx::CanvasImageSource:
  void Draw(gfx::Canvas* canvas) override {
    cc::PaintFlags paint;
    paint.setAntiAlias(true);
    paint.setColor(color_);
    canvas->DrawRoundRect(gfx::RectF(gfx::SizeF(size())), shadow_corner_radius_,
                          paint);
  }

  int shadow_corner_radius_;
  SkColor color_;
};

// Calculate outsets of the |window_| based on a |hit_test| code and |thickness|
const gfx::Insets CalculateOutsets(int hit, int thickness) {
  bool show_top = hit == HTTOPLEFT || hit == HTTOP || hit == HTTOPRIGHT;
  bool show_left = hit == HTTOPLEFT || hit == HTLEFT || hit == HTBOTTOMLEFT;
  bool show_bottom =
      hit == HTBOTTOMLEFT || hit == HTBOTTOM || hit == HTBOTTOMRIGHT;
  bool show_right = hit == HTTOPRIGHT || hit == HTRIGHT || hit == HTBOTTOMRIGHT;

  const int outset = -thickness;
  return gfx::Insets(show_top ? outset : 0, show_left ? outset : 0,
                     show_bottom ? outset : 0, show_right ? outset : 0);
}

}  // namespace

namespace ash {

ResizeShadow::ResizeShadow(aura::Window* window, const InitParams& params)
    : window_(window), params_(params) {
  // Use a NinePatchLayer to tile the shadow image (which is simply a
  // roundrect).
  gfx::Insets aperture_insets(params_.thickness + params_.window_corner_radius);
  layer_ = std::make_unique<ui::Layer>(ui::LAYER_NINE_PATCH);
  layer_->SetName("WindowResizeShadow");
  layer_->SetFillsBoundsOpaquely(false);
  layer_->SetOpacity(0.f);
  layer_->SetVisible(false);

  static base::NoDestructor<gfx::ImageSkia> shadow_image;

  if (shadow_image->isNull()) {
    // The image has to have enough space to depict the visual thickness (left
    // and right) plus an inset for extending beneath the window's rounded
    // corner plus one pixel for the center of the nine patch.
    int image_side = 2 * (params_.thickness + params_.window_corner_radius) + 1;
    auto source = std::make_unique<ResizeShadowImageSource>(image_side);
    source->shadow_corner_radius_ = params.shadow_corner_radius;
    source->color_ = params.color;
    const gfx::Size source_size = source->size();
    *shadow_image = gfx::ImageSkia(std::move(source), source_size);
  }
  layer_->UpdateNinePatchLayerImage(*shadow_image);
  gfx::Rect aperture(shadow_image->size());
  aperture.Inset(aperture_insets);
  layer_->UpdateNinePatchLayerAperture(aperture);
  layer_->UpdateNinePatchLayerBorder(
      gfx::Rect(aperture_insets.left(), aperture_insets.top(),
                aperture_insets.width(), aperture_insets.height()));

  ReparentLayer();
}

ResizeShadow::~ResizeShadow() = default;

void ResizeShadow::ShowForHitTest(int hit) {
  UpdateHitTest(hit);
  visible_ = true;
  UpdateBoundsAndVisibility();
}

void ResizeShadow::Hide() {
  UpdateHitTest(HTNOWHERE);
  visible_ = false;
  UpdateBoundsAndVisibility();
}

void ResizeShadow::UpdateHitTest(int hit) {
  // Don't start animations unless something changed.
  if (hit == last_hit_test_)
    return;
  last_hit_test_ = hit;
}

void ResizeShadow::UpdateBoundsAndVisibility() {
  // The shadow layer is positioned such that one or two edges will stick out
  // from underneath |window_|. Thus |window_| occludes the rest of the
  // roundrect.
  const gfx::Insets outsets =
      params_.hit_test_enabled
          ? CalculateOutsets(last_hit_test_, params_.thickness)
          : gfx::Insets(-params_.thickness);

  if (outsets.IsEmpty() && !layer_->GetTargetVisibility())
    return;

  visible_ &= !outsets.IsEmpty();
  if (visible_) {
    gfx::Rect bounds = window_->bounds();
    bounds.Inset(outsets);
    layer_->SetBounds(bounds);
  }

  ui::ScopedLayerAnimationSettings settings(layer_->GetAnimator());
  if (!visible_)
    settings.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(params_.hide_duration_ms));
  layer_->SetOpacity(visible_ ? params_.opacity : 0.f);
  layer_->SetVisible(visible_);
}

void ResizeShadow::ReparentLayer() {
  DCHECK(window_->layer()->parent());
  if (layer_->parent() != window_->layer()->parent())
    window_->layer()->parent()->Add(layer_.get());
  layer_->parent()->StackBelow(layer_.get(), window_->layer());
}

}  // namespace ash
