// Copyright 2012 The Chromium Authors
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
  ResizeShadowImageSource(int width, int corner_radius, SkColor color)
      : gfx::CanvasImageSource(gfx::Size(width, width)),
        shadow_corner_radius_(corner_radius),
        color_(color) {}
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
  return gfx::Insets::TLBR(show_top ? outset : 0, show_left ? outset : 0,
                           show_bottom ? outset : 0, show_right ? outset : 0);
}

// static
const gfx::ImageSkia& MakeShadowImageOnce(
    ash::ResizeShadowType type,
    const ash::ResizeShadow::InitParams& params) {
  //  The image has to have enough space to depict the visual thickness
  //  (left and right) plus an inset for extending beneath the window's
  //  rounded corner plus one pixel for the center of the nine patch.
  int image_side = 2 * (params.thickness + params.window_corner_radius) + 1;
  switch (type) {
    case ash::ResizeShadowType::kLock: {
      static base::NoDestructor<gfx::ImageSkia> lock_shadow_image;
      if (lock_shadow_image->isNull()) {
        *lock_shadow_image =
            gfx::CanvasImageSource::MakeImageSkia<ResizeShadowImageSource>(
                image_side, params.shadow_corner_radius, params.color);
      }
      return *lock_shadow_image;
    }
    case ash::ResizeShadowType::kUnlock: {
      static base::NoDestructor<gfx::ImageSkia> unlock_shadow_image;
      if (unlock_shadow_image->isNull()) {
        *unlock_shadow_image =
            gfx::CanvasImageSource::MakeImageSkia<ResizeShadowImageSource>(
                image_side, params.shadow_corner_radius, params.color);
      }
      return *unlock_shadow_image;
    }
  }
}

}  // namespace

namespace ash {

ResizeShadow::ResizeShadow(aura::Window* window,
                           const InitParams& params,
                           ResizeShadowType type)
    : window_(window), params_(params), type_(type) {
  // Use a NinePatchLayer to tile the shadow image (which is simply a
  // roundrect).
  gfx::Insets aperture_insets(params_.thickness + params_.window_corner_radius);
  layer_ = std::make_unique<ui::Layer>(ui::LAYER_NINE_PATCH);
  layer_->SetName("WindowResizeShadow");
  layer_->SetFillsBoundsOpaquely(false);
  layer_->SetOpacity(0.f);
  layer_->SetVisible(false);
  auto shadow_image = MakeShadowImageOnce(type, params);
  layer_->UpdateNinePatchLayerImage(shadow_image);
  gfx::Rect aperture(shadow_image.size());
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
  UpdateBounds(window_->bounds());
}

void ResizeShadow::UpdateBounds(const gfx::Rect& window_bounds) {
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
    gfx::Rect bounds = window_bounds;
    bounds.Inset(outsets);
    layer_->SetBounds(bounds);
  }

  ui::ScopedLayerAnimationSettings settings(layer_->GetAnimator());
  if (!visible_)
    settings.SetTransitionDuration(
        base::Milliseconds(params_.hide_duration_ms));
  layer_->SetOpacity(visible_ ? params_.opacity : 0.f);
  layer_->SetVisible(visible_);
}

void ResizeShadow::ReparentLayer() {
  // This shadow could belong to a window that has been removed from its parent.
  // In that case, we should not try to access `window_`'s parent.
  if (!window_->layer()->parent())
    return;

  if (layer_->parent() != window_->layer()->parent())
    window_->layer()->parent()->Add(layer_.get());
  layer_->parent()->StackBelow(layer_.get(), window_->layer());
}

}  // namespace ash
